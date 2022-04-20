/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      USB mass storage class support
*/

#include "usb_msc.h"
#include "usb_msc_common.h"

#include <message.h>

usb_msc_class_data_t *msc_class_data;
TaskData class_task;

void UsbMsc_BlockWaitReady(Sink sink, uint16 size)
{
    /* While not enough space and USB still attached, wait */
    while((SinkSlack(sink) < size) &&
            (UsbDeviceState() == usb_device_state_configured));
}

void UsbMsc_SendBulkData(uint8 *data, uint16 size_data)
{
    uint8 *ptr;
    Sink sink = msc_class_data->ep_sink;

    /* wait for free space in Sink */
    UsbMsc_BlockWaitReady(sink, size_data);

    if ((ptr = SinkMapClaim(sink, size_data)) != 0)
    {
        DEBUG_LOG_INFO("UsbMsc: sending bulk data %d bytes", size_data);
        memmove(ptr, data, size_data);
        (void) SinkFlush(sink, size_data);
    }
}

static void usbMsc_ProcessCBW(Source req, uint16 packet_size)
{
    uint16 i;
    usb_msc_cbw_t cbw;
    usb_msc_csw_t csw;
    const uint8 *ptr;
    bool found = FALSE;
    uint16 status_code = CSW_STATUS_PASSED;
    uint32 xfer_length;

    ptr = SourceMap(req);
    if (!ptr)
    {
        return;
    }

    /* check for CBW */
    if (packet_size >= CBW_SIZE)
    {
        for (i = 0; i < packet_size; i++)
        {
            if ((packet_size - i) >= CBW_SIZE)
            {
                if (ptr[i] == CBW_SIGNATURE_B1)
                {
                    if ((ptr[i+1] == CBW_SIGNATURE_B2) &&
                        (ptr[i+2] == CBW_SIGNATURE_B3) &&
                        (ptr[i+3] == CBW_SIGNATURE_B4) &&
                        ((ptr[i+12] == 0x00) || (ptr[i+12] == 0x80)) &&
                        (ptr[i+13] < MAX_LUN) &&
                        (ptr[i+14] <= 0x10) &&
                        (ptr[i+14] >= 0x01))
                    {
                        DEBUG_LOG_DEBUG("UsbMsc: found CBW, SourceDrop:%d", i);
                        SourceDrop(req, i);
                        ptr = SourceMap(req);
                        found = TRUE;
                        break;
                    }
                }
            }
        }

        if (!found)
        {
            SourceDrop(req, packet_size);
            DEBUG_LOG_WARN("UsbMsc: couldn't find CBW, SourceDrop:%d", packet_size);
            return;
        }

        memmove(&cbw, ptr, CBW_SIZE);

        /* Bytes 0-3 are dCBWSignature */
        /* Bytes 4-7 are dCBWTag */
        /* Bytes 8-11 are dCBWDataTransferLength */
        /* Byte 12 is bmCBWFlags */
        /* Byte 13 is bCBWLUN */
        /* Byte 14 is bCBWCBLength */
        /* Bytes 15-30 are CBWCB */
        DEBUG_LOG_DEBUG("UsbMsc: CB ");
        for (i = 0; i < cbw.bCBWCBLength[0]; i++)
        {
            DEBUG_LOG_DEBUG("%x ", cbw.CBWCB[i]);
        }

        /* the CBW has been verified so start building CSW */

        xfer_length = ((uint32)cbw.dCBWDataTransferLength[3] << 24) |
                      ((uint32)cbw.dCBWDataTransferLength[2] << 16) |
                              (cbw.dCBWDataTransferLength[1] << 8) |
                               cbw.dCBWDataTransferLength[0];

        DEBUG_LOG_DEBUG("UsbMsc: flags 0x%x LUN 0x%x CB len %d xfer len %ld",
                cbw.bCBWFlags[0], cbw.bCBWLUN[0], cbw.bCBWCBLength[0],
                xfer_length);

        /* write CSW signature */
        csw.dCSWSignature[0] = CSW_SIGNATURE_B1;
        csw.dCSWSignature[1] = CSW_SIGNATURE_B2;
        csw.dCSWSignature[2] = CSW_SIGNATURE_B3;
        csw.dCSWSignature[3] = CSW_SIGNATURE_B4;
        /* copy CBW tag to CSW */
        for (i = 0; i < 4; i++)
        {
            csw.dCSWTag[i] = cbw.dCBWTag[i];
        }

        if (xfer_length != 0)
        {
            bool is_to_host = (cbw.bCBWFlags[0] == CBW_FLAG_DIRECTION_TO_HOST);
            scsi_command_t cmd = cbw.CBWCB[0];
            uint8 *data = &cbw.CBWCB[1];

            status_code = UsbMsc_ScsiCommand(is_to_host, cmd, data, xfer_length);
        }

        /* send CSW */
        csw.dCSWDataResidue[0] = cbw.dCBWDataTransferLength[0];
        csw.dCSWDataResidue[1] = cbw.dCBWDataTransferLength[1];
        csw.dCSWDataResidue[2] = cbw.dCBWDataTransferLength[2];
        csw.dCSWDataResidue[3] = cbw.dCBWDataTransferLength[3];
        csw.bCSWStatus[0] = status_code;
        UsbMsc_SendBulkData((uint8 *)&csw, CSW_SIZE);

        SourceDrop(req, CBW_SIZE);
        DEBUG_LOG_DEBUG("UsbMsc: discard CBW, SourceDrop:%d\n", CBW_SIZE);
    }
    else
    {
        SourceDrop(req, packet_size);
        DEBUG_LOG_WARN("UsbMsc: small packet, SourceDrop:%d\n", packet_size);
    }
}

static void usbMsc_BulkRequestHandler(Source source)
{
    uint16 packet_size;

    /* Check for outstanding Class requests */
    while ((packet_size = SourceBoundary(source)) != 0)
    {
        /* Process data received from host, which should be a CBW */
        usbMsc_ProcessCBW(source, packet_size);
    }
}

static void usbMsc_ClassRequestHandler(Source source)
{
    uint16 packet_size;
    Sink sink = msc_class_data->class_sink;

    /* Check for outstanding Class requests */
    while ((packet_size = SourceBoundary(source)) != 0)
    {
        if (SourceSizeHeader(source))
        {
            /*
                This must be a class specific request so build the response.
                It must contain the original request, so copy from the source header.
            */
            UsbResponse usbresp;
            memcpy(&usbresp.original_request,
                   SourceMapHeader(source),
                   sizeof(UsbRequest));

            /* Set the response fields to default values to make the code below simpler */
            usbresp.success = FALSE;
            usbresp.data_length = 0;

            if (usbresp.original_request.bRequest == MS_BULK_RESET)
            {
                /* USB Mass Storage Class - Bulk Only Transport
                 * 3.1 Bulk-Only Mass Storage Reset
                 *
                 * This request is used to reset the mass storage device and
                 * its associated interface. This class-specific request shall
                 * ready the device for the next CBW from the host. */
                DEBUG_LOG_INFO("UsbMsc: BULK_RESET");
                if(usbresp.original_request.wValue == 0)
                {
                    usbresp.success = TRUE;
                }
            }
            else if (usbresp.original_request.bRequest == MS_GET_MAX_LUN)
            {
                /* USB Mass Storage Class - Bulk Only Transport
                 * 3.2 Get Max LUN
                 *
                 * The device may implement several logical units that share
                 * common device characteristics. The host uses bCBWLUN
                 * (see 5.1 Command Block Wrapper (CBW)) to designate which
                 * logical unit of the device is the destination of the CBW.
                 * The Get Max LUN device request is used to determine
                 * the number of logical units supported by the device.
                 * Logical Unit Numbers on the device shall be numbered
                 * contiguously starting from LUN 0 to a maximum LUN of 15 (Fh).*/
                DEBUG_LOG_INFO("UsbMsc: GET_MAX_LUN");
                if(usbresp.original_request.wValue == 0)
                {
                    uint8 *ptr = SinkMapClaim(sink, 1);
                    if (ptr != 0)
                    {
                        ptr[0] = MAX_LUN - 1; /* Number of Logical Units supported - 1 */
                        usbresp.data_length = 1;
                        usbresp.success = TRUE;
                    }
                }
            }
            else
            {
                DEBUG_LOG_WARN("UsbMsc: unknown control xfer, bRequest=0x%x",
                        usbresp.original_request.bRequest);
            }

            /* Send response */
            if (usbresp.data_length)
            {
                (void)SinkFlushHeader(sink, usbresp.data_length,
                                     (void *)&usbresp, sizeof(UsbResponse));
            }
            else
            {
                /* Sink packets can never be zero-length, so flush a dummy byte */
                (void)SinkClaim(sink, 1);
                (void)SinkFlushHeader(sink, 1, (void *)&usbresp, sizeof(UsbResponse));
            }
        }

        /* Discard the original request */
        SourceDrop(source, packet_size);
    }
}

static void usbMsc_Handler(Task task, MessageId id, Message message)
{
    UNUSED(task);

    if (!msc_class_data)
    {
        return;
    }

    if (id == MESSAGE_MORE_DATA)
    {
        Source request_source = ((MessageMoreData *)message)->source;

        if (request_source == msc_class_data->class_source)
        {
            usbMsc_ClassRequestHandler(request_source);
        }
        else if (request_source == msc_class_data->ep_source)
        {
            usbMsc_BulkRequestHandler(request_source);
        }
    }
}

static usb_class_context_t usbMsc_Create(usb_device_index_t dev_index,
                                  usb_class_interface_config_data_t config_data)
{
    UsbCodes codes;
    UsbInterface intf;
    EndPointInfo ep_info[2];
    uint8 source_endpoint, sink_endpoint;
    usb_msc_class_data_t *data;
    const usb_msc_config_params_t *config = (const usb_msc_config_params_t *)config_data;

    DEBUG_LOG_INFO("UsbMsc: create");

    if (msc_class_data)
    {
        DEBUG_LOG_ERROR("UsbMsc: ERROR - class already present");
        Panic();
    }

    codes.bInterfaceClass    = B_INTERFACE_CLASS_MASS_STORAGE;
    codes.bInterfaceSubClass = B_INTERFACE_SUB_CLASS_MASS_STORAGE;
    codes.bInterfaceProtocol = B_INTERFACE_PROTOCOL_MASS_STORAGE;
    codes.iInterface         = 0;

    /* USB Mass Storage class does not need class descriptors */
    intf = UsbAddInterface(&codes, 0, NULL, 0);

    if (intf == usb_interface_error)
    {
        DEBUG_LOG_ERROR("UsbMsc: UsbAddInterface ERROR");
        Panic();
    }

    /* USB Mass Storage endpoint information */
    sink_endpoint = UsbDevice_AllocateEndpointAddress(dev_index,
                                                      TRUE /* is_to_host */);
    if (!sink_endpoint)
    {
        DEBUG_LOG_ERROR("UsbMsc: UsbDevice_AllocateEndpointAddress ERROR");
        Panic();
    }

    ep_info[0].bEndpointAddress = sink_endpoint;
    ep_info[0].bmAttributes = end_point_attr_bulk;
    ep_info[0].wMaxPacketSize = 64;
    ep_info[0].bInterval = 1;
    ep_info[0].extended = NULL;
    ep_info[0].extended_length = 0;

    source_endpoint = UsbDevice_AllocateEndpointAddress(dev_index,
                                                        FALSE /* is_to_host*/);
    if (!source_endpoint)
    {
        DEBUG_LOG_ERROR("UsbMsc: UsbDevice_AllocateEndpointAddress ERROR");
        Panic();
    }

    ep_info[1].bEndpointAddress = source_endpoint;
    ep_info[1].bmAttributes = end_point_attr_bulk;
    ep_info[1].wMaxPacketSize = 64;
    ep_info[1].bInterval = 1;
    ep_info[1].extended = NULL;
    ep_info[1].extended_length = 0;

    /* Add required endpoints to the interface */
    if (!UsbAddEndPoints(intf, 2, ep_info))
    {
        DEBUG_LOG_ERROR("UsbMsc: UsbAddEndPoints ERROR");
        Panic();
    }

    data = (usb_msc_class_data_t *)
            PanicUnlessMalloc(sizeof(usb_msc_class_data_t));
    memset(data, 0, sizeof(usb_msc_class_data_t));

    class_task.handler = usbMsc_Handler;

    data->class_sink = StreamUsbClassSink(intf);
    data->class_source = StreamSourceFromSink(data->class_sink);
    MessageStreamTaskFromSink(data->class_sink, &class_task);

    data->ep_sink = StreamUsbEndPointSink(sink_endpoint);
    MessageStreamTaskFromSink(data->ep_sink, &class_task);
    (void)SinkConfigure(data->ep_sink,
                        VM_SINK_MESSAGES, VM_MESSAGES_NONE);


    data->ep_source = StreamUsbEndPointSource(source_endpoint);
    MessageStreamTaskFromSource(data->ep_source, &class_task);
    (void)SourceConfigure(data->ep_source,
                        VM_SOURCE_MESSAGES, VM_MESSAGES_ALL);

    msc_class_data = data;

    UsbMsc_ScsiInit();

    UsbMsc_Fat16_ConfigureDataArea(data,
                            config->data_area.file,
                            config->data_area.size, NULL);
    UsbMsc_Fat16_ConfigureFat(data,
                       config->table.file,
                       config->table.size, NULL);
    UsbMsc_Fat16_ConfigureRootDir(data,
                           config->root_dir.file,
                           config->root_dir.size, NULL);

    UsbMsc_Fat16_Initialise(data);

    return (usb_class_context_t)msc_class_data;
}

static usb_result_t usbMsc_Destroy(usb_class_context_t context)
{
    if (!msc_class_data ||
            (usb_class_context_t)msc_class_data != context)
    {
        return USB_RESULT_NOT_FOUND;
    }

    free(msc_class_data);
    msc_class_data = NULL;

    DEBUG_LOG_INFO("UsbMsc: closed");

    return USB_RESULT_OK;
}

const usb_class_interface_cb_t UsbMsc_Callbacks =
{
        .Create = usbMsc_Create,
        .Destroy = usbMsc_Destroy,
        .SetInterface = NULL
};

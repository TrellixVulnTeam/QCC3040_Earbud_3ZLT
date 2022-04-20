/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      USB HID: Datalink interface
*/

#include "usb_hid_datalink.h"
#include "usb_hid_class.h"

#include "logging.h"

#include <csrtypes.h>

#include <usb_device.h>
#include <usb_device_utils.h>
#include <usb.h>

#include <message.h>

#include <panic.h>
#include <sink.h>
#include <source.h>
#include <string.h>
#include <stream.h>
#include <stdlib.h>

TaskData hid_datalink_task;

/*! Report id and size */
typedef struct
{
    /*! Report ID */
    uint8 report_id;
    /*! Report type */
    usb_hid_report_main_t report_type;
    /*! Report size in bytes */
    uint16 report_size;
} report_config;

/*! USB HID class context data structure */
typedef struct usb_hid_dl_t
{

    Sink class_sink;
    Source class_source;
    Sink ep_sink;
    Source ep_source;
    uint8 idle_rate;

    /*! Number of HID reports. */
    uint8 num_reports;
    /*! Array of HID reports. */
    report_config *reports;
} usb_hid_dl_t;

static usb_hid_dl_t *hid_dl_data;

usb_hid_handler_t *hid_datalink_handlers;
unsigned num_hid_datalink_handlers;

static report_config *usbHid_FindReport(uint8 report_id,
                                        report_config *reports,
                                        uint8 num_reports);

void UsbHid_Datalink_RegisterHandler(usb_hid_handler_t handler)
{
    hid_datalink_handlers = (usb_hid_handler_t *)
                realloc(hid_datalink_handlers,
                        (num_hid_datalink_handlers + 1) * sizeof(usb_hid_handler_t));
    PanicZero(hid_datalink_handlers);

    hid_datalink_handlers[num_hid_datalink_handlers] = handler;
    num_hid_datalink_handlers++;
}

void UsbHid_Datalink_UnregisterHandler(usb_hid_handler_t handler)
{
    int i;

    for (i=0; i < num_hid_datalink_handlers; i++)
    {
        if (hid_datalink_handlers[i] == handler)
        {
            memmove(&hid_datalink_handlers[i], &hid_datalink_handlers[i+1],
                    ((num_hid_datalink_handlers - 1) - i) * sizeof(usb_hid_handler_t));
            num_hid_datalink_handlers--;
            break;
        }
    }
}

usb_result_t UsbHid_Datalink_SendReport(uint8 report_id,
                                        const uint8 *report_data,
                                        uint16 data_size)
{
    usb_hid_dl_t *data = hid_dl_data;

    if (!data || !data->ep_sink)
    {
        return USB_RESULT_NOT_FOUND;
    }

    uint16 transfer_size;
    uint16 data_offset = 0;

    /* report_id "0" is reserved and indicates that device has only one report
     * and report id does not need to be specified. */
    if (report_id)
    {
        /* reserve space for report_id */
        data_offset = 1;
    }

    report_config *r = usbHid_FindReport(report_id,
                                         data->reports,
                                         data->num_reports);
    if (r && r->report_type == HID_REPORT_INPUT)
    {
        transfer_size = data_offset + r->report_size;
    }
    else
    {
        DEBUG_LOG_WARN("UsbHid:DL SendReport - unknown report\n");
        transfer_size = data_size + data_offset;
    }

    uint8 *sink_data = SinkMapClaim(data->ep_sink, transfer_size);

    if (!sink_data)
    {
        DEBUG_LOG_ERROR("UsbHid:DL SendReport - cannot claim sink space\n");
        return USB_RESULT_NO_SPACE;
    }

    if (report_id)
    {
        sink_data[0] = report_id;
    }
    /* copy report data, truncate if data_size > report_size */
    memcpy(&sink_data[data_offset],
           report_data,
           MIN(data_size, transfer_size - data_offset));
    /* if necessary pad with zeros to the report size */
    if (transfer_size - data_offset > data_size)
    {
        memset(&sink_data[data_size + data_offset], 0,
               transfer_size - data_offset - data_size);
    }

    if (!SinkFlush(data->ep_sink, transfer_size))
    {
        DEBUG_LOG_ERROR("UsbHid:DL SendReport - failed to send data\n");
        Panic();
    }

    return USB_RESULT_OK;
}

static void usbHid_DatalinkEndpointData(void)
{
    usb_hid_dl_t *data = hid_dl_data;
    Source source = data->ep_source;
    uint16 packet_size;

    while ((packet_size = SourceBoundary(source)) != 0)
    {
        const uint8 *report_data = SourceMap(source);
        /* only makes sense if complete == TRUE */
        uint8 report_id = report_data[0];

        int i;
        for (i=0; i < num_hid_datalink_handlers; i++)
        {
            hid_datalink_handlers[i](report_id, report_data, packet_size);
        }

        SourceDrop(source, packet_size);
    }
}

static void usbHid_ClassRequestHandler(void)
{
    usb_hid_dl_t *data = hid_dl_data;
    Source source = data->class_source;
    Sink sink = data->class_sink;
    uint16 packet_size;

    while ((packet_size = SourceBoundary(source)) != 0)
    {
        UsbResponse resp;
        bool response_sent = FALSE;
        /* Build the response. It must contain the original request, so copy
           from the source header. */
        memcpy(&resp.original_request, SourceMapHeader(source), sizeof(UsbRequest));

        /* Set the response fields to default values to make the code below simpler */
        resp.success = FALSE;
        resp.data_length = 0;

        switch (resp.original_request.bRequest)
        {
            case HID_GET_REPORT:
                /* GET_REPORT is not supported */
                break;

            case HID_GET_IDLE:
            {
                uint8 *out;
                if ((out = SinkMapClaim(sink, 1)) != 0)
                {
                    DEBUG_LOG_DEBUG("UsbHid:DL Get_Idle wValue=0x%X wIndex=0x%X",
                                    resp.original_request.wValue,
                                    resp.original_request.wIndex);
                    out[0] = data->idle_rate;
                    resp.success = TRUE;
                    resp.data_length = 1;
                }
                break;
            }

            case HID_SET_REPORT:
            {
                uint16 size_data = resp.original_request.wLength;
                uint8 report_id = resp.original_request.wValue & 0xff;
                DEBUG_LOG_INFO("UsbHid:DL Set_Report wValue=0x%X wIndex=0x%X wLength=0x%X",
                               resp.original_request.wValue,
                               resp.original_request.wIndex,
                               resp.original_request.wLength);

                resp.success = TRUE;

                if (size_data)
                {
                    /* First, acknowledge control transfer, this allows
                     * the host to start sending the next transfer.
                     * Careful: need to make sure Source buffer is big enough
                     * for 2x largest requests. */
                    (void) SinkClaim(sink, 1);
                    (void) SinkFlushHeader(sink, 1,(uint16 *) &resp, sizeof(UsbResponse));
                    response_sent = TRUE;

                    const uint8 *report_data = SourceMap(source);
                    size_data = MIN(packet_size, size_data);

                    int i;
                    for (i=0; i < num_hid_datalink_handlers; i++)
                    {
                        hid_datalink_handlers[i](report_id, report_data, size_data);
                    }
                }
                break;
            }

            case HID_SET_IDLE:
                 DEBUG_LOG_INFO("UsbHid:DL Set_Idle wValue=0x%X wIndex=0x%X",
                                resp.original_request.wValue,
                                resp.original_request.wIndex);
                data->idle_rate = (resp.original_request.wValue >> 8) & 0xff;
                resp.success = TRUE;
                break;

            default:
            {
                 DEBUG_LOG_ERROR("UsbHid:DL req=0x%X wValue=0x%X HID wIndex=0x%X wLength=0x%X\n",
                        resp.original_request.bRequest,
                        resp.original_request.wValue,
                        resp.original_request.wIndex,
                        resp.original_request.wLength);
                break;
            }
        }

        if (!response_sent)
        {
            /* Send response */
            if (resp.data_length)
            {
                (void)SinkFlushHeader(sink, resp.data_length, (uint16 *)&resp, sizeof(UsbResponse));
            }
            else
            {
                   /* Sink packets can never be zero-length, so flush a dummy byte */
                (void) SinkClaim(sink, 1);
                (void) SinkFlushHeader(sink, 1, (uint16 *) &resp, sizeof(UsbResponse));
            }
        }
        /* Discard the original request */
        SourceDrop(source, packet_size);
    }
}

static void usbHid_DatalinkHandler(Task task, MessageId id, Message message)
{
    usb_hid_dl_t *data = hid_dl_data;

    UNUSED(task);

    if (!data)
    {
        return;
    }

    if (id == MESSAGE_MORE_DATA)
    {
        Source source = ((MessageMoreData *)message)->source;

        if (source == data->class_source)
        {
            usbHid_ClassRequestHandler();
        }
        else if (source == data->ep_source)
        {
            usbHid_DatalinkEndpointData();
        }
    }
}

/*! Parse report descriptor and return list of all reports. */
static report_config *usbHid_GetReports(const uint8 *report_desc,
                                        uint16 report_desc_size,
                                        uint8 *return_num_reports)
{
    int num_reports = 0;
    report_config *cfg = NULL;

    const uint8 *ptr = report_desc;
    uint16 size = report_desc_size;

    unsigned report_size = 0;
    unsigned report_count = 0;
    unsigned report_id = 0;

    while (size)
    {
        int bSize = *ptr & 3;
        int bType = (*ptr >> 2) & 3;
        int bTag =(*ptr >> 4) & 0xf;

        unsigned value = 0;
        int i;
        for (i=0; i < bSize; i++)
        {
            value += ptr[1+i] << (i*8);
        }

        switch (bType)
        {
        case HID_REPORT_MAIN:
            if (bTag == HID_REPORT_OUTPUT ||
                    bTag == HID_REPORT_FEATURE ||
                    bTag == HID_REPORT_INPUT)
            {
                cfg = realloc(cfg, sizeof(*cfg) * (num_reports + 1));
                PanicNull(cfg);
                /* create report */
                cfg[num_reports].report_id = report_id;
                cfg[num_reports].report_type = bTag;
                cfg[num_reports].report_size = (report_size * report_count + 7) / 8;
                DEBUG_LOG_VERBOSE("UsbHid:DL CREATE type enum:usb_hid_report_main_t:%d ID %u size %u",
                                  cfg[num_reports].report_type,
                                  cfg[num_reports].report_id,
                                  cfg[num_reports].report_size);
                num_reports += 1;
            }
            break;
        case HID_REPORT_GLOBAL:
            switch (bTag)
            {
            case HID_REPORT_ID:
                report_id = value;
                DEBUG_LOG_VERBOSE("UsbHid:DL REPORT_ID %u", value);
                break;
            case HID_REPORT_SIZE:
                report_size = value;
                DEBUG_LOG_VERBOSE("UsbHid:DL REPORT_SIZE %u", value);
                break;
            case HID_REPORT_COUNT:
                report_count = value;
                DEBUG_LOG_VERBOSE("UsbHid:DL REPORT_COUNT %u", value);
                break;
            default:
                break;
            }
            break;
        default:
            break;
        }

        int skip = 1 + bSize;

        ptr += skip;
        size -= MIN(size, skip);
    }

    if (return_num_reports)
    {
        *return_num_reports = num_reports;
    }
    return cfg;
}

/*! Find report config by report id  */
static report_config *usbHid_FindReport(uint8 report_id, report_config *reports, uint8 num_reports)
{
    int i;

    for (i=0; i < num_reports; i++)
    {
        report_config *r = &reports[i];

        if (r->report_id == report_id)
        {
            return r;
        }

    }
    return NULL;
}

/*! Find the size of the longest from-host report */
static uint16 usbHid_GetMaxFromHostReportSize(report_config *reports, uint8 num_reports)
{
    uint16 max_report_size = 0;
    bool use_report_id = FALSE;

    if (reports)
    {
        int i;
        for (i=0; i < num_reports; i++)
        {
            report_config *r = &reports[i];

            if (r->report_id != 0)
            {
                use_report_id = TRUE;
            }

            /* from-host reports are OUTPUT and FEATURE */
            if (r->report_type != HID_REPORT_OUTPUT &&
                    r->report_type != HID_REPORT_FEATURE)
            {
                continue;
            }

            if (r->report_size > max_report_size)
            {
                max_report_size = r->report_size;
            }
        }
    }
    if (use_report_id)
    {
        /* reserve 1 byte for report_id */
        max_report_size += 1;
    }
    return max_report_size;
}


static usb_class_context_t usbHid_Datalink_Create(usb_device_index_t dev_index,
                                  usb_class_interface_config_data_t config_data)
{
    UsbCodes codes;
    UsbInterface intf;
    EndPointInfo ep_info[2];
    uint8 source_endpoint = 0, sink_endpoint = 0;
    uint16 source_max_packet_size = 0;
    usb_hid_dl_t *data;
    const usb_hid_config_params_t *config = (const usb_hid_config_params_t *)config_data;

    DEBUG_LOG_INFO("UsbHid:DL Datalink");

    if (hid_dl_data)
    {
        DEBUG_LOG_ERROR("UsbHid:DL ERROR - class already present");
        Panic();
    }

    if (!config)
    {
        DEBUG_LOG_ERROR("UsbHid:DL ERROR - configuration not provided");
        Panic();
    }

    /* HID no boot codes */
    codes.bInterfaceClass    = B_INTERFACE_CLASS_HID;
    codes.bInterfaceSubClass = B_INTERFACE_SUB_CLASS_HID_NO_BOOT;
    codes.bInterfaceProtocol = B_INTERFACE_PROTOCOL_HID_NO_BOOT;
    codes.iInterface         = 0;

    intf = UsbAddInterface(&codes, B_DESCRIPTOR_TYPE_HID,
                           config->class_desc->descriptor,
                           config->class_desc->size_descriptor);

    if (intf == usb_interface_error)
    {
        DEBUG_LOG_ERROR("UsbHid:DL UsbAddInterface ERROR");
        Panic();
    }

    /* Register HID Datalink report descriptor with the interface */
    if (!UsbAddDescriptor(intf, B_DESCRIPTOR_TYPE_HID_REPORT,
                          config->report_desc->descriptor,
                          config->report_desc->size_descriptor))
    {
        DEBUG_LOG_ERROR("UsbHid:DL UsbAddDescriptor ERROR");
        Panic();
    }

    /* USB HID endpoint information */
    assert(config->num_endpoints == 1 || config->num_endpoints == 2);

    int ep_index;
    for (ep_index = 0; ep_index < config->num_endpoints; ep_index++)
    {
        const usb_hid_endpoint_desc_t *ep_config = &config->endpoints[ep_index];

        uint8 endpoint = UsbDevice_AllocateEndpointAddress(dev_index, ep_config->is_to_host);
        if (!endpoint)
        {
            DEBUG_LOG_ERROR("UsbHid:DL UsbDevice_AllocateEndpointAddress ERROR");
            Panic();
        }

        ep_info[ep_index].bEndpointAddress = endpoint;
        ep_info[ep_index].bmAttributes = end_point_attr_int;
        ep_info[ep_index].wMaxPacketSize = ep_config->wMaxPacketSize;
        ep_info[ep_index].bInterval = ep_config->bInterval;
        ep_info[ep_index].extended = NULL;
        ep_info[ep_index].extended_length = 0;

        if (ep_config->is_to_host)
        {
            sink_endpoint = endpoint;
        }
        else
        {
            source_endpoint = endpoint;
            source_max_packet_size = ep_config->wMaxPacketSize;
        }
    }

    /* Add required endpoints to the interface */
    if (!UsbAddEndPoints(intf, config->num_endpoints, ep_info))
    {
        DEBUG_LOG_ERROR("UsbHid:DL UsbAddEndPoints ERROR");
        Panic();
    }

    hid_datalink_task.handler = usbHid_DatalinkHandler;

    data = (usb_hid_dl_t *)
            PanicUnlessMalloc(sizeof(usb_hid_dl_t));
    memset(data, 0, sizeof(usb_hid_dl_t));

    data->class_sink = StreamUsbClassSink(intf);
    data->class_source = StreamSourceFromSink(data->class_sink);
    MessageStreamTaskFromSink(data->class_sink, &hid_datalink_task);
    (void)SinkConfigure(data->class_sink,
                        VM_SINK_MESSAGES, VM_MESSAGES_SOME);

    if (sink_endpoint)
    {
        data->ep_sink = StreamUsbEndPointSink(sink_endpoint);
        MessageStreamTaskFromSink(data->ep_sink, &hid_datalink_task);
        (void)SinkConfigure(data->ep_sink,
                            VM_SINK_MESSAGES, VM_MESSAGES_NONE);
    }
    else
    {
        data->ep_sink = 0;
    }

    if (source_endpoint)
    {
        data->ep_source = StreamUsbEndPointSource(source_endpoint);
        MessageStreamTaskFromSource(data->ep_source, &hid_datalink_task);
        (void)SourceConfigure(data->ep_source,
                            VM_SOURCE_MESSAGES, VM_MESSAGES_SOME);

        /* parse report descriptor and find all reports */
        data->reports = usbHid_GetReports(config->report_desc->descriptor,
                                          config->report_desc->size_descriptor,
                                          &data->num_reports);

        if (data->reports)
        {
            /* Configure USB transfer size if the longest report size
             * is a multiple of wMaxPacketSize */
            uint16 max_report_size = usbHid_GetMaxFromHostReportSize(data->reports,
                                                                     data->num_reports);

            DEBUG_LOG_VERBOSE("UsbHid:DL max report size %u", max_report_size);

            if (max_report_size &&
                (max_report_size % source_max_packet_size) == 0)
            {
                /* Longest report size is a multiple of wMaxPacketSize.
                 *
                 * USB HID 1.1, 8.4 Report Constraints: "All reports except
                 * the longest which exceed wMaxPacketSize for the endpoint must
                 * terminate with a short packet. The longest report does not require
                 * a short packet terminator."
                 *
                 * Configure USB HID endpoint to end transfers once they reach
                 * the longest report size. */
                bool result = SourceConfigure(data->ep_source,
                                              VM_SOURCE_USB_TRANSFER_LENGTH,
                                              max_report_size);

                DEBUG_LOG_WARN("UsbHid:DL set VM_SOURCE_USB_TRANSFER_LENGTH = %u, result %d",
                               max_report_size,
                               result);
            }
        }
    }
    else
    {
        data->ep_source = 0;
    }

    hid_dl_data = data;

    return (usb_class_context_t)hid_dl_data;
}

static usb_result_t usbHid_Datalink_Destroy(usb_class_context_t context)
{
    if (!hid_dl_data ||
            (usb_class_context_t)hid_dl_data != context)
    {
        return USB_RESULT_NOT_FOUND;
    }

    free(hid_dl_data);
    hid_dl_data = NULL;

    DEBUG_LOG_INFO("UsbHid:DL closed");

    return USB_RESULT_OK;
}


const usb_class_interface_cb_t UsbHid_Datalink_Callbacks =
{
        .Create = usbHid_Datalink_Create,
        .Destroy = usbHid_Datalink_Destroy,
        .SetInterface = NULL
};

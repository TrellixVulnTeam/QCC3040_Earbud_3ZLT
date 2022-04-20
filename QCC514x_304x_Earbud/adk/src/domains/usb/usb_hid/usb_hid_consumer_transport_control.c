/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      USB HID: Consumer Transport Control interface
*/

#include "usb_hid_consumer_transport_control.h"
#include "usb_hid_class.h"
#include "usb_source.h"

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

/*! size of USB HID consumer transport & telephony usage page report size */
#define USB_HID_CT_TELEPHONY_USAGE_REPORT_SIZE    0x02      /* Only 2 is supported */

typedef enum
{
    STATE_ON = 1,
    STATE_OFF = 2,
    STATE_TOGGLE = STATE_ON | STATE_OFF
} key_state_t;

typedef struct
{
    uint16 key_code;
    uint16 key_state;
} event_key_map_t;

typedef enum
{
    PLAY_PAUSE = 1,
    STOP = 8,
    NEXT_TRACK = 2,
    PREVIOUS_TRACK = 4,
    PLAY = 16,
    PAUSE = 32,
    VOL_UP = 256,
    VOL_DOWN = 512,
    MUTE = 1024,
    FFWD = 64,
    RWD = 128,
} consumer_transport_keys;

typedef enum
{
    PHONE_MUTE = 1,
    HOOK_SWITCH = 2,
    FLASH = 4,
    BUTTON_ONE = 8,
} telephony_keys_t;

static const event_key_map_t event_key_map[] =
{
    /* USB_SOURCE_PLAY_PAUSE */
    {PLAY_PAUSE, STATE_TOGGLE},

    /*  USB_SOURCE_STOP */
    {STOP, STATE_TOGGLE},

    /* USB_SOURCE_NEXT_TRACK */
    {NEXT_TRACK, STATE_TOGGLE},

    /* USB_SOURCE_PREVIOUS_TRACK */
    {PREVIOUS_TRACK, STATE_TOGGLE},

    /* USB_SOURCE_PLAY */
    {PLAY, STATE_ON},

    /* USB_SOURCE_PAUSE */
    {PAUSE, STATE_ON},

    /* USB_SOURCE_VOL_UP */
    {VOL_UP, STATE_TOGGLE},

    /* USB_SOURCE_VOL_DOWN */
    {VOL_DOWN, STATE_TOGGLE},

    /* USB_SOURCE_MUTE */
    {MUTE, STATE_TOGGLE},

    /* USB_SOURCE_FFWD_ON */
    {FFWD, STATE_ON},

    /* USB_SOURCE_FFWD_OFF */
    {FFWD, STATE_OFF},

    /* USB_SOURCE_REW_ON */
    {RWD, STATE_ON},

    /* USB_SOURCE_REW_OFF */
    {RWD, STATE_OFF},

    /* USB_SOURCE_PHONE_MUTE */
    {PHONE_MUTE, STATE_TOGGLE},

    /* USB_SOURCE_HOOK_SWITCH_ANSWER */
    {HOOK_SWITCH, STATE_ON},

    /* USB_SOURCE_HOOK_SWITCH_TERMINATE */
    {HOOK_SWITCH, STATE_OFF},

    /* USB_SOURCE_FLASH */
    {FLASH, STATE_TOGGLE},

    /* USB_SOURCE_BUTTON_ONE */
    {BUTTON_ONE, STATE_TOGGLE},
};

static usb_result_t usbHid_ConsumerTransporControl_SendEvent(usb_source_control_event_t event);

static uint16 telephony_key_evt_data;
static TaskData hid_consumer_task;
static usb_rx_hid_event_handler_t usb_hid_event_handler;

typedef struct usb_hid_ct_t
{
    Sink class_sink;
    Source class_source;
    Sink ep_sink;
    uint8 idle_rate;
} usb_hid_ct_t;

static usb_hid_ct_t *hid_ct_data;

static bool usbHid_GetHookSwitchStatus(void)
{
    return ((telephony_key_evt_data & HOOK_SWITCH) == HOOK_SWITCH);
}

static bool usbHid_HandleConsumerSetReport(uint8 report_id, uint8 data_size, const uint8 *data)
{
    if(data == NULL || data_size <= 1 || data[0] != report_id)
    {
        return FALSE;
    }

    switch (report_id)
    {
        case USB_HID_LED_MUTE_REPORT_ID:
            DEBUG_LOG("UsbHid:CT USB_HID_LED_MUTE_REPORT_ID 0x%X",data[1]);
            if(usb_hid_event_handler)
            {
                usb_hid_event_handler(USB_SOURCE_RX_HID_MUTE_EVT, data[1]);
            }
            break;
        case USB_HID_LED_OFF_HOOK_REPORT_ID:
            DEBUG_LOG("UsbHid:CT USB_HID_LED_OFF_HOOK_REPORT_ID 0x%X",data[1]);
            if(usb_hid_event_handler)
            {
                usb_hid_event_handler(USB_SOURCE_RX_HID_OFF_HOOK_EVT, data[1]);
            }

            if (data[1] && !usbHid_GetHookSwitchStatus())
            {
                usbHid_ConsumerTransporControl_SendEvent(USB_SOURCE_HOOK_SWITCH_ANSWER);
            }
            else if(!data[1] && usbHid_GetHookSwitchStatus())
            {
                usbHid_ConsumerTransporControl_SendEvent(USB_SOURCE_HOOK_SWITCH_TERMINATE);
            }
            break;
        case USB_HID_LED_RING_REPORT_ID:
            DEBUG_LOG("UsbHid:CT USB_HID_LED_RING_REPORT_ID 0x%X",data[1]);
            if(usb_hid_event_handler)
            {
                usb_hid_event_handler(USB_SOURCE_RX_HID_RING_EVT, data[1]);
            }
            break;
        default:
            DEBUG_LOG_WARN("UsbHid:CT SetReport report_id 0x%X length 0x%X ",report_id, data_size);
            break;
    }
    /* SET_REPORT is not handled* for Consumer Transport. */
    return TRUE;
}

static void usbHid_ConsumerHandler(Task task, MessageId id, Message message)
{
    Source source;
    Sink sink;
    uint16 packet_size;
    usb_hid_ct_t *data = hid_ct_data;

    UNUSED(task);

    if (id != MESSAGE_MORE_DATA)
    {
        return;
    }

    source = ((MessageMoreData *)message)->source;


    if (!data || data->class_source != source)
    {
        return;
    }

    sink = data->class_sink;

    while ((packet_size = SourceBoundary(source)) != 0)
    {
        UsbResponse resp;
        /* Build the response. It must contain the original request, so copy
           from the source header. */
        memcpy(&resp.original_request, SourceMapHeader(source), sizeof(UsbRequest));

        /* Set the response fields to default values to make the code below simpler */
        resp.success = FALSE;
        resp.data_length = 0;

        switch (resp.original_request.bRequest)
        {
            case HID_GET_REPORT:
                DEBUG_LOG_INFO("UsbHid:CT Get_Report wValue=0x%X wIndex=0x%X wLength=0x%X",
                               resp.original_request.wValue,
                               resp.original_request.wIndex,
                               resp.original_request.wLength);
                break;

            case HID_GET_IDLE:
            {
                uint8 *out;
                if ((out = SinkMapClaim(sink, 1)) != 0)
                {
                     DEBUG_LOG_INFO("UsbHid:CT Get_Idle wValue=0x%X wIndex=0x%X",
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
                uint16 data_size = resp.original_request.wLength;
                uint8 report_id = resp.original_request.wValue & 0xff;
                const uint8 *source_data = SourceMap(source);

                DEBUG_LOG_VERBOSE("UsbHid:CT Set_Report wValue=0x%X wIndex=0x%X wLength=0x%X",
                           resp.original_request.wValue,
                           resp.original_request.wIndex,
                           resp.original_request.wLength);

                if (packet_size != data_size)
                {
                    DEBUG_LOG_WARN("UsbHid:CT Set_Report Length Mismatch = 0x%X 0x%X",
                                   packet_size, data_size);
                }

                resp.success = usbHid_HandleConsumerSetReport(report_id, packet_size, source_data);;
                break;
            }

            case HID_SET_IDLE:
                DEBUG_LOG_INFO("UsbHid:CT Set_Idle wValue=0x%X wIndex=0x%X",
                               resp.original_request.wValue,
                               resp.original_request.wIndex);
                data->idle_rate = (resp.original_request.wValue >> 8) & 0xff;
                resp.success = TRUE;
                break;

            default:
                DEBUG_LOG_INFO("UsbHid:CT req=0x%X wValue=0x%X wIndex=0x%X wLength=0x%X",
                               resp.original_request.bRequest,
                               resp.original_request.wValue,
                               resp.original_request.wIndex,
                               resp.original_request.wLength);
                break;
        }

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
        /* Discard the original request */
        SourceDrop(source, packet_size);
    }
}

static void consumerTransport_SendKeyEvent(Sink ep_sink, uint8 report_id, uint16 key, uint16 state)
{
    Sink sink = ep_sink;
    uint8 *input_report;

    /* data_size = 1 + size of usage page size;
     * [report_id] [data(0)], [data(1)], ... data(report_size-1) */
    uint16 data_size = 1+USB_HID_CT_TELEPHONY_USAGE_REPORT_SIZE;
    /* Only data_size = 3 is supported */
    PanicFalse(data_size == 3);

    if ((input_report = SinkMapClaim(sink, data_size)) != 0)
    {
        input_report[0] = report_id;     /* REPORT ID */
        switch(report_id)
        {
            case USB_HID_CONSUMER_TRANSPORT_REPORT_ID:
            {
                if (state)
                {
                    input_report[1] = key & 0xff;        /* key on code */
                    input_report[2] = (key >> 8) & 0xff;   /* key on code */
                }
                else
                {
                    input_report[1] = 0; /* key released */
                    input_report[2] = 0; /* key released */
                }
                DEBUG_LOG_VERBOSE("UsbHid:CT SendKeyEvent %X %X %X sending", input_report[0], input_report[1], input_report[2]);
                break;
            }
            case USB_HID_TELEPHONY_REPORT_ID:
            {
                /* PHONE_MUTE event should not change status bit of HOOK_SWITCH  */
                if (state)
                {
                    telephony_key_evt_data |= key;
                }
                else
                {
                    telephony_key_evt_data &= ~key;
                }
                input_report[1] = telephony_key_evt_data & 0xff;        /* key on code */
                input_report[2] = (telephony_key_evt_data >> 8) & 0xff;   /* key on code */
                DEBUG_LOG_VERBOSE("UsbHid:telephony_SendKeyEvent %X %X %X sending", input_report[0], input_report[1], input_report[2]);
                break;
            }
            default:
            {
                DEBUG_LOG_ERROR("UsbHid:CT Unsupported report_id %d ", report_id);
                Panic();
                break;
            }
        }

        DEBUG_LOG_VERBOSE("UsbHid:CT report_id %d  key event %d state %d sending", report_id, key, state);
        /* Flush data */
        (void) SinkFlush(sink, data_size);
    }
    else
    {
        DEBUG_LOG_WARN("UsbHid:CT key event %d state %d dropped", key, state);
    }
}

static usb_result_t usbHid_ConsumerTransporControl_SendEvent(usb_source_control_event_t event)
{
    uint16 key_code;
    key_state_t key_state;
    usb_hid_ct_t *data = hid_ct_data;

    if (!data ||
            event >= USB_SOURCE_EVT_COUNT)
    {
        return USB_RESULT_NOT_FOUND;
    }

    DEBUG_LOG_INFO("UsbHid:CT send event %d", event);

    key_code = event_key_map[event].key_code;
    key_state = event_key_map[event].key_state;
    uint8 report_id = USB_HID_CONSUMER_TRANSPORT_REPORT_ID;
    if(event >= USB_SOURCE_PHONE_MUTE)
    {
        report_id = USB_HID_TELEPHONY_REPORT_ID;
    }
    if (key_state & STATE_ON)
    {
        consumerTransport_SendKeyEvent(data->ep_sink, report_id, key_code, 1);
    }
    if (key_state & STATE_OFF)
    {
        consumerTransport_SendKeyEvent(data->ep_sink, report_id, key_code, 0);
    }

    return USB_RESULT_OK;
}

/* Source app using it to send vendor specific data */
static usb_result_t usbHid_ConsumerTransporControl_SendReport(const uint8 *report, uint16 size)
{
    usb_hid_ct_t *data = hid_ct_data;
    uint8 *report_data;

    if (!data)
    {
        return USB_RESULT_NOT_FOUND;
    }

    DEBUG_LOG_INFO("UsbHid:CT send report 0x%x size %d",
            report[0], size);

    report_data = SinkMapClaim(data->ep_sink, size);

    if (report_data)
    {
        /* report ID is first byte */
        memcpy(report_data, report, size);

        return SinkFlush(data->ep_sink, size) ?
                    USB_RESULT_OK :
                    USB_RESULT_FAIL;
    }

    return USB_RESULT_NO_SPACE;
}

/*! Register handler for receiving HID events */
static void usbHid_Register_handler(usb_rx_hid_event_handler_t handler)
{
    usb_hid_event_handler = handler;
}

/*! Unregister handler for receiving HID events */
static void usbHid_Unregister_handler(void)
{
    usb_hid_event_handler = NULL;
}

static usb_source_hid_interface_t usb_hid_consumer_transport_interface =
{
        .send_event = usbHid_ConsumerTransporControl_SendEvent,
        .send_report = usbHid_ConsumerTransporControl_SendReport,
        .register_handler = usbHid_Register_handler,
        .unregister_handler = usbHid_Unregister_handler
};

static usb_class_context_t usbHid_ConsumerTransporControl_Create(usb_device_index_t dev_index,
                                  usb_class_interface_config_data_t config_data)
{
    UsbCodes codes;
    UsbInterface intf;
    EndPointInfo ep_info;
    uint8 endpoint;
    usb_hid_ct_t *data;
    const usb_hid_config_params_t *config = (const usb_hid_config_params_t *)config_data;

    DEBUG_LOG_INFO("UsbHid:CT Consumer Transport");

    if (hid_ct_data)
    {
        DEBUG_LOG_ERROR("UsbHid:CT ERROR - class already present");
        Panic();
    }

    if (!config)
    {
        DEBUG_LOG_ERROR("UsbHid:CT ERROR - configuration not provided");
        Panic();
    }

    /* Initializing telephony_key_evt_data */
    telephony_key_evt_data = 0;

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
        DEBUG_LOG_ERROR("UsbHid:CT UsbAddInterface ERROR");
        Panic();
    }

    /* Register HID Consumer Control Device report descriptor with the interface */
    if (!UsbAddDescriptor(intf, B_DESCRIPTOR_TYPE_HID_REPORT,
                          config->report_desc->descriptor,
                          config->report_desc->size_descriptor))
    {
        DEBUG_LOG_ERROR("UsbHid:CT sbAddDescriptor ERROR");
        Panic();
    }


    /* USB HID endpoint information */
    assert(config->num_endpoints == 1);

    endpoint = UsbDevice_AllocateEndpointAddress(dev_index,
                                                 config->endpoints[0].is_to_host);
    if (!endpoint)
    {
        DEBUG_LOG_ERROR("UsbHid:CT UsbDevice_AllocateEndpointAddress ERROR");
        Panic();
    }

    ep_info.bEndpointAddress = endpoint;
    ep_info.bmAttributes = end_point_attr_int;
    ep_info.wMaxPacketSize = config->endpoints[0].wMaxPacketSize;
    ep_info.bInterval = config->endpoints[0].bInterval;
    ep_info.extended = NULL;
    ep_info.extended_length = 0;

    /* Add required endpoints to the interface */
    if (!UsbAddEndPoints(intf, 1, &ep_info))
    {
        DEBUG_LOG_ERROR("UsbHid:CT UsbAddEndPoints ERROR");
        Panic();
    }

    hid_consumer_task.handler = usbHid_ConsumerHandler;

    data = (usb_hid_ct_t *)
            PanicUnlessMalloc(sizeof(usb_hid_ct_t));
    memset(data, 0, sizeof(usb_hid_ct_t));

    data->class_sink = StreamUsbClassSink(intf);
    data->class_source = StreamSourceFromSink(data->class_sink);
    MessageStreamTaskFromSink(data->class_sink, &hid_consumer_task);
    data->ep_sink = StreamUsbEndPointSink(endpoint);
    hid_ct_data = data;

    UsbSource_RegisterHid(&usb_hid_consumer_transport_interface);

    return (usb_class_context_t)hid_ct_data;
}

static usb_result_t usbHid_ConsumerTransporControl_Destroy(usb_class_context_t context)
{
    if (!hid_ct_data ||
            (usb_class_context_t)hid_ct_data != context)
    {
        return USB_RESULT_NOT_FOUND;
    }

    UsbSource_UnregisterHid();

    free(hid_ct_data);
    hid_ct_data = NULL;

    DEBUG_LOG_INFO("UsbHid:CT closed");

    return USB_RESULT_OK;
}

const usb_class_interface_cb_t UsbHid_ConsumerTransport_Callbacks =
{
        .Create = usbHid_ConsumerTransporControl_Create,
        .Destroy = usbHid_ConsumerTransporControl_Destroy,
        .SetInterface = NULL
};

/*******************************************************************************
Copyright (c) 2016 - 2021 Qualcomm Technologies International, Ltd.
 

FILE NAME
    hid_upgrade.c

DESCRIPTION
    Implementation for the USB HID Upgrade Transport.
*******************************************************************************/

#ifndef USB_DEVICE_CLASS_REMOVE_HID

#include <string.h>
#include <stdlib.h>

#include <upgrade.h>
#include <usb_device_class.h>
#include <panic.h>

#include "hid_upgrade.h"

TaskData hid_upgrade_transport_task;

#define HID_REPORTID_UPGRADE_DATA_TRANSFER (5)
#define HID_REPORTID_UPGRADE_RESPONSE      (6)
#define HID_REPORTID_COMMAND               (3)

/* Only connect and disconnect commands are supported. */
#define HID_CMD_CONNECTION_REQ   (0x02)
#define HID_CMD_DISCONNECT_REQ   (0x07)

/* Defines representing the maximum size of the 'data' field within various
   reports (not the size of the entire report). MAX_NUM_PACKETS is chosen based
   on the number of available PMALLOC pools of a given MAX_SIZE, that we can use
   as buffer space, and also affects transfer speed. Adjust with caution! */
#define HID_UPGRADE_RESPONSE_DATA_MAX_SIZE (11)
#define HID_UPGRADE_TRANSFER_DATA_MAX_SIZE (249)
#define HID_UPGRADE_TRANSFER_DATA_MAX_NUM_PACKETS (3)

typedef struct
{
    uint8 size;
    uint8 data[HID_UPGRADE_RESPONSE_DATA_MAX_SIZE];
} hid_upgrade_response_t;

static uint16 hid_upgrade_report_queue_len = 0;
static uint16 hid_upgrade_report_queue_max = 0;
static hid_upgrade_input_report_cb_t hid_upgrade_input_report_cb = NULL;

void HidUpgradeRegisterInputReportCb(hid_upgrade_input_report_cb_t handler)
{
    hid_upgrade_input_report_cb = handler;
}

static void HidUpgradeSendUpgradeResponse(const uint8 *data, uint16 size)
{
    if (hid_upgrade_input_report_cb && data && size <= HID_UPGRADE_RESPONSE_DATA_MAX_SIZE)
    {
        hid_upgrade_response_t response;

        /* Construct response */
        memset(&response, 0, sizeof(response));
        memcpy(response.data, data, size);
        response.size = (uint8)(size & 0xff);

        /* Send report containing response */
        hid_upgrade_input_report_cb(HID_REPORTID_UPGRADE_RESPONSE, (uint8*)&response, sizeof(response));
    }
}

/* Upgrade Library Message Handling. */
static void HidUpgradeConnectCfmHandler(UPGRADE_TRANSPORT_CONNECT_CFM_T *message);
static void HidUpgradeDisconnectCfmHandler(UPGRADE_TRANSPORT_DISCONNECT_CFM_T *message);
static void HidUpgradeDataIndHandler(UPGRADE_TRANSPORT_DATA_IND_T *message);
static void HidUpgradeDataCfmHandler(UPGRADE_TRANSPORT_DATA_CFM_T *message);

static void HidUpgradeMessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);
    switch(id)
    {
    /* Response from call to UpgradeTransportConnectRequest() */
    case UPGRADE_TRANSPORT_CONNECT_CFM:
        HidUpgradeConnectCfmHandler((UPGRADE_TRANSPORT_CONNECT_CFM_T *)message);
        break;

    /* Response from call to UpgradeTransportDisonnectRequest() */
    case UPGRADE_TRANSPORT_DISCONNECT_CFM:
        HidUpgradeDisconnectCfmHandler((UPGRADE_TRANSPORT_DISCONNECT_CFM_T *)message);
        break;

    /* Request from upgrade library to send a data packet to the host */
    case UPGRADE_TRANSPORT_DATA_IND: /* aka UPGRADE_PROCESS_DATA_IND */
        HidUpgradeDataIndHandler((UPGRADE_TRANSPORT_DATA_IND_T *)message);
        break;

    /* Response from call to UpgradeProcessDataRequest() */
    case UPGRADE_TRANSPORT_DATA_CFM:
        HidUpgradeDataCfmHandler((UPGRADE_TRANSPORT_DATA_CFM_T *)message);
        break;

    default:
        /* Unhandled */
        break;
    }
}

static void HidUpgradeConnectCfmHandler(UPGRADE_TRANSPORT_CONNECT_CFM_T *message)
{
    HidUpgradeSendUpgradeResponse((uint8*)&message->status,
                                  sizeof(message->status));
}

static void HidUpgradeDisconnectCfmHandler(UPGRADE_TRANSPORT_DISCONNECT_CFM_T *message)
{
    /* Do not send response to host for transport disconnect requests,
       it is not expected by the HidDfuApp (for backwards compatibility). */
    /*HidUpgradeSendUpgradeResponse((uint8*)&message->status,
                                  sizeof(message->status));*/
    UNUSED(message);
}

static void HidUpgradeDataIndHandler(UPGRADE_TRANSPORT_DATA_IND_T *message)
{
    HidUpgradeSendUpgradeResponse(message->data,
                                  message->size_data);
}

static void HidUpgradeDataCfmHandler(UPGRADE_TRANSPORT_DATA_CFM_T *message)
{
    if (message->status != upgrade_status_success)
    {
        Panic();
    }

    if (message->data)
    {
        /* Free the memory we allocated earlier when forwarding data to the
           Upgrade library. We should always get one UPGRADE_TRANSPORT_DATA_CFM
           per every call to UpgradeProcessDataRequest(), with the same pointer
           that was originally passed to it, so this should be safe (so long as
           HID messages don't come in faster than we can process them & we have
           enough memory to buffer them - tests seem to indicate that we do). */
        free((uint8*)(message->data));
        hid_upgrade_report_queue_len--;
    }
}

/* SET_REPORT handling functions */
static void HidUpgradeCommandHandler(uint16 data_in_size, const uint8 *data_in);
static void HidUpgradeDataRequestHandler(uint16 data_in_size, const uint8 *data_in);

uint16 HidUpgradeHandleReport(uint16 report_id, uint16 data_in_size, const uint8 *data_in)
{
    uint16 reports_queued = 0;

    PanicNull((uint8 *)data_in);
    PanicZero(data_in_size);

    switch(report_id)
    {
    case HID_REPORTID_COMMAND:
        HidUpgradeCommandHandler(data_in_size, data_in);
        reports_queued = hid_upgrade_report_queue_len;
        if (reports_queued < 1)
        {
            /* Need to indicate "success", even though Command reports are
               processed immediately and so not really "queued". */
            reports_queued = 1;
        }
        break;

    case HID_REPORTID_UPGRADE_DATA_TRANSFER:
        HidUpgradeDataRequestHandler(data_in_size, data_in);
        reports_queued = hid_upgrade_report_queue_len;
        break;

    default:
        /* Unhandled. */
        break;
    }
    return reports_queued;
}

static void HidUpgradeConnect(void);
static void HidUpgradeDisconnect(void);

static void HidUpgradeCommandHandler(uint16 data_in_size, const uint8 *data_in)
{
    UNUSED(data_in_size);

    switch(data_in[0])
    {
    case HID_CMD_CONNECTION_REQ:
        HidUpgradeConnect();
        break;

    case HID_CMD_DISCONNECT_REQ:
        HidUpgradeDisconnect();
        break;

    default:
        /* Unhandled. */
        break;
    }
}

static void HidUpgradeConnect(void)
{
    hid_upgrade_report_queue_len = 0;
    hid_upgrade_transport_task.handler = HidUpgradeMessageHandler;
    /* Connect transport task and request UPGRADE_TRANSPORT_DATA_CFM messages.
       The maximum request size is limited to 3 packets at a time, since they
       are rather large, and we have to buffer them whilst waiting for each
       asynchronous UPGRADE_TRANSPORT_DATA_CFM. This is because they are
       delivered to us synchronously by the USB domain (via callback). */
    UpgradeTransportConnectRequest(&hid_upgrade_transport_task, UPGRADE_DATA_CFM_ALL,
                                   (HID_UPGRADE_TRANSFER_DATA_MAX_SIZE *
                                    HID_UPGRADE_TRANSFER_DATA_MAX_NUM_PACKETS));
}

static void HidUpgradeDisconnect(void)
{
    UpgradeTransportDisconnectRequest();
    /* The Upgrade library ensures that we will always get exactly one
       UPGRADE_TRANSPORT_DATA_CFM per call to UpgradeProcessDataRequest(),
       *even* if we disconnect the transport. The message will still exist on
       the queue, so we will still get it if a disconnect just so happens to
       fall during a ProcessDataRequest. So there is no need to free any memory
       here - it will be free'd when we get the UPGRADE_TRANSPORT_DATA_CFM. */
}

static void HidUpgradeDataRequestHandler(uint16 data_in_size, const uint8 *data_in)
{
    /* UpgradeProcessDataRequest does not buffer data before returning, so we
       must do so here (free'd later once we get UPGRADE_TRANSPORT_DATA_CFM). */
    size_t num_bytes = sizeof(uint8) * data_in_size;
    uint8 *hid_upgrade_data_buffer = PanicUnlessMalloc(num_bytes);
    memcpy(hid_upgrade_data_buffer, data_in, num_bytes);

    UpgradeProcessDataRequest(data_in_size, hid_upgrade_data_buffer);

    /* Keep track of the queue length (memory allocated), for statistics. */
    hid_upgrade_report_queue_len++;
    if (hid_upgrade_report_queue_len > hid_upgrade_report_queue_max)
    {
        hid_upgrade_report_queue_max = hid_upgrade_report_queue_len;
    }
}

uint16 HidUpgradeGetStatsMaxReportQueueLen(void)
{
    return hid_upgrade_report_queue_max;
}

#endif /* !USB_DEVICE_CLASS_REMOVE_HID */

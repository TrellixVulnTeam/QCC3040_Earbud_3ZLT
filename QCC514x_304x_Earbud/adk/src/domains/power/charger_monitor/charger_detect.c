/*!
\copyright  Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Charger Detection
*/
#include "logging.h"

#include "charger_detect.h"
#include "charger_detect_config.h"

#include "charger_monitor.h"

#include "usb_device.h"
#include <usb.h>

#include "panic.h"

#include "charger_data.h"
#include "charger_monitor_config.h"

#ifdef INCLUDE_CHARGER_DETECT

unsigned ChargerDetect_Current(void)
{
    chargerTaskData *data = appGetCharger();
    unsigned current = 0;

    if (data->charger_config)
    {
        current = data->charger_config->current;

        if (data->charger_config->usb_events_apply)
        {
            if (data->usb_suspend)
            {
                current = 0;
            }
            else if (!data->usb_enumerated)
            {
                current = MIN(current,
                              appConfigChargerUsbUnconfiguredCurrent());
            }
        }
        if (data->charger_config->current_limiting)
        {
            current = MIN(current,
                          data->current_limit);
        }
    }

    return current;
}

static void chargerDetect_Resolved(bool is_connected)
{
    chargerTaskData *data = appGetCharger();

    if (data->charger_config->current_limiting)
    {
        data->current_limit = appConfigChargerDCPMinCurrent();
        data->current_increasing = 1;
        data->current_limit_detected = 0;

        Charger_VChgMonitorStart(data);
    }
    else
    {
        Charger_VChgMonitorStop(data);
    }

    data->usb_enumerated = 0;
    /* get up to date USB suspend status */
    data->usb_suspend = (UsbDeviceState() == usb_device_state_suspended) ? 1:0;

    Charger_UpdateConnected(is_connected);
    Charger_UpdateCurrent();

    /* new charger might need to react to USB suspend events */
    Charger_HandleChange();
}

void ChargerDetect_Detected(MessageChargerDetected *msg)
{
    chargerTaskData *data = appGetCharger();

    DEBUG_LOG("ChargerDetect: MSG detected %d", msg->attached_status);

    const charger_config_t *charger_config = ChargerDetect_GetConfig(msg);

    if (!charger_config ||
            data->charger_config == charger_config)
    {
        return;
    }

    DEBUG_LOG_INFO("ChargerDetect: current %d", charger_config->current);

    data->charger_config = charger_config;

    chargerDetect_Resolved(msg->attached_status != DETACHED);
}

void ChargerDetect_NotifyCurrentChanged(void)
{
    chargerTaskData *data = appGetCharger();

    /* Check if limit detection needs to be resumed */

    if (!data->vchg_monitor_enabled &&
            data->charger_config &&
            data->charger_config->current_limiting &&
            !data->current_limit_detected &&
            data->current_limit == Charger_GetFastCurrent())
    {
        DEBUG_LOG_INFO("ChargerDetect: resume current limit probing");
        data->current_increasing = 1;
        Charger_VChgMonitorStart(data);
    }
}

void ChargerDetect_VchgReading(uint16 voltage_mv, uint16 current_mA)
{
    chargerTaskData *data = appGetCharger();
    uint16 requested_current = Charger_GetFastCurrent();

    /* stop if current is less than 90% of requested current - this usually
     * means battery is nearly full and charger is in the constant voltage mode. */
    if (current_mA < (uint32)requested_current * 9 / 10)
    {
        DEBUG_LOG_INFO("ChargerDetect: limited by charger HW");
        data->current_limit = MAX(current_mA, appConfigChargerDCPMinCurrent());
        data->current_limit_detected = 1;
        Charger_VChgMonitorStop(data);
        Charger_UpdateCurrent();
        return;
    }

    if (data->current_increasing)
    {
        if (voltage_mv <= appConfigChargerVchgLowThreshold())
        {   /* voltage is too low - need to decrease the current */
            data->current_increasing = 0;
            /* continue below */
        }
        else
        {   /* vchg level is good */
            if (data->current_limit > requested_current)
            {   /* Current is limited by Charger Monitor - stop for now.
                   Detection will resume when Charger Monitor increases
                   current limit. */
                DEBUG_LOG_INFO("ChargerDetect: limited by charger monitor");
                Charger_VChgMonitorStop(data);
            }
            else
            { /* increase current */
                uint16 new_current_limit = data->current_limit + appConfigChargerVchgStep();

                if (new_current_limit > data->charger_config->current)
                {   /* Current is limited by detected charger config - stop. */
                    DEBUG_LOG_INFO("ChargerDetect: limited by charger config");
                    data->current_limit_detected = 1;
                    Charger_VChgMonitorStop(data);
                }
                else
                {   /* increase it a bit */
                    data->current_limit = new_current_limit;
                    /* notify Charger Monitor */
                    Charger_UpdateCurrent();
                }
            }
        }
    }

    if (!data->current_increasing)
    { /* decreasing */
        if (voltage_mv <= appConfigChargerVchgRecoveryThreshold())
        {   /* voltage is still too low, decrease current further */
            if (data->current_limit == appConfigChargerDCPMinCurrent())
            {   /* don't decrease below IDCP_min */
                data->current_limit_detected = 1;
                Charger_VChgMonitorStop(data);
            }
            else
            {   /* decrease it a bit */
                data->current_limit -= appConfigChargerVchgStep();
                /* notify Charger Monitor */
                Charger_UpdateCurrent();
            }
        }
        else
        {   /* OK voltage is good again, stop now. */
            DEBUG_LOG_INFO("ChargerDetect: charger limit detected");
            data->current_limit_detected = 1;
            Charger_VChgMonitorStop(data);
        }
    }
}

void ChargerDetect_Changed(MessageChargerChanged *msg)
{
    chargerTaskData *data = appGetCharger();

    DEBUG_LOG("ChargerDetect: MSG connected %d", msg->charger_connected);

    const charger_config_t *charger_config = ChargerDetect_GetConnectedConfig(msg->charger_connected);

    if (!charger_config ||
            data->charger_config == charger_config)
    {
        return;
    }

    DEBUG_LOG_INFO("ChargerDetect: connected %d", msg->charger_connected);

    data->charger_config = charger_config;

    chargerDetect_Resolved(msg->charger_connected);
}

void ChargerDetect_UpdateUsbStatus(MessageId id)
{
    chargerTaskData *data = appGetCharger();
    unsigned old_usb_suspend = data->usb_suspend;

    switch (id)
    {
        case USB_DEVICE_ENUMERATED:
            data->usb_enumerated = 1;
            break;
        case USB_DEVICE_DECONFIGURED:
            data->usb_enumerated = 0;
            data->usb_suspend = 0;
            break;
        case USB_DEVICE_SUSPEND:
            data->usb_suspend = 1;
            break;
        case USB_DEVICE_RESUME:
            data->usb_suspend = 0;
            break;
        default:
            /* silently ignore other messages */
            return;
    }
    DEBUG_LOG_INFO("Charger: USB enumerated: %d suspend: %d",
            data->usb_enumerated, data->usb_suspend);

    Charger_UpdateCurrent();

    if (data->charger_config &&
            data->charger_config->usb_events_apply &&
            old_usb_suspend != data->usb_suspend)
    {
        Charger_HandleChange();
    }
}

bool ChargerDetect_UsbIsSuspend(void)
{
    chargerTaskData *data = appGetCharger();

    if (data->charger_config &&
            data->charger_config->usb_events_apply &&
            data->usb_suspend)
    {
        return TRUE;
    }
    return FALSE;
}

void ChargerDetect_Init(Task task)
{
    /* Register for USB device events */
    UsbDevice_ClientRegister(task);

    MessageChargerChanged msg_changed;
    msg_changed.charger_connected = Charger_Status() != NO_POWER;
    msg_changed.vreg_en_high = 0;

    /* update connection state early */
    ChargerDetect_Changed(&msg_changed);

    /* re-request message with charger detection result */
    ChargerMessageRequest();
}

charger_detect_type ChargerDetect_GetChargerType(void)
{
    chargerTaskData *data = appGetCharger();

    return (data->charger_config) ?
            data->charger_config->charger_type :
            CHARGER_TYPE_NOT_RESOLVED;
}


#endif /* INCLUDE_CHARGER_DETECT */

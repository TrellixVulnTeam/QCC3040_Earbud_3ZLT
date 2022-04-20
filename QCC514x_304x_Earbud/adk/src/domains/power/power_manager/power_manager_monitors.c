/*!
\copyright  Copyright (c) 2008 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Monitoring of charger status, battery voltage and temperature.

*/

#include "power_manager_monitors.h"
#include "power_manager.h"
#include "power_manager_private.h"
#include "power_manager_sm.h"

#include "temperature.h"
#include "battery_region.h"
#include "battery_monitor_config.h"
#include "charger_monitor.h"

#include "ui.h"

#include <logging.h>

void appPowerRegisterMonitors(void)
{
    (void)Charger_ClientRegister(PowerGetTask());

    (void)BatteryRegion_Register(PowerGetTask());

    /* Need to power off when temperature is outside battery's operating range */
    if (!appTemperatureClientRegister(PowerGetTask(),
                                      appConfigBatteryDischargingTemperatureMin(),
                                      appConfigBatteryDischargingTemperatureMax()))
    {
        DEBUG_LOG_WARN("appPowerInit no temperature support");
    }
}

void appPowerHandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);

    if (isMessageUiInput(id))
        return;

#ifdef DEBUG_POWER_MANAGER_MESSAGES
    switch(id)
    {
        case CHARGER_MESSAGE_ATTACHED:
            DEBUG_LOG_V_VERBOSE("appPowerHandleMessage CHARGER_MESSAGE_ATTACHED");
            return;
        case CHARGER_MESSAGE_DETACHED:
            DEBUG_LOG_V_VERBOSE("appPowerHandleMessage CHARGER_MESSAGE_DETACHED");
            return;
        case MESSAGE_BATTERY_REGION_UPDATE:
            {
                MESSAGE_BATTERY_REGION_UPDATE_STATE_T *msg = (MESSAGE_BATTERY_REGION_UPDATE_STATE_T *)message;
                if(msg)
                {
                    DEBUG_LOG_V_VERBOSE("appPowerHandleMessage MESSAGE_BATTERY_REGION_UPDATE_STATE_T state 0x%x", msg->state);
                }
            }
            return;
        case TEMPERATURE_STATE_CHANGED_IND:
            {
                TEMPERATURE_STATE_CHANGED_IND_T *msg = (TEMPERATURE_STATE_CHANGED_IND_T *)message;
                if(msg)
                {
                    DEBUG_LOG_V_VERBOSE("appPowerHandleMessage TEMPERATURE_STATE_CHANGED_IND state 0x%x", msg->state);
                }
            }
            return;
        default:
            break;
    }
#endif

    switch (id)
    {
        case CHARGER_MESSAGE_ATTACHED:
        case CHARGER_MESSAGE_DETACHED:
        case CHARGER_MESSAGE_COMPLETED:
        case MESSAGE_BATTERY_REGION_UPDATE:
        case TEMPERATURE_STATE_CHANGED_IND:
            appPowerHandlePowerEvent();
            return;
        default:
            break;
    }

    switch (id)
    {
        case POWER_MANAGER_INTERNAL_MESSAGE_PERFORMANCE_RELINIQUISH:
            appPowerPerformanceProfileRelinquish();
            return;

        default:
            break;
    }
}

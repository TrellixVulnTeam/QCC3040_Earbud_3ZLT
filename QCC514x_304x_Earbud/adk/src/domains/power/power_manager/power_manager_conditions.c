/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Functions used to decide if system should go to sleep or power off.

*/

#include "power_manager_conditions.h"
#include "power_manager.h"

#include "charger_monitor.h"
#include "battery_region.h"
#include "temperature.h"

bool appPowerCanPowerOff(void)
{
    return Charger_CanPowerOff();
}

bool appPowerCanSleep(void)
{
    return Charger_CanEnterDormant()
           && PowerGetTaskData()->allow_dormant
           && PowerGetTaskData()->init_complete;
}

bool appPowerNeedsToPowerOff(void)
{
    bool battery_not_safe = (battery_region_unsafe == BatteryRegion_GetState());
    bool user_initiated = PowerGetTaskData()->user_initiated_shutdown;
    bool temperature_extreme = (appTemperatureClientGetState(PowerGetTask()) !=
                                TEMPERATURE_STATE_WITHIN_LIMITS);
    return appPowerCanPowerOff() && (temperature_extreme || user_initiated || battery_not_safe);
}

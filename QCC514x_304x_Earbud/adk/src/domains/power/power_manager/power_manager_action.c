/*!
\copyright  Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief       Implementation of 'go to sleep' and power off
*/

#include "power_manager_action.h"
#include "power_manager.h"

#include "phy_state.h"
#include "acceleration_config.h"
#include "touch.h"
#include "charger_monitor.h"
#include "pio_common.h"
#ifdef INCLUDE_CASE_COMMS
#include "cc_protocol.h"
#endif

#include <dormant.h>
#include <psu.h>
#include <panic.h>
#include <logging.h>

static void appPowerRegisterPeripheralsAsWakeupSource(void)
{
#define MAX_SENSORS (2)
int sensor_count = 0;
dormant_config_key key[MAX_SENSORS];
uint32 value[MAX_SENSORS];

    /*
     * When providing DormantConfigure information into this module, clients that
     * need inverted logic should provide PIO_WAKE_INVERT_MASK as the key.
     *
     * DormantConfigure needs both PIO_WAKE_INVERT_MASK and PIO_WAKE_MASK, the
     * code in this module handles configuring PIO_WAKE_MASK.
     */

#ifdef INCLUDE_CAPSENSE
    if (AppTouchSensorGetDormantConfigureKeyValue(&key[sensor_count], &value[sensor_count]))
    {
        sensor_count++;
    }
#endif

#ifdef INCLUDE_ACCELEROMETER
    powerTaskData *thePower = PowerGetTaskData();
    /* Register to ensure accelerometer is active */
    appAccelerometerClientRegister(&thePower->task);
    if (appAccelerometerGetDormantConfigureKeyValue(&key[sensor_count], &value[sensor_count]))
    {
        sensor_count++;
    } else
    {
        appAccelerometerClientUnregister(&thePower->task);
    }
#endif

    if (sensor_count == 0)
        return;
    /* For now we have only 2 sources; this algorithm would need improving
     * to be more generic if ever there are additional wake up sources in
     * future. */
    if (sensor_count == 1)
    {
        /* If we have 1 sensor then just configure dormant */
        PanicFalse(DormantConfigure(key[0], value[0]));
        if (key[0] == PIO_WAKE_INVERT_MASK)
        {
            PanicFalse(DormantConfigure(PIO_WAKE_MASK, value[0]));          
        }
    } else
    {
        /* We want to identify how many unique keys there are, since
         * DormantConfigure() will overwrite a key/value pair if called
         * with a previous key.
         * As we have a maximum of 2 wake sources, we do not need an
         * iterative algorithm to identify unique keys. */
        uint32 val;
        if (key[0] == key[1])
        {
            val = value[0] | value[1];
            PanicFalse(DormantConfigure(key[0], val));
            if (key[0] == PIO_WAKE_INVERT_MASK)
            {
                PanicFalse(DormantConfigure(PIO_WAKE_MASK, value[0]));
            }
        } else
        {
            PanicFalse(DormantConfigure(key[0], value[0]));
            if (key[0] == PIO_WAKE_INVERT_MASK)
            {
                PanicFalse(DormantConfigure(PIO_WAKE_MASK, value[0]));
            }

            PanicFalse(DormantConfigure(key[1], value[1]));
            if (key[1] == PIO_WAKE_INVERT_MASK)
            {
                PanicFalse(DormantConfigure(PIO_WAKE_MASK, value[1]));
            }
        }
    }
}

void appPowerEnterDormantMode(bool extended_wakeup_events)
{
    DEBUG_LOG_ALWAYS("appPowerEnterDormantMode");

    if(PowerGetTaskData()->panic_on_sleep)
    {
        DEBUG_LOG_ERROR("appPowerEnterDormantMode panic_on_sleep enabled IT WON'T SLEEP!!!");
        PowerGetTaskData()->went_to_sleep = TRUE;
        Panic();
    }

    if (extended_wakeup_events)
    {
        appPowerRegisterPeripheralsAsWakeupSource();
    }

#ifdef INCLUDE_CASE_COMMS
    CcProtocol_ConfigureAsWakeupSource();
#endif    
    appPhyStatePrepareToEnterDormant();
    
    /* An active charge module blocks dormant, regardless of whether
       it has power */
    Charger_ForceDisable();

    /* Make sure dormant will ignore any wake up time */
    PanicFalse(DormantConfigure(DEADLINE_VALID,FALSE));

    /* Enter dormant */
    PanicFalse(DormantConfigure(DORMANT_ENABLE,TRUE));

    DEBUG_LOG_ERROR("appPowerEnterDormantMode FAILED");

    /* If we happen to get here then Dormant didn't work,
     * so make sure the charger is running again (if needed)
     * so we could continue. */
    Charger_RestoreState();

    Panic();
}

bool appPowerDoPowerOff(void)
{
    DEBUG_LOG_ALWAYS("appPowerDoPowerOff");

    if(PowerGetTaskData()->panic_on_sleep)
    {
        DEBUG_LOG_ERROR("appPowerDoPowerOff panic_on_sleep enabled IT WON'T POWER OFF!!!");
        PowerGetTaskData()->powered_off = TRUE;
        Panic();
    }

    /* Try to power off.*/
    PsuConfigure(PSU_ALL, PSU_ENABLE, FALSE);

    /* Failed to power off.. */
    DEBUG_LOG_ERROR("appPowerDoPowerOff Turning off power supplies was ineffective?");

    /* No need to disable charger for power down, but if a charger is connected
       we want to charge. Check if we failed to power off because charger
       detection is not complete */
    if (!Charger_AttachedStatusPending())
    {
        /* Fall back to Dormant */
        appPowerEnterDormantMode(FALSE);

        /* Should never get here...*/
        Panic();
    }

    DEBUG_LOG_ERROR("appPowerDoPowerOff Failed to power off because charger detection ongoing. Check charger again later");

    return FALSE;
}


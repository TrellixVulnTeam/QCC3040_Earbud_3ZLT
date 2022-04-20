/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    system_reboot
\brief      Sophisticated version of the reboot.

*/

#include "system_reboot.h"

#include <system_clock.h>
#include <logging.h>
#include <stdlib.h>
#include <boot.h>
#include <panic.h>
#include <ps.h>
#include <ps_key_map.h>

#define APP_POWER_SEC_TO_US(s)    ((rtime_t) ((s) * (rtime_t) US_PER_SEC))

/*!@{ \name Details of the Persistent Storage offset of reboot actions */
#define REBOOT_ACTION_STORE_OFFSET   0 /*!< Offset within that key to the context */
#define REBOOT_PSKEY_STORAGE_LENGTH  1 /*!< Maximum PSKEY length in uint16 */

static reboot_action_t SystemReboot_RetrieveAction(void)
{
    reboot_action_t reboot_action = reboot_action_default_state;

    uint16 num_of_words = PsRetrieve(PS_KEY_REBOOT_ACTION, NULL, 0);
    if(num_of_words > 0)
    {
        uint16 *key_cache = PanicUnlessMalloc( num_of_words * sizeof(uint16) );

         uint16 read_words = PsRetrieve(PS_KEY_REBOOT_ACTION, key_cache, num_of_words);

        if(read_words > 0)
        {
            reboot_action = (reboot_action_t)key_cache[REBOOT_ACTION_STORE_OFFSET];
        }

        free(key_cache);
    }

    return reboot_action;
}

static void SystemReboot_SetAction(reboot_action_t reboot_action)
{
        uint16 key_cache[REBOOT_PSKEY_STORAGE_LENGTH];
        key_cache[REBOOT_ACTION_STORE_OFFSET] = (uint16) reboot_action;
        PsStore(PS_KEY_REBOOT_ACTION, key_cache, REBOOT_PSKEY_STORAGE_LENGTH);
}

reboot_action_t SystemReboot_GetAction(void)
{
    reboot_action_t reboot_action;
    
    reboot_action = SystemReboot_RetrieveAction();
    
    return reboot_action;
}

void SystemReboot_ResetAction(void)
{
    SystemReboot_SetAction(reboot_action_default_state);
}

void SystemReboot_RebootWithAction(reboot_action_t reboot_action)
{
    /* Store the reboot action in persistant storage*/
    SystemReboot_SetAction(reboot_action);

    /* Reboot now */
    BootSetMode(BootGetMode());
    DEBUG_LOG("SystemReboot, post reboot");

    /* BootSetMode returns control on some devices, although should reboot.
       Wait here for 1 seconds and then Panic() to force the reboot. */
    rtime_t start = SystemClockGetTimerTime();

    while (1)
    {
        rtime_t now = SystemClockGetTimerTime();
        if( rtime_gt(rtime_sub(now, start), APP_POWER_SEC_TO_US(1)) )
        {
            DEBUG_LOG("SystemReboot, forcing reboot by panicking");
            Panic();
        }
    }
}

void SystemReboot_Reboot(void)
{
    SystemReboot_RebootWithAction(reboot_action_default_state);
}


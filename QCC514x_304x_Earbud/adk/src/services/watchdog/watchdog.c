/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       watchdog.c
\brief      Software Watchdog Timers
*/

#include <vm.h>
#include <app/vm/vm_if.h>
#include <message.h>
#include <panic.h>
#include <util.h>
#include <logging.h>
#include "watchdog.h"

#ifdef INCLUDE_WATCHDOG

/* Implementation Notes

Each watchdog timer is stored in 1 byte in a reserved section of RAM.  This byte stores the expiriation time of the
timer in 512ms units.  0 is used to indicate timer is not active.

The actual watchdog functionality is implemented in P0 and acceess via VmSoftwareWdKick() trap.  Whenever
any watchdog timer is started or stopped, all the expiration times of the active watchdog timers are compared
against the current time returned by VmGetClock().  The time of the timer which will expire first is then
passed to P0 via the VmSoftwareWdKick() trap.

If no timers are active then the special sequence of 3 calls to VmSoftwareWdKick() are used to stop the P0 watchdog timer.

The reserved section of RAM is checksummed using UtilHash and the checksum is stored seperately.  The checksum is checked
on entry to Watchdog_Kick() and Watchdog_Stop() to detect memory corruption of the watchdog timers state.
*/

/*! Special value used to determine if no watchdog active */
#define NO_KICK (255)

/*! Hash of watchdog states, used to detect memory corruption that could disable a watchdog */
static uint16 watchdog_Hash;


/*! \brief Update hash
 *
    This function updates the stored hash with the hash calculated over the
    watchdog timers state.
*/
static uint8 watchdog_CalcHash(void)
{
    const uint16 size_bytes = (uint32)watchdog_states_end - (uint32)watchdog_states_begin;
    return UtilHash(watchdog_states_begin, size_bytes, 0xC0DE);
}


/*! \brief Check hash matches
 *
    This function checks that the stored hash matches the hash calculated over the
    watchdog timers state.  If there's a mismatch then the chip will panic.
*/
static void watchdog_Check(void)
{
    PanicFalse(watchdog_CalcHash() == watchdog_Hash);
}

/*! \brief Update P0 software watchdog
 *
    This function is called whenever any watchdimg timer is started or stopped.
    It iterates through the array of timers looking for the active timer that
    will expire next.
*/

static void watchdog_Update(void)
{
    uint8 kick_s = NO_KICK;
    uint8 clock = (uint8)(VmGetClock() / 512);

    /* Walk through list of 'virtual' watchdogs finding closest timeout */
    watchdog_state_t *state;
    for (state = watchdog_states_begin; state < watchdog_states_end; state++)
    {
        if (*state)
        {
            /* Calculate time in 512ms units until watchdog expires */
            int8 delta = (int8)(*state) - (int8)clock;

            /* Round up to seconds, handle case where watchdog expiry time is now or in the past */
            uint8 delta_s = (delta > 0) ? (delta * 512UL + 999) / 1000 : 1;
            DEBUG_LOG_VERBOSE("watchdog_Update, watchdog %p, delta %d ms / %d s", state, delta * 512, delta_s);

            /* Update kick time if this watchdog expires sooner */
            if (delta_s < kick_s)
                kick_s = delta_s;
        }
    }

    /* Call software watch trap to kick watchdog if virtual watchdog active */
    if (kick_s != NO_KICK)
    {
        DEBUG_LOG_VERBOSE("watchdog_Update, kick watchdog within %u seconds", kick_s);
        VmSoftwareWdKick(kick_s);
    }
    else
    {
        DEBUG_LOG_VERBOSE("watchdog_Update, stop watchdog");

        /* No watchdogs enabled, so disable it */
        VmSoftwareWdKick(VM_SW_WATCHDOG_DISABLE_CODE1);
        VmSoftwareWdKick(VM_SW_WATCHDOG_DISABLE_CODE2);
        VmSoftwareWdKick(VM_SW_WATCHDOG_DISABLE_CODE3);
    }

    /* Hash everything to detect corruption */
    watchdog_Hash = watchdog_CalcHash();
}


/*! \brief Initialise the software watchdog component */
void Watchdog_Init(void)
{
    /* Check hash is 0 as it should be straight from reaet */
    PanicFalse(watchdog_Hash == 0);

    /* Calculate initial hash */
    watchdog_Hash = watchdog_CalcHash();
}


/*! \brief (Re)start a software watchdog timer */
void Watchdog_Kick(watchdog_state_t *state, uint8 time_s)
{
    /* Check for memory corruption */
    watchdog_Check();

    /* Check time is less or equal to 64 seconds, anything higher doesn't fit into 8 bits */
    PanicFalse(time_s <= 64);

    /* Check state is within bounds */
    PanicFalse(state >= watchdog_states_begin && state < watchdog_states_end);

    /* Calculate expiry time in 512ms units (avoid 0 as that represents inactive watchdog) */
    *state = (VmGetClock() + (time_s * 1000UL) + 511) / 512;
    if (*state == 0)
        *state = 1;

    /* Update P0 software watchdog */
    watchdog_Update();
}


/*! \brief Stop a software watchdog timer */
void Watchdog_Stop(watchdog_state_t *state)
{
    /* Check for memory corruption */
    watchdog_Check();

    /* Check state is within bounds */
    PanicFalse(state >= watchdog_states_begin && state < watchdog_states_end);

    /* Check watchdog isn't already stopped */
    if (*state)
    {
        /* Mark watchdog as disabled */
        *state = 0;

        /* Update P0 software watchdog */
        watchdog_Update();
    }
    else
        DEBUG_LOG_WARN("Watchdog_Stop, stopping watchdog %p when it's already stoppped", state);
}

#endif /* INCLUDE_WATCHDOG */

/* Copyright (c) 2016 Qualcomm Technologies International, Ltd. */
/*   %%version */
/**
 * \file ipc_deep_sleep.c
 *
 */

#include "ipc/ipc_private.h"
#include "dorm/dorm.h"

#ifdef DESKTOP_TEST_BUILD
#include "subsleep/subsleep.h"

void ipc_deep_sleep_msg_handler(IPC_SIGNAL_ID id, const void *msg)
{
    switch(id)
    {
    case IPC_SIGNAL_ID_P1_DEEP_SLEEP_MSG:
    {
        /* Set dorm request for P1/P0 */
        const IPC_P1_DEEP_SLEEP_MSG_PRIM *prim =
                                         (const IPC_P1_DEEP_SLEEP_MSG_PRIM *)msg;

        dorm_set_sleep_info_for_p1 (prim->p1_sleep,
                                    (TIME)prim->earliest_wake_up_time,
                                    (TIME)prim->latest_wake_up_time);
        /* No need to confirm the message */
        break;
    }
    case IPC_SIGNAL_ID_DEEP_SLEEP_WAKEUP_SOURCE:
    {
        const IPC_DEEP_SLEEP_WAKEUP_SOURCE *prim =
                (const IPC_DEEP_SLEEP_WAKEUP_SOURCE *)msg;
        subsleep_configure_wake_mask(prim->wake_source, prim->mask);
        break;
    }

    default:
        panic_diatribe(PANIC_IPC_UNHANDLED_MESSAGE_ID, id);
        /* no break */
    }
}

#else /* defined(PROCESSOR_P0) || defined(DESKTOP_TEST_BUILD) */

void ipc_send_p1_deep_sleep_msg(bool p1_sleep, uint32 earliest_wake_up_time, uint32 latest_wake_up_time)
{
    IPC_P1_DEEP_SLEEP_MSG_PRIM prim;

    prim.p1_sleep              = p1_sleep;
    prim.earliest_wake_up_time = earliest_wake_up_time;
    prim.latest_wake_up_time   = latest_wake_up_time;

    ipc_send(IPC_SIGNAL_ID_P1_DEEP_SLEEP_MSG, (const void *)&prim, sizeof(prim));
}

#endif /* defined(PROCESSOR_P0) || defined(DESKTOP_TEST_BUILD) */

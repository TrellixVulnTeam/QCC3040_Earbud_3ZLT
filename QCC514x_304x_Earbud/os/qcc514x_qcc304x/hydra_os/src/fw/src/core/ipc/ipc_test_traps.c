/* Copyright (c) 2017 Qualcomm Technologies International, Ltd. */
/*   %%version */
/**
 * \file
 */

#if defined(DESKTOP_TEST_BUILD) && TRAPSET_TEST
#include "ipc/ipc_private.h"
#include "testtraps/testtraps.h"
#include "transport_bt/transport_bt.h"

void ipc_test_trap_handler(IPC_SIGNAL_ID id, const void *msg)
{
    switch(id)
    {
        case IPC_SIGNAL_ID_TESTTRAP_BT_REQ:
        {
            const IPC_TESTTRAP_BT_REQ *ipc_prim =
                                            (const IPC_TESTTRAP_BT_REQ *)msg;

            switch(transport_bt_run_state())
            {
                case TRANSPORT_OFF:
                    /* Save BCCMD first and start BT service */
                    if(testtraps_save_bccmd(ipc_prim))
                    {
                        testtraps_start_bt_service();
                    }
                    else
                    {
                        ipc_send_bool(IPC_SIGNAL_ID_TESTTRAP_BT_RSP, FALSE);
                    }
                    break;

                case TRANSPORT_STARTING:
                /* We are in the middle of BT service start process that
                 * has been initiated by other subystem. In this case,
                 * testtraps will not receive BT start completion notification.
                 * Let's return failure response.
                 */
                case TRANSPORT_START_FAILED:
                case TRANSPORT_STOPPING:
                    ipc_send_bool(IPC_SIGNAL_ID_TESTTRAP_BT_RSP, FALSE);
                    break;

                case TRANSPORT_RUNNING:
                    testtraps_send_bccmd(ipc_prim);
                    break;

                default:
                    L2_DBG_MSG("ipc_test_trap_handler: unknown transport_bt "
                                "state");
                    assert(FALSE);
                    break;
            }
        }
        break;

        default:
            L2_DBG_MSG1("ipc_test_trap_handler: unexpected signal ID 0x%x"
                        " received", id);
            assert(FALSE);
            break;
    }
}

#endif /* (defined(PROCESSOR_P0) || defined(DESKTOP_TEST_BUILD)) && TRAPSET_TEST */

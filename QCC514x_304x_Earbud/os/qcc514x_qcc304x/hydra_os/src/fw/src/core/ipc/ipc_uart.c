/* Copyright (c) 2016 Qualcomm Technologies International, Ltd. */
/*   %%version */
/**
 * \file
 */

#include "ipc/ipc_private.h"

#if defined(DESKTOP_TEST_BUILD) && TRAPSET_UART
void ipc_uart_handler(IPC_SIGNAL_ID id)
{
    switch(id)
    {
    case IPC_SIGNAL_ID_STREAM_UART_SINK:
    {
        /* Ignore the returned Sink ID, it will be populated later.
         * See the definition of vm_trap_StreamUartSink() for details.
         */
        (void)vm_trap_StreamUartSink();
    }
        break;

    default:
        L1_DBG_MSG1("ipc_uart_handler: unexpected signal ID 0x%x received", id);
        assert(FALSE);
        break;
    }
}
#endif /* (defined(PROCESSOR_P0) || defined(DESKTOP_TEST_BUILD)) && TRAPSET_UART */

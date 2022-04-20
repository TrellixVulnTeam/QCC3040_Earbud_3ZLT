/* Copyright (c) 2020 Qualcomm Technologies International, Ltd. */
/*   %%version */

/**
 * @file
 *
 * Functions for IPC receive that are common between the multitasking and
 * non-multitasking IPC protocols.
 */

#include "ipc/ipc_private.h"
#include "ipc/ipc_signal.h"


void ipc_recv_message_free(uint16 msg_length)
{
    ipc_buffer_update_back(ipc_data.recv, msg_length);

    if( ipc_data.leave_pages_mapped )
    {
        ipc_buffer_update_tail_no_free(ipc_data.recv);
    }
    else
    {
        ipc_buffer_update_tail_free(ipc_data.recv);
    }
}





#ifdef OS_OXYGOS
/**
 * @brief Process a static callback message.
 *
 * First handles static callback messages that are generic between P0 and P1, if
 * it's not a generic message it's handed to
 * ipc_recv_process_cpu_static_callback_message for CPU specific handling.
 *
 * @param msg         Pointer to the message.
 * @param msg_length  The length of the message in bytes.
 *
 * @return TRUE if the message was recognised and handled, FALSE otherwise.
 */
static bool ipc_recv_process_static_callback_message(const IPC_HEADER *msg,
                                                     uint16 msg_length)
{
    IPC_SIGNAL_ID id;

    id = msg->id;
    switch(id)
    {
        /* The cases here are for static callback messages that are handled
           similarly on both processors. */
    case IPC_SIGNAL_ID_TEST_TUNNEL_PRIM:
        ipc_test_tunnel_handler(id, msg, msg_length);
        break;
    case IPC_SIGNAL_ID_SCHED_MSG_PRIM:
        ipc_sched_handler(id, msg);
        break;
    case IPC_SIGNAL_ID_PFREE:
        ipc_malloc_msg_handler(id, msg);
        break;
    case IPC_SIGNAL_ID_SIGNAL_INTERPROC_EVENT:
        hal_set_reg_interproc_event_1(1);
        break;
    case IPC_SIGNAL_ID_TRAP_API_VERSION:
        ipc_trap_api_version_prim_handler(id, msg);
        break;
    default:
        /* Defer to the processor specific handler. */
        return ipc_recv_process_cpu_static_callback_message(msg, msg_length);
    }

    return TRUE;
}

/**
 * @brief Process an auto-generated message.
 *
 * First handles static callback messages that are generic between P0 and P1, if
 * it's not a generic message it's handed to
 * ipc_recv_process_cpu_autogen_message for CPU specific handling.
 *
 * @param msg         Pointer to the message.
 * @param msg_length  The length of the message in bytes.
 * @return TRUE if the message was recognized and handled, FALSE otherwise.
 */
static bool ipc_recv_process_autogen_message(const IPC_HEADER *msg,
                                             uint16 msg_length)
{
    if(ipc_signal_is_autogen(msg->id))
    {
        ipc_recv_process_cpu_autogen_message(msg, msg_length);
        return TRUE;
    }

    return FALSE;
}

void ipc_recv_process_async_message(const IPC_HEADER *msg, uint16 msg_length)
{
    if(ipc_recv_process_static_callback_message(msg, msg_length))
    {
        return;
    }

    if(ipc_recv_process_autogen_message(msg, msg_length))
    {
        return;
    }

    panic_diatribe(PANIC_IPC_UNHANDLED_MESSAGE_ID, msg->id);
}
#endif /* OS_OXYGOS */

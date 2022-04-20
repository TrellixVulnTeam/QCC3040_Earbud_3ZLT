/* Copyright (c) 2016 - 2020 Qualcomm Technologies International, Ltd. */
/*   %%version */

#include "ipc/ipc_private.h"

/**
 * @brief Determines whether ipc_recv() should try to sleep.
 * @return TRUE  If there's no pending work and it's safe to sleep.
 * @return FALSE If ipc_recv() shouldn't sleep.
 */
static bool ipc_recv_should_sleep(void)
{
    bool should_sleep = FALSE;

    if (!ipc_data.pending && !background_work_pending)
    {
        /* The send queue must be clear before sleeping or we may block waiting
           for a response to a message that hasn't been sent yet. */
        block_interrupts();
        should_sleep = ipc_clear_queue();
        unblock_interrupts();
    }

    return should_sleep;
}

/**
 * @brief Put the processor into shallow sleep if possible.
 */
static void ipc_recv_try_to_sleep(void)
{
    while (ipc_recv_should_sleep())
    {
        /* Purely nominal timeout - it is ignored by dorm_shallow_sleep. */
        TIME timeout = time_add(hal_get_time(), SECOND);
        dorm_shallow_sleep(timeout);
    }
}

/**
 * Process everything in the receive buffer.  If the current blocking_msg_id is
 * encountered amongst them, save a copy of the payload in a local pmalloc block
 * and return it to the caller after everything has been processed.  Ownership
 * of the memory is passed to the caller.
 *
 * @param blocking_id The signal ID this function should wait for.  Set this to
 * IPC_SIGNAL_ID_NULL if not waiting for a blocking response.
 * @param blocking_msg Pointer to pre-allocated space for any expected message,
 * or NULL if no particular message is expected.  If this function
 * isn't being called in the context of blocking, the value is irrelevant,
 * except that it will be returned unchanged, so it is highly recommended to
 * pass NULL, so that the case where an blocking message is erroneously received
 * can be detected (e.g. see \c ipc_background_handler())
 * @return TRUE if the blocking message was received, FALSE otherwise.
 *
 * \ingroup ipc_recv_impl
 */
static bool ipc_recv_handler(IPC_SIGNAL_ID blocking_id, void *blocking_msg)
{
    uint16f n_processed = 0;
    bool blocking_message_seen = FALSE;

    block_interrupts();
    {
        ipc_data.pending = FALSE;
    }
    unblock_interrupts();

    /* We consume everything there is because IPC is a relatively high-priority
     * task */
    while(ipc_buffer_any_messages(ipc_data.recv) &&
          n_processed < IPC_MAX_RECV_MSGS)
    {
        const IPC_HEADER *msg = ipc_buffer_map_read(ipc_data.recv);
        const uint16 msg_length = ipc_buffer_map_read_length(ipc_data.recv);

        if(msg->id == blocking_id)
        {
            /* Received the response for the expected blocking message. */
            assert(blocking_msg);
            memcpy(blocking_msg, msg, msg_length);
            blocking_message_seen = TRUE;
        }
        else
        {
            ipc_recv_process_async_message(msg, msg_length);
        }

        /* Free ipc message as it has already been processed in
         * message handler called above */
        ipc_recv_message_free(msg_length);
        n_processed++;
    }

    /* Reschedule ourselves if there's anything left to process */
    if (ipc_buffer_any_messages(ipc_data.recv))
    {
        ipc_data.pending = TRUE;
        GEN_BG_INT(ipc_bg);
    }

    return blocking_message_seen;
}

void ipc_recv(IPC_SIGNAL_ID recv_id, void *blocking_msg)
{
    bool changed_background_work_pending = FALSE;

    /* Memory must be provided for the response. */
    assert(blocking_msg);

    /* Sleep until we see the IPC interrupt fire, and then process the
     * entries.  Keep doing this until we see the recv_id message */
    /*lint -e(716) Loop will terminate when the recv_id message is found */
    while (TRUE)
    {
        ipc_recv_try_to_sleep();

        if (ipc_recv_handler(recv_id, blocking_msg))
        {
            /* Restore indicator of pending background work.
             * Note: this is safe because code running from interrupt handlers
             * only increases TotalNumMessages, so once background_work_pending is set,
             * it doesn't get cleared until the scheduler has a chance to run
             * background work. */
            if (changed_background_work_pending)
            {
                background_work_pending = TRUE;
            }
            break;
        }

        if (background_work_pending)
        {
            /* We can't service background work anyway until
             * an expected IPC response is received, so no need to prevent
             * processor from shallow sleeping. */
            background_work_pending = FALSE;
            /* Remember that we tampered with it */
            changed_background_work_pending = TRUE;
        }
    }
}

void ipc_background_handler(void)
{
    /*
     * Attempt to send queued messages
     */
    block_interrupts();
    if (!ipc_clear_queue())
    {
        /* Couldn't post them all, so reschedule self */
        GEN_BG_INT(ipc_bg);
    }
    unblock_interrupts();

    /*
     * Process messages in the IPC recv buffer without reference to any blocking
     * msg ID.  The background handler is only called when there *isn't* a
     * blocking call waiting - if there is, control returns from the interrupt
     * direct to ipc_recv() instead
     */
    if (ipc_recv_handler(IPC_SIGNAL_ID_NULL, NULL))
    {
        /* P0 sent IPC_SIGNAL_ID_NULL to P1, this shouldn't happen. */
        panic(PANIC_IPC_UNEXPECTED_BLOCKING_MSG);
    }
}

void ipc_interrupt_handler(void)
{
    ipc_data.pending = TRUE;
    GEN_BG_INT(ipc_bg);
}

bool ipc_recv_process_cpu_static_callback_message(const IPC_HEADER *msg,
                                                  uint16 msg_length)
{
    IPC_SIGNAL_ID id = msg->id;
    switch(id)
    {
    case IPC_SIGNAL_ID_BLUESTACK_PRIM:
        ipc_bluestack_handler(id, msg);
        break;
    case IPC_SIGNAL_ID_APP_MSG:
    case IPC_SIGNAL_ID_APP_SINK_SOURCE_MSG:
    case IPC_SIGNAL_ID_APP_MSG_TO_HANDLER:
        ipc_trap_api_handler(id, msg, msg_length);
        break;
    case IPC_SIGNAL_ID_IPC_LEAVE_RECV_BUFFER_PAGES_MAPPED:
        ipc_data.leave_pages_mapped = TRUE;
        break;
    case IPC_SIGNAL_ID_STREAM_DESTROYED:
    case IPC_SIGNAL_ID_OPERATORS_DESTROYED:
        ipc_stream_handler(id, msg);
        break;
    case IPC_SIGNAL_ID_MEMORY_ACCESS_FAULT_INFO:
        ipc_memory_access_fault_handler(id, msg);
        break;
    default:
        /* This is not a P1 specific static callback message. */
        return FALSE;
    }

    /* Message has been handled. */
    return TRUE;
}

void ipc_recv_process_cpu_autogen_message(const IPC_HEADER *msg,
                                          uint16 msg_length)
{
    ipc_trap_api_handler(msg->id, msg, msg_length);
}

void ipc_recv_messages_sent_before_init(void)
{
    ipc_data.pending = TRUE;
    GEN_BG_INT(ipc_bg);
}

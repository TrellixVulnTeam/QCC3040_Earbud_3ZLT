/* Copyright (c) 2016 Qualcomm Technologies International, Ltd. */
/*   %%version */

#include "ipc/ipc_private.h"
#include "ipc/ipc_signal.h"


/**
 * Attempt to send the supplied message
 *
 * \note This function must be called with interrupts blocked!
 *
 * @param msg_id ID of messae to send
 * @param msg Pointer to message body
 * @param len_bytes Length of message body
 * @return TRUE if the message was successfully added to the IPC send buffer,
 * else FALSE
 */
static bool ipc_try_send(IPC_SIGNAL_ID msg_id, const void *msg, uint16 len_bytes)
{
    if (ipc_buffer_has_space_for(ipc_data.send, len_bytes))
    {
        IPC_HEADER header;
        header.id = msg_id;
        ipc_header_timestamp_set(&header);
        ipc_send_no_checks(&header, msg, len_bytes);
        return TRUE;
    }
    return FALSE;
}

/**
 * Tries to send the Signal Interproc Event message. The attempt fails if there
 * is not enough space in the send buffer to copy the message. The only reason
 * it should happen is if the signal message is already pending.
 */
static void ipc_send_signal_interproc_event(void)
{
    if (ipc_buffer_has_space_for_interproc_event(ipc_data.send))
    {
        IPC_SIGNAL_INTERPROC_EVENT_PRIM sig_msg;
        sig_msg.header.id = IPC_SIGNAL_ID_SIGNAL_INTERPROC_EVENT;
        ipc_header_timestamp_set(&sig_msg.header);
        ipc_send_no_checks(&sig_msg.header, &sig_msg, sizeof(sig_msg));
    }
}

/**
 * Place the supplied message on the back-up queue
 *
 * \note This function must be called with interrupts blocked!
 *
 * @param msg_id Message ID
 * @param msg Message body
 * @param len_bytes Length of message body
 */
static void ipc_queue_msg(IPC_SIGNAL_ID msg_id, const void *msg,
                          uint16 len_bytes)
{
    IPC_MSG_QUEUE **pnext = &ipc_data.send_queue;
    IPC_MSG_QUEUE *new;
    void *mem;
    while(*pnext != NULL)
    {
        pnext = &((*pnext)->next);
    }
    /* Allocate a block big enough for both queue entry structure and the
     * message*/
    mem = pmalloc(sizeof(IPC_MSG_QUEUE) + len_bytes);
    new = (IPC_MSG_QUEUE *)mem;
    new->next = NULL;
    new->msg_id = msg_id;
    new->msg = (void *)((char *)mem + sizeof(IPC_MSG_QUEUE));
    memcpy(new->msg, msg, len_bytes);
    new->length_bytes = len_bytes;
    *pnext = new;

    /* Schedule another attempt to send */
    GEN_BG_INT(ipc_bg);
}

bool ipc_clear_queue(void)
{
    IPC_MSG_QUEUE **pnext = &ipc_data.send_queue;
    while(*pnext != NULL)
    {
        IPC_MSG_QUEUE *msg_entry = *pnext;
        if (ipc_try_send(msg_entry->msg_id, msg_entry->msg,
                                                    msg_entry->length_bytes))
        {
            /* remove this entry from the list and continue */
            *pnext = (*pnext)->next;
            /* the queue entry and message are in a single pmalloc block, so
             * just one pfree is required */
            pfree(msg_entry);
        }
        else
        {
            /* Ran out of space again... */
            return FALSE;
        }
    }
    assert(ipc_data.send_queue == NULL);
    return TRUE;
}

void ipc_send(IPC_SIGNAL_ID msg_id, const void *msg, uint16 len_bytes)
{

    /* Check the length is a multiple of 4 */
    assert(!(len_bytes & 3));

    block_interrupts();
    /* Try and clear the queue first; if that succeeds, try and send the message
     * If the message isn't sent, queue it */
    if (!ipc_clear_queue() || !ipc_try_send(msg_id, msg, len_bytes))
    {
        ipc_send_signal_interproc_event();
        ipc_queue_msg(msg_id, msg, len_bytes);
    }
    unblock_interrupts();

}


void ipc_transaction(IPC_SIGNAL_ID msg_id, const void *msg, uint16 len_bytes,
                     IPC_SIGNAL_ID rsp_id, void *blocking_msg)
{
    ipc_send(msg_id, msg, len_bytes);
    ipc_recv(rsp_id, blocking_msg);
}

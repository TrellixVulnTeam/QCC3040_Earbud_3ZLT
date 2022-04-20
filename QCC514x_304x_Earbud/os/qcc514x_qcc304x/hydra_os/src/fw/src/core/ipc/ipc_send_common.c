/* Copyright (c) 2020 Qualcomm Technologies International, Ltd. */
/*   %%version */

#include "ipc/ipc_private.h"

void ipc_send_outband(IPC_SIGNAL_ID msg_id, void *payload,
                      uint32 payload_len_bytes)
{
    IPC_TUNNELLED_PRIM_OUTBAND prim;

    prim.length = payload_len_bytes;
    prim.payload = payload;
    ipc_send(msg_id, &prim, sizeof(IPC_TUNNELLED_PRIM_OUTBAND));
}

void ipc_send_bool(IPC_SIGNAL_ID msg_id, bool val)
{
    IPC_BOOL_RSP rsp;
    rsp.ret = val;
    ipc_send(msg_id, &rsp, sizeof(rsp));
}

void ipc_send_uint16(IPC_SIGNAL_ID msg_id, uint16 val)
{
    IPC_UINT16_RSP rsp;
    rsp.ret = val;
    ipc_send(msg_id, &rsp, sizeof(rsp));
}

void ipc_send_int16(IPC_SIGNAL_ID msg_id, int16 val)
{
    IPC_INT16_RSP rsp;
    rsp.ret = val;
    ipc_send(msg_id, &rsp, sizeof(rsp));
}

void ipc_send_signal(IPC_SIGNAL_ID sig_id)
{
    IPC_SIGNAL sig;
    sig.header.id = sig_id;
    ipc_send(sig_id, &sig, sizeof(sig));
}

void ipc_send_no_checks(const IPC_HEADER *header, const void *msg,
                        uint16 len_bytes)
{
    IPC_HEADER *send;

    /* Check the length is a multiple of 4 */
    assert(!(len_bytes & 3));

    send = ipc_buffer_map_write(ipc_data.send);
    memcpy(send, msg, len_bytes);

    /* Overwrite the start of the message with the header which is populated by
       IPC rather than the client. */
    *send = *header;

    ipc_buffer_update_write(ipc_data.send, len_bytes);

    /* Raise IPC interrupt.  It doesn't matter what we write */
    hal_set_reg_interproc_event_1(1);

    /* Update maximum IPC buffer utilisation. */
    if(ipc_buffer_used(ipc_data.send) > ipc_data.max_send_bytes_used)
    {
        ipc_data.max_send_bytes_used = ipc_buffer_used(ipc_data.send);
    }
}

#if IPC_PROTOCOL_ID == 2
void ipc_try_send_common(const IPC_HEADER *header, const void *msg,
                         uint16 len_bytes)
{
    block_interrupts();
    {
        if (ipc_buffer_has_space_for(ipc_data.send, len_bytes))
        {
            ipc_send_no_checks(header, msg, len_bytes);
        }
        else
        {
            panic_diatribe(PANIC_IPC_BUFFER_OVERFLOW, len_bytes);
        }
    }
    unblock_interrupts();
}
#endif

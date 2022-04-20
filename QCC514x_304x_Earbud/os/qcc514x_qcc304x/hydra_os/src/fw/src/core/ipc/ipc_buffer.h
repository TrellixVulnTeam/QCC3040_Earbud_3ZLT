/* Copyright (c) 2020 Qualcomm Technologies International, Ltd. */
/*   %%version */

#ifndef IPC_BUFFER_H_
#define IPC_BUFFER_H_

#include "ipc/ipc_prim.h"
#include "buffer/buffer.h"

/**
 * The IPC module uses an IPC_BUFFER to communicate with the other core.
 * Depending on the protocol version the IPC_BUFFER could be a message aware
 * or message unaware buffer.
 *
 * For IPC_PROTOCOL_ID 0 and 1 IPC_BUFFER is a BUFFER_MSG type. Lengths of
 * messages are communicated to the other core by a length entry in an array.
 * The number of array entries is limited to 15 so the maximum number of
 * messages at once is limited to 15.
 *
 * For IPC_PROTOCOL_ID 2 the IPC_BUFFER is a BUFFER type. Lengths of messages
 * are communicated to the other core in-band. Each message has a length entry
 * as part of the header.
 */

#if IPC_PROTOCOL_ID == 2
/**
 * The size of the IPC send and receive buffers.
 *
 * Once the IPC buffers are full the firmware will panic.
 * To ensure this never happens the IPC buffer should be oversized.
 * Since the IPC buffers are MMU based the extra memory will only be used if
 * there are a lot of outstanding IPC messages.
 * The IPC does not use BUFFER_MSG buffers so it not limited to 15 message
 * entries.
 *
 * \ingroup ipc_impl
 */
#define IPC_BUFFER_SIZE MMU_BUFFER_SIZE_1024

/**
 * The IPC buffer type.
 *
 * This is a define rather than a typedef so that pydbg can tell how to decode
 * the IPC buffer based on the type information in the dwarf.
 */
#define IPC_BUFFER BUFFER

#define ipc_buffer_create_buffer_location(_size, _recv) \
    buf_create_buffer_location((_size), (_recv))
#define ipc_buffer_init_from_handle(_size, _hdl, _send) \
    buf_init_from_handle((_size), (_hdl), (_send))
#define ipc_buffer_update_back(_recv, _msg_length) \
    buf_raw_read_update((_recv), (_msg_length))
#define ipc_buffer_update_tail_free(_recv) \
    buf_raw_update_tail_free((_recv), (_recv)->outdex)
#define ipc_buffer_update_tail_no_free(_recv) \
    buf_raw_update_tail_no_free((_recv), (_recv)->outdex)
#define ipc_buffer_map_write(_send) (void *) \
    buf_raw_write_only_map_8bit((_send))
#define ipc_buffer_update_write(_send, _octets) \
    buf_raw_write_update((_send), (_octets))
#define ipc_buffer_used(_send) \
    BUF_GET_USED((_send))
#define ipc_buffer_has_space_for(_send, _bytes) \
    (BUF_GET_FREESPACE((_send)) >= (_bytes))
#define ipc_buffer_map_read(_recv) (const void *) \
    buf_raw_read_map_8bit((_recv))
#define ipc_buffer_map_read_length(_recv) \
    (((const void *) buf_raw_read_map_8bit((_recv)))->length_bytes)
#define ipc_buffer_any_messages(_recv) \
    BUF_GET_AVAILABLE((_recv))

#else
/**
 * The size of the IPC send and receive buffers.
 *
 * The implementation is tolerant of overflowing the IPC buffers, it puts
 * messages it can't insert into a linked list until it can insert them.
 * Hence we trade off buffer size against pmalloc memory.
 * Not only buffer-size is important, since a buffer can be logically full
 * (contains 16 messages) before it is physically full.
 * Enhancing the BUFFER_MSG class to fall back on pmalloc for additional logical
 * slots, increases implementation complexity, and we have chosen not to.
 *
 * \ingroup ipc_impl
 */
#define IPC_BUFFER_SIZE MMU_BUFFER_SIZE_512

/**
 * The IPC buffer type.
 *
 * This is a define rather than a typedef so that older pydbg's can continue
 * to work with newer firmware that uses this IPC version.
 */
#define IPC_BUFFER BUFFER_MSG

#define ipc_buffer_create_buffer_location(_size, _recv) \
    buf_msg_create_buffer_location((_size), (_recv))
#define ipc_buffer_init_from_handle(_size, _hdl, _send) do { \
        memset((_send), 0, sizeof(IPC_BUFFER)); \
        buf_init_from_handle((_size) ,(_hdl), &(_send)->buf); \
    } while(0)
#define ipc_buffer_update_back(_recv, _msg_length) do { \
        UNUSED((_msg_length)); \
        buf_update_back((_recv)); \
    } while(0)
#define ipc_buffer_update_tail_free(_recv) \
    buf_update_behind_free((_recv))
#define ipc_buffer_update_tail_no_free(_recv) \
    buf_update_behind((_recv))
#define ipc_buffer_map_write(_send) \
    (void *) buf_map_front_msg((_send))
#define ipc_buffer_update_write(_send, _octets) \
    buf_add_to_front((_send), (_octets))
#define ipc_buffer_used(_send) \
    BUF_GET_USED(&(_send)->buf)
/* Always leave space to send an interproc event message. See B-204884. */
#define ipc_buffer_has_space_for(_send, _bytes) \
    ((BUF_NUM_MSGS_AVAILABLE((_send)) > 1) && \
     (BUF_GET_FREESPACE(&(_send)->buf) >= \
      (_bytes) + sizeof(IPC_SIGNAL_INTERPROC_EVENT_PRIM)))
/* Must be used for interproc event message only */
#define ipc_buffer_has_space_for_interproc_event(_send) \
    ((BUF_NUM_MSGS_AVAILABLE(_send)) && \
     (BUF_GET_FREESPACE(&(_send)->buf) >= \
      sizeof(IPC_SIGNAL_INTERPROC_EVENT_PRIM)))
#define ipc_buffer_map_read(_recv) \
    (const void *) buf_map_back_msg((_recv))
#define ipc_buffer_map_read_length(_recv) \
    buf_get_back_msg_len((_recv))
#define ipc_buffer_any_messages(_recv) \
    BUF_ANY_MSGS_TO_SEND((_recv))

#endif

#endif /* IPC_BUFFER_H_ */

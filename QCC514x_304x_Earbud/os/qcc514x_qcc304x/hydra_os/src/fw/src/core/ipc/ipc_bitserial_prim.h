/* Copyright (c) 2016 - 2021 Qualcomm Technologies International, Ltd. */
/*   %%version */
/**
 * \file
 *
 * Bitserial primitive declarations for IPC
 */
#ifndef IPC_BITSERIAL_PRIM_H__
#define IPC_BITSERIAL_PRIM_H__

#if TRAPSET_BITSERIAL
#include "bitserial/bitserial_if.h"

typedef struct IPC_BITSERIAL_OPEN {
    IPC_HEADER header;
    bitserial_block_index block_index;
    const bitserial_config * config;
} IPC_BITSERIAL_OPEN;

typedef struct IPC_BITSERIAL_CLOSE {
    IPC_HEADER header;
    bitserial_handle handle;
} IPC_BITSERIAL_CLOSE;

typedef struct IPC_BITSERIAL_TRANSFER {
    IPC_HEADER header;
    bitserial_handle handle;
    bitserial_transfer_handle * transfer_handle_ptr;
    const uint8 * tx_data;
    uint16 tx_size;
    uint8 * rx_data;
    uint16 rx_size;
} IPC_BITSERIAL_TRANSFER;

typedef struct IPC_BITSERIAL_WRITE {
    IPC_HEADER header;
    bitserial_handle handle;
    bitserial_transfer_handle * transfer_handle_ptr;
    const uint8 * data;
    uint16 size;
    bitserial_transfer_flags flags;
} IPC_BITSERIAL_WRITE;

typedef struct IPC_BITSERIAL_READ {
    IPC_HEADER header;
    bitserial_handle handle;
    bitserial_transfer_handle * transfer_handle_ptr;
    uint8 * data;
    uint16 size;
    bitserial_transfer_flags flags;
} IPC_BITSERIAL_READ;

typedef struct IPC_BITSERIAL_CHANGE_PARAM {
    IPC_HEADER header;
    bitserial_handle handle;
    bitserial_changeable_params key;
    uint16 value;
    bitserial_transfer_flags flags;
} IPC_BITSERIAL_CHANGE_PARAM;

typedef struct IPC_BITSERIAL_HANDLE_RSP {
    IPC_HEADER header;
    bitserial_handle ret;
} IPC_BITSERIAL_HANDLE_RSP;


typedef struct IPC_BITSERIAL_RESULT_RSP {
    IPC_HEADER header;
    bitserial_result ret;
} IPC_BITSERIAL_RESULT_RSP;



#endif /* TRAPSET_BITSERIAL */

#endif /* IPC_BITSERIAL_PRIM_H__ */

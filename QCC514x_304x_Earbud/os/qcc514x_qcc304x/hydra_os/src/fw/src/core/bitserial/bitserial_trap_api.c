/* Copyright (c) 2021 Qualcomm Technologies International, Ltd. */
/*   %%version */
/**
 * \file bitserial_trap_api.c
 * Helper functions for Bitserial trap APIs in P0 and P1.
 */

#include "bitserial/bitserial.h"

#if defined(INSTALL_BITSERIAL)
#if TRAPSET_BITSERIAL

bool bitserial_trap_api_to_bs_flags(bitserial_action_flags *bs_flags, bitserial_transfer_flags api_flags)
{
    bitserial_action_flags gen_flags = BITSERIAL_ACTION_FLAGS_NONE;

    /* Can't have both 0 and 1 as the start/stop bit value */
    if ((api_flags & (BITSERIAL_SPI_FLAG_START_0 | BITSERIAL_SPI_FLAG_START_1)) ||
        (api_flags & (BITSERIAL_SPI_FLAG_STOP_0 | BITSERIAL_SPI_FLAG_STOP_1)))
    {
        return FALSE;
    }

    /* Set start bits */
    if (api_flags & BITSERIAL_SPI_FLAG_START_0)
    {
        gen_flags |= BITSERIAL_ACTION_TRANSFER_START_BIT_EN;
    }
    else if (api_flags & BITSERIAL_SPI_FLAG_START_1)
    {
        gen_flags |= (BITSERIAL_ACTION_TRANSFER_START_BIT_EN | BITSERIAL_ACTION_TRANSFER_START_BIT_1);
    }

    /* Set stop bits */
    if (api_flags & BITSERIAL_SPI_FLAG_STOP_0)
    {
        gen_flags |= BITSERIAL_ACTION_TRANSFER_STOP_BIT_EN;
    }
    else if (api_flags & BITSERIAL_SPI_FLAG_STOP_1)
    {
        gen_flags |= (BITSERIAL_ACTION_TRANSFER_STOP_BIT_EN | BITSERIAL_ACTION_TRANSFER_STOP_BIT_1);
    }

    /* Set no stop */
    if (api_flags & BITSERIAL_FLAG_NO_STOP)
    {
        gen_flags |= BITSERIAL_ACTION_TRANSFER_STOP_TOKEN_DISABLE;
    }

    /* Blocking */
    if (api_flags & BITSERIAL_FLAG_BLOCK)
    {
        gen_flags |= BITSERIAL_ACTION_FLAG_BLOCKING;
    }   

    *bs_flags = gen_flags;
    return TRUE;
}

#endif /* TRAPSET_BITSERIAL */
#endif

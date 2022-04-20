/* Copyright (c) 2016 Qualcomm Technologies International, Ltd. */
/*   %%version */
/**
 * \file 
 * Bitserial public header file.
 * This file contains public interfaces.
 */

#ifndef __BITSERIAL_H__
#define __BITSERIAL_H__

#include "hal/hal_bitserial.h"

#ifdef INSTALL_BITSERIAL
#include "app/bitserial/bitserial_if.h"

/******************************************************************************
 *
 *  Enums
 *
 *****************************************************************************/
/** Enum encoding the signals used by the bitserial. */
typedef enum bitserial_signal
{
    BITSERIAL_CLOCK_IN,
    BITSERIAL_CLOCK_OUT,
    BITSERIAL_DATA_IN,
    BITSERIAL_DATA_OUT,
    BITSERIAL_SEL_IN,
    BITSERIAL_SEL_OUT
} bitserial_signal;


/* Enum for submitting actions (config, transfer) */
typedef enum
{
    BITSERIAL_ACTION_TYPE_NULL           = 0,
    BITSERIAL_ACTION_TYPE_TRANSFER       = 1,
    BITSERIAL_ACTION_TYPE_CONFIG_I2CADDRESS = 2,
    BITSERIAL_ACTION_TYPE_CONFIG_SPEED   = 3
} bitserial_action_type;


/* Type definition for flags in an 'add' operation */
typedef enum
{
    BITSERIAL_ACTION_FLAGS_NONE = 0,
    BITSERIAL_ACTION_TRANSFER_RXFIRST = 1<<0,
    BITSERIAL_ACTION_TRANSFER_STOP_TOKEN_DISABLE = 1<<1,
    BITSERIAL_ACTION_TRANSFER_START_BIT_EN = 1<<2,
    BITSERIAL_ACTION_TRANSFER_START_BIT_1 = 1<<3,
    BITSERIAL_ACTION_TRANSFER_STOP_BIT_EN = 1<<4,
    BITSERIAL_ACTION_TRANSFER_STOP_BIT_1 = 1<<5,

    /* Universal flag to indicate blockingness */
    BITSERIAL_ACTION_FLAG_BLOCKING = 1<<11,

    /* INTERNAL FLAGS - DO NOT SET OUTSIDE OF BITSERIAL CODE */
    /* Universal flag to indicate the action has been processed */
    BITSERIAL_ACTION_FLAG_COMPLETE = 1<<13,
    /* Universal flag to say whether the action block is dynamic */
    BITSERIAL_ACTION_FLAG_DYNAMIC = 1<<14,
    /* Universal flag to indidate the hardware is idle */
    BITSERIAL_ACTION_FLAG_IDLE = 1<<15
} bitserial_action_flags;


/******************************************************************************
 *
 *  Macros
 *
 *****************************************************************************/
/* Default timeout of 1000ms used if application did not provide a customer timeout */
#define PANIC_ON_TIMEOUT_MS 1000


/******************************************************************************
 *
 *  Function definitions
 *
 *****************************************************************************/

/** Prototypes for callback functions
 */
typedef void (*bitserial_cb_fn)(bitserial_handle handle);
typedef void (*bitserial_done_cb_fn)(bitserial_transfer_handle *tfh, bool blocking, bitserial_result result);


/** Open the bitserial instance
 *
 * Opening a bitserial instance is an asynchronous operation, hence the need
 * for a callback function to report the eventual handle.
 *
 * \param i The bitserial block index to open
 * \param config Pointer to the configuaration data for the instance
 * \param open_cb Function to call with the handle once the instance is opened
 * \return False if there is any error condition preventing opening
 */
extern bool bitserial_open(bitserial_block_index i,
                           const bitserial_config *config,
                           bitserial_cb_fn open_cb);


/** Close the bitserial instance
 *
 * Closing a bitserial instance is an asynchronous operation, hence the need
 * for a callback function to report the closure.
 * Any pending transfers will be junked, and any in-progress ones will complete
 * without further notification.
 *
 * \param handle Handle of the bitserial to close
 * \param close_cb Function to call once the instance is closed
 * \return False if there is an error preventing closing
 */
extern bool bitserial_close(bitserial_handle handle,
                            bitserial_cb_fn close_cb);


/** Add a configuration request to the queue
 *
 * All bitserials queue incoming requests if they can. Blocking requests
 * cannot be queued if there are any other requests already in the queue.
 *
 * Both configs and transfers are queued together, so will execute in the
 * order that they are submitted.
 *
 * \param handle Handle of the bitserial submitting to
 * \param action The config action to be requested
 * \param value Value of the config action parameter
 * \param flags Blocking/non-blocking
 * \return False if the request is invalid or cannot be queued. In blocking
 *         mode, the result of the configuration.
 */
extern bool bitserial_add_config(bitserial_handle handle,
                                 bitserial_action_type action,
                                 uint16 value,
                                 bitserial_action_flags flags);


/** Add a transfer request to the queue
 *
 * All bitserials queue incoming requests if they can. Blocking requests
 * cannot be queued if there are any other requests already in the queue.
 *
 * Both configs and transfers are queued together, so will execute in the
 * order that they are submitted.
 *
 * \param handle Handle of the bitserial submitting to
 * \param xf_hdl Location to store the transfer unique identifier
 * \param tx_data Address of the data to transmit
 * \param tx_size Size of the data to transmit, in bytes
 * \param rx_data Address to put received data
 * \param rx_size Size of the data to receive, in bytes
 * \param flags Flags relating to the transfer operation
 * \param completed_cb The function to call on completion of the transfer
 * \return False if the request is invalid or cannot be queued. In blocking
 *         mode, the result of the transfer.
 */
extern bool bitserial_add_transfer(bitserial_handle handle,
                                   bitserial_transfer_handle *xf_hdl,
                                   const uint8 *tx_data, uint16 tx_size,
                                   uint8 *rx_data, uint16 rx_size,
                                   bitserial_action_flags flags,
                                   bitserial_done_cb_fn completed_cb);


/** Configure PIOs
 *
 * Configures the bitserial PIOs. Used internally to set up for production test
 * The block index must be one that is controlled by P0.
 * \param blk_idx Used to indicate which bitserial instance to configure.
 * \param signal Bitserial signal to mux on the given PIO.
 * \param pio_index PIO index. Please use NUMBER_OF_PIOS to indicate an unused
 * signal.
 */
extern void bitserial_configure_pio(bitserial_block_index blk_idx,
                                    bitserial_signal signal,
                                    uint8 pio_index);


/** Convert bitserial_transfer_flags to bitserial_action_flags.
 *
 * Convert API flags into ones that the bitserial subsystem uses, doing
 * some sanity checking at the same time.
 * \param bs_flags Pointer to target bitserial flags.
 * \param api_flags Source API flags.
 * \return TRUE if valid api_flags and conversion complete.
 */
extern bool bitserial_trap_api_to_bs_flags(bitserial_action_flags *bs_flags, bitserial_transfer_flags api_flags);

#endif /* INSTALL_BITSERIAL */

#endif /* __BITSERIAL_H__ */

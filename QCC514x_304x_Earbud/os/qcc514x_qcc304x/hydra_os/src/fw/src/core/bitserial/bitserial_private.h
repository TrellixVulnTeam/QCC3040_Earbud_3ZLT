/* Copyright (c) 2016-2021 Qualcomm Technologies International, Ltd. */
/*   %%version */
/**
 * \file 
 * Bitserial private header file.
 * This file contains private interfaces used internally and for testing.
 */

#ifndef __BITSERIAL_PRIVATE_H__
#define __BITSERIAL_PRIVATE_H__

#include "hydra/hydra_types.h"
#include "hydra/hydra_macros.h"
#include "hydra_log/hydra_log.h"
#include "sched/sched.h"
#include "hal/hal_bitserial.h"
#include "buffer/buffer.h"
#include "hal/hal_macros.h"
#include "bus_interrupt/bus_interrupt.h"
#include "panic/panic.h"
#include <assert.h>
#include "utils/utils_sll.h"
#include "app/bitserial/bitserial_if.h"

#ifdef INSTALL_BITSERIAL

#ifdef DORM_MODULE_PRESENT
#include "dorm/dorm.h"

#define BITSERIAL_ENABLE_SLEEP() \
    dorm_allow_deep_sleep(DORMID_BITSERIAL)
#define BITSERIAL_DISABLE_SLEEP() \
    dorm_disallow_deep_sleep(DORMID_BITSERIAL)
#else
#define BITSERIAL_ENABLE_SLEEP() ((void)0)
#define BITSERIAL_DISABLE_SLEEP() ((void)0)
#endif /* DORM_MODULE_PRESENT */

/* Define the default level of bitserial debug compiled in */
#ifndef LOG_LEVEL_BITSERIAL
#define LOG_LEVEL_BITSERIAL 0
#endif

/* Debug macros to determine what debug is compiled in */
#if LOG_LEVEL_BITSERIAL > 3
#define BITSERIAL_L4_DBG_MSG(x)              L4_DBG_MSG(x)
#define BITSERIAL_L4_DBG_MSG1(x, a)          L4_DBG_MSG1(x, a)
#define BITSERIAL_L4_DBG_MSG2(x, a, b)       L4_DBG_MSG2(x, a, b)
#define BITSERIAL_L4_DBG_MSG3(x, a, b, c)    L4_DBG_MSG3(x, a, b, c)
#define BITSERIAL_L4_DBG_MSG4(x, a, b, c, d) L4_DBG_MSG4(x, a, b, c, d)
#else
#define BITSERIAL_L4_DBG_MSG(x)
#define BITSERIAL_L4_DBG_MSG1(x, a)
#define BITSERIAL_L4_DBG_MSG2(x, a, b)
#define BITSERIAL_L4_DBG_MSG3(x, a, b, c)
#define BITSERIAL_L4_DBG_MSG4(x, a, b, c, d)
#endif /* LOG_LEVEL_BITSERIAL */
#if LOG_LEVEL_BITSERIAL > 4
#define BITSERIAL_L5_DBG_MSG(x)              L5_DBG_MSG(x)
#define BITSERIAL_L5_DBG_MSG1(x, a)          L5_DBG_MSG1(x, a)
#define BITSERIAL_L5_DBG_MSG2(x, a, b)       L5_DBG_MSG2(x, a, b)
#define BITSERIAL_L5_DBG_MSG3(x, a, b, c)    L5_DBG_MSG3(x, a, b, c)
#define BITSERIAL_L5_DBG_MSG4(x, a, b, c, d) L5_DBG_MSG4(x, a, b, c, d)
#else
#define BITSERIAL_L5_DBG_MSG(x)
#define BITSERIAL_L5_DBG_MSG1(x, a)
#define BITSERIAL_L5_DBG_MSG2(x, a, b)
#define BITSERIAL_L5_DBG_MSG3(x, a, b, c)
#define BITSERIAL_L5_DBG_MSG4(x, a, b, c, d)
#endif /* LOG_LEVEL_BITSERIAL */


/** Quick safety check while we're using bitserial_instance for indexing an
 * array. I know we shouldn't, but it's easy...
 */
COMPILE_TIME_ASSERT(BITSERIAL_INSTANCE_0 == 0, bitserial_instance_0_not_indexable);
COMPILE_TIME_ASSERT(BITSERIAL_INSTANCE_1 == 1, bitserial_instance_1_not_indexable);


/******************************************************************************
 *
 *  Data structures
 *
 ******************************************************************************/
/* Pending action item element */
typedef struct
{
    utils_SLLMember sll_header; /* THIS MUST BE THE FIRST ELEMENT */
    bitserial_action_type type; /* What the action is */
    union {
        struct transfer_action
        {
            /* Param for MESSAGE_BITSERIAL_EVENT on completion. If BITSERIAL_TRANSFER_HANDLE_NONE,
             * then don't send the message on completion.
             */
            bitserial_transfer_handle *tf_handle;
            const uint8  *tx_data; /* Pointer to the data to transmit */
            uint8  *rx_data; /* Base pointer to write received data to */
            uint16  tx_len;  /* Amount to transmit */
            uint16  rx_len;  /* Expected amount to receive */
        } transfer;
        struct config_action
        {
            uint16 value;    /* Config value */
        } config;
    } u;
    bitserial_action_flags flags; /* Control flags for the action */
    bitserial_done_cb_fn done_cb;
} bitserial_action;

/*
 * Structure containing everything that the hardware needs to know to be able
 * to do a transfer 'operation'. Flags gets used to do partial transfers and
 * other things like that.
 *
 * Some fields can be updated by the irq routine following the completion of
 * the op. Specifically, the tx/rx lengths will be updated with the actual
 * amount transferred (the 'complete' routine can then use that to determine
 * whether the head transfer is actually complete or not).
 */
typedef struct
{
    uint16                 tx_len;  /* Amount to transmit */
    uint16                 rx_len;  /* Amount to recieve */
    bitserial_action_flags flags;
    bitserial_result       result;  /* If completed, the result to return */
    tid                    timeout_tid;  /* Timer ID for the timeout manager */
#ifdef BITSERIAL_EARLY_INTERRUPT_WORKAROUND
    tid                    check_tid;      /* For managing early interrupt */
#endif
} bitserial_op;


/* The anchor structure. One per bitserial hw instance. Must contain all the
 * information that is needed for transfers, regardless of processor.
 */
typedef struct
{
    BUFFER *tx_buffer;
    BUFFER *rx_buffer;
    bitserial_handle handle;   /* Instance handle given on bitserial_open */
    utils_SLL    action_queue; /* Queue of things waiting on the hardware */
    bitserial_op active_op;    /* Info on the active op on the hardware */
    uint32  byte_time_ns;   /* Time one byte takes */
    uint16  api_timeout_ms; /* API-passed timeout _after_ the transfer should have finished */
    uint8   rolling_handle; /* Value used for the rolling part of the 'transfer handle' */
    uint8   bytes_per_word; /* Bytes per word - needed for sanity checking */
    const bitserial_config *hw_config; /* The instance-specific configuration */
} bitserial_hw;


/******************************************************************************
 *
 *  Function definitions
 *
 ******************************************************************************/

/** Initialise the action engine
 *
 * The engine may run on a different processor to the open/close code, so this
 * initialises it. It is called from the open trap code.
 *
 * \param i Physical instance to initialise
 * \param hw_data Pointer to the data structure populated by the open code
 */
extern void bitserial_action_init(bitserial_instance i, bitserial_hw *hw_data);


/** Destroy the data engine
 *
 * The engine may run on a different processor to the open/close code, so this
 * destroys it. It is called from the close trap code.
 *
 * \param i Physical instance to destroy
 */
extern void bitserial_action_destroy(bitserial_instance i);


/** Set up the hardware instance
 *
 * Sets up the hardware according to the configuration provided.
 * \param i Used to indicate which bitserial instance to configure.
 * \param vm_config Pointer to a configuration structure.
 * \param byte_time_ns Pointer to bitserial_hw.byte_time_ns.
 */
extern void bitserial_configure(
    bitserial_instance i,
    const bitserial_config *vm_config,
    uint32 *byte_time_ns
);


/** Validate the requested bitserial clock and set the HW registers.
 *
 * Set up the hardware registers for frequency and offset.
 * \param i Used to indicate which bitserial instance to configure.
 * \param bs_mode bitserial_mode (I2C or SPI).
 * \param config_clock_frequency_khz Requested bitserial clock.
 * \param freq_change TRUE = frequency change.  FALSE = bitserial config.
 * \param byte_time_ns Pointer to bitserial_hw.byte_time_ns.
 */
extern void bitserial_config_clock_freq_set(
    bitserial_instance i,
    bitserial_mode bs_mode,
    uint32 config_clock_frequency_khz,
    bool freq_change,
    uint32 *byte_time_ns
);

/******************************************************************************
 *
 *  Macros
 *
 ******************************************************************************/

/* Conversions between:
 *  bitserial_block_index : API identifier of bitserial hardware to use.
 *  bitserial_handle      : Returned identifier of bitserial hardware to use
 *  bitserial_instance    : HAL identifier of bitserial hardware to use.
 *
 * bitserial_block_index is defined explicitly in the common header, and is an
 * incrementing value. Bit 7 is used as a flag to 'virtualise' P1 access to
 * the bitserial blocks.
 *
 * bitserial_handle is an abstract 8-bit 'key' to use to submit transfers once
 * a block has been BitserialOpen'ed. It is implemented as simply being the
 * block_index.
 *
 * bitserial_instance is opaque in the HAL, so can only be converted to/from
 * by calls into the HAL layer.
 */

/* Get an instance from a block index - Needs to go via the uint8 because the
 * hal doesn't understand what a /block/ index is. We do.
 */
#define BITSERIAL_BLOCK_INDEX_TO_INSTANCE(x) \
    ((bitserial_instance)(hal_bitserial_get_instance_from_index((((uint8)(x))&0x7f))))

/* Derive a handle from a block index */
#define BITSERIAL_BLOCK_INDEX_TO_HANDLE(x) (x)
/* Get an array index from a handle */
#define BITSERIAL_HANDLE_TO_INDEX(x)  (((uint8)(x))&0x7f)

/* Get the hardware instance from the handle */
#define BITSERIAL_HANDLE_TO_INSTANCE(x) (BITSERIAL_BLOCK_INDEX_TO_INSTANCE(x))

/* True if the given handle indicates P1 transfer code */
#define BITSERIAL_HANDLE_ON_P1(x) ((x) & 0x80)


/******************************************************************************
 *
 *  Debug functions
 *
 ******************************************************************************/
#ifdef DEBUG_BITSERIAL_ON_PIO
extern void bitserial_debug_claim_pio(uint16 pio);
extern void bitserial_debug_set_pio(uint16 pio, bool level);
#endif

#endif /* INSTALL_BITSERIAL */


#endif /* __BITSERIAL_PRIVATE_H__ */

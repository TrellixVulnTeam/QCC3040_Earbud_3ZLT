/* Copyright (c) 2021 Qualcomm Technologies International, Ltd. */
/*   %%version */
/**
 * \file
 * Implement action queueing and data transfer routines for the bitserialiser
 *
 * The action queue code must be able to run on a different processor to the
 * open/close code. So any information required by the action engine must be
 * stored locally to this file and populated appropriately.
 *
 * Note: Configuration changes are also queued as actions, as they need to be
 * processed at the right point in time.
 *
 * API functions take a handle, all other (internal) functions use the
 * instance as the identifier, as that's what the hal layer requires.
 *
 */
#include "bitserial/bitserial.h"
#include "bitserial/bitserial_private.h"
#include "flash_header/flash_header.h"
#include "memory_map.h"
#include "pmalloc/pmalloc.h"

#ifdef INSTALL_BITSERIAL

/*---------------------------------------------------------------------------*
 * Static function prototypes
 *---------------------------------------------------------------------------*/
static void action_engine_run(bitserial_instance i);
static void action_engine_complete(bitserial_instance i, bitserial_result result);
static bool action_submit(bitserial_instance i, bitserial_action *action);
static void action_engine_irq(bitserial_instance i);
static void action_engine_process(void);
static void transfer_timeout(void *dptr);
#ifdef BITSERIAL_EARLY_INTERRUPT_WORKAROUND
static void transfer_check(void *dptr);
#endif
static void active_op_program(bitserial_instance i);

static bool action_queue_add(bitserial_instance i, bitserial_action *action);
static void action_queue_advance(bitserial_instance i);

/*---------------------------------------------------------------------------*
 * Local data declarations
 *---------------------------------------------------------------------------*/
/* If this code is on P1 then the pointers will point into P0-allocated dynamic
 * memory, which we're allowed to look at/write to.
 */
bitserial_hw *instance[HAVE_NUMBER_OF_BITSERIALS];


/******************************************************************************
 *
 * bitserial_action_init
 *
 * Called once per instance from BitserialOpen() to initialise the transfer
 * engine, as it may be running on a different processor to the Open() code.
 *
 * This function must only be called if the instance is not already in use.
 * This function must be called after the hardware has been claimed and
 * initialised to its idle state.
 */
void bitserial_action_init(bitserial_instance i, bitserial_hw *hw_data)
{
    /* Point at the (pmalloc'ed on P0, so visible from P1 too) hardware
     * instance data created by BitserialOpen()
     */
    instance[i] = hw_data;

    /* Claim interrupts on this processor for the transfer engine handler */
    hal_bitserial_int_enable(i, action_engine_irq);

    /* Initialise the transfer-specific data structures for this instance */
    instance[i]->active_op.flags = BITSERIAL_ACTION_FLAG_IDLE;
    utils_SLL_init(&instance[i]->action_queue);

    /* Register the swirq - we only do this on opening the first instance */
    if (!hal_bitserial_swint_handler_get())
    {
        hal_bitserial_swint_enable(action_engine_process);
    }

    BITSERIAL_L5_DBG_MSG2("BITSERIAL%d: Action engine initialised. hw_data @0x%08x", i, hw_data);
}


/******************************************************************************
 *
 * bitserial_action_destroy
 *
 * Destroy all queued transfers and tidy-down the instance. Expected to be
 * called from BitserialClose().
 */
void bitserial_action_destroy(bitserial_instance i)
{
    uint8   j;

    /* Stop any active timers */
#ifdef BITSERIAL_EARLY_INTERRUPT_WORKAROUND
    if (instance[i]->active_op.check_tid != NO_TID)
    {
        timer_cancel_event(instance[i]->active_op.check_tid);
        instance[i]->active_op.check_tid = NO_TID;
    }
#endif
    if (instance[i]->active_op.timeout_tid != NO_TID)
    {
        timer_cancel_event(instance[i]->active_op.timeout_tid);
        instance[i]->active_op.timeout_tid = NO_TID;
    }

    /* Prevent any interrupts */
    hal_bitserial_int_disable(i);

    /* Teardown the instance */
    hal_bitserial_event_clear(i, BITSERIAL_EVENT_ALL_MASK);
    hal_bitserial_config_set(i, 0);
    hal_bitserial_config2_set(i, 0);

    /* Destroy the queue */
    while(!utils_SLL_isEmpty(&(instance[i]->action_queue)))
    {
        action_queue_advance(i);
    }

    /* Dereference the instance - the data is owned by the open/close code */
    instance[i] = NULL;

    /* Only de-register the swirq if we were the only instance left */
    for (j = 0; j< HAVE_NUMBER_OF_BITSERIALS; j++)
    {
        if (instance[j])
        {
            return;
        }
    }

    hal_bitserial_swint_disable();

    return;
}


/******************************************************************************
 *
 * bitserial_add_config
 *
 * Given a list of parameters, creates a config action and queues it.
 */
bool bitserial_add_config(bitserial_handle handle,
                          bitserial_action_type action,
                          uint16 value,
                          bitserial_action_flags flags)
{
    bitserial_action *new_action;
    bitserial_instance i;

    i = BITSERIAL_HANDLE_TO_INSTANCE(handle);

    /* Check the instance is valid/active on this processor */
    if ((!hal_bitserial_instance_is_valid(i)) ||
        (instance[i]->handle != handle) ||
        !BITSERIAL_HANDLE_ON_P1(instance[i]->handle)
       )
    {
        return FALSE;
    }

    /* Check the config type is valid and sane */
    switch (action)
    {
        case BITSERIAL_ACTION_TYPE_CONFIG_I2CADDRESS:
            if (value > 0x7f)
            {
                return FALSE; /* Only 7-bit addresses supported */   
            }
            break;

        case BITSERIAL_ACTION_TYPE_CONFIG_SPEED:
            /* No sanity check here.  Speed will be adjusted in config. */
            break;

        default:
            /* We can't queue something we don't know about */
            return FALSE;
    }

    new_action = pmalloc(sizeof(bitserial_action));
    new_action->type = action;
    new_action->flags = flags | BITSERIAL_ACTION_FLAG_DYNAMIC;
    new_action->u.config.value = value;

    BITSERIAL_L5_DBG_MSG2("BITSERIAL%d: add_config() - Add action to the queue (value=0x%04x)", i, value);

    if (!action_queue_add(i, new_action))
    {
        BITSERIAL_L5_DBG_MSG("BITSERIAL: Adding config failed");
        return FALSE;
    }
    BITSERIAL_L5_DBG_MSG1("BITSERIAL%d: add_config() complete", i);
    return TRUE;
}


/******************************************************************************
 *
 * bitserial_add_transfer
 *
 * Given a list of parameters, creates a transfer action and queues it.
 *
 * The transfer details (pointers and size) must be valid.
 */
bool bitserial_add_transfer(bitserial_handle handle,
                                   bitserial_transfer_handle *xf_hdl,
                                   const uint8 *tx_data,
                                   uint16 tx_size,
                                   uint8 *rx_data,
                                   uint16 rx_size,
                                   bitserial_action_flags flags,
                                   bitserial_done_cb_fn done_fn)
{
    bitserial_action *new_action;
    bitserial_instance i;

    i = BITSERIAL_HANDLE_TO_INSTANCE(handle);

    BITSERIAL_L4_DBG_MSG4("BITSERIAL%d: Adding a transfer action to the queue (TX=%d, RX=%d) flags=0x%04x", i, tx_size, rx_size, flags);

    /* Check the instance is valid/active on this processor */
    if ((!hal_bitserial_instance_is_valid(i)) ||
        (instance[i]->handle != handle) ||
        !BITSERIAL_HANDLE_ON_P1(instance[i]->handle)
       )
    {
        return FALSE;
    }

    /* Check there's something to do */
    if ((tx_size == 0) && (rx_size == 0))
    {
        BITSERIAL_L5_DBG_MSG("BITSERIAL: Adding transfer failed - nothing to do");
        return FALSE;
    }

    /* Check the transfer is a multiple of bytes_per_word */
    if ( (tx_size % instance[i]->bytes_per_word) ||
         (rx_size % instance[i]->bytes_per_word))
    {
        BITSERIAL_L5_DBG_MSG("BITSERIAL: Adding transfer failed - not a multiple of b/w");
        return FALSE;
    }

    /* Can't use start/stop bits if 4 bytes/word */
    if ((instance[i]->bytes_per_word == 4) &&
        (flags & (BITSERIAL_ACTION_TRANSFER_START_BIT_EN | BITSERIAL_ACTION_TRANSFER_STOP_BIT_EN)))
    {
        BITSERIAL_L5_DBG_MSG("BITSERIAL: Adding transfer failed - bad start/stop");
        return FALSE;
    }

    new_action = pmalloc(sizeof(bitserial_action));

    /* Can't have a transfer complete call if it's blocking */
    if (flags & BITSERIAL_ACTION_FLAG_BLOCKING)
    {
        xf_hdl = NULL;
    }

    new_action->u.transfer.tf_handle = xf_hdl;
    new_action->u.transfer.tx_data = tx_data;
    new_action->u.transfer.rx_data = rx_data;
    new_action->u.transfer.tx_len = tx_size;
    new_action->u.transfer.rx_len = rx_size;
    new_action->done_cb = done_fn;
    new_action->type = BITSERIAL_ACTION_TYPE_TRANSFER;
    new_action->flags = flags | BITSERIAL_ACTION_FLAG_DYNAMIC;

    if (!action_queue_add(i, new_action))
    {
        BITSERIAL_L5_DBG_MSG("BITSERIAL: Adding transfer to queue failed");
        return FALSE;
    }
    BITSERIAL_L5_DBG_MSG1("BITSERIAL%d: add_transfer() complete", i);
    return TRUE;
}


/******************************************************************************
 *
 * Everything below is static to this file
 *
 *****************************************************************************/


/******************************************************************************
 *
 * action_queue_add
 *
 * Add an incoming action on the right queue for the hardware instance,
 * and kick the transfer engine.
 *
 * The incoming action must have been pmalloced, and we will pfree it when it
 * has been dealt with.
 */
static bool action_queue_add(bitserial_instance i, bitserial_action *action)
{
    bool q_was_empty;

    q_was_empty = utils_SLL_isEmpty(&(instance[i]->action_queue));

    if (!q_was_empty && (action->flags & BITSERIAL_ACTION_FLAG_BLOCKING))
    {
        pfree(action);
        L2_DBG_MSG1("BITSERIAL%d: ERROR - can not queue a blocking action", i);
        return FALSE; /* We can't queue something that is requested to block! */
    }

    /* Add it to the tail of the queue for the given instance */
    block_interrupts();
    utils_SLL_append(&instance[i]->action_queue, (utils_SLLMember *)action);
    unblock_interrupts();

    /* If the queue was empty before adding this entry, then there's no active
     * hardware activity. Which means the new transfer needs to be started
     * manually (otherwise it'd be started by the previous one finishing).
     */
    if (q_was_empty)
    {
        BITSERIAL_L5_DBG_MSG1("BITSERIAL%d: action_queue_add() queue was empty.",i);
        action_engine_run(i);
    }
    return TRUE;
}


/******************************************************************************
 *
 * action_queue_advance
 *
 * Remove the head item from the queue, cleaning up memory if needed
 */
static void action_queue_advance(bitserial_instance i)
{
    bitserial_action *completed;

    completed = (bitserial_action *)(void *)utils_SLL_removeHead(&instance[i]->action_queue);
    if (completed &&
        (completed->flags & BITSERIAL_ACTION_FLAG_DYNAMIC))
    {
        pfree(completed);
    }
}


/******************************************************************************
 *
 * action_engine_run
 *
 * MUST BE RE-ENTRANT ACROSS INSTANCES
 */
static void action_engine_run(bitserial_instance i)
{
    uint8   l;

    /* Safety check */
    if (instance[i]->active_op.flags != BITSERIAL_ACTION_FLAG_IDLE)
    {
        /* There's already an op in progress */
        BITSERIAL_L5_DBG_MSG1("BITSERIAL%d: Can not run action_engine", i);
        panic_diatribe(PANIC_BITSERIAL_OP_ERROR, i);
    }

    /* Loop over queue, submitting actions until we get to one that we can't deal
     * with immediately.
     */
    while (!utils_SLL_isEmpty(&instance[i]->action_queue))
    {
        bitserial_action *new_action;

        /* Get the head of the queue */
        new_action = (bitserial_action *)(void *)utils_SLL_head(&instance[i]->action_queue);

        /* Ensure instance is on and disable deep sleep */
        hal_bitserial_clock_enable_set(i, 1);
        BITSERIAL_DISABLE_SLEEP();

        if (!action_submit(i, new_action))
        {
            /* Action has been submitted, but is still in progress so
             * just return - there will be an interrupt on completion.
             */
            BITSERIAL_L5_DBG_MSG1("BITSERIAL%d: action_submit sent to hardware", i);
            return;
        }
        action_queue_advance(i);
    }

    /* Queue emptied - turn instance off and see if we can deep sleep */
    BITSERIAL_L5_DBG_MSG1("BITSERIAL%d: action_engine_run(), queue empty", i);

    hal_bitserial_clock_enable_set(i, 0);

    /* Deep sleep only if _all_ instances are idle */
    for (l = 0; l < HAVE_NUMBER_OF_BITSERIALS; l++)
    {
        if ((instance[l] != NULL) &&
            (instance[l]->active_op.flags != BITSERIAL_ACTION_FLAG_IDLE))
        {
            /* Not all instances are idle, so don't allow deep sleep */
            return;
        }
    }
    BITSERIAL_ENABLE_SLEEP();
}


/******************************************************************************
 *
 * action_engine_complete
 *
 * Callback function for an action item that did a data transfer.
 * We are still the 'active_op' on the given instance.
 *
 * MUST BE RE-ENTRANT ACROSS INSTANCES
 */
static void action_engine_complete(bitserial_instance i, bitserial_result result)
{
    bitserial_action *current_transfer;
    const uint8 *buf_addr;
    bitserial_done_cb_fn done_cb;
    bitserial_transfer_handle *tf_handle;
    bool blocking;

    BITSERIAL_L5_DBG_MSG2("BITSERIAL%d: action_engine_complete entered (%d)", i, result);
    /* Cancel any timeout pending */
    if (instance[i]->active_op.timeout_tid != NO_TID)
    {
        timer_cancel_event(instance[i]->active_op.timeout_tid);
        instance[i]->active_op.timeout_tid = NO_TID;
    }

    /* Point at the current transfer - the head of the queue */
    current_transfer = (bitserial_action *)(void *)utils_SLL_head(&instance[i]->action_queue);

    if (!current_transfer)
    {
        /* We've got here despite there being no transfer active */
        L0_DBG_MSG2("BITSERIAL%d: Completing a non-existent transfer (%d)", i, result);
        panic_diatribe(PANIC_BITSERIAL_OP_ERROR, i);
    }

    /* If we received any data, then copy it from the rx buffer to the dest */
    if ((result == BITSERIAL_RESULT_SUCCESS) && instance[i]->active_op.rx_len)
    {
        /* Copy the received data to the destination */
        buf_addr = buf_raw_read_map_8bit(instance[i]->rx_buffer);
        memcpy(current_transfer->u.transfer.rx_data, buf_addr, instance[i]->active_op.rx_len);
        buf_read_port_close();
        buf_raw_write_update(instance[i]->rx_buffer, instance[i]->active_op.rx_len);
        buf_raw_read_update(instance[i]->rx_buffer, instance[i]->active_op.rx_len);
        buf_raw_update_tail_free(instance[i]->rx_buffer, instance[i]->rx_buffer->outdex);
    }

    /* We need to move on past transmitted data in the buffer too */
    if ((result == BITSERIAL_RESULT_SUCCESS) && instance[i]->active_op.tx_len)
    {
        buf_raw_read_update(instance[i]->tx_buffer, instance[i]->active_op.tx_len);
        buf_raw_update_tail_free(instance[i]->tx_buffer, instance[i]->tx_buffer->outdex);
    }

    /* Update the current transfer if the completed operation doesn't complete it */
    if ((result == BITSERIAL_RESULT_SUCCESS) &&
        ((instance[i]->active_op.tx_len != current_transfer->u.transfer.tx_len) ||
        (instance[i]->active_op.rx_len != current_transfer->u.transfer.rx_len)))
    {
        /* Data was transferred, but there's more to do yet */

        /* Modify the transfer queue head element so that it can be treated
         * as a 'new' transfer. I don't _think_ we need to give it a clue that
         * it's a continuation. 
         */
        current_transfer->u.transfer.tx_data += instance[i]->active_op.tx_len;
        current_transfer->u.transfer.tx_len -= instance[i]->active_op.tx_len;
        current_transfer->u.transfer.rx_data += instance[i]->active_op.rx_len;
        current_transfer->u.transfer.rx_len -= instance[i]->active_op.rx_len;

        /* The modified queue head element gets reprocessed in action_engine_run() shortly... */
        instance[i]->active_op.flags = BITSERIAL_ACTION_FLAG_IDLE;
        return;
    }

    if (result != BITSERIAL_RESULT_SUCCESS)
    {
        /* The operation failed, which means the transfer as a whole failed.
         * We need to correct the tx and/or rx buffer indices to match the
         * hardware ones - as we can't manipulate the hardware ones from P1.
         */
        uint16 rxed, txed;
        uint16 word_count = hal_bitserial_words_sent_get(i);

        switch (result)
        {
            case BITSERIAL_RESULT_I2C_NACK:
                /* The hardware may have aborted, depending on the i2c flags */
                if (instance[i]->active_op.rx_len)
                {
                    /* Write or Read/Write op */
                    rxed = word_count;
                    txed = instance[i]->active_op.tx_len;
                }
                else
                {
                    /* Write-only op */
                    rxed = 0;
                    txed = instance[i]->active_op.tx_len;
                }

                if ((hal_bitserial_act_on_nak_get(i) == BITSERIAL_ACT_ON_NAK_STOP) &&
                    instance[i]->active_op.tx_len)
                {
                    /* There was a TX and the hardware is configured to abort,
                     * so adjust the software pointers to compensate - we can't
                     * read the hardware pointer from P1.
                     */
                    uint16 windback = 1;
                    if (instance[i]->active_op.tx_len > 1)
                    {
                        windback = 2; /* The h/w consumes 2 bytes (tx one, pre-read one) if there are more than 2 pending. */
                    }
                    buf_raw_write_update(instance[i]->tx_buffer, BUF_GET_SIZE_OCTETS(instance[i]->tx_buffer)-(instance[i]->active_op.tx_len - windback));
                    txed = windback;
                }
                break;

            case BITSERIAL_RESULT_I2C_ARBITRATION:
                /* I2C lost arbitration, so we only transmitted before the
                 * hardware aborted.
                */
                rxed = 0;
                txed = word_count;
                break;

            default:
                /* Other errors are assumed to complete, but they're fatal */
                rxed = instance[i]->active_op.rx_len;
                txed = instance[i]->active_op.tx_len;
                break;
        }

        if (txed)
        {
            buf_raw_read_update(instance[i]->tx_buffer, txed);
            buf_raw_update_tail_free(instance[i]->tx_buffer, instance[i]->tx_buffer->outdex);
        }
        if (rxed)
        {
            buf_raw_write_update(instance[i]->rx_buffer, rxed);
            buf_raw_read_update(instance[i]->rx_buffer, rxed);
            buf_raw_update_tail_free(instance[i]->rx_buffer, instance[i]->rx_buffer->outdex);
        }

        /* Make sure that CS is deasserted */
        hal_bitserial_set_sel_inactive_set(i, 1);
        hal_bitserial_set_sel_inactive_set(i, 0);
    }
 
    /* As the action is now complete, remove it from the head of the queue, after
     * taking copies of what we still need. */
    done_cb = current_transfer->done_cb;
    tf_handle = current_transfer->u.transfer.tf_handle;
    blocking = (current_transfer->flags & BITSERIAL_ACTION_FLAG_BLOCKING)?TRUE:FALSE;
    action_queue_advance(i);

    /* Call the 'done' callback, which must also be re-entrant */
    if (done_cb != NULL)
    {
        BITSERIAL_L5_DBG_MSG4("BITSERIAL%d: done_cb(%d,%d,%d) called",i, tf_handle, blocking, result);
        done_cb(tf_handle, blocking, result);
    }

    /* Mark the hardware as idle (for now) */
    instance[i]->active_op.flags = BITSERIAL_ACTION_FLAG_IDLE;

    /* After this, action_engine_run() gets called, which will look to see if
     * there's anything else on the action queue.
     */
    BITSERIAL_L5_DBG_MSG1("BITSERIAL%d: action_engine_complete exited", i);
}


/******************************************************************************
 *
 * action_submit
 *
 * Submits the transfer (or a subset of it) into the hardware.
 *
 * Return: TRUE if executed, FALSE if waiting on hardware to complete.
 *
 * NOTE: Any submitted transfer _must_ have tx_len+rx_len != 0
 */
static bool action_submit(bitserial_instance i, bitserial_action *new_action)
{
    bitserial_action_flags op_flags;
    uint16 buf_free;
    INTERVAL timeout = 0;
    uint16 tx_len, rx_len;
    uint16 value;
    uint8 *buf_addr;
    bitserial_mode bs_mode;

    if (hal_bitserial_config_get(i) & HAL_BITSERIAL_CONFIG_I2C_MODE_EN_MASK)
    {
        bs_mode = BITSERIAL_MODE_I2C_MASTER;
    }
    else
    {
        bs_mode = BITSERIAL_MODE_SPI_MASTER;
    }

    /* Clear the idle flag, as we're now in use */
    instance[i]->active_op.flags = BITSERIAL_ACTION_FLAGS_NONE;
    
    /* The action might be immediate (eg address change) or may take some time
     * after which an interrupt will be generated. In the former case, we
     * consume it here and return to do the next one
     */
    switch (new_action->type)
    {
        case BITSERIAL_ACTION_TYPE_CONFIG_SPEED:
            /* Change the speed in the hardware config immediately */
            value = new_action->u.config.value;
            bitserial_config_clock_freq_set(
                i, bs_mode, value, TRUE /* freq_change */, &instance[i]->byte_time_ns
            );
            instance[i]->active_op.flags = BITSERIAL_ACTION_FLAG_IDLE;
            return TRUE;

        case BITSERIAL_ACTION_TYPE_CONFIG_I2CADDRESS:
            /* Change the I2C address in the hardware config immediately */
            value = new_action->u.config.value;
            if (value > 0x7f)
            {
                BITSERIAL_L5_DBG_MSG2("BITSERIAL%d: Config I2C address out of range (0x%02x)", i, value);
                panic_diatribe(PANIC_BITSERIAL_OP_ERROR, i);
            }
            BITSERIAL_L5_DBG_MSG2("BITSERIAL%d: I2C address set to 0x%02x", i, value);
            hal_bitserial_i2c_address_set(i, value);
            instance[i]->active_op.flags = BITSERIAL_ACTION_FLAG_IDLE;
            return TRUE;

        case BITSERIAL_ACTION_TYPE_TRANSFER:
            if (new_action->u.transfer.tf_handle)
            {
                /* Put a transfer id into the caller-supplied location. This id will
                 * be used to identify this transfer in the message it sends on
                 * completion. It consists of an 8-bit rolling counter and the instance.
                 */
                if (instance[i]->rolling_handle == BITSERIAL_TRANSFER_HANDLE_NONE)
                {
                    instance[i]->rolling_handle++;
                }
                *(new_action->u.transfer.tf_handle) = instance[i]->rolling_handle++ | (uint16)(i<<8); 
            }

            /* Pre-load the op flags from the transfer */
            BITSERIAL_L5_DBG_MSG2("BITSERIAL%d: Loading flags as 0x%04x",i,new_action->flags);
            op_flags = new_action->flags;
        
            /* Analyse the transmit and receive data, setting flags and copying
             * (part of) the data as required.
             */
            tx_len = new_action->u.transfer.tx_len;
            rx_len = new_action->u.transfer.rx_len;

            if (tx_len != 0)
            {
                volatile BUFFER *buff = instance[i]->tx_buffer;

                /* Wait for buf_raw_update_tail_free to finish before getting freespace.
                 * On P1 buf_raw_update_tail_free is a non-blocking IPC to P0.
                 */
                while (buff->tail != buff->outdex){}
                buf_free = (uint16)BUF_GET_FREESPACE(buff);
                if (tx_len > buf_free)
                {
                    tx_len = buf_free;

                    /* Flag that the op is just a partial tx - the rest of the tx
                     * and any rx will be done in a later op.
                     */
                    op_flags |= BITSERIAL_ACTION_TRANSFER_STOP_TOKEN_DISABLE;
                    rx_len = 0;
                }
                /* Write the transmit data into the tx buffer */
                buf_addr = buf_raw_write_only_map_8bit(instance[i]->tx_buffer);
                memcpy(buf_addr, new_action->u.transfer.tx_data, tx_len);
                buf_write_port_close();
                buf_raw_write_update(instance[i]->tx_buffer, tx_len);
                /* The data pointer and size are updated on transfer completion */
            }

            if (rx_len != 0)
            {
                volatile BUFFER *buff = instance[i]->rx_buffer;

                /* Wait for buf_raw_update_tail_free to finish before getting freespace.
                 * On P1 buf_raw_update_tail_free is a non-blocking IPC to P0.
                 */
                while (buff->tail != buff->outdex){}
                /* Only have the op read as much as there is space in the buffer */
                buf_free = (uint16)BUF_GET_FREESPACE(buff);
                if (rx_len > buf_free)
                {
                    rx_len = buf_free;

                    /* Flag that the op is a partial rx - the rest of the rx
                     * will be done in a later op.
                     */
                    op_flags |= BITSERIAL_ACTION_TRANSFER_STOP_TOKEN_DISABLE;
#ifdef BITSERIAL_COMBO_STOP_TOKEN_DISABLE2_WA
                    /* Avoid a tx/rx pair if rx is going to get split. Make this
                     * op just a partial tx, and the rx will happen in a later op.
                     */
                    if (tx_len != 0)
                    {
                        rx_len = 0;
                    }
#endif
                }

                if (tx_len != 0)
                {
                    /* It's a txrx or rxtx pair. We need to disable the stop token
                     * between the two operations.
                     */
                    op_flags |= BITSERIAL_ACTION_TRANSFER_STOP_TOKEN_DISABLE;
                }
            }
            /* Write the tx and rx lengths into the op we're creating. */
            instance[i]->active_op.tx_len = tx_len;
            instance[i]->active_op.rx_len = rx_len;

            /* Calculate the timeout required, based on the tx and rx lengths.
             * We assume the worst case of I2C and 10-bit address, so add 4 bytes
             * to cover the tx and rx addresses. Add another 1 to (generously)
             * cover the pre/post and inter tx+rx gaps. Finally, add a
             * 'safety margin' and round it up to the nearest millisecond.
             */
            timeout = (INTERVAL)((rx_len + tx_len + 4 + 1) * instance[i]->byte_time_ns);
            timeout += timeout/8; /* 12.5% safety margin */
            timeout = (timeout/1000)+1; /* Units have changed from ns to us here! */

            /* Now add the API-given timeout or setup a fallback panic */
            if (instance[i]->api_timeout_ms != 0)
            {
                timeout += instance[i]->api_timeout_ms * MILLISECOND;
            }
            else
            {
                timeout += PANIC_ON_TIMEOUT_MS * MILLISECOND;
                /* The callback knows that instance[].api_timeout_ms==0 means panic */
            }

            /* Everything else is done, copy in the flags */
            instance[i]->active_op.flags = op_flags;

            /* Start the timeout before the transfer in case it's incredibly quick */
            instance[i]->active_op.timeout_tid = timer_schedule_event_in(timeout,
                                                                         transfer_timeout,
                                                                         (void *)i);

            /* Program the bitserial hardware to start the transfer */
            active_op_program(i);

            /* The hardware is now running the op, control transfers to the irq
             * handler on completion.
             *
             * If we were to 'truly' block, we could poll the status register here
             * rather than enable interrupts. However, given that we have a
             * callback function defined when complete, it is easiest for the
             * bitserial code itself NOT to block - that behaviour can be implemented
             * in the calling layer by having the cb update a volatile global to
             * unblock - see the production_test_i2c code for an example.
             */
            break;

        default:
            /* Whatever it was, say we did it */
            BITSERIAL_L5_DBG_MSG2("BITSERIAL%d: action_submit() - action %d unknown", i, new_action->type);
            instance[i]->active_op.flags = BITSERIAL_ACTION_FLAG_IDLE;
            return TRUE; 
    }
    return FALSE;
}


/******************************************************************************
 *
 * action_engine_irq
 *
 * Deal with an interrupt from the hardware.
 */
static void action_engine_irq(bitserial_instance i)
{
    bitserial_events events;
    bitserial_result op_result = BITSERIAL_RESULT_SUCCESS;
    uint16 status;

    /* A transfer operation has completed, either good or bad */
    events = hal_bitserial_events_get(i);

    /* The old code had problems with spurious interrupts at some point - so we'll
     * persist with dealing with them for now, even though it looks like they were
     * probably caused by a variant of the early interrupt problem.
     */
    if (!events)
    {
        BITSERIAL_L5_DBG_MSG1("BITSERIAL%d: Spurious interrupt", i);
        return;
    }

    /* Check to see if we're in a race with the timeout. Under normal conditions,
     * the timeout will be active. If it's not, then it's fired and the timeout
     * handler has started. In which case we lost the race, tidy up and exit.
     */
    if (instance[i]->active_op.timeout_tid == NO_TID)
    {
        hal_bitserial_event_clear(i, events);
        return;
    }

    status = hal_bitserial_status_get(i);
    /* Ensure that the bitserial device isn't still 'busy' - if it is, then
     * it's not quite finished, and we'll set a timer to come back later.
     * (B-293134)
     */
    if (status & BITSERIAL_BUSY_MASK)
    {
        BITSERIAL_L5_DBG_MSG3("BITSERIAL%d: Interrupt while BUSY. Event 0x%04x, Status 0x%04x", i, events, status);
        /* Note that we don't clear the interrupt source - this is so we can
         * treat it just like it was a 'real' interrupt on timer expiry.
         */
#ifdef BITSERIAL_EARLY_INTERRUPT_WORKAROUND
        instance[i]->active_op.check_tid = timer_schedule_event_in(MILLISECOND, transfer_check, (void *)i);
#endif
        return;
    }

    /* We're here to stay, so clear the events we'll now deal with */
    hal_bitserial_event_clear(i, events);

    /* Check for 'bad' events quickly, before diving into details */
    if (BITSERIAL_EVENT_IS_FAIL(events))
    {
        /* I2C arbitration lost */
        if (events & BITSERIAL_EVENT_I2C_LOST_ARB_MASK)
        {
            op_result = BITSERIAL_RESULT_I2C_ARBITRATION;
        }
        /* I2C NACK - see B-214852 */
        if (events & (BITSERIAL_EVENT_I2C_ACK_ERROR_MASK |
                      BITSERIAL_EVENT_I2C_NAK_STOP_MASK |
                      BITSERIAL_EVENT_I2C_NAK_RESTART_MASK))
        {
            op_result = BITSERIAL_RESULT_I2C_NACK;
        }
        /* Buffer error */
        if (events & (BITSERIAL_EVENT_ERROR_IN_BUFFER_MASK |
                      BITSERIAL_EVENT_ERROR_IN_READ_MASK))
        {
            BITSERIAL_L5_DBG_MSG2("BITSERIAL%d: MMU event error. Status 0x%04x", i, status);
            /* Workaround for B-241466 */
            hal_bitserial_event_clear(i, events & (BITSERIAL_EVENT_ERROR_IN_BUFFER_MASK |
                                                   BITSERIAL_EVENT_ERROR_IN_READ_MASK));
            op_result = BITSERIAL_RESULT_MMU;
        }
        /* Slave mode errors */
        if (events & (BITSERIAL_EVENT_SLAVE_START_STOP_BITS_ERROR_MASK |
                      BITSERIAL_EVENT_FIFO_UNDERFLOW_MASK |
                      BITSERIAL_EVENT_FIFO_OVERFLOW_MASK |
                      BITSERIAL_EVENT_TX_NOT_CONFIGURED_MASK |
                      BITSERIAL_EVENT_RX_CMD_DETECTED_MASK |
                      BITSERIAL_EVENT_TX_STREAMING_SUCCESS))
        {
            BITSERIAL_L5_DBG_MSG2("BITSERIAL%d: Slave event error. Status 0x%04x", i, status);
            op_result = BITSERIAL_RESULT_INVAL;
        }
    }

    /* Raise a software interrupt to do the complete/run calls so that they're
     * outside of interrupt context.
     */
    instance[i]->active_op.flags |= BITSERIAL_ACTION_FLAG_COMPLETE;
    instance[i]->active_op.result = op_result;
    hal_bitserial_raise_swint();
}


/******************************************************************************
 *
 * transfer_timeout
 *
 * Timeout handler for the soft-timeout expiring on an active op
 *
 * i is the bitserial instance.
 */
static void transfer_timeout(void *dptr)
{
    uint16 i = (uint16)((uint32)dptr & 0xffff);

    /* Check (with interrupts blocked) whether we've raced against the
     * completion interrupt by seeing whether it has cleared the timeout tid.
     * If it hasn't then we clear it to 'win' the race. There is a similar
     * check in action_engine_irq().
     */
    bitserial_block_int();
    if (instance[i]->active_op.timeout_tid == NO_TID)
    {
        /* We've lost the race, so just exit */
        bitserial_unblock_int();
        return;
    }
    /* Clear the TID, to show that we've won. */
    instance[i]->active_op.timeout_tid = NO_TID;
    bitserial_unblock_int();

    if (instance[i]->api_timeout_ms == 0)
    {
        /* There is no caller timeout set, so just panic */
        panic_diatribe(PANIC_BITSERIAL_TIMEOUT, i);
    }
    else
    {
#ifdef BITSERIAL_EARLY_INTERRUPT_WORKAROUND
        /* Kill any pending early interrupt timer */
        if (instance[i]->active_op.check_tid != NO_TID)
        {
            timer_cancel_event(instance[i]->active_op.check_tid);
            instance[i]->active_op.check_tid = NO_TID;
        }
#endif
        /* Process it as completed but timed-out */
        action_engine_complete((bitserial_instance)i, BITSERIAL_RESULT_TIMEOUT);
        /* Find the next thing to do */
        action_engine_run((bitserial_instance)i);
    }
}


#ifdef BITSERIAL_EARLY_INTERRUPT_WORKAROUND
/******************************************************************************
 *
 * transfer_check
 *
 * Handler for the early interrupt workaround.
 */
static void transfer_check(void *dptr)
{
    uint16 i = (uint16)((uint32)dptr & 0xffff);

    /* Check the timeout TID - if it's unset, then we shouldn't be here */
    if (instance[i]->active_op.timeout_tid == NO_TID)
    {
        return;
    }

    /* Check the busy flag - it was set when this timer was kicked off - if
     * it is still set, go round again...
     */
    if (hal_bitserial_status_get((bitserial_instance)i) & BITSERIAL_BUSY_MASK)
    {
        instance[(bitserial_instance)i]->active_op.check_tid = timer_schedule_event_in(MILLISECOND, transfer_check, (void *)i);
    }
    else
    {
        /* Busy flag has cleared - so call the IRQ handler to 'fake' the
         * original interrupt. Note that we've blocked any genuine
         * interrupts from occuring.
         */
        BITSERIAL_L5_DBG_MSG1("BITSERIAL: Device %d - early interrupt workround triggered.", i);
        instance[i]->active_op.check_tid = NO_TID;
        action_engine_irq((bitserial_instance)i);
    }

}
#endif


/******************************************************************************
 *
 * active_op_program
 *
 * Program the active_op into the hardware and run it
 */
static void active_op_program(bitserial_instance i)
{
    uint16 tmpreg;
    uint8  bytes_per_word;
    uint16 first_len, second_len;
    bitserial_rw_modes rw_mode;
    uint16 prot_words = 0;
    bool slave_mode, combo_mode;

    combo_mode = (instance[i]->active_op.tx_len != 0) && (instance[i]->active_op.rx_len != 0);

    BITSERIAL_L4_DBG_MSG2("BITSERIAL%d: active_op_program - flags=0x%04x",i,instance[i]->active_op.flags);

    /* BITSERIALn_CONFIG2 */
    tmpreg = hal_bitserial_config2_get(i) &
             ~(HAL_BITSERIAL_CONFIG2_COMBO_MODE_MASK |
               HAL_BITSERIAL_CONFIG2_STOP_TOKEN_DISABLE2_MASK);
    slave_mode = tmpreg & HAL_BITSERIAL_CONFIG2_SLAVE_MODE_MASK; /* Need this later */
    tmpreg |= (HAL_BITSERIAL_CONFIG2_SLAVE_ANY_CMD_BYTE_EN_MASK |
              (combo_mode?HAL_BITSERIAL_CONFIG2_COMBO_MODE_MASK : 0));
    hal_bitserial_config2_set(i, tmpreg);

    /* BITSERIALn_CONFIG */
    tmpreg = hal_bitserial_config_get(i) &
             ~HAL_BITSERIAL_CONFIG_STOP_TOKEN_DISABLE_MASK;
    tmpreg |= (instance[i]->active_op.flags & BITSERIAL_ACTION_TRANSFER_STOP_TOKEN_DISABLE)?
                  HAL_BITSERIAL_CONFIG_STOP_TOKEN_DISABLE_MASK:0;
    hal_bitserial_config_set(i, tmpreg);

    /* BITSERIALn_WORD_CONFIG */
    bytes_per_word = instance[i]->bytes_per_word;
    tmpreg = (uint16)(((bytes_per_word - 1) << HAL_BITSERIAL_WORD_CONFIG_NUM_BYTES_POSN) |
              ((instance[i]->active_op.flags & BITSERIAL_ACTION_TRANSFER_START_BIT_EN)?
                    1<< HAL_BITSERIAL_WORD_CONFIG_NUM_START_POS:0) |
              ((instance[i]->active_op.flags & BITSERIAL_ACTION_TRANSFER_START_BIT_1)?
                    1<< HAL_BITSERIAL_WORD_CONFIG_START_BITS_POS:0) |
              ((instance[i]->active_op.flags & BITSERIAL_ACTION_TRANSFER_STOP_BIT_EN)?
                    1<< HAL_BITSERIAL_WORD_CONFIG_NUM_STOP_POS:0) |
              ((instance[i]->active_op.flags & BITSERIAL_ACTION_TRANSFER_STOP_BIT_1)?
                    1<< HAL_BITSERIAL_WORD_CONFIG_STOP_BITS_POS:0));
    hal_bitserial_word_config_set(i, tmpreg);

    /* Clear events/errors */
    tmpreg = hal_bitserial_events_get(i) &
             ~(BITSERIAL_EVENT_FIFO_OVERFLOW_MASK |
               BITSERIAL_EVENT_FIFO_UNDERFLOW_MASK);
    hal_bitserial_event_clear(i, tmpreg | BITSERIAL_EVENT_RX_SUCCESS_MASK); /* wa for B-216380 */
    hal_bitserial_clear_sticky_ack(i);

    /* Set up the tx and rx transfers */
    if (combo_mode)
    {
        /* TX and RX - could be TXRX or RXTX */
        if (instance[i]->active_op.flags & BITSERIAL_ACTION_TRANSFER_RXFIRST)
        {
            first_len = instance[i]->active_op.rx_len;
            second_len = instance[i]->active_op.tx_len;
            rw_mode = BITSERIAL_READ_WRITE_MODE;
        }
        else
        {
            first_len = instance[i]->active_op.tx_len;
            second_len = instance[i]->active_op.rx_len;
            rw_mode = BITSERIAL_WRITE_READ_MODE;
        }
        if (slave_mode)
        {
            BITSERIAL_L5_DBG_MSG1("BITSERIAL%d: Combo slave mode not supported", i);
            panic_diatribe(PANIC_BITSERIAL_OP_ERROR, i); /* Hw doesn't support this */
        }
    }
    else
    {
        second_len = 0;

        if (instance[i]->active_op.tx_len != 0)
        {
            /* TX only */
            first_len = instance[i]->active_op.tx_len;
            rw_mode = BITSERIAL_WRITE_MODE;
        }
        else
        {
            /* RX only */
            first_len = instance[i]->active_op.rx_len;
            rw_mode = BITSERIAL_READ_MODE;
        }
        if (slave_mode)
        {
            if (rw_mode == BITSERIAL_READ_MODE)
            {
                prot_words = (uint16)((first_len/bytes_per_word) - 1); /* B-216380 */
            }
            /* In slave mode, read is write and write is read. Flip it using xor */
            rw_mode = rw_mode ^ BITSERIAL_READ_MODE;
        }
    }

    hal_bitserial_rwb_set(i, rw_mode);
    hal_bitserial_num_protocol_words_set(i, (uint16)prot_words);
    hal_bitserial_txrx_length2_set(i, (uint16)(second_len/bytes_per_word));

    /* Finally, set the first tx/rx length, which starts the transaction */
    hal_bitserial_txrx_length_set(i, (uint16)(first_len/bytes_per_word));
}


/******************************************************************************
 *
 * action_engine_process
 *
 * On completion of a hardware operation, tidies up and starts the next.
 * Called as a SWINT from the interrupt.
 */
static void action_engine_process(void)
{
    bitserial_instance i;

    /* Look at all active operations to see which triggered us */
    for (i = (bitserial_instance)0; i < HAVE_NUMBER_OF_BITSERIALS; i++)
    {
        if ((instance[i] != NULL) && (instance[i]->active_op.flags & BITSERIAL_ACTION_FLAG_COMPLETE))
        {
            /* There's a pending post-interrupt processing */
            action_engine_complete(i, instance[i]->active_op.result);
            action_engine_run(i);
        }
    }
}

#endif /* INSTALL_BITSERIAL */

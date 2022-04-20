/* Copyright (c) 2017 - 2021 Qualcomm Technologies International, Ltd. */
/*   %%version */
/**
 * \file
 * P1 bitserial API.
 * 
 * If P1 requests BITSERIAL_BLOCK_0, BITSERIAL_BLOCK_1 etc. in BitserialOpen
 * then subsequent bitserial operations (for that handle) will run on P0 via IPC.
 * If P1 requests P1_BITSERIAL_BLOCK_0, P1_BITSERIAL_BLOCK_1 etc. in BitserialOpen
 * then subsequent bitserial operations (for that handle) will run on P1.
 */

#include "ipc/ipc_prim.h"
#include "message/system_message.h"
#include "trap_api/bitserial_api.h"
#include "trap_api/message.h"
#include "trap_api/trap_api_private.h"

#if TRAPSET_BITSERIAL && !defined(DESKTOP_TEST_BUILD)
#include "bitserial/bitserial.h"
#include "bitserial/bitserial_private.h"
#include "hal/hal_bitserial.h"

/*****************************************************************************
 * Private function prototypes
 ****************************************************************************/

static bitserial_result trap_api_bitserial_transfer
(
    bitserial_handle bs_handle,
    bitserial_transfer_handle *transfer_handle_ptr,
    const uint8 *tx_data,
    uint16 tx_size,
    uint8 *rx_data,
    uint16 rx_size,
    bitserial_transfer_flags api_flags
);

static void trap_api_bitserial_transfer_done_all
(
    bitserial_instance i,
    bitserial_transfer_handle *tf_hdl,
    bool blocking,
    bitserial_result result
);

static void trap_api_bitserial_transfer_done_0
(
    bitserial_transfer_handle *tf_hdl,
    bool blocking,
    bitserial_result result
);

static void trap_api_bitserial_transfer_done_1
(
    bitserial_transfer_handle *tf_hdl,
    bool blocking,
    bitserial_result result
);

#if HAVE_NUMBER_OF_BITSERIALS > 2
static void trap_api_bitserial_transfer_done_2
(
    bitserial_transfer_handle *tf_hdl,
    bool blocking,
    bitserial_result result
);
#endif /* HAVE_NUMBER_OF_BITSERIALS > 2 */


/*****************************************************************************
 * Private data
 ****************************************************************************/

/* Has the current blocking transfer been done? */
static volatile bool bitserial_transfer_done[HAVE_NUMBER_OF_BITSERIALS];

/* We don't get the instance in the callback so we need a callback for each one. */
static bitserial_done_cb_fn done_cb_fn[HAVE_NUMBER_OF_BITSERIALS] = {
    trap_api_bitserial_transfer_done_0,
    trap_api_bitserial_transfer_done_1
#if HAVE_NUMBER_OF_BITSERIALS > 2
    , trap_api_bitserial_transfer_done_2
#endif /* HAVE_NUMBER_OF_BITSERIALS > 2 */
};


/******************************************************************************
 * Private function definitions
 *****************************************************************************/

/**
 * \brief Validate parameters and send to bitserial_add_transfer.
 *
 * \param bs_handle Handle returned from \c BitserialOpen.
 * \param transfer_handle_ptr Address to store the transfer handle.
 * \param tx_data Pointer to data to transmit or \c NULL if only receiving.
 * \param tx_size Size of data pointed to by \p tx_data.
 * \param rx_data Pointer to data store for receiving data or \c NULL if only
 *                transmitting.
 * \param rx_size Size of data pointed to by \p rx_data.
 * \param api_flags Transfer flags.
 * \return Result of the transfer.
 */
static bitserial_result trap_api_bitserial_transfer
(
    bitserial_handle bs_handle,
    bitserial_transfer_handle *transfer_handle_ptr,
    const uint8 *tx_data,
    uint16 tx_size,
    uint8 *rx_data,
    uint16 rx_size,
    bitserial_transfer_flags api_flags
)
{
    bitserial_action_flags bs_flags = BITSERIAL_ACTION_FLAGS_NONE;
    bitserial_instance i = BITSERIAL_HANDLE_TO_INSTANCE(bs_handle);

    if (!hal_bitserial_instance_is_valid(i))
    {
        return BITSERIAL_RESULT_INVAL;
    }

    /* Perform sanity checks on the parameters */
    if ((tx_data && !tx_size) || (!tx_data && tx_size) ||
        (rx_data && !rx_size) || (!rx_data && rx_size) ||
        (!rx_size && !tx_size))
    {
        return BITSERIAL_RESULT_INVAL;
    }

    if (!bitserial_trap_api_to_bs_flags(&bs_flags, api_flags))
    {
        /* Invalid flags */
        return BITSERIAL_RESULT_INVAL;
    }

    bitserial_transfer_done[i] = FALSE;
    L5_DBG_MSG3(
        "trap_api_bitserial_transfer: bs = 0x%02X, th = 0x%08X, bs_flags = 0x%08X",
        bs_handle, transfer_handle_ptr, bs_flags
    );
    /* Send to the bitserial code. */
    if (bitserial_add_transfer(
            bs_handle, transfer_handle_ptr,
            tx_data, tx_size,
            rx_data, rx_size,
            bs_flags,
            done_cb_fn[i]
        )
    )
    {
        /* If blocking and not done: spin.
         * Should not be doing this for long transfers (use non-blocking).
         */
        if (bs_flags & BITSERIAL_ACTION_FLAG_BLOCKING)
        {
            /* Wait for transfer complete callback to set the flag. */
            while(!bitserial_transfer_done[i])
            {
                /* Empty block. */
                /** \todo (maybe): if expected completion time > schedule tick
                 *  then taskYIELD()
                 *  or vTaskDelay(ms_to_tick(expected_completion_time_ms)).
                 */
            }
        }
        return BITSERIAL_RESULT_SUCCESS;
    }
    else
    {
        /* Bitserial code refused it */
        return BITSERIAL_RESULT_INVAL;
    }
}


/**
 * \brief Send transfer completion message to the requesting task.
 *        Called by the individual callbacks when the transfer has completed.
 *        Sets the flag used by a blocking call so that the task can be released.
 * 
 * \param i Physical bitserial instance.
 * \param tf_hdl Address of the transfer handle.
 * \param blocking \c TRUE if the original request was blocking.
 * \param result Result of the transfer.
 */
static void trap_api_bitserial_transfer_done_all
(
    bitserial_instance i,
    bitserial_transfer_handle *tf_hdl,
    bool blocking,
    bitserial_result result
)
{
    bitserial_transfer_done[i] = TRUE;

    L4_DBG_MSG4(
        "trap_api_bitserial_transfer_done_all "
        "instance = %d, *tf_hdl = %04X, blocking = %d, result = %d",
        i, tf_hdl ? *tf_hdl : 0, blocking, result
    );
    /* If the API caller requested notification, send it */
    if (!blocking && tf_hdl && (*tf_hdl != BITSERIAL_TRANSFER_HANDLE_NONE))
    {
        Task task = trap_api_lookup_message_task(IPC_MSG_TYPE_BITSERIAL);
        
        if (task)
        {
            MessageBitserialEvent *message = pnew(MessageBitserialEvent);

            message->transfer_handle = *tf_hdl;
            message->result = result;
            MessageSend(task, MESSAGE_BITSERIAL_EVENT, message);
        }
    }
}


/**
 * \brief Supplies trap_api_bitserial_transfer_done_all with the correct instance.
 *        Called by the transfer completed interrupt.
 * 
 * \param tf_hdl Address of the transfer handle.
 * \param blocking \c TRUE if the original request was blocking.
 * \param result Result of the transfer.
 */
static void trap_api_bitserial_transfer_done_0
(
    bitserial_transfer_handle *tf_hdl,
    bool blocking,
    bitserial_result result
)
{
    trap_api_bitserial_transfer_done_all((bitserial_instance)0, tf_hdl, blocking, result);
}


/**
 * \brief Supplies trap_api_bitserial_transfer_done_all with the correct instance.
 *        Called by the transfer completed interrupt.
 * 
 * \param tf_hdl Address of the transfer handle.
 * \param blocking \c TRUE if the original request was blocking.
 * \param result Result of the transfer.
 */
static void trap_api_bitserial_transfer_done_1
(
    bitserial_transfer_handle *tf_hdl,
    bool blocking,
    bitserial_result result
)
{
    trap_api_bitserial_transfer_done_all((bitserial_instance)1, tf_hdl, blocking, result);
}


#if HAVE_NUMBER_OF_BITSERIALS > 2
/**
 * \brief Supplies trap_api_bitserial_transfer_done_all with the correct instance.
 *        Called by the transfer completed interrupt.
 * 
 * \param tf_hdl Address of the transfer handle.
 * \param blocking \c TRUE if the original request was blocking.
 * \param result Result of the transfer.
 */
static void trap_api_bitserial_transfer_done_2
(
    bitserial_transfer_handle *tf_hdl,
    bool blocking,
    bitserial_result result
)
{
    trap_api_bitserial_transfer_done_all((bitserial_instance)2, tf_hdl, blocking, result);
}
#endif /* HAVE_NUMBER_OF_BITSERIALS > 2 */


/******************************************************************************
 * Public function definitions
 *****************************************************************************/

bitserial_result BitserialChangeParam
(
    bitserial_handle bs_handle,
    bitserial_changeable_params key,
    uint16 value,
    bitserial_transfer_flags api_flags
)
{
    if (BITSERIAL_HANDLE_ON_P1(bs_handle))
    {
        bitserial_action_type bs_action_type;
        bitserial_action_flags bs_flags;

        switch (key)
        {
            case BITSERIAL_PARAMS_CLOCK_FREQUENCY_KHZ:
                bs_action_type = BITSERIAL_ACTION_TYPE_CONFIG_SPEED;
                break;
            case BITSERIAL_PARAMS_I2C_DEVICE_ADDRESS:
                bs_action_type = BITSERIAL_ACTION_TYPE_CONFIG_I2CADDRESS;
                break;
            default:
                return BITSERIAL_RESULT_INVAL;
        }

        if (!bitserial_trap_api_to_bs_flags(&bs_flags, api_flags))
        {
            return BITSERIAL_RESULT_INVAL;
        }

        if (bitserial_add_config(bs_handle, bs_action_type, value, bs_flags))
        {
            return BITSERIAL_RESULT_SUCCESS;
        }
        else
        {
            /* Bitserial code refused it */
            return BITSERIAL_RESULT_INVAL;
        }
    }
    else
    {
        IPC_BITSERIAL_CHANGE_PARAM ipc_send_prim;
        IPC_BITSERIAL_RESULT_RSP ipc_recv_prim;

        ipc_send_prim.handle = bs_handle;
        ipc_send_prim.key = key;
        ipc_send_prim.value = value;
        ipc_send_prim.flags = api_flags;
        ipc_transaction(
            IPC_SIGNAL_ID_BITSERIAL_CHANGE_PARAM,
            &ipc_send_prim,
            sizeof(ipc_send_prim),
            IPC_SIGNAL_ID_BITSERIAL_CHANGE_PARAM_RSP,
            &ipc_recv_prim
        );
        return ipc_recv_prim.ret;
    }
}


void BitserialClose(bitserial_handle bs_handle)
{
    IPC_BITSERIAL_CLOSE ipc_send_prim;
    IPC_VOID_RSP ipc_recv_prim;

    ipc_send_prim.handle = bs_handle;
    ipc_transaction(
        IPC_SIGNAL_ID_BITSERIAL_CLOSE,
        &ipc_send_prim,
        sizeof(ipc_send_prim),
        IPC_SIGNAL_ID_BITSERIAL_CLOSE_RSP,
        &ipc_recv_prim
    );
}


bitserial_handle BitserialOpen(bitserial_block_index block_index, const bitserial_config *config)
{
    bitserial_instance bs_instance = BITSERIAL_BLOCK_INDEX_TO_INSTANCE(block_index);
    bitserial_handle bs_handle;
    IPC_BITSERIAL_OPEN ipc_send_prim;
    IPC_BITSERIAL_HANDLE_RSP ipc_recv_prim;

    if (!hal_bitserial_instance_is_valid(bs_instance))
    {
        return BITSERIAL_HANDLE_ERROR;
    }
    ipc_send_prim.block_index = block_index;
    ipc_send_prim.config = config;
    ipc_transaction(
        IPC_SIGNAL_ID_BITSERIAL_OPEN,
        &ipc_send_prim,
        sizeof(ipc_send_prim),
        IPC_SIGNAL_ID_BITSERIAL_OPEN_RSP,
        &ipc_recv_prim
    );
    bs_handle = ipc_recv_prim.ret;

    if ((bs_handle != BITSERIAL_HANDLE_ERROR) && BITSERIAL_HANDLE_ON_P1(bs_handle))
    {
        /* Get the bitserial_hw address from P0 and pass it to bitserial_action_init. */
        bitserial_hw *bs_hw;

        /* Using the length2 HW register to transfer the bitserial_hw address from P0 to P1. */
        bs_hw = (bitserial_hw *)hal_bitserial_txrx_length2_get(bs_instance);
        L3_DBG_MSG2("bs_instance 0x%02X, bs_hw = 0x%08X", bs_instance, bs_hw);
        /* Initialise bitserial_transfer.c::instance[i] and P1 bitserial interrupts. */
        bitserial_action_init(bs_instance, bs_hw);
    }
    return bs_handle;
}


bitserial_result BitserialRead
(
    bitserial_handle bs_handle,
    bitserial_transfer_handle *transfer_handle_ptr,
    uint8 *rx_data,
    uint16 rx_size,
    bitserial_transfer_flags api_flags
)
{
    if (BITSERIAL_HANDLE_ON_P1(bs_handle))
    {
        return trap_api_bitserial_transfer(
                   bs_handle,
                   transfer_handle_ptr,
                   NULL, /* tx_data */
                   0, /* tx_size */
                   rx_data,
                   rx_size,
                   api_flags
               );
    }
    else
    {
        IPC_BITSERIAL_READ ipc_send_prim;
        IPC_BITSERIAL_RESULT_RSP ipc_recv_prim;

        ipc_send_prim.handle = bs_handle;
        ipc_send_prim.transfer_handle_ptr = transfer_handle_ptr;
        ipc_send_prim.data = rx_data;
        ipc_send_prim.size = rx_size;
        ipc_send_prim.flags = api_flags;
        ipc_transaction(
            IPC_SIGNAL_ID_BITSERIAL_READ,
            &ipc_send_prim,
            sizeof(ipc_send_prim),
            IPC_SIGNAL_ID_BITSERIAL_READ_RSP,
            &ipc_recv_prim
        );
        return ipc_recv_prim.ret;
    }
}


bitserial_result BitserialTransfer
(
    bitserial_handle bs_handle,
    bitserial_transfer_handle *transfer_handle_ptr,
    const uint8 *tx_data,
    uint16 tx_size,
    uint8 *rx_data,
    uint16 rx_size
)
{
    if (BITSERIAL_HANDLE_ON_P1(bs_handle))
    {
        bitserial_transfer_flags api_flags = (bitserial_transfer_flags)0;

        if (transfer_handle_ptr == NULL)
        {
            api_flags = BITSERIAL_FLAG_BLOCK;
        }
        return trap_api_bitserial_transfer(
                   bs_handle,
                   transfer_handle_ptr,
                   tx_data, tx_size,
                   rx_data, rx_size,
                   api_flags
               );
    }
    else
    {
        IPC_BITSERIAL_TRANSFER ipc_send_prim;
        IPC_BITSERIAL_RESULT_RSP ipc_recv_prim;

        ipc_send_prim.handle = bs_handle;
        ipc_send_prim.transfer_handle_ptr = transfer_handle_ptr;
        ipc_send_prim.tx_data = tx_data;
        ipc_send_prim.tx_size = tx_size;
        ipc_send_prim.rx_data = rx_data;
        ipc_send_prim.rx_size = rx_size;
        ipc_transaction(
            IPC_SIGNAL_ID_BITSERIAL_TRANSFER,
            &ipc_send_prim,
            sizeof(ipc_send_prim),
            IPC_SIGNAL_ID_BITSERIAL_TRANSFER_RSP,
            &ipc_recv_prim
        );
        return ipc_recv_prim.ret;
    }
}


bitserial_result BitserialWrite
(
    bitserial_handle bs_handle,
    bitserial_transfer_handle *transfer_handle_ptr,
    const uint8 *tx_data,
    uint16 tx_size,
    bitserial_transfer_flags api_flags
)
{
    if (BITSERIAL_HANDLE_ON_P1(bs_handle))
    {
        return trap_api_bitserial_transfer(
                   bs_handle,
                   transfer_handle_ptr,
                   tx_data,
                   tx_size,
                   NULL, /* rx_data */
                   0, /* rx_size */
                   api_flags
               );
    }
    else
    {
        IPC_BITSERIAL_WRITE ipc_send_prim;
        IPC_BITSERIAL_RESULT_RSP ipc_recv_prim;

        ipc_send_prim.handle = bs_handle;
        ipc_send_prim.transfer_handle_ptr = transfer_handle_ptr;
        ipc_send_prim.data = tx_data;
        ipc_send_prim.size = tx_size;
        ipc_send_prim.flags = api_flags;
        ipc_transaction(
            IPC_SIGNAL_ID_BITSERIAL_WRITE,
            &ipc_send_prim,
            sizeof(ipc_send_prim),
            IPC_SIGNAL_ID_BITSERIAL_WRITE_RSP,
            &ipc_recv_prim
        );
        return ipc_recv_prim.ret;
    }
}


Task MessageBitserialTask(Task task)
{
    return trap_api_register_message_task(task, IPC_MSG_TYPE_BITSERIAL);
}


#endif /* TRAPSET_BITSERIAL */

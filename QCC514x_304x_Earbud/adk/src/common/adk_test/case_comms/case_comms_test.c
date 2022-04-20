/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of case comms testing functions
*/

#ifdef INCLUDE_CASE_COMMS

#include "case_comms_test.h"
#include <cc_protocol.h>

#include <logging.h>
#include <panic.h>
#include <stdlib.h>

#ifdef GC_SECTIONS
/* Move all functions in KEEP_PM section to ensure they are not removed during
 * garbage collection */
#pragma unitcodesection KEEP_PM
#endif

/* Size of the circular buffers tracking case comms transactions. */
#define NUM_QUEUED_TRANSACTIONS (16)
/* The simple logic for tracking read/write indexes only works if the circular buffers
   are at least 2 entries wide. */
COMPILE_TIME_ASSERT(NUM_QUEUED_TRANSACTIONS >= 2, CaseCommsTestBufferTooShort);

/* Run time state to track case comms transactions, allocated when CaseCommsTest_Init() is called. */
typedef struct
{
    /* Buffer of transmit status */
    cc_tx_status_t cc_test_tx_status[NUM_QUEUED_TRANSACTIONS];
    uint8 tx_status_read:8;
    uint8 tx_status_write:8;
    bool tx_status_full:1;

    /* buffers of received messages and received message lengths */
    uint8* cc_test_buffer[NUM_QUEUED_TRANSACTIONS];
    uint16 cc_test_buffer_data_len[NUM_QUEUED_TRANSACTIONS];
    uint8 rx_buffer_read:8;
    uint8 rx_buffer_write:8;
    bool rx_buffer_full:1;

    uint16 max_msg_len;
} case_comms_test_t;
case_comms_test_t* case_comms_test = NULL;

/* Read and Write indices for TX and RX buffers. */
#define STATUS_WRITE_INDEX      (case_comms_test->tx_status_write)
#define STATUS_READ_INDEX       (case_comms_test->tx_status_read)
#define RX_WRITE_INDEX          (case_comms_test->rx_buffer_write)
#define RX_READ_INDEX           (case_comms_test->rx_buffer_read)

static void caseCommsTest_IncTxStatusWriteIndex(void)
{
    case_comms_test->tx_status_write = ((case_comms_test->tx_status_write + 1) % NUM_QUEUED_TRANSACTIONS);
    case_comms_test->tx_status_full = (STATUS_READ_INDEX == STATUS_WRITE_INDEX) ? TRUE : FALSE;
}

static void caseCommsTest_IncTxStatusReadIndex(void)
{
    case_comms_test->tx_status_read = ((case_comms_test->tx_status_read + 1) % NUM_QUEUED_TRANSACTIONS);
    if (STATUS_READ_INDEX != STATUS_WRITE_INDEX)
    {
        case_comms_test->tx_status_full = FALSE;
    }
}

static void caseCommsTest_IncRxBufferWriteIndex(void)
{
    case_comms_test->rx_buffer_write = ((case_comms_test->rx_buffer_write + 1) % NUM_QUEUED_TRANSACTIONS);
    case_comms_test->rx_buffer_full = (RX_WRITE_INDEX == RX_READ_INDEX) ? TRUE : FALSE;
}

static void caseCommsTest_IncRxBufferReadIndex(void)
{
    case_comms_test->rx_buffer_read = ((case_comms_test->rx_buffer_read + 1) % NUM_QUEUED_TRANSACTIONS);
    if (RX_READ_INDEX != RX_WRITE_INDEX)
    {
        case_comms_test->rx_buffer_full = FALSE;
    }
}

static bool CaseCommsTest_IsInitialised(void)
{
    if (case_comms_test == NULL)
    {
        DEBUG_LOG_ALWAYS("CaseCommsTest_IsInitialised not initialised");
        return FALSE;
    }

    return TRUE;
}

static void CaseCommsTest_HandleTxStatus(cc_tx_status_t status, unsigned mid)
{
    /* no need for initialisation protection, can only be called if initialised and the test CID
       is registered */
    DEBUG_LOG_ALWAYS("CaseCommsTest_HandleTxStatus idx:%d mid:%d enum:cc_tx_status_t:%d", STATUS_WRITE_INDEX, mid, status);
    case_comms_test->cc_test_tx_status[STATUS_WRITE_INDEX] = status;
    caseCommsTest_IncTxStatusWriteIndex();;
}

static void CaseCommsTest_HandleRxInd(unsigned mid, const uint8* msg, unsigned length, cc_dev_t source_dev)
{
    /* no need for initialisation protection, can only be called if initialised and the test CID
       is registered */
    if (!case_comms_test->rx_buffer_full)
    {
        unsigned length_ceil = length <= case_comms_test->max_msg_len ? length : case_comms_test->max_msg_len;
        case_comms_test->cc_test_buffer[RX_WRITE_INDEX] = PanicUnlessMalloc(length_ceil);

        DEBUG_LOG_ALWAYS("CaseCommsTest_HandleRxInd idx:%d rx %d byte on mid:%d from enum:cc_dev_t:%d", RX_WRITE_INDEX, length, mid, source_dev);

        case_comms_test->cc_test_buffer_data_len[RX_WRITE_INDEX] = length_ceil;
        memcpy(case_comms_test->cc_test_buffer[RX_WRITE_INDEX], msg, case_comms_test->cc_test_buffer_data_len[RX_WRITE_INDEX]);
        caseCommsTest_IncRxBufferWriteIndex();
    }
    else
    {
        DEBUG_LOG_ALWAYS("CaseCommsTest_HandleRxInd DISCARDED RX MESSAGE idx:%d rx %d byte on mid:%d from enum:cc_dev_t:%d", RX_WRITE_INDEX, length, mid, source_dev);
    }
}

void CaseCommsTest_Init(unsigned max_msg_len)
{
    cc_chan_config_t cfg;
    
    DEBUG_LOG_ALWAYS("CaseCommsTest_Init");

    if (!case_comms_test)
    {
        case_comms_test = PanicUnlessMalloc(sizeof(case_comms_test_t));
        case_comms_test->max_msg_len = max_msg_len;
        for (int i=0; i < NUM_QUEUED_TRANSACTIONS; i++)
        {
            case_comms_test->cc_test_buffer[i] = NULL;
            case_comms_test->cc_test_buffer_data_len[i] = 0;
            case_comms_test->cc_test_tx_status[i] = CASECOMMS_STATUS_UNKNOWN;
        }
        case_comms_test->tx_status_write = 0;
        case_comms_test->tx_status_read = 0;
        case_comms_test->rx_buffer_write = 0;
        case_comms_test->rx_buffer_read = 0;
        case_comms_test->tx_status_full = FALSE;
        case_comms_test->rx_buffer_full = FALSE;

        /* register for the case comms test channel */
        cfg.cid = CASECOMMS_CID_TEST;
        cfg.tx_sts = CaseCommsTest_HandleTxStatus;
        cfg.rx_ind = CaseCommsTest_HandleRxInd;
        CcProtocol_RegisterChannel(&cfg);
    }
}

bool CaseCommsTest_TxMsg(cc_dev_t dest, unsigned mid, uint8* msg, uint16 len, bool expect_response)
{
    bool tx_result = FALSE;

    if (CaseCommsTest_IsInitialised() && !case_comms_test->tx_status_full)
    {
        if (expect_response)
        {
            tx_result = CcProtocol_Transmit(dest, CASECOMMS_CID_TEST, mid, msg, len);
        }
        else
        {
            tx_result = CcProtocol_TransmitNotification(dest, CASECOMMS_CID_TEST, mid, msg, len);
        }

        DEBUG_LOG_ALWAYS("CaseCommsTest_TxMsg %d bytes to enum:cc_dev_t:%d mid:%d resp:%d sts:%d", len, dest, mid, expect_response, tx_result);
    }

    return tx_result;
}

uint16 CaseCommsTest_RxMsg(uint8* msg)
{
    uint16 len = 0;

    if (CaseCommsTest_IsInitialised())
    {
        len = case_comms_test->cc_test_buffer_data_len[RX_READ_INDEX];

        DEBUG_LOG_ALWAYS("CaseCommsTest_GetRxMsg idx:%d len:%d", RX_READ_INDEX, len);

        memcpy(msg, case_comms_test->cc_test_buffer[RX_READ_INDEX], len);

        /* reset the read length, clean up the receive buffer and increment the
           index for next transaction */
        case_comms_test->cc_test_buffer_data_len[RX_READ_INDEX] = 0;
        free(case_comms_test->cc_test_buffer[RX_READ_INDEX]);
        case_comms_test->cc_test_buffer[RX_READ_INDEX] = NULL;
        caseCommsTest_IncRxBufferReadIndex();
    }

    return len;
}

uint16 CaseCommsTest_PollRxMsgLen(void)
{
    uint16 len = 0;

    if (CaseCommsTest_IsInitialised())
    {
        DEBUG_LOG_ALWAYS("CaseCommsTest_PollRxMsgLen idx:%d len:%d", RX_READ_INDEX, case_comms_test->cc_test_buffer_data_len[RX_READ_INDEX]);
        len = case_comms_test->cc_test_buffer_data_len[RX_READ_INDEX];
    }

    return len;
}

cc_tx_status_t CaseCommsTest_PollTxStatus(void)
{
    cc_tx_status_t sts = CASECOMMS_STATUS_UNKNOWN;

    if (CaseCommsTest_IsInitialised())
    {
        DEBUG_LOG_ALWAYS("CaseCommsTest_GetTxStatus idx:%d enum:cc_tx_status_t:%d", STATUS_READ_INDEX, case_comms_test->cc_test_tx_status[STATUS_READ_INDEX]);
        sts = case_comms_test->cc_test_tx_status[STATUS_READ_INDEX];

        /* if status has changed, reset to unknown and increment read index for next transaction */
        if (sts != CASECOMMS_STATUS_UNKNOWN)
        {
            case_comms_test->cc_test_tx_status[STATUS_READ_INDEX] = CASECOMMS_STATUS_UNKNOWN;
            caseCommsTest_IncTxStatusReadIndex();
        }
    }

    return sts;
}

#endif /* INCLUDE_CASE_COMMS */

/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    case_comms
\brief      
*/
/*! \addtogroup case_comms
@{
*/

#ifdef INCLUDE_CASE_COMMS
#ifdef HAVE_CC_MODE_CASE

#include "cc_with_earbuds.h"
#include "cc_with_earbuds_private.h"

#include <cc_protocol.h>
#include <cc_case_channel.h>

#include <logging.h>
#include <task_list.h>
#include <timestamp_event.h>

#include <message.h>
#include <panic.h>

#pragma unitcodesection KEEP_PM

/* Limit consecutive loopbacks to maximum number of messages which can be put into a stream. */
#define MAX_LOOPBACK_ITERATIONS (16)

/*! Case comms with Earbuds task data. */
cc_with_earbuds_t cc_with_earbuds;

static void ccWithEarbuds_GetEbStatus(cc_dev_t dest)
{
    DEBUG_LOG_VERBOSE("ccWithEarbuds_GetEbStatus enum:cc_dest_t:%d", dest);
    CcCaseChannel_EarbudStatusReqTx(dest);
}

/* Get the earbud state data by cascomms source device. */
static eb_state* ccWithEarbuds_EbState(cc_dev_t eb)
{
    switch (eb)
    {
        case CASECOMMS_DEVICE_LEFT_EB:
            return &CcWithEarbuds_GetTaskData()->earbuds_state[0];
        case CASECOMMS_DEVICE_RIGHT_EB:
            return &CcWithEarbuds_GetTaskData()->earbuds_state[1];
        default:
            DEBUG_LOG_ERROR("ccWithEarbuds_EbState unknown EB enum:cc_dev_t:%d", eb);
            Panic();
    }

    return NULL;
}

/* Reset an Earbud state to unknown. */
static void ccWithEarbuds_ResetEbState(cc_dev_t dev)
{
    eb_state* eb = ccWithEarbuds_EbState(dev);

    /*! \todo use BATTERY_STATUS_UNKNOWN when it moves to a common location */
    eb->battery_state = 0x7F;
    eb->present = FALSE;
    eb->peer_paired = PP_STATE_UNKNOWN;
}

static void ccWithEarbuds_HandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);

    switch (id)
    {
        case CCWE_INTERNAL_TIMEOUT_GET_EB_STATUS:
            ccWithEarbuds_GetEbStatus(CASECOMMS_DEVICE_LEFT_EB);
            break;
        default:
            break;
    }
}

bool CcWithEarbuds_Init(Task init_task)
{
    cc_with_earbuds_t* td = CcWithEarbuds_GetTaskData();

    UNUSED(init_task);

    DEBUG_LOG("CcWithEarbuds_Init");

    memset(td, 0, sizeof(cc_with_earbuds_t));
    td->task.handler = ccWithEarbuds_HandleMessage;
    TaskList_InitialiseWithCapacity(CcWithEarbuds_GetClientTasks(), CLIENTS_TASK_LIST_INIT_CAPACITY);
    ccWithEarbuds_ResetEbState(CASECOMMS_DEVICE_LEFT_EB);
    ccWithEarbuds_ResetEbState(CASECOMMS_DEVICE_RIGHT_EB);

    /* initialise case comms protocol and transport and the case channel */
    CcProtocol_Init(CASECOMMS_MODE_CASE, CC_TRANSPORT);
    CcCaseChannel_Init();

    return TRUE;
}

void CcWithEarbuds_RegisterClient(Task client_task)
{
    TaskList_AddTask(TaskList_GetFlexibleBaseTaskList(CcWithEarbuds_GetClientTasks()), client_task);
}

void CcWithEarbuds_UnregisterClient(Task client_task)
{
    TaskList_RemoveTask(TaskList_GetFlexibleBaseTaskList(CcWithEarbuds_GetClientTasks()), client_task);
}

void CcWithEarbuds_EarbudStatusRx(cc_dev_t source, uint8 battery_state, bool peer_paired)
{
    eb_state* eb = ccWithEarbuds_EbState(source);

    DEBUG_LOG("CcWithEarbuds_EarbudStatusRx enum:cc_dev_t:%d batt:0x%x pp:%d ", source, battery_state, peer_paired);

    eb->present = TRUE;
    eb->peer_paired = peer_paired ? PP_STATE_PAIRED : PP_STATE_NOT_PAIRED;
    eb->battery_state = battery_state;
}

void CcWithEarbuds_EarbudBtAddressRx(const bdaddr* addr, cc_dev_t source)
{
    eb_state* eb = ccWithEarbuds_EbState(source);

    DEBUG_LOG("CcWithEarbuds_EarbudBtAddressRx enum:cc_dev_t:%d 0x%x 0x%x 0x%x", source, addr->lap, addr->uap, addr->nap);

    eb->present = TRUE;
    eb->addr = *addr;
}

void CcWithEarbuds_LoopbackTx(cc_dev_t dest, uint8* data, unsigned len, unsigned iterations)
{
    cc_with_earbuds_t* td = CcWithEarbuds_GetTaskData();
    unsigned iter = iterations <= MAX_LOOPBACK_ITERATIONS ? iterations : MAX_LOOPBACK_ITERATIONS;
    unsigned tx_done = 0;

    td->loopback_sent = 0;
    td->loopback_recv = 0;

    TimestampEvent(TIMESTAMP_EVENT_CASECOMMS_LOOPBACK_TX);

    for (int i = 0; i<iter; i++)
    {
        if (CcCaseChannel_LoopbackTx(dest, data, len))
        {
            tx_done++;
        }
    }
    DEBUG_LOG("CcWithEarbuds_LoopbackTx managed %d transmits of %d requested iterations", tx_done, iterations);
    td->loopback_sent = tx_done;
}

void CcWithEarbuds_LoopbackRx(cc_dev_t source, uint8* data, unsigned len)
{
    cc_with_earbuds_t* td = CcWithEarbuds_GetTaskData();

    UNUSED(source);
    UNUSED(data);

    td->loopback_recv++;

    if (td->loopback_recv == td->loopback_sent)
    {
        TimestampEvent(TIMESTAMP_EVENT_CASECOMMS_LOOPBACK_RX);
        DEBUG_LOG("CcWithEarbuds_LoopbackRx len:%d elapsed: %u", len, TimestampEvent_Delta(TIMESTAMP_EVENT_CASECOMMS_LOOPBACK_TX, TIMESTAMP_EVENT_CASECOMMS_LOOPBACK_RX));
    }
}

void CcWithEarbuds_TransmitStatusRx(cc_tx_status_t status, unsigned mid)
{
    DEBUG_LOG_V_VERBOSE("CcWithEarbuds_TransmitStatusRx enum:cc_tx_status_t:%d mid:%d", status, mid);
}

void CcWithEarbuds_PeerPairResponseRx(cc_dev_t source, bool peer_pairing_started)
{
    DEBUG_LOG_INFO("CcWithEarbuds_PeerPairResponseRx enum:cc_dev_t:%d peer_pairing_started %d", source, peer_pairing_started);
}

void CcWithEarbuds_ShippingModeResponseRx(cc_dev_t source, bool shipping_mode_accepted)
{
    DEBUG_LOG_INFO("CcWithEarbuds_ShippingModeResponseRx enum:cc_dev_t:%d shipping_mode_accepted %d", source, shipping_mode_accepted);
}

#endif /* HAVE_CC_MODE_CASE */
#endif /* INCLUDE_CASE_COMMS */
/*! @} End of group documentation */

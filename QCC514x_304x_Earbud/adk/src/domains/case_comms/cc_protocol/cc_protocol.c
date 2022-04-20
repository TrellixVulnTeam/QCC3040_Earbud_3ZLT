/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    case_comms
\brief      Communication with the case using charger comms traps.
*/
/*! \addtogroup case_comms
@{
*/

#include "cc_protocol.h"
#include "cc_protocol_private.h"
#include "cc_protocol_config.h"
#include "cc_protocol_trans_schemeA.h"
#include "cc_protocol_trans_schemeB.h"
#include "cc_protocol_trans_test_uart.h"

#include <multidevice.h>

#include <logging.h>

#include <message.h>
#include <stdlib.h>
#include <panic.h>
#include <chargercomms.h>
#include <stream.h>
#include <Source.h>
#include <sink.h>

#include <dormant.h>

#ifdef INCLUDE_CASE_COMMS

#pragma unitcodesection KEEP_PM

/*! Case comms protocol task state. */
cc_protocol_t cc_protocol;

/***********************************************
 * Case Comms Protocol message utility functions
 ***********************************************/
/*! \brief Utility function to read Channel ID from a Case Comms header. */
cc_cid_t ccProtocol_CaseCommsGetCID(uint8 ccomms_header)
{
    return ((ccomms_header & CASECOMMS_CID_MASK) >> CASECOMMS_CID_BIT_OFFSET);
}

/*! \brief Utility function to set Channel ID in a Case Comms header. */
void ccProtocol_CaseCommsSetCID(uint8* ccomms_header, cc_cid_t cid)
{
    *ccomms_header = (*ccomms_header & ~CASECOMMS_CID_MASK) | ((cid << CASECOMMS_CID_BIT_OFFSET) & CASECOMMS_CID_MASK);
}

/*! \brief Utility function to read Message ID from a Case Comms header. */
unsigned ccProtocol_CaseCommsGetMID(uint8 ccomms_header)
{
    return ((ccomms_header & CASECOMMS_MID_MASK) >> CASECOMMS_MID_BIT_OFFSET);
}

/*! \brief Utility function to set Message ID in a Case Comms header. */
void ccProtocol_CaseCommsSetMID(uint8* ccomms_header, unsigned mid)
{
    *ccomms_header = (*ccomms_header & ~CASECOMMS_MID_MASK) | ((mid << CASECOMMS_MID_BIT_OFFSET) & CASECOMMS_MID_MASK);
}

/****************************************
 * Case Comms component utility functions
 ****************************************/
/* Return a configuration if CID is known, otherwise NULL. */
static cc_chan_config_t* ccProtocol_GetChannelConfig(cc_cid_t cid)
{
    cc_protocol_t* td = CcProtocol_GetTaskData();
    cc_chan_config_t* config = NULL;

    if (cid < CASECOMMS_CID_MAX)
    {
        config = &td->channel_cfg[cid];
    }
    return config;
}

/*! \brief Convert charger comms status to case comms status. */
static cc_tx_status_t ccProtocol_CommsGetStatus(charger_comms_msg_status status)
{
    cc_tx_status_t sts = CASECOMMS_TX_SUCCESS;

    if (status != CHARGER_COMMS_MSG_SUCCESS)
    {
        sts = CASECOMMS_TX_FAIL;
    }

    return sts;
}

#ifdef HAVE_CC_TRANS_SCHEME_B
/*! \brief Convert charger comms status to case comms status. */
static cc_tx_status_t ccProtocol_CommsUartStatusToCcStatus(charger_comms_uart_tx_status status)
{
    cc_tx_status_t sts = CASECOMMS_TX_SUCCESS;

    switch (status)
    {
        /* already initialised to success */
        case CHARGER_COMMS_UART_TX_SUCCESS:
            break;

        /* timeout and flushed are explicitly identified */
        case CHARGER_COMMS_UART_TX_TIMEOUT:
            sts = CASECOMMS_TX_TIMEOUT;
            break;
        case CHARGER_COMMS_UART_TX_BROADCAST_FLUSH:
            sts = CASECOMMS_TX_BROADCAST_FLUSHED;
            break;

        /* all other failures are generically TX_FAIL */
        case CHARGER_COMMS_UART_TX_FAILED:
        case CHARGER_COMMS_UART_TX_INVALID_REQ:
        case CHARGER_COMMS_UART_TX_HW_BUFFER_FULL:
        default:
            sts = CASECOMMS_TX_FAIL;
            break;
    }

    return sts;
}

/*! \brief Convert charger comms address to case comms address. */
static cc_dev_t ccProtocol_CommsUartAddrToCcAddr(charger_comms_uart_address addr)
{
    cc_dev_t dev = CASECOMMS_DEVICE_BROADCAST;

    switch (addr)
    {
        case CHARGER_COMMS_UART_ADDRESS_CASE:
            dev = CASECOMMS_DEVICE_CASE;
            break;
        case CHARGER_COMMS_UART_ADDRESS_EB_R:
            dev = CASECOMMS_DEVICE_RIGHT_EB;
            break;
        case CHARGER_COMMS_UART_ADDRESS_EB_L:
            dev = CASECOMMS_DEVICE_LEFT_EB;
            break;
        default:
            dev = CASECOMMS_DEVICE_BROADCAST;
            break;
    }

    return dev;
}
#endif

/* Case mode poll handling for Scheme B transport.
  
   Clients sending messages over their casecomms channel indicate if the
   message expects a response. For Scheme B transport only, in case mode,
   it is the responsibility of P1 to generate poll messages where there are
   outstanding responses, to provide Earbuds with an opportunity to send the
   response.
  
   A reference count is maintained per Earbud, per casecomms channel, of the
   number of outstanding responses. Reference count is incremented on successful
   submission to P0 for transmit and decremented on message receipt or on status
   report of transmit failure.
  
   Decision to schedule a poll message is made after transmit status is received
   from P0 or failure to transmit a previous poll. If only one Earbud has responses
   outstanding poll will be scheduled for that Earbud, or if both require poll
   then the least recently polled Earbud will be polled.
*/
static void ccProtocol_ResetAllOutstandingResponseCounts(void)
{
    cc_protocol_t* td = CcProtocol_GetTaskData();

    for (int i=0; i<CASECOMMS_CID_MAX; i++)
    {
        td->channel_cfg[i].left_outstanding_response_count = 0;
        td->channel_cfg[i].right_outstanding_response_count = 0;
    }
}

/* Determine if either Earbud needs to be polled and if so which one.
 
   \param dev[out] Pointer to return Earbud dev type, if one requires polling
   \param bool TRUE an Earbud needs to be polled #dev will be valid
               FALSE no poll required, #dev not valid.
*/
static bool ccProtocol_GetEarbudToPoll(cc_dev_t* dev)
{
    cc_protocol_t* td = CcProtocol_GetTaskData();
    bool reqd = FALSE;
    unsigned left_outstanding_response_total = 0;
    unsigned right_outstanding_response_total = 0;

    /* calculate total outstanding response messasges for left and right earbuds,
       across all channnels */
    for (int i=0; i<CASECOMMS_CID_MAX; i++)
    {
        left_outstanding_response_total += td->channel_cfg[i].left_outstanding_response_count;
        right_outstanding_response_total += td->channel_cfg[i].right_outstanding_response_count;
    }

    if (left_outstanding_response_total != 0 && right_outstanding_response_total == 0)
    {
        /* only left needs to be polled */
        *dev = CASECOMMS_DEVICE_LEFT_EB;
        reqd = TRUE;
    }
    else if (left_outstanding_response_total == 0 && right_outstanding_response_total > 0)
    {
        /* only right needs to be polled */
        *dev = CASECOMMS_DEVICE_RIGHT_EB;
        reqd = TRUE;
    }
    else if (left_outstanding_response_total > 0 && right_outstanding_response_total > 0)
    {
        /* both need to be polled, poll least recent */
        if (td->last_earbud_polled == CASECOMMS_DEVICE_LEFT_EB)
        {
            *dev = CASECOMMS_DEVICE_RIGHT_EB;
        }
        else
        {
            *dev = CASECOMMS_DEVICE_LEFT_EB;
        }
        reqd = TRUE;
    }

    if (reqd)
    {
        DEBUG_LOG_V_VERBOSE("ccProtocol_GetEarbudToPoll poll reqd for enum:cc_dev_t:%d", *dev);
    }

    return reqd;
}

/* Update polling reference counter for device and channel.
   If polling is no longer required, ensure no polling timer is pending.
*/
static void ccProtocol_ModifyPollRequired(cc_dev_t dev, cc_cid_t cid, bool incr)
{
    cc_chan_config_t* chan = ccProtocol_GetChannelConfig(cid);
    uint8* outstanding_response_count = NULL;
    MessageId id = 0;
    
    if (chan)
    {
        outstanding_response_count = &(chan->left_outstanding_response_count);
        id = CC_PROTOCOL_INTERNAL_POLL_LEFT_TIMEOUT;

        if (dev == CASECOMMS_DEVICE_RIGHT_EB)
        {
            outstanding_response_count = &(chan->right_outstanding_response_count);
            id = CC_PROTOCOL_INTERNAL_POLL_RIGHT_TIMEOUT;
        }
        else if (dev != CASECOMMS_DEVICE_LEFT_EB)
        {
            DEBUG_LOG_ERROR("ccProtocol_ModifyPollRequired unsupported device enum:cc_dev_t:%d", dev);
            Panic();
        }

        if (incr)
        {
            DEBUG_LOG_V_VERBOSE("ccProtocol_ModifyPollRequired incr enum:cc_dev_t:%d enum:cc_cid_t:%d current:%d", dev, cid, *outstanding_response_count);
            PanicFalse(*outstanding_response_count < 0xff);
            *outstanding_response_count += 1;
        }
        else
        {
            DEBUG_LOG_V_VERBOSE("ccProtocol_ModifyPollRequired decr enum:cc_dev_t:%d enum:cc_cid_t:%d current:%d", dev, cid, *outstanding_response_count);
            if (*outstanding_response_count > 0x0)
            {
                *outstanding_response_count -= 1;
            }

            /* no more responses required, so no polls are required,
               cancel any pending poll timer */
            if (*outstanding_response_count == 0)
            {
                DEBUG_LOG_V_VERBOSE("ccProtocol_ModifyPollRequired cancelling poll enum:cc_protocol_internal_message_t:%d", id);
                MessageCancelFirst(CcProtocol_GetTask(), id);
            }
        }
    }
}

static inline void ccProtocol_IncrementPollRequired(cc_dev_t dev, cc_cid_t cid)
{
    if (ccProtocol_TransRequiresPolling())
    {
        ccProtocol_ModifyPollRequired(dev, cid, TRUE);
    }
}

static inline void ccProtocol_DecrementPollRequired(cc_dev_t dev, cc_cid_t cid)
{
    if (ccProtocol_TransRequiresPolling())
    {
        ccProtocol_ModifyPollRequired(dev, cid, FALSE);
    }
}

/* If an Earbud needs a poll and one not already scheduled then start timer.
   Timer may get cancelled by arrival of a response.
*/
static void ccProtocol_SchedulePollIfRequired(void)
{
    cc_protocol_t* td = CcProtocol_GetTaskData();
    cc_dev_t dest = CASECOMMS_DEVICE_LEFT_EB;

    if (ccProtocol_TransRequiresPolling() && ccProtocol_GetEarbudToPoll(&dest))
    {
        int32 due;
        MessageId id = (dest == CASECOMMS_DEVICE_LEFT_EB) ? CC_PROTOCOL_INTERNAL_POLL_LEFT_TIMEOUT :
                                                            CC_PROTOCOL_INTERNAL_POLL_RIGHT_TIMEOUT;
        if (!MessagePendingFirst(CcProtocol_GetTask(), id, &due))
        {
            CC_PROTOCOL_INTERNAL_POLL_TIMEOUT_T* poll = PanicUnlessMalloc(sizeof(CC_PROTOCOL_INTERNAL_POLL_TIMEOUT_T));
            poll->dest = dest;
            DEBUG_LOG_V_VERBOSE("ccProtocol_SchedulePollIfRequired sched poll for enum:cc_dev_t:%d", dest);
            MessageSendLater(CcProtocol_GetTask(), id, poll, td->poll_timeout);
        }
    }
}

/* If poll still required, call transport to send it.
*/
static void ccProtocol_HandlePollTimer(const CC_PROTOCOL_INTERNAL_POLL_TIMEOUT_T* poll)
{
    cc_protocol_t* td = CcProtocol_GetTaskData();
    cc_dev_t dest;

    /*! Check if last outstanding response was received before this poll
        timer was delivered and no longer needed */
    if (!ccProtocol_GetEarbudToPoll(&dest))
    {
        DEBUG_LOG_V_VERBOSE("ccProtocol_HandlePollTimer poll no longer reqd");
        return;
    }

    DEBUG_LOG_V_VERBOSE("ccProtocol_HandlePollTimer enum:cc_dev_t:%d", poll->dest);

    if (ccProtocol_TransTransmit(poll->dest, CASECOMMS_CID_INVALID, 0, NULL, 0))
    {
        td->last_earbud_polled = poll->dest;
    }
    else
    {
        /* failed to send the poll, likely full sink, schedule again */
        ccProtocol_SchedulePollIfRequired();
    }
}

/***********************************
 * Transmit Handling
 ***********************************/
static void ccProtocol_TransmitBroadcastReset(void)
{
    if (!ccProtocol_TransTransmit(CASECOMMS_DEVICE_BROADCAST, CASECOMMS_CID_INVALID, 0, NULL, 0))
    {
        DEBUG_LOG_WARN("ccProtocol_TransmitBroadcastReset transport failed");
    }
    /* broadcast message will flush the transmit queue, but we don't yet receive
       CHARGER_COMMS_UART_TX_FAILED for each flushed message, so manually reset
       the outstanding response counts, so we don't keep polling. */
    ccProtocol_ResetAllOutstandingResponseCounts();
}

/* In case mode track the number of NAKs received, if it reaches
   CcProtocol_ConfigNumFailsToReset() consider the
   seqnum broken and generate a reset message to resync.
*/
static void ccProtocol_CheckAckNakReset(cc_tx_status_t status)
{
    cc_protocol_t* td = CcProtocol_GetTaskData();

    if (ccProtocol_TransRequiresReset())
    {
        switch (status)
        {
            case CASECOMMS_TX_SUCCESS:
                td->tx_fail_count = 0;
                break;
            case CASECOMMS_TX_FAIL:
                /* fall-thru */
            case CASECOMMS_TX_TIMEOUT:
                if (++td->tx_fail_count == CcProtocol_ConfigNumFailsToReset())
                {
                    DEBUG_LOG_VERBOSE("ccProtocol_CheckAckNakReset");
                    td->reset_fn();
                    td->tx_fail_count = 0;
                }
                break;
            case CASECOMMS_TX_BROADCAST_FLUSHED:
                break;
            case CASECOMMS_STATUS_UNKNOWN:
                /* fall-thru */
            default:
                DEBUG_LOG_WARN("ccProtocol_TransRequiresReset unexpected status enum:cc_tx_status_t:%d", status);
                break;
        }
    }
}

static void ccProtocol_SendStatusToClient(cc_tx_status_t status, cc_cid_t cid, unsigned mid)
{
    cc_protocol_t* td = CcProtocol_GetTaskData();
    cc_chan_config_t* chan = ccProtocol_GetChannelConfig(cid);

    if (td->mode == CASECOMMS_MODE_CASE)
    {
        ccProtocol_CheckAckNakReset(status);
    }

    /* if client has registered a status callback, pass status to client */
    if (chan && chan->tx_sts)
    {
        chan->tx_sts(status, mid);
    }
}

/* Scheme A transmit status handling. */
static void ccProtocol_HandleMessageChargercommsStatus(const MessageChargerCommsStatus* msg)
{
    DEBUG_LOG_VERBOSE("ccProtocol_HandleMessageChargercommsStatus sts:%d", msg->status);

    /* Scheme A transport only supports one message TX at a time and records
       the CID and MIB, retrieve those to send client status. */
    ccProtocol_SendStatusToClient(ccProtocol_CommsGetStatus(msg->status),
                                  ccProtocol_TransSchemeAGetCidInTransmit(),
                                  ccProtocol_TransSchemeAGetMidInTransmit());
    
    /* no checking for polling required, scheme A is only supported on the earbud side */
}

#ifdef HAVE_CC_TRANS_SCHEME_B
/* Scheme B transmit status handling. */
static void ccProtocol_HandleMessageChargercommsUartStatus(const MessageChargerCommsUartStatus* msg)
{
    cc_protocol_t* td = CcProtocol_GetTaskData();

    DEBUG_LOG_VERBOSE("ccProtocol_HandleMessageChargercommsUartStatus enum:charger_comms_uart_tx_status:%d enum:charger_comms_uart_msg_type:%d enum:charger_comms_uart_address:%d cc_header:0x%x",
                        msg->status, msg->header.type, msg->header.dest_address, msg->header.header);

    /* if this is the case and status isn't success, and destination was an earbud, then
       the message was either not transmitted or failed to be acknowledged, so reduce the
       outstanding response count accordingly */
    if (   td->mode == CASECOMMS_MODE_CASE
        && msg->status != CHARGER_COMMS_UART_TX_SUCCESS)
    {
        if (   msg->header.dest_address == CHARGER_COMMS_UART_ADDRESS_EB_L
            || msg->header.dest_address == CHARGER_COMMS_UART_ADDRESS_EB_R)
        {
            ccProtocol_DecrementPollRequired(ccProtocol_CommsUartAddrToCcAddr(msg->header.dest_address),
                                             ccProtocol_CaseCommsGetCID(msg->header.header));
        }
    }

    /* only report status to clients for data packets, not polls */
    if (msg->header.type == CHARGER_COMMS_UART_MSG_TYPE_COMMS_DATA)
    {
        ccProtocol_SendStatusToClient(ccProtocol_CommsUartStatusToCcStatus(msg->status),
                                      ccProtocol_CaseCommsGetCID(msg->header.header),
                                      ccProtocol_CaseCommsGetMID(msg->header.header));
    }

    /* if this is the case, check if any polls needed to be sent to get
       outstanding message responsees */
    if (td->mode == CASECOMMS_MODE_CASE)
    {
        ccProtocol_SchedulePollIfRequired();
    }
}
#endif

static bool ccProtocol_TransmitInternal(cc_dev_t dest, cc_cid_t cid, unsigned mid, 
                        uint8* data, uint16 len, bool response_reqd)
{
    cc_protocol_t* td = CcProtocol_GetTaskData();

    /* call the transport specific transmit function, resolved by the linker to the
       specific transport compiled in. */
    if (!ccProtocol_TransTransmit(dest, cid, mid, data, len))
    {
        DEBUG_LOG_WARN("CcProtocol_Transmit transport failed to send message enum:cc_dev_t:%d enum:cc_cid_t:%d mid:%d", dest, cid, mid);
        return FALSE;
    }

    DEBUG_LOG_VERBOSE("CcProtocol_Transmit enum:cc_dev_t:%d enum:cc_cid_t:%d mid:%d len:%d resp:%d", dest, cid, mid, len, response_reqd);

    /* case records successful transmissions which may require polling to get response */
    if (response_reqd && td->mode == CASECOMMS_MODE_CASE)
    {
        ccProtocol_IncrementPollRequired(dest, cid);
    }

    return TRUE;
}

/***********************************
 * Receive Handling
 ***********************************/
/* Called by transport recieve handlers to pass on incoming messages. */
void ccProtocol_SendRXPacketToClient(const uint8* pkt, unsigned len, cc_cid_t cid, unsigned mid, cc_dev_t source_dev)
{
    cc_protocol_t* td = CcProtocol_GetTaskData();
    cc_chan_config_t* chan = ccProtocol_GetChannelConfig(cid);

    DEBUG_LOG_VERBOSE("ccProtocol_SendRXPacketToClient len:%d enum:cc_cid_t:%d mid:%d enum:cc_dev_t:%d", len, cid, mid, source_dev);

    /* if known channel and client registered a receive callback
       forward the incoming message payload, otherwise ignore */
    if (chan)
    {
        if (chan->rx_ind)
        {
            chan->rx_ind(mid, pkt, len, source_dev);
        }
    }
    else
    {
        DEBUG_LOG_WARN("ccProtocol_SendRXPacketToClient unsupported cid enum:cc_cid_t:%d", cid);
    }

    /* a case receiving a data packet reduces the count of outstanding responses
       for the channel and device */
    if (td->mode == CASECOMMS_MODE_CASE)
    {
        ccProtocol_DecrementPollRequired(source_dev, cid);
    }
}

void ccProtocol_ProcessStreamSource(Source src)
{
    cc_protocol_t* td = CcProtocol_GetTaskData();

    switch (td->trans)
    {
        case CASECOMMS_TRANS_SCHEME_B:
            ccProtocol_TransSchemeBReceive(src);
            break;
        case CASECOMMS_TRANS_TEST_UART:
            ccProtocol_TransTestUartReceive(src);
            break;
        case CASECOMMS_TRANS_SCHEME_A:
            /* fall-thru */
        default:
            DEBUG_LOG_ERROR("ccProtocol_ProcessStreamSource unsupported transport enum:cc_trans_t:%d for MMD from source 0x%x", td->trans, src);
            Panic();
            break;
    }
}

static void CcProtocol_HandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch (id)
    {
        /* stream based transport messages */
        case MESSAGE_MORE_DATA:
            ccProtocol_ProcessStreamSource(((const MessageMoreData*)message)->source);
            break;
        case MESSAGE_MORE_SPACE:
            /* not used */
            break;
#ifdef HAVE_CC_TRANS_SCHEME_B
        case MESSAGE_CHARGERCOMMS_UART_STATUS:
            ccProtocol_HandleMessageChargercommsUartStatus((const MessageChargerCommsUartStatus*)message);
            break;
#endif

        /* trap based transport messages */
        case MESSAGE_CHARGERCOMMS_IND:
            ccProtocol_TransSchemeAReceive((const MessageChargerCommsInd*)message);
            break;
        case MESSAGE_CHARGERCOMMS_STATUS:
            ccProtocol_HandleMessageChargercommsStatus((const MessageChargerCommsStatus*)message);
            break;

        /* polling timers */
        case CC_PROTOCOL_INTERNAL_POLL_LEFT_TIMEOUT:
        case CC_PROTOCOL_INTERNAL_POLL_RIGHT_TIMEOUT:
            ccProtocol_HandlePollTimer((const CC_PROTOCOL_INTERNAL_POLL_TIMEOUT_T*)message);
            break;

        default:
            DEBUG_LOG_WARN("CcProtocol_HandleMessage. Unhandled message MESSAGE:0x%x",id);
            break;
    }
}

/*****************************
 * Public API
 *****************************/
void CcProtocol_Init(cc_mode_t mode, cc_trans_t trans)
{
    cc_protocol_t* td = CcProtocol_GetTaskData();

    memset(td, 0, sizeof(cc_protocol_t));
    td->task.handler = CcProtocol_HandleMessage;
    td->mode = mode;
    td->trans = trans;
    td->poll_timeout = CcProtocol_ConfigPollScheduleTimeoutMs();
    td->reset_fn = ccProtocol_TransmitBroadcastReset;

    /* Initialise channel config CIDs, so they don't all default to
       CASECOMMS_CID_CASE (0x0) */
    for (int i = 0; i < CASECOMMS_CID_MAX; i++)
    {
        td->channel_cfg[i].cid = CASECOMMS_CID_INVALID;
    }

    /* Register to receive charger comms messages from P0 */
    MessageChargerCommsTask(CcProtocol_GetTask());

    /* call the transport specific setup function, resolved by the linker to the
       specific transport compiled in. */
    ccProtocol_TransSetup();

    /* Enable the transport at startup, if configured to do so and if supported
       by the transport. This only takes effect for scheme B, scheme A and the
       test UART are always enabled during ccProtocol_TransSetup(). */
    if (CcProtocol_ConfigEnableTransportOnStartup())
    {
        PanicFalse(ccProtocol_TransEnable());
    }
}

bool CcProtocol_Transmit(cc_dev_t dest, cc_cid_t cid, unsigned mid, 
                        uint8* data, uint16 len)
{
    return ccProtocol_TransmitInternal(dest, cid, mid, data, len, TRUE);
}

bool CcProtocol_TransmitNotification(cc_dev_t dest, cc_cid_t cid, unsigned mid, 
                                     uint8* data, uint16 len)
{
    return ccProtocol_TransmitInternal(dest, cid, mid, data, len, FALSE);
}

void CcProtocol_RegisterChannel(const cc_chan_config_t* config)
{
    cc_chan_config_t* cfg = ccProtocol_GetChannelConfig(config->cid);

    if (cfg)
    {
        cfg->cid = config->cid;
        cfg->tx_sts = config->tx_sts;
        cfg->rx_ind = config->rx_ind;
    }
    else
    {
        DEBUG_LOG_ERROR("CcProtocol_RegisterChannel unsupported channel enum:cc_cid_t:$d", config->cid);
    }
}

void CcProtocol_RegisterBroadcastResetFn(CcProtocol_Reset_fn reset_fn)
{
    cc_protocol_t* td = CcProtocol_GetTaskData();

    if (reset_fn)
    {
        td->reset_fn = reset_fn;
    }
}

bool CcProtocol_Disable(void)
{
    return ccProtocol_TransDisable();
}

bool CcProtocol_Enable(void)
{
    return ccProtocol_TransEnable();
}

bool CcProtocol_IsEnabled(void)
{
    return ccProtocol_TransIsEnabled();
}

void CcProtocol_ConfigureAsWakeupSource(void)
{
    cc_protocol_t* td = CcProtocol_GetTaskData();

    if (td->mode == CASECOMMS_MODE_EARBUD && td->trans == CASECOMMS_TRANS_SCHEME_B)
    {    
        PanicFalse(DormantConfigure(LED_WAKE_MASK, 1 << (CASE_COMMS_LED)));
        PanicFalse(DormantConfigure(LED_WAKE_INVERT_MASK, 1 << (CASE_COMMS_LED)));
    }
}


#endif /* INCLUDE_CASE_COMMS */
/*! @} End of group documentation */

/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    case_domain
\brief      Communication with the case using charger comms traps.
*/
/*! \addtogroup case_domain
@{
*/

#include "case.h"
#include "case_private.h"
#include "case_comms.h"
#include "case_comms_case_channel.h"

#include <logging.h>

#include <message.h>
#include <chargercomms.h>

#ifdef INCLUDE_CASE

/*! Definitions related to the charger/case comms packet format. */
/*! @{ */
#define CHARGERCOMMS_HEADER_OFFSET      (0)
#define CHARGERCOMMS_HEADER_LEN         (1)
#define CHARGERCOMMS_DEST_MASK          (0x30)
#define CHARGERCOMMS_DEST_BIT_OFFSET    (4)

#define CASECOMMS_HEADER_OFFSET         (1)
#define CASECOMMS_HEADER_LEN            (1)
#define CASECOMMS_CID_MASK              (0x70)
#define CASECOMMS_CID_BIT_OFFSET        (4)
#define CASECOMMS_MID_MASK              (0x0f)
#define CASECOMMS_MID_BIT_OFFSET        (0)

#define CASECOMMS_MAX_MSG_PAYLOAD       (13)
#define CASECOMMS_PAYLOAD_OFFSET        (2)
#define CASECOMMS_MAX_TX_MSG_SIZE       (CHARGERCOMMS_HEADER_LEN + CASECOMMS_HEADER_LEN + CASECOMMS_MAX_MSG_PAYLOAD)
/*! @} */

/*! Buffer in which to build outgoing case comms message. */
static uint8 casecomms_msg_buffer[CASECOMMS_MAX_TX_MSG_SIZE];

/*! CID of message currently being transmitted and not yet acknowledged. */ 
static case_comms_cid_t casecomms_cid_in_transmit = CASECOMMS_CID_INVALID;

/*! \brief Utility function to set the destination field of the charger comms header. */
static void case_ChargerCommsSetDest(uint8* charger_comms_header, case_comms_device_t dest)
{
    *charger_comms_header |= ((dest << CHARGERCOMMS_DEST_BIT_OFFSET) & CHARGERCOMMS_DEST_MASK);
}

/*! \brief Utility function to read Channel ID from a Case Comms header. */
static case_comms_cid_t case_CommsGetCID(uint8 ccomms_header)
{
    return ((ccomms_header & CASECOMMS_CID_MASK) >> CASECOMMS_CID_BIT_OFFSET);
}

/*! \brief Utility function to set Channel ID in a Case Comms header. */
static void case_CommsSetCID(uint8* ccomms_header, case_comms_cid_t cid)
{
    *ccomms_header |= ((cid << CASECOMMS_CID_BIT_OFFSET) & CASECOMMS_CID_MASK);
}

/*! \brief Utility function to read Message ID from a Case Comms header. */
static unsigned case_CommsGetMID(uint8 ccomms_header)
{
    return ((ccomms_header & CASECOMMS_MID_MASK) >> CASECOMMS_MID_BIT_OFFSET);
}

/*! \brief Utility function to set Message ID in a Case Comms header. */
static void case_CommsSetMID(uint8* ccomms_header, unsigned mid)
{
    *ccomms_header |= ((mid << CASECOMMS_MID_BIT_OFFSET) & CASECOMMS_MID_MASK);
}

void Case_CommsInit(void)
{
    /* currently only support charger comms as the case comms mechanism,
       register to receive incoming messages and status updates. */
    MessageChargerCommsTask(Case_GetTask());
}

void Case_CommsHandleMessageChargerCommsInd(const MessageChargerCommsInd* ind)
{
    case_comms_cid_t cid = case_CommsGetCID(ind->data[CASECOMMS_HEADER_OFFSET]);
    unsigned mid = case_CommsGetMID(ind->data[CASECOMMS_HEADER_OFFSET]);
    unsigned payload_length = ind->length - CHARGERCOMMS_HEADER_LEN - CASECOMMS_HEADER_LEN;

    DEBUG_LOG_INFO("Case_CommsHandleMessageChargerCommsInd pktlen:%d cid enum:case_comms_cid_t:%d mid:%d paylen:%d", ind->length, cid, mid, payload_length);

    switch (cid)
    {
        case CASECOMMS_CID_CASE:
            /* pass incoming message payload to case channel handler */
            Case_CommsCaseChannelHandleMessage(mid,
                                               ind->data + CHARGERCOMMS_HEADER_LEN + CASECOMMS_HEADER_LEN,
                                               payload_length);
            break;
        default:
            DEBUG_LOG_WARN("Case_CommsHandleMessageChargerCommsInd unsupported cid %d", cid);
            break;
    }

    /* message contains pointer to incoming data which needs to be freed */
    free(ind->data);
}

/*! \brief Convert charger comms status to case comms status. */
static case_comms_tx_status_t Case_CommsGetStatus(charger_comms_msg_status status)
{
    case_comms_tx_status_t sts = CASECOMMS_TX_SUCCESS;

    if (status != CHARGER_COMMS_MSG_SUCCESS)
    {
        sts = CASECOMMS_TX_FAIL;
    }

    return sts;
}

void Case_CommsHandleMessageChargerCommsStatus(const MessageChargerCommsStatus* status)
{
    DEBUG_LOG_INFO("Case_CommsHandleMessageChargerCommsStatus sts:%d", status->status);

    /* notify client status of message transmission */
    switch (casecomms_cid_in_transmit)
    {
        case CASECOMMS_CID_CASE:
            Case_CommsCaseChannelHandleTransmitStatus(Case_CommsGetStatus(status->status));
            break;
        case CASECOMMS_CID_INVALID:
            DEBUG_LOG_WARN("Case_CommsHandleMessageChargerCommsStatus recv unsolicited status %d", status->status);
            break;
        default:
            DEBUG_LOG_WARN("Case_CommsHandleMessageChargerCommsStatus recv status %d for unsupported cid %d", status->status, casecomms_cid_in_transmit);
            break;
    }

    casecomms_cid_in_transmit = CASECOMMS_CID_INVALID;
}

bool Case_CommsTransmit(case_comms_device_t dest, case_comms_cid_t cid, unsigned mid, 
                        uint8* data, uint16 len)
{
    uint16 length = 0;

    /* validate transmit request */
    if (dest != CASECOMMS_DEVICE_CASE)
    {
        DEBUG_LOG_WARN("Case_CommsTransmit unsupported destination %d", dest);
        return FALSE;
    }
    if (len > CASECOMMS_MAX_MSG_PAYLOAD)
    {
        DEBUG_LOG_WARN("Case_CommsTransmit message length too long %d maxlen is %d", len, CASECOMMS_MAX_MSG_PAYLOAD);
    }

    /* only support a single message at a time */
    if (casecomms_cid_in_transmit != CASECOMMS_CID_INVALID)
    {
        DEBUG_LOG_WARN("Case_CommsTransmit message already in transit");
        return FALSE;
    }

    /* build the message */
    memset(casecomms_msg_buffer, 0, CASECOMMS_MAX_TX_MSG_SIZE);
    case_ChargerCommsSetDest(&casecomms_msg_buffer[CHARGERCOMMS_HEADER_OFFSET], dest);
    case_CommsSetCID(&casecomms_msg_buffer[CASECOMMS_HEADER_OFFSET], cid);
    case_CommsSetMID(&casecomms_msg_buffer[CASECOMMS_HEADER_OFFSET], mid);
    memcpy(&casecomms_msg_buffer[CASECOMMS_PAYLOAD_OFFSET], data, len);

    /* calculate actual length of data being sent and transmit the message and record
     * the cid for blocking further TX and routing the status when it arrives */
    length = CHARGERCOMMS_HEADER_LEN + CASECOMMS_HEADER_LEN + len;
    casecomms_cid_in_transmit = cid;
    ChargerCommsTransmit(length, casecomms_msg_buffer);

    return TRUE;
}

#endif /* INCLUDE_CASE */
/*! @} End of group documentation */

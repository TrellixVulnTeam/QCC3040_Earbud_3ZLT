/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    case_comms
\brief      Transmit and receive handling for Scheme A transport.
*/
/*! \addtogroup case_comms
@{
*/

#ifdef INCLUDE_CASE_COMMS
#ifdef HAVE_CC_TRANS_SCHEME_A

#include "cc_protocol.h"
#include "cc_protocol_private.h"
#include "cc_protocol_trans_schemeA.h"

#include <logging.h>

#include <message.h>
#include <chargercomms.h>
#include <stdlib.h>
#include <panic.h>

/* Utility function to set the destination field of the charger comms header. */
static void ccProtocol_ChargerCommsSetDest(uint8* charger_comms_header, cc_dev_t dest)
{
    *charger_comms_header |= ((dest << SCHEME_A_CHARGERCOMMS_DEST_BIT_OFFSET) & SCHEME_A_CHARGERCOMMS_DEST_MASK);
}

static void ccProtocol_TransSchemeAResetCidMid(void)
{
    cc_protocol_t* td = CcProtocol_GetTaskData();
    td->scheme_data.cid_in_transit = CASECOMMS_CID_INVALID;
    td->scheme_data.mid_in_transit = 0;
}

bool ccProtocol_TransSchemeATransmit(cc_dev_t dest, cc_cid_t cid, unsigned mid, 
                        uint8* data, uint16 len)
{
    cc_protocol_t* td = CcProtocol_GetTaskData();

    uint16 length = 0;

    /* validate transmit request */
    if (dest != CASECOMMS_DEVICE_CASE)
    {
        DEBUG_LOG_WARN("ccProtocol_TransSchemeATransmit unsupported destination enum:cc_dev_t:%d", dest);
        return FALSE;
    }
    if (len > SCHEME_A_CASECOMMS_MAX_MSG_PAYLOAD)
    {
        DEBUG_LOG_ERROR("ccProtocol_TransSchemeATransmit message length too long %d maxlen is %d", len, SCHEME_A_CASECOMMS_MAX_MSG_PAYLOAD);
        Panic();
    }
    if (td->scheme_data.cid_in_transit != CASECOMMS_CID_INVALID)
    {
        DEBUG_LOG_ERROR("ccProtocol_TransSchemeATransmit already msg in transit %d", td->scheme_data.cid_in_transit);
        return FALSE;
    }

    /* build the message */
    memset(td->scheme_data.casecomms_msg_buffer, 0, SCHEME_A_CASECOMMS_MAX_TX_MSG_SIZE);
    ccProtocol_ChargerCommsSetDest(&td->scheme_data.casecomms_msg_buffer[SCHEME_A_CHARGERCOMMS_HEADER_OFFSET], dest);
    ccProtocol_CaseCommsSetCID(&td->scheme_data.casecomms_msg_buffer[SCHEME_A_CASECOMMS_HEADER_OFFSET], cid);
    ccProtocol_CaseCommsSetMID(&td->scheme_data.casecomms_msg_buffer[SCHEME_A_CASECOMMS_HEADER_OFFSET], mid);
    memcpy(&td->scheme_data.casecomms_msg_buffer[SCHEME_A_CASECOMMS_PAYLOAD_OFFSET], data, len);

    /* calculate actual length of data being sent and transmit the message */
    length = SCHEME_A_CHARGERCOMMS_HEADER_LEN + CASECOMMS_HEADER_LEN + len;
            
    DEBUG_LOG_VERBOSE("ccProtocol_TransSchemeATransmit enum:cc_dev_t:%d enum:cc_cid_t:%d mid:%d len:%d", dest, cid, mid, length);
    
    ChargerCommsTransmit(length, td->scheme_data.casecomms_msg_buffer);

    /* sending data now and scheme A only supports a single message in transmit
       remember the CID and MID for status reporting to clients. */
    td->scheme_data.cid_in_transit = cid;
    td->scheme_data.mid_in_transit = mid;

    return TRUE;
}

void ccProtocol_TransSchemeAReceive(const MessageChargerCommsInd* ind)
{
    cc_cid_t cid = ccProtocol_CaseCommsGetCID(ind->data[SCHEME_A_CASECOMMS_HEADER_OFFSET]);
    unsigned mid = ccProtocol_CaseCommsGetMID(ind->data[SCHEME_A_CASECOMMS_HEADER_OFFSET]);
    unsigned payload_length = ind->length - SCHEME_A_CHARGERCOMMS_HEADER_LEN - CASECOMMS_HEADER_LEN;

    DEBUG_LOG_VERBOSE("ccProtocol_TransSchemeAReceive enum:cc_dev_t:%d enum:cc_cid_t:%d mid:%d len:%d", CASECOMMS_DEVICE_CASE, cid, mid, payload_length);

    /* pass packet to client, strip chargercomm and case comms headers */
    ccProtocol_SendRXPacketToClient(ind->data + SCHEME_A_CHARGERCOMMS_HEADER_LEN + CASECOMMS_HEADER_LEN,
                                    payload_length, cid, mid, CASECOMMS_DEVICE_CASE);

    /* receive handling completed, reset CID and MID, which will permit further transmit */
    ccProtocol_TransSchemeAResetCidMid();

    /* message contains pointer to incoming data which needs to be freed */
    free(ind->data);
}

void ccProtocol_TransSchemeASetup(void)
{
    ccProtocol_TransSchemeAResetCidMid();
}

cc_cid_t ccProtocol_TransSchemeAGetCidInTransmit(void)
{
    cc_protocol_t* td = CcProtocol_GetTaskData();
    return td->scheme_data.cid_in_transit;
}

unsigned ccProtocol_TransSchemeAGetMidInTransmit(void)
{
    cc_protocol_t* td = CcProtocol_GetTaskData();
    return td->scheme_data.mid_in_transit;
}

#endif /* HAVE_CC_TRANS_SCHEME_A */
#endif /* INCLUDE_CASE_COMMS */
/*! @} End of group documentation */

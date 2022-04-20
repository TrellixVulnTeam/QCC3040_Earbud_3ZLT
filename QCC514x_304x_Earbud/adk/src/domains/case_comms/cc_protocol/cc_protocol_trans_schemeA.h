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

#ifndef CC_PROTOCOL_TRANS_SCHEME_A_H
#define CC_PROTOCOL_TRANS_SCHEME_A_H

#ifdef INCLUDE_CASE_COMMS

#ifdef HAVE_CC_TRANS_SCHEME_A
#include "cc_protocol.h"
#include "cc_protocol_private.h"

#include <message.h>

/*! Definitions related to the low speed charger comms packet format. */
/*! @{ */
#define SCHEME_A_CHARGERCOMMS_HEADER_OFFSET      (0)
#define SCHEME_A_CHARGERCOMMS_HEADER_LEN         (1)
#define SCHEME_A_CHARGERCOMMS_DEST_MASK          (0x30)
#define SCHEME_A_CHARGERCOMMS_DEST_BIT_OFFSET    (4)
#define SCHEME_A_CASECOMMS_HEADER_OFFSET         (1)
#define SCHEME_A_CASECOMMS_MAX_MSG_PAYLOAD       (13)
#define SCHEME_A_CASECOMMS_PAYLOAD_OFFSET        (2)
#define SCHEME_A_CASECOMMS_MAX_TX_MSG_SIZE       (SCHEME_A_CHARGERCOMMS_HEADER_LEN + CASECOMMS_HEADER_LEN + SCHEME_A_CASECOMMS_MAX_MSG_PAYLOAD)
/*! @} */

/*! Scheme A transport data. */
typedef struct
{
    /*! Buffer in which to build outgoing case comms message. */
    uint8 casecomms_msg_buffer[SCHEME_A_CASECOMMS_MAX_TX_MSG_SIZE];

    /*! If not CASECOMMS_CID_INVALID, indicates the CID of a message still waiting ack. */
    cc_cid_t cid_in_transit:4;

    /*! Indicates the NID of a message still waiting ack. */
    unsigned mid_in_transit:4;
} transport_scheme_data;

/*! \brief Transmit a packet over a Scheme A transport.
    \param dest Case comms destination device ID.
    \param cid Case comms channel ID.
    \param mid Case comms message ID.
    \param data Pointer to the packet.
    \param len Length of data in bytes.
    \return bool TRUE packet accepted for transmission, FALSE otherwise.

    \note A return value of TRUE does not indicate receipt of the packet
          by the destination device, caller must wait for acknowledgement
          via CcProtocol_TxStatus_fn callback.
*/
bool ccProtocol_TransSchemeATransmit(cc_dev_t dest, cc_cid_t cid, unsigned mid, 
                                uint8* data, uint16 len);

/*! Define this transport's transmit function as the common transmit symbol for cc_protocol.c */
#define ccProtocol_TransTransmit ccProtocol_TransSchemeATransmit

/*! \brief Handle receipt of a packet over a Scheme A transport. 
    \param ind Indication of incoming packet.
*/
void ccProtocol_TransSchemeAReceive(const MessageChargerCommsInd* ind);

/*! \brief Initialise the Scheme A transport.
*/
void ccProtocol_TransSchemeASetup(void);
/*! Define this transport's setup function as the common setup symbol for cc_protocol.c */
#define ccProtocol_TransSetup ccProtocol_TransSchemeASetup

/*! \brief Get the CID marked as in transit.
    \return cc_cid_t Case comms channel ID.
*/
cc_cid_t ccProtocol_TransSchemeAGetCidInTransmit(void);

/*! \brief Get the MID marked as in transit.
    \return unsigned Case comms message ID.
*/
unsigned ccProtocol_TransSchemeAGetMidInTransmit(void);

/* Scheme A transport does not require reset or eolling, report that when queried. */
#define ccProtocol_TransRequiresPolling() (FALSE)
#define ccProtocol_TransRequiresReset() (FALSE)

/* Scheme A transport cannot be enabled/disabled.
   Always report success for Enable or IsEnabled and failure for any attempt
   to disable. */
#define ccProtocol_TransIsEnabled() (TRUE)
#define ccProtocol_TransEnable()    (TRUE)
#define ccProtocol_TransDisable()   (FALSE)

#else

#define ccProtocol_TransSchemeATransmit(_dest, _cid, _mid, _data, _len) (FALSE)
#define ccProtocol_TransSchemeAReceive(_ind)                            (UNUSED(_ind))
#define ccProtocol_TransSchemeASetup()
#define ccProtocol_TransSchemeAGetCidInTransmit() (CASECOMMS_CID_INVALID)
#define ccProtocol_TransSchemeAGetMidInTransmit() (0)

#endif /* HAVE_CC_TRANS_SCHEME_A */
#endif /* INCLUDE_CASE_COMMS */
#endif /* CC_PROTOCOL_TRANS_SCHEME_A_H */
/*! @} End of group documentation */

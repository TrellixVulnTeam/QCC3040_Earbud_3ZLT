/*!
\copyright  Copyright (c) 2019-2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\defgroup   handover_profile Handover Profile
\ingroup    profiles
\brief      Implementation of the protocol between earbuds during handover.
*/
#ifndef HANDOVER_PROTOCOL_H_
#define HANDOVER_PROTOCOL_H_

#ifdef INCLUDE_MIRRORING

/*! \brief Handover Protocol Opcodes. */
typedef enum
{
    /*! Handover Protocol Start request */
    HANDOVER_PROTOCOL_START_REQ,
    /*! Handover Protocol Start confirmation */
    HANDOVER_PROTOCOL_START_CFM,
    /*! Handover Protocol Cancel indication */
    HANDOVER_PROTOCOL_CANCEL_IND,
    /*! Handover Protocol Unmarshal P1 confirmation */
    HANDOVER_PROTOCOL_UNMARSHAL_P1_CFM,
    /*! Unused Message ID */
    HANDOVER_PROTOCOL_UNUSED,
    /*! Handover Protocol firmware version indication */
    HANDOVER_PROTOCOL_VERSION_IND,
    /*! Handover Protocol Marshal data */
    HANDOVER_PROTOCOL_MARSHAL_DATA=0xAA,
    /*! This is used by secondary after processing a handover start request,
        to wait for further messages from Primary. */
    HANDOVER_PROTOCOL_ANY_MSG_ID=0xFF
} handover_protocol_opcode_t;

/*! For opcode HANDOVER_PROTOCOL_MARSHAL_DATA, the tag indicating the type of marshal data */
typedef enum
{
    HANDOVER_MARSHAL_BT_STACK_TAG = 0xAA,
    HANDOVER_MARSHAL_APPSP1_TAG = 0xEE,
} handover_protocol_marshal_tag_t;

/*! Sent from secondary to primary after connection. Allows the primary to
    check if the firmware versions match */
typedef struct
{
    /*! AppsP0 firmware version */
    uint32 appsp0_version;
    /*! AppsP1 firmware version */
    uint32 appsp1_version;
    /*! BTSS ROM firmware version */
    uint32 btss_rom_version;
    /*! BTSS patch firmware version */
    uint32 btss_patch_version;

} HANDOVER_PROTOCOL_VERSION_IND_T;

/*! Handover start sequest message sent from primary to secondary to start the handover procedure */
typedef struct
{
    /*! Incremented each time the message is sent. */
    uint8      session_id;
    /*! Peer signalling transmit sequence number used to check no messages in flight. */
    uint8      last_tx_seq;
    /*! Peer signalling receive sequence number used to check no messages in flight. */
    uint8      last_rx_seq;
    /*! Primary earbud's mirror state used to check mirror states are identical */
    uint16     mirror_state;
    /*! The number of devices being handed over */
    uint8      number_of_devices;
    /*! List of device addresses */
    tp_bdaddr  address[1];
} HANDOVER_PROTOCOL_START_REQ_T;

#define sizeof_HANDOVER_PROTOCOL_START_REQ_T(number_of_devices) (sizeof(HANDOVER_PROTOCOL_START_REQ_T) + (((number_of_devices) - 1) * sizeof(tp_bdaddr)))

/*! Handover start confirmation message sent from secondary to primary */
typedef struct
{
    /*! The session_id received in the HANDOVER_PROTOCOL_START_REQ_T */
    uint8   session_id;
    /*! The status */
    handover_profile_status_t status;
} HANDOVER_PROTOCOL_START_CFM_T;

/*! \brief Send version information message to peer.
    \return Status.
*/
handover_profile_status_t handoverProtocol_SendVersionInd(void);

/*! \brief Send handover protocol start request message to peer.
    \return Status.
*/
handover_profile_status_t handoverProtocol_SendStartReq(void);

/*! \brief Send handover protocol cancel indication message to peer.
    \return Status.
*/
handover_profile_status_t handoverProtocol_SendCancelInd(void);

handover_profile_status_t handoverProtocol_SendP1MarshalData(void);

/*! \brief Send P0 marshal data to peer.
    \param focused Selects whether to send focused device data or non-focused device data.
    \return Status.
*/
handover_profile_status_t handoverProtocol_SendBtStackMarshalData(bool focused);


/*! \brief Wait for handover start confirm message from peer.
    \return TRUE if valid confirm is received, FALSE otherwise.
*/
handover_profile_status_t handoverProtocol_WaitForStartCfm(void);

/*! \brief Wait for unmarshal P1 confirm message from peer.
    \return Status.
*/
handover_profile_status_t handoverProtocol_WaitForUnmarshalP1Cfm(void);


void handoverProtocol_HandleMessage(Source source);

#endif
#endif

/*!
\Copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    case_comms
\brief      Interface to communication with the case using charger comms traps.
*/
/*! \addtogroup case_comms
@{
*/

#ifndef CC_PROTOCOL_H
#define CC_PROTOCOL_H

#ifdef INCLUDE_CASE_COMMS

/*! \brief Types of transport over which to operate case comms. */
typedef enum
{
    /*! Original low speed transport. */
    CASECOMMS_TRANS_SCHEME_A,

    /*! High speed single wire UART transport. */
    CASECOMMS_TRANS_SCHEME_B,

    /*! Plain UART for testing. */
    CASECOMMS_TRANS_TEST_UART
} cc_trans_t;

/* Enforce during compilation that a transport must be defined, and define a commmon symbol
   for the selected transport, used in CcProtocol_Init calls.
*/
#if defined(HAVE_CC_TRANS_SCHEME_A)
#define CC_TRANSPORT CASECOMMS_TRANS_SCHEME_A
#elif defined(HAVE_CC_TRANS_SCHEME_B)
#define CC_TRANSPORT CASECOMMS_TRANS_SCHEME_B
#elif defined(HAVE_CC_TRANS_TEST_UART)
#define CC_TRANSPORT CASECOMMS_TRANS_TEST_UART
#else
#error "No case comms transport defined, must define one of HAVE_CC_TRANS_SCHEME_A, HAVE_CC_TRANS_SCHEME_B or HAVE_CC_TRANS_TEST_UART"
#endif

/*! \brief Channel IDs used by components to communicate over Case Comms. 
    \note These values are used in the protocol with the case
          and must remain in sync with case software.
*/
typedef enum
{
    /*! Status information from the case. */
    CASECOMMS_CID_CASE = 0x0,

    /*! Device Test Service channel. */
    CASECOMMS_CID_DTS = 0x1,

    /*! Channel available for customer use. */
    CASECOMMS_CID_CUSTOMER = 0x2,

    /*! Test channel, used by application test APIs. */
    CASECOMMS_CID_TEST = 0x3,

    /*! Number of channels defined. */
    CASECOMMS_CID_MAX,

    /*! Invalid channel ID, used in initialisation and when no specific CID
        is required. */
    CASECOMMS_CID_INVALID = 0xF
} cc_cid_t;

/*! Result of call to #Case_CommsTransmit().
*/
typedef enum
{
    /*! Message successfully received by destination.
        An acknowledgement from the destination was received. */
    CASECOMMS_TX_SUCCESS,

    /*! Message transmit failed, no successful acknowledgement received from 
        destination. Destination could have received the message and the 
        acknowledgement was lost or corrupted. */
    CASECOMMS_TX_FAIL,

    /*! Message transmit failed, no response from destination. */
    CASECOMMS_TX_TIMEOUT,

    /*! Message transmit failed, message was flushed from transmit buffer by
        transmit or receipt of a broadcast message. */
    CASECOMMS_TX_BROADCAST_FLUSHED,

    /*! Unknown status, used as initialisation value. */
    CASECOMMS_STATUS_UNKNOWN,
} cc_tx_status_t;

/*! \brief Devices participating in the Case Comms network.
    \note These values are used in the protocol with the case
          and must remain in sync with case software.
 */
typedef enum
{
    /*! Case. */
    CASECOMMS_DEVICE_CASE      = 0x0,

    /*! Right Earbud. */
    CASECOMMS_DEVICE_RIGHT_EB  = 0x1,

    /*! Left Earbud. */
    CASECOMMS_DEVICE_LEFT_EB   = 0x2,

    /*! Broadcast to both Left and Right Earbud. */
    CASECOMMS_DEVICE_BROADCAST = 0x3,
} cc_dev_t;

/*! \brief Types of mode in which the case comms protocol may be operating.
*/
typedef enum
{
    /*! Earbud mode for use on Earbud devices. */
    CASECOMMS_MODE_EARBUD,

    /*! Case mode for use on Case devices and communicating with 2 devices
        in Earbud mode.
        \note This mode enables additional handling:-
            - transmit polls when responses are pending from the Earbuds
            - reset the link with a broadcast message in transmit failure scenarios */
    CASECOMMS_MODE_CASE
} cc_mode_t;

/*! Callback declaration to be provided by case comms clients for receiving TX status. */
typedef void (*CcProtocol_TxStatus_fn)(cc_tx_status_t status, unsigned mid);

/*! Callback declaration to be provided by case comms clients for receiving incoming messages. */
typedef void (*CcProtocol_RxInd_fn)(unsigned mid, const uint8* msg, unsigned len, cc_dev_t source_dev);

/*! Callback declaration for a function to be called to send data with the broadcast seqnum reset. */
typedef void (*CcProtocol_Reset_fn)(void);

/*! \brief Channel configuration to be supplied by case commms channel clients. */
typedef struct
{
    /*! TX status callback. */
    CcProtocol_TxStatus_fn tx_sts;

    /*! RX indication callback. */
    CcProtocol_RxInd_fn rx_ind;

    /*! Case comms channel ID being registered. */
    cc_cid_t cid;

    /*! number of outstanding messages requiring poll to get response from left earbud. */
    uint8 left_outstanding_response_count;

    /*! number of outstanding messages requiring poll to get response from right earbud. */
    uint8 right_outstanding_response_count;
} cc_chan_config_t;

/*! \brief Initialise comms with the case.
*/
void CcProtocol_Init(cc_mode_t mode, cc_trans_t trans);

/*! \brief Register client handler for a case comms channel.
 
    \param config Pointer to channel ID and callback configuration.
*/
void CcProtocol_RegisterChannel(const cc_chan_config_t* config);

/*! \brief Transmit a message over case comms, where a response message is expected.

    \param dest Destination device.
    \param cid Case comms channel on which to transmit.
    \param mid Case comms message ID of the packet.
    \param data Pointer to the packet payload.
    \param len Length in bytes of the payload.

    \return bool TRUE packet will be transmitted.
                 FALSE packet can not be transmitted.

    \note Return value of TRUE only indicates the packet has been accepted
          for transmission. Client must await CcProtocol_TxStatus_fn callback
          for indication of success/fail receipt by destination.

    \note The response message expected by using this API is a new message sent
          by the remote device, not the ACK/NAK received by clients via the
          CcProtocol_TxStatus_fn callback, which is a transmit status indication.
          Using this API instructs the case comms protocol to poll the remote
          device if required to receive the response message.
*/
bool CcProtocol_Transmit(cc_dev_t dest, cc_cid_t cid, unsigned mid, 
                         uint8* data, uint16 len);

/*! \brief Transmit a message over case comms and no response message is expected.

    \param dest Destination device.
    \param cid Case comms channel on which to transmit.
    \param mid Case comms message ID of the packet.
    \param data Pointer to the packet payload.
    \param len Length in bytes of the payload.

    \return bool TRUE packet will be transmitted.
                 FALSE packet can not be transmitted.

    \note Return value of TRUE only indicates the packet has been accepted
          for transmission. Client must await CcProtocol_TxStatus_fn callback
          for indication of success/fail receipt by destination.

    \note Clients will receive transmit status indication via the CcProtocol_TxStatus_fn
          callback when using this API. 
          Using this API instructs the case comms protocol that no polling is
          required as the client does not expect a response message from the
          remote device.
*/
bool CcProtocol_TransmitNotification(cc_dev_t dest, cc_cid_t cid, unsigned mid, 
                                     uint8* data, uint16 len);

/*! \brief Register a callback to use when reset with Broadcast message required. 
 
    \note The Case comms protocol detects some types of sequence number failure
          modes and sends a broadcast message to reset the transport. This also
          flushes any queued messages on both Case and Earbuds.
          
          If no callback is registered, case comms will use an empty broadcast
          message, which just resets the transport sequence numbers but provides
          no additional information to the Earbuds.

          Clients can register a function to generate a broadcast message with
          content to piggyback on the broadcast reset. Broadcast messages are
          received by both earbuds. Client reset function must call
          CcProtocol_Transmit() with destination of CASECOMMS_DEVICE_BROADCAST.
          No return value is required. See ccProtocol_TransmitBroadcastReset()
          for an example.
*/
void CcProtocol_RegisterBroadcastResetFn(CcProtocol_Reset_fn reset_fn);

/*! \brief Disable case comms.
    \return bool TRUE Case comms is disabled.
                 FALSE Case comms is not disabled.

    \note This API is only supported for Scheme B transport configuration.

    When disabled, the UART used by the scheme B transport is available to be
    reconfigured and used by the application.
    
    Using this API will cause any data in the UART and stream to be discarded.
    Any case comms transmissions to this device whilst disabled are ignored.
*/
bool CcProtocol_Disable(void);

/*! \brief Enable case comms.
    \return bool TRUE Case comms is enabled and available for use.
                 FALSE Case comms is not enabled.

    \note This API is only supported for Scheme B transport configuration.

    This API will attempt to reacquire the UART and chargercomms UART stream
    for use by case comms. The application must ensure any use of the UART
    has been completely stopped and the UART stream released, or this API
    will fail.
*/
bool CcProtocol_Enable(void);

/*! \brief Determine if case comms is enabled.
    \return bool TRUE if case comms is enabled.
                 FALSE otherwise.

    \note Will always return TRUE for Scheme A and test UART transpots
          which do not support enable/disable.
*/
bool CcProtocol_IsEnabled(void);

/*! \brief Allow the app to configure charger comms as a dormant wakeup source, if necessary.
 * 
 * The hardware automatically wakes the chip from dormant if there is a change on VCHG.
 * Some devices use LED pads for the charger comms interface. This function should be used
 * to configure the dormant module for such scenarios.
*/
void CcProtocol_ConfigureAsWakeupSource(void);
#else
#define CcProtocol_ConfigureAsWakeupSource()

#endif /* INCLUDE_CASE_COMMS */
#endif /* CC_PROTOCOL_H */
/*! @} End of group documentation */

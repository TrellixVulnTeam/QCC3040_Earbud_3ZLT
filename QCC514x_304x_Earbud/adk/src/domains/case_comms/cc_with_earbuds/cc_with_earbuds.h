/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   case_comms Case Communications Domain
\ingroup    domains
\brief      
@{
*/

#ifndef CC_WITH_EARBUDS_H
#define CC_WITH_EARBUDS_H

#if defined(INCLUDE_CASE_COMMS) && defined (HAVE_CC_MODE_CASE)

#include <cc_protocol.h>

#include <task_list.h>

#include <message.h>

/*! Peer pairing state of an Earbud. */
typedef enum
{
    PP_STATE_UNKNOWN,
    PP_STATE_NOT_PAIRED,
    PP_STATE_PAIRED
} pp_state_t;

/*! Initialise the case comms with earbuds component. */
bool CcWithEarbuds_Init(Task init_task);

/*! \brief Register client task to receive Earbud state messages.
*/
void CcWithEarbuds_RegisterClient(Task client_task);

/*! \brief Unregister client task to stop receiving Earbud state messages.
*/
void CcWithEarbuds_UnregisterClient(Task client_task);

/*! \brief Handle receipt of non-info Earbud Status message. */
void CcWithEarbuds_EarbudStatusRx(cc_dev_t source, uint8 battery_state, bool peer_paired);

/*! \brief Handle receipt of BT address message. */
void CcWithEarbuds_EarbudBtAddressRx(const bdaddr* addr, cc_dev_t source);

/*! \brief Transmit loopback message(s) to an Earbud.
    \param dest Casecomms device address.
    \param data Pointer to buffer holding data to loopback.
    \param len Length of data in bytes.
    \param iterations Number of loopback messages to send.

    \note Only the left or right earbud addresses are valid destinations.

    Transmission of the loopback message(s) is marked as a timestamped event
    and when the same number of loopback messages are received from the earbud
    another timestamp is marked. The difference between these two timestamps is
    written to the log.
*/
void CcWithEarbuds_LoopbackTx(cc_dev_t dest, uint8* data, unsigned len, unsigned iterations);

/*! \brief Handle a loopback message from an Earbud.
    \param source Casecomms device address.
    \param data Pointer to buffer holding data to loopback.
    \param len Length of data in bytes.
*/
void CcWithEarbuds_LoopbackRx(cc_dev_t source, uint8* data, unsigned len);

/*| \brief Handle transmit status for messages sent to an Earbud.
    \param status CASECOMMS_TX_SUCCESS destination received message.
                  CASECOMMS_TX_FAIL destination did not receive message.
    \param mid Message ID corresponding to status.

    \note Failure could be a result of both failure to transmit the
          message, or it was transmitted, but not acknowledged by the
          destination Earbud.
*/
void CcWithEarbuds_TransmitStatusRx(cc_tx_status_t status, unsigned mid);

/*! \brief Send indication to case about state of request to peer pair.
    \param source Casecomms device address.
    \param peer_pairing_started TRUE if #source Earbud has accepted command and started peer pairing
                                FALSE otherwise.
*/
void CcWithEarbuds_PeerPairResponseRx(cc_dev_t source, bool peer_pairing_started);

/*! \brief Send indication to case about state of request to enter shipping mode.
    \param source Casecomms device address.
    \param shipping_mode_accepted TRUE if #source Earbud has accepted command and will enter shipping mode when VCHG is removed.
                                  FALSE otherwise.
*/
void CcWithEarbuds_ShippingModeResponseRx(cc_dev_t source, bool shipping_mode_accepted);

#else /* defined(INCLUDE_CASE_COMMS) && defined (HAVE_CC_MODE_CASE) */

#define CcWithEarbuds_RegisterClient(_client_task)      (UNUSED(_client_task))
#define CcWithEarbuds_UnregisterClient(_client_task)    (UNUSED(_client_task))
#define CcWithEarbuds_EarbudStatusRx(_source, _battery_state, _peer_paired) (UNUSED(_source), UNUSED(_battery_state), UNUSED(_peer_paired))
#define CcWithEarbuds_EarbudBtAddressRx(_addr, _source)   (UNUSED(_addr), UNUSED(_source))
#define CcWithEarbuds_LoopbackTx(_dest, _data, _len, _iterations)    (UNUSED(_dest), UNUSED(_data), UNUSED(_len), (UNUSED(_iterations))
#define CcWithEarbuds_LoopbackRx(_source, _data, _len)  (UNUSED(_source), UNUSED(_data), UNUSED(_len))
#define CcWithEarbuds_TransmitStatusRx(_status, _mid)   (UNUSED(_status), UNUSED(_mid))
#define CcWithEarbuds_PeerPairResponseRx(_source, _peer_pairing_started)    (UNUSED(_source), UNUSED(_peer_pairing_started))
#define CcWithEarbuds_ShippingModeResponseRx(_source, _shipping_mode_accepted)    (UNUSED(_source), UNUSED(_shipping_mode_accepted))

#endif /* defined(INCLUDE_CASE_COMMS) && defined (HAVE_CC_MODE_CASE) */
#endif /* CC_WITH_EARBUDS_H */
/*! @} End of group documentation */

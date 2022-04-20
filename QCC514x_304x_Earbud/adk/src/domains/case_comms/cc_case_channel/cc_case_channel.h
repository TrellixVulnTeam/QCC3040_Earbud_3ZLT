/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    case_comms
\brief      Case channel interface.
*/
/*! \addtogroup case_comms
@{
*/

#ifdef INCLUDE_CASE_COMMS

#include "cc_protocol.h"

#define BATTERY_STATE_CHARGING_BIT      (0x80)
#define BATTERY_STATE_SET_CHARGING(x)   ((x) |= BATTERY_STATE_CHARGING_BIT)
#define BATTERY_STATE_CLEAR_CHARGING(x) ((x) &= ~BATTERY_STATE_CHARGING_BIT)
#define BATTERY_STATE_IS_CHARGING(x)    (((x) & BATTERY_STATE_CHARGING_BIT) == BATTERY_STATE_CHARGING_BIT)

#define BATTERY_STATE_PERCENTAGE(x)     ((x) & 0x7F)

/*! Configuration of a CASE_CHANNEL_MID_CASE_STATUS message. */
typedef struct
{
    /*! Set to TRUE to only send the first byte and omit the
        battery state bytes. */
    bool short_form:1;

    /*! @{ lid and charger connectivity - included in short form. */
    bool lid_open:1;
    bool charger_connected:1;
    /* @} */

    /*! @{ Battery levels - only included if short_form is FALSE. */
    uint8 case_battery_state;
    uint8 left_earbud_battery_state;
    uint8 right_earbud_battery_state;
    /* @} */
} case_status_config_t;

/*! \brief Initialise the Case Channel.
 
    Registers case channel with CcProtocol as the handler for
    CASECOMMS_CID_CASE.
*/
void CcCaseChannel_Init(void);

/*! \brief Send a CASE_CHANNEL_MID_EARBUD_STATUS_REQ to a device.
    \param dest Earbud destination.
    \return bool TRUE if packet was accepted for TX to the Earbud, FALSE otherwise. 

    \note Valid destinations are CASECOMMS_DEVICE_LEFT_EB or CASECOMMS_DEVICE_RIGHT_EB.
*/ 
bool CcCaseChannel_EarbudStatusReqTx(cc_dev_t dest);

/*! \brief Send a CASE_CHANNEL_MID_CASE_STATUS to device(s).
    \param dest single Earbud or broadcast to both Earbuds.
    \return bool TRUE if packet was accepted for TX to the Earbud, FALSE otherwise. 

    \note Valid destinations are CASECOMMS_DEVICE_LEFT_EB, CASECOMMS_DEVICE_RIGHT_EB
          or CASECOMMS_DEVICE_BROADCAST
*/
bool CcCaseChannel_CaseStatusTx(cc_dev_t dest, case_status_config_t* config);

/*! \brief Command an Earbud to reboot.
    \param factory_reset TRUE perform factory reset and reboot, otherwise just a reboot.
    \return bool TRUE if packet was accepted for TX to the Earbud, FALSE otherwise. 

    \note Factory reset of Earbuds is not currently supported.
          The message is sent but Earbuds will log a warning only.
*/
bool CcCaseChannel_EarbudResetTx(cc_dev_t dest, bool factory_reset);

/*! \brief Get the BT address of an Earbud.
    \param dest Left or right earbud destinations only are valid.
    \return bool TRUE if packet was accepted for TX to the Earbud, FALSE otherwise. 
*/
bool CcCaseChannel_EarbudBtAddressInfoReqTx(cc_dev_t dest);

/*! \brief Transmit a loopback message.
    \param dest Left or right earbud destinations only are valid.
    \param data Pointer to buffer containing loopback contents.
    \param len Length of data.
*/
bool CcCaseChannel_LoopbackTx(cc_dev_t dest, uint8* data, unsigned len);

/*! \brief Transmit a peer pair message.
    \param dest Left or right earbud destinations only are valid.
    \param data Pointer to BT address of earbud to peer pair with.
    \return bool TRUE if packet was accepted for TX to the Earbud, FALSE otherwise. 

    \note This command is for use on the case only, and will result in
          PeerPairing_PeerPairToAddress() being called on the Earbud.

          The case will receive confirmation the message was received,
          however there are circumstances when the Earbud cannot start
          peer pairing (see documentation for PeerPairing_PeerPairToAddress).
          An Earbud will send a command response indicating if peer pairing
          will be performed, received by the case via CcWithEarbuds_PeerPairResponseRx
          callback.

          Peer pairing completion will be indicated by change in the peer
          pairing state received in Earbud Status messages.
*/
bool CcCaseChannel_PeerPairCmdTx(cc_dev_t dest, bdaddr* addr);

/*! \brief Send an Earbud Command Response message to the Peer Pair command.
    \param bool cmd_accepted TRUE Earbud able to peer pair and process started.
                             FALSE otherwise.
    \return bool TRUE if packet was accepted for TX, FALSE otherwise. 

    \note No destination is required, as command responses are only valid
          for sending to the case.

    \note Expected usage is on the Earbuds only.
*/
bool CcCaseChannel_PeerPairCmdRespTx(bool cmd_accepted);

/*! \brief Send Earbud Command to enter shipping mode.
    \param dest Left or right earbud destinations only are valid.
    \return bool TRUE if packet was accepted for TX to the Earbud, FALSE otherwise. 

    \note Shipping Mode is the dormant power state, with external sensors powered off.
          
          This command for use on the case side only.
          
          The case will receive confirmation the message was received by
          #dest earbud, however it must wait for the Command Response
          message from the Earbud with confirmation that it will enter
          shipping mode.

          The Earbud does not immediately enter shipping mode, having
          received this command and returned a response that the command
          has been accepted, the Earbud will wait for VCHG to be removed
          (charger disconnected) and then enter shipping mode. 

          If the Earbud is taken out of the case before VCHG is removed,
          it will cancel the pending shipping mode, and take no action on
          subsequent VCHG removal. The case must send another shipping mode 
          command to restart the process.
*/
bool CcCaseChannel_ShippingModeCmdTx(cc_dev_t dest);

/*! \brief Send an Earbud Command Response message to the Shipping Mode command.
    \param cmd_accepted TRUE if Earbud will enter shipping mode when VCHG removed.
                        FALSE otherwise.
    \return bool TRUE if packet was accepted for TX to the Earbud, FALSE otherwise. 

    \note No destination is required, as command responses are only valid
          for sending to the case.

          note Expected usage is on the Earbuds only.
*/
bool CcCaseChannel_ShippingModeCmdRespTx(bool cmd_accepted);

#endif /* INCLUDE_CASE_COMMS */
/*! @} End of group documentation */

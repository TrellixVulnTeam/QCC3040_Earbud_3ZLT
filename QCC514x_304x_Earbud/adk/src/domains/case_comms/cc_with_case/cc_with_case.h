/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   case_comms Case Communications Domain
\ingroup    domains
\brief      Public interface to the case domain, managing interactions with a product case.

This component uses charger communications (chargercomms) transport(s) provided by lower layers to facilitate messaging between applications and their case.

Chargercomms is a service provided by the Curator and Apps P0 subsystems and accessed via the Apps P0 trap interface, enabling transmission and receipt of messages over the charger.
A Case Communications (casecomms) protocol is overlaid on chargercomms, which is a transport independent layer over which application specific communication channels can be multiplexed.

This case domain component implements the casecomms protocol layer and the Case Info Channel and provides a high level interface to applications and ADK components to access the information and state
received from the case.

Application and ADK component clients can register with this case domain component to receive notification messages.
Currently supported events are the case lid open/closed state, and the battery status of the case and peer Earbud.

This component also provides API for clients to query the current known state, corresponding to the same case lid and battery status supplied in notifications.

@{
*/

/*! This state diagram illustrates how the Case domain models the case lid state. */
/*!
@startuml

    [*] --> LID_STATE_UNKNOWN : Init                                                                                      
    LID_STATE_UNKNOWN : Lid state on boot is unknown.\nAlso unknown when not in the case.\nAfter going into the case, state is still unknown until case tells the earbud.
    LID_STATE_UNKNOWN --> LID_STATE_OPEN : LidOpen Event
    LID_STATE_UNKNOWN --> LID_STATE_CLOSED : LidClosed Event
    LID_STATE_OPEN -l-> LID_STATE_CLOSED : LidClosed Event
    LID_STATE_CLOSED -r-> LID_STATE_OPEN : LidOpen Event
    LID_STATE_OPEN -u-> LID_STATE_UNKNOWN : OutOfCase Event
    LID_STATE_UNKNOWN --> LID_STATE_UNKNOWN : InCase Event

@enduml
*/

#ifndef CC_WITH_CASE_H
#define CC_WITH_CASE_H

#include <domain_message.h>

/*! State of the case lid. */
typedef enum
{
    /*! Case lid is closed. */
    CASE_LID_STATE_CLOSED   = 0,
    /*! Case lid is open. */
    CASE_LID_STATE_OPEN     = 1,
    /*! State of the case lid is not known. */
    CASE_LID_STATE_UNKNOWN  = 2,
} case_lid_state_t;

/*! Value used in #CASE_POWER_STATUS_T message to indicate state is currently unknown. */
#define BATTERY_STATUS_UNKNOWN     (0x7F)

/*! \brief Message sent by the Case domain component. */
typedef enum
{
    /*! Notification about state of the case lid. */
    CASE_LID_STATE = CASE_MESSAGE_BASE,

    /*! Notification about battery state of case and peer earbud and and case charger connectivity. */
    CASE_POWER_STATE,

    /*! This must be the final message */
    CASE_MESSAGE_END
} case_message_t;
ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(CASE, CASE_MESSAGE_END)

/*! \brief Message indicating change in state of the case lid. */
typedef struct
{
    /*! Current state of the lid */
    case_lid_state_t lid_state;
} CASE_LID_STATE_T;

/*! \brief Message indicating current known state of case and peer battery. */
typedef struct
{
    /*! Last received state of the case battery. */
    uint8 case_battery_state;
    /*! Last received state of the peer battery. */
    uint8 peer_battery_state;
    /*! Current local battery state. */
    uint8 local_battery_state;
    /*! Last received state of case charger connectivity. */
    bool case_charger_connected;
} CASE_POWER_STATE_T;

#if defined(INCLUDE_CASE_COMMS) && defined(HAVE_CC_MODE_EARBUDS)

/*! \brief Initialise the Case domain component.

    \param init_task    Task to send init completion message (if any) to

    \returns TRUE
*/
bool CcWithCase_Init(Task init_task);

/*! \brief Register client task to receive Case state messages.
 
    \param[in] client_task Task to receive messages.
*/
void CcWithCase_RegisterStateClient(Task client_task);

/*! \brief Unregister client task to stop receiving Case state messages.
 
    \param[in] client_task Task to unregister.
*/
void CcWithCase_UnregisterStateClient(Task client_task);

/*! \brief Get the current state of the case lid.
    \return case_lid_state_t Current state of the lid.
    \note May return CASE_LID_STATE_UNKNOWN if Earbud is not in the case
          or called before receipt of notification from case.
*/
case_lid_state_t CcWithCase_GetLidState(void);

/*! \brief Get the battery state of the case.
    \return uint8 Case battery state.
    \note may return #BATTERY_STATUS_UNKNOWN if earbud is not
          in the case or called before receipt of notification from case.
    \note Returned state is a combination of battery level (expressed as a
          percentage) and whether the device is charging.
          Described throughout this component as the "combined format" battery
          state.

          Least significant 7 bits (0..6) contain the battery percentage in
          the range 0..100.
          Most significant bit 7 is set to 1 if device is charging, otherwise
          0.

          Examples 0b10010100 (0x94) Battery is 20% and charging.
                   0b01100100 (0x64) Battery is 100% and not charging.

          0b01111111 (0x7f) indicates battery state is unknown.
*/
uint8 CcWithCase_GetCaseBatteryState(void);

/*! \brief Get the battery state of the peer earbud.
    \return uint8 Peer battery state.

    \note may return #BATTERY_STATUS_UNKNOWN if this (or peer)
          earbud is not in the case or called before receipt of notification
          from case.

    \note Format of the return battery state is the same as described
          for #CcWithCase_GetCaseBatteryState().
*/
uint8 CcWithCase_GetPeerBatteryState(void);

/*! \brief Determine if the case has the charger connected.
    \return bool TRUE if case charger is known to be connected.
                 FALSE otherwise.
    \note May return FALSE if this earbud is not in the case or called
          before receipt of notification from the case.
*/
bool CcWithCase_IsCaseChargerConnected(void);

/*! \brief Handle new lid state event from the case.
    \param new_lid_state New case lid state.

    \note Will result in state clients of the Case domain receiving
          notification of updated state.
*/
void CcWithCase_LidEvent(case_lid_state_t new_lid_state);

/*! \brief Handle new power state message from the case.
    \param case_battery_state Latest case battery state in combined format.
    \param peer_battery_state Latest case battery state in combined format.
    \param local_battery_state Latest case battery state in combined format.
    \param case_charger_connected TRUE case charger is connected, FALSE not connected.

    \note Will result in state clients of the Case domain receiving
          notification of updated state.
*/
void CcWithCase_PowerEvent(uint8 case_battery_state, 
                     uint8 peer_battery_state, uint8 local_battery_state,
                     bool case_charger_connected);

/*! \brief Handle command to perform peer pairing.
    \param peer_address Pointer to BT address of Earbud to peer pair with.

    \note Earbud will send a peer pairing commmand response to indicate to
          the case if peer pairing has been successfully started, or
          rejected by PeerPairing_PeerPairToAddress.
*/
void CcWithCase_PeerPairCmdRx(const bdaddr* peer_address);

/*! \brief Handle command to enter shipping mode.

    \note Earbud will send a shipping mode command response to indicate
          to the case if the command was accepted and the Earbud will
          enter shipping mode when VCHG is removed.
*/
void CcWithCase_ShippingModeCmdRx(void);

/*! \brief Are case events going to be generated? */
#define CcWithCase_EventsEnabled()                    (TRUE)

#else /* defined(INCLUDE_CASE_COMMS) && defined(HAVE_CC_MODE_EARBUDS) */

#define CcWithCase_RegisterStateClient(_task)          (UNUSED(_task))
#define CcWithCase_UnregisterStateClient(_task)        (UNUSED(_task))
#define CcWithCase_GetLidState()                       (CASE_LID_STATE_UNKNOWN)
#define CcWithCase_GetCaseBatteryState()               (BATTERY_STATUS_UNKNOWN)
#define CcWithCase_GetPeerBatteryState()               (BATTERY_STATUS_UNKNOWN)
#define CcWithCase_IsCaseChargerConnected()            (FALSE)
#define CcWithCase_LidEvent(_new_list_state)           (UNUSED(_new_list_state))
#define CcWithCase_PowerEvent(_case_battery_state, \
                        _peer_battery_state, \
                        _local_battery_state, \
                        _case_charger_connected) (UNUSED(_case_battery_state), UNUSED(_peer_battery_state), UNUSED(_local_battery_state), UNUSED(_case_charger_connected))
#define CcWithCase_EventsEnabled()                     (FALSE)
#define CcWithCase_PeerPairCmdRx(_peer_address)        (UNUSED(_peer_address))
#define CcWithCase_ShippingModeCmdRx()

#endif /* defined(INCLUDE_CASE_COMMS) && defined(HAVE_CC_MODE_EARBUDS) */

#endif /* CC_WITH_CASE_H */

/*! @} End of group documentation */

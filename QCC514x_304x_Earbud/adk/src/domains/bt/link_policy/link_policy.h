/*!
\copyright  Copyright (c) 2008 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       link_policy.h
\brief      Header file for the Link policy manager.

            This module controls the settings for classic Bluetooth links, selecting
            the link parameters based on the current activity.
*/

#ifndef LINK_POLICY_H_
#define LINK_POLICY_H_

#include "connection_abstraction.h"
#include <connection.h>


/*! Power table indexes */
typedef enum lp_power_table_index
{
    /*! Power table used when idle and one handset connected */
    POWERTABLE_IDLE,
    /*! Power table used when idle and two handsets are connected */
    POWERTABLE_MULTIPOINT_IDLE,
    /*! Power table used when the BR/EDR ACL is idle and the controller is also receiving a broadcast */
    POWERTABLE_IDLE_WITH_BROADCAST,
    /*! Power table used when VA is active */
    POWERTABLE_VA_ACTIVE,
    /*! Power table used when only DFU is active */
    POWERTABLE_DFU,
    /*! Power table used when A2DP streaming */
    POWERTABLE_A2DP_STREAMING,
    /*! Power table used when SCO active */
    POWERTABLE_SCO_ACTIVE,
    /*! Must be the final value */
    POWERTABLE_UNASSIGNED,
} lpPowerTableIndex;

/*! Link policy state per ACL connection, stored for us by the connection manager. */
typedef struct
{
    lpPowerTableIndex pt_index;     /*!< Current powertable in use */
} lpPerConnectionState;


bool appLinkPolicyInit(Task init_task);

/*! @brief Update the link policy of connected handsets based on the system state.
    @param bd_addr The Bluetooth address of the handset whose state has changed.
    This may be set NULL if the state change is not related to any single handset.
    This address is not currently used, but may be used in the future.
*/
void appLinkPolicyUpdatePowerTable(const bdaddr *bd_addr);

/*! @brief Force update the link policy of connected handsets based on the system state.
    @param bd_addr The Bluetooth address of the handset whose state has changed.
    This may be set NULL if the state change is not related to any single handset.
    This address is not currently used, but may be used in the future.
*/
void appLinkPolicyForceUpdatePowerTable(const bdaddr *bd_addr);

/*! Handler for connection library messages not sent directly

    This function is called to handle any connection library messages sent to
    the application that the link policy module is interested in. If a message
    is processed then the function returns TRUE.

    \note Some connection library messages can be sent directly as the
        request is able to specify a destination for the response.

    \param  id              Identifier of the connection library message
    \param  message         The message content (if any)
    \param  already_handled Indication whether this message has been processed by
                            another module. The handler may choose to ignore certain
                            messages if they have already been handled.

    \returns TRUE if the message has been processed, otherwise returns the
        value in already_handled
 */
extern bool appLinkPolicyHandleConnectionLibraryMessages(MessageId id,Message message, bool already_handled);

/*! \brief Link policy handles the opening of an ACL by discovering the link
           role. If the local device is central, a role switch will be
           requested to become peripheral.
    \param ind The indication.
*/
void appLinkPolicyHandleClDmAclOpenedIndication(const bdaddr *bd_addr, bool is_ble, bool is_local);

/*! \brief Make changes to link policy following an address swap.
*/
void appLinkPolicyHandleAddressSwap(void);

/*! @brief Force update the handset link policy on the new primary after handover.
    @param bd_addr The Bluetooth address of the handset that has just been
                   handed-over.
*/
void appLinkPolicyHandoverForceUpdateHandsetLinkPolicy(const bdaddr *bd_addr);

#endif /* LINK_POLICY_H_ */

/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    case_domain
\brief      Internal Case domain module for the sending case state notifications to registered clients.
*/
/*! \addtogroup case_domain
@{
*/

#include "cc_with_case.h"
#include "cc_with_case_private.h"
#include "cc_with_case_state_client_msgs.h"

#include <task_list.h>

#include <panic.h>

#ifdef INCLUDE_CASE_COMMS
#ifdef HAVE_CC_MODE_EARBUDS

void CcWithCase_ClientMsgLidState(case_lid_state_t lid_state)
{
    MAKE_CCWC_MESSAGE(CASE_LID_STATE);
    message->lid_state = lid_state;
    TaskList_MessageSend(TaskList_GetFlexibleBaseTaskList(CcWithCase_GetStateClientTasks()), CASE_LID_STATE, message);
}

void CcWithCase_ClientMsgPowerState(uint8 case_battery_state,
                              uint8 peer_battery_state, uint8 local_battery_state,
                              bool case_charger_connected)
{
    MAKE_CCWC_MESSAGE(CASE_POWER_STATE);
    message->case_battery_state = case_battery_state;
    message->peer_battery_state = peer_battery_state;
    message->local_battery_state = local_battery_state;
    message->case_charger_connected = case_charger_connected;
    TaskList_MessageSend(TaskList_GetFlexibleBaseTaskList(CcWithCase_GetStateClientTasks()), CASE_POWER_STATE, message);
}

#endif /* HAVE_CC_MODE_EARBUDS */
#endif /* INCLUDE_CASE_COMMS */
/*! @} End of group documentation */

/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    case_domain
\brief      Internal interface for the sending case state notifications to registered clients.
*/
/*! \addtogroup case_domain
@{
*/

#ifndef CC_WITH_CASE_STATE_CLIENT_MSGS_H
#define CC_WITH_CASE_STATE_CLIENT_MSGS_H

#include "cc_with_case.h"

#ifdef INCLUDE_CASE_COMMS
#ifdef HAVE_CC_MODE_EARBUDS

/*! \brief Send a lid state message to registered clients.
*/
void CcWithCase_ClientMsgLidState(case_lid_state_t lid_state);

/*! \brief Send a case power state message to registered clients.
*/
void CcWithCase_ClientMsgPowerState(uint8 case_battery_state,
                              uint8 peer_battery_state, uint8 local_battery_state,
                              bool case_charger_connected);

#endif /* HAVE_CC_MODE_EARBUDS */
#endif /* INCLUDE_CASE_COMMS */
#endif /* CC_WITH_CASE_STATE_CLIENT_MSGS_H */
/*! @} End of group documentation */

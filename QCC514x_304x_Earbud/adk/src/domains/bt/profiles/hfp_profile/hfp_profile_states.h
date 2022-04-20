/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      HFP state translation between libs/synergy and domains
*/

#ifndef HFP_PROFILE_STATES_H_
#define HFP_PROFILE_STATES_H_

#include <hfp.h>
#include "hfp_profile_states_typedef.h"

/*! \brief Convert library call state into hfpState for hfp_profile

    \return The hfpState corresponding to the call state
 */
hfpState hfpProfile_GetStateFromCallState(hfp_call_state call_state);

#define HfpProfile_StateHasAnySubStateIn(state, substates) ((state & (substates)) != 0)

#define HfpProfile_StateHasSubStates(state, substates) ((state & (substates)) == (substates))

#define HfpProfile_StateIsInitialised(state)                        ((state != HFP_STATE_NULL) && !HfpProfile_StateHasSubStates(state, HFP_SUB_STATE_INITIALISING))

#define HfpProfile_StateIsSlcTransition(state)                      HfpProfile_StateHasAnySubStateIn(state, HFP_SUB_STATE_SLC_CONNECTING | HFP_SUB_STATE_SLC_DISCONNECTING)

#define HfpProfile_StateIsSlcConnectedOrConnecting(state)           HfpProfile_StateHasAnySubStateIn(state, HFP_SUB_STATE_SLC_CONNECTING | HFP_SUB_STATE_SLC_CONNECTED)

#define HfpProfile_StateIsSlcDisconnectedOrDisconnecting(state)     HfpProfile_StateHasAnySubStateIn(state, HFP_SUB_STATE_SLC_DISCONNECTING | HFP_SUB_STATE_SLC_DISCONNECTED)

#define HfpProfile_StateIsSlcConnecting(state)                      HfpProfile_StateHasSubStates(state, HFP_SUB_STATE_SLC_CONNECTING)

#define HfpProfile_StateIsSlcConnected(state)                       HfpProfile_StateHasSubStates(state, HFP_SUB_STATE_SLC_CONNECTED)

#define HfpProfile_StateIsSlcDisconnecting(state)                   HfpProfile_StateHasSubStates(state, HFP_SUB_STATE_SLC_DISCONNECTING)

#define HfpProfile_StateIsSlcDisconnected(state)                    HfpProfile_StateHasSubStates(state, HFP_SUB_STATE_SLC_DISCONNECTED)

#define HfpProfile_StateHasIncomingCall(state)                      HfpProfile_StateHasSubStates(state, HFP_SUB_STATE_CALL_INCOMING)

#define HfpProfile_StateHasOutgoingCall(state)                      HfpProfile_StateHasSubStates(state, HFP_SUB_STATE_CALL_OUTGOING)

#define HfpProfile_StateHasActiveCall(state)                        HfpProfile_StateHasSubStates(state, HFP_SUB_STATE_CALL_ACTIVE)

#define HfpProfile_StateHasHeldCall(state)                          HfpProfile_StateHasSubStates(state, HFP_SUB_STATE_CALL_HELD)

#define HfpProfile_StateHasActiveAndIncomingCall(state)             HfpProfile_StateHasSubStates(state, HFP_SUB_STATE_CALL_ACTIVE | HFP_SUB_STATE_CALL_INCOMING)

#define HfpProfile_StateHasHeldOrIncomingCall(state)                HfpProfile_StateHasAnySubStateIn(state, HFP_SUB_STATE_CALL_INCOMING | HFP_SUB_STATE_CALL_HELD)

#define HfpProfile_StateHasEstablishedCall(state)                   HfpProfile_StateHasAnySubStateIn(state, HFP_SUB_STATE_CALL_ACTIVE | HFP_SUB_STATE_CALL_HELD)

#define HfpProfile_StateHasAnyCall(state)                           HfpProfile_StateHasAnySubStateIn(state, HFP_SUB_STATE_CALL_ACTIVE | \
                                                                                                            HFP_SUB_STATE_CALL_HELD | \
                                                                                                            HFP_SUB_STATE_CALL_INCOMING | \
                                                                                                            HFP_SUB_STATE_CALL_OUTGOING | \
                                                                                                            HFP_SUB_STATE_CALL_MULTIPARTY)

#define HfpProfile_StateHasMultipleCalls(state)                     (HfpProfile_StateHasActiveCall(state) && HfpProfile_StateHasAnySubStateIn(state, HFP_SUB_STATE_CALL_HELD | \
                                                                                                                                                     HFP_SUB_STATE_CALL_INCOMING | \
                                                                                                                                                     HFP_SUB_STATE_CALL_OUTGOING | \
                                                                                                                                                     HFP_SUB_STATE_CALL_MULTIPARTY))

#endif /*HFP_PROFILE_STATES_H_*/

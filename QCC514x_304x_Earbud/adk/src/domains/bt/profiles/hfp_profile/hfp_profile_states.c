/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      HFP state translation between libs/synergy and domains
*/

#ifdef INCLUDE_HFP

#include <stdio.h>
#include <panic.h>
#include <macros.h>

#include "hfp_profile.h"
#include "hfp_profile_instance.h"
#include "hfp_profile_states.h"
#include "hfp_profile_sm.h"

#define HFP_CALL_STATE_TABLE_LENGTH ARRAY_DIM(hfp_call_state_table)

/*! \brief Call status state lookup table

    This table is used to convert the call setup and call indicators into
    the appropriate HFP state machine state
*/
static const hfpState hfp_call_state_table[] =
{
    [hfp_call_state_idle]           = HFP_STATE_CONNECTED_IDLE,
    [hfp_call_state_incoming]       = HFP_STATE_CONNECTED_INCOMING,
    [hfp_call_state_incoming_held]  = HFP_STATE_CONNECTED_HELD,
    [hfp_call_state_outgoing]       = HFP_STATE_CONNECTED_OUTGOING,
    [hfp_call_state_active]         = HFP_STATE_CONNECTED_ACTIVE,
    [hfp_call_state_twc_incoming]   = HFP_STATE_CONNECTED_ACTIVE_WITH_INCOMING,
    [hfp_call_state_twc_outgoing]   = HFP_STATE_CONNECTED_ACTIVE_WITH_OUTGOING,
    [hfp_call_state_held_active]    = HFP_STATE_CONNECTED_ACTIVE_WITH_HELD,
    [hfp_call_state_held_remaining] = HFP_STATE_CONNECTED_HELD,
    [hfp_call_state_multiparty]     = HFP_STATE_CONNECTED_MULTIPARTY,
};

hfpState hfpProfile_GetStateFromCallState(hfp_call_state call_state)
{
    PanicFalse(call_state < HFP_CALL_STATE_TABLE_LENGTH);
    return hfp_call_state_table[call_state];
}

/*! \brief Is HFP connected */
bool appHfpIsConnectedForInstance(hfpInstanceTaskData * instance)
{
    return HfpProfile_StateIsSlcConnected(appHfpGetState(instance));
}

/*! \brief Is HFP connected */
bool appHfpIsConnected(void)
{
    bool is_connected = FALSE;
    hfpInstanceTaskData * instance = NULL;
    hfp_instance_iterator_t iterator;

    for_all_hfp_instances(instance, &iterator)
    {
        is_connected = appHfpIsConnectedForInstance(instance);
        if (is_connected)
            break;
    }
    return is_connected;
}

/*! \brief Is HFP in a call */
bool appHfpIsCallForInstance(hfpInstanceTaskData * instance)
{
    return HfpProfile_StateHasAnyCall(appHfpGetState(instance));
}

bool appHfpIsCall(void)
{
    bool is_call = FALSE;
    hfpInstanceTaskData * instance = NULL;
    hfp_instance_iterator_t iterator;

    for_all_hfp_instances(instance, &iterator)
    {
        is_call = appHfpIsCallForInstance(instance);
        if (is_call)
            break;
    }
    return is_call;
}

/*! \brief Is HFP in an active call */
bool appHfpIsCallActiveForInstance(hfpInstanceTaskData * instance)
{
    return HfpProfile_StateHasActiveCall(appHfpGetState(instance));
}

bool appHfpIsCallActive(void)
{
    bool is_call_active = FALSE;
    hfpInstanceTaskData * instance = NULL;
    hfp_instance_iterator_t iterator;

    for_all_hfp_instances(instance, &iterator)
    {
        is_call_active = appHfpIsCallActiveForInstance(instance);
        if (is_call_active)
            break;
    }
    return is_call_active;
}

/*! \brief Is HFP in an incoming call */
bool appHfpIsCallIncomingForInstance(hfpInstanceTaskData * instance)
{
    return HfpProfile_StateHasIncomingCall(appHfpGetState(instance));
}

bool appHfpIsCallIncoming(void)
{
    bool is_call_incoming = FALSE;
    hfpInstanceTaskData * instance = NULL;
    hfp_instance_iterator_t iterator;

    for_all_hfp_instances(instance, &iterator)
    {
        is_call_incoming = appHfpIsCallIncomingForInstance(instance);
        if (is_call_incoming)
            break;
    }
    return is_call_incoming;
}

/*! \brief Is HFP in an outgoing call */
bool appHfpIsCallOutgoingForInstance(hfpInstanceTaskData * instance)
{
    return HfpProfile_StateHasOutgoingCall(appHfpGetState(instance));
}

bool appHfpIsCallOutgoing(void)
{
    bool is_call_outgoing = FALSE;
    hfpInstanceTaskData * instance = NULL;
    hfp_instance_iterator_t iterator;

    for_all_hfp_instances(instance, &iterator)
    {
        is_call_outgoing = appHfpIsCallOutgoingForInstance(instance);
        if (is_call_outgoing)
            break;
    }
    return is_call_outgoing;
}

/*! \brief Is HFP disconnected */
bool HfpProfile_IsDisconnected(hfpInstanceTaskData * instance)
{
    return !HfpProfile_StateIsSlcConnected(appHfpGetState(instance));
}

#endif

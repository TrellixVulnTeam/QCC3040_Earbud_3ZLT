/*!
\copyright  Copyright (c) 2005 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      TWS feature specific rule functions.
*/

#include "tws_topology_twm_primary_rule_functions.h"
#include "tws_topology_primary_ruleset.h"
#include "tws_topology_goals.h"

#include <phy_state.h>
#include <rules_engine.h>
#include <connection_manager.h>
#include <mirror_profile.h>
#include <cc_with_case.h>
#include <peer_find_role.h>
#include <bandwidth_manager.h>

#include <logging.h>

#define TWSTOP_TWM_PRIMARY_RULE_LOG(...)        DEBUG_LOG(__VA_ARGS__)

rule_action_t ruleTwsTopTwmPriFindRole(void)
{
    if (   (appPhyStateGetState() == PHY_STATE_IN_CASE)
        && (!CcWithCase_EventsEnabled()))
    {
        TWSTOP_TWM_PRIMARY_RULE_LOG("ruleTwsTopTwmPriFindRole ignore as in case and lid events not enabled");
        return rule_action_ignore;
    }
    
    if (   (appPhyStateGetState() == PHY_STATE_IN_CASE)
        && (CcWithCase_EventsEnabled())
        && (CcWithCase_GetLidState() != CASE_LID_STATE_OPEN))
    {
        TWSTOP_TWM_PRIMARY_RULE_LOG("ruleTwsTopTwmPriFindRole, ignore as in case, lid event enabled and lid is not open");
        return rule_action_ignore;
    }


    if (TwsTopology_IsGoalActive(tws_topology_goal_no_role_idle) ||
            TwsTopology_IsGoalQueued(tws_topology_goal_no_role_idle))
    {
        TWSTOP_TWM_PRIMARY_RULE_LOG("ruleTwsTopTwmPriFindRole, defer as no_role setting is ongoing");
        return rule_action_defer;
    }

    if (PeerFindRole_IsActive())
    {
        TWSTOP_TWM_PRIMARY_RULE_LOG("ruleTwsTopTwmPriFindRole, ignore as role selection already running");
        return rule_action_ignore;
    }

    if (   TwsTopology_GetRole() == tws_topology_role_primary
        || TwsTopology_GetRole() == tws_topology_role_secondary)
    {
        TWSTOP_TWM_PRIMARY_RULE_LOG("ruleTwsTopTwmPriFindRole, ignore as already have a role");
        return rule_action_ignore;
    }

    /* if peer pairing is active ignore */
    if (TwsTopology_IsGoalActive(tws_topology_goal_pair_peer))
    {
        TWSTOP_TWM_PRIMARY_RULE_LOG("ruleTwsTopTwmPriFindRole, ignore as peer pairing active");
        return rule_action_ignore;
    }

    if (TwsTopology_IsGoalActive(tws_topology_goal_dynamic_handover))
    {
        /* Ignore as there is a handover goal running  */
        TWSTOP_TWM_PRIMARY_RULE_LOG("ruleTwsTopTwmPriFindRole, Ignore as dynamic handover is still in progress ");
        return rule_action_ignore;
    }

    if (TwsTopology_IsGoalActive(tws_topology_goal_no_role_find_role))
    {
        TWSTOP_TWM_PRIMARY_RULE_LOG("ruleTwsTopTwmPriFindRole, ignore as no role find role in progress ");
        return rule_action_ignore;
    }

    TWSTOP_TWM_PRIMARY_RULE_LOG("ruleTwsTopTwmPriFindRole, run as not in case or in case but lid is open");
    return rule_action_run;
}

rule_action_t ruleTwsTopTwmPriNoRoleIdle(void)
{
    if (appPhyStateGetState() != PHY_STATE_IN_CASE)
    {
        TWSTOP_TWM_PRIMARY_RULE_LOG("ruleTwsTopTwmPriNoRoleIdle, ignore as out of case");
        return rule_action_ignore;
    }

    if (TwsTopology_IsGoalActive(tws_topology_goal_pair_peer))
    {
        TWSTOP_TWM_PRIMARY_RULE_LOG("ruleTwsTopTwmPriNoRoleIdle, ignore as peer pairing active");
        return rule_action_ignore;
    }

    if (TwsTopology_IsDfuInCase() && appPhyStateGetState() == PHY_STATE_IN_CASE)
    {
        /* ToDo: Need to decide on the scope to clear it. */
        DEBUG_LOG("ruleTwsTopTwmPriNoRoleIdle, ignore as in-case DFU pending to retain links");
        return rule_action_ignore;
    }

    /* this permits HDMA to react to the IN_CASE and potentially generate a handover event
     * in the first instance */
    if ((TwsTopologyGetTaskData()->hdma_created) && !(TwsTopologyGetTaskData()->app_prohibit_handover))
    {
        TWSTOP_TWM_PRIMARY_RULE_LOG("ruleTwsTopTwmPriNoRoleIdle, defer as HDMA is Active and will generate handover recommendation shortly");
        return rule_action_defer;
    }

    /* this prevent IN_CASE stopping an in-progress dynamic handover from continuing to run
     * where we've past the point that HDMA has been destroyed */
    if(TwsTopology_IsGoalActive(tws_topology_goal_dynamic_handover))
    {
        if(appDeviceIsPeerConnected())
        {
            TWSTOP_TWM_PRIMARY_RULE_LOG("ruleTwsTopTwmPriNoRoleIdle, Defer as dynamic handover is Active and still have peer link");
            return rule_action_defer;
        }
    }

    /* Make sure that there is role_none already achieved without outstanding PFR active.
     * There might be chance that PFR goal sets role_none and begin to start find role procedure */
    if (
            ((TwsTopology_GetRole() == tws_topology_role_none) && !TwsTopology_IsGoalActive(tws_topology_goal_find_role) && !PeerFindRole_IsActive()) ||
            (TwsTopology_IsGoalActive(tws_topology_goal_no_role_idle))
        )
    {
        TWSTOP_TWM_PRIMARY_RULE_LOG("ruleTwsTopTwmPriNoRoleIdle, ignore as already have no role or already actively going to no role");
        return rule_action_ignore;
    }

    /* Check if remain active for peer is set, if then don't run the no role idle rule. */
    if (TwsTopology_IsRemainActiveForPeerEnabled())
    {
        DEBUG_LOG("ruleTwsTopTwmPriNoRoleIdle, ignore as remain active for peer is set, so don't need to become idle");
        return rule_action_ignore;
    }

    TWSTOP_TWM_PRIMARY_RULE_LOG("ruleTwsTopTwmPriNoRoleIdle, run as primary in case");
    return rule_action_run;
}

/*! Decide whether to run the handover now. 

    The implementation of this rule works on the basis of the following:

     a) Handover is allowed by application now.
     b) No active goals executing.
*/
rule_action_t ruleTwsTopTwmHandoverStart(void)
{
    if (TwsTopology_IsDfuInCase())
    {
        /* ToDo: Need to decide on the scope to clear it. */
        DEBUG_LOG("ruleTwsTopTwmHandoverStart, Ignored as in-case DFU is pending ");
        return rule_action_ignore;
    }

    if (TwsTopology_IsGoalActive(tws_topology_goal_dynamic_handover) 
         || TwsTopology_IsGoalQueued(tws_topology_goal_dynamic_handover))
    {
        /* Ignore any further handover requests as there is already one in progress */
        TWSTOP_TWM_PRIMARY_RULE_LOG("ruleTwsTopTwmHandoverStart, Ignored as dynamic handover is still in progress ");
        return rule_action_ignore;
    }

    /* We must not do rssi based dynamic handover if aptX adaptive LL mode is active */
    if ((TwsTopologyGetTaskData()->handover_info.reason == HDMA_HANDOVER_REASON_RSSI)
         && (BandwidthManager_IsFeatureRunning(BANDWIDTH_MGR_FEATURE_A2DP_LL)))
    {
        TWSTOP_TWM_PRIMARY_RULE_LOG("ruleTwsTopTwmHandoverStart, Ignored as aptX adaptive is in low latency mode ");
        return rule_action_ignore;
    }

    /* Check if Handover is allowed by application */
    if(TwsTopologyGetTaskData()->app_prohibit_handover)
    {
        TWSTOP_TWM_PRIMARY_RULE_LOG("ruleTwsTopTwmHandoverStart, defer as App has blocked ");
        return rule_action_defer;
    }

    if (   TwsTopology_IsGoalActive(tws_topology_goal_primary_connect_peer_profiles)
        || !MirrorProfile_IsConnected())
    {
        TWSTOP_TWM_PRIMARY_RULE_LOG("ruleTwsTopTwmHandoverStart, defer as handover profiles not ready");
        return rule_action_defer;
    }
	
	if (TwsTopology_IsGoalActive(tws_topology_goal_disconnect_handset))
    {
        TWSTOP_TWM_PRIMARY_RULE_LOG("ruleTwsTopTwmHandoverStart, defer as goal disconnect handset on going");
        return rule_action_defer;
    }

    if (TwsTopology_IsGoalActive(tws_topology_goal_no_role_idle))
    {
        TWSTOP_TWM_PRIMARY_RULE_LOG("ruleTwsTopTwmHandoverStart, ignore as goal no role idle on going");
        return rule_action_ignore;
    }

    /* Run the rule now */
    TWSTOP_TWM_PRIMARY_RULE_LOG("ruleTwsTopTwmHandoverStart, run");
    return rule_action_run;
}


/*!
\copyright  Copyright (c) 2005 - 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Common primary role TWS topology rule functions.
*/

#include "tws_topology_common_primary_rule_functions.h"
#include "tws_topology_primary_ruleset.h"
#include "tws_topology_goals.h"
#include "tws_topology_config.h"
#include "tws_topology_procedure_enable_le_connectable_handset.h"
#include "tws_topology_procedure_enable_connectable_peer.h"
#include "tws_topology_procedure_enable_connectable_handset.h"
#include "tws_topology_private.h"
#include "tws_topology_config.h"

#include <bt_device.h>
#include <device_properties.h>
#include <device.h>
#include <device_list.h>
#include <hfp_profile.h>
#include <phy_state.h>
#include <rules_engine.h>
#include <connection_manager.h>
#include <peer_signalling.h>
#include <peer_find_role.h>
#include <logging.h>
#include <cc_with_case.h>
#include <handset_service_sm.h>
#include <pairing.h>

/*! \{
    Macros for diagnostic output that can be suppressed. */
#define TWSTOP_PRIMARY_RULE_LOG(...)         DEBUG_LOG(__VA_ARGS__)
/*! \} */

/*! Types of event that can initiate a connection rule decision. */
typedef enum
{
    /*! Completion of a role switch. */
    rule_connect_role_switch = 1 << 0,
    /*! Earbud taken out of the case. */
    rule_connect_out_of_case  = 1 << 1,
    /*! Completion of handset pairing. (TWS+) */
    rule_connect_pairing = 1 << 2,
    /*! Link loss with handset. */
    rule_connect_linkloss = 1 << 3,
    /*! Topology user requests connection */
    rule_connect_user = 1 << 4,
    /*! Post Handover. */
    rule_connect_post_handover = 1 << 5,
} rule_connect_reason_t;


/*! \brief Rule to decide if topology can shut down */
rule_action_t ruleTwsTopPriShutDown(void)
{
    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriShutDown, run always");

    return rule_action_run;
}

rule_action_t ruleTwsTopPriPeerPairedInCase(void)
{
    if (appPhyStateGetState() != PHY_STATE_IN_CASE && !TwsTopology_IsRemainActiveForPeerEnabled())
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriPeerPairedInCase, ignore as neither in case nor remain active for peer is set");
        return rule_action_ignore;
    }

    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriPeerPairedInCase, run as peer paired and in the case");
    return rule_action_run;
}

rule_action_t ruleTwsTopPriPeerPairedOutCase(void)
{
    if (appPhyStateGetState() == PHY_STATE_IN_CASE && !TwsTopology_IsRemainActiveForPeerEnabled())
    {
        if (!CcWithCase_EventsEnabled())
        {
            TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriPeerPairedOutCase, ignore as in case and lid events not enabled");
            return rule_action_ignore;
        }

        if (CcWithCase_GetLidState() != CASE_LID_STATE_OPEN)
        {
            TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriPeerPairedOutCase, ignore as in case and lid is not open");
            return rule_action_ignore;
        }
    }

    if (TwsTopology_IsGoalActive(tws_topology_goal_no_role_find_role))
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriPeerPairedOutCase, ignore as already finding role");
        return rule_action_ignore;
    }

    if (TwsTopology_IsGoalQueued(tws_topology_goal_no_role_find_role))
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriPeerPairedOutCase, ignore as find role already scheduled");
        return rule_action_ignore;
    }

    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriPeerPairedOutCase, run as peer paired and out of case OR remain active for peer is set");
    return rule_action_run;
}

rule_action_t ruleTwsTopPriPairPeer(void)
{
    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriPairPeer, run");
    return rule_action_run;
}

rule_action_t ruleTwsTopPriDisableConnectablePeer(void)
{
    const ENABLE_CONNECTABLE_PEER_PARAMS_T disable_connectable =
    {
        .enable = FALSE,
        .auto_disable = FALSE,
        .page_scan_type = SCAN_MAN_PARAMS_TYPE_SLOW,
    };
    bdaddr secondary_addr;

    if (!appDeviceGetSecondaryBdAddr(&secondary_addr))
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriDisableConnectablePeer, ignore as unknown secondary address");
        return rule_action_ignore;
    }
    if (!ConManagerIsConnected(&secondary_addr))
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriDisableConnectablePeer, ignore as not connected to peer");
        return rule_action_ignore;
    }
    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriDisableConnectablePeer, run as have connection to secondary peer");
    return PRIMARY_RULE_ACTION_RUN_PARAM(disable_connectable);
}

rule_action_t ruleTwsTopPriEnableConnectablePeer(void)
{
    const ENABLE_CONNECTABLE_PEER_PARAMS_T enable_connectable =
    {
        .enable = TRUE,
        .auto_disable = TRUE,
        .page_scan_type = SCAN_MAN_PARAMS_TYPE_FAST
    };
    bdaddr secondary_addr;

    if (!appDeviceGetSecondaryBdAddr(&secondary_addr))
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriEnableConnectablePeer, ignore as unknown secondary address");
        return rule_action_ignore;
    }

    if (appPhyStateGetState() == PHY_STATE_IN_CASE && !TwsTopology_IsRemainActiveForPeerEnabled())
    {
        if (!CcWithCase_EventsEnabled())
        {
            TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriEnableConnectablePeer ignore as in case and lid events not enabled");
            return rule_action_ignore;
        }

        if (CcWithCase_GetLidState() == CASE_LID_STATE_CLOSED)
        {
            TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriEnableConnectablePeer, ignore as in case, lid event enabled and lid is closed");
            return rule_action_ignore;
        }
    }

    if (ConManagerIsConnected(&secondary_addr))
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriEnableConnectablePeer ignore as peer connected");
        return rule_action_ignore;
    }

    if (TwsTopology_IsActingPrimary())
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriEnableConnectablePeer ignore as acting primary");
        return rule_action_ignore;
    }

    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriEnableConnectablePeer, run as out of case and peer not connected");
    return PRIMARY_RULE_ACTION_RUN_PARAM(enable_connectable);
}

rule_action_t ruleTwsTopPriConnectPeerProfiles(void)
{
    uint32 profiles = TwsTopologyConfig_PeerProfiles();
    uint32 peer_profile_connect_mask = TwsTopology_GetPeerProfileConnectMask();
    bool in_case = (appPhyStateGetState() == PHY_STATE_IN_CASE);

    if ( in_case && peer_profile_connect_mask == 0)
    {
        if (!CcWithCase_EventsEnabled())
        {
            TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriConnectPeerProfiles ignore as in case and lid events not enabled");
            return rule_action_ignore;
        }

        if (CcWithCase_GetLidState() == CASE_LID_STATE_CLOSED)
        {
            TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriConnectPeerProfiles, ignore as in case, lid event enabled and lid is closed");
            return rule_action_ignore;
        }
    }

    if (in_case && peer_profile_connect_mask != 0)
    {
        /*
         * Keep the subset of peer profiles and peer_profile_connect_mask as earbud is in the case.
         * Only those profiles needed to be connected when in-case, peer signalling profile should be
         * established.
         * Whereas for peer profiles to be connected when out-of-case, all applicable
         * peer profiles should be established.
         */
        profiles &= peer_profile_connect_mask;
    }

    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriConnectPeerProfiles run as out of case or peer profile connect mask enabled (profiles:x%x)", profiles);
    return PRIMARY_RULE_ACTION_RUN_PARAM(profiles);
}

rule_action_t ruleTwsTopPriDisconnectPeerProfiles(void)
{
    uint32 profiles = TwsTopologyConfig_PeerProfiles();

    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriDisconnectPeerProfiles run (profiles:x%x)", profiles);
    return PRIMARY_RULE_ACTION_RUN_PARAM(profiles);
}

rule_action_t ruleTwsTopPriReleasePeer(void)
{
    if (appPhyStateGetState() == PHY_STATE_IN_CASE && !TwsTopology_IsRemainActiveForPeerEnabled())
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriReleasePeer run. Device is now in case and remain active for peer is not set");
        return rule_action_run;
    }

    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriReleasePeer ignore. Device not in case (normal) Or remain active for peer is set");
    return rule_action_ignore;
}

rule_action_t ruleTwsTopPriSelectedPrimary(void)
{
    if (appPhyStateGetState() == PHY_STATE_IN_CASE && !TwsTopology_IsRemainActiveForPeerEnabled())
    {
        if (!CcWithCase_EventsEnabled())
        {
            TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriSelectedPrimary ignore as in case and lid events not enabled");
            return rule_action_ignore;
        }

        if (CcWithCase_GetLidState() == CASE_LID_STATE_CLOSED)
        {
            TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriSelectedPrimary, ignore as in case, lid event enabled and lid is closed");
            return rule_action_ignore;
        }
    }

    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriSelectedPrimary, run as selected as Primary out of case, or in case with remain active for peer is set but lid not closed");
    return rule_action_run;
}

rule_action_t ruleTwsTopPriSelectedActingPrimary(void)
{
    if (appPhyStateGetState() == PHY_STATE_IN_CASE && !TwsTopology_IsRemainActiveForPeerEnabled())
    {
        if (!CcWithCase_EventsEnabled())
        {
            TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriSelectedActingPrimary ignore as in case and lid events not enabled");
            return rule_action_ignore;
        }

        if (CcWithCase_GetLidState() == CASE_LID_STATE_CLOSED)
        {
            TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriSelectedActingPrimary, ignore as in case, lid event enabled and lid is closed");
            return rule_action_ignore;
        }
    }

    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriSelectedActingPrimary, run as selected as Acting Primary out of case, or  in case with remain active for peer is set but lid not closed");
    return rule_action_run;
}

rule_action_t ruleTwsTopPriNoRoleSelectedSecondary(void)
{
    if (appPhyStateGetState() == PHY_STATE_IN_CASE && !TwsTopology_IsRemainActiveForPeerEnabled())
    {
        if (!CcWithCase_EventsEnabled())
        {
            TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriNoRoleSelectedSecondary ignore as in case and lid events not enabled");
            return rule_action_ignore;
        }

        if (CcWithCase_GetLidState() == CASE_LID_STATE_CLOSED)
        {
            TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriNoRoleSelectedSecondary, ignore as in case, lid event enabled and lid is closed");
            return rule_action_ignore;
        }
    }

    if (TwsTopology_GetRole() != tws_topology_role_none)
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriNoRoleSelectedSecondary, ignore as already have role");
        return rule_action_ignore;
    }

    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriNoRoleSelectedSecondary, run as selected as Secondary out of case, or in the case with remain active for peer is set but lid not closed");
    return rule_action_run;
}

rule_action_t ruleTwsTopPriPrimarySelectedSecondary(void)
{
    if (appPhyStateGetState() == PHY_STATE_IN_CASE && !TwsTopology_IsRemainActiveForPeerEnabled())
    {
        if (!CcWithCase_EventsEnabled())
        {
            TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriPrimarySelectedSecondary ignore as in case and lid events not enabled");
            return rule_action_ignore;
        }

        if (CcWithCase_GetLidState() == CASE_LID_STATE_CLOSED)
        {
            TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriPrimarySelectedSecondary, ignore as in case, lid event enabled and lid is closed");
            return rule_action_ignore;
        }
    }

    if (TwsTopology_GetRole() != tws_topology_role_primary)
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriPrimarySelectedSecondary, ignore as not primary");
        return rule_action_ignore;
    }

    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriPrimarySelectedSecondary, run as Primary out of case, or in case with remain active for peer is set but lid not closed");
    return rule_action_run;
}

rule_action_t ruleTwsTopPriPeerLostFindRole(void)
{
    bdaddr secondary_addr;

    if (appPhyStateGetState() == PHY_STATE_IN_CASE && !TwsTopology_IsRemainActiveForPeerEnabled())
    {
        if (!CcWithCase_EventsEnabled())
        {
            TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriPeerLostFindRole ignore as in case and lid events not enabled");
            return rule_action_ignore;
        }

        if (CcWithCase_GetLidState() == CASE_LID_STATE_CLOSED)
        {
            TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriPeerLostFindRole, ignore as in case, lid event enabled and lid is closed");
            return rule_action_ignore;
        }
    }

    if (TwsTopology_GetRole() != tws_topology_role_primary)
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriPeerLostFindRole, ignore as not primary");
        return rule_action_ignore;
    }

    if (   TwsTopology_IsGoalActive(tws_topology_goal_no_role_idle)
        || TwsTopology_IsGoalActive(tws_topology_goal_no_role_find_role)
        || TwsTopology_IsGoalActive(tws_topology_goal_role_switch_to_secondary))
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriPeerLostFindRole, defer as switching role");
        return rule_action_defer;
    }

    if (!appDeviceGetSecondaryBdAddr(&secondary_addr))
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriPeerLostFindRole, ignore as unknown secondary address");
        return rule_action_ignore;
    }
    if (ConManagerIsConnected(&secondary_addr))
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriPeerLostFindRole, ignore as still connected to secondary");
        return rule_action_ignore;
    }
    /* Do not ignore if DFU is in progess */
    if (twsTopology_JustWentInCase() && !TwsTopology_IsRemainActiveForPeerEnabled())
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriPeerLostFindRole, ignore as just went in the case and remain active for peer is not set");
        return rule_action_ignore;
    }

    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriPeerLostFindRole, run as Primary out of case (or in case with lid not closed), and not connected to secondary");
    return rule_action_run;
}

rule_action_t ruleTwsTopPriEnableConnectableHandset(void)
{
    bdaddr handset_addr;
    const ENABLE_CONNECTABLE_HANDSET_PARAMS_T enable_connectable = {.enable = TRUE};

    if (!appDeviceGetHandsetBdAddr(&handset_addr))
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriEnableConnectableHandset, ignore as not paired with handset");
        return rule_action_ignore;
    }

    if (appPhyStateGetState() == PHY_STATE_IN_CASE && !TwsTopology_IsRemainActiveForHandsetEnabled())
    {
        if (!CcWithCase_EventsEnabled())
        {
            TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriEnableConnectableHandset ignore as in case and lid events not enabled");
            return rule_action_ignore;
        }

        if (CcWithCase_GetLidState() == CASE_LID_STATE_CLOSED)
        {
            TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriEnableConnectableHandset, ignore as in case, lid event enabled and lid is closed");
            return rule_action_ignore;
        }
    }

    if (TwsTopology_GetRole() != tws_topology_role_primary)
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriEnableConnectableHandset, ignore as role is not primary");
        return rule_action_ignore;
    }

    if (TwsTopology_IsGoalActive(tws_topology_goal_no_role_idle))
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriEnableConnectableHandset, ignore as no-role-idle goal is active");
        return rule_action_ignore;
    }

    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriEnableConnectableHandset, run as primary out of case or remain active is set and not connected to handset");
    return PRIMARY_RULE_ACTION_RUN_PARAM(enable_connectable);
}

rule_action_t ruleTwsTopPriEnableLeConnectableHandset(void)
{
    const TWSTOP_PRIMARY_GOAL_ENABLE_LE_CONNECTABLE_HANDSET_T enable_le_adverts =
        {.enable = TRUE};

    if (appPhyStateGetState() == PHY_STATE_IN_CASE && !TwsTopology_IsRemainActiveForHandsetEnabled())
    {
        if (!CcWithCase_EventsEnabled())
        {
            TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriEnableLeConnectableHandset ignore as in case and lid events not enabled");
            return rule_action_ignore;
        }

        if (CcWithCase_GetLidState() == CASE_LID_STATE_CLOSED)
        {
            TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriEnableLeConnectableHandset, ignore as in case, lid event enabled and lid is closed");
            return rule_action_ignore;
        }
    }

    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriEnableLeConnectableHandset, run as primary out of case or in case with remain active for handset is set and not connected to handset");
    return PRIMARY_RULE_ACTION_RUN_PARAM(enable_le_adverts);
}

rule_action_t ruleTwsTopPriDisableConnectableHandset(void)
{
    const ENABLE_CONNECTABLE_HANDSET_PARAMS_T disable_connectable = {.enable = FALSE};

    if (!appDeviceIsBredrHandsetConnected())
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriDisableConnectableHandset, ignore as not connected with handset");
        return rule_action_ignore;
    }

    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriDisableConnectableHandset, run as have connection to handset");
    return PRIMARY_RULE_ACTION_RUN_PARAM(disable_connectable);
}

static rule_action_t ruleTwsTopPriConnectHandset(rule_connect_reason_t reason)
{
    bdaddr handset_addr;
    bool remain_active_for_handset = TwsTopology_IsRemainActiveForHandsetEnabled();

    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriConnectHandset, reason enum:rule_connect_reason_t:%d", reason);

    if (   (appPhyStateGetState() == PHY_STATE_IN_CASE && !remain_active_for_handset)
        && (!CcWithCase_EventsEnabled()))
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriConnectHandset ignore as in case and lid events not enabled");
        return rule_action_ignore;
    }

    if (   (appPhyStateGetState() == PHY_STATE_IN_CASE && !remain_active_for_handset)
        && (CcWithCase_EventsEnabled())
        && (CcWithCase_GetLidState() == CASE_LID_STATE_CLOSED))
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriConnectHandset, ignore as in case, lid event enabled and lid is closed");
        return rule_action_ignore;
    }

    if (!appDeviceGetHandsetBdAddr(&handset_addr))
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriConnectHandset, ignore as not paired with handset");
        return rule_action_ignore;
    }

    if(TwsTopologyGetTaskData()->prohibit_connect_to_handset)
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriConnectHandset, ignore as handset connection disabled");
        return rule_action_ignore;
    }

    if ((reason & rule_connect_linkloss) && Av_IsA2dpSinkStreaming())
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriConnectHandset, ignore as linkloss and other AG is streaming");
        return rule_action_ignore;
    }

    if (   (reason & rule_connect_role_switch)
        && HandsetService_IsAnyBredrConnected())
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriConnectHandset, ignore as roleswitch and already connected to handset");
        return rule_action_ignore;
    }

    device_t handset_device = BtDevice_GetDeviceForBdAddr(&handset_addr);
    uint32 profiles = BtDevice_GetSupportedProfilesForDevice(handset_device);
    if (profiles || (reason & (rule_connect_out_of_case | rule_connect_user | rule_connect_post_handover)))
    {
        /* always connect HFP and A2DP if out of case or pairing connect */
        if (reason & (rule_connect_out_of_case | rule_connect_pairing))
            profiles |= DEVICE_PROFILE_HFP | DEVICE_PROFILE_A2DP;

        if (profiles)
        {
            TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriConnectHandset, run as handset we were connected to before, profiles %08x", profiles);
            return PRIMARY_RULE_ACTION_RUN_PARAM(profiles);
        }
        else
        {
            TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriConnectHandset, ignored as no profiles to connect");
            return rule_action_ignore;
        }
    }
    else
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriConnectHandset, ignored as wasn't connected before");
        return rule_action_ignore;
    }
}

rule_action_t ruleTwsTopPriRoleSwitchConnectHandset(void)
{
    rule_connect_reason_t reason = rule_connect_role_switch;

    if (TwsTopologyGetTaskData()->reconnect_post_handover)
    {
        /* Need to RESET the reconnect_post_handover flag here, it gets set when
        connect_handset goal cancelled in order to run the dynamic handover goal. */
        twsTopology_SetReconnectPostHandover(FALSE);

        reason = rule_connect_post_handover;
    }
    return ruleTwsTopPriConnectHandset(reason);
}

rule_action_t ruleTwsTopPriOutCaseConnectHandset(void)
{
    return ruleTwsTopPriConnectHandset(rule_connect_out_of_case);
}

rule_action_t ruleTwsTopPriHandsetLinkLossReconnect(void)
{
    return ruleTwsTopPriConnectHandset(rule_connect_linkloss);
}

rule_action_t ruleTwsTopPriUserRequestConnectHandset(void)
{
    return ruleTwsTopPriConnectHandset(rule_connect_user);
}

rule_action_t ruleTwsTopPriDisconnectHandset(void)
{
    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriDisconnectHandset");
    return rule_action_run;
}

rule_action_t ruleTwsTopPriDisconnectLruHandset(void)
{
    if(TwsTopology_IsGoalActive(tws_topology_goal_disconnect_handset)
        || TwsTopology_IsGoalActive(tws_topology_goal_connect_handset)
        || (!HandsetService_IsAnyBredrConnected()))
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriDisconnectLruHandset, ignore");
        return rule_action_ignore;
    }

    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriDisconnectLruHandset, run");
    return rule_action_run;
}

rule_action_t ruleTwsTopPriInCaseDisconnectHandset(void)
{
    if (appPhyStateGetState() != PHY_STATE_IN_CASE)
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriInCaseDisconnectHandset, ignore as not in case");
        return rule_action_ignore;
    }

    if (!appDeviceIsHandsetAnyProfileConnected())
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriInCaseDisconnectHandset, ignore as not connected to handset");
        return rule_action_ignore;
    }

    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriInCaseDisconnectHandset, run as in case");
    return rule_action_run;
}

/*! Decide whether to allow handset BR/EDR connections */
rule_action_t ruleTwsTopPriAllowHandsetConnect(void)
{
    const bool allow_connect = TRUE;

    if (appPhyStateGetState() == PHY_STATE_IN_CASE && !TwsTopology_IsRemainActiveForHandsetEnabled())
    {
        if (!CcWithCase_EventsEnabled())
        {
            TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriAllowHandsetConnect ignore as in case and lid events not enabled");
            return rule_action_ignore;
        }

        if (CcWithCase_GetLidState() == CASE_LID_STATE_CLOSED)
        {
            TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriAllowHandsetConnect, ignore as in case, lid event enabled and lid is closed");
            return rule_action_ignore;
        }
    }

    /* If role is not any kind of primary don't allow handsets to connect */
    if (!TwsTopology_IsPrimary())
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriAllowHandsetConnect, ignore as not a primary role");
        return rule_action_ignore;
    }

    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriAllowHandsetConnect, run as primary out of case");
    return PRIMARY_RULE_ACTION_RUN_PARAM(allow_connect);
}



rule_action_t ruleTwsTopPriInCaseWatchdogStart(void)
{
    const uint8 timer = TwsTopologyConfig_InCaseResetDelay();
    if (timer == 0)
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriInCaseWatchdogStart, ignore as no delay set");
        return rule_action_ignore;
    }

    if (appPhyStateGetState() != PHY_STATE_IN_CASE)
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriInCaseWatchdogStart, ignore as out of case");
        return rule_action_ignore;
    }

    if (CcWithCase_EventsEnabled() &&
        (CcWithCase_GetLidState() != CASE_LID_STATE_CLOSED))
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriInCaseWatchdogStart, ignore as lid event enabled and lid is not closed");
        return rule_action_ignore;
    }

    if (TwsTopology_IsGoalActive(tws_topology_goal_pair_peer))
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriInCaseWatchdogStart, ignore as peer pairing active");
        return rule_action_ignore;
    }

    if (TwsTopology_IsDfuInCase() && appPhyStateGetState() == PHY_STATE_IN_CASE)
    {
        /* ToDo: Need to decide on the scope to clear it. */
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriInCaseWatchdogStart, ignore as in-case DFU pending to retain links");
        return rule_action_ignore;
    }

    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriInCaseWatchdogStart, run with %d second timer", timer);
    return PRIMARY_RULE_ACTION_RUN_PARAM(timer);
}


rule_action_t ruleTwsTopPriOutOfCaseWatchdogStop(void)
{
    if ((appPhyStateGetState() == PHY_STATE_IN_CASE) &&
        !CcWithCase_EventsEnabled())
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriOutOfCaseWatchdogStop ignore as in case and lid events not enabled");
        return rule_action_ignore;
    }

    if ((appPhyStateGetState() == PHY_STATE_IN_CASE) &&
        CcWithCase_EventsEnabled() &&
        (CcWithCase_GetLidState() != CASE_LID_STATE_OPEN))
    {
        TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriOutOfCaseWatchdogStop, ignore as in case, lid event enabled and lid is not open");
        return rule_action_ignore;
    }

    TWSTOP_PRIMARY_RULE_LOG("ruleTwsTopPriOutOfCaseWatchdogStop, run");
    return rule_action_run;
}

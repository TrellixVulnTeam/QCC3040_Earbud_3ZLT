/*!
\copyright  Copyright (c) 2005 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Common rule functions for all TWS modes.
*/

#include "tws_topology_common_secondary_rule_functions.h"
#include "tws_topology_secondary_ruleset.h"
#include "tws_topology_goals.h"
#include "tws_topology_private.h"
#include "tws_topology_config.h"

#include <bt_device.h>
#include <phy_state.h>
#include <rules_engine.h>
#include <connection_manager.h>
#include <logging.h>
#include <cc_with_case.h>

#define TWSTOP_SECONDARY_RULE_LOG(...)         DEBUG_LOG(__VA_ARGS__)

/*! \brief Rule to decide if topology can shut down */
rule_action_t ruleTwsTopSecShutDown(void)
{
    TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecShutDown, run always");

    return rule_action_run;
}

/*! \brief Rule to decide if Secondary should start role selection on peer linkloss. */
rule_action_t ruleTwsTopSecPeerLostFindRole(void)
{
    if (appPhyStateGetState() == PHY_STATE_IN_CASE && !TwsTopology_IsRemainActiveForPeerEnabled())
    {
        if (!CcWithCase_EventsEnabled())
        {
            TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecPeerLostFindRole, ignore as in case and lid events not enabled");
            return rule_action_ignore;
        }

        if (CcWithCase_GetLidState() != CASE_LID_STATE_OPEN)
        {
            TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecPeerLostFindRole, ignore as in case and lid is not open");
            return rule_action_ignore;
        }
    }

    /* If DFU is not in progess, use no_role_idle running as indication we went into the case, so don't enable PFR */
    if (twsTopology_JustWentInCase() && !TwsTopology_IsRemainActiveForPeerEnabled())
    {
        TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecPeerLostFindRole, ignore as just went in the case and remain active for peer is not set");
        return rule_action_ignore;
    }

    TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecPeerLostFindRole, run as out of case, or in case with remain active for peer is set but lid is open");
    return rule_action_run;
}

/*! \brief Rule to decide if Secondary should connect to Primary. */
rule_action_t ruleTwsTopSecRoleSwitchPeerConnect(void)
{
    bdaddr primary_addr;

    if (!appDeviceGetPrimaryBdAddr(&primary_addr))
    {
        TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecRoleSwitchPeerConnect, ignore as unknown primary address");
        return rule_action_ignore;
    }

    if (appPhyStateGetState() == PHY_STATE_IN_CASE && !TwsTopology_IsRemainActiveForPeerEnabled())
    {
        if (!CcWithCase_EventsEnabled())
        {
            TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecRoleSwitchPeerConnect, ignore as in case and lid events not enabled");
            return rule_action_ignore;
        }

        if (CcWithCase_GetLidState() == CASE_LID_STATE_CLOSED)
        {
            TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecRoleSwitchPeerConnect, ignore as in case, lid event enabled and lid is closed");
            return rule_action_ignore;
        }
    }

    if (ConManagerIsConnected(&primary_addr))
    {
        TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecRoleSwitchPeerConnect, ignore as peer already connected");
        return rule_action_ignore;
    }

    TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecRoleSwitchPeerConnect, run as secondary out of case and peer not connected");
    return rule_action_run;
}

rule_action_t ruleTwsTopSecNoRoleIdle(void)
{
    if (appPhyStateGetState() != PHY_STATE_IN_CASE)
    {
        TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecNoRoleIdle, ignore as out of case");
        return rule_action_ignore;
    }

    if ((TwsTopology_IsDfuInCase() || TwsTopology_IsRemainActiveForPeerEnabled()) && 
        appPhyStateGetState() == PHY_STATE_IN_CASE)
    {
        /* ToDo: Need to decide on the scope to clear it. */
        TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecNoRoleIdle, ignore as either in-case DFU pending or remain active for peer is set");
        return rule_action_ignore;
    }

    TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecNoRoleIdle, run as secondary in case");
    return rule_action_run;
}

rule_action_t ruleTwsTopSecFailedConnectFindRole(void)
{
    bdaddr primary_addr;

    if (appPhyStateGetState() == PHY_STATE_IN_CASE && !TwsTopology_IsRemainActiveForPeerEnabled())
    {
        if (!CcWithCase_EventsEnabled())
        {
            TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecFailedConnectFindRole, ignore as in the case and lid events not enabled");
            return rule_action_ignore;
        }

        if (CcWithCase_GetLidState() != CASE_LID_STATE_CLOSED)
        {
            TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecFailedConnectFindRole, ignore as in the case, lid events enabled but lid isn't closed");
            return rule_action_ignore;
        }
    }

    if (!appDeviceGetPrimaryBdAddr(&primary_addr))
    {
        TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecFailedConnectFindRole, ignore as unknown primary address");
        return rule_action_ignore;
    }

    if (ConManagerIsConnected(&primary_addr))
    {
        TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecFailedConnectFindRole, ignore as peer already connected");
        return rule_action_ignore;
    }

    TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecFailedConnectFindRole, run as secondary out of case with no peer link");
    return rule_action_run;
}

rule_action_t ruleTwsTopSecFailedSwitchSecondaryFindRole(void)
{
    bdaddr primary_addr;

    if (appPhyStateGetState() == PHY_STATE_IN_CASE && !TwsTopology_IsRemainActiveForPeerEnabled())
    {
        if (!CcWithCase_EventsEnabled())
        {
            TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecFailedSwitchSecondaryFindRole, ignore as in the case and lid events not enabled");
            return rule_action_ignore;
        }

        if (CcWithCase_GetLidState() != CASE_LID_STATE_CLOSED)
        {
            TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecFailedSwitchSecondaryFindRole, ignore as in the case, lid events enabled but lid isn't closed");
            return rule_action_ignore;
        }
    }

    if (!appDeviceGetPrimaryBdAddr(&primary_addr))
    {
        TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecFailedSwitchSecondaryFindRole, ignore as unknown primary address");
        return rule_action_ignore;
    }

    if((TwsTopology_GetRole() == tws_topology_role_secondary) && ConManagerIsConnected(&primary_addr))
    {
        TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecFailedSwitchSecondaryFindRole, ignore as have secondary role and connected to primary");
        return rule_action_ignore;
    }

    TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecFailedSwitchSecondaryFindRole, run as out of case and not a secondary with peer link");
    return rule_action_run;
}


rule_action_t ruleTwsTopSecInCaseWatchdogStart(void)
{
    const uint8 timer = TwsTopologyConfig_InCaseResetDelay();
    if (timer == 0)
    {
        TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecInCaseWatchdogStart, ignore as no delay set");
        return rule_action_ignore;
    }

    if (appPhyStateGetState() != PHY_STATE_IN_CASE)
    {
        TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecInCaseWatchdogStart, ignore as out of case");
        return rule_action_ignore;
    }

    if (CcWithCase_EventsEnabled() &&
        (CcWithCase_GetLidState() != CASE_LID_STATE_CLOSED))
    {
        TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecInCaseWatchdogStart, ignore as lid event enabled and lid is not closed");
        return rule_action_ignore;
    }

    if (TwsTopology_IsGoalActive(tws_topology_goal_pair_peer))
    {
        TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecInCaseWatchdogStart, ignore as peer pairing active");
        return rule_action_ignore;
    }

    if (TwsTopology_IsDfuInCase() && appPhyStateGetState() == PHY_STATE_IN_CASE)
    {
        /* ToDo: Need to decide on the scope to clear it. */
        TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecInCaseWatchdogStart, ignore as in-case DFU pending to retain links");
        return rule_action_ignore;
    }

    TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecInCaseWatchdogStart, run with %d second timer", timer);
    return SECONDARY_RULE_ACTION_RUN_PARAM(timer);
}


rule_action_t ruleTwsTopSecOutOfCaseWatchdogStop(void)
{
    if ((appPhyStateGetState() == PHY_STATE_IN_CASE) &&
        !CcWithCase_EventsEnabled())
    {
        TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecOutOfCaseWatchdogStop ignore as in case and lid events not enabled");
        return rule_action_ignore;
    }

    if ((appPhyStateGetState() == PHY_STATE_IN_CASE) &&
        CcWithCase_EventsEnabled() &&
        (CcWithCase_GetLidState() != CASE_LID_STATE_OPEN))
    {
        TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecOutOfCaseWatchdogStop, ignore as in case, lid event enabled and lid is not open");
        return rule_action_ignore;
    }

    TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecOutOfCaseWatchdogStop, run");
    return rule_action_run;
}

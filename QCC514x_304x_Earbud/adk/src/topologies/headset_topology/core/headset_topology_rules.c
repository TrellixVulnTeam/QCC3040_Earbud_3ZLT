/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      
*/

#include "headset_topology_rules.h"
#include "headset_topology_goals.h"

#include <bt_device.h>
#include <device_properties.h>
#include <device_list.h>
#include <rules_engine.h>
#include <connection_manager.h>
#include <handset_service_sm.h>
#include <av.h>

#include <logging.h>

#pragma unitsuppress Unused

/*! \{
    Macros for diagnostic output that can be suppressed. */

#define HSTOP_RULE_LOG         DEBUG_LOG
/*! \} */

/* Forward declaration for use in RULE_ACTION_RUN_PARAM macro below */
static rule_action_t headsetTopologyRules_CopyRunParams(const void* param, size_t size_param);

/*! \brief Macro used by rules to return RUN action with parameters to return.
 
    Copies the parameters/data into the rules instance where the rules engine 
    can use it when building the action message.
*/
#define RULE_ACTION_RUN_PARAM(x)   headsetTopologyRules_CopyRunParams(&(x), sizeof(x))

/*! Get pointer to the connection rules task data structure. */
#define HeadsetTopologyRulesGetTaskData()  (&headset_topology_rules_task_data)

HeadsetTopologyRulesTaskData headset_topology_rules_task_data;

/*! \{
    Rule function prototypes, so we can build the rule tables below. */

DEFINE_RULE(ruleHsTopEnableConnectableHandset);
DEFINE_RULE(ruleHsTopHandsetLinkLossReconnect);
DEFINE_RULE(ruleHsTopEnableConnectableHandset);
DEFINE_RULE(ruleHsTopAllowHandsetConnect);
DEFINE_RULE(ruleHsTopAutoConnectHandset);
DEFINE_RULE(ruleHsTopAllowLEConnection);
DEFINE_RULE(ruleHsTopDisconnectHandset);
DEFINE_RULE(ruleHsTopStop);
DEFINE_RULE(ruleHsTopUserRequestConnectHandset);
DEFINE_RULE(ruleHsTopDisconnectLruHandset);

/*! \} */

/*! \brief HEADSET Topology rules deciding behaviour.
*/
const rule_entry_t hstop_rules_set[] =
{
    /*! When we are shutting down, disconnect everything. */
    RULE(HSTOP_RULE_EVENT_STOP,                       ruleHsTopStop,                     HSTOP_GOAL_SYSTEM_STOP),
    /* Upon link-loss of BREDR, Connect the headset back to the previously connected handset */
    RULE(HSTOP_RULE_EVENT_HANDSET_LINKLOSS,           ruleHsTopHandsetLinkLossReconnect, HSTOP_GOAL_CONNECT_HANDSET),
    /* Upon start of day of topology, Allow LE connection, make the handset connectable and connect handset if PDL is not empty */
    RULE(HSTOP_RULE_EVENT_START,                      ruleHsTopAllowLEConnection,        HSTOP_GOAL_ALLOW_LE_CONNECTION),
    RULE(HSTOP_RULE_EVENT_START,                      ruleHsTopEnableConnectableHandset, HSTOP_GOAL_CONNECTABLE_HANDSET),
    RULE(HSTOP_RULE_EVENT_START,                      ruleHsTopAllowHandsetConnect,      HSTOP_GOAL_ALLOW_HANDSET_CONNECT),
    RULE(HSTOP_RULE_EVENT_START,                      ruleHsTopAutoConnectHandset,       HSTOP_GOAL_CONNECT_HANDSET),
    /* Prohibit connection upon request from app */
    RULE(HSTOP_RULE_EVENT_PROHIBIT_CONNECT_TO_HANDSET,ruleHsTopDisconnectHandset,        HSTOP_GOAL_DISCONNECT_HANDSET),
    /* Connect handset requested by topology user */
    RULE(HSTOP_RULE_EVENT_USER_REQUEST_CONNECT_HANDSET, ruleHsTopUserRequestConnectHandset,         HSTOP_GOAL_CONNECT_HANDSET),
    RULE(HSTOP_RULE_EVENT_USER_REQUEST_DISCONNECT_ALL_HANDSETS, ruleHsTopDisconnectHandset, HSTOP_GOAL_DISCONNECT_HANDSET),
    RULE(HSTOP_RULE_EVENT_USER_REQUEST_DISCONNECT_LRU_HANDSET, ruleHsTopDisconnectLruHandset, HSTOP_GOAL_DISCONNECT_LRU_HANDSET),
};

/*! Types of event that can initiate a connection rule decision. */
typedef enum
{
    /*! Auto connect to MRU device on headset power on. */
    rule_auto_connect = 1 << 0,
    /*! Link loss with handset. */
    rule_connect_linkloss = 1 << 1,
    /*! Topology user requests for connection. */
    rule_connect_user = 1 << 2,
} rule_connect_reason_t;


/*****************************************************************************
 * RULES FUNCTIONS
 *****************************************************************************/
static rule_action_t ruleHsTopEnableConnectableHandset(void)
{
    bdaddr handset_addr;
    const HSTOP_GOAL_CONNECTABLE_HANDSET_T enable_connectable = {.enable = TRUE};

    /* Ignore the rule if no devices is PDL */
    if (!appDeviceGetHandsetBdAddr(&handset_addr))
    {
        HSTOP_RULE_LOG("ruleHsEnableConnectableHandset, ignore as not paired with handset");
        return rule_action_ignore;
    }

    /* Ignore the rule if already connected with handset */
    if (ConManagerIsConnected(&handset_addr))
    {
        HSTOP_RULE_LOG("ruleHsEnableConnectableHandset, ignore as connected to handset");
        return rule_action_ignore;
    }

    /* Ignore the rule if we are in shutdown mode */
    if(HeadsetTopologyGetTaskData()->shutdown_in_progress)
    {
        HSTOP_RULE_LOG("ruleHsEnableConnectableHandset, ignore as we are in shutdown mode");
        return rule_action_ignore;
    }

    HSTOP_RULE_LOG("ruleHsEnableConnectableHandset, run as headset not connected to handset");

    return RULE_ACTION_RUN_PARAM(enable_connectable);
}


/*! Decide whether to allow handset BR/EDR connections */
static rule_action_t ruleHsTopAllowHandsetConnect(void)
{
    const bool allow_connect = TRUE;

    /* Ignore the rule if we are in shutdown mode */
    if(HeadsetTopologyGetTaskData()->shutdown_in_progress)
    {
        HSTOP_RULE_LOG("ruleHsTopAllowHandsetConnect, ignore as we are in shutdown mode");
        return rule_action_ignore;
    }
    HSTOP_RULE_LOG("ruleHsTopAllowHandsetConnect, run ");

    return RULE_ACTION_RUN_PARAM(allow_connect);
}


static rule_action_t ruleHsTopConnectHandset(rule_connect_reason_t reason)
{
    bdaddr handset_addr;

    HSTOP_RULE_LOG("ruleHsTopConnectHandset, reason %u", reason);

    /* Ignore the rule if no devices is PDL */
    if (!appDeviceGetHandsetBdAddr(&handset_addr))
    {
        HSTOP_RULE_LOG("ruleHsConnectHandset, ignore as not paired with handset");
        return rule_action_ignore;
    }

    /* Ignore the rule if already connected with handset */
    if (ConManagerIsConnected(&handset_addr))
    {
        HSTOP_RULE_LOG("ruleHsEnableConnectableHandset, ignore as connected to handset");
        return rule_action_ignore;
    }

    /* Ignore the rule as prohibit connect is true */
    if(HeadsetTopologyGetTaskData()->prohibit_connect_to_handset)
    {
        HSTOP_RULE_LOG("ruleHsConnectHandset, ignore as handset connection is disabled");
        return rule_action_ignore;
    }

    if ((reason & rule_connect_linkloss) && Av_IsA2dpSinkStreaming())
    {
        HSTOP_RULE_LOG("ruleHsTopConnectHandset, ignore as linkloss and other AG is streaming");
        return rule_action_ignore;
    }

    if ((reason != rule_connect_linkloss) && HandsetServiceSm_MaxBredrAclConnectionsReached())
    {
        HSTOP_RULE_LOG("ruleHsTopConnectHandset, ignore as already connected to handset");
        return rule_action_ignore;
    }
    device_t handset_device = BtDevice_GetDeviceForBdAddr(&handset_addr);
    uint32 profiles = BtDevice_GetSupportedProfilesForDevice(handset_device);
    if (profiles || (reason & rule_connect_user))
    {
        if(reason & rule_auto_connect)
        {
            /* connect HFP and A2DP if pairing connect*/
            profiles |= DEVICE_PROFILE_HFP | DEVICE_PROFILE_A2DP;
        }

        if(profiles)
        {
            HSTOP_RULE_LOG("ruleHsConnectHandset, run as handset we were connected to before");
            return RULE_ACTION_RUN_PARAM(profiles);
        }
        else
        {
            HSTOP_RULE_LOG("ruleHsConnectHandset, ignored as no profiles to connect");
            return rule_action_ignore;
        }
    }
    else
    {
        HSTOP_RULE_LOG("ruleHsConnectHandset, ignored as headset wasn't connected before");
        return rule_action_ignore;
    }
}


static rule_action_t ruleHsTopAutoConnectHandset(void)
{
    HSTOP_RULE_LOG("ruleHsTopPairConnectHandset");

    return ruleHsTopConnectHandset(rule_auto_connect);
}

/*! Decide whether to allow handset LE connections */
static rule_action_t ruleHsTopAllowLEConnection(void)
{
    const HSTOP_GOAL_ALLOW_LE_CONNECTION_T allow_connect = {.allow = TRUE};

    HSTOP_RULE_LOG("ruleHsTopAllowLEConnection, run ");
    return RULE_ACTION_RUN_PARAM(allow_connect);
}


static rule_action_t ruleHsTopHandsetLinkLossReconnect(void)
{
    HSTOP_RULE_LOG("ruleHsTopHandsetLinkLossReconnect");

    return ruleHsTopConnectHandset(rule_connect_linkloss);
}


static rule_action_t ruleHsTopDisconnectHandset(void)
{
    HSTOP_RULE_LOG("ruleHsTopDisconnectHandset");

    return rule_action_run;
}

static rule_action_t ruleHsTopDisconnectLruHandset(void)
{
    if(HeadsetTopology_IsGoalActive(hs_topology_goal_disconnect_handset)
        || HeadsetTopology_IsGoalActive(hs_topology_goal_connect_handset)
        || (!HandsetService_IsAnyBredrConnected()))
    {
        HSTOP_RULE_LOG("ruleHsTopDisconnectLruHandset, ignore");
        return rule_action_ignore;
    }

    HSTOP_RULE_LOG("ruleHsTopDisconnectLruHandset, run");
    return rule_action_run;
}

static rule_action_t ruleHsTopStop(void)
{
    HSTOP_RULE_LOG("ruleHsTopStop");

    return rule_action_run;
}

static rule_action_t ruleHsTopUserRequestConnectHandset(void)
{
    return ruleHsTopConnectHandset(rule_connect_user);
}

/*****************************************************************************
 * END RULES FUNCTIONS
 *****************************************************************************/

/*! \brief Initialise the headset rules module. */
bool HeadsetTopologyRules_Init(Task result_task)
{
    HeadsetTopologyRulesTaskData *hs_rules = HeadsetTopologyRulesGetTaskData();
    rule_set_init_params_t rule_params;

    memset(&rule_params, 0, sizeof(rule_params));
    rule_params.rules = hstop_rules_set;
    rule_params.rules_count = ARRAY_DIM(hstop_rules_set);
    rule_params.nop_message_id = HSTOP_GOAL_NOP;
    rule_params.event_task = result_task;
    hs_rules->rule_set = RulesEngine_CreateRuleSet(&rule_params);

    return TRUE;
}

rule_set_t HeadsetTopologyRules_GetRuleSet(void)
{
    HeadsetTopologyRulesTaskData *hs_rules = HeadsetTopologyRulesGetTaskData();
    return hs_rules->rule_set;
}

void HeadsetTopologyRules_SetEvent(rule_events_t event_mask)
{
    HeadsetTopologyRulesTaskData *hs_rules = HeadsetTopologyRulesGetTaskData();
    RulesEngine_SetEvent(hs_rules->rule_set, event_mask);
}

void HeadsetTopologyRules_ResetEvent(rule_events_t event)
{
    HeadsetTopologyRulesTaskData *hs_rules = HeadsetTopologyRulesGetTaskData();
    RulesEngine_ResetEvent(hs_rules->rule_set, event);
}

rule_events_t HeadsetTopologyRules_GetEvents(void)
{
    HeadsetTopologyRulesTaskData *hs_rules = HeadsetTopologyRulesGetTaskData();
    return RulesEngine_GetEvents(hs_rules->rule_set);
}

void HeadsetTopologyRules_SetRuleComplete(MessageId message)
{
    HeadsetTopologyRulesTaskData *hs_rules = HeadsetTopologyRulesGetTaskData();
    RulesEngine_SetRuleComplete(hs_rules->rule_set, message);
}

void HeadsetTopologyRules_SetRuleWithEventComplete(MessageId message, rule_events_t event)
{
    HeadsetTopologyRulesTaskData *hs_rules = HeadsetTopologyRulesGetTaskData();
    RulesEngine_SetRuleWithEventComplete(hs_rules->rule_set, message, event);
}

/*! \brief Copy rule param data for the engine to put into action messages.
    \param param Pointer to data to copy.
    \param size_param Size of the data in bytes.
    \return rule_action_run_with_param to indicate the rule action message needs parameters.
 */
static rule_action_t headsetTopologyRules_CopyRunParams(const void* param, size_t size_param)
{
    HeadsetTopologyRulesTaskData *hs_rules = HeadsetTopologyRulesGetTaskData();
    RulesEngine_CopyRunParams(hs_rules->rule_set, param, size_param);

    return rule_action_run_with_param;
}

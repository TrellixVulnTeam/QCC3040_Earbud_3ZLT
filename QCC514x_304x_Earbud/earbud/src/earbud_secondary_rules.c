/*!
\copyright  Copyright (c) 2005 - 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       earbud_secondary_rules.c
\brief	    Earbud application rules run when in a Secondary earbud role.
*/

#include "earbud_secondary_rules.h"
#include "earbud_primary_rules.h"
#include "earbud_rules_config.h"

#include <earbud_config.h>
#include <earbud_init.h>
#include <adk_log.h>
#include <anc_state_manager.h>
#include <earbud_sm.h>
#include <gatt_handler.h>

#include <domain_message.h>
#include <av.h>
#include <phy_state.h>
#include <bt_device.h>
#include <bredr_scan_manager.h>
#include <hfp_profile.h>
#include <peer_signalling.h>
#include <state_proxy.h>
#include <rules_engine.h>

#include <bdaddr.h>
#include <panic.h>
#include <system_clock.h>
#include <dfu.h>

#pragma unitsuppress Unused

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_ENUM(earbud_secondary_rules_messages)


/*! \{
    Macros for diagnostic output that can be suppressed.*/
#define SECONDARY_RULE_LOG(...)         DEBUG_LOG_DEBUG(__VA_ARGS__)
/*! \} */

/* Forward declaration for use in RULE_ACTION_RUN_PARAM macro below */
static rule_action_t secondaryRulesCopyRunParams(const void* param, size_t size_param);

/*! \brief Macro used by rules to return RUN action with parameters to return.
    Copies the parameters/data into secondary_rules where the rules engine can uses
    it when building the action message.
*/
#define RULE_ACTION_RUN_PARAM(x)   secondaryRulesCopyRunParams(&(x), sizeof(x))

/*! Get pointer to the connection rules task data structure. */
#define SecondaryRulesGetTaskData()           (&secondary_rules_task_data)

/*!< Connection rules. */
SecondaryRulesTaskData secondary_rules_task_data;

/*! \{
    Rule function prototypes, so we can build the rule tables below. */
DEFINE_RULE(ruleOutOfCaseAllowHandsetConnect);
DEFINE_RULE(ruleOutOfCasePeerSignallingConnect);
DEFINE_RULE(ruleCheckUpgradable);
DEFINE_RULE(ruleInCaseEnterDfu);
DEFINE_RULE(ruleForwardLinkKeys);
#ifdef INCLUDE_FAST_PAIR
DEFINE_RULE(ruleForwardAccountKeys);
#endif
DEFINE_RULE(ruleOutOfCaseAncTuning);
DEFINE_RULE(ruleInCaseLedEnable);
DEFINE_RULE(ruleInCaseAncTuning);
DEFINE_RULE(ruleOutOfCaseAdaptiveAncTuning);

DEFINE_RULE(ruleOutEarDisableAnc);
DEFINE_RULE(ruleInEarEnableAnc);

DEFINE_RULE(ruleInCaseDisableLeakthrough);

/*! \} */

/*! \brief Set of rules to run on Earbud startup. */
const rule_entry_t secondary_rules_set[] =
{
    RULE_ALWAYS(RULE_EVENT_CHECK_DFU,           ruleCheckUpgradable,                CONN_RULES_DFU_ALLOW),

    RULE(RULE_EVENT_IN_CASE,                    ruleInCaseLedEnable,                CONN_RULES_LED_ENABLE),
    /* Rules to start DFU when going in the case. */
    RULE(RULE_EVENT_IN_CASE,                    ruleInCaseEnterDfu,                 SEC_CONN_RULES_ENTER_DFU),
    /* Rules to drive ANC tuning. */
    RULE(RULE_EVENT_OUT_CASE,                   ruleOutOfCaseAncTuning,             CONN_RULES_ANC_TUNING_STOP),
    RULE(RULE_EVENT_IN_CASE,                    ruleInCaseAncTuning,                CONN_RULES_ANC_TUNING_START),
    /* Rule to exit Adaptive ANC tuning. */
    RULE(RULE_EVENT_OUT_CASE,                   ruleOutOfCaseAdaptiveAncTuning,     CONN_RULES_ADAPTIVE_ANC_TUNING_STOP),

    /*! \{
        Rules to drive ANC states in RDP project. */
    RULE(RULE_EVENT_OUT_EAR,                    ruleOutEarDisableAnc,               CONN_RULES_ANC_DISABLE),
    RULE(RULE_EVENT_IN_EAR,                     ruleInEarEnableAnc,                 CONN_RULES_ANC_ENABLE),
    /*! \} */

    /* Rules to synchronise link keys. */
    RULE(RULE_EVENT_PEER_CONNECTED,             ruleForwardLinkKeys,                CONN_RULES_PEER_SEND_LINK_KEYS),

#ifdef INCLUDE_FAST_PAIR
    /* Rule to synchronize account keys. */
    RULE(RULE_EVENT_PEER_CONNECTED,             ruleForwardAccountKeys,             CONN_RULES_PEER_SEND_FP_ACCOUNT_KEYS),
#endif

    /*! \{
        Rule to Disable Leakthrough. */
    RULE(RULE_EVENT_IN_CASE,                    ruleInCaseDisableLeakthrough,       CONN_RULES_LEAKTHROUGH_DISABLE),
    /*! \} */
};

/*! \brief Types of event that can cause connect rules to run. */
typedef enum
{
    RULE_CONNECT_USER,              /*!< User initiated connection */
    RULE_CONNECT_PAIRING,           /*!< Connect on startup */
    RULE_CONNECT_PEER_SYNC,         /*!< Peer sync complete initiated connection */
    RULE_CONNECT_OUT_OF_CASE,       /*!< Out of case initiated connection */
    RULE_CONNECT_LINK_LOSS,         /*!< Link loss recovery initiated connection */
    RULE_CONNECT_PEER_OUT_OF_CASE,  /*!< Peer out of case initiated connection */
} ruleConnectReason;

/*****************************************************************************
 * RULES FUNCTIONS
 *****************************************************************************/

static rule_action_t ruleOutOfCaseAllowHandsetConnect(void)
{
    SECONDARY_RULE_LOG("ruleOutOfCaseAllowHandsetConnect, run as out of case");
    return rule_action_run;
}

static rule_action_t ruleOutOfCasePeerSignallingConnect(void)
{
    SECONDARY_RULE_LOG("ruleOutOfCasePeerSignallingConnect, run as out of case");

    if (!appPeerSigIsConnected())
    {
        return /*rule_action_run*/rule_action_ignore;
    }

    return rule_action_ignore;
}

static rule_action_t ruleCheckUpgradable(void)
{
    bool allow_dfu = TRUE;
    bool block_dfu = FALSE;
    bool is_upgrade_in_progress = Dfu_IsUpgradeInProgress();

    if (appSmIsOutOfCase())
    {
        if (appConfigDfuOnlyFromUiInCase())
        {
            SECONDARY_RULE_LOG("ruleCheckUpgradable, block as only allow DFU from UI (and in case)");
            return RULE_ACTION_RUN_PARAM(block_dfu);
        }
        if ((is_upgrade_in_progress || appPeerSigIsConnected()) && 
            (appConfigDfuAllowBredrUpgradeOutOfCase() || appConfigDfuAllowBleUpgradeOutOfCase()))
        {
            SECONDARY_RULE_LOG("ruleCheckUpgradable, allow as out of case permitted");
            return RULE_ACTION_RUN_PARAM(allow_dfu);
        }

        SECONDARY_RULE_LOG("ruleCheckUpgradable, block as out of case and not permitted");
        return RULE_ACTION_RUN_PARAM(block_dfu);
    }
    else
    {
        SECONDARY_RULE_LOG("ruleCheckUpgradable, allow as in case");
        return RULE_ACTION_RUN_PARAM(allow_dfu);
    }
}

/*! @brief Rule to determine if Earbud should start DFU  when put in case
    Rule is triggered by the 'in case' event
    @startuml

    start
    if (IsInCase() and DfuUpgradePending()) then (yes)
        :DFU upgrade can start as it was pending and now in case;
        stop
    endif
    end
    @enduml
*/
static rule_action_t ruleInCaseEnterDfu(void)
{
#ifdef INCLUDE_DFU
    if (appSmIsInCase() && appSmIsInDfuMode())
    {
        SECONDARY_RULE_LOG("ruleInCaseEnterDfu, run as still in case & DFU pending/active");
        return rule_action_run;
    }
    else
    {
        SECONDARY_RULE_LOG("ruleInCaseEnterDfu, ignore as not in case or no DFU pending");
        return rule_action_ignore;
    }
#else
    return rule_action_ignore;
#endif
}

/*! @brief Rule to determine if Earbud should attempt to forward handset link-key to peer
    @startuml

    start
        if (IsPairedWithPeer()) then (yes)
            :Forward any link-keys to peer;
            stop
        else (no)
            :Not paired;
            end
    @enduml
*/
static rule_action_t ruleForwardLinkKeys(void)
{
    if (BtDevice_IsPairedWithPeer())
    {
        SECONDARY_RULE_LOG("ruleForwardLinkKeys, run");
        return rule_action_run;
    }
    else
    {
        SECONDARY_RULE_LOG("ruleForwardLinkKeys, ignore as there's no peer");
        return rule_action_ignore;
    }
}

/*! @brief Rule to determine if Earbud should attempt to forward fp account keys to peer
    @startuml

    start
        if (IsPairedWithPeer()) then (yes)
            :Forward fp aacount keys to peer;
            stop
        else (no)
            :Not paired;
            end
    @enduml
*/
#ifdef INCLUDE_FAST_PAIR
static rule_action_t ruleForwardAccountKeys(void)
{
    if(BtDevice_IsPairedWithPeer())
    {
        SECONDARY_RULE_LOG("ruleForwardAccountKeys, run");
        return rule_action_run;
    }
    else
    {
        SECONDARY_RULE_LOG("ruleForwardAccountKeys, ignore as there's no peer");
        return rule_action_ignore;
    }
}
#endif

static rule_action_t ruleInEarEnableAnc(void)
{
#ifdef DEFAULT_ANC_ON
    if(appSmIsOutOfCase())
    {
        SECONDARY_RULE_LOG("ruleInEarEnableAnc, run and turn on the ANC");
        return rule_action_run;
    }
#endif
    SECONDARY_RULE_LOG("ruleInEarEnableAnc, ignored");
    return rule_action_ignore;
}

static rule_action_t ruleOutEarDisableAnc(void)
{
#ifdef DEFAULT_ANC_ON
    if(appSmIsOutOfCase())
    {
        SECONDARY_RULE_LOG("ruleOutEarDisableAnc, run and disable the ANC");
        return rule_action_run;
    }
#endif
    SECONDARY_RULE_LOG("ruleOutEarDisableAnc, ignored");
    return rule_action_ignore;
}

static rule_action_t ruleInCaseLedEnable(void)
{
    SECONDARY_RULE_LOG("ruleInCaseLedEnable, run and enable LEDs");
    return rule_action_run;
}

static rule_action_t ruleInCaseAncTuning(void)
{
    if (appConfigAncTuningEnabled())
    {
        SECONDARY_RULE_LOG("ruleInCaseAncTuning, run and enter into the tuning mode");
        return rule_action_run;
    }
    SECONDARY_RULE_LOG("ruleInCaseAncTuning, ignored");
    return rule_action_ignore;
}

static rule_action_t ruleOutOfCaseAncTuning(void)
{
    if (AncStateManager_IsTuningModeActive())
    {
        SECONDARY_RULE_LOG("ruleOutOfCaseAncTuning, run and exit from the tuning mode");
        return rule_action_run;
    }
    SECONDARY_RULE_LOG("ruleOutOfCaseAncTuning, ignored");
    return rule_action_ignore;
}

static rule_action_t ruleOutOfCaseAdaptiveAncTuning(void)
{
#ifdef ENABLE_ADAPTIVE_ANC
    if (AncStateManager_IsAdaptiveAncTuningModeActive())
    {
        SECONDARY_RULE_LOG("ruleOutOfCaseAdaptiveAncTuning, run and exit from the Adaptive ANC tuning mode");
        return rule_action_run;
    }
#endif
    SECONDARY_RULE_LOG("ruleOutOfCaseAdaptiveAncTuning, ignored");
    return rule_action_ignore;
}

static rule_action_t ruleInCaseDisableLeakthrough(void)
{
    if(appSmIsInCase() && AecLeakthrough_IsLeakthroughEnabled())
    {
        SECONDARY_RULE_LOG("ruleInCaseDisableLeakthrough, run and Disable Leakthrough");
        return rule_action_run;
    }
    SECONDARY_RULE_LOG("ruleInCaseDisableLeakthrough, ignored");
    return rule_action_ignore;
}

/*****************************************************************************
 * END RULES FUNCTIONS
 *****************************************************************************/

/*! \brief Initialise the secondary rules module. */
bool SecondaryRules_Init(Task init_task)
{
    SecondaryRulesTaskData *secondary_rules = SecondaryRulesGetTaskData();
    rule_set_init_params_t rule_params;

    UNUSED(init_task);

    memset(&rule_params, 0, sizeof(rule_params));
    rule_params.rules = secondary_rules_set;
    rule_params.rules_count = ARRAY_DIM(secondary_rules_set);
    rule_params.nop_message_id = CONN_RULES_NOP;
    rule_params.event_task = SmGetTask();
    secondary_rules->rule_set = RulesEngine_CreateRuleSet(&rule_params);

    return TRUE;
}

rule_set_t SecondaryRules_GetRuleSet(void)
{
    SecondaryRulesTaskData *secondary_rules = SecondaryRulesGetTaskData();
    return secondary_rules->rule_set;
}

void SecondaryRules_SetEvent(rule_events_t event_mask)
{
    SecondaryRulesTaskData *secondary_rules = SecondaryRulesGetTaskData();
    RulesEngine_SetEvent(secondary_rules->rule_set, event_mask);
}

void SecondaryRules_ResetEvent(rule_events_t event)
{
    SecondaryRulesTaskData *secondary_rules = SecondaryRulesGetTaskData();
    RulesEngine_ResetEvent(secondary_rules->rule_set, event);
}

rule_events_t SecondaryRules_GetEvents(void)
{
    SecondaryRulesTaskData *secondary_rules = SecondaryRulesGetTaskData();
    return RulesEngine_GetEvents(secondary_rules->rule_set);
}

void SecondaryRules_SetRuleComplete(MessageId message)
{
    SecondaryRulesTaskData *secondary_rules = SecondaryRulesGetTaskData();
    RulesEngine_SetRuleComplete(secondary_rules->rule_set, message);
}

void SecondaryRules_SetRuleWithEventComplete(MessageId message, rule_events_t event)
{
    SecondaryRulesTaskData *secondary_rules = SecondaryRulesGetTaskData();
    RulesEngine_SetRuleWithEventComplete(secondary_rules->rule_set, message, event);
}

/*! \brief Copy rule param data for the engine to put into action messages.
    \param param Pointer to data to copy.
    \param size_param Size of the data in bytes.
    \return rule_action_run_with_param to indicate the rule action message needs parameters.
 */
static rule_action_t secondaryRulesCopyRunParams(const void* param, size_t size_param)
{
    SecondaryRulesTaskData *secondary_rules = SecondaryRulesGetTaskData();
    RulesEngine_CopyRunParams(secondary_rules->rule_set, param, size_param);
    return rule_action_run_with_param;
}


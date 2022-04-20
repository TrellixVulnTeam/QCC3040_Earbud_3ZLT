/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      TWS Topology goal handling.
*/

#include "tws_topology.h"
#include "tws_topology_private.h"

#include "tws_topology_primary_ruleset.h"
#include "tws_topology_secondary_ruleset.h"

#include "tws_topology_goals.h"
#include "tws_topology_rule_events.h"
#include "tws_topology_config.h"

#include "tws_topology_procedures.h"
#include "tws_topology_procedure_acting_primary_role.h"
#include "tws_topology_procedure_allow_handset_connect.h"
#include "tws_topology_procedure_cancel_find_role.h"
#include "tws_topology_procedure_connect_handset.h"
#include "tws_topology_procedure_enable_connectable_handset.h"
#include "tws_topology_procedure_disconnect_handset.h"
#include "tws_topology_procedure_disconnect_peer_profiles.h"
#include "tws_topology_procedure_disconnect_peer_find_role.h"
#include "tws_topology_procedure_find_role.h"
#include "tws_topology_procedure_no_role_find_role.h"
#include "tws_topology_procedure_no_role_idle.h"
#include "tws_topology_procedure_pair_peer.h"
#include "tws_topology_procedure_permit_bt.h"
#include "tws_topology_procedure_pri_connect_peer_profiles.h"
#include "tws_topology_procedure_enable_connectable_peer.h"
#include "tws_topology_procedure_primary_addr_find_role.h"
#include "tws_topology_procedure_primary_find_role.h"
#include "tws_topology_procedure_primary_role.h"
#include "tws_topology_procedure_release_peer.h"
#include "tws_topology_procedure_sec_connect_peer.h"
#include "tws_topology_procedure_secondary_role.h"
#include "tws_topology_procedure_set_address.h"
#include "tws_topology_procedure_set_role.h"
#include "tws_topology_procedure_switch_to_secondary.h"
#include "tws_topology_procedure_handover.h"
#include "tws_topology_procedure_dynamic_handover.h"
#include "tws_topology_procedure_enable_le_connectable_handset.h"
#include "tws_topology_procedure_system_stop.h"

#include <logging.h>

#include <message.h>
#include <panic.h>
#include <watchdog.h>

CREATE_WATCHDOG(topology_watchdog);

#pragma unitsuppress Unused

static tws_topology_goal_id twsTopology_GetHandoverGoal(hdma_handover_reason_t reason);

LOGGING_PRESERVE_MESSAGE_TYPE(tws_topology_procedure)

/*! \brief This table defines each goal supported by the topology.

    Each entry links the goal set by a topology rule decision with the procedure required to achieve it.

    Entries also provide the configuration for how the goal should be handled, identifying the following
    characteristics:
     - is the goal exclusive with another, requiring the exclusive goal to be cancelled
     - the contention policy of the goal
        - can cancel other goals
        - can execute concurrently with other goals
        - must wait for other goal completion
     - function pointers to the procedure or script to achieve the goal
     - events to generate back into the role rules engine following goal completion
        - success, failure or timeout are supported
     
    Not all goals require configuration of all parameters so utility macros are used to define a
    goal and set default parameters for unrequired fields.
*/
const goal_entry_t goals[] =
{
    SCRIPT_GOAL(tws_topology_goal_pair_peer, tws_topology_procedure_pair_peer_script,
                &pair_peer_script, tws_topology_goal_none),

    GOAL(tws_topology_goal_find_role, tws_topology_procedure_find_role,
         &proc_find_role_fns, tws_topology_goal_none),

    GOAL_WITH_TIMEOUT_AND_FAIL(tws_topology_goal_secondary_connect_peer, tws_topology_procedure_sec_connect_peer,
                 &proc_sec_connect_peer_fns, tws_topology_goal_none, 
                 TWSTOP_RULE_EVENT_FAILED_PEER_CONNECT,TWSTOP_RULE_EVENT_FAILED_PEER_CONNECT),

    GOAL_WITH_CONCURRENCY_SUCCESS(tws_topology_goal_primary_connect_peer_profiles, tws_topology_procedure_pri_connect_peer_profiles,
                          &proc_pri_connect_peer_profiles_fns, tws_topology_goal_primary_disconnect_peer_profiles,
                          TWSTOP_RULE_EVENT_KICK,
                          CONCURRENT_GOALS_INIT(tws_topology_goal_primary_connectable_peer,
                                                tws_topology_goal_connectable_handset,
                                                tws_topology_goal_connect_handset,
                                                tws_topology_goal_allow_handset_connect,
                                                tws_topology_goal_le_connectable_handset)),

    GOAL_WITH_CONCURRENCY(tws_topology_goal_primary_disconnect_peer_profiles, tws_topology_procedure_disconnect_peer_profiles,
                          &proc_disconnect_peer_profiles_fns, tws_topology_goal_primary_connect_peer_profiles,
                          CONCURRENT_GOALS_INIT(tws_topology_goal_connectable_handset,
                                                tws_topology_goal_connect_handset,
                                                tws_topology_goal_allow_handset_connect,
                                                tws_topology_goal_le_connectable_handset)),

    GOAL_WITH_CONCURRENCY_TIMEOUT(tws_topology_goal_primary_connectable_peer, tws_topology_procedure_enable_connectable_peer,
                                  &proc_enable_connectable_peer_fns, tws_topology_goal_none,
                                  TWSTOP_RULE_EVENT_FAILED_PEER_CONNECT,
                                  CONCURRENT_GOALS_INIT(tws_topology_goal_primary_connect_peer_profiles,
                                                        tws_topology_goal_connect_handset,
                                                        tws_topology_goal_connectable_handset,
                                                        tws_topology_goal_allow_handset_connect,
                                                        tws_topology_goal_le_connectable_handset)),

    SCRIPT_GOAL_CANCEL_SUCCESS(tws_topology_goal_no_role_idle, tws_topology_procedure_no_role_idle,
                       &no_role_idle_script, tws_topology_goal_none, TWSTOP_RULE_EVENT_NO_ROLE),

    GOAL_WITH_CONCURRENCY(tws_topology_goal_connect_handset, tws_topology_procedure_connect_handset,
                          &proc_connect_handset_fns, tws_topology_goal_disconnect_handset,
                          CONCURRENT_GOALS_INIT(tws_topology_goal_primary_connect_peer_profiles,
                                                tws_topology_goal_primary_connectable_peer,
                                                tws_topology_goal_connectable_handset,
                                                tws_topology_goal_allow_handset_connect,
                                                tws_topology_goal_le_connectable_handset,
                                                tws_topology_goal_primary_disconnect_peer_profiles)),

    GOAL(tws_topology_goal_disconnect_handset, tws_topology_procedure_disconnect_handset,
         &proc_disconnect_handset_fns, tws_topology_goal_connect_handset),

    GOAL_WITH_CONCURRENCY(tws_topology_goal_connectable_handset, tws_topology_procedure_enable_connectable_handset,
                          &proc_enable_connectable_handset_fns, tws_topology_goal_none,
                          CONCURRENT_GOALS_INIT(tws_topology_goal_primary_connectable_peer,
                                                tws_topology_goal_primary_connect_peer_profiles,
                                                tws_topology_goal_connect_handset,
                                                tws_topology_goal_allow_handset_connect,
                                                tws_topology_goal_le_connectable_handset,
                                                tws_topology_goal_primary_disconnect_peer_profiles)),

    SCRIPT_GOAL_CANCEL_SUCCESS(tws_topology_goal_become_primary, tws_topology_procedure_become_primary,
                        &primary_role_script, tws_topology_goal_none, TWSTOP_RULE_EVENT_ROLE_SWITCH),

    SCRIPT_GOAL_CANCEL_SUCCESS(tws_topology_goal_become_secondary, tws_topology_procedure_become_secondary,
                        &secondary_role_script, tws_topology_goal_none, TWSTOP_RULE_EVENT_ROLE_SWITCH),

    SCRIPT_GOAL_SUCCESS(tws_topology_goal_become_acting_primary, tws_topology_procedure_become_acting_primary,
                        &acting_primary_role_script, tws_topology_goal_none, TWSTOP_RULE_EVENT_ROLE_SWITCH),
    
    SCRIPT_GOAL(tws_topology_goal_set_address, tws_topology_procedure_set_address,
                &set_primary_address_script, tws_topology_goal_none),

    SCRIPT_GOAL(tws_topology_goal_set_primary_address_and_find_role, tws_topology_procedure_set_primary_address_and_find_role,
                &primary_address_find_role_script, tws_topology_goal_none),
    
    SCRIPT_GOAL_SUCCESS_TIMEOUT_FAILED(tws_topology_goal_role_switch_to_secondary,
                                       tws_topology_procedure_role_switch_to_secondary,
                                       &switch_to_secondary_script,
                                       tws_topology_goal_none,
                                       TWSTOP_RULE_EVENT_ROLE_SWITCH,
                                       TWSTOP_RULE_EVENT_FAILED_SWITCH_SECONDARY,
                                       TWSTOP_RULE_EVENT_FAILED_SWITCH_SECONDARY),

    SCRIPT_GOAL(tws_topology_goal_no_role_find_role, tws_topology_procedure_no_role_find_role,
                &no_role_find_role_script, tws_topology_goal_none),
    
    GOAL(tws_topology_goal_cancel_find_role, tws_topology_procedure_cancel_find_role,
         &proc_cancel_find_role_fns, tws_topology_goal_none),

    SCRIPT_GOAL(tws_topology_goal_primary_find_role, tws_topology_procedure_primary_find_role,
                &primary_find_role_script, tws_topology_goal_connect_handset),

    GOAL(tws_topology_goal_release_peer, tws_topology_procedure_release_peer, 
         &proc_release_peer_fns, tws_topology_goal_none),

    SCRIPT_GOAL_CANCEL_SUCCESS_FAILED(tws_topology_goal_dynamic_handover,
                                      tws_topology_procedure_dynamic_handover,
                                      &dynamic_handover_script, tws_topology_goal_none,
                                      TWSTOP_RULE_EVENT_ROLE_SWITCH,
                                      TWSTOP_RULE_EVENT_HANDOVER_FAILED),

    GOAL_WITH_CONCURRENCY(tws_topology_goal_le_connectable_handset, tws_topology_procedure_enable_le_connectable_handset,
            &proc_enable_le_connectable_handset_fns, tws_topology_goal_none,
            CONCURRENT_GOALS_INIT(tws_topology_goal_allow_handset_connect,
                                  tws_topology_goal_connectable_handset,
                                  tws_topology_goal_connect_handset,
                                  tws_topology_goal_primary_connectable_peer,
                                  tws_topology_goal_primary_connect_peer_profiles,
                                  tws_topology_goal_primary_disconnect_peer_profiles)),

    GOAL_WITH_CONCURRENCY(tws_topology_goal_allow_handset_connect, tws_topology_procedure_allow_handset_connection,
                          &proc_allow_handset_connect_fns, tws_topology_goal_none,
                          CONCURRENT_GOALS_INIT(tws_topology_goal_primary_connectable_peer,
                                                tws_topology_goal_connectable_handset,
                                                tws_topology_goal_primary_connect_peer_profiles,
                                                tws_topology_goal_connect_handset,
                                                tws_topology_goal_le_connectable_handset,
                                                tws_topology_goal_primary_disconnect_peer_profiles)),

    SCRIPT_GOAL_CANCEL(tws_topology_goal_system_stop, tws_topology_procedure_system_stop,
                       &system_stop_script, tws_topology_goal_none),

    GOAL(tws_topology_goal_disconnect_lru_handset, tws_topology_procedure_disconnect_lru_handset,
         &proc_disconnect_lru_handset_fns, tws_topology_goal_connect_handset), 
};


/******************************************************************************
 * Callbacks for procedure confirmations
 *****************************************************************************/

/*! \brief Handle confirmation of procedure start.
    
    Provided as a callback to procedures.
*/
static void twsTopology_GoalProcStartCfm(procedure_id proc, procedure_result_t result)
{
    twsTopologyTaskData* td = TwsTopologyGetTaskData();
    tws_topology_goal_id tws_goal = GoalsEngine_FindGoalForProcedure(td->goal_set, proc);

    DEBUG_LOG("twsTopology_GoalProcStartCfm enum:tws_topology_procedure:%d for enum:tws_topology_goal_id:%d", proc, tws_goal);

    UNUSED(result);
}

/*! \brief Handle completion of a goal.
  
    Provided as a callback for procedures to use to indicate goal completion.

    Remove the goal and associated procedure from the lists tracking
    active ones.
    May generate events into the rules engine based on the completion
    result of the goal.
*/
static void twsTopology_GoalProcComplete(procedure_id proc, procedure_result_t result)
{
    twsTopologyTaskData* td = TwsTopologyGetTaskData();
    goal_id completed_goal = GoalsEngine_FindGoalForProcedure(td->goal_set, proc);
    rule_events_t complete_event = GoalsEngine_GetGoalCompleteEvent(td->goal_set, completed_goal, result);

    DEBUG_LOG("twsTopology_GoalProcComplete enum:tws_topology_procedure:%d for enum:tws_topology_goal_id:%d", proc, completed_goal);
    
    /* clear the goal from list of active goals, this may cause further
     * goals to be delivered from the pending_goal_queue_task */
    GoalsEngine_ClearGoal(td->goal_set, completed_goal);

    if (complete_event)
    {
        DEBUG_LOG("twsTopology_GoalProcComplete generating event 0x%08lx%08lx", PRINT_ULL(complete_event));
        twsTopology_RulesSetEvent(complete_event);
    }
}

/*! \brief Handle confirmation of goal cancellation.

    Provided as a callback for procedures to use to indicate cancellation has
    been completed.
*/
static void twsTopology_GoalProcCancelCfm(procedure_id proc, procedure_result_t result)
{
    twsTopologyTaskData* td = TwsTopologyGetTaskData();
    goal_id goal = GoalsEngine_FindGoalForProcedure(td->goal_set, proc);

    DEBUG_LOG("twsTopology_GoalProcCancelCfm enum:tws_topology_procedure:%d for enum:tws_topology_goal_id:%d", proc, goal);

    UNUSED(result);

    GoalsEngine_ClearGoal(td->goal_set, goal);
}

/******************************************************************************
 * Handlers for converting rules decisions to goals
 *****************************************************************************/

/*! \brief Find and return the relevant handover goal,by mapping the 
           HDMA reason code to topology goal. 
    \param[in] HDMA reason code
*/
tws_topology_goal_id twsTopology_GetHandoverGoal(hdma_handover_reason_t reason)
{
    tws_topology_goal_id goal = tws_topology_goal_none;

    switch(reason)
    {
        case HDMA_HANDOVER_REASON_IN_CASE:
        case HDMA_HANDOVER_REASON_OUT_OF_EAR:
        case HDMA_HANDOVER_REASON_BATTERY_LEVEL:
        case HDMA_HANDOVER_REASON_VOICE_QUALITY:
        case HDMA_HANDOVER_REASON_EXTERNAL:
        case HDMA_HANDOVER_REASON_RSSI:
        case HDMA_HANDOVER_REASON_LINK_QUALITY:
            if (TwsTopologyConfig_DynamicHandoverSupported())
            {
                goal = tws_topology_goal_dynamic_handover;
            }
            break;

        default:
            DEBUG_LOG_ERROR("twsTopology_GetHandoverGoal invalid HDMA handover reason enum:hdma_handover_reason_t:%d", reason);
            break;
    }

    DEBUG_LOG_INFO("twsTopology_GetHandoverGoal enum:tws_topology_goal_id:%d for enum:hdma_handover_reason_t:%d", goal, reason);
    return goal;
}

/*! \brief Determine if a goal is currently being executed. */
bool TwsTopology_IsGoalActive(tws_topology_goal_id goal)
{
    twsTopologyTaskData* td = TwsTopologyGetTaskData();
    return (GoalsEngine_IsGoalActive(td->goal_set, goal));
}

/*! \brief Determine if a goal is currently being queued. */
bool TwsTopology_IsGoalQueued(tws_topology_goal_id goal)
{
    twsTopologyTaskData* td = TwsTopologyGetTaskData();
    return (GoalsEngine_IsGoalQueued(td->goal_set, goal));
}

/*! \brief Check if there are any pending goals. */
bool TwsTopology_IsAnyGoalPending(void)
{
    twsTopologyTaskData* td = TwsTopologyGetTaskData();
    return (GoalsEngine_IsAnyGoalPending(td->goal_set));
}

/*! \brief Given a new goal decision from a rules engine, find the goal and attempt to start it. */
void TwsTopology_HandleGoalDecision(Task task, MessageId id, Message message)
{
    twsTopologyTaskData* td = TwsTopologyGetTaskData();

    if (td->stopping_state != twstop_state_started)
    {
        DEBUG_LOG_VERBOSE("TwsTopology_HandleGoalDecision. Flushed id enum:tws_topology_goal_id:%d (enum:tws_topology_stopping_state_t:%d)",
                                id, td->stopping_state);
        return;
    }

    DEBUG_LOG_INFO("TwsTopology_HandleGoalDecision enum:tws_topology_goal_id:%d", id);

    switch (id)
    {
        case tws_topology_goal_none:
            break;

        case tws_topology_goal_set_address:
            GoalsEngine_ActivateGoal(td->goal_set, id, task, id, PROC_SET_ADDRESS_TYPE_DATA_PRIMARY, sizeof(SET_ADDRESS_TYPE_T));
            break;

        case tws_topology_goal_find_role:
            GoalsEngine_ActivateGoal(td->goal_set, id, task, id, PROC_FIND_ROLE_TIMEOUT_DATA_TIMEOUT, sizeof(FIND_ROLE_PARAMS_T));
            break;

        case tws_topology_goal_primary_find_role:
            GoalsEngine_ActivateGoal(td->goal_set, id, task, id, PROC_FIND_ROLE_TIMEOUT_DATA_CONTINUOUS, sizeof(FIND_ROLE_PARAMS_T));
            break;

        case tws_topology_goal_primary_connect_peer_profiles:
            GoalsEngine_ActivateGoal(td->goal_set, id, task, id, message, sizeof(TWSTOP_PRIMARY_GOAL_CONNECT_PEER_PROFILES_T));
            break;

        case tws_topology_goal_primary_connectable_peer:
            GoalsEngine_ActivateGoal(td->goal_set, id, task, id, message, sizeof(ENABLE_CONNECTABLE_PEER_PARAMS_T));
            break;

        case tws_topology_goal_primary_disconnect_peer_profiles:
            GoalsEngine_ActivateGoal(td->goal_set, id, task, id, message, sizeof(DISCONNECT_PEER_PROFILES_T));
            break;

        case tws_topology_goal_connectable_handset:
            GoalsEngine_ActivateGoal(td->goal_set, id, task, id, message, sizeof(ENABLE_CONNECTABLE_HANDSET_PARAMS_T));
            break;

        case tws_topology_goal_le_connectable_handset:
            GoalsEngine_ActivateGoal(td->goal_set, id, task, id, message, sizeof(TWSTOP_PRIMARY_GOAL_ENABLE_LE_CONNECTABLE_HANDSET_T));
            break;

        case tws_topology_goal_connect_handset:
            GoalsEngine_ActivateGoal(td->goal_set, id, task, id, message, sizeof(TWSTOP_PRIMARY_GOAL_CONNECT_HANDSET_T));
            break;

        case tws_topology_goal_allow_handset_connect:
            GoalsEngine_ActivateGoal(td->goal_set, id, task, id, message, sizeof(ALLOW_HANDSET_CONNECT_PARAMS_T));
            break;

        case tws_topology_goal_dynamic_handover:
            if (   (TwsTopology_IsGoalActive(tws_topology_goal_connect_handset))
                || (TwsTopology_IsGoalQueued(tws_topology_goal_connect_handset)))
            {
                twsTopology_SetReconnectPostHandover(TRUE);
            }
            GoalsEngine_ActivateGoal(td->goal_set, twsTopology_GetHandoverGoal((hdma_handover_reason_t)td->handover_info.reason), task, id, NULL, 0);
            break;

        case tws_topology_goal_start_watchdog:
        {
            const uint8 *time = (const uint8 *)message;
            Watchdog_Kick(&topology_watchdog, *time);
        }
        break;

        case tws_topology_goal_stop_watchdog:
            Watchdog_Stop(&topology_watchdog);
            break;

        /* Goals with no specified message/size */
        case tws_topology_goal_pair_peer:
        case tws_topology_goal_secondary_connect_peer:
        case tws_topology_goal_no_role_idle:
        case tws_topology_goal_disconnect_handset:
        case tws_topology_goal_become_primary:
        case tws_topology_goal_become_secondary:
        case tws_topology_goal_become_acting_primary:
        case tws_topology_goal_set_primary_address_and_find_role:
        case tws_topology_goal_role_switch_to_secondary:
        case tws_topology_goal_no_role_find_role:
        case tws_topology_goal_cancel_find_role:
        case tws_topology_goal_release_peer:
        case tws_topology_goal_system_stop:
        case tws_topology_goal_disconnect_lru_handset:
            GoalsEngine_ActivateGoal(td->goal_set, id, task, id, NULL, 0);
            break;
    }

    /* Always mark the rule as complete, once the goal has been added.
     * Important to do it now, as some goals may change the role and therefore
     * the rule engine which generated the goal and in which the completion must
     * be marked. */
    twsTopology_RulesMarkComplete(id);
}

void TwsTopology_GoalsInit(void)
{
    twsTopologyTaskData *td = TwsTopologyGetTaskData();
    goal_set_init_params_t init_params;

    td->pending_goal_queue_task.handler = TwsTopology_HandleGoalDecision;

    memset(&init_params, 0, sizeof(init_params));
    init_params.goals = goals;
    init_params.goals_count = ARRAY_DIM(goals);
    init_params.pending_goal_queue_task = &td->pending_goal_queue_task;

    init_params.proc_result_task = TwsTopologyGetTask();
    init_params.proc_start_cfm_fn = twsTopology_GoalProcStartCfm;
    init_params.proc_cancel_cfm_fn = twsTopology_GoalProcCancelCfm;
    init_params.proc_complete_cfm_fn = twsTopology_GoalProcComplete;

    td->goal_set = GoalsEngine_CreateGoalSet(&init_params);
}

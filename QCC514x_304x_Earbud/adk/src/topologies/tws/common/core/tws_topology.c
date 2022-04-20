/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      TWS Topology component core.
*/

#include "tws_topology.h"
#include "tws_topology_rule_events.h"
#include "tws_topology_private.h"
#include "tws_topology_primary_ruleset.h"
#include "tws_topology_secondary_ruleset.h"
#include "tws_topology_client_msgs.h"
#include "tws_topology_role_change_client_notifier.h"
#include "tws_topology_goals.h"
#include "tws_topology_procedure_set_address.h"
#include "tws_topology_procedure_find_role.h"
#include "tws_topology_config.h"
#include "tws_topology_sdp.h"
#include "tws_topology_marshal_typedef.h"
#include "tws_topology_peer_sig.h"
#include "tws_topology_goals.h"
#include "tws_topology_advertising.h"
#include <av.h>
#include <peer_find_role.h>
#include <peer_pair_le.h>
#include <state_proxy.h>
#include <phy_state.h>
#include <bt_device.h>
#include <bredr_scan_manager.h>
#include <key_sync.h>
#include <mirror_profile.h>
#include <hfp_profile.h>
#include <hdma.h>
#include <handset_service.h>
#include <peer_signalling.h>
#include <telephony_messages.h>
#include <le_advertising_manager.h>
#include <cc_with_case.h>
#include <dfu.h>

#include <task_list.h>
#include <logging.h>
#include <message.h>

#include <panic.h>
#include <bdaddr.h>

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_TYPE(tws_topology_message_t)
LOGGING_PRESERVE_MESSAGE_TYPE(tws_topology_internal_message_t)
LOGGING_PRESERVE_MESSAGE_TYPE(tws_topology_client_notifier_message_t)
ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(TWS_TOPOLOGY, TWS_TOPOLOGY_MESSAGE_END)
ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(TWS_TOPOLOGY_CLIENT_NOTIFIER, TWS_TOPOLOGY_CLIENT_NOTIFIER_MESSAGE_END)


/*! Instance of the TWS Topology. */
twsTopologyTaskData tws_topology = {0};

void twsTopology_RulesSetEvent(rule_events_t event)
{
    switch (TwsTopologyGetTaskData()->role)
    {
        case tws_topology_role_none:
            /* fall-thru, use primary rules in 'none' role */
        case tws_topology_role_primary:
            TwsTopologyPrimaryRules_SetEvent(event);
            break;

        case tws_topology_role_secondary:
            TwsTopologySecondaryRules_SetEvent(event);
            break;

        default:
            break;
    }
}

void twsTopology_RulesResetEvent(rule_events_t event)
{
    switch (TwsTopologyGetTaskData()->role)
    {
        case tws_topology_role_none:
            /* fall-thru, use primary rules in 'none' role */
        case tws_topology_role_primary:
            TwsTopologyPrimaryRules_ResetEvent(event);
            break;

        case tws_topology_role_secondary:
            TwsTopologySecondaryRules_ResetEvent(event);
            break;

        default:
            break;
    }
}

void twsTopology_RulesMarkComplete(MessageId message)
{
    switch (TwsTopologyGetTaskData()->role)
    {
        case tws_topology_role_none:
            /* fall-thru, use primary rules in 'none' role */
        case tws_topology_role_primary:
            TwsTopologyPrimaryRules_SetRuleComplete(message);
            break;

        case tws_topology_role_secondary:
            TwsTopologySecondaryRules_SetRuleComplete(message);
            break;

        default:
            break;
    }
}

tws_topology_role twsTopology_GetRole(void)
{
    return TwsTopologyGetTaskData()->role;
}

/*! \brief Re evaluate deferred Events after a Set-Role and re-inject them to rules 
    engine if evaluation succeeded
 */
static void twsTopology_ReEvaluateDeferredEvents(rule_events_t event_mask)
{
    phyState current_phy_state = appPhyStateGetState();
    
    /* If the defer occur because of dynamic handover, re-inject physical state */
    if(TwsTopology_IsGoalActive(tws_topology_goal_dynamic_handover) &&
        (event_mask & TWSTOP_RULE_EVENT_IN_CASE)&&(current_phy_state == PHY_STATE_IN_CASE))
        {
            DEBUG_LOG("twsTopology_ReEvaluateDeferredEvents : Set In Case Event");
            twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_IN_CASE);
        }

}

static void twsTopology_EvaluatePhyState(rule_events_t event_mask)
{
    if (   (event_mask & TWSTOP_RULE_EVENT_IN_CASE)
        && (appPhyStateGetState() == PHY_STATE_IN_CASE))
    {
        DEBUG_LOG("twsTopology_EvaluatePhyState setting unhandled IN_CASE event in new rule set");
        twsTopology_RulesResetEvent(TWSTOP_RULE_EVENT_OUT_CASE);
        twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_IN_CASE);
    }
    else if (   (event_mask & TWSTOP_RULE_EVENT_OUT_CASE)
             && (appPhyStateGetState() != PHY_STATE_IN_CASE))
    {
        DEBUG_LOG("twsTopology_EvaluatePhyState setting unhandled OUT_CASE event in new rule set");
        twsTopology_RulesResetEvent(TWSTOP_RULE_EVENT_IN_CASE);
        twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_OUT_CASE);
    }
}

void twsTopology_SetRole(tws_topology_role role)
{
    tws_topology_role current_role = TwsTopologyGetTaskData()->role;
    rule_events_t pri_event_mask = TwsTopologyPrimaryRules_GetEvents();
    rule_events_t sec_event_mask = TwsTopologySecondaryRules_GetEvents();

    DEBUG_LOG("twsTopology_SetRole Current role enum:tws_topology_role:%u -> New role enum:tws_topology_role:%u", current_role, role);

    /* inform clients of role change */
    TwsTopology_SendRoleChangedInd(role);

    /* only need to change role if actually changes */
    if (current_role != role)
    {
        TwsTopologyGetTaskData()->role = role;

        /* when going to no role always reset rule engines */
        if (role == tws_topology_role_none)
        {
            TwsTopologyPrimaryRules_ResetEvent(RULE_EVENT_ALL_EVENTS_MASK);
            TwsTopologySecondaryRules_ResetEvent(RULE_EVENT_ALL_EVENTS_MASK);
            twsTopology_EvaluatePhyState(current_role == tws_topology_role_primary ? pri_event_mask:sec_event_mask);
            twsTopology_UpdateAdvertisingParams();
        }

        if(role == tws_topology_role_secondary)
        {
            Av_SetupForSecondaryRole();
            HfpProfile_SetRole(FALSE);
            MirrorProfile_SetRole(FALSE);
            TwsTopologyPrimaryRules_ResetEvent(RULE_EVENT_ALL_EVENTS_MASK);
            twsTopology_ReEvaluateDeferredEvents(pri_event_mask);            
        }
        else if(role == tws_topology_role_primary)
        {
            Av_SetupForPrimaryRole();
            HfpProfile_SetRole(TRUE);
            MirrorProfile_SetRole(TRUE);
            twsTopology_UpdateAdvertisingParams();
            TwsTopologySecondaryRules_ResetEvent(RULE_EVENT_ALL_EVENTS_MASK);        
        }

    }
}

void twsTopology_SetActingInRole(bool acting)
{
    TwsTopologyGetTaskData()->acting_in_role = acting;
}

void twsTopology_CreateHdma(void)
{
    /* Initialize the handover information to none */
    memset(&TwsTopologyGetTaskData()->handover_info,0,sizeof(handover_data_t));
    TwsTopologyGetTaskData()->hdma_created = TRUE;
    Hdma_Init(TwsTopologyGetTask());
}

void twsTopology_DestroyHdma(void)
{
    twsTopology_RulesResetEvent(TWSTOP_RULE_EVENT_HANDOVER);
    TwsTopologyGetTaskData()->hdma_created = FALSE;
    Hdma_Destroy();
}

static void twsTopology_StartHdma(void)
{
    /*  Mirror ACL connection established, Invoke Hdma_Init  */
    if(TwsTopology_IsPrimary())
    {
        twsTopology_CreateHdma();
    }
}

static void twsTopology_StopHdma(void)
{
    /*  Mirror ACL connection disconnected, Invoke HDMA_Destroy  */
    twsTopology_DestroyHdma();
}

/*! \brief Handle failure to find a role due to not having a paired peer Earbud.
 */
static void twsTopology_HandlePeerFindRoleNoPeer(void)
{
    DEBUG_LOG("twsTopology_HandlePeerFindRoleNoPeer");
    twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_NO_PEER);
}

static void twsTopology_HandlePeerFindRoleTimeout(void)
{
    if (TwsTopologyGetTaskData()->start_cfm_needed)
    {
        TwsTopology_SendStartCfm(tws_topology_status_success, tws_topology_role_primary);
    }
    twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_ROLE_SELECTED_ACTING_PRIMARY);
}

static void twsTopology_HandlePeerFindRolePrimary(void)
{
    if (TwsTopologyGetTaskData()->start_cfm_needed)
    {
        TwsTopology_SendStartCfm(tws_topology_status_success, tws_topology_role_primary);
    }
    twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_ROLE_SELECTED_PRIMARY);
}

static void twsTopology_HandlePeerFindRoleSecondary(void)
{
    DEBUG_LOG("twsTopology_HandlePeerFindRoleSecondary");

    if (TwsTopologyGetTaskData()->start_cfm_needed)
    {
        TwsTopology_SendStartCfm(tws_topology_status_success, tws_topology_role_secondary);
    }
    twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_ROLE_SELECTED_SECONDARY);
}

static void twsTopology_Start(void)
{
    twsTopologyTaskData *twst = TwsTopologyGetTaskData();

    bdaddr bd_addr_secondary;

    twst->stopping_state = twstop_state_started;

    DEBUG_LOG_INFO("twsTopology_Start enum:tws_topology_stopping_state_t:%d", TwsTopologyGetTaskData()->stopping_state);

    /* Check if Earbud is paired with peer, will not have a secondary address if not peer paired */
    if (appDeviceGetSecondaryBdAddr(&bd_addr_secondary))
    {
        /* generate peer paired event into rules engine which will determine how to proceed with startup
           depending on in or out of the case */
        twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_PEER_PAIRED);

        /* inform application that topology has started */
        TwsTopology_SendStartCfm(tws_topology_status_success, twst->role);
    }
    else
    {
        /* generate not peer paired event, which will start peer pairing
           note that application will not be informed topology has started until earbud is paired with
           a peer */
        twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_NO_PEER);
    }
}

/*! Handle TWSTOP_INTERNAL_START */
static void twsTopology_HandleInternalStart(const TWSTOP_INTERNAL_START_T *start)
{
    twsTopologyTaskData *twst = TwsTopologyGetTaskData();

    twst->start_cfm_needed = TRUE;
    twst->app_task = start->app_task;

    if (twst->stopping_state != twstop_state_started)
    {
        DEBUG_LOG("twsTopology_HandleInternalStart enum:tws_topology_stopping_state_t:%d starting", twst->stopping_state);
        twsTopology_Start();
    }
    else
    {
        DEBUG_LOG_WARN("twsTopology_HandleInternalStart called again");
    }
}

/*! Handle TWSTOP_INTERNAL_STOP */
static void twsTopology_HandleInternalStop(const TWSTOP_INTERNAL_STOP_T *stop)
{
    twsTopologyTaskData *twst = TwsTopologyGetTaskData();

    twst->app_task = stop->app_task;

    switch (twst->stopping_state)
    {
        case twstop_state_stopped:
            DEBUG_LOG("twsTopology_HandleInternalStop already stopped");
            TwsTopology_SendStopCfm(tws_topology_status_success);
            twst->app_task = NULL;
        break;

        case twstop_state_started:
        {
            uint32 timeout_ms = D_SEC(TwsTopologyConfig_TwsTopologyStopTimeoutS());

            DEBUG_LOG("twsTopology_HandleInternalStop timeout:%u", timeout_ms);

            if (timeout_ms)
            {
                MessageSendLater(TwsTopologyGetTask(), 
                                TWSTOP_INTERNAL_TIMEOUT_TOPOLOGY_STOP, NULL,
                                timeout_ms);
            }
            twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_SHUTDOWN);
        }
        break;

        case twstop_state_stopping:
            DEBUG_LOG("twsTopology_HandleInternalStop already stopping");
        break;

        default:
            Panic();
        break;
    }
}


static void twsTopology_MarkAsStopped(void)
{
    twsTopologyTaskData *twst = TwsTopologyGetTaskData();
    twst->app_task = NULL;
    twst->stopping_state = twstop_state_stopped;
}

static void twsTopology_HandleStopTimeout(void)
{
    DEBUG_LOG_FN_ENTRY("twsTopology_HandleStopTimeout");

    TwsTopology_SendStopCfm(tws_topology_status_fail);
    twsTopology_MarkAsStopped();
}

static void twsTopology_HandleStopCompletion(void)
{
    if (TwsTopologyGetTaskData()->stopping_state == twstop_state_stopping)
    {
        DEBUG_LOG_FN_ENTRY("twsTopology_HandleStopCompletion");

        /* Send the stop message BEFORE clearing the app task below */
        TwsTopology_SendStopCfm(tws_topology_status_success);
        twsTopology_MarkAsStopped();
    }
}


static void twsTopology_HandleClearHandoverPlay(void)
{
    DEBUG_LOG("twsTopology_HandleClearHandoverPlay");

    appAvPlayOnHandsetConnection(FALSE);
}


static void twsTopology_HandleProcPeerPairResult(TWSTOP_INTERNAL_PROC_PAIR_PEER_RESULT_T* pppr)
{
    if (pppr->success == TRUE)
    {
        DEBUG_LOG("twsTopology_HandleProcPeerPairResult PEER PAIR SUCCESS");
    }
    else
    {
        DEBUG_LOG("twsTopology_HandleProcPeerPairResult PEER PAIR FAILED");
    }
    twsTopology_Start();
}

bool twsTopology_JustWentInCase(void)
{
    twsTopologyTaskData *twst = TwsTopologyGetTaskData();
    /* only return just_went_in_case as TRUE if phystate concurs we're in the case */
    return (twst->just_went_in_case && (appPhyStateGetState() == PHY_STATE_IN_CASE));
}

/* \brief Generate physical state related events into rules engine. */
static void twsTopology_HandlePhyStateChangedInd(PHY_STATE_CHANGED_IND_T* ind)
{
    twsTopologyTaskData *twst = TwsTopologyGetTaskData();

    DEBUG_LOG("twsTopology_HandlePhyStateChangedInd ev enum:phy_state_event:%u", ind->event);

    switch (ind->event)
    {
        case phy_state_event_out_of_case:
            twst->just_went_in_case = FALSE;
            /* Reset the In case rule event set out of case rule event */
            twsTopology_RulesResetEvent(TWSTOP_RULE_EVENT_IN_CASE);
            twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_OUT_CASE);
            break;
        case phy_state_event_in_case:
            twst->just_went_in_case = TRUE;
            if (TwsTopology_IsDfuMode())
                TwsTopology_SetDfuInCase(TRUE);

            /* Reset the out of case rule event set in case rule event */
            twsTopology_RulesResetEvent(TWSTOP_RULE_EVENT_OUT_CASE);
            twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_IN_CASE);
            break;
        default:
        break;
    }
}

/* \brief Update the handover data to record  HDMA notification message in topology */
static void twsTopology_UpdateHandoverInfo(hdma_handover_decision_t* message)
{
    /* Store the HDMA recommendation in topology, currently only
     * handover reason is being stored and used */
    TwsTopologyGetTaskData()->handover_info.reason = (uint16)message->reason;
}

/* \brief Trigger a handover event to the rules engine */
static void twsTopology_TriggerHandoverEvent(void)
{
    switch (TwsTopologyGetTaskData()->handover_info.reason)
    {
        case HDMA_HANDOVER_REASON_IN_CASE:
#ifdef INCLUDE_MIRRORING
        case HDMA_HANDOVER_REASON_OUT_OF_EAR:
        case HDMA_HANDOVER_REASON_BATTERY_LEVEL:
        case HDMA_HANDOVER_REASON_VOICE_QUALITY:
        case HDMA_HANDOVER_REASON_EXTERNAL:
        case HDMA_HANDOVER_REASON_RSSI:
        case HDMA_HANDOVER_REASON_LINK_QUALITY:
#endif
        {
            DEBUG_LOG("Reason: %d\n", TwsTopologyGetTaskData()->handover_info.reason);
            twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_HANDOVER);
        }   
        break;
        default:
        break;
    }
}

/* \brief Handle HDMA notifications */
static void twsTopology_HandleHDMARequest(hdma_handover_decision_t* message)
{
    DEBUG_LOG("twsTopology_HandleHDMARequest");

    if(!TwsTopologyGetTaskData()->app_prohibit_handover)
    {
        /* Store the HDMA recommendation message in topology */
        twsTopology_UpdateHandoverInfo(message);

        /* Check and trigger a handover event to the rules engine */
        twsTopology_TriggerHandoverEvent();
    }
}

/* \brief Handle HDMA cancel notification */
static void twsTopology_HandleHDMACancelHandover(void)
{
    DEBUG_LOG("twsTopology_HandleHDMACancelHandover");

    twsTopology_RulesResetEvent(TWSTOP_RULE_EVENT_HANDOVER);
    /* Initialize the handover information to none, irrespective of whether
     * application prohibit handover or not, handover process shall be cancelled upon hdma recommendation*/
    memset(&TwsTopologyGetTaskData()->handover_info,0,sizeof(handover_data_t));

    /*There might occur race condition of CancelHandover recommendation from hdma handled while 
    tws_topology_goal_dynamic_handover is in vm_queue. Therefore cancel the rule message*/
    MessageCancelFirst(TwsTopologyGetGoalTask(), tws_topology_goal_dynamic_handover);

    /* TODO: shall dequeue the handover goal if it's in pending state, this could happen when cancellable 
    handover goal is trying to cancel all the current goals and waiting for goal cancel confirmation 
    asynchronously, meanwhile if hdma recommends to cancel handover. This might also lead to the same 
    deliberate panic situation */
}

/*! \brief Generate handset related Connection events into rule engine. */
static void twsTopology_HandleHandsetServiceConnectedInd(const HANDSET_SERVICE_CONNECTED_IND_T* ind)
{

    DEBUG_LOG("twsTopology_HandleHandsetConnectedInd %04x,%02x,%06lx", ind->addr.nap,
                                                                       ind->addr.uap,
                                                                       ind->addr.lap);
    twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_HANDSET_CONNECTED_BREDR);
    twsTopology_UpdateAdvertisingParams();
}

/*! \brief Generate handset related disconnection events into rule engine. */
static void twsTopology_HandleHandsetServiceDisconnectedInd(const HANDSET_SERVICE_DISCONNECTED_IND_T* ind)
{
    DEBUG_LOG("twsTopology_HandleHandsetDisconnectedInd %04x,%02x,%06lx status %u", ind->addr.nap,
                                                                                    ind->addr.uap,
                                                                                    ind->addr.lap,
                                                                                    ind->status);

    if (ind->status == handset_service_status_link_loss)
    {
        twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_HANDSET_LINKLOSS);
    }
    else
    {
        twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_HANDSET_DISCONNECTED_BREDR);
    }
    twsTopology_UpdateAdvertisingParams();
}

/*! \brief Start or stop HDMA depending on Earbud state.
 
    HDMA is enabled on if the Earbud has connection to handset and peer Earbud and
    State Proxy has received initial state from peer to be synchronised.
*/
static void twsTopology_CheckHdmaRequired(void)
{
    bool handset_connected = appDeviceIsBredrHandsetConnected();
    bool peer_connected = appDeviceIsPeerConnected();
    bool state_proxy_rx = StateProxy_InitialStateReceived();

    DEBUG_LOG("twsTopology_CheckHdmaRequired handset %u peer %u stateproxy %u",
                                handset_connected, peer_connected, state_proxy_rx);

    if (handset_connected && peer_connected && state_proxy_rx)
    {
        if (!TwsTopologyGetTaskData()->hdma_created)
        {

            DEBUG_LOG("twsTopology_CheckHdmaRequired start HDMA");
            twsTopology_StartHdma();
        }
    }
    else
    {
        if (TwsTopologyGetTaskData()->hdma_created)
        {
            DEBUG_LOG("twsTopology_CheckHdmaRequired stop HDMA");
            twsTopology_StopHdma();
        }
    }
}

/*! \brief Generate connection related events into rule engine. */
static void twsTopology_HandleConManagerConnectionInd(const CON_MANAGER_CONNECTION_IND_T* ind)
{
    DEBUG_LOG("twsTopology_HandleConManagerConnectionInd Conn:%u BLE:%u %04x,%02x,%06lx", ind->connected,
                                                                                          ind->ble,
                                                                                          ind->bd_addr.nap,
                                                                                          ind->bd_addr.uap,
                                                                                          ind->bd_addr.lap);
    if(!ind->ble)
    {
        /* start or stop HDMA as BREDR links have changed. */
        twsTopology_CheckHdmaRequired();

        if (appDeviceIsPeer(&ind->bd_addr))
        {   /* generate peer BREDR connection events into rules engines */
            if (ind->connected)
            {
                DEBUG_LOG("twsTopology_HandleConManagerConnectionInd PEER BREDR Connected");
                twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_PEER_CONNECTED_BREDR);
            }
            else
            {
                if (ind->reason == hci_error_conn_timeout)
                {
                    DEBUG_LOG("twsTopology_HandleConManagerConnectionInd PEER BREDR LINKLOSS");
                    twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_PEER_LINKLOSS);
                }
                else
                {
                    DEBUG_LOG("twsTopology_HandleConManagerConnectionInd PEER BREDR DISCONNECTED");
                    twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_PEER_DISCONNECTED_BREDR);
                }
            }
            twsTopology_UpdateAdvertisingParams();
        }
        else if (appDeviceIsHandset(&ind->bd_addr))
        {
            if (ind->connected)
            {
                DEBUG_LOG("twsTopology_HandleConManagerConnectionInd Handset ACL Connected");
                twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_HANDSET_ACL_CONNECTED);
            }
        }
    }
    else
    {
        DEBUG_LOG("twsTopology_HandleConManagerConnectionInd not interested in BLE events atm");
    }
}

static void twsTopology_HandleMirrorProfileConnectedInd(void)
{
    /* this message indicates the mirroring ACL is setup, this
     * may have occurred after HDMA has issued a handover decision
     * that was deferred by the primary rules (due to mirroring
     * not being setup yet), so need to kick the rules to reevaluate
     * if a handover needs to be started */
    twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_KICK);
}

/*! \brief Handle the sleep prepare.

    \note This function can be called if the TWS Topology has 
    not yet started
 */
static void twsTopology_HandlePowerSleepPrepareInd(void)
{
    DEBUG_LOG("twsTopology_HandlePowerSleepPrepareInd");
    /* nothing to prepare, responsd immediately */
    appPowerSleepPrepareResponse(TwsTopologyGetTask());
}

static void twsTopology_HandlePowerSleepCancelledInd(void)
{
    DEBUG_LOG("twsTopology_HandlePowerSleepCancelledInd");
}

/*! \brief Handle the shutdown prepare.

    \note This function can be called if the TWS Topology has 
    not yet started
 */
static void twsTopology_HandlePowerShutdownPrepareInd(void)
{
    DEBUG_LOG("twsTopology_HandlePowerShutdownPrepareInd");
    appPowerShutdownPrepareResponse(TwsTopologyGetTask());

}

static void twsTopology_HandlePowerShutdownCancelledInd(void)
{
    DEBUG_LOG("twsTopology_HandlePowerShutdownCancelledInd");

    /* todo Leave the shutdown role and run the find role procedure -
     * this will be achieved by gating rule execution of new events, a
     * different mechanism which is currently wip. This function will
     * remove the gate and run the no_role_find_role procedure
    twsTopology_SetRole(tws_topology_role_none);

    twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_ROLE_SWITCH);
    */
}

static void twsTopology_HandleStateProxyInitialStateReceived(void)
{
    DEBUG_LOG("twsTopology_HandleStateProxyInitialStateReceived");
    twsTopology_CheckHdmaRequired();
}

#ifdef INCLUDE_CASE_COMMS
/*! \brief Handle notification of case lid state. */
static void twsTopology_HandleCaseLidState(const CASE_LID_STATE_T* cls)
{
    twsTopologyTaskData *twst = TwsTopologyGetTaskData();

    DEBUG_LOG("twsTopology_HandleCaseLidState enum:case_lid_state_t:%d", cls->lid_state);

    switch (cls->lid_state)
    {
        case CASE_LID_STATE_OPEN:
            twsTopology_RulesResetEvent(TWSTOP_RULE_EVENT_CASE_LID_CLOSED);
            twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_CASE_LID_OPEN);
            break;
        case CASE_LID_STATE_CLOSED:
            twsTopology_RulesResetEvent(TWSTOP_RULE_EVENT_CASE_LID_OPEN);
            twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_CASE_LID_CLOSED);
            twst->just_went_in_case = FALSE;
            break;
        case CASE_LID_STATE_UNKNOWN:
            twsTopology_RulesResetEvent(TWSTOP_RULE_EVENT_CASE_LID_CLOSED);
            twsTopology_RulesResetEvent(TWSTOP_RULE_EVENT_CASE_LID_OPEN);
        default:
            break;
    }
}
#endif

/*! \brief TWS Topology message handler.
 */
static void twsTopology_HandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch (id)
    {
        case CL_SDP_REGISTER_CFM:
	{
	    CL_SDP_REGISTER_CFM_T *cfm = (CL_SDP_REGISTER_CFM_T*)message;
            TwsTopology_HandleSdpRegisterCfm(TwsTopologyGetTask(), cfm->status == sds_status_success, cfm->service_handle);
            return;
	}

        case TWSTOP_INTERNAL_START:
            twsTopology_HandleInternalStart(message);
            return;

        case TWSTOP_INTERNAL_STOP:
            twsTopology_HandleInternalStop(message);
            return;

        /* Always handle sleep command as automatically acccept */
        case APP_POWER_SLEEP_PREPARE_IND:
            twsTopology_HandlePowerSleepPrepareInd();
            break;

        /* Always handle shutdown command */
        case APP_POWER_SHUTDOWN_PREPARE_IND:
            twsTopology_HandlePowerShutdownPrepareInd();
            break;

        default:
            if (!twsTopology_IsRunning())
            {
                DEBUG_LOG("twsTopology_HandleMessage. Not yet started. MESSAGE:tws_topology_internal_message_t:0x%x", id);
                return;
            }
            break;
    }

    /* handle all other messages once running */
    switch (id)
    {
            /* ROLE SELECT SERVICE */
        case PEER_FIND_ROLE_NO_PEER:
            DEBUG_LOG("twsTopology_HandleMessage: PEER_FIND_ROLE_NO_PEER");
            twsTopology_HandlePeerFindRoleNoPeer();
            break;
        case PEER_FIND_ROLE_ACTING_PRIMARY:
            DEBUG_LOG("twsTopology_HandleMessage: PEER_FIND_ROLE_ACTING_PRIMARY");
            twsTopology_HandlePeerFindRoleTimeout();
            break;
        case PEER_FIND_ROLE_PRIMARY:
            DEBUG_LOG("twsTopology_HandleMessage: PEER_FIND_ROLE_PRIMARY");
            twsTopology_HandlePeerFindRolePrimary();
            break;
        case PEER_FIND_ROLE_SECONDARY:
            DEBUG_LOG("twsTopology_HandleMessage: PEER_FIND_ROLE_SECONDARY");
            twsTopology_HandlePeerFindRoleSecondary();
            break;
        case PEER_FIND_ROLE_CANCELLED:
            /* no action required */
            DEBUG_LOG("twsTopology_HandleMessage: PEER_FIND_ROLE_CANCELLED");
            break;

            /* PROCEDURE COMPLETION */
        case TWSTOP_INTERNAL_PROC_PAIR_PEER_RESULT:
            twsTopology_HandleProcPeerPairResult((TWSTOP_INTERNAL_PROC_PAIR_PEER_RESULT_T*)message);
            break;

        case TWSTOP_INTERNAL_PROC_SEND_TOPOLOGY_MESSAGE_SYSTEM_STOP_FINISHED:
            twsTopology_HandleStopCompletion();
            break;

            /* STATE PROXY MESSAGES */
        case STATE_PROXY_EVENT_INITIAL_STATE_RECEIVED:
            twsTopology_HandleStateProxyInitialStateReceived();
            break;

            /* MIRROR PROFILE MESSAGES */
        case MIRROR_PROFILE_CONNECT_IND:
            twsTopology_HandleMirrorProfileConnectedInd();
            break;

        case MIRROR_PROFILE_DISCONNECT_IND:
            /* No action required */
            break;

            /* PHY STATE MESSAGES */
        case PHY_STATE_CHANGED_IND:
            twsTopology_HandlePhyStateChangedInd((PHY_STATE_CHANGED_IND_T*)message);
            break;

        case CON_MANAGER_CONNECTION_IND:
            twsTopology_HandleConManagerConnectionInd((CON_MANAGER_CONNECTION_IND_T*)message);
            break;

            /* PEER SIGNALLING */
        case PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND:
            TwsTopology_HandleMarshalledMsgChannelRxInd((PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND_T*)message);
            break;

        case PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM:
            TwsTopology_HandleMarshalledMsgChannelTxCfm((PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM_T*)message);
            break;

            /* POWER INDICATIONS */
        case APP_POWER_SLEEP_CANCELLED_IND:
            twsTopology_HandlePowerSleepCancelledInd();
            break;

        case APP_POWER_SHUTDOWN_CANCELLED_IND:
            twsTopology_HandlePowerShutdownCancelledInd();
            break;

            /* TELEPHONY MESSAGES */
        case TELEPHONY_AUDIO_DISCONNECTED:
            /* fall-through */
        case TELEPHONY_CALL_ENDED:
            /* kick rules to evaluate any deferred events again, we may have
             * deferred handover due to the on-going call now ended */
            twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_KICK);
            break;

            /* INTERNAL MESSAGES */
        case TWSTOP_INTERNAL_CLEAR_HANDOVER_PLAY:
            twsTopology_HandleClearHandoverPlay();
            break;

        case TWSTOP_INTERNAL_TIMEOUT_TOPOLOGY_STOP:
            twsTopology_HandleStopTimeout();
            break;

            /* CONNECTION LIBRARY (SERVICE DISCOVERY PROTOCOL) MESSAGES */
        case CL_SDP_UNREGISTER_CFM:
	    {
			CL_SDP_UNREGISTER_CFM_T * cfm = (CL_SDP_UNREGISTER_CFM_T*)message;
    	    DEBUG_LOG("CL_SDP_UNREGISTER_CFM_T status %d", cfm->status);
			if(cfm->status != sds_status_pending)
			{
					TwsTopology_HandleSdpUnregisterCfm(TwsTopologyGetTask(), cfm->status == sds_status_success, cfm->service_handle);
			}
			else
			{
			/* Wait for final confirmation message */
			}
			break;
		}

            /* HANDOVER MODULE MESSAGES */
        case HDMA_HANDOVER_NOTIFICATION:
            twsTopology_HandleHDMARequest((hdma_handover_decision_t*)message);
            break;

        case HDMA_CANCEL_HANDOVER_NOTIFICATION:
            twsTopology_HandleHDMACancelHandover();
            break;

            /* HANDSET SERVICE MESSAGES */
        case HANDSET_SERVICE_CONNECTED_IND:
            twsTopology_HandleHandsetServiceConnectedInd((HANDSET_SERVICE_CONNECTED_IND_T*)message);
            break;

        case HANDSET_SERVICE_DISCONNECTED_IND:
            twsTopology_HandleHandsetServiceDisconnectedInd((HANDSET_SERVICE_DISCONNECTED_IND_T*)message);
            break;

#ifdef INCLUDE_CASE_COMMS
        case CASE_LID_STATE:
            twsTopology_HandleCaseLidState((CASE_LID_STATE_T*)message);
            break;
        case CASE_POWER_STATE:
            /*! \todo remove to silently ignore */
            DEBUG_LOG("twsTopology_HandleMessage POWER STATE");
            break;
#endif

        default:
            DEBUG_LOG("twsTopology_HandleMessage. Unhandled message. MESSAGE:tws_topology_internal_message_t:0x%x", id);
            break;
    }
}

static void twsTopologyHandlePairingSuccessMessage(void)
{
    KeySync_Sync();

    /* just completed pairing, check if we need to start HDMA, necessary
     * because the normal checks to start HDMA performed on CON_MANAGER_CONNECTION_INDs
     * will not succeed immediately after pairing because the link type
     * would not have been known to be a handset */
    twsTopology_CheckHdmaRequired();
}

static void twsTopology_HandlePairingActivity(const PAIRING_ACTIVITY_T *message)
{
    DEBUG_LOG("twsTopology_HandlePairingActivity status=enum:pairingActivityStatus:%d",
                message->status);

    switch(message->status)
    {
        case pairingActivitySuccess:
            twsTopologyHandlePairingSuccessMessage();
            break;

        case pairingActivityInProgress:
        case pairingActivityNotInProgress:
        {
            twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_PAIRING_ACTIVITY_CHANGED);
            twsTopology_UpdateAdvertisingParams();
        }
        break;
        default:
            break;
    }
}

static void twsTopology_HandlePairingActivityNotification(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch(id)
    {
        case PAIRING_ACTIVITY:
            DEBUG_LOG("TwsTopology PAIRING_ACTIVITY");
            twsTopology_HandlePairingActivity(message);
            break;
            
        default:
            break;
    }
}

static void twsTopology_RegisterForStateProxyEvents(void)
{
    if (TwsTopologyConfig_StateProxyRegisterEvents())
    {
        StateProxy_EventRegisterClient(TwsTopologyGetTask(), TwsTopologyConfig_StateProxyRegisterEvents());
    }
}

static void twsTopology_RegisterBtParameters(void)
{
    DEBUG_LOG("twsTopology_RegisterBtParameters");

    BredrScanManager_PageScanParametersRegister(&page_scan_params);
    BredrScanManager_InquiryScanParametersRegister(&inquiry_scan_params);
    PanicFalse(LeAdvertisingManager_ParametersRegister(&le_adv_params));
}

static void twsTopology_SelectBtParameters(void)
{
    DEBUG_LOG("twsTopology_SelectBtParameters");

    PanicFalse(LeAdvertisingManager_ParametersSelect(LE_ADVERTISING_PARAMS_SET_TYPE_FAST));
}

bool TwsTopology_Init(Task init_task)
{
    twsTopologyTaskData *tws_taskdata = TwsTopologyGetTaskData();

    UNUSED(init_task);

    tws_taskdata->task.handler = twsTopology_HandleMessage;
    tws_taskdata->goal_task.handler = TwsTopology_HandleGoalDecision;

    tws_taskdata->role = tws_topology_role_none;

    /* Set hdma_created to FALSE */
    tws_taskdata->hdma_created = FALSE;

    /* Handover is allowed by default, app may prohibit handover by calling
    TwsTopology_ProhibitHandover() function with TRUE parameter */
    tws_taskdata->app_prohibit_handover = FALSE;

    tws_taskdata->prohibit_connect_to_handset = FALSE;

    tws_taskdata->stopping_state = twstop_state_stopped;

    tws_taskdata->reconnect_post_handover = FALSE;

    tws_taskdata->advertising_params = LE_ADVERTISING_PARAMS_SET_TYPE_UNSET;

    TwsTopologyPrimaryRules_Init(TwsTopologyGetGoalTask());
    TwsTopologySecondaryRules_Init(TwsTopologyGetGoalTask());

    TwsTopology_GoalsInit();

    PeerFindRole_RegisterTask(TwsTopologyGetTask());
    StateProxy_StateProxyEventRegisterClient(TwsTopologyGetTask());

    /* Register to enable interested state proxy events */
    twsTopology_RegisterForStateProxyEvents();

    /* Register for connect / disconnect events from mirror profile */
    MirrorProfile_ClientRegister(TwsTopologyGetTask());

    /* Register for telephony events to have better control of handover during calls */
    Telephony_RegisterForMessages(TwsTopologyGetTask());

    /* Register with power to receive sleep/shutdown messages and 
     * indicate this client does allow sleep. */
    appPowerClientRegister(TwsTopologyGetTask());
    appPowerClientAllowSleep(TwsTopologyGetTask());

    /* register with handset service as we need disconnect and connect notification */
    HandsetService_ClientRegister(TwsTopologyGetTask());

    appPhyStateRegisterClient(TwsTopologyGetTask());
    ConManagerRegisterConnectionsClient(TwsTopologyGetTask());

    tws_taskdata->pairing_notification_task.handler = twsTopology_HandlePairingActivityNotification;
    Pairing_ActivityClientRegister(&tws_taskdata->pairing_notification_task);

    twsTopology_RegisterBtParameters();
    twsTopology_SelectBtParameters();

    /* Register to use marshalled message channel with topology on peer Earbud. */
    appPeerSigMarshalledMsgChannelTaskRegister(TwsTopologyGetTask(),
                                               PEER_SIG_MSG_CHANNEL_TOPOLOGY,
                                               tws_topology_marshal_type_descriptors,
                                               NUMBER_OF_TWS_TOPOLOGY_MARSHAL_TYPES);

    TwsTopology_SetState(TWS_TOPOLOGY_STATE_SETTING_SDP);


    TaskList_InitialiseWithCapacity(TwsTopologyGetMessageClientTasks(), MESSAGE_CLIENT_TASK_LIST_INIT_CAPACITY);

    
    unsigned rc_registrations_array_dim;
    rc_registrations_array_dim = (unsigned)role_change_client_registrations_end -
                              (unsigned)role_change_client_registrations_begin;
    PanicFalse((rc_registrations_array_dim % sizeof(role_change_client_callback_t)) == 0);
    rc_registrations_array_dim /= sizeof(role_change_client_callback_t);

    TwsTopology_RoleChangeClientNotifierInit(role_change_client_registrations_begin, 
                                    rc_registrations_array_dim); 

#ifdef INCLUDE_CASE_COMMS
    CcWithCase_RegisterStateClient(TwsTopologyGetTask());
#endif

    return TRUE;
}

void TwsTopology_Start(Task requesting_task)
{
    MESSAGE_MAKE(msg, TWSTOP_INTERNAL_START_T);
    msg->app_task = requesting_task;
    MessageSend(TwsTopologyGetTask(), TWSTOP_INTERNAL_START, msg);
}

void TwsTopology_Stop(Task requesting_task)
{
    MESSAGE_MAKE(msg, TWSTOP_INTERNAL_STOP_T);
    msg->app_task = requesting_task;
    MessageSend(TwsTopologyGetTask(), TWSTOP_INTERNAL_STOP, msg);

}

void twsTopology_StopHasStarted(void)
{
    DEBUG_LOG_FN_ENTRY("twsTopology_StopHasStarted");

    TwsTopologyGetTaskData()->stopping_state = twstop_state_stopping;
}


void TwsTopology_RegisterMessageClient(Task client_task)
{
    TaskList_AddTask(TaskList_GetFlexibleBaseTaskList(TwsTopologyGetMessageClientTasks()), client_task);
}

void TwsTopology_UnRegisterMessageClient(Task client_task)
{
    TaskList_RemoveTask(TaskList_GetFlexibleBaseTaskList(TwsTopologyGetMessageClientTasks()), client_task);
}

tws_topology_role TwsTopology_GetRole(void)
{
    return TwsTopologyGetTaskData()->role;
}

void TwsTopology_SetDfuMode(bool val)
{
    DEBUG_LOG("TwsTopology_SetDfuMode. enter_dfu_mode:%d", val);
    TwsTopologyGetTaskData()->enter_dfu_mode = val;

    if(!val)
    {
        /* If DFU mode is reset; reset the DFUInCase flag as well */
        TwsTopology_SetDfuInCase(val);
    }
}

bool TwsTopology_IsDfuMode(void)
{
    DEBUG_LOG("TwsTopology_IsDfuMode. enter_dfu_mode:%d", TwsTopologyGetTaskData()->enter_dfu_mode);
    return TwsTopologyGetTaskData()->enter_dfu_mode;
}

void TwsTopology_SetDfuInCase(bool val)
{
    DEBUG_LOG("TwsTopology_SetDfuInCase. enter_dfu_in_case:%d", val);
    TwsTopologyGetTaskData()->enter_dfu_in_case = val;
}

bool TwsTopology_IsDfuInCase(void)
{
    DEBUG_LOG("TwsTopology_IsDfuInCase. enter_dfu_in_case:%d", TwsTopologyGetTaskData()->enter_dfu_in_case);
    return TwsTopologyGetTaskData()->enter_dfu_in_case;
}

void TwsTopology_EndDfu(void)
{
    if (appPhyStateGetState() == PHY_STATE_IN_CASE)
    {
        DEBUG_LOG("TwsTopology_EndDfu. Set TWSTOP_RULE_EVENT_IN_CASE in order to disconnect links & eventually give up role.");
        /* Remove peer profile connect mask before going in-case, to give up role */
        TwsTopology_SetPeerProfileConnectMask(DEVICE_PROFILE_PEERSIG, FALSE);
        /* No need to remain active for peer */
        TwsTopology_EnableRemainActiveForPeer(FALSE);
        twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_IN_CASE);
    }
}

void TwsTopology_SetPeerProfileConnectMask(uint32 profile_mask, bool enable)
{
    DEBUG_LOG("TwsTopology_SetPeerProfileConnectMask profile_mask: 0x%x, enable:%d",
               profile_mask, enable);
    if(enable)
    {
        /* Set the profile connect mask */
        TwsTopologyGetTaskData()->peer_profile_connect_mask |= profile_mask;
    }
    else
    {
        /* Reset the profile connect mask  */
        TwsTopologyGetTaskData()->peer_profile_connect_mask &= ~profile_mask;
    }
}

void TwsTopology_EnableRemainActiveForHandset(bool remain_active_for_handset)
{
    DEBUG_LOG("TwsTopology_EnableRemainActiveForHandset remain_active_for_handset:%d", remain_active_for_handset);

    TwsTopologyGetTaskData()->remain_active_for_handset = remain_active_for_handset;
}

void TwsTopology_EnableRemainActiveForPeer(bool remain_active_for_peer)
{
    DEBUG_LOG("TwsTopology_EnableRemainActiveForPeer remain_active_for_peer:%d", remain_active_for_peer);

    TwsTopologyGetTaskData()->remain_active_for_peer = remain_active_for_peer;
}

uint32 TwsTopology_GetPeerProfileConnectMask(void)
{
    return TwsTopologyGetTaskData()->peer_profile_connect_mask;
}

bool TwsTopology_IsRemainActiveForHandsetEnabled(void)
{
    return TwsTopologyGetTaskData()->remain_active_for_handset;
}

bool TwsTopology_IsRemainActiveForPeerEnabled(void)
{
    return TwsTopologyGetTaskData()->remain_active_for_peer;
}

bool TwsTopology_IsPrimary(void)
{
    return (TwsTopology_GetRole() == tws_topology_role_primary);
}

bool TwsTopology_IsFullPrimary(void)
{
    return (   (TwsTopology_GetRole() == tws_topology_role_primary)
            && !TwsTopologyGetTaskData()->acting_in_role);
}

bool TwsTopology_IsSecondary(void)
{
    return (TwsTopology_GetRole() == tws_topology_role_secondary);
}

bool TwsTopology_IsActingPrimary(void)
{
    return ((TwsTopology_GetRole() == tws_topology_role_primary)
            && (TwsTopologyGetTaskData()->acting_in_role));
}

void TwsTopology_ProhibitHandover(bool prohibit)
{
    TwsTopologyGetTaskData()->app_prohibit_handover = prohibit;

    DEBUG_LOG_FN_ENTRY("TwsTopology_ProhibitHandover %d", prohibit);

    if(!prohibit)
    {
        twsTopology_TriggerHandoverEvent();
    }
}

bool TwsTopology_IsHandoverProhibited(void)
{
    DEBUG_LOG_FN_ENTRY("TwsTopology_IsHandoverProhibited %d",
                       TwsTopologyGetTaskData()->app_prohibit_handover);

    return TwsTopologyGetTaskData()->app_prohibit_handover;
}

void TwsTopology_ProhibitHandsetConnection(bool prohibit)
{
    TwsTopologyGetTaskData()->prohibit_connect_to_handset = prohibit;

    if (prohibit)
    {
        twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_PROHIBIT_CONNECT_TO_HANDSET);
    }
    else
    {
        twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_KICK);
    }
}

bool twsTopology_IsRunning(void)
{
    twsTopologyTaskData *twst = TwsTopologyGetTaskData();

    return (twst->stopping_state == twstop_state_started) ||
           (twst->stopping_state == twstop_state_stopping);
}

void TwsTopology_ConnectMruHandset(void)
{
    if (TwsTopology_IsPrimary())
    {
        DEBUG_LOG("TwsTopology_ConnectMruHandset");
        twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_USER_REQUEST_CONNECT_HANDSET);
    }
}

void TwsTopology_DisconnectLruHandset(void)
{
    if (TwsTopology_IsPrimary())
    {
        DEBUG_LOG("TwsTopology_DisconnectLruHandset");
        twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_USER_REQUEST_DISCONNECT_LRU_HANDSET);
    }
}

void TwsTopology_DisconnectAllHandsets(void)
{
    if (TwsTopology_IsPrimary())
    {
        DEBUG_LOG("TwsTopology_DisconnectAllHandsets");
        twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_USER_REQUEST_DISCONNECT_ALL_HANDSETS);
    }
}

void twsTopology_SetReconnectPostHandover(bool reconnect_post_handover)
{
    twsTopologyTaskData *twst = TwsTopologyGetTaskData();

    DEBUG_LOG("twsTopology_SetReconnectPostHandover reconnect_post_handover %d", reconnect_post_handover);
    twst->reconnect_post_handover = reconnect_post_handover;
}

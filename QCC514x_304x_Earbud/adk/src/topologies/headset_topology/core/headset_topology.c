    /*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Headset Topology component core.
*/

#include "headset_topology.h"
#include "headset_topology_private.h"
#include "headset_topology_config.h"
#include "headset_topology_rules.h"
#include "headset_topology_goals.h"
#include "headset_topology_client_msgs.h"
#include "headset_topology_private.h"
#include "headset_topology_procedure_system_stop.h"


#include "core/headset_topology_rules.h"

#include <logging.h>

#include <handset_service.h>
#include <bredr_scan_manager.h>
#include <connection_manager.h>
#include <power_manager.h>
#include <pairing.h>
#include <panic.h>

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_TYPE(headset_topology_message_t)
LOGGING_PRESERVE_MESSAGE_TYPE(hs_topology_internal_message_t)
LOGGING_PRESERVE_MESSAGE_ENUM(headset_topology_goals)

/*! Instance of the headset Topology. */
headsetTopologyTaskData headset_topology = {0};

static void headsetTopology_HandlePairingActivity(const PAIRING_ACTIVITY_T *message)
{
    UNUSED(message);
    DEBUG_LOG_VERBOSE("headsetTopology_HandlePairingActivity status=enum:pairingActivityStatus:%d",
                        message->status);
}

/*! \brief Take action following power's indication of imminent shutdown.*/
static void headsetTopology_HandlePowerShutdownPrepareInd(void)
{
    headsetTopologyTaskData *headset_taskdata = HeadsetTopologyGetTaskData();

    DEBUG_LOG_VERBOSE("headsetTopology_HandlePowerShutdownPrepareInd");
    /* Headset should stop being connectable during shutdown. */
    headset_taskdata->shutdown_in_progress = TRUE;
    appPowerShutdownPrepareResponse(HeadsetTopologyGetTask());
}

/*! \brief Generate handset related disconnection events . */
static void headsetTopology_HandleHandsetServiceDisconnectedInd(const HANDSET_SERVICE_DISCONNECTED_IND_T* ind)
{
    DEBUG_LOG_VERBOSE("headsetTopology_HandleHandsetServiceDisconnectedInd %04x,%02x,%06lx status %u", ind->addr.nap,
                                                                                               ind->addr.uap,
                                                                                               ind->addr.lap,
                                                                                               ind->status);

    if(ind->status == handset_service_status_link_loss)
    {
        HeadsetTopologyRules_SetEvent(HSTOP_RULE_EVENT_HANDSET_LINKLOSS);
    }
}

/*! \brief Print bluetooth address of the handset. */
static void headsetTopology_PrintBdaddr(const bdaddr addr)
{
    DEBUG_LOG_VERBOSE("headsetTopology_printbdaddr %04x,%02x,%06lx", addr.nap,
                                                                     addr.uap,
                                                                     addr.lap);
}

static void headsetTopology_MarkAsStopped(void)
{
    headsetTopologyTaskData *hst_taskdata = HeadsetTopologyGetTaskData();

    hst_taskdata->app_task = NULL;
    hst_taskdata->hstop_state = hstop_state_stopped;
}

static void headsetTopology_HandleStopTimeout(void)
{
    DEBUG_LOG_FN_ENTRY("headsetTopology_HandleStopTimeout");

    HeadsetTopology_SendStopCfm(hs_topology_status_fail);
    headsetTopology_MarkAsStopped();
}

static void headsetTopology_HandleStopCompletion(void)
{
    if (HeadsetTopologyGetTaskData()->hstop_state == hstop_state_stopping)
    {
        DEBUG_LOG_FN_ENTRY("headsetTopology_HandleStopCompletion");

        /* Send the stop message before clearing the app task below */
        HeadsetTopology_SendStopCfm(hs_topology_status_success);
        headsetTopology_MarkAsStopped();
    }
}


/*! \brief Headset Topology message handler.
 */
static void headsetTopology_HandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);
    DEBUG_LOG_VERBOSE("headsetTopology_HandleMessage. message id MESSAGE:hs_topology_internal_message_t:0x%x",id);

    /* handle messages */
    switch (id)
    {
        case PAIRING_ACTIVITY:
            headsetTopology_HandlePairingActivity(message);
            break;

        case HANDSET_SERVICE_CONNECTED_IND:
            {
                HANDSET_SERVICE_CONNECTED_IND_T *ind = (HANDSET_SERVICE_CONNECTED_IND_T *)message;
                DEBUG_LOG_INFO("headsetTopology_HandleMessage: HANDSET_SERVICE_CONNECTED_IND profiles_connected = %d",
                              ind->profiles_connected);
                headsetTopology_PrintBdaddr(ind->addr);
            }
            break;

        case HANDSET_SERVICE_DISCONNECTED_IND:
            DEBUG_LOG_DEBUG("headsetTopology_HandleMessage: HANDSET_SERVICE_DISCONNECTED_IND");
            headsetTopology_HandleHandsetServiceDisconnectedInd((HANDSET_SERVICE_DISCONNECTED_IND_T*)message);
            break;

        case CON_MANAGER_CONNECTION_IND:
            {
                CON_MANAGER_CONNECTION_IND_T *ind = (CON_MANAGER_CONNECTION_IND_T *)message;
                DEBUG_LOG_DEBUG("headsetTopology_HandleMessage: CON_MANAGER_CONNECTION_IND Connected = %d, Transport BLE = %d",
                                 ind->connected, ind->ble);
                headsetTopology_PrintBdaddr(ind->bd_addr);
            }
            break;

        /* Power indications */
        case APP_POWER_SHUTDOWN_PREPARE_IND:
            headsetTopology_HandlePowerShutdownPrepareInd();
            DEBUG_LOG_VERBOSE("headsetTopology_HandleMessage: APP_POWER_SHUTDOWN_PREPARE_IND");
            break;

        case HSTOP_INTERNAL_TIMEOUT_TOPOLOGY_STOP:
            headsetTopology_HandleStopTimeout();
            break;

        case PROC_SEND_HS_TOPOLOGY_MESSAGE_SYSTEM_STOP_FINISHED:
            headsetTopology_HandleStopCompletion();
            break;

        default:
            DEBUG_LOG_VERBOSE("headsetTopology_HandleMessage: Unhandled message MESSAGE:hs_topology_internal_message_t:0x%x", id);
            break;
      }
}


bool HeadsetTopology_Init(Task init_task)
{
    UNUSED(init_task);

    headsetTopologyTaskData *hst_taskdata = HeadsetTopologyGetTaskData();
    hst_taskdata->task.handler = headsetTopology_HandleMessage;
    hst_taskdata->goal_task.handler = HeadsetTopology_HandleGoalDecision;
    hst_taskdata->prohibit_connect_to_handset = FALSE;
    hst_taskdata->shutdown_in_progress = FALSE;
    hst_taskdata->hstop_state = hstop_state_stopped;

    /*Initialize Headset topology's goals and rules */
    
    HeadsetTopologyRules_Init(HeadsetTopologyGetGoalTask());
    HeadsetTopology_GoalsInit();

    /* Register with power to receive shutdown messages. */
    appPowerClientRegister(HeadsetTopologyGetTask());
    /* Allow topology to sleep */
    appPowerClientAllowSleep(HeadsetTopologyGetTask());

    /* register with handset service as we need disconnect and connect notification */
    HandsetService_ClientRegister(HeadsetTopologyGetTask());
    ConManagerRegisterConnectionsClient(HeadsetTopologyGetTask());
    Pairing_ActivityClientRegister(HeadsetTopologyGetTask());
    BredrScanManager_PageScanParametersRegister(&hs_page_scan_params);
    BredrScanManager_InquiryScanParametersRegister(&hs_inquiry_scan_params);

    TaskList_InitialiseWithCapacity(HeadsetTopologyGetMessageClientTasks(), MESSAGE_CLIENT_TASK_LIST_INIT_CAPACITY);

    return TRUE;
}


bool HeadsetTopology_Start(Task requesting_task)
{
    UNUSED(requesting_task);

    headsetTopologyTaskData *hst_taskdata = HeadsetTopologyGetTaskData();

    if (hst_taskdata->hstop_state == hstop_state_stopped)
    {
        DEBUG_LOG("HeadsetTopology_Start (normal start)");
        hst_taskdata->hstop_state = hstop_state_started;
        hst_taskdata->prohibit_connect_to_handset = FALSE;
        hst_taskdata->shutdown_in_progress = FALSE;
        HeadsetTopologyRules_ResetEvent(RULE_EVENT_ALL_EVENTS_MASK);
        /* Set the rule to get the headset rolling (EnableConnectable, AllowHandsetConnect) */
        HeadsetTopologyRules_SetEvent(HSTOP_RULE_EVENT_START);
    }
    else
    {
        DEBUG_LOG("HeadsetTopology_Start:Topology already started or is in process of stopping,topology state MESSAGE:hs_topology_state_t:0x%x", hst_taskdata->hstop_state);
    }

    return TRUE;
}


void HeadsetTopology_RegisterMessageClient(Task client_task)
{
   TaskList_AddTask(TaskList_GetFlexibleBaseTaskList(HeadsetTopologyGetMessageClientTasks()), client_task);
}


void HeadsetTopology_UnRegisterMessageClient(Task client_task)
{
   TaskList_RemoveTask(TaskList_GetFlexibleBaseTaskList(HeadsetTopologyGetMessageClientTasks()), client_task);
}


void HeadsetTopology_ProhibitHandsetConnection(bool prohibit)
{
    HeadsetTopologyGetTaskData()->prohibit_connect_to_handset = prohibit;

    if(prohibit)
    {
        HeadsetTopologyRules_SetEvent(HSTOP_RULE_EVENT_PROHIBIT_CONNECT_TO_HANDSET);
    }
    return;
}

bool HeadsetTopology_Stop(Task requesting_task)
{
    headsetTopologyTaskData *headset_top = HeadsetTopologyGetTaskData();
    headset_top->app_task = requesting_task;

    DEBUG_LOG_WARN("HeadsetTopology_Stop - topology state:0x%x", headset_top->hstop_state);

    if(headset_top->hstop_state == hstop_state_started)
    {
        uint32 timeout_ms = D_SEC(HeadsetTopologyConfig_HeadsetTopologyStopTimeoutS());
        DEBUG_LOG_DEBUG("HeadsetTopology_Stop(). Timeout:%u", timeout_ms);

        if (timeout_ms)
        {
            MessageSendLater(HeadsetTopologyGetTask(),
                             HSTOP_INTERNAL_TIMEOUT_TOPOLOGY_STOP, NULL,
                             timeout_ms);
        }
        HeadsetTopologyRules_SetEvent(HSTOP_RULE_EVENT_STOP);
    }
    else
    {
        if(headset_top->hstop_state == hstop_state_stopped)
        {
            DEBUG_LOG_WARN("HeadsetTopology_Stop - already stopped");
            HeadsetTopology_SendStopCfm(hs_topology_status_success);
        }
        else
        {
            DEBUG_LOG("HeadsetTopology_Stop -- already stopping");
        }
    }

    return TRUE;
}


void headsetTopology_StopHasStarted(void)
{
    DEBUG_LOG_FN_ENTRY("headsetTopology_StopHasStarted");

    HeadsetTopologyGetTaskData()->hstop_state = hstop_state_stopping;
}

bool headsetTopology_IsRunning(void)
{
    headsetTopologyTaskData *hst = HeadsetTopologyGetTaskData();

    return hst->app_task && (hst->hstop_state != hstop_state_stopped);
}

void HeadsetTopology_ConnectMruHandset(void)
{
    DEBUG_LOG("HeadsetTopology_ConnectMruHandset");
    HeadsetTopologyRules_SetEvent(HSTOP_RULE_EVENT_USER_REQUEST_CONNECT_HANDSET);
}

void HeadsetTopology_DisconnectLruHandset(void)
{
    DEBUG_LOG("HeadsetTopology_DisconnectLruHandset");
    HeadsetTopologyRules_SetEvent(HSTOP_RULE_EVENT_USER_REQUEST_DISCONNECT_LRU_HANDSET);
}

void HeadsetTopology_DisconnectAllHandsets(void)
{
    DEBUG_LOG("HeadsetTopology_DisconnectAllHandsets");
    HeadsetTopologyRules_SetEvent(HSTOP_RULE_EVENT_USER_REQUEST_DISCONNECT_ALL_HANDSETS);
}



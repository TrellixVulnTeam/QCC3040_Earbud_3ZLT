/*!
\copyright  Copyright (c) 2019 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Procedure for Primary to connect BR/EDR ACL to Handset.

\note       Whilst the procedure is running, if audio streaming is started the handset 
            connection is stopped but the procedure continues to be active.
            If the streaming stops within PROC_CONNECT_HANDSET_STREAMING_STOP_TIMEOUT_MS,
            the handset connection is resumed.
            If the streaming continues beyond 30s, the procedure completes returning failure status.

            Peer find role scanning is disabled when paging the first handset in order to
            get the very fast connection to the (first) handset.
            Once first handset is connected, other activities are resumed i.e. peer find
            role scanning. Which may result in taking longer to connect to second handset.
            Peer find role is only scanning when its an "acting primary", so it only impacts 
            the second handset connection time when the earbud is not connected to the secondary. 
*/

#include "tws_topology_procedure_connect_handset.h"
#include "tws_topology_procedures.h"
#include "tws_topology_config.h"
#include "tws_topology_primary_ruleset.h"
#include "tws_topology_common_primary_rule_functions.h"

#include <handset_service.h>
#include <handset_service_config.h>
#include <connection_manager.h>
#include <peer_find_role.h>
#include <av.h>

#include <logging.h>

#include <message.h>
#include <panic.h>

void TwsTopology_ProcedureConnectHandsetStart(Task result_task,
                                        procedure_start_cfm_func_t proc_start_cfm_fn,
                                        procedure_complete_func_t proc_complete_fn,
                                        Message goal_data);
void TwsTopology_ProcedureConnectHandsetCancel(procedure_cancel_cfm_func_t proc_cancel_cfm_fn);

const procedure_fns_t proc_connect_handset_fns = {
    TwsTopology_ProcedureConnectHandsetStart,
    TwsTopology_ProcedureConnectHandsetCancel,
};

typedef struct
{
    TaskData task;
    procedure_complete_func_t complete_fn;
    procedure_cancel_cfm_func_t cancel_fn;
    bool prepare_requested;
    bool audio_started;
    bdaddr handset_addr;
    uint32 profiles_requested;
} twsTopProcConnectHandsetTaskData;

twsTopProcConnectHandsetTaskData twstop_proc_connect_handset;

#define TwsTopProcConnectHandsetGetTaskData()     (&twstop_proc_connect_handset)
#define TwsTopProcConnectHandsetGetTask()         (&twstop_proc_connect_handset.task)

/*! Timeout if handset has not stopped streaming.

    If the timer expires, the procedure completes.
*/
#define PROC_CONNECT_HANDSET_STREAMING_STOP_TIMEOUT_MS    (30000)

/*! Internal messages use by this ConnectHandset procedure. */
typedef enum
{   /*! Internal message to complete the procedure. */
    PROC_CONNECT_HANDSET_INTERNAL_STREAMING_STOP_TIMEOUT,
} procConnetHandsetInternalMessages;

static void twsTopology_ProcConnectHandsetHandleMessage(Task task, MessageId id, Message message);

twsTopProcConnectHandsetTaskData twstop_proc_connect_handset = {twsTopology_ProcConnectHandsetHandleMessage};

/*! \brief Send a response to a PEER_FIND_ROLE_PREPARE_FOR_ROLE_SELECTION.

    This will only send the response if we have received a
    PEER_FIND_ROLE_PREPARE_FOR_ROLE_SELECTION, otherwise it will do
    nothing.

    Note: There should only ever be one response per
          PEER_FIND_ROLE_PREPARE_FOR_ROLE_SELECTION received, hence why
          this is guarded on the prepare_requested flag.
*/
static void twsTopology_ProcConnectHandsetPeerFindRolePrepareRespond(void)
{
    twsTopProcConnectHandsetTaskData* td = TwsTopProcConnectHandsetGetTaskData();

    if (td->prepare_requested)
    {
        PeerFindRole_PrepareResponse(TwsTopProcConnectHandsetGetTask());
        td->prepare_requested = FALSE;
    }
}

static void twsTopology_ProcConnectHandsetResetTaskData(void)
{
    twsTopProcConnectHandsetTaskData* td = TwsTopProcConnectHandsetGetTaskData();

    memset(td, 0, sizeof(*td));
}

static void twsTopology_ProcConnectHandsetResetProc(void)
{
    DEBUG_LOG("twsTopology_ProcConnectHandsetResetProc");

    twsTopology_ProcConnectHandsetPeerFindRolePrepareRespond();

    MessageCancelAll(TwsTopProcConnectHandsetGetTask(), PROC_CONNECT_HANDSET_INTERNAL_STREAMING_STOP_TIMEOUT);
    PeerFindRole_DisableScanning(FALSE);
    PeerFindRole_UnregisterPrepareClient(TwsTopProcConnectHandsetGetTask());
    ConManagerUnregisterConnectionsClient(TwsTopProcConnectHandsetGetTask());
    appAvStatusClientUnregister(TwsTopProcConnectHandsetGetTask());

    twsTopology_ProcConnectHandsetResetTaskData();
}

void TwsTopology_ProcedureConnectHandsetStart(Task result_task,
                                        procedure_start_cfm_func_t proc_start_cfm_fn,
                                        procedure_complete_func_t proc_complete_fn,
                                        Message goal_data)
{
    twsTopProcConnectHandsetTaskData* td = TwsTopProcConnectHandsetGetTaskData();
    TWSTOP_PRIMARY_GOAL_CONNECT_HANDSET_T* chp = (TWSTOP_PRIMARY_GOAL_CONNECT_HANDSET_T*)goal_data;

    UNUSED(result_task);

    DEBUG_LOG("TwsTopology_ProcedureConnectHandsetStart profiles 0x%x", chp->profiles);
    twsTopology_ProcConnectHandsetResetTaskData();

    td->task.handler = twsTopology_ProcConnectHandsetHandleMessage;
    td->profiles_requested = chp->profiles;

    /* Block scanning temporarily while we are connecting */
    PeerFindRole_DisableScanning(TRUE);

    /* save state to perform the procedure */
    td->complete_fn = proc_complete_fn;

    /* start the procedure */
    if (appDeviceGetHandsetBdAddr(&td->handset_addr))
    {
        PeerFindRole_RegisterPrepareClient(TwsTopProcConnectHandsetGetTask());
        /* Register with AV to receive notifications of A2DP and AVRCP activity */
        appAvStatusClientRegister(TwsTopProcConnectHandsetGetTask());

        HandsetService_ReconnectRequest(TwsTopProcConnectHandsetGetTask(), td->profiles_requested);
        ConManagerRegisterConnectionsClient(TwsTopProcConnectHandsetGetTask());

        proc_start_cfm_fn(tws_topology_procedure_connect_handset, procedure_result_success);
    }
    else
    {
        DEBUG_LOG("TwsTopology_ProcedureConnectHandsetStart shouldn't be called with no paired handset");
        Panic();
    }
}

void TwsTopology_ProcedureConnectHandsetCancel(procedure_cancel_cfm_func_t proc_cancel_cfm_fn)
{
    twsTopProcConnectHandsetTaskData* td = TwsTopProcConnectHandsetGetTaskData();

    DEBUG_LOG("TwsTopology_ProcedureConnectHandsetCancel");

    td->complete_fn = NULL;
    td->cancel_fn = proc_cancel_cfm_fn;
    HandsetService_StopReconnect(TwsTopProcConnectHandsetGetTask());
}

static void twsTopology_ProcConnectHandsetHandleHandsetMpConnectCfm(const HANDSET_SERVICE_MP_CONNECT_CFM_T *cfm)
{
    twsTopProcConnectHandsetTaskData* td = TwsTopProcConnectHandsetGetTaskData();

    DEBUG_LOG("twsTopology_ProcConnectHandsetHandleHandsetMpConnectCfm status enum:handset_service_status_t:%d", cfm->status);

    /* Topology shall rely on handset service's responsibilty to establish handset connection
     * and notify MP_CONNECT_CFM after what constitues to be connection as per handset service.
     * Topology shall not inspect and match b/w connected and requested profiles.
     * Reason: All requested profiles may not have to be connected for service to treat as
     * handset connection success */

    if (cfm->status != handset_service_status_cancelled)
    {
        if(!td->audio_started)
        {
            /* The procedure could be finished by either HANDSET_SERVICE_MP_CONNECT_CFM or 
             * HANDSET_SERVICE_MP_CONNECT_STOP_CFM but there is no guarantee which order they
             * arrive in so it has to handle them arriving in either order. */
            if(cfm->status == handset_service_status_success)
            {
                if (td->complete_fn)
                {
                    td->complete_fn(tws_topology_procedure_connect_handset, procedure_result_success);
                }
                else if (td->cancel_fn)
                {
                    td->cancel_fn(tws_topology_procedure_connect_handset, procedure_result_success);
                }
            }
            else
            {
                if (td->complete_fn)
                {
                    td->complete_fn(tws_topology_procedure_connect_handset, procedure_result_failed);
                }
                else if (td->cancel_fn)
                {
                    td->cancel_fn(tws_topology_procedure_connect_handset, procedure_result_success);
                }
            }
            twsTopology_ProcConnectHandsetResetProc();
        }
    }
    else
    {
        /* A status of handset_service_status_cancelled means the connect
           request was cancelled by a separate disconnect request. In the
           tws topology we should never overlap connect & disconnect requests
           like this so it is an error. */
        Panic();
    }
}

static void twsTopology_ProcConnectHandsetHandleHandsetMpConnectStopCfm(const HANDSET_SERVICE_MP_CONNECT_STOP_CFM_T* cfm)
{
    twsTopProcConnectHandsetTaskData* td = TwsTopProcConnectHandsetGetTaskData();

    DEBUG_LOG("twsTopology_ProcConnectHandsetHandleHandsetMpConnectStopCfm status enum:handset_service_status_t:%d",cfm->status);

    twsTopology_ProcConnectHandsetPeerFindRolePrepareRespond();

    /* If the procedure was cancelled, let the topology know and tidy up
     * this procedure. If not cancelled, wait for the
     * HANDSET_SERVICE_MP_CONNECT_CFM instead. */
    if (td->cancel_fn)
    {
        td->cancel_fn(tws_topology_procedure_connect_handset, procedure_result_success);
        twsTopology_ProcConnectHandsetResetProc();
    }
}

static void twsTopology_ProcConnectHandsetHandleStreamingStopTimeout(void)
{
    twsTopProcConnectHandsetTaskData* td = TwsTopProcConnectHandsetGetTaskData();

    DEBUG_LOG("twsTopology_ProcConnectHandsetHandleStreamingStopTimeout");

    if (td->complete_fn)
    {
        td->complete_fn(tws_topology_procedure_connect_handset, procedure_result_failed);
    }
    else if (td->cancel_fn)
    {
        td->cancel_fn(tws_topology_procedure_connect_handset, procedure_result_success);
    }
    twsTopology_ProcConnectHandsetResetProc();
}

static void twsTopology_ProcConnectHandsetHandlePeerFindRolePrepareForRoleSelection(void)
{
    twsTopProcConnectHandsetTaskData* td = TwsTopProcConnectHandsetGetTaskData();

    DEBUG_LOG("twsTopology_ProcConnectHandsetHandlePeerFindRolePrepareForRoleSelection");

    HandsetService_StopReconnect(TwsTopProcConnectHandsetGetTask());
    td->prepare_requested = TRUE;
}

static void twsTopology_ProcConnectHandsetHandleHandleAvA2dpAudioConnected(void)
{
    /* Not expected for singlepoint. */
    if (handsetService_BredrAclMaxConnections() <= 1)
    {
        DEBUG_LOG("twsTopology_ProcConnectHandsetHandleHandleAvA2dpAudioConnected, not expected for singlepoint");
        Panic();
    }
    twsTopProcConnectHandsetTaskData* td = TwsTopProcConnectHandsetGetTaskData();
    DEBUG_LOG("twsTopology_ProcConnectHandsetHandleHandleAvA2dpAudioConnected");

    HandsetService_StopReconnect(TwsTopProcConnectHandsetGetTask());
    MessageSendLater(TwsTopProcConnectHandsetGetTask(), PROC_CONNECT_HANDSET_INTERNAL_STREAMING_STOP_TIMEOUT, 
                         NULL, PROC_CONNECT_HANDSET_STREAMING_STOP_TIMEOUT_MS);
    td->audio_started = TRUE;
}

static void twsTopology_ProcConnectHandsetHandleHandleAvA2dpAudioDisconnected(void)
{
    /* Not expected for singlepoint. */
    if (handsetService_BredrAclMaxConnections() <= 1)
    {
        DEBUG_LOG("twsTopology_ProcConnectHandsetHandleHandleAvA2dpAudioDisconnected, not expected for singlepoint");
        Panic();
    }
    twsTopProcConnectHandsetTaskData* td = TwsTopProcConnectHandsetGetTaskData();
    DEBUG_LOG("twsTopology_ProcConnectHandsetHandleHandleAvA2dpAudioDisconnected");

    /* start the reconnection again as audio stopped before 30s timeout. */
    if (td->audio_started)
    {
        HandsetService_ReconnectRequest(TwsTopProcConnectHandsetGetTask(), td->profiles_requested);
    }

    td->audio_started = FALSE;
    MessageCancelAll(TwsTopProcConnectHandsetGetTask(), PROC_CONNECT_HANDSET_INTERNAL_STREAMING_STOP_TIMEOUT);
}

/*! Use connection manager indication to re-enable scanning once we connect to handset

    We will do this anyway once we are fully connected to the handset (all selected
    profiles), but that can take some time.

    \param conn_ind The Connection manager indication
 */
static void twsTopology_ProcConnectHandsetHandleConMgrConnInd(const CON_MANAGER_CONNECTION_IND_T *conn_ind)
{
    DEBUG_LOG("twsTopology_ProcConnectHandsetHandleConMgrConnInd [%04x,%02x,%06lx] ble:%d conn:%d",
              conn_ind->bd_addr.nap,
              conn_ind->bd_addr.uap,
              conn_ind->bd_addr.lap,
              conn_ind->ble,
              conn_ind->connected);

    if (!conn_ind->ble
        && conn_ind->connected
        && appDeviceIsHandset(&conn_ind->bd_addr))
    {
        /* Additional call here as we only care about the handset connection,
            not the profiles */
        PeerFindRole_DisableScanning(FALSE);
        ConManagerUnregisterConnectionsClient(TwsTopProcConnectHandsetGetTask());
    }
}

static void twsTopology_ProcConnectHandsetHandleMessage(Task task, MessageId id, Message message)
{
    twsTopProcConnectHandsetTaskData* td = TwsTopProcConnectHandsetGetTaskData();

    UNUSED(task);
    UNUSED(message);

    if (!td->complete_fn && !td->cancel_fn)
    {
        /* If neither callback is set this procedure is not active so ignore any messages */
        return;
    }

    if (   (id == AV_A2DP_AUDIO_CONNECTED || id == AV_A2DP_AUDIO_DISCONNECTED)
        && (handsetService_BredrAclMaxConnections() <= 1))
    {
        /* For singlepoint do not stop reconnection on A2DP connection and 
           restart on A2DP disconnection. It is only required for multipoint
           to avoid the audio glitches */
        return;
    }

    switch (id)
    {
    case HANDSET_SERVICE_MP_CONNECT_CFM:
        twsTopology_ProcConnectHandsetHandleHandsetMpConnectCfm((const HANDSET_SERVICE_MP_CONNECT_CFM_T *)message);
        break;

    case HANDSET_SERVICE_MP_CONNECT_STOP_CFM:
        twsTopology_ProcConnectHandsetHandleHandsetMpConnectStopCfm((const HANDSET_SERVICE_MP_CONNECT_STOP_CFM_T *)message);
        break;

    case CON_MANAGER_CONNECTION_IND:
        twsTopology_ProcConnectHandsetHandleConMgrConnInd((const CON_MANAGER_CONNECTION_IND_T *)message);
        break;

    case PEER_FIND_ROLE_PREPARE_FOR_ROLE_SELECTION:
        twsTopology_ProcConnectHandsetHandlePeerFindRolePrepareForRoleSelection();
        break;

    /* AV messages */
    case AV_A2DP_AUDIO_CONNECTED:
        twsTopology_ProcConnectHandsetHandleHandleAvA2dpAudioConnected();
        break;

    case AV_A2DP_AUDIO_DISCONNECTED:
        twsTopology_ProcConnectHandsetHandleHandleAvA2dpAudioDisconnected();
        break;

    /* Internal message */
    case PROC_CONNECT_HANDSET_INTERNAL_STREAMING_STOP_TIMEOUT:
        twsTopology_ProcConnectHandsetHandleStreamingStopTimeout();
        break;

    default:
        DEBUG_LOG("twsTopology_ProcConnectHandsetHandleMessage unhandled id MESSAGE:0x%x", id);
        break;
    }
}

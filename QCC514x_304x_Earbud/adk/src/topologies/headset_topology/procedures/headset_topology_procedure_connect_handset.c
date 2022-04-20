/*!
\copyright  Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Procedure for headset to connect BR/EDR ACL to Handset.

\note       Whilst the procedure is running, if audio streaming is started the handset 
            connection is stopped but the procedure continues to be active.
            If the streaming stops within PROC_CONNECT_HANDSET_STREAMING_STOP_TIMEOUT_MS,
            the handset connection is resumed.
            If the streaming continues beyond 30s, the procedure completes returning failure status.
*/

#include "headset_topology_procedure_connect_handset.h"
#include "headset_topology_config.h"
#include "core/headset_topology_rules.h"
#include "procedures.h"
#include "headset_topology_procedures.h"

#include <av.h>
#include <handset_service.h>
#include <handset_service_config.h>
#include <connection_manager.h>
#include <logging.h>

#include <message.h>
#include <panic.h>

void HeadsetTopology_ProcedureConnectHandsetStart(Task result_task,
                                                  procedure_start_cfm_func_t proc_start_cfm_fn,
                                                  procedure_complete_func_t proc_complete_fn,
                                                  Message goal_data);
static void HeadsetTopology_ProcedureConnectHandsetCancel(procedure_cancel_cfm_func_t proc_cancel_cfm_fn);
static void headsetTopology_ProcConnectHandsetResetAvStatus(void);

const procedure_fns_t hs_proc_connect_handset_fns = {
    HeadsetTopology_ProcedureConnectHandsetStart,
    HeadsetTopology_ProcedureConnectHandsetCancel,
};


typedef struct
{
    TaskData task;
    procedure_complete_func_t complete_fn;
    procedure_cancel_cfm_func_t cancel_fn;
    bool audio_started;
    bdaddr handset_addr;
    uint32 profiles_requested;
} headsetTopProcConnectHandsetTaskData;

headsetTopProcConnectHandsetTaskData headsettop_proc_connect_handset;

#define HeadsetTopProcConnectHandsetGetTaskData() (&headsettop_proc_connect_handset)
#define HeadsetTopProcConnectHandsetGetTask()     (&headsettop_proc_connect_handset.task)

/*! Timeout if handset has not stopped streaming.

    If the timer expires, the procedure completes.
*/
#define PROC_CONNECT_HANDSET_STREAMING_STOP_TIMEOUT_MS    (30000)

/*! Internal messages use by this ConnectHandset procedure. */
typedef enum
{   /*! Internal message to complete the procedure. */
    PROC_CONNECT_HANDSET_INTERNAL_STREAMING_STOP_TIMEOUT,
} procConnetHandsetInternalMessages;

static void headsetTopology_ProcConnectHandsetHandleMessage(Task task, MessageId id, Message message);

headsetTopProcConnectHandsetTaskData headsettop_proc_connect_handset = {headsetTopology_ProcConnectHandsetHandleMessage};

static void headsetTopology_ProcConnectHandsetResetTaskData(void)
{
    headsetTopProcConnectHandsetTaskData* td = HeadsetTopProcConnectHandsetGetTaskData();

    memset(td, 0, sizeof(*td));
}

static void headsetTopology_ProcConnectHandsetResetCompleteFunc(void)
{
    headsetTopProcConnectHandsetTaskData* td = HeadsetTopProcConnectHandsetGetTaskData();
    td->complete_fn = NULL;

}

static void headsetTopology_ProcConnectHandsetResetCancelFunc(void)
{
    headsetTopProcConnectHandsetTaskData* td = HeadsetTopProcConnectHandsetGetTaskData();
    td->cancel_fn = NULL;

}

void HeadsetTopology_ProcedureConnectHandsetStart(Task result_task,
                                                  procedure_start_cfm_func_t proc_start_cfm_fn,
                                                  procedure_complete_func_t proc_complete_fn,
                                                  Message goal_data)
{
    UNUSED(result_task);
    headsetTopProcConnectHandsetTaskData* td = HeadsetTopProcConnectHandsetGetTaskData();
    HSTOP_GOAL_CONNECT_HANDSET_T* chp = (HSTOP_GOAL_CONNECT_HANDSET_T*)goal_data;

    DEBUG_LOG_VERBOSE("HeadsetTopology_ProcedureConnectHandsetStart profiles 0x%x", chp->profiles);
    headsetTopology_ProcConnectHandsetResetTaskData();

    td->task.handler = headsetTopology_ProcConnectHandsetHandleMessage;
    td->profiles_requested = chp->profiles;
    td->complete_fn = proc_complete_fn;

    /* start the procedure */
    if (appDeviceGetHandsetBdAddr(&td->handset_addr))
    {
        HandsetService_ReconnectRequest(HeadsetTopProcConnectHandsetGetTask(), chp->profiles);

        /* Register with AV to receive notifications of A2DP and AVRCP activity */
        appAvStatusClientRegister(HeadsetTopProcConnectHandsetGetTask());

        proc_start_cfm_fn(hs_topology_procedure_connect_handset, procedure_result_success);
    }
    else
    {
        DEBUG_LOG_ERROR("HeadsetTopology_ProcedureConnectHandsetStart shouldn't be called with no paired handset");
        Panic();
    }
}

void HeadsetTopology_ProcedureConnectHandsetCancel(procedure_cancel_cfm_func_t proc_cancel_cfm_fn)
{
    headsetTopProcConnectHandsetTaskData* td = HeadsetTopProcConnectHandsetGetTaskData();
    DEBUG_LOG_VERBOSE("HeadsetTopology_ProcedureConnectHandsetCancel ");

    td->complete_fn = NULL;
    td->cancel_fn = proc_cancel_cfm_fn;

    HandsetService_StopReconnect(HeadsetTopProcConnectHandsetGetTask());
}

static void headsetTopology_ProcConnectHandsetResetAvStatus(void)
{
    MessageCancelAll(HeadsetTopProcConnectHandsetGetTask(), PROC_CONNECT_HANDSET_INTERNAL_STREAMING_STOP_TIMEOUT);

    appAvStatusClientUnregister(HeadsetTopProcConnectHandsetGetTask());
}


static void headsetTopology_ProcConnectHandsetHandleHandsetMpConnectCfm(const HANDSET_SERVICE_MP_CONNECT_CFM_T *cfm)
{
    headsetTopProcConnectHandsetTaskData* td = HeadsetTopProcConnectHandsetGetTaskData();

    DEBUG_LOG_VERBOSE("headsetTopology_ProcConnectHandsetHandleHandsetMpConnectCfm status enum:handset_service_status_t:%d", cfm->status);

    /* Topology shall rely on handset service's responsibilty to establish handset connection
     * and notify CONNECT_CFM after what constitues to be connection as per handset service.
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
                    td->complete_fn(hs_topology_procedure_connect_handset, procedure_result_success);
                }
                else if (td->cancel_fn)
                {
                    td->cancel_fn(hs_topology_procedure_connect_handset, procedure_result_success);
                }
            }
            else
            {
                if (td->complete_fn)
                {
                    td->complete_fn(hs_topology_procedure_connect_handset, procedure_result_failed);
                }
                else if (td->cancel_fn)
                {
                    td->cancel_fn(hs_topology_procedure_connect_handset, procedure_result_success);
                    headsetTopology_ProcConnectHandsetResetCancelFunc();
                }
            }
            headsetTopology_ProcConnectHandsetResetAvStatus();
            headsetTopology_ProcConnectHandsetResetCompleteFunc();
        }
    }
    else
    {
        DEBUG_LOG("headsetTopology_ProcConnectHandsetHandleHandsetMpConnectCfm, connect procedure has been cancelled");
    }
}

static void headsetTopology_ProcConnectHandsetHandleHandsetMpConnectStopCfm(const HANDSET_SERVICE_MP_CONNECT_STOP_CFM_T* cfm)
{
    headsetTopProcConnectHandsetTaskData* td = HeadsetTopProcConnectHandsetGetTaskData();

    DEBUG_LOG("headsetTopology_ProcConnectHandsetHandleHandsetMpConnectStopCfm status enum:handset_service_status_t:%d", cfm->status);

    /* If the procedure was cancelled, let the topology know and tidy up
       this procedure. If not cancelled, wait for the
       HANDSET_SERVICE_MP_CONNECT_CFM instead. */
    if (td->cancel_fn)
    {
        td->cancel_fn(hs_topology_procedure_connect_handset, procedure_result_success);
        headsetTopology_ProcConnectHandsetResetAvStatus();
        headsetTopology_ProcConnectHandsetResetCancelFunc();
    }
}

static void headsetTopology_ProcConnectHandsetHandleStreamingStopTimeout(void)
{
    headsetTopProcConnectHandsetTaskData* td = HeadsetTopProcConnectHandsetGetTaskData();

    DEBUG_LOG("headsetTopology_ProcConnectHandsetHandleStreamingStopTimeout");

    if (td->complete_fn)
    {
        td->complete_fn(hs_topology_procedure_connect_handset, procedure_result_failed);
    }
    else if (td->cancel_fn)
    {
        td->cancel_fn(hs_topology_procedure_connect_handset, procedure_result_success);
    }
    headsetTopology_ProcConnectHandsetResetAvStatus();
    headsetTopology_ProcConnectHandsetResetTaskData();
}

static void headsetTopology_ProcConnectHandsetHandleHandleAvA2dpAudioConnected(void)
{
    /* Not expected for singlepoint. */
    if (handsetService_BredrAclMaxConnections() <= 1)
    {
        DEBUG_LOG("headsetTopology_ProcConnectHandsetHandleHandleAvA2dpAudioConnected, not expected for singlepoint");
        Panic();
    }
    headsetTopProcConnectHandsetTaskData* td = HeadsetTopProcConnectHandsetGetTaskData();
    DEBUG_LOG("headsetTopology_ProcConnectHandsetHandleHandleAvA2dpAudioConnected");

    HandsetService_StopReconnect(HeadsetTopProcConnectHandsetGetTask());
    MessageSendLater(HeadsetTopProcConnectHandsetGetTask(), PROC_CONNECT_HANDSET_INTERNAL_STREAMING_STOP_TIMEOUT, 
                         NULL, PROC_CONNECT_HANDSET_STREAMING_STOP_TIMEOUT_MS);
    td->audio_started = TRUE;
}

static void headsetTopology_ProcConnectHandsetHandleHandleAvA2dpAudioDisconnected(void)
{
    /* Not expected for singlepoint. */
    if (handsetService_BredrAclMaxConnections() <= 1)
    {
        DEBUG_LOG("headsetTopology_ProcConnectHandsetHandleHandleAvA2dpAudioDisconnected, not expected for singlepoint");
        Panic();
    }
    headsetTopProcConnectHandsetTaskData* td = HeadsetTopProcConnectHandsetGetTaskData();
    DEBUG_LOG("headsetTopology_ProcConnectHandsetHandleHandleAvA2dpAudioDisconnected");

    /* start the reconnection again as audio stopped before 30s timeout. */
    if (td->audio_started)
    {
        HandsetService_ReconnectRequest(HeadsetTopProcConnectHandsetGetTask(), td->profiles_requested);
    }

    td->audio_started = FALSE;
    MessageCancelAll(HeadsetTopProcConnectHandsetGetTask(), PROC_CONNECT_HANDSET_INTERNAL_STREAMING_STOP_TIMEOUT);
}

static void headsetTopology_ProcConnectHandsetHandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);

    headsetTopProcConnectHandsetTaskData* td = HeadsetTopProcConnectHandsetGetTaskData();

    /* ignore any delivered messages if no longer active */
    if ((td->complete_fn == NULL) && (td->cancel_fn == NULL))
    {
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
            headsetTopology_ProcConnectHandsetHandleHandsetMpConnectCfm((const HANDSET_SERVICE_MP_CONNECT_CFM_T *)message);
            break;

        case HANDSET_SERVICE_MP_CONNECT_STOP_CFM:
            headsetTopology_ProcConnectHandsetHandleHandsetMpConnectStopCfm((const HANDSET_SERVICE_MP_CONNECT_STOP_CFM_T *)message);
            break;

        /* AV messages */
        case AV_A2DP_AUDIO_CONNECTED:
            headsetTopology_ProcConnectHandsetHandleHandleAvA2dpAudioConnected();
            break;

        case AV_A2DP_AUDIO_DISCONNECTED:
            headsetTopology_ProcConnectHandsetHandleHandleAvA2dpAudioDisconnected();
            break;

        /* Internal message */
        case PROC_CONNECT_HANDSET_INTERNAL_STREAMING_STOP_TIMEOUT:
            headsetTopology_ProcConnectHandsetHandleStreamingStopTimeout();
            break;

        default:
            DEBUG_LOG_VERBOSE("headsetTopology_ProcConnectHandsetHandleMessage unhandled id MESSAGE:0x%x", id);
            break;
    }
}


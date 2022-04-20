/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      This file contains procedures that perform handover operation as well as cancellation
            of handover operation. Cancellation of handover is performed in case of cancellation
            request from script engine as well as during internal handover failure.

            Handover procedure first invokes a set of procedures before starting actual handover
            process. They are prerequisite procedures before starting handover i.e. role change 
            client, enable le connectable handset and disconnect le connections.

            In case of handover cancellation request from script engine or handover failure, system
            needs to revert back to previous pre handover state. So handover cancel/fail procedure also
            invokes a set of procedures(enable le connectable handset and role change clients) to bring
            the system back in pre handover state.

            Operation of handover procedure is different from other procedures. Other procedures are
            invoked via relevent scripts. Handover procedure is also invoked by dynamic handover script
            but internally it invokes required start up procedures or cancellation procedures. These
            procedures are invoked with in handover start or cancellation/failure procedures to make
            sure that they get completed. If these separate procedures get called by the script
            engine and get canclelled during the process then this can result in some procedure
            steps not being completed(e.g. page/inquiry scan not enabled again). So to avoid this type
            of situation, these procedures are executed within handover start or cancellation procedures.

*/
#include "tws_topology_private.h"
#include "tws_topology_config.h"
#include "tws_topology.h"
#include "tws_topology_procedure_handover.h"
#include "tws_topology_procedures.h"
#include "tws_topology_goals.h"
#include "handover_profile.h"
#include "tws_topology_procedure_notify_role_change_clients.h"
#include "tws_topology_procedure_permit_bt.h"
#include "tws_topology_procedure_disconnect_le_connections.h"
#include <bt_device.h>
#include <logging.h>
#include <message.h>
#include <panic.h>



/*! handover return status */
typedef enum
{
    handover_success = 0,
    handover_failed,
    handover_timedout
}handover_result_t;

/*! handover procedure task data */
typedef struct
{
    TaskData task;
    /*! handover retry attempts counter */
    uint8 handover_retry_count;
    /*! counter to track which internal handover procedure being executed */
    uint8 handover_procedure_count;
    /*! Flag to decide if handover start procedures need executing
        or cancellation procedures need executing */
    bool handover_failed_or_cancelled;
    /*! Overall handover result */
    uint8 handover_result;
    /*! Callback used by handover procedure to indicate if it has completed. */
    procedure_complete_func_t complete_fn;
    /*! Callback used by handover cancellation procedure to indicate if it
        has completed. */
    procedure_cancel_cfm_func_t proc_cancel_cfm_fn;
} twsTopProcHandoverTaskData;

/*! handover procedure message handler */
static void twsTopology_ProcHandoverHandleMessage(Task task, MessageId id, Message message);

twsTopProcHandoverTaskData twstop_proc_handover = {.task = twsTopology_ProcHandoverHandleMessage};

#define TwsTopProcHandoverGetTaskData()     (&twstop_proc_handover)
#define TwsTopProcHandoverGetTask()         (&twstop_proc_handover.task)

typedef enum
{
    /*! Internal message to retry the handover */
    TWS_TOP_PROC_HANDOVER_INTERNAL_RETRY,
    /*! Internal message to cancel the retry of handover */
    TWS_TOP_PROC_HANDOVER_INTERNAL_CANCEL_RETRY,
} tws_top_proc_handover_internal_message_t;

void TwsTopology_ProcedureHandoverStart(Task result_task,
                                        procedure_start_cfm_func_t proc_start_cfm_fn,
                                        procedure_complete_func_t proc_complete_fn,
                                        Message goal_data);

void TwsTopology_ProcedureHandoverCancel(procedure_cancel_cfm_func_t proc_cancel_cfm_fn);

void twsTopology_ProcHandoverProcedureStart(void);
void twsTopology_ProcHandoverProcedureCancel(void);

const procedure_fns_t proc_handover_fns = {
    TwsTopology_ProcedureHandoverStart,
    TwsTopology_ProcedureHandoverCancel,
};

const procedure_fns_t *handover_start_proc_fns[] = { & proc_notify_role_change_clients_fns,
                                                     & proc_permit_bt_fns,
                                                     /* This must be the final procedure in the list */
                                                     & proc_disconnect_le_connections_fns,
                                                   };

const Message handover_start_proc_goal_data[] = { PROC_NOTIFY_ROLE_CHANGE_CLIENTS_FORCE_NOTIFICATION,
                                                  PROC_PERMIT_BT_DISABLE,
                                                  NO_DATA
                                                };

const procedure_fns_t *handover_cancel_proc_fns[] = { & proc_permit_bt_fns,
                                                      & proc_notify_role_change_clients_fns,
                                                    };

const Message handover_cancel_proc_goal_data[] = { PROC_PERMIT_BT_ENABLE,
                                                   PROC_NOTIFY_ROLE_CHANGE_CLIENTS_CANCEL_NOTIFICATION
                                                 };

/*! The array index of the disconnect LE connections procedure.
    The disconnect LE connections procedure must be the final procedure in the
    list of handover_start_proc_fns so that it can be re-run in isolation before
    starting each handover re-try. This ensures all LE ACLs are disconnect before
    handover is attempted. */
#define DISCONNECT_LE_CONNECTIONS_INDEX 2
COMPILE_TIME_ASSERT(DISCONNECT_LE_CONNECTIONS_INDEX == (ARRAY_DIM(handover_start_proc_fns) - 1), proc_disconnect_le_connections_fns_out_of_position);

static void twsTopology_ProcHandoverReset(void)
{
    twsTopProcHandoverTaskData* td = TwsTopProcHandoverGetTaskData();
    MessageCancelFirst(TwsTopProcHandoverGetTask(), TWS_TOP_PROC_HANDOVER_INTERNAL_RETRY);
    MessageCancelFirst(TwsTopProcHandoverGetTask(), TWS_TOP_PROC_HANDOVER_INTERNAL_CANCEL_RETRY);
    td->handover_retry_count = 0;
    td->handover_procedure_count = 0;
    td->handover_failed_or_cancelled = FALSE;
    td->complete_fn = NULL;
    td->proc_cancel_cfm_fn = NULL;
    HandoverProfile_ClientUnregister(TwsTopProcHandoverGetTask());
    memset(&TwsTopologyGetTaskData()->handover_info,0,sizeof(handover_data_t));
    /*There might be a race condition of HandoverReset while tws_topology_goal_dynamic_handover
     is in vm_queue. Therefore cancel the rule message or the app may PANIC when trying to handle it */
    MessageCancelFirst(TwsTopologyGetGoalTask(), tws_topology_goal_dynamic_handover);
}

static handover_result_t twsTopology_ProcGetStatus(handover_profile_status_t status)
{
    handover_result_t result=handover_success;

    DEBUG_LOG("twsTopology_ProcGetStatus() Status: %d", status);

    /* Handle return status from procedure */
    switch (status)
    {
        case HANDOVER_PROFILE_STATUS_SUCCESS:
            /* Return success */
        break;

        case HANDOVER_PROFILE_STATUS_PEER_CONNECT_FAILED:
        case HANDOVER_PROFILE_STATUS_PEER_CONNECT_CANCELLED:
        case HANDOVER_PROFILE_STATUS_PEER_DISCONNECTED:
        case HANDOVER_PROFILE_STATUS_PEER_LINKLOSS:
        case HANDOVER_PROFILE_STATUS_HANDOVER_FAILURE:
            result = handover_failed;
        break;

        case HANDOVER_PROFILE_STATUS_HANDOVER_TIMEOUT:
        case HANDOVER_PROFILE_STATUS_HANDOVER_VETOED:
        {
            twsTopProcHandoverTaskData* td = TwsTopProcHandoverGetTaskData();
            td->handover_retry_count++;
            if(td->handover_retry_count >= TwsTopologyConfig_HandoverMaxRetryAttempts())
            {
                /* we have exhausted maximum number of handover retry attempts, flag failed event */
                td->handover_retry_count = 0;
                result = handover_failed;
            }
            else
            {
                result = handover_timedout;
            }
        }
        break;
        default:
        break;
    }

    return result;
}

static void twsTopology_ProcHandoverFailedOrCancelled(void)
{
    twsTopProcHandoverTaskData* td = TwsTopProcHandoverGetTaskData();
    DEBUG_LOG("twsTopology_ProcHandoverFailedOrCancelled");
    td->handover_procedure_count = 0;
    td->handover_failed_or_cancelled = TRUE;
    MessageCancelFirst(TwsTopProcHandoverGetTask(), TWS_TOP_PROC_HANDOVER_INTERNAL_RETRY);
    MessageCancelFirst(TwsTopProcHandoverGetTask(), TWS_TOP_PROC_HANDOVER_INTERNAL_CANCEL_RETRY);
    /* Handover has failed or cancelled so system needs to go back to pre handover state.
     * Start executing cancellation procedures to bring back system in pre handover state. */
    twsTopology_ProcHandoverProcedureCancel();
}

static void twsTopology_ProcHandoverStart(void)
{
    twsTopProcHandoverTaskData* td = TwsTopProcHandoverGetTaskData();
    handover_profile_status_t handover_status = HANDOVER_PROFILE_STATUS_HANDOVER_FAILURE;

    /* check if the handover reason is still valid to proceed with next attempt.
     * In a time for next attempt, there could be recommendation received to cancel it */
    if(appDeviceIsBredrHandsetConnected() && TwsTopologyGetTaskData()->handover_info.reason)
    {
        DEBUG_LOG("twsTopology_ProcHandoverStart() Started");
        handover_status = HandoverProfile_Handover();
    }
    
    td->handover_result = twsTopology_ProcGetStatus(handover_status);

    switch (td->handover_result)
    {
        case handover_success:
        {
            DEBUG_LOG("twsTopology_ProcHandoverStart() Success ");
            Procedures_DelayedCompleteCfmCallback(td->complete_fn, tws_topology_procedure_handover,procedure_result_success);
            twsTopology_ProcHandoverReset();
        }
        break;

        case handover_failed:
        {
            DEBUG_LOG("twsTopology_ProcHandoverStart() Failed ");
            twsTopology_ProcHandoverFailedOrCancelled();
        }
        break;

        case handover_timedout:
        {
            /* Retry handover */
            DEBUG_LOG("twsTopology_ProcHandoverStart() Timedout, retry handover ");
            /* Restart the procedure by disconnecting LE connections */
            td->handover_procedure_count = DISCONNECT_LE_CONNECTIONS_INDEX;
            MessageSendLater(TwsTopProcHandoverGetTask(), TWS_TOP_PROC_HANDOVER_INTERNAL_RETRY, NULL, TwsTopologyConfig_HandoverRetryTimeoutMs());
        }
        break;
    }
}

static void twsTopology_ProcHandoverHandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);
    twsTopProcHandoverTaskData* td = TwsTopProcHandoverGetTaskData();

    DEBUG_LOG("twsTopology_ProcHandoverHandleMessage() Received id: MESSAGE:0x%x", id);

    switch (id)
    {
        case TWS_TOP_PROC_HANDOVER_INTERNAL_RETRY:
        {
            twsTopology_ProcHandoverProcedureStart();
        }
        break;
        case TWS_TOP_PROC_HANDOVER_INTERNAL_CANCEL_RETRY:
        {
            DEBUG_LOG("twsTopology_ProcHandoverHandleMessage() Cancel handover retry and complete the handover proc");
            td->handover_result = handover_failed;
            twsTopology_ProcHandoverFailedOrCancelled();
        }
        break;
        default:
        break;
    }
}

static void twsTopology_ProcHandoverFailCfmSend(void)
{
    twsTopProcHandoverTaskData* td = TwsTopProcHandoverGetTaskData();
    DEBUG_LOG("twsTopology_ProcHandoverFailCfmSend");
    Procedures_DelayedCompleteCfmCallback(td->complete_fn,
                                          tws_topology_procedure_handover,
                                          procedure_result_failed);
    twsTopology_ProcHandoverReset();
}

static void twsTopology_ProcHandoverCancelCfmSend(void)
{
    twsTopProcHandoverTaskData* td = TwsTopProcHandoverGetTaskData();
    DEBUG_LOG("twsTopology_ProcHandoverCancelCfmSend");
    Procedures_DelayedCancelCfmCallback(td->proc_cancel_cfm_fn,
                                        tws_topology_procedure_handover,
                                        procedure_result_success);
    twsTopology_ProcHandoverReset();
}

static void twsTopology_ProcHandoverCancelOrFailCfmSend(void)
{
    twsTopProcHandoverTaskData* td = TwsTopProcHandoverGetTaskData();
    DEBUG_LOG("twsTopology_ProcHandoverCancelOrFailCfmSend");

    /* the same code path is used to handle multiple failure and cancellation
       conditions:-
        1 - failed start due to handover profile failure
        2 - HDMA decision to cancel handover which is internally managed by this procedure
        3 - Goal engine decision to cancel procedure

     1 and 2 are internally managed by this procedure and will have the result set as
     handover_failed, which can trigger follow on HANDOVER_FAILED processing.
     3 uses the same cancellation processing but requires use of the cancel confirm callback
     which the script engine is expecting after calling this procedure's cancel function,
     catch case 3 by the fact the proc_cancel_cfm_fn callback is set. */
    if (td->handover_result == handover_failed && !td->proc_cancel_cfm_fn)

    {
        twsTopology_ProcHandoverFailCfmSend();
    }
    else
    {
        twsTopology_ProcHandoverCancelCfmSend();
    }
}

static bool twsTopology_ProcHandoverCancelProceduresCompleted(void)
{
    twsTopProcHandoverTaskData* td = TwsTopProcHandoverGetTaskData();

    if (td->handover_procedure_count == ARRAY_DIM(handover_cancel_proc_fns) &&
        td->handover_failed_or_cancelled)
    {
        DEBUG_LOG("twsTopology_ProcHandoverCancelProceduresCompleted");
        return TRUE;
    }
    return FALSE;
}

static bool twsTopology_ProcHandoverStartProceduresCompleted(void)
{
    twsTopProcHandoverTaskData* td = TwsTopProcHandoverGetTaskData();

    if (td->handover_procedure_count == ARRAY_DIM(handover_start_proc_fns) &&
        !td->handover_failed_or_cancelled)
    {
        DEBUG_LOG("twsTopology_ProcHandoverStartProceduresCompleted");
        return TRUE;
    }
    return FALSE;
}

static void twsTopology_ProcHandoverNextProcedure(void)
{
    twsTopProcHandoverTaskData* td = TwsTopProcHandoverGetTaskData();
    DEBUG_LOG("twsTopology_ProcHandoverNextProcedure");

    if (!td->handover_failed_or_cancelled)
    {
        twsTopology_ProcHandoverProcedureStart();
    }
    else
    {
        twsTopology_ProcHandoverProcedureCancel();
    }
}

static void twsTopology_ProcHandoverProcedureCompleteCfm(procedure_id proc, procedure_result_t result)
{
    twsTopProcHandoverTaskData* td = TwsTopProcHandoverGetTaskData();
    DEBUG_LOG("twsTopology_ProcCompleteCfm: proc = %d result = %d",proc,result);

    if (td->complete_fn)
    {
        switch (result)
        {
            case procedure_result_success:
            {
                td->handover_procedure_count++;
                if (twsTopology_ProcHandoverStartProceduresCompleted())
                {
                    /* Prepare procedures completed . Start handover */
                    twsTopology_ProcHandoverStart();
                }
                else if (twsTopology_ProcHandoverCancelProceduresCompleted())
                {
                    twsTopology_ProcHandoverCancelOrFailCfmSend();
                }
                else
                {
                    twsTopology_ProcHandoverNextProcedure();
                }
            }
            break;
            case  procedure_result_timeout:
            case  procedure_result_failed:
            {
                DEBUG_LOG("twsTopology_ProcHandoverProcedureStart() Failed ");
                Procedures_DelayedCompleteCfmCallback(td->complete_fn,
                                                      tws_topology_procedure_handover,
                                                      procedure_result_failed);
                twsTopology_ProcHandoverReset();
            }
            break;
            default:
            break;
        }
    }
}

static void twsTopology_ProcHandoverProcedureStartCfm(procedure_id proc, procedure_result_t result)
{
    DEBUG_LOG("twsTopology_ProcstartCfm: proc = %d result = %d",proc,result);
    if (result != procedure_result_success)
    {
        Panic();
    }
}

void twsTopology_ProcHandoverProcedureCancel(void)
{
    twsTopProcHandoverTaskData* td = TwsTopProcHandoverGetTaskData();
    handover_cancel_proc_fns[td->handover_procedure_count]->proc_start_fn(TwsTopProcHandoverGetTask(),
                                                                          twsTopology_ProcHandoverProcedureStartCfm,
                                                                          twsTopology_ProcHandoverProcedureCompleteCfm,
                                                                          handover_cancel_proc_goal_data[td->handover_procedure_count]);
}

void twsTopology_ProcHandoverProcedureStart(void)
{
    twsTopProcHandoverTaskData* td = TwsTopProcHandoverGetTaskData();
    handover_start_proc_fns[td->handover_procedure_count]->proc_start_fn(TwsTopProcHandoverGetTask(),
                                                                         twsTopology_ProcHandoverProcedureStartCfm,
                                                                         twsTopology_ProcHandoverProcedureCompleteCfm,
                                                                         handover_start_proc_goal_data[td->handover_procedure_count]);

}

void TwsTopology_ProcedureHandoverStart(Task result_task,
                                        procedure_start_cfm_func_t proc_start_cfm_fn,
                                        procedure_complete_func_t proc_complete_fn,
                                        Message goal_data)
{
    DEBUG_LOG("TwsTopology_ProcedureHandOverStart");
    UNUSED(result_task);
    UNUSED(goal_data);

    twsTopProcHandoverTaskData* td = TwsTopProcHandoverGetTaskData();
    td->complete_fn = proc_complete_fn;
    td->handover_retry_count = 0;
    td->handover_procedure_count = 0;
    td->handover_failed_or_cancelled = FALSE;

    /* procedure started synchronously so indicate success */
    proc_start_cfm_fn(tws_topology_procedure_handover, procedure_result_success);
    /* This event is expected to deliver, when handover info
     * is reset upon handover cancel recommendation from hdma */
    MessageSendConditionally(TwsTopProcHandoverGetTask(), TWS_TOP_PROC_HANDOVER_INTERNAL_CANCEL_RETRY, NULL, (void*)&TwsTopologyGetTaskData()->handover_info.reason);
    twsTopology_ProcHandoverProcedureStart();
}

void TwsTopology_ProcedureHandoverCancel(procedure_cancel_cfm_func_t proc_cancel_cfm_fn)
{
    twsTopProcHandoverTaskData* td = TwsTopProcHandoverGetTaskData();
    DEBUG_LOG("TwsTopology_ProcedureHandOverCancel complete_fn %p", td->complete_fn);

    td->proc_cancel_cfm_fn = proc_cancel_cfm_fn;

    if (td->complete_fn)
    {
        /* Only initiate cancellation if not already in progress */
        if (!td->handover_failed_or_cancelled)
        {
            twsTopology_ProcHandoverFailedOrCancelled();
        }
    }
    else
    {
        /* Procedure is not active so nothing to cnacel. Send cancel cfm */
        twsTopology_ProcHandoverCancelCfmSend();
    }
}

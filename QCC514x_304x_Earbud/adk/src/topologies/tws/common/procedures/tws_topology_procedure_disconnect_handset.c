/*!
\copyright  Copyright (c) 2019 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief
*/

#include "tws_topology_procedure_disconnect_handset.h"
#include "tws_topology_procedures.h"
#include "tws_topology_client_msgs.h"

#include <bt_device.h>
#include <device_list.h>
#include <device_properties.h>
#include <handset_service.h>
#include <connection_manager.h>

#include <logging.h>

#include <message.h>

static void twsTopology_ProcDisconnectHandsetHandleMessage(Task task, MessageId id, Message message);
void TwsTopology_ProcedureDisconnectHandsetStart(Task result_task,
                                        procedure_start_cfm_func_t proc_start_cfm_fn,
                                        procedure_complete_func_t proc_complete_fn,
                                        Message goal_data);
void TwsTopology_ProcedureDisconnectHandsetCancel(procedure_cancel_cfm_func_t proc_cancel_cfm_fn);

const procedure_fns_t proc_disconnect_handset_fns = {
    TwsTopology_ProcedureDisconnectHandsetStart,
    TwsTopology_ProcedureDisconnectHandsetCancel,
};

void TwsTopology_ProcedureDisconnectLruHandsetStart(Task result_task,
                                        procedure_start_cfm_func_t proc_start_cfm_fn,
                                        procedure_complete_func_t proc_complete_fn,
                                        Message goal_data);

void TwsTopology_ProcedureDisconnectLruHandsetCancel(procedure_cancel_cfm_func_t proc_cancel_cfm_fn);

const procedure_fns_t proc_disconnect_lru_handset_fns = {
    TwsTopology_ProcedureDisconnectLruHandsetStart,
    TwsTopology_ProcedureDisconnectLruHandsetCancel
};

typedef struct
{
    TaskData task;
    procedure_complete_func_t complete_fn;
    bool active;
} twsTopProcDisconnectHandsetTaskData;

twsTopProcDisconnectHandsetTaskData twstop_proc_disconnect_handset = {twsTopology_ProcDisconnectHandsetHandleMessage};

#define TwsTopProcDisconnectHandsetGetTaskData()     (&twstop_proc_disconnect_handset)
#define TwsTopProcDisconnectHandsetGetTask()         (&twstop_proc_disconnect_handset.task)

static void twsTopology_ProcDisconnectHandsetResetProc(void)
{
    twsTopProcDisconnectHandsetTaskData* td = TwsTopProcDisconnectHandsetGetTaskData();
    td->active = FALSE;
}

void TwsTopology_ProcedureDisconnectLruHandsetStart(Task result_task,
                                        procedure_start_cfm_func_t proc_start_cfm_fn,
                                        procedure_complete_func_t proc_complete_fn,
                                        Message goal_data)
{
    twsTopProcDisconnectHandsetTaskData* td = TwsTopProcDisconnectHandsetGetTaskData();
    UNUSED(result_task);
    UNUSED(goal_data);

    DEBUG_LOG_VERBOSE("HeadsetTopology_ProcedureDisconnectLruHandsetStart");

    HandsetService_DisconnectLruHandsetRequest(TwsTopProcDisconnectHandsetGetTask());

    /* start the procedure */
    td->complete_fn = proc_complete_fn;
    td->active = TRUE;

    proc_start_cfm_fn(tws_topology_procedure_disconnect_lru_handset, procedure_result_success);
}

void TwsTopology_ProcedureDisconnectLruHandsetCancel(procedure_cancel_cfm_func_t proc_cancel_cfm_fn)
{
    DEBUG_LOG("TwsTopology_ProcedureDisconnectLruHandsetCancel");

    twsTopology_ProcDisconnectHandsetResetProc();
    Procedures_DelayedCancelCfmCallback(proc_cancel_cfm_fn,
                                         tws_topology_procedure_disconnect_lru_handset,
                                         procedure_result_success);
}

void TwsTopology_ProcedureDisconnectHandsetStart(Task result_task,
                                        procedure_start_cfm_func_t proc_start_cfm_fn,
                                        procedure_complete_func_t proc_complete_fn,
                                        Message goal_data)
{
    twsTopProcDisconnectHandsetTaskData* td = TwsTopProcDisconnectHandsetGetTaskData();

    UNUSED(result_task);
    UNUSED(goal_data);

    DEBUG_LOG("TwsTopology_ProcedureDisconnectHandsetStart");

    /* Request to Handset Services to disconnect Handset even it is disconnected.
    Handset Services sends disconnection confirmation if nothing to do. This
    message is used by topology to send TWS_TOPOLOGY_HANDSET_DISCONNECTED_IND to
    apps sm. When the earbud been requested to enter into User Pairing Mode, the apps state
    machine makes decision of entering into pairing mode after reception of
    TWS_TOPOLOGY_HANDSET_DISCONNECTED_IND. */
    HandsetService_DisconnectAll(TwsTopProcDisconnectHandsetGetTask());

    /* start the procedure */
    td->complete_fn = proc_complete_fn;
    td->active = TRUE;

    proc_start_cfm_fn(tws_topology_procedure_disconnect_handset, procedure_result_success);
}

void TwsTopology_ProcedureDisconnectHandsetCancel(procedure_cancel_cfm_func_t proc_cancel_cfm_fn)
{
    DEBUG_LOG("TwsTopology_ProcedureDisconnectHandsetCancel");

    twsTopology_ProcDisconnectHandsetResetProc();
    Procedures_DelayedCancelCfmCallback(proc_cancel_cfm_fn,
                                         tws_topology_procedure_disconnect_handset,
                                         procedure_result_success);
}

static void twsTopology_ProcDisconnectHandsetHandleHandsetConnectCfm(const HANDSET_SERVICE_CONNECT_CFM_T *cfm)
{
    DEBUG_LOG("twsTopology_ProcDisconnectHandsetHandleHandsetConnectCfm status enum:handset_service_status_t:%d", cfm->status);

    UNUSED(cfm);
}

static void twsTopology_ProcDisconnectHandsetHandleHandsetMpDisconnectAllCfm(const HANDSET_SERVICE_MP_DISCONNECT_ALL_CFM_T *cfm)
{
    twsTopProcDisconnectHandsetTaskData* td = TwsTopProcDisconnectHandsetGetTaskData();
    DEBUG_LOG_VERBOSE("twsTopology_ProcDisconnectHandsetHandleHandsetMpDisconnectAllCfm status enum:handset_service_status_t:%d", cfm->status);
    twsTopology_ProcDisconnectHandsetResetProc();
    td->complete_fn(tws_topology_procedure_disconnect_handset, procedure_result_success);

    TwsTopology_SendHandsetDisconnectedIndication();
}

static void twsTopology_ProcDisconnectHandsetHandleHandsetDisconnectCfm(const HANDSET_SERVICE_DISCONNECT_CFM_T *cfm)
{
    twsTopProcDisconnectHandsetTaskData* td = TwsTopProcDisconnectHandsetGetTaskData();
    DEBUG_LOG("twsTopology_ProcDisconnectHandsetHandleHandsetDisconnectCfm status enum:handset_service_status_t:%d", cfm->status);

    twsTopology_ProcDisconnectHandsetResetProc();
    td->complete_fn(tws_topology_procedure_disconnect_lru_handset, procedure_result_success);
    TwsTopology_SendHandsetDisconnectedIndication();
}

static void twsTopology_ProcDisconnectHandsetHandleMessage(Task task, MessageId id, Message message)
{
    twsTopProcDisconnectHandsetTaskData* td = TwsTopProcDisconnectHandsetGetTaskData();

    UNUSED(task);

    if (!td->active)
    {
        return;
    }

    switch (id)
    {
        case HANDSET_SERVICE_CONNECT_CFM:
            twsTopology_ProcDisconnectHandsetHandleHandsetConnectCfm((const HANDSET_SERVICE_CONNECT_CFM_T *)message);
            break;

        case HANDSET_SERVICE_MP_DISCONNECT_ALL_CFM:
            twsTopology_ProcDisconnectHandsetHandleHandsetMpDisconnectAllCfm((const HANDSET_SERVICE_MP_DISCONNECT_ALL_CFM_T *)message);
            break;

        case HANDSET_SERVICE_DISCONNECT_CFM:
            twsTopology_ProcDisconnectHandsetHandleHandsetDisconnectCfm((const HANDSET_SERVICE_DISCONNECT_CFM_T *)message);
            break;

        default:
            break;
    }
}

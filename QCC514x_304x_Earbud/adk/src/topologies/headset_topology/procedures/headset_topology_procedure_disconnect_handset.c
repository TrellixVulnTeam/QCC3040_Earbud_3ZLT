/*!
\copyright  Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Procedure to handle handset connection.
*/

#include "headset_topology_procedure_disconnect_handset.h"
#include "headset_topology_client_msgs.h"
#include "headset_topology_procedures.h"

#include <bt_device.h>
#include <device_list.h>
#include <device_properties.h>
#include <handset_service.h>
#include <connection_manager.h>

#include <logging.h>

#include <message.h>

static void headsetTopology_ProcDisconnectHandsetHandleMessage(Task task, MessageId id, Message message);
static void HeadsetTopology_ProcedureDisconnectHandsetStart(Task result_task,
                                                     procedure_start_cfm_func_t proc_start_cfm_fn,
                                                     procedure_complete_func_t proc_complete_fn,
                                                     Message goal_data);
static void HeadsetTopology_ProcedureDisconnectHandsetCancel(procedure_cancel_cfm_func_t proc_cancel_cfm_fn);

const procedure_fns_t hs_proc_disconnect_handset_fns = {
    HeadsetTopology_ProcedureDisconnectHandsetStart,
    HeadsetTopology_ProcedureDisconnectHandsetCancel,
};

static void HeadsetTopology_ProcedureDisconnectLruHandsetStart(Task result_task,
                                                     procedure_start_cfm_func_t proc_start_cfm_fn,
                                                     procedure_complete_func_t proc_complete_fn,
                                                     Message goal_data);
static void HeadsetTopology_ProcedureDisconnectLruHandsetCancel(procedure_cancel_cfm_func_t proc_cancel_cfm_fn);

const procedure_fns_t hs_proc_disconnect_lru_handset_fns = {
    HeadsetTopology_ProcedureDisconnectLruHandsetStart,
    HeadsetTopology_ProcedureDisconnectLruHandsetCancel,
};

typedef struct
{
    TaskData task;
    procedure_complete_func_t complete_fn;
} headsetTopProcDisconnectHandsetTaskData;

headsetTopProcDisconnectHandsetTaskData handsettop_proc_disconnect_handset = {headsetTopology_ProcDisconnectHandsetHandleMessage};

#define HeadsetTopProcDisconnectHandsetGetTaskData()     (&handsettop_proc_disconnect_handset)
#define HeadsetTopProcDisconnectHandsetGetTask()         (&handsettop_proc_disconnect_handset.task)

static void headsetTopology_ProcDisconnectHandsetResetProc(void)
{
    headsetTopProcDisconnectHandsetTaskData *td = HeadsetTopProcDisconnectHandsetGetTaskData();
    td->complete_fn = NULL;
}

static void HeadsetTopology_ProcedureDisconnectLruHandsetStart(Task result_task,
                                                     procedure_start_cfm_func_t proc_start_cfm_fn,
                                                     procedure_complete_func_t proc_complete_fn,
                                                     Message goal_data)
{
    headsetTopProcDisconnectHandsetTaskData *td = HeadsetTopProcDisconnectHandsetGetTaskData();
    UNUSED(result_task);
    UNUSED(goal_data);

    DEBUG_LOG_VERBOSE("HeadsetTopology_ProcedureDisconnectLruHandsetStart");

    HandsetService_DisconnectLruHandsetRequest(HeadsetTopProcDisconnectHandsetGetTask());

    /* start the procedure */
    td->complete_fn = proc_complete_fn;

    proc_start_cfm_fn(hs_topology_procedure_disconnect_lru_handset, procedure_result_success);
}

static void HeadsetTopology_ProcedureDisconnectLruHandsetCancel(procedure_cancel_cfm_func_t proc_cancel_cfm_fn)
{
    DEBUG_LOG_VERBOSE("HeadsetTopology_ProcedureDisconnectLruHandsetCancel");

    headsetTopology_ProcDisconnectHandsetResetProc();
    Procedures_DelayedCancelCfmCallback(proc_cancel_cfm_fn,
                                        hs_topology_procedure_disconnect_lru_handset,
                                        procedure_result_success);
}

static void HeadsetTopology_ProcedureDisconnectHandsetStart(Task result_task,
                                                            procedure_start_cfm_func_t proc_start_cfm_fn,
                                                            procedure_complete_func_t proc_complete_fn,
                                                            Message goal_data)
{
    headsetTopProcDisconnectHandsetTaskData *td = HeadsetTopProcDisconnectHandsetGetTaskData();
    UNUSED(result_task);
    UNUSED(goal_data);

    DEBUG_LOG_VERBOSE("HeadsetTopology_ProcedureDisconnectHandsetStart");

    HandsetService_DisconnectAll(HeadsetTopProcDisconnectHandsetGetTask());


    /* Request to Handset Services to disconnect Handsets, even it is disconnected.
    Handset Services sends a confirmation message if nothing to do. This
    message is used by topology to send HEADSET_TOPOLOGY_HANDSET_DISCONNECTED_IND to
    apps sm. When the headset has been requested to enter into User Pairing Mode, the apps state
    machine makes the decision of entering into pairing mode after reception of
    HEADSET_TOPOLOGY_HANDSET_DISCONNECTED_IND. */


    /* start the procedure */
    td->complete_fn = proc_complete_fn;

    proc_start_cfm_fn(hs_topology_procedure_disconnect_handset, procedure_result_success);
}

static void HeadsetTopology_ProcedureDisconnectHandsetCancel(procedure_cancel_cfm_func_t proc_cancel_cfm_fn)
{
    DEBUG_LOG_VERBOSE("HeadsetTopology_ProcedureDisconnectHandsetCancel");

    headsetTopology_ProcDisconnectHandsetResetProc();
    Procedures_DelayedCancelCfmCallback(proc_cancel_cfm_fn,
                                        hs_topology_procedure_disconnect_handset,
                                        procedure_result_success);
}

static void headsetTopology_ProcDisconnectHandsetHandleHandsetMpDisconnectAllCfm(const HANDSET_SERVICE_MP_DISCONNECT_ALL_CFM_T *cfm)
{
    headsetTopProcDisconnectHandsetTaskData* td = HeadsetTopProcDisconnectHandsetGetTaskData();

    DEBUG_LOG_VERBOSE("headsetTopology_ProcDisconnectHandsetHandleHandsetMpDisconnectAllCfm status enum:handset_service_status_t:%d", cfm->status);

    td->complete_fn(hs_topology_procedure_disconnect_handset, procedure_result_success);
    headsetTopology_ProcDisconnectHandsetResetProc();
    HeadsetTopology_SendHandsetDisconnectedIndication(cfm->status);
}

static void headsetTopology_ProcDisconnectHandsetHandleHandsetDisconnectCfm(const HANDSET_SERVICE_DISCONNECT_CFM_T *cfm)
{
    headsetTopProcDisconnectHandsetTaskData* td = HeadsetTopProcDisconnectHandsetGetTaskData();
    DEBUG_LOG("headsetTopology_ProcDisconnectHandsetHandleHandsetDisconnectCfm status enum:handset_service_status_t:%d", cfm->status);

    td->complete_fn(hs_topology_procedure_disconnect_lru_handset, procedure_result_success);
    headsetTopology_ProcDisconnectHandsetResetProc();
    HeadsetTopology_SendHandsetDisconnectedIndication(cfm->status);
}

static void headsetTopology_ProcDisconnectHandsetHandleMessage(Task task, MessageId id, Message message)
{
    headsetTopProcDisconnectHandsetTaskData* td = HeadsetTopProcDisconnectHandsetGetTaskData();

    UNUSED(task);

    if(td->complete_fn == NULL)
    {
        return;
    }
    switch (id)
    {
        case HANDSET_SERVICE_MP_DISCONNECT_ALL_CFM:
            headsetTopology_ProcDisconnectHandsetHandleHandsetMpDisconnectAllCfm((const HANDSET_SERVICE_MP_DISCONNECT_ALL_CFM_T*)message);
            break;

        case HANDSET_SERVICE_DISCONNECT_CFM:
            headsetTopology_ProcDisconnectHandsetHandleHandsetDisconnectCfm((const HANDSET_SERVICE_DISCONNECT_CFM_T *)message);
            break;

        default:
            DEBUG_LOG_VERBOSE("headsetTopology_ProcDisconnectHandsetHandleMessage unhandled id MESSAGE:0x%x", id);
            break;
    }
}


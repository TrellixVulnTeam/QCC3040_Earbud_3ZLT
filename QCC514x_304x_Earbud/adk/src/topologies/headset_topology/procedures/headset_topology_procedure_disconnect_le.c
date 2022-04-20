/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Procedure to handle BLE disconnection.
*/

#include "headset_topology_procedure_disconnect_le.h"
#include "headset_topology_client_msgs.h"
#include "headset_topology_procedures.h"

#include <connection_manager.h>
#include <logging.h>
#include <message.h>

static void headsetTopology_ProcDisconnectLeHandleMessage(Task task, MessageId id, Message message);
static void HeadsetTopology_ProcedureDisconnectLeStart(Task result_task,
                                                     procedure_start_cfm_func_t proc_start_cfm_fn,
                                                     procedure_complete_func_t proc_complete_fn,
                                                     Message goal_data);
static void HeadsetTopology_ProcedureDisconnectLeCancel(procedure_cancel_cfm_func_t proc_cancel_cfm_fn);

const procedure_fns_t hs_proc_disconnect_le_fns = {
    HeadsetTopology_ProcedureDisconnectLeStart,
    HeadsetTopology_ProcedureDisconnectLeCancel,
};

typedef struct
{
    TaskData task;
    procedure_complete_func_t complete_fn;
} headsetTopProcDisconnectLeTaskData;

headsetTopProcDisconnectLeTaskData handsettop_proc_disconnect_le = {headsetTopology_ProcDisconnectLeHandleMessage};

#define HeadsetTopProcDisconnectLeGetTaskData()     (&handsettop_proc_disconnect_le)
#define HeadsetTopProcDisconnectLeGetTask()         (&handsettop_proc_disconnect_le.task)

static void headsetTopology_ProcDisconnectLeResetProc(void)
{
    headsetTopProcDisconnectLeTaskData *td = HeadsetTopProcDisconnectLeGetTaskData();
    td->complete_fn = NULL;
}

static void HeadsetTopology_ProcedureDisconnectLeStart(Task result_task,
                                                            procedure_start_cfm_func_t proc_start_cfm_fn,
                                                            procedure_complete_func_t proc_complete_fn,
                                                            Message goal_data)
{
    headsetTopProcDisconnectLeTaskData *td = HeadsetTopProcDisconnectLeGetTaskData();
    UNUSED(result_task);
    UNUSED(goal_data);

    DEBUG_LOG_VERBOSE("HeadsetTopology_ProcedureDisconnectLeStart");

    ConManagerDisconnectAllLeConnectionsRequest(HeadsetTopProcDisconnectLeGetTask());

    td->complete_fn = proc_complete_fn;

    proc_start_cfm_fn(hs_topology_procedure_disconnect_le, procedure_result_success);
}

static void HeadsetTopology_ProcedureDisconnectLeCancel(procedure_cancel_cfm_func_t proc_cancel_cfm_fn)
{
    DEBUG_LOG_VERBOSE("HeadsetTopology_ProcedureDisconnectLeCancel");

    headsetTopology_ProcDisconnectLeResetProc();
    Procedures_DelayedCancelCfmCallback(proc_cancel_cfm_fn,
                                        hs_topology_procedure_disconnect_le,
                                        procedure_result_success);
}

static void headsetTopology_ProcDisconnectLeHandleLeDisconnectCfm(void)
{
    headsetTopProcDisconnectLeTaskData* td = HeadsetTopProcDisconnectLeGetTaskData();

    DEBUG_LOG_VERBOSE("headsetTopology_ProcDisconnectLeHandleLeDisconnectCfm, all LE ACL (if present) is disconnected");

    td->complete_fn(hs_topology_procedure_disconnect_le, procedure_result_success);
    headsetTopology_ProcDisconnectLeResetProc();
}

static void headsetTopology_ProcDisconnectLeHandleMessage(Task task, MessageId id, Message message)
{
    headsetTopProcDisconnectLeTaskData* td = HeadsetTopProcDisconnectLeGetTaskData();

    UNUSED(task);
    UNUSED(message);

    if(td->complete_fn == NULL)
    {
        DEBUG_LOG_VERBOSE("headsetTopology_ProcDisconnectLeHandleMessage: Ignore because the procedure already completed/or cancelled");
        return;
    }

    switch (id)
    {
        case CON_MANAGER_DISCONNECT_ALL_LE_CONNECTIONS_CFM:
            headsetTopology_ProcDisconnectLeHandleLeDisconnectCfm();
            break;

        default:
            DEBUG_LOG_VERBOSE("headsetTopology_ProcDisconnectLeHandleMessage unhandled id MESSAGE:0x%x", id);
            break;
    }
}


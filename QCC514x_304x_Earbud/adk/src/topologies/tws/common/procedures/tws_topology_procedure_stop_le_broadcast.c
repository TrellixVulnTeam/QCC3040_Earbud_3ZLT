/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\brief      Procedure to stop LE broadcast.
*/

#include "tws_topology_procedure_stop_le_broadcast.h"
#include "tws_topology_procedures.h"
#include "tws_topology_config.h"

#include <logging.h>
#include <message.h>

typedef struct
{
    TaskData task_data;
    procedure_complete_func_t complete_fn;
    procedure_cancel_cfm_func_t cancel_fn;
} twsTopProcStopLeBroadcastTaskData;

static void twsTopology_ProcStopLeBroadcastHandleMessage(Task task, MessageId id, Message message);
static void twsTopology_ProcStopLeBroadcastStart(Task result_task,
                                        procedure_start_cfm_func_t proc_start_cfm_fn,
                                        procedure_complete_func_t proc_complete_fn,
                                        Message goal_data);
static void twsTopology_ProcStopLeBroadcastHandleStopCfm(twsTopProcStopLeBroadcastTaskData *td);
static void twsTopology_ProcStopLeBroadcastCancel(procedure_cancel_cfm_func_t proc_cancel_cfm_fn);

const procedure_fns_t proc_stop_le_broadcast_fns = {
    twsTopology_ProcStopLeBroadcastStart,
    twsTopology_ProcStopLeBroadcastCancel,
};

twsTopProcStopLeBroadcastTaskData twstop_proc_stop_le_broadcast = {twsTopology_ProcStopLeBroadcastHandleMessage};

#define TwsTopProcStopLeBroadcastGetTaskData()     (&twstop_proc_stop_le_broadcast)
#define TwsTopProcStopLeBroadcastGetTask()         (&twstop_proc_stop_le_broadcast.task_data)

static void twsTopology_ProcStopLeBroadcastStart(Task result_task,
                                        procedure_start_cfm_func_t proc_start_cfm_fn,
                                        procedure_complete_func_t proc_complete_fn,
                                        Message goal_data)
{
    twsTopProcStopLeBroadcastTaskData* td = TwsTopProcStopLeBroadcastGetTaskData();

    UNUSED(result_task);
    UNUSED(goal_data);

    /* start the procedure */
    td->complete_fn = proc_complete_fn;
    td->cancel_fn = NULL;

    /* procedure starts synchronously so return TRUE */
    proc_start_cfm_fn(tws_topology_proc_stop_le_broadcast, procedure_result_success);

    DEBUG_LOG("twsTopology_ProcStopLeBroadcastStart");
    twsTopology_ProcStopLeBroadcastHandleStopCfm(td);
}

static void twsTopology_ProcStopLeBroadcastCancel(procedure_cancel_cfm_func_t proc_cancel_cfm_fn)
{
    twsTopProcStopLeBroadcastTaskData* td = TwsTopProcStopLeBroadcastGetTaskData();

    DEBUG_LOG("twsTopology_ProcStopLeBroadcastCancel");

    /* Need to wait for the response from broadcast manager to complete */
    td->cancel_fn = proc_cancel_cfm_fn;
}

static void twsTopology_ProcStopLeBroadcastHandleStopCfm(twsTopProcStopLeBroadcastTaskData *td)
{
    if (td->cancel_fn)
    {
        Procedures_DelayedCancelCfmCallback(td->cancel_fn, tws_topology_proc_stop_le_broadcast, procedure_result_success);
        DEBUG_LOG("twsTopology_ProcStopLeBroadcastHandleStopCfm cancel complete");
    }
    else if (td->complete_fn)
    {
        Procedures_DelayedCompleteCfmCallback(td->complete_fn, tws_topology_proc_stop_le_broadcast, procedure_result_success);
        DEBUG_LOG("twsTopology_ProcStopLeBroadcastHandleStopCfm complete");
    }
    td->cancel_fn = NULL;
    td->complete_fn = NULL;
}

static void twsTopology_ProcStopLeBroadcastHandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(id);
    UNUSED(message);
}

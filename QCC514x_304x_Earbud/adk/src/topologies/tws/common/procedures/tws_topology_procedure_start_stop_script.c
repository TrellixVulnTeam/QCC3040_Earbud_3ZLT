/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      simple topology procedure to indicate that a script designed to stop toology has started.
 */

#include "tws_topology_private.h"
#include "tws_topology_config.h"
#include "tws_topology.h"
#include "tws_topology_procedure_start_stop_script.h"
#include "tws_topology_procedures.h"

#include <logging.h>
#include <panic.h>


void TwsTopology_ProcedureStartStopScriptStart(Task result_task,
                                               procedure_start_cfm_func_t proc_start_cfm_fn,
                                               procedure_complete_func_t proc_complete_fn,
                                               Message goal_data);

void TwsTopology_ProcedureStartStopScriptCancel(procedure_cancel_cfm_func_t proc_cancel_cfm_fn);

const procedure_fns_t proc_start_stop_script_fns = {
    TwsTopology_ProcedureStartStopScriptStart,
    TwsTopology_ProcedureStartStopScriptCancel,
};

void TwsTopology_ProcedureStartStopScriptStart(Task result_task,
                                               procedure_start_cfm_func_t proc_start_cfm_fn,
                                               procedure_complete_func_t proc_complete_fn,
                                               Message goal_data)
{

    DEBUG_LOG("TwsTopology_ProcedureStartStopScriptStart");

    UNUSED(result_task);
    UNUSED(goal_data);

    /* procedure started synchronously so indicate success */
    proc_start_cfm_fn(tws_topology_procedure_start_stop_script, procedure_result_success);

    /* Now tell the topology that we have started */
    twsTopology_StopHasStarted();

    Procedures_DelayedCompleteCfmCallback(proc_complete_fn,
                                          tws_topology_procedure_start_stop_script,
                                          procedure_result_success);
}


void TwsTopology_ProcedureStartStopScriptCancel(procedure_cancel_cfm_func_t proc_cancel_cfm_fn)
{
    UNUSED(proc_cancel_cfm_fn);

    DEBUG_LOG("TwsTopology_ProcedureStartStopScriptCancel. Should never be called.");

    Panic();
}


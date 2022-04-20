/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      simple topology procedure to indicate that a script designed to stop toology has started.
 */

#include "headset_topology_private.h"
#include "headset_topology_config.h"
#include "headset_topology.h"
#include "headset_topology_procedure_start_stop_script.h"
#include "headset_topology_procedures.h"

#include <logging.h>
#include <panic.h>


void HeadsetTopology_ProcedureStartStopScriptStart(Task result_task,
                                                   procedure_start_cfm_func_t proc_start_cfm_fn,
                                                   procedure_complete_func_t proc_complete_fn,
                                                   Message goal_data);

void HeadsetTopology_ProcedureStartStopScriptCancel(procedure_cancel_cfm_func_t proc_cancel_cfm_fn);

const procedure_fns_t hs_proc_start_stop_script_fns = {
    HeadsetTopology_ProcedureStartStopScriptStart,
    HeadsetTopology_ProcedureStartStopScriptCancel,
};

void HeadsetTopology_ProcedureStartStopScriptStart(Task result_task,
                                                                    procedure_start_cfm_func_t proc_start_cfm_fn,
                                                                    procedure_complete_func_t proc_complete_fn,
                                                                    Message goal_data)
{

    DEBUG_LOG_VERBOSE("HeadsetTopology_ProcedureStartStopScriptStart");

    UNUSED(result_task);
    UNUSED(goal_data);

    /* procedure started synchronously so indicate success */
    proc_start_cfm_fn(hs_topology_procedure_start_stop_script, procedure_result_success);

    /* Now tell the topology that we have started */
    headsetTopology_StopHasStarted();

    Procedures_DelayedCompleteCfmCallback(proc_complete_fn,
                                          hs_topology_procedure_start_stop_script,
                                          procedure_result_success);
}


void HeadsetTopology_ProcedureStartStopScriptCancel(procedure_cancel_cfm_func_t proc_cancel_cfm_fn)
{
    UNUSED(proc_cancel_cfm_fn);

    DEBUG_LOG_ERROR("HeadsetTopology_ProcedureStartStopScriptCancel. Should never be called.");

    Panic();
}


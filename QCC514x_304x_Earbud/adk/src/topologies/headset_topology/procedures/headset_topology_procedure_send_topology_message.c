/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      simple topology procedure to send a message to the topology task.
*/

#include "headset_topology_private.h"
#include "headset_topology_config.h"
#include "headset_topology.h"
#include "headset_topology_procedure_send_topology_message.h"
#include "headset_topology_procedures.h"

#include <logging.h>
#include <message.h>
#include <panic.h>


void HeadsetTopology_ProcedureSendTopologyMessageStart(Task result_task,
                                                   procedure_start_cfm_func_t proc_start_cfm_fn,
                                                   procedure_complete_func_t proc_complete_fn,
                                                   Message goal_data);

void HeadsetTopology_ProcedureSendTopologyMessageCancel(procedure_cancel_cfm_func_t proc_cancel_cfm_fn);

const procedure_fns_t hs_proc_send_topology_message_fns = {
    HeadsetTopology_ProcedureSendTopologyMessageStart,
    HeadsetTopology_ProcedureSendTopologyMessageCancel,
};

void HeadsetTopology_ProcedureSendTopologyMessageStart(Task result_task,
                                                      procedure_start_cfm_func_t proc_start_cfm_fn,
                                                      procedure_complete_func_t proc_complete_fn,
                                                      Message goal_data)
{
    HSTOP_PROC_SEND_MESSAGE_PARAMS_T *params = (HSTOP_PROC_SEND_MESSAGE_PARAMS_T*)goal_data;
    void *message = NULL;

    DEBUG_LOG_VERBOSE("HeadsetTopology_ProcedureSendTopologyMessageStart MESSAGE:0x%x Msg:%p",
                      params->id, params->message);

    UNUSED(result_task);

    if (params->message && params->message_size)
    {
        message = PanicUnlessMalloc(params->message_size);
        memcpy(message, params->message, params->message_size);
    }

    /* procedure started synchronously so indicate success */
    proc_start_cfm_fn(hs_topology_procedure_send_message_to_topology, procedure_result_success);

    MessageSend(HeadsetTopologyGetTask(), params->id, message);

    Procedures_DelayedCompleteCfmCallback(proc_complete_fn,
                                          hs_topology_procedure_send_message_to_topology,
                                          procedure_result_success);

}

void HeadsetTopology_ProcedureSendTopologyMessageCancel(procedure_cancel_cfm_func_t proc_cancel_cfm_fn)
{
    UNUSED(proc_cancel_cfm_fn);

    DEBUG_LOG_ERROR("HeadsetTopology_ProcedureSendTopologyMessageCancel. Should never be called.");

    Panic();
}


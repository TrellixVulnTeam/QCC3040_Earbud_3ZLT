/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      simple topology procedure to send a message to the topology task.
*/

#include "tws_topology_private.h"
#include "tws_topology_config.h"
#include "tws_topology.h"
#include "tws_topology_procedure_send_topology_message.h"
#include "tws_topology_procedures.h"

#include <logging.h>
#include <message.h>
#include <panic.h>


void TwsTopology_ProcedureSendTopologyMessageStart(Task result_task,
                                                   procedure_start_cfm_func_t proc_start_cfm_fn,
                                                   procedure_complete_func_t proc_complete_fn,
                                                   Message goal_data);

void TwsTopology_ProcedureSendTopologyMessageCancel(procedure_cancel_cfm_func_t proc_cancel_cfm_fn);

const procedure_fns_t proc_send_topology_message_fns = {
    TwsTopology_ProcedureSendTopologyMessageStart,
    TwsTopology_ProcedureSendTopologyMessageCancel,
};

void TwsTopology_ProcedureSendTopologyMessageStart(Task result_task,
                                                  procedure_start_cfm_func_t proc_start_cfm_fn,
                                                  procedure_complete_func_t proc_complete_fn,
                                                  Message goal_data)
{
    TWSTOP_PROC_SEND_MESSAGE_PARAMS_T *params = (TWSTOP_PROC_SEND_MESSAGE_PARAMS_T*)goal_data;
    void *message = NULL;

    DEBUG_LOG("TwsTopology_ProcedureSendTopologyMessageStart MESSAGE:0x%x Msg:%p", 
              params->id, params->message);

    UNUSED(result_task);

    if (params->message && params->message_size)
    {
        message = PanicUnlessMalloc(params->message_size);
        memcpy(message, params->message, params->message_size);
    }

    /* procedure started synchronously so indicate success */
    proc_start_cfm_fn(tws_topology_procedure_send_message_to_topology, procedure_result_success);

    MessageSend(TwsTopologyGetTask(), params->id, message);

    Procedures_DelayedCompleteCfmCallback(proc_complete_fn,
                                          tws_topology_procedure_send_message_to_topology,
                                          procedure_result_success);

} 


void TwsTopology_ProcedureSendTopologyMessageCancel(procedure_cancel_cfm_func_t proc_cancel_cfm_fn)
{
    UNUSED(proc_cancel_cfm_fn);

    DEBUG_LOG("TwsTopology_ProcedureSendTopologyMessageCancel. Should never be called.");

    Panic();
}


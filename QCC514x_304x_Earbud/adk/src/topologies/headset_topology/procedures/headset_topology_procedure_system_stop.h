/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      
*/

#ifndef HEADSET_TOPOLOGY_PROC_SYSTEM_STOP_H
#define HEADSET_TOPOLOGY_PROC_SYSTEM_STOP_H

#include "script_engine.h"
#include "headset_topology_private.h"
#include "headset_topology_procedures.h"
#include "headset_topology_private.h"

typedef enum
{
    PROC_SEND_HS_TOPOLOGY_MESSAGE_SYSTEM_STOP_FINISHED = HEADSETTOP_INTERNAL_PROCEDURE_RESULTS_MSG_BASE | hs_topology_procedure_system_stop,
} hsprocSystemStopMessages;


extern const procedure_script_t headset_system_stop_script;

#endif /* HEADSET_TOPOLOGY_PROC_SYSTEM_STOP_H */

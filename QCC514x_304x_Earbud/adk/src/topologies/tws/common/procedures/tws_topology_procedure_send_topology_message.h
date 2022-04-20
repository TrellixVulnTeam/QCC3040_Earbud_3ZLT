/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Interface for simple topology procedure to send a message to the topology task.

            This will be used for scripts that require additional / special processing.
            Normal interaction between goals / scripts should be handled by having the
            goal raise events on success, failure or timeout.
*/

#ifndef TWS_TOPOLOGY_PROCEDURE_SEND_TOPOLOGY_MESSAGE_H
#define TWS_TOPOLOGY_PROCEDURE_SEND_TOPOLOGY_MESSAGE_H


#include "tws_topology_procedures.h"
#include "tws_topology_common_primary_rule_functions.h"

#include <message.h>

typedef struct
{
    MessageId id;
    Message   message;
    unsigned  message_size;
} TWSTOP_PROC_SEND_MESSAGE_PARAMS_T;

extern const procedure_fns_t proc_send_topology_message_fns;

#endif /* TWS_TOPOLOGY_PROCEDURE_SEND_TOPOLOGY_MESSAGE_H */


/*!
\copyright  Copyright (c) 2015 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Implementation of TWS Topology use of peer signalling marshalled message channel.
*/

#include "tws_topology.h"
#include "tws_topology_goals.h"
#include "tws_topology_peer_sig.h"
#include "tws_topology_private.h"
#include "tws_topology_typedef.h"
#include "tws_topology_marshal_typedef.h"
#include "tws_topology_rule_events.h"

#include <peer_signalling.h>
#include <power_manager.h>
#include <task_list.h>
#include <logging.h>
#include <timestamp_event.h>

#include <message.h>
#include <panic.h>
#include <marshal.h>
#include "voice_ui.h"


void TwsTopology_HandleMarshalledMsgChannelRxInd(PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND_T* ind)
{
    switch (ind->type)
    {
        case MARSHAL_TYPE(tws_topology_remote_rule_event_t):
            {
                tws_topology_remote_rule_event_t* event_req = (tws_topology_remote_rule_event_t*)ind->msg;

                DEBUG_LOG("TwsTopology_HandleMarshalledMsgChannelRxInd tws_topology_remote_rule_event_t event 0x%llx", event_req->event);
                twsTopology_RulesSetEvent(event_req->event);
            }
            break;

        default:
            break;
    }

    /* free unmarshalled msg */
    free(ind->msg);
}

void TwsTopology_HandleMarshalledMsgChannelTxCfm(PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM_T* cfm)
{
    UNUSED(cfm);
}

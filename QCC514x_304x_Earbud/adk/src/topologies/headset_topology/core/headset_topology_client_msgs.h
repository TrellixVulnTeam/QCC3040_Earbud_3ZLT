/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Interface to HEADSET Topology utility functions for sending messages to clients.
*/

#ifndef HEADSET_TOPOLOGY_CLIENT_MSGS_H
#define HEADSET_TOPOLOGY_CLIENT_MSGS_H
#include <handset_service.h>
#include "headset_topology.h"

/*! \brief Send indication to registered clients that handset disconnected goal has been reached.
    \param[in] Handset service disconnect status message.
*/
void HeadsetTopology_SendHandsetDisconnectedIndication(const handset_service_status_t status);


/*! \brief Send confirmation message to the task which called #HeadsetTopology_Stop().
    \param[in] Status Status of the start operation.

    \note It is expected that the task will be the application SM task.
*/
void HeadsetTopology_SendStopCfm(headset_topology_status_t status);

#endif /* HEADSET_TOPOLOGY_CLIENT_MSGS_H */

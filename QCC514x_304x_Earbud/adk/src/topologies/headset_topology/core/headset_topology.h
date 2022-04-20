/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   headset HS Topology
\ingroup    topologies       
\brief      Headset topology public interface.
*/

#ifndef HEADSET_TOPOLOGY_H_
#define HEADSET_TOPOLOGY_H_

#include "domain_message.h"
#include "handset_service.h"

/*! \brief Confirmation of completed handset disconnect request

    This message is sent once handset_topology service has completed the disconnect,
    or it has been cancelled.
*/
typedef struct
{
    /*! Status of the request */
    handset_service_status_t status;
} HEADSET_TOPOLOGY_HANDSET_DISCONNECTED_IND_T;

/*! Definition of messages that Headset Topology can send to clients. */
typedef enum
{
    /*! Indication to clients that handset have been disconnected. */
    HEADSET_TOPOLOGY_HANDSET_DISCONNECTED_IND = HEADSET_TOPOLOGY_MESSAGE_BASE,
    HEADSET_TOPOLOGY_STOP_CFM,

    /*! This must be the final message */
    HEADSET_TOPOLOGY_MESSAGE_END
} headset_topology_message_t;
ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(HEADSET_TOPOLOGY, HEADSET_TOPOLOGY_MESSAGE_END)

/*! Definition of status code returned by Headset Topology. */
typedef enum
{
    /*! The operation has been successful */
    hs_topology_status_success,

    /*! The requested operation has failed. */
    hs_topology_status_fail,
} headset_topology_status_t;


/*! Definition of the #HEADSET_TOPOLOGY_STOP_CFM message. */
typedef struct 
{
    /*! Result of the #HeadsetTopology_Stop() operation. 
        If this is not hs_topology_status_success then the topology was not 
        stopped cleanly within the time requested */
    headset_topology_status_t       status;
} HEADSET_TOPOLOGY_STOP_CFM_T;


/*! \brief Initialise the  Headset topology component

    \param init_task    Task to send init completion message (if any) to

    \returns TRUE
*/
bool HeadsetTopology_Init(Task init_task);


/*! \brief Start the  Headset topology

    The topology will run semi-autonomously from this point.

    \param requesting_task Task to send messages to

    \returns TRUE
*/
bool HeadsetTopology_Start(Task requesting_task);


/*! \brief Register client task to receive  Headset topology messages.
 
    \param[in] client_task Task to receive messages.
*/
void HeadsetTopology_RegisterMessageClient(Task client_task);


/*! \brief Unregister client task from Headset topology.
 
    \param[in] client_task Task to unregister.
*/
void HeadsetTopology_UnRegisterMessageClient(Task client_task);


/*! \brief function to prohibit or allow connection to handset in  Headset topology.

    Prohibits or allows topology to connect handset. When prohibited any connection attempt in progress will
    be cancelled and any connected handset will be disconnected.

    Note: By default handset connection is allowed.

    \param prohibit TRUE to prohibit handset connection, FALSE to allow.
*/
void HeadsetTopology_ProhibitHandsetConnection(bool prohibit);


/*! \brief Stop the Headset topology

    The topology will enter a known clean state then send a message to 
    confirm.

    The device should be restarted after the HEADSET_TOPOLOGY_STOP_CFM message
    is sent.

    \param requesting_task Task to send the response to

    \return TRUE
*/
bool HeadsetTopology_Stop(Task requesting_task);


/*! \brief Request topology to connect the most recently used handset.
*/
void HeadsetTopology_ConnectMruHandset(void);

/*! \brief Request topology to disconnect the least recently used handset.
*/
void HeadsetTopology_DisconnectLruHandset(void);


/*! \brief Request topology disconnect all handsets.
    \note This currently only disconnects one connected handset.
    \todo This functionality will need to be updated when multiple
          handsets connections are supported (multipoint)
*/
void HeadsetTopology_DisconnectAllHandsets(void);

#endif /* HEADSET_TOPOLOGY_H_ */

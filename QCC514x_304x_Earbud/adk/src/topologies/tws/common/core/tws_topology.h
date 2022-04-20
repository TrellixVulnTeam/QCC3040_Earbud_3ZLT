/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   tws TWS Topology
\ingroup    topologies       
\brief      TWS topology public interface.
*/

#ifndef TWS_TOPOLOGY_H_
#define TWS_TOPOLOGY_H_

#include <message.h>
#include <bdaddr.h>

#include <peer_find_role.h>
#include <power_manager.h>

#include "domain_message.h"

/*\{*/
/*! Definition of messages that TWS Topology can send to clients. */
typedef enum
{
    /*! Confirmation that the TWS Topology module has initialised, sent 
        once #TwsTopology_Init() has completed */
    TWS_TOPOLOGY_INIT_CFM =  TWS_TOPOLOGY_MESSAGE_BASE,

    /*! Confirmation that TWS Topology has started, sent in response
        to #TwsTopology_Start() */
    TWS_TOPOLOGY_START_CFM,

    /*! Confirmation that TWS Topology has stopped, sent in response
        to #TwsTopology_Stop() */
    TWS_TOPOLOGY_STOP_CFM,

    /*! Indication to clients that the Earbud role has changed. */
    TWS_TOPOLOGY_ROLE_CHANGED_IND,

    /*! Indication to clients that handset have been disconnected. */
    TWS_TOPOLOGY_HANDSET_DISCONNECTED_IND,

    /*! This must be the final message */
    TWS_TOPOLOGY_MESSAGE_END
} tws_topology_message_t;

/*! Definition of status code returned by TWS Topology. */
typedef enum
{
    /*! The operation has been successful */
    tws_topology_status_success,

    /*! The requested operation has failed. */
    tws_topology_status_fail,
} tws_topology_status_t;

/*! Definition of the Earbud roles in a TWS Topology. */
typedef enum
{
    /*! Role is not yet known. */
    tws_topology_role_none,

    /*! Earbud has the Primary role. */
    tws_topology_role_primary,

    /*! Earbud has the Secondary role. */
    tws_topology_role_secondary,

} tws_topology_role;

/*! Definition of the #TWS_TOPOLOGY_START_CFM message. */
typedef struct 
{
    /*! Result of the #TwsTopology_Start() operation. */
    tws_topology_status_t       status;

    /*! Current role of the Earbud. */
    tws_topology_role           role;
} TWS_TOPOLOGY_START_CFM_T;

/*! Definition of the #TWS_TOPOLOGY_STOP_CFM message. */
typedef struct 
{
    /*! Result of the #TwsTopology_Stop() operation. 
        If this is not tws_topology_status_success then the topology was not 
        stopped cleanly within the time requested */
    tws_topology_status_t       status;
} TWS_TOPOLOGY_STOP_CFM_T;

/*! Indication of a change in the Earbud role. */
typedef struct
{
    /*! New Earbud role. */
    tws_topology_role           role;
} TWS_TOPOLOGY_ROLE_CHANGED_IND_T;

/*! \brief Initialise the TWS topology component

    \param init_task    Task to send init completion message (if any) to

    \returns TRUE
*/
bool TwsTopology_Init(Task init_task);


/*! \brief Start the TWS topology

    The topology will run semi-autonomously from this point.

    \todo To allow for the application behaviour to be adapted, error
    conditions are reported to the application. This avoids continual
    retries and may allow applications to try different behaviour.
    
    \param requesting_task Task to send messages to
*/
void TwsTopology_Start(Task requesting_task);

/*! \brief Stop the TWS topology

    The topology will enter a known clean state then send a message to 
    confirm.

    The device should be restarted after the TWS_TOPOLOGY_STOP_CFM message
    is sent.

    \param requesting_task Task to send the response to
*/
void TwsTopology_Stop(Task requesting_task);

/*! \brief Register client task to receive TWS topology messages.
 
    \param[in] client_task Task to receive messages.
*/
void TwsTopology_RegisterMessageClient(Task client_task);

/*! \brief Unregister client task to stop receiving TWS topology messages.
 
    \param[in] client_task Task to unregister.
*/
void TwsTopology_UnRegisterMessageClient(Task client_task);

/*! \brief Find the current role of the Earbud.
    \return tws_topology_role Role of the Earbud.
*/
tws_topology_role TwsTopology_GetRole(void);

/*! \brief Utility function to easily determine Primary role.
    \return bool TRUE if Earbud is the Primary (including acting), 
        otherwise FALSE.
*/
bool TwsTopology_IsPrimary(void);

/*! \brief Utility function to easily determine Primary role.

    \return bool TRUE if Earbud is the Primary (excluding acting), 
        otherwise FALSE.
*/
bool TwsTopology_IsFullPrimary(void);

/*! \brief Utility function to easily determine Primary role.
    \return bool TRUE if Earbud is the Primary, otherwise FALSE.
*/
bool TwsTopology_IsActingPrimary(void);

/*! \brief Utility function to easily determine Primary role.
    \return bool TRUE if Earbud is the Primary, otherwise FALSE.
*/

bool TwsTopology_IsSecondary(void);

/*! \brief End DFU.

    Tell the topology that DFU has completed, regardless of whether it
    was successful.

    This indication is significant when the DFU ends as in-case.
    i.e. If indicated when in-case then the links can be dropped & eventually
    the main role is given up (as expected when in-case).
 */
void TwsTopology_EndDfu(void);

/*\}*/

/*! \brief function for Application to prohibit or allow handover.
   
   If app sets prohibit to TRUE, handover will not occur. 
   If app sets prohibit to FALSE, handover may occur when system
   determines handover should be performed.

   Note: By default handover is allowed. App may prohibit handover by calling this 
   function with TRUE parameter. 

    \param prohibit To prohibit or allow handover.
*/
void TwsTopology_ProhibitHandover(bool prohibit);

/*! \brief function to check if handover has been prohibited

    \return TRUE if handover has been disabled using TwsTopology_ProhibitHandover().
 */
bool TwsTopology_IsHandoverProhibited(void);

/*! \brief function to prohibit or allow connection to handset in TWS topology.
   
    Prohibits or allows topology to connect handset. When prohibited any connection attempt in progress will
    be cancelled and any connected handset will be disconnected.

    Note: By default handset connection is allowed.

    \param prohibit TRUE to prohibit handset connection, FALSE to allow.
*/
void TwsTopology_ProhibitHandsetConnection(bool prohibit);

/*! \brief Request topology connect the most recently used handset.
*/
void TwsTopology_ConnectMruHandset(void);

/*! \brief Request topology to disconnect the least recently used handset.
*/
void TwsTopology_DisconnectLruHandset(void);

/*! \brief Request topology disconnect all handsets.
    \note This currently only disconnects one connected handset.
    \todo This functionality will need to be updated when multiple
          handsets connections are supported (multipoint)
*/
void TwsTopology_DisconnectAllHandsets(void);

/*! \brief function to enable/disable DFU mode (used for in-case DFU)

    Request the topology to enable/disable DFU mode. It is used for profile
    retention for handset and peer profiles, if Earbud is going in case.

    \param val Boolean variable to enable/disable DFU mode
 */
void TwsTopology_SetDfuMode(bool val);

/*! \brief function to SET/RESET profile mask of peer profiles to connect.

    Request the topology to SET/RESET profile mask for given peer
    profiles to be connected, regardless of whether Earbud is going in the case.

    \param profile_mask A bit mask of the peer profiles to SET/RESET
    \param enable Boolean variable to SET/RESET peer profiles connect mask
 */
void TwsTopology_SetPeerProfileConnectMask(uint32 profile_mask, bool enable);

/*! \brief function to force topology to remain active for handset even earbud is in the case.

    Request the topology to remain active for handset regardless Earbud is going in the case.

    \param enable Boolean variable to force topology to remain active or not.
 */
void TwsTopology_EnableRemainActiveForHandset(bool remain_active_for_handset);

/*! \brief function to force topology to remain active for peer even earbud is in the case.

    Request the topology to remain active for peer regardless Earbud is going in the case.

    \param enable Boolean variable to force topology to remain active or not.
 */
void TwsTopology_EnableRemainActiveForPeer(bool remain_active_for_peer);
#endif /* TWS_TOPOLOGY_H_ */

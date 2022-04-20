/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       gaia_debug_plugin_router.h
\brief      Header file for the router that manages PyDbg Remote Debug commands addressed to the Secondary device.
*/

#ifndef GAIA_DEBUG_PLUGIN_ROUTER_H_
#define GAIA_DEBUG_PLUGIN_ROUTER_H_

#include <gaia.h>
#include "gaia_debug_plugin_router_l2cap_peer_link.h"


/*! \brief The value for invalid 'Routed Response Route' for error check. */
#define REMOTE_DEBUG_ROUTED_RESP_ROUTE_T_INVALID    (0xFF)


/*! \brief Handle 'Debug Tunnel To Chip' command.

    \param t                Pointer to transport instance.

    \param payload_length   Size of the command parameter data in bytes.

    \param payload          Command parameter(s) in uint8 array.

    This function parses the PyDbg remote debug packet and executes the request.

    Command Payload Format:
        0        1        2       ...       N     (Byte)
    +--------+--------+--------+--------+--------+
    |ClientID|   Tag  |     Tunneling payload    |
    +--------+--------+--------+--------+--------+

    Response Payload Format (if any):
        0        1        2       ...       N     (Byte)
    +--------+--------+--------+--------+--------+
    |ClientID|   Tag  |     Tunneling payload    |
    +--------+--------+--------+--------+--------+
*/
void GaiaDebugPlugin_DebugTunnelToChip(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload);


/*! \brief Callback function that receives messages from the peer device.

    \param peer_link_cmd    Command type of the message.

    \param payload_length   Size of the message in bytes.

    \param payload          Message data in uint8 array.
    
    PyDbg Remote Debug Command Format (Type = 2:IP Protocol 'Routed' Type)
         0        1        2        3        4       ...       N    (Byte)
     +--------+--------+--------+--------+--------+--------+--------+
     | Req/Rsp Routing | RtType | RtCmdID|     Payload (if any)     |
     +--------+--------+--------+--------+--------+--------+--------+
     |<----- 'Routed' Type Header  ----->|<----- RtCmd Payload ---->|
*/
#ifdef INCLUDE_L2CAP_MANAGER
void GaiaDebugPlugin_PydbgRoutingCbReceiveMessageFromPeer(const gaia_debug_l2cap_peer_link_message_command_t peer_link_cmd, const uint16 payload_length, const uint8 *payload);
#endif

/*! \brief Callback function that is called when attempt to connect to the peer
           device failed. (i.e. received a CONNECT_CFM with non successful status code).
*/
#ifdef INCLUDE_L2CAP_MANAGER
void GaiaDebugPlugin_PydbgRoutingCbPeerLinkFailedToConnect(void);
#endif

/*! \brief Callback function that is called when the link to the peer device is
           lost (i.e. DISCONNECT_IND is received).
*/
#ifdef INCLUDE_L2CAP_MANAGER
void GaiaDebugPlugin_PydbgRoutingCbPeerLinkDisconnectInd(void);
#endif

/*! \brief Callback function that is called when a handover process is started.
*/
#ifdef INCLUDE_L2CAP_MANAGER
void GaiaDebugPlugin_PydbgRoutingCbHandoverVeto(void);
#endif

/*! \brief Callback function that is called when a handover process has been completed.
*/
#ifdef INCLUDE_L2CAP_MANAGER
void GaiaDebugPlugin_PydbgRoutingCbHandoverComplete(GAIA_TRANSPORT *t, bool is_primary);
#endif

#endif /* GAIA_DEBUG_PLUGIN_ROUTER_H_ */
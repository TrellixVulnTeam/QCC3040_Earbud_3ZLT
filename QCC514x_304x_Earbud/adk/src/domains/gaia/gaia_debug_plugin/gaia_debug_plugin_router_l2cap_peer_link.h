/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       gaia_debug_plugin_router_l2cap_peer_link.h
\brief      Header file for the router that manages L2CAP Peer Link (to the Secondary device).
*/

#ifndef GAIA_DEBUG_PLUGIN_ROUTER_L2CAP_PEER_LINK_H_
#define GAIA_DEBUG_PLUGIN_ROUTER_L2CAP_PEER_LINK_H_

#if defined(INCLUDE_GAIA_PYDBG_REMOTE_DEBUG) && defined(INCLUDE_L2CAP_MANAGER)

#include <csrtypes.h>
#include "gaia.h"


/*! \brief The transport PDU size available for the GAIA Debug.
           This is the PDU size that the GAIA Debug can use. The PDU size
           includes all the protocol headers and their payloads.
           Since the GAIA Debug supports only the RFCOMM (over BR/EDR link) to
           the handset, this PDU size is equal to the GAIA's RFCOMM transport
           PDU size.
    \note  The same size is also applied to the link between the Primary and
           the Secondary devices for Earbud applications.
*/
#define GAIA_DEBUG_TRANSPORT_PDU_SIZE                               (254)


/*! \brief Peer link response timeout (in milliseconds) after sending a request to the Secondary device. */
#define GAIA_DEBUG_L2CAP_PEER_LINK_RESPONSE_TIMEOUT_IN_MS           (3000)


/*! \brief The status code for an attempt to send a Pydbg command to the peer device. */
typedef enum
{
    gaia_debug_l2cap_peer_link_send_status_success,
    gaia_debug_l2cap_peer_link_send_status_pending,

    gaia_debug_l2cap_peer_link_send_status_not_a_pair_type_device,
    gaia_debug_l2cap_peer_link_send_status_failed_to_get_peer_bdaddr,

    gaia_debug_l2cap_peer_link_send_status_failure_with_unknown_reason,


    gaia_debug_l2cap_peer_link_send_status_rejected_due_to_ongoing_handover,
    gaia_debug_l2cap_peer_link_send_status_failure_peer_unreachable,

} gaia_debug_l2cap_peer_link_send_status_t;


/*! \brief Commands for the GAIA Debug L2CAP Peer Link. */
typedef enum
{
    link_message_command_invalid                = 0,    /*!< Invalid command. */
    link_message_command_ping,                          /*!< Ping command for checking the peer is reachable or not. */
    link_message_command_req,                           /*!< Request message that expects a response message. */
    link_message_command_rsp,                           /*!< Response message to a request message. */
    link_message_command_error,                         /*!< Error response to a request message. */

    link_message_command_text,                          /*!< A text string (for testing). */

} gaia_debug_l2cap_peer_link_message_command_t;


/*! \brief Table of callback handler funcitons, which are called by the GAIA
           Debug L2CAP Peer Link to notify events.
 */
typedef struct
{
    /*! Function pointer type for a function that receives messages from the peer device. */
    void (*handle_peer_link_received_messages)(const gaia_debug_l2cap_peer_link_message_command_t, const uint16, const uint8*);
    /*! Function pointer type for a function that is called when failed to connect to the peer device. */
    void (*handle_peer_link_failed_to_connect)(void);
    /*! Function pointer type for a function that is called when the peer link is disconnected (i.e. DISCONNECT_IND is received). */
    void (*handle_peer_link_disconnect_ind)(void);

    /*! Function pointer type for a function that is called when a handover process is started. */
    void (*handle_peer_link_handover_veto)(void);
    /*! Function pointer type for a function that is called when a handover process has been completed. */
    void (*handle_peer_link_handover_complete)(GAIA_TRANSPORT *t, bool is_primary);

} gaia_debug_l2cap_peer_link_functions_t;





/*! \brief Initialise the L2CAP Peer Link for GAIA Debug plugin.

    This function sets up the L2CAP Peer Link: Obtain a PSM and register L2CAP
    and the SDP record.
*/
void GaiaDebugPlugin_L2capPeerLinkInit(void);


/*! \brief Register a callback function that receives messages sent over the
           L2CAP Peer Link from the peer device.
 */
void GaiaDebugPlugin_L2capPeerLinkRegisterCallbackFunctions(const gaia_debug_l2cap_peer_link_functions_t *functions);


/*! \brief Send a Pydbg Remote Debug command to the peer device.
 */
gaia_debug_l2cap_peer_link_send_status_t GaiaDebugPlugin_L2capPeerLinkSend(gaia_debug_l2cap_peer_link_message_command_t peer_link_cmd, uint16 payload_length, const uint8 *payload);


/*! \brief Disconnect the L2CAP Peer Link.
 */
bool GaiaDebugPlugin_L2capPeerLinkDisconnect(void);


/*! \brief Discard transmitting data saved in the Tx Buffer.
 */
void GaiaDebugPlugin_L2capPeerLinkDiscardTxBufferredData(void);



/*! \brief Callback function that is called when the link to the mobile app is connected. */
void GaiaDebugPlugin_L2capPeerLinkCbGaiaLinkConnect(GAIA_TRANSPORT *t);


/*! \brief Callback function that is called when the link to the mobile app is disconnected. */
void GaiaDebugPlugin_L2capPeerLinkCbGaiaLinkDisconnect(GAIA_TRANSPORT *t);


/*! \brief Handover callback function that can 'veto' an ongoing handover. */
bool GaiaDebugPlugin_L2capPeerLinkCbHandoverVeto(GAIA_TRANSPORT *t);


/*! \brief Handover callback function that is called when a handover process has been completed. */
void GaiaDebugPlugin_L2capPeerLinkCbHandoverComplete(GAIA_TRANSPORT *t, bool is_primary);


#endif /* INCLUDE_GAIA_PYDBG_REMOTE_DEBUG && INCLUDE_L2CAP_MANAGER */
#endif /* GAIA_DEBUG_PLUGIN_ROUTER_L2CAP_PEER_LINK_H_ */

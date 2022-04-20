/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       gaia_debug_plugin_router.c
\brief      The router manages PyDbg Remote Debug commands addressed to the Secondary device.
*/

#ifdef INCLUDE_GAIA_PYDBG_REMOTE_DEBUG

#include "gaia_debug_plugin_router.h"
#include "gaia_debug_plugin_router_l2cap_peer_link.h"

#include <panic.h>
#include <stdlib.h>


/* Enable debug log outputs with per-module debug log levels.
 * The log output level for this module can be changed with the PyDbg command:
 *      >>> apps1.log_level("gaia_debug_plugin_router", 3)
 * Where the second parameter value means:
 *      0:ERROR, 1:WARN, 2:NORMAL(= INFO), 3:VERBOSE(= DEBUG), 4:V_VERBOSE(= VERBOSE), 5:V_V_VERBOSE(= V_VERBOSE)
 * See 'logging.h' and PyDbg 'log_level()' command descriptions for details. */
#define DEBUG_LOG_MODULE_NAME gaia_debug_plugin_router
#include <logging.h>
DEBUG_LOG_DEFINE_LEVEL_VAR

#include <bt_device.h>
#include <multidevice.h>

#include "gaia_debug_plugin.h"
#include "gaia_debug_plugin_router_private.h"
#include "gaia_debug_plugin_pydbg_remote_debug.h"
#include "remote_debug_prim.h"


#undef  PRETEND_AS_IF_THE_RESPONSE_COMES_FROM_THE_REQUESTED_DEVICE_BY_PYDBG     /* ToDo: Tentative. Remove this later! */


/******************************************************************************
 * Macros/Defines used within this source file.
 ******************************************************************************/
/*! \brief Data struct for context data that the Primary saves when it forwards
           a request to the Secondary. The saved context will be restored when
           the Primary receives the response from the Secondary. */
typedef struct 
{
    bool                            in_use;
    pydbg_remote_debug_pdu_info_t   pdu_info;

} pydbg_remote_debug_context_data_t;



/******************************************************************************
 * File-scope variables.
 ******************************************************************************/
static pydbg_remote_debug_context_data_t    saved_context =
{
    .in_use = FALSE,
    .pdu_info = { 0 }
};


/*! \brief Hydra log string definitions. */
HYDRA_LOG_STRING(prim, "PRIMARY");
HYDRA_LOG_STRING(secnd, "SECONDARY");
HYDRA_LOG_STRING(left, "LEFT");
HYDRA_LOG_STRING(right, "RIGHT");
HYDRA_LOG_STRING(invalid, "INVALID!");

HYDRA_LOG_STRING(prim_left, "PRIMARY-Left");
HYDRA_LOG_STRING(prim_right, "PRIMARY-Right");
HYDRA_LOG_STRING(secondary_left, "SECONDARY-Left");
HYDRA_LOG_STRING(secondary_right, "SECONDARY-RIGHT");

HYDRA_LOG_STRING(yes, "YES");
HYDRA_LOG_STRING(no, "NO");


/*! \brief A DEBUG_LOG parsing macro to display a 'Request routing to'. */
#define GAIA_DEBUG_DEBUG_LOG_REQ_ROUTING(routed_req_to, type_str) \
    { \
        DEBUG_LOG_##type_str("GaiaDebugPlugin Request-routing to: 0x%04X (%s)", routed_req_to, \
                                  (routed_req_to == REMOTE_DEBUG_ROUTED_REQ_ROUTE_T_PRIMARY) ? prim : \
                                  ((routed_req_to == REMOTE_DEBUG_ROUTED_REQ_ROUTE_T_SECONDARY) ? secnd : \
                                  ((routed_req_to == REMOTE_DEBUG_ROUTED_REQ_ROUTE_T_LEFT) ? left : \
                                  ((routed_req_to == REMOTE_DEBUG_ROUTED_REQ_ROUTE_T_RIGHT) ? right : invalid)))); \
    }

/*! \brief A DEBUG_LOG parsing macro to display a 'Response routing from'. */
#define GAIA_DEBUG_DEBUG_LOG_RESP_ROUTING(routed_rsp_from, type_str) \
    { \
        DEBUG_LOG_##type_str("GaiaDebugPlugin Response-routing: 0x%04X (%s)", routed_rsp_from, \
                            (routed_rsp_from == REMOTE_DEBUG_ROUTED_RESP_ROUTE_T_LEFT_PRIMARY) ? prim_left : \
                            ((routed_rsp_from == REMOTE_DEBUG_ROUTED_RESP_ROUTE_T_RIGHT_PRIMARY) ? prim_right : \
                            ((routed_rsp_from == REMOTE_DEBUG_ROUTED_RESP_ROUTE_T_LEFT_SECONDARY) ? secondary_left : \
                            ((routed_rsp_from == REMOTE_DEBUG_ROUTED_RESP_ROUTE_T_RIGHT_SECONDARY) ? secondary_right : invalid)))); \
    }


/******************************************************************************
 * Static function prototypes.
 ******************************************************************************/
static void gaiaDebugPlugin_PydbgRoutingSaveContext(const pydbg_remote_debug_pdu_info_t *pdu_info);
static bool gaiaDebugPlugin_PydbgRoutingLoadContext(pydbg_remote_debug_pdu_info_t *pdu_info);
static void gaiaDebugPlugin_PydbgRoutingSendTunnelPduResponse(pydbg_remote_debug_pdu_info_t *pdu_info, allocated_pydbg_rsp_pdu_t *rsp_pdu, uint8 rsp_cmd_id, uint16 payload_size);
static void gaiaDebugPlugin_PydbgRoutingSendUnroutableTunnelPduResponse(uint8 reason);
#ifdef INCLUDE_L2CAP_MANAGER
static void gaiaDebugPlugin_PydbgRoutingSendRspToPrimary(gaia_debug_l2cap_peer_link_message_command_t peer_link_cmd, uint16 payload_length, const uint8 *payload);
#endif
static void gaiaDebugPlugin_PydbgRoutingHandler(GAIA_TRANSPORT *t, uint8 gaia_client_id, uint8 gaia_tag, uint16 pydbg_pdu_length, const uint8 *pydbg_pdu);
static void gaiaDebugPlugin_PydbgRoutedCommandHandler(pydbg_remote_debug_pdu_info_t *pdu_info, REMOTE_DEBUG_ROUTE_CMD cmd_id, uint16 payload_length, const uint8 *payload);
static bool gaiaDebugPlugin_IsThisTheDestinedDevice(REMOTE_DEBUG_ROUTED_REQ_ROUTE_T req_to);
#ifdef PRETEND_AS_IF_THE_RESPONSE_COMES_FROM_THE_REQUESTED_DEVICE_BY_PYDBG
static REMOTE_DEBUG_ROUTED_RESP_ROUTE_T gaiaDebugPlugin_GetRespRouting(REMOTE_DEBUG_ROUTED_REQ_ROUTE_T req_to);
#else
static REMOTE_DEBUG_ROUTED_RESP_ROUTE_T gaiaDebugPlugin_GetRespRouting(void);
#endif


/******************************************************************************
 * Public API functions
 ******************************************************************************/
void GaiaDebugPlugin_DebugTunnelToChip(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    DEBUG_LOG_DEBUG("GaiaDebugPlugin DbgTunnelToChip");

    if (payload_length >= (number_of_debug_plugin_debug_tunnel_to_chip_cmd_bytes + PYDBG_REMOTE_DEBUG_PDU_DEBUG_TYPE_HEADER_SIZE))
    {
        uint8 client_id = payload[debug_plugin_debug_tunnel_to_chip_cmd_client_id];
        uint8 tag       = payload[debug_plugin_debug_tunnel_to_chip_cmd_tag];

        gaiaDebugPlugin_PydbgRoutingHandler(t, client_id, tag,
                                            payload_length - number_of_debug_plugin_debug_tunnel_to_chip_cmd_bytes,
                                            &payload[debug_plugin_debug_tunnel_to_chip_cmd_payload_0]);
    }
    else
    {
        DEBUG_LOG_WARN("GaiaDebugPlugin TunnelToChip: WARNING! Invalid PDU Size:%d", payload_length);
        GaiaFramework_SendError(t, GAIA_DEBUG_FEATURE_ID, debug_tunnel_to_chip, debug_plugin_status_invalid_parameters);
    }
}


/******************************************************************************
 * Public API Callback functions
 ******************************************************************************/
#ifdef INCLUDE_L2CAP_MANAGER
void GaiaDebugPlugin_PydbgRoutingCbReceiveMessageFromPeer(const gaia_debug_l2cap_peer_link_message_command_t peer_link_cmd, const uint16 payload_length, const uint8 *payload)
{
    DEBUG_LOG_DEBUG("GaiaDebugPlugin PydbgRoutingCbReceiveMessageFromPeer: (Cmd:%d, Size:%d)", peer_link_cmd, payload_length);
    GAIA_DEBUG_DEBUG_LOG_FORMATTED_ARRAY(payload_length, payload, V_VERBOSE);

    if (BtDevice_IsMyAddressPrimary())
    {
        /* Primary device has received a message from the Secondary device. */
        /* PyDbg Remote Debug Command Format (Type=2:IP Protocol 'Routed' Type, CmdId=1:Routed Response)
         *      0        1        2        3        4        5        6        7        8        9        10       11       12      ...       N    (Byte)
         *  +--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+
         *  |ClientID|   Tag  | Type=2 | Cmd ID |  Payload Length |  Tag (Seq No.)  | Response Routing| RtType | RtCmdID|     Payload (if any)     |
         *  +--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+
         *  |<-  Tunnelling ->|<-----  PyDbg Remote Debug Protocol Header (Type = 2 : IP Protocol 'Routed' Type)  ----->|<---- PyDbg Payload  ---->|
         *  |<---------------------------------------------- header_size ---------------------------------------------->|<------- payload -------->|
         *  |                          |<------------------------------------------- cmd_rsp_message --------------------------------------------->|
         *  |<----------------------------------------------------------- rsp_pdu->pdu ----------------------------------------------------------->|
         *  |                                                                       |<--------------- The PDU send back to the peer -------------->|
         */
        if (peer_link_cmd == link_message_command_rsp)
        {
            pydbg_remote_debug_pdu_info_t   pdu_info;

            if (gaiaDebugPlugin_PydbgRoutingLoadContext(&pdu_info))
            {
                allocated_pydbg_rsp_pdu_t rsp_msg;
                const uint16 tunnelling_header_size = (GAIA_DEBUG_TUNNEL_TO_CHIP_CMD_RSP_PARAMETER_HEADER_SIZE + PYDBG_REMOTE_DEBUG_PDU_ROUTED_TYPE_HEADER_SIZE);
                uint16 pydbg_payload_size = payload_length - REMOTE_DEBUG_ROUTED_RESP_T_PAYLOAD_BYTE_OFFSET;
                REMOTE_DEBUG_ROUTED_RESP_T *rsp = (REMOTE_DEBUG_ROUTED_RESP_T*) payload;
                uint8 rsp_cmd_id = REMOTE_DEBUG_ROUTED_RESP_T_ROUTED_CMD_ID_GET(rsp);

                GaiaDebugPlugin_PydbgRoutingMallocRspPDU(&rsp_msg, REMOTE_DEBUG_CMD_TYPE_ROUTED_CMD, pydbg_payload_size);
                memmove(&rsp_msg.pdu[tunnelling_header_size], payload, payload_length);

                pdu_info.routed_pdu_type = REMOTE_DEBUG_ROUTED_RESP_T_ROUTED_TYPE_GET(rsp);
                pdu_info.routed_rsp_from = REMOTE_DEBUG_ROUTED_RESP_T_RESPONSE_ROUTING_GET(rsp);
                GAIA_DEBUG_DEBUG_LOG_RESP_ROUTING(pdu_info.routed_rsp_from, DEBUG);

                DEBUG_LOG_DEBUG("GaiaDebugPlugin PydbgRoutingCbReceiveMessageFromPeer:  - routed_pdu_type: 0x%02X", pdu_info.routed_pdu_type);
                DEBUG_LOG_DEBUG("GaiaDebugPlugin PydbgRoutingCbReceiveMessageFromPeer:  - routed_rsp_from: 0x%04X", pdu_info.routed_rsp_from);
                gaiaDebugPlugin_PydbgRoutingSendTunnelPduResponse(&pdu_info, &rsp_msg, rsp_cmd_id, pydbg_payload_size);
                free(rsp_msg.pdu);
            }
        }
        else if (peer_link_cmd == link_message_command_error)
        {
            uint8 status_code = payload[0];
            GaiaDebugPlugin_PydbgRoutingSendError(status_code);
        }
        else
        {
            DEBUG_LOG_ERROR("GaiaDebugPlugin PydbgRoutingCbReceiveMessageFromPeer: ERROR! Invalid peer link command:%d", peer_link_cmd);
            Panic();
        }
    }
    else
    {
        /* Secondary device has received a request message from the Primary device. */
        /* PyDbg Remote Debug Command Format (Type=2:IP Protocol 'Routed' Type, CmdId=0:Routed Request)
         *      0        1        2        3        4       ...       N    (Byte)
         *  +--------+--------+--------+--------+--------+--------+--------+
         *  | Request Routing | RtType | RtCmdID|     Payload (if any)     |
         *  +--------+--------+--------+--------+--------+--------+--------+
         *  |<----- 'Routed' Type Header  ----->|<----- RtCmd Payload ---->|
         */
        REMOTE_DEBUG_ROUTED_REQ_T *routed_req_payload = (REMOTE_DEBUG_ROUTED_REQ_T*) payload;
        REMOTE_DEBUG_ROUTED_REQ_ROUTE_T routed_req_to = REMOTE_DEBUG_ROUTED_REQ_T_REQUEST_ROUTING_GET(routed_req_payload);
        REMOTE_DEBUG_CMD_TYPE routed_pdu_type         = REMOTE_DEBUG_ROUTED_REQ_T_ROUTED_TYPE_GET(routed_req_payload);
        REMOTE_DEBUG_ROUTE_CMD routed_cmd_id          = REMOTE_DEBUG_ROUTED_REQ_T_ROUTED_CMD_ID_GET(routed_req_payload);

        if (payload_length < REMOTE_DEBUG_ROUTED_REQ_T_PAYLOAD_BYTE_OFFSET)
        {
            DEBUG_LOG_WARN("GaiaDebugPlugin PydbgRoutingCbReceiveMessageFromPeer: WARNING! Routed Req payload is too small (%d:less than 4)", payload_length);
            GaiaDebugPlugin_PydbgRoutingSendError(debug_plugin_status_invalid_parameters);
            return;
        }

        switch (routed_pdu_type)
        {
            case REMOTE_DEBUG_CMD_TYPE_DEBUG_CMD:
            {
                if (gaiaDebugPlugin_IsThisTheDestinedDevice(routed_req_to))
                {
                    /* No need to forward the request to the Secondary Earbud. */
                    const uint8 *dbg_cmd_payload = NULL;
                    uint16 dbg_cmd_payload_size = payload_length - REMOTE_DEBUG_ROUTED_REQ_T_PAYLOAD_BYTE_OFFSET;

                    if (0 < dbg_cmd_payload_size)
                    {
                        dbg_cmd_payload = &payload[REMOTE_DEBUG_ROUTED_REQ_T_PAYLOAD_BYTE_OFFSET];
                    }
                    DEBUG_LOG_DEBUG("GaiaDebugPlugin PydbgRoutingCbReceiveMessageFromPeer: Routed(Route:0x%04X Cmd:0x%02X, Type:0x%02X)", \
                                    routed_req_to, routed_cmd_id, routed_pdu_type);
                    
                    DEBUG_LOG_DEBUG("  MessageFromPeer: .payload_size:    %d", dbg_cmd_payload_size);
                    GaiaDebugPlugin_PyDbgDebugCommandHandler(REMOTE_DEBUG_CMD_TYPE_ROUTED_CMD, routed_cmd_id, dbg_cmd_payload_size, dbg_cmd_payload);
                }
                break;
            }

            default:
                DEBUG_LOG_WARN("GaiaDebugPlugin PydbgRoutingCbReceiveMessageFromPeer: ERROR! Not supported Routed PDU-Type:%d", routed_pdu_type);
                GaiaDebugPlugin_PydbgRoutingSendError(debug_plugin_status_invalid_parameters);
                return;
        }
    }
}
#endif /* INCLUDE_L2CAP_MANAGER */


#ifdef INCLUDE_L2CAP_MANAGER
static void gaiaDebugPlugin_UnableToConnectToPeer(uint8 reason)
{
    if (BtDevice_IsMyAddressPrimary())
    {
        DEBUG_LOG_DEBUG("GaiaDebugPlugin UnableToConnectToPeer: This is PRIMARY");
        if (saved_context.in_use)
        {
            DEBUG_LOG_DEBUG("GaiaDebugPlugin UnableToConnectToPeer: Context data is available.");

            /* Note that this callback function can be called in both the Primary/Secondary roles.
             * Only the Primary device, which has the context data to the host, must send the
             * 'Unroutable Response' to the host. */
            GaiaDebugPlugin_L2capPeerLinkDiscardTxBufferredData();
            gaiaDebugPlugin_PydbgRoutingSendUnroutableTunnelPduResponse(reason);
        }
    }
}
#endif /* INCLUDE_L2CAP_MANAGER */


#ifdef INCLUDE_L2CAP_MANAGER
void GaiaDebugPlugin_PydbgRoutingCbPeerLinkFailedToConnect(void)
{
    DEBUG_LOG_DEBUG("GaiaDebugPlugin PydbgRoutingCbPeerLinkFailedToConnect: CONNECT_CFM with unsuccessful status code.");
    gaiaDebugPlugin_UnableToConnectToPeer(REMOTE_DEBUG_ROUTED_REASON_LINK_CLOSED);
}
#endif /* INCLUDE_L2CAP_MANAGER */


#ifdef INCLUDE_L2CAP_MANAGER
void GaiaDebugPlugin_PydbgRoutingCbPeerLinkDisconnectInd(void)
{
    DEBUG_LOG_DEBUG("GaiaDebugPlugin PydbgRoutingCbPeerLinkDisconnectInd: DISCONNECT_IND, Connection Lost!");
    gaiaDebugPlugin_UnableToConnectToPeer(REMOTE_DEBUG_ROUTED_REASON_LINK_LOST);
}
#endif /* INCLUDE_L2CAP_MANAGER */


#ifdef INCLUDE_L2CAP_MANAGER
void GaiaDebugPlugin_PydbgRoutingCbHandoverVeto(void)
{
    DEBUG_LOG_DEBUG("GaiaDebugPlugin PydbgRoutingCbHandoverVeto");

    if (BtDevice_IsMyAddressPrimary())
    {
        if (saved_context.in_use)
        {
            /* If the Primary device is waiting for a response from the Secondary,
             * immediately cancel it and send the 'Unroutable Response' back to
             * the mobile app. */
            GaiaDebugPlugin_L2capPeerLinkDiscardTxBufferredData();
            gaiaDebugPlugin_PydbgRoutingSendUnroutableTunnelPduResponse(REMOTE_DEBUG_ROUTED_REASON_HANDOVER);
            DEBUG_LOG_INFO("GaiaDebugPlugin PydbgRoutingCbHandoverVeto: Cancelled waiting for response from the Secondary!");
        }
    }
}
#endif /* INCLUDE_L2CAP_MANAGER */


#ifdef INCLUDE_L2CAP_MANAGER
void GaiaDebugPlugin_PydbgRoutingCbHandoverComplete(GAIA_TRANSPORT *t, bool is_primary)
{
    UNUSED(t);
    DEBUG_LOG_DEBUG("GaiaDebugPlugin PydbgRoutingCbHandoverComplete: (is_primary:%d)", is_primary);

    /* Make sure that previous role's state does not affect the new role. */
    saved_context.in_use = FALSE;
}
#endif /* INCLUDE_L2CAP_MANAGER */


/******************************************************************************
 * Public API functions for 'Pydbg Remote Debug' message handler
 ******************************************************************************/
void GaiaDebugPlugin_PydbgRoutingMallocRspPDU(allocated_pydbg_rsp_pdu_t *result, const REMOTE_DEBUG_CMD_TYPE pdu_type, const uint16 payload_size)
{
    uint16 header_size;

    switch (pdu_type)
    {
        case REMOTE_DEBUG_CMD_TYPE_DEBUG_CMD:
            /* PyDbg Remote Debug Command Format (Type = 1:IP Protocol 'Debug' Type)
             *      0        1        2        3        4        5        6        7        8       ...       N    (Byte)
             *  +--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+
             *  |ClientID|   Tag  | Type=1 | Cmd ID |  Payload Length |  Tag (Seq No.)  |     Payload (if any)     |
             *  +--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+
             *  |<-  Tunnelling ->|<-----   PyDbg Remote Debug Protocol Header    ----->|<---- PyDbg Payload  ---->|
             *  |<--------------------------- header_size ----------------------------->|<------- payload -------->|
             *  |                          |<-------------------------- cmd_rsp_message -------------------------->|
             *  |<----------------------------------------- PDU -------------------------------------------------->|
             */
            header_size = GAIA_DEBUG_TUNNEL_TO_CHIP_CMD_RSP_PARAMETER_HEADER_SIZE + PYDBG_REMOTE_DEBUG_PDU_DEBUG_TYPE_HEADER_SIZE;
            break;

        case REMOTE_DEBUG_CMD_TYPE_ROUTED_CMD:
            /* PyDbg Remote Debug Command Format (Type = 2:IP Protocol 'Routed' Type)
             *      0        1        2        3        4        5        6        7        8        9        10       11       12      ...       N    (Byte)
             *  +--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+
             *  |ClientID|   Tag  | Type=2 | Cmd ID |  Payload Length |  Tag (Seq No.)  | Req/Rsp Routing | RtType | RtCmdID|     Payload (if any)     |
             *  +--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+
             *  |<-  Tunnelling ->|<-----  PyDbg Remote Debug Protocol Header (Type = 2 : IP Protocol 'Routed' Type)  ----->|<---- PyDbg Payload  ---->|
             *  |<---------------------------------------------- header_size ---------------------------------------------->|<------- payload -------->|
             *  |                          |<------------------------------------------- cmd_rsp_message --------------------------------------------->|
             *  |<----------------------------------------- PDU -------------------------------------------------------------------------------------->|
             */
            header_size = GAIA_DEBUG_TUNNEL_TO_CHIP_CMD_RSP_PARAMETER_HEADER_SIZE + PYDBG_REMOTE_DEBUG_PDU_ROUTED_TYPE_HEADER_SIZE
                        + REMOTE_DEBUG_ROUTED_RESP_T_PAYLOAD_BYTE_OFFSET;
            break;

        default:
            DEBUG_LOG_ERROR("GaiaDebugPlugin PydbgRoutingMallocRspPDU: ERROR! Invalid PyDbg PDU Type:%d", pdu_type);
            Panic();
            result->pdu_size        = 0;
            result->pdu             = NULL;
            result->payload         = NULL;
            result->cmd_rsp_message = NULL;
            return;
    }

    result->pdu_size = header_size + payload_size;
    result->pdu = PanicUnlessMalloc(result->pdu_size);
    result->payload = &result->pdu[header_size];
    result->cmd_rsp_message = &result->pdu[GAIA_DEBUG_TUNNEL_TO_CHIP_CMD_RSP_PARAMETER_HEADER_SIZE + PYDBG_REMOTE_DEBUG_IP_PROTOCOL_TYPE_FIELD_SIZE];
}


/*! \brief Send a normal response back to the mobile app. */
void GaiaDebugPlugin_PydbgRoutingSendResponse(allocated_pydbg_rsp_pdu_t *rsp_pdu, uint8 rsp_cmd_id, uint16 payload_size)
{
    pydbg_remote_debug_pdu_info_t pdu_info;

    DEBUG_LOG_DEBUG("GaiaDebugPlugin PydbgRoutingSendResponse:");
#ifdef INCLUDE_L2CAP_MANAGER
    if (BtDevice_IsMyAddressPrimary())
#endif
    {
        /* This is the Primary:
         * Send a response back to the mobile app. */
        if (gaiaDebugPlugin_PydbgRoutingLoadContext(&pdu_info))
        {
#ifdef PRETEND_AS_IF_THE_RESPONSE_COMES_FROM_THE_REQUESTED_DEVICE_BY_PYDBG
            pdu_info.routed_rsp_from = gaiaDebugPlugin_GetRespRouting(pdu_info->routed_req_to);
#else
            pdu_info.routed_rsp_from = gaiaDebugPlugin_GetRespRouting();
#endif
            GAIA_DEBUG_DEBUG_LOG_RESP_ROUTING(pdu_info.routed_rsp_from, DEBUG);

            gaiaDebugPlugin_PydbgRoutingSendTunnelPduResponse(&pdu_info, rsp_pdu, rsp_cmd_id, payload_size);
        }
    }
#ifdef INCLUDE_L2CAP_MANAGER
    else
    {
        /* This is the Secondary:
         * Send a response back to the Primary device. */
        /* PyDbg Remote Debug Command Format (Type=2:IP Protocol 'Routed' Type, CmdId=1:Routed Response)
         *      0        1        2        3        4        5        6        7        8        9        10       11       12      ...       N    (Byte)
         *  +--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+
         *  |ClientID|   Tag  | Type=2 | Cmd ID |  Payload Length |  Tag (Seq No.)  | Response Routing| RtType | RtCmdID|     Payload (if any)     |
         *  +--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+
         *  |<-  Tunnelling ->|<-----  PyDbg Remote Debug Protocol Header (Type = 2 : IP Protocol 'Routed' Type)  ----->|<---- PyDbg Payload  ---->|
         *  |<---------------------------------------------- header_size ---------------------------------------------->|<------- payload -------->|
         *  |                          |<------------------------------------------- cmd_rsp_message --------------------------------------------->|
         *  |<----------------------------------------------------------- rsp_pdu->pdu ----------------------------------------------------------->|
         *  |                                                                       |                                                              |
         *  |                 (This part is added by the Primary)                   |<--------------- The PDU send back to the peer -------------->|
         */
        const uint16 tunnelling_header_size = (GAIA_DEBUG_TUNNEL_TO_CHIP_CMD_RSP_PARAMETER_HEADER_SIZE + PYDBG_REMOTE_DEBUG_PDU_ROUTED_TYPE_HEADER_SIZE);
        uint16 peer_pdu_size = rsp_pdu->pdu_size - tunnelling_header_size;
        uint8 *peer_pdu      = &rsp_pdu->pdu[tunnelling_header_size];
        REMOTE_DEBUG_ROUTED_RESP_T *rsp = (REMOTE_DEBUG_ROUTED_RESP_T*) peer_pdu;

        if (Multidevice_IsLeft())
            REMOTE_DEBUG_ROUTED_RESP_T_RESPONSE_ROUTING_SET(rsp, REMOTE_DEBUG_ROUTED_RESP_ROUTE_T_LEFT_SECONDARY);
        else
            REMOTE_DEBUG_ROUTED_RESP_T_RESPONSE_ROUTING_SET(rsp, REMOTE_DEBUG_ROUTED_RESP_ROUTE_T_RIGHT_SECONDARY);

        REMOTE_DEBUG_ROUTED_RESP_T_ROUTED_TYPE_SET(rsp, REMOTE_DEBUG_CMD_TYPE_DEBUG_CMD);               /* FixMe: Hard-coded Routed-Type. */
        REMOTE_DEBUG_ROUTED_RESP_T_ROUTED_CMD_ID_SET(rsp, rsp_cmd_id);

        DEBUG_LOG_DEBUG("GaiaDebugPlugin PydbgRoutingSendResponse:  Rsp sent to the Primary: (Size:%d)", peer_pdu_size);
        GAIA_DEBUG_DEBUG_LOG_FORMATTED_ARRAY(peer_pdu_size, peer_pdu, V_VERBOSE);

        gaiaDebugPlugin_PydbgRoutingSendRspToPrimary(link_message_command_rsp, peer_pdu_size, peer_pdu);
    }
#endif /* INCLUDE_L2CAP_MANAGER */
}


/*! \brief Send an error response back to the mobile app. */
void GaiaDebugPlugin_PydbgRoutingSendError(uint8 status_code)
{
#ifdef INCLUDE_L2CAP_MANAGER
    if (BtDevice_IsMyAddressPrimary())
#endif
    {
        pydbg_remote_debug_pdu_info_t pdu_info;

        if (gaiaDebugPlugin_PydbgRoutingLoadContext(&pdu_info))
        {
            DEBUG_LOG_DEBUG("GaiaDebugPlugin PydbgRoutingSendError: Error code (sent to mobile). (Error:0x%02X)", status_code);
            GaiaFramework_SendError(pdu_info.gaia_transport, GAIA_DEBUG_FEATURE_ID, debug_tunnel_to_chip, status_code);
        }
    }
#ifdef INCLUDE_L2CAP_MANAGER
    else
    {
        DEBUG_LOG_DEBUG("GaiaDebugPlugin PydbgRoutingSendError:  Error code (sent to Primary): (Error:0x%02X)", status_code);
        gaiaDebugPlugin_PydbgRoutingSendRspToPrimary(link_message_command_error, sizeof(status_code), &status_code);
    }
#endif /* INCLUDE_L2CAP_MANAGER */
}


/*! \brief Return the type of this device (Primary/Secondary & Left/Right). */
gaia_debug_device_type_t GaiaDebugPlugin_GetDeviceType(void)
{
    if (Multidevice_IsPair())
    {
        if (Multidevice_IsLeft())
        {
            if (BtDevice_IsMyAddressPrimary())
                return gaia_debug_device_type_earbud_left_primary;
            else
                return gaia_debug_device_type_earbud_left_secondary;
        }
        else
        {
            if (BtDevice_IsMyAddressPrimary())
                return gaia_debug_device_type_earbud_right_primary;
            else
                return gaia_debug_device_type_earbud_right_secondary;
        }
    }
    else
        return gaia_debug_device_type_headset;
}


/******************************************************************************
 * Static functions
 ******************************************************************************/
/*! \brief Save the context data that are required when send a response back to
           the mobile app.
    \note  This runs only when the device is the Primary role. */
static void gaiaDebugPlugin_PydbgRoutingSaveContext(const pydbg_remote_debug_pdu_info_t *pdu_info)
{
    if (saved_context.in_use)
    {
        /* In normal use cases, this would not happen.
         * But in case that there is no response from the Secondary,
         * the Primary should allow another new command from the mobile app. */
        DEBUG_LOG_WARN("GaiaDebugPlugin PydbgRoutingSaveContext: WARNING! Context already in use! (pydbg_seq_no, Saved:0x%04X <--> 0x%04X:New)",
                        saved_context.pdu_info.pydbg_seq_no, pdu_info->pydbg_seq_no);
        DEBUG_LOG_DEBUG("GaiaDebugPlugin PydbgRoutingSaveContext: Context DISCARDED! (pydbg_seq_no:0x%04X)", saved_context.pdu_info.pydbg_seq_no);
        DEBUG_LOG_V_VERBOSE("GaiaDebugPlugin PydbgRoutingSaveContext:  - gaia_transport:  %p", saved_context.pdu_info.gaia_transport);
        DEBUG_LOG_V_VERBOSE("GaiaDebugPlugin PydbgRoutingSaveContext:  - gaia_client_id:  0x%02X", saved_context.pdu_info.gaia_client_id);
        DEBUG_LOG_V_VERBOSE("GaiaDebugPlugin PydbgRoutingSaveContext:  - gaia_tag:        0x%02X", saved_context.pdu_info.gaia_tag);
        DEBUG_LOG_V_VERBOSE("GaiaDebugPlugin PydbgRoutingSaveContext:  - pdu_type:        0x%02X", saved_context.pdu_info.pdu_type);
        DEBUG_LOG_V_VERBOSE("GaiaDebugPlugin PydbgRoutingSaveContext:  - cmd_id:          0x%02X", saved_context.pdu_info.cmd_id);
        DEBUG_LOG_DEBUG("GaiaDebugPlugin PydbgRoutingSaveContext:  - pydbg_seq_no:    0x%04X", saved_context.pdu_info.pydbg_seq_no);
    }

    {
        saved_context.pdu_info = *pdu_info;
        saved_context.in_use = TRUE;

        DEBUG_LOG_DEBUG("GaiaDebugPlugin PydbgRoutingSaveContext: Context SAVED (pydbg_seq_no:0x%04X)", saved_context.pdu_info.pydbg_seq_no);
        DEBUG_LOG_V_VERBOSE("GaiaDebugPlugin PydbgRoutingSaveContext:  - gaia_transport:  %p", saved_context.pdu_info.gaia_transport);
        DEBUG_LOG_V_VERBOSE("GaiaDebugPlugin PydbgRoutingSaveContext:  - gaia_client_id:  0x%02X", saved_context.pdu_info.gaia_client_id);
        DEBUG_LOG_V_VERBOSE("GaiaDebugPlugin PydbgRoutingSaveContext:  - gaia_tag:        0x%02X", saved_context.pdu_info.gaia_tag);
        DEBUG_LOG_V_VERBOSE("GaiaDebugPlugin PydbgRoutingSaveContext:  - pdu_type:        0x%02X", saved_context.pdu_info.pdu_type);
        DEBUG_LOG_V_VERBOSE("GaiaDebugPlugin PydbgRoutingSaveContext:  - cmd_id:          0x%02X", saved_context.pdu_info.cmd_id);
        DEBUG_LOG_DEBUG("GaiaDebugPlugin PydbgRoutingSaveContext:  - pydbg_seq_no:    0x%04X", saved_context.pdu_info.pydbg_seq_no);
#if !defined(GAIA_DEBUG_DEBUG_LOG_REQ_ROUTING)
        DEBUG_LOG_DEBUG("GaiaDebugPlugin PydbgRoutingSaveContext:  - routed_req_to:   0x%04X", saved_context.pdu_info.routed_req_to);
#else
        GAIA_DEBUG_DEBUG_LOG_REQ_ROUTING(saved_context.pdu_info.routed_req_to, DEBUG);
#endif

        if (saved_context.pdu_info.pdu_type == REMOTE_DEBUG_CMD_TYPE_ROUTED_CMD)
        {
            DEBUG_LOG_DEBUG("GaiaDebugPlugin PydbgRoutingSaveContext:  - routed_cmd_id:   0x%02X", saved_context.pdu_info.routed_cmd_id);
            DEBUG_LOG_DEBUG("GaiaDebugPlugin PydbgRoutingSaveContext:  - routed_req_to:   0x%04X", saved_context.pdu_info.routed_req_to);
         /* DEBUG_LOG_DEBUG("GaiaDebugPlugin PydbgRoutingSaveContext:  - routed_rsp_from: 0x%04X", saved_context.pdu_info.routed_rsp_from); */
            DEBUG_LOG_DEBUG("GaiaDebugPlugin PydbgRoutingSaveContext:  - routed_pdu_type: 0x%02X", saved_context.pdu_info.routed_pdu_type);
        }
    }
}


/*! \brief Load the context data that contains protocol header info required to
           constract a response PDU to the mobile app.
    \note  This runs only when the device is the Primary role. */
static bool gaiaDebugPlugin_PydbgRoutingLoadContext(pydbg_remote_debug_pdu_info_t *pdu_info)
{
    if (saved_context.in_use)
    {
        *pdu_info = saved_context.pdu_info;
        saved_context.in_use = FALSE;

        DEBUG_LOG_DEBUG("GaiaDebugPlugin PydbgRoutingLoadContext: Context LOADED (pydbg_seq_no:0x%04X)", pdu_info->pydbg_seq_no);
        DEBUG_LOG_V_VERBOSE("GaiaDebugPlugin PydbgRoutingLoadContext:  - gaia_transport:  %p", pdu_info->gaia_transport);
        DEBUG_LOG_V_VERBOSE("GaiaDebugPlugin PydbgRoutingLoadContext:  - gaia_client_id:  0x%02X", pdu_info->gaia_client_id);
        DEBUG_LOG_V_VERBOSE("GaiaDebugPlugin PydbgRoutingLoadContext:  - gaia_tag:        0x%02X", pdu_info->gaia_tag);
        DEBUG_LOG_V_VERBOSE("GaiaDebugPlugin PydbgRoutingLoadContext:  - pdu_type:        0x%02X", pdu_info->pdu_type);
        DEBUG_LOG_V_VERBOSE("GaiaDebugPlugin PydbgRoutingLoadContext:  - cmd_id:          0x%02X", pdu_info->cmd_id);
        DEBUG_LOG_DEBUG("GaiaDebugPlugin PydbgRoutingLoadContext:  - pydbg_seq_no:    0x%04X", pdu_info->pydbg_seq_no);
#if !defined(GAIA_DEBUG_DEBUG_LOG_REQ_ROUTING)
        DEBUG_LOG_DEBUG("GaiaDebugPlugin PydbgRoutingLoadContext:  - routed_req_to:   0x%04X", pdu_info->routed_req_to);
#else
        GAIA_DEBUG_DEBUG_LOG_REQ_ROUTING(pdu_info->routed_req_to, DEBUG);
#endif

        if (pdu_info->pdu_type == REMOTE_DEBUG_CMD_TYPE_ROUTED_CMD)
        {
            DEBUG_LOG_DEBUG("GaiaDebugPlugin PydbgRoutingLoadContext:  - routed_cmd_id:   0x%02X", pdu_info->routed_cmd_id);
            DEBUG_LOG_DEBUG("GaiaDebugPlugin PydbgRoutingLoadContext:  - routed_req_to:   0x%04X", pdu_info->routed_req_to);
         /* DEBUG_LOG_DEBUG("GaiaDebugPlugin PydbgRoutingLoadContext:  - routed_rsp_from: 0x%04X", pdu_info->routed_rsp_from); */
            DEBUG_LOG_DEBUG("GaiaDebugPlugin PydbgRoutingLoadContext:  - routed_pdu_type: 0x%02X", pdu_info->routed_pdu_type);
        }
        return TRUE;
    }
    else
    {
        DEBUG_LOG_WARN("GaiaDebugPlugin PydbgRoutingLoadContext: WARNING! No saved context exist!");
        return FALSE;
    }
}


/*! \brief Send a response back to the mobile app.
    \note  This runs only when the device is the Primary role. */
static void gaiaDebugPlugin_PydbgRoutingSendTunnelPduResponse(pydbg_remote_debug_pdu_info_t *pdu_info, allocated_pydbg_rsp_pdu_t *rsp_pdu, uint8 rsp_cmd_id, uint16 payload_size)
{
    /* Note that this functions runs only in the Primary device.
     * The Tunnelling Protocol Header is added only on the Primary right before
     * sending a response to the mobile app. */

    switch (pdu_info->pdu_type)
    {
        case REMOTE_DEBUG_CMD_TYPE_DEBUG_CMD:
            /* PyDbg Remote Debug Command Format (Type = 1:IP Protocol 'Debug' Type)
             *      0        1        2        3        4        5        6        7        8       ...       N    (Byte)
             *  +--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+
             *  |ClientID|   Tag  | Type=1 | Cmd ID |  Payload Length |  Tag (Seq No.)  |     Payload (if any)     |
             *  +--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+
             *  |<-  Tunnelling ->|<---- PyDbg IP Protocol (Type:'Debug') Header ------>|<---- PyDbg Payload  ---->|
             *                             |<------------------------- cmd_rsp_message --------------------------->|
             *                                                                          |<---- (payload_size) ---->|
             */
            rsp_pdu->pdu[0] = pdu_info->gaia_client_id;
            rsp_pdu->pdu[1] = pdu_info->gaia_tag;
            rsp_pdu->pdu[2] = pdu_info->pdu_type;
            {
                REMOTE_DEBUG_DEBUG_CMD_PAYLOAD *debug_rsp_header = (REMOTE_DEBUG_DEBUG_CMD_PAYLOAD*) rsp_pdu->cmd_rsp_message;

                REMOTE_DEBUG_DEBUG_CMD_PAYLOAD_PACK(debug_rsp_header, rsp_cmd_id, payload_size, pdu_info->pydbg_seq_no);
            }
            DEBUG_LOG_V_VERBOSE("GaiaDebugPlugin PydbgRoutingSendTunnelPduResponse: %02X %02X Type:%02X Cmd:%02X Len:%04X Seq:%04X", \
                                rsp_pdu->pdu[0], rsp_pdu->pdu[1], rsp_pdu->pdu[2], rsp_pdu->pdu[3], payload_size, pdu_info->pydbg_seq_no);
            GAIA_DEBUG_DEBUG_LOG_FORMATTED_ARRAY(rsp_pdu->pdu_size, rsp_pdu->pdu, V_VERBOSE);

            GaiaFramework_SendResponse(pdu_info->gaia_transport, GAIA_DEBUG_FEATURE_ID, debug_tunnel_to_chip, rsp_pdu->pdu_size, rsp_pdu->pdu);
            break;

        case REMOTE_DEBUG_CMD_TYPE_ROUTED_CMD:
            /* PyDbg Remote Debug Command Format (Type=2:IP Protocol 'Routed' Type, CmdId=1:Routed Response)
             *      0        1        2        3        4        5        6        7        8        9        10       11       12      ...       N    (Byte)
             *  +--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+
             *  |ClientID|   Tag  | Type=2 | Cmd ID |  Payload Length |  Tag (Seq No.)  | Response Routing| RtType | RtCmdID|     Payload (if any)     |
             *  +--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+
             *  |<-  Tunnelling ->|<---- PyDbg IP Protocol (Type:'Routed') Header ------>|<--------------------- PyDbg Payload  ---------------------->|
             *  |<---------------------------------------------- header_size ---------------------------------------------->|<------- payload -------->|
             *                             |<------------------------------------------- cmd_rsp_message --------------------------------------------->|
             *                                                                          |<------------------- routed_rsp_payload --------------------->|
             */
            rsp_pdu->pdu[0] = pdu_info->gaia_client_id;
            rsp_pdu->pdu[1] = pdu_info->gaia_tag;
            rsp_pdu->pdu[2] = pdu_info->pdu_type;
            {
                REMOTE_DEBUG_ROUTED_CMD_PAYLOAD *routed_rsp_header = (REMOTE_DEBUG_ROUTED_CMD_PAYLOAD*) rsp_pdu->cmd_rsp_message;

                /* Note that 'payload_size' given to this function assumes (Type = 1:IP Protocol 'Debug' Type) PDU
                 * format, but the response PDU must be a routed one (Type = 2: IP Protocol 'Routed' Type).
                 * So, the PDU size needs to be adjusted for the routed PDU. */
                payload_size += REMOTE_DEBUG_ROUTED_RESP_T_PAYLOAD_BYTE_OFFSET;     /* Add the size for 'Req/Rsp Routing' + 'RtType' + 'RtCmdID' */
                REMOTE_DEBUG_ROUTED_CMD_PAYLOAD_PACK(routed_rsp_header, REMOTE_DEBUG_ROUTE_CMD_ROUTED_RESPONSE, payload_size, pdu_info->pydbg_seq_no);
            }
            {
                REMOTE_DEBUG_ROUTED_RESP_T *routed_rsp_payload = (REMOTE_DEBUG_ROUTED_RESP_T*) &rsp_pdu->cmd_rsp_message[REMOTE_DEBUG_ROUTED_CMD_PAYLOAD_PAYLOAD_BYTE_OFFSET];
                REMOTE_DEBUG_ROUTED_RESP_ROUTE_T response_routing = pdu_info->routed_rsp_from;

                REMOTE_DEBUG_ROUTED_RESP_T_PACK(routed_rsp_payload, response_routing, pdu_info->routed_pdu_type, rsp_cmd_id);
                DEBUG_LOG_V_VERBOSE("GaiaDebugPlugin PydbgRoutingSendTunnelPduResponse:  %02X %02X Type:%02X Cmd:%02X Len:%04X Seq:%04X Route:%04X RType:%02X RCmd:%02X", \
                                    rsp_pdu->pdu[0], rsp_pdu->pdu[1], rsp_pdu->pdu[2], rsp_pdu->pdu[3], payload_size, pdu_info->pydbg_seq_no, response_routing, rsp_pdu->pdu[10], rsp_pdu->pdu[11]);
                DEBUG_LOG_V_VERBOSE("  %02X %02X | %02X %02X %02X_%02X %02X_%02X",\
                                    rsp_pdu->pdu[0], rsp_pdu->pdu[1], rsp_pdu->pdu[2], rsp_pdu->pdu[3], \
                                    rsp_pdu->pdu[4], rsp_pdu->pdu[5], rsp_pdu->pdu[6], rsp_pdu->pdu[7]);
                DEBUG_LOG_V_VERBOSE("  %02X_%02X %02X %02X", \
                                    rsp_pdu->pdu[8], rsp_pdu->pdu[9], rsp_pdu->pdu[10], rsp_pdu->pdu[11]);
                DEBUG_LOG_V_VERBOSE("  rsp_pdu->pdu_size: %d", rsp_pdu->pdu_size);
            }

            GAIA_DEBUG_DEBUG_LOG_FORMATTED_ARRAY(rsp_pdu->pdu_size, rsp_pdu->pdu, V_VERBOSE);
            GaiaFramework_SendResponse(pdu_info->gaia_transport, GAIA_DEBUG_FEATURE_ID, debug_tunnel_to_chip, rsp_pdu->pdu_size, rsp_pdu->pdu);
            break;
        
        default:
            DEBUG_LOG_ERROR("GaiaDebugPlugin PydbgRoutingSendTunnelPduResponse: ERROR! Invalid Pydbg PDU Type:%d", pdu_info->pdu_type);
            Panic();
            break;
    }
}


/*! \brief Send a unroutable response back to the mobile app.
    \note  This runs only when the device is the Primary role. */
static void gaiaDebugPlugin_PydbgRoutingSendUnroutableTunnelPduResponse(uint8 reason)
{
    /* PyDbg Remote Debug Command Format (Type=2:IP Protocol 'Routed' Type, CmdId=2:Unroutable Response)
     *      0        1        2        3        4        5        6        7        8        9        10       11       12       13       14   (Byte)
     *  +--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+
     *  |ClientID|   Tag  | Type=2 | Cmd ID |  Payload Length |  Tag (Seq No.)  | Request Routing | RtType | RtCmdID| Response Routing| Reason |
     *  +--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+
     *  |<-  Tunnelling ->|<-----  PyDbg Remote Debug Protocol Header (Type = 2 : IP Protocol 'Routed' Type)  ----->|<---- PyDbg Payload  ---->|
     *  |<---------------------------------------------- header_size ---------------------------------------------->|<------- payload -------->|
     *  |                          |<------------------------------------------- cmd_rsp_message --------------------------------------------->|
     *  |                                                                       |<-------------- REMOTE_DEBUG_UNROUTABLE_RESP_T -------------->|
     *  |<----------------------------------------------------------- rsp_pdu->pdu ----------------------------------------------------------->|
     */
    const uint16 tunnelling_header_size = (GAIA_DEBUG_TUNNEL_TO_CHIP_CMD_RSP_PARAMETER_HEADER_SIZE + PYDBG_REMOTE_DEBUG_PDU_ROUTED_TYPE_HEADER_SIZE);
    const uint16 pydbg_payload_size     = (REMOTE_DEBUG_UNROUTABLE_RESP_T_PAYLOAD_BYTE_OFFSET - REMOTE_DEBUG_ROUTED_RESP_T_PAYLOAD_BYTE_OFFSET);
    const uint16 payload_length         = REMOTE_DEBUG_UNROUTABLE_RESP_T_PAYLOAD_BYTE_OFFSET;   /* The size for 'Request Routing' ~ 'Reason'. */
    pydbg_remote_debug_pdu_info_t    pdu_info;
    allocated_pydbg_rsp_pdu_t        rsp_pdu;
    REMOTE_DEBUG_ROUTED_CMD_PAYLOAD *routed_rsp_header;
    REMOTE_DEBUG_UNROUTABLE_RESP_T  *rsp;

    GaiaDebugPlugin_PydbgRoutingMallocRspPDU(&rsp_pdu, REMOTE_DEBUG_CMD_TYPE_ROUTED_CMD, pydbg_payload_size);

    gaiaDebugPlugin_PydbgRoutingLoadContext(&pdu_info);

    rsp_pdu.pdu[0] = pdu_info.gaia_client_id;
    rsp_pdu.pdu[1] = pdu_info.gaia_tag;
    rsp_pdu.pdu[2] = pdu_info.pdu_type;
    routed_rsp_header = (REMOTE_DEBUG_ROUTED_CMD_PAYLOAD*) rsp_pdu.cmd_rsp_message;
    REMOTE_DEBUG_ROUTED_CMD_PAYLOAD_PACK(routed_rsp_header, REMOTE_DEBUG_ROUTE_CMD_UNROUTABLE_RESPONSE, payload_length, pdu_info.pydbg_seq_no);

    /* Set the Unroutable Response fields. */
    rsp = (REMOTE_DEBUG_UNROUTABLE_RESP_T*) &rsp_pdu.pdu[tunnelling_header_size];
    REMOTE_DEBUG_UNROUTABLE_RESP_T_REQUEST_ROUTING_SET(rsp, pdu_info.routed_req_to);
    REMOTE_DEBUG_UNROUTABLE_RESP_T_ROUTED_TYPE_SET(rsp, pdu_info.routed_pdu_type);
    REMOTE_DEBUG_UNROUTABLE_RESP_T_ROUTED_CMD_ID_SET(rsp, pdu_info.routed_cmd_id);
    if (Multidevice_IsPair())
    {
        if (Multidevice_IsLeft())
            pdu_info.routed_rsp_from = REMOTE_DEBUG_ROUTED_RESP_ROUTE_T_LEFT_PRIMARY;
        else
            pdu_info.routed_rsp_from = REMOTE_DEBUG_ROUTED_RESP_ROUTE_T_RIGHT_PRIMARY;
    }
    else
        pdu_info.routed_rsp_from = REMOTE_DEBUG_ROUTED_RESP_ROUTE_T_SECONDARY_NOT_SUPPORTED;
    REMOTE_DEBUG_UNROUTABLE_RESP_T_RESPONSE_ROUTING_SET(rsp, pdu_info.routed_rsp_from);
    REMOTE_DEBUG_UNROUTABLE_RESP_T_ROUTED_REASON_SET(rsp, reason);

    DEBUG_LOG_WARN("GaiaDebugPlugin PydbgRoutingSendUnroutableTunnelPduResponse:  UNROUTABLE Rsp sent to the mobile app! (reason:%d)", reason);
    GAIA_DEBUG_DEBUG_LOG_FORMATTED_ARRAY(rsp_pdu.pdu_size, rsp_pdu.pdu, V_VERBOSE);

    GaiaFramework_SendResponse(pdu_info.gaia_transport, GAIA_DEBUG_FEATURE_ID, debug_tunnel_to_chip, rsp_pdu.pdu_size, rsp_pdu.pdu);
    free(rsp_pdu.pdu);
}


/*! \brief Send a (normal/error) response the Primary device.
    \note  This runs only when the device is the Secondary role. */
#ifdef INCLUDE_L2CAP_MANAGER
static void gaiaDebugPlugin_PydbgRoutingSendRspToPrimary(gaia_debug_l2cap_peer_link_message_command_t peer_link_cmd, uint16 payload_length, const uint8 *payload)
{
    gaia_debug_l2cap_peer_link_send_status_t result;

    DEBUG_LOG_DEBUG("GaiaDebugPlugin PydbgRoutingSendRspToPrimary: (Cmd:%d, Len:%d)", peer_link_cmd, payload_length);

    result = GaiaDebugPlugin_L2capPeerLinkSend(peer_link_cmd, payload_length, payload);
    if (result == gaia_debug_l2cap_peer_link_send_status_rejected_due_to_ongoing_handover)
    {
        DEBUG_LOG_DEBUG("GaiaDebugPlugin PydbgRoutingSendRspToPrimary: Failed to send to the peer! Due to ongoing HANDOVER.");

        /* If the Secondary cannot send a response to the Primary,
         * because of an ongoing handover, then just discard the response.
         * The Primary does not wait for the response from the Secondary
         * and it sends 'Unroutable Response' to the mobile app. */
    }
}
#endif /* INCLUDE_L2CAP_MANAGER */


/*! \brief Parse PyDbg IP transport protocol messages sent over the
           'DebugTunnelToChip' command and routes the request.

    \param t                Pointer to transport instance.

    \param gaia_client_id   An identifier assigned and used by the host (the mobile app).

    \param gaia_tag         Another identifier used by the host (the mobile app).

    \param pydbg_pdu_length Payload size of the PyDbg PDU (Header + Payload) in bytes.

    \param pydbg_pdu        PyDbg PDU data.

    \note  This runs only when the device is the Primary role.
*/
static void gaiaDebugPlugin_PydbgRoutingHandler(GAIA_TRANSPORT *t, uint8 gaia_client_id, uint8 gaia_tag, uint16 pydbg_pdu_length, const uint8 *pydbg_pdu)
{
    pydbg_remote_debug_pdu_info_t pydbg_pdu_info;
    REMOTE_DEBUG_CMD_TYPE pdu_type = pydbg_pdu[0];
    const uint8 *payload = NULL;

    pydbg_pdu_info.gaia_transport = t;
    pydbg_pdu_info.gaia_client_id = gaia_client_id;
    pydbg_pdu_info.gaia_tag       = gaia_tag;
    pydbg_pdu_info.pdu_type       = pdu_type;

    switch (pdu_type)
    {
        case REMOTE_DEBUG_CMD_TYPE_DEBUG_CMD:
        {
            /* PyDbg Remote Debug Command Format (Type = 1:IP Protocol 'Debug' Type)
            *      0        1        2        3        4        5        6       ...       N    (Byte)
            *  +--------+--------+--------+--------+--------+--------+--------+--------+--------+
            *  | Type=1 | Cmd ID |  Payload Length |  Tag (Seq No.)  |     Payload (if any)     |
            *  +--------+--------+--------+--------+--------+--------+--------+--------+--------+
            *  |<---- PyDbg IP Protocol (Type:'Debug') Header ------>|<---- PyDbg Payload  ---->|
            */
            REMOTE_DEBUG_DEBUG_CMD_PAYLOAD *cmd_payload = (REMOTE_DEBUG_DEBUG_CMD_PAYLOAD*) &pydbg_pdu[PYDBG_REMOTE_DEBUG_IP_PROTOCOL_TYPE_FIELD_SIZE];
            uint16 payload_length = REMOTE_DEBUG_DEBUG_CMD_PAYLOAD_PAYLOAD_LENGTH_GET(cmd_payload);

            if (pydbg_pdu_length != (PYDBG_REMOTE_DEBUG_PDU_DEBUG_TYPE_HEADER_SIZE + payload_length))
            {
                DEBUG_LOG_WARN("GaiaDebugPlugin PydbgRoutingHandler(Dbg): ERROR! Invalid PyDbg PDU length:(%d + %d) != (Received:%d)", payload_length, PYDBG_REMOTE_DEBUG_PDU_DEBUG_TYPE_HEADER_SIZE, pydbg_pdu_length);
                GaiaFramework_SendError(t, GAIA_DEBUG_FEATURE_ID, debug_tunnel_to_chip, debug_plugin_status_invalid_parameters);
                return;
            }

            pydbg_pdu_info.cmd_id       = REMOTE_DEBUG_DEBUG_CMD_PAYLOAD_DEBUG_COMMAND_GET(cmd_payload);
            pydbg_pdu_info.pydbg_seq_no = REMOTE_DEBUG_DEBUG_CMD_PAYLOAD_TAG_GET(cmd_payload);

            if (0 < payload_length)
                payload = &pydbg_pdu[PYDBG_REMOTE_DEBUG_PDU_DEBUG_TYPE_HEADER_SIZE];

            DEBUG_LOG_V_VERBOSE("GaiaDebugPlugin PydbgRoutingHandler(Dbg):  %02X %02X Type:%02X Cmd:%02X Len:%04X Seq:%04X", \
                                gaia_client_id, gaia_tag, pdu_type, pydbg_pdu_info.cmd_id, payload_length, pydbg_pdu_info.pydbg_seq_no);
            gaiaDebugPlugin_PydbgRoutingSaveContext(&pydbg_pdu_info);
            GaiaDebugPlugin_PyDbgDebugCommandHandler(pdu_type, pydbg_pdu_info.cmd_id, payload_length, payload);
            break;
        }

        case REMOTE_DEBUG_CMD_TYPE_ROUTED_CMD:
        {
            /* PyDbg Remote Debug Command Format (Type=2:IP Protocol 'Routed' Type, CmdId=0:Routed Request)
             *      0        1        2        3        4        5        6        7        8        9        10      ...       N    (Byte)
             *  +--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+
             *  | Type=2 | Cmd ID |  Payload Length |  Tag (Seq No.)  | Request Routing | RtType | RtCmdID|     Payload (if any)     |
             *  +--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+
             *  |<---- PyDbg IP Protocol (Type:'Routed') Header ----->|<------------- PyDbg Payload ('payload_length')  ------------>|
             *  |<-----  PyDbg Remote Debug Protocol Header (Type = 2 : IP Protocol 'Routed' Type)  ----->|<----- RtCmd Payload ---->|
             *           |<-------------------------------------------- cmd_payload ------------------------------------------------>|
             *
             *       .pdu_type        = 'Type':             pydbg_pdu[0]
             *       .cmd_id          = 'Cmd ID':           pydbg_pdu[1]        (This must be 1 = 'Routed Request')
             *       .routed_req_to   = 'Req/Rsp Routing':  pydbg_pdu[6-7]  
             *       .routed_pdu_type = 'RtType':           pydbg_pdu[8]
             *       .routed_cmd_id   = 'RtCmdID':          pydbg_pdu[9]
             */
            REMOTE_DEBUG_ROUTED_CMD_PAYLOAD *cmd_payload = (REMOTE_DEBUG_ROUTED_CMD_PAYLOAD*) &pydbg_pdu[PYDBG_REMOTE_DEBUG_IP_PROTOCOL_TYPE_FIELD_SIZE];
            uint16 payload_length = REMOTE_DEBUG_ROUTED_CMD_PAYLOAD_PAYLOAD_LENGTH_GET(cmd_payload);

            if (pydbg_pdu_length != (PYDBG_REMOTE_DEBUG_PDU_ROUTED_TYPE_HEADER_SIZE + payload_length))
            {
                DEBUG_LOG_WARN("GaiaDebugPlugin PydbgRoutingHandler(Rt): ERROR! Invalid PyDbg PDU length:(%d + %d) != (Received:%d)", payload_length, PYDBG_REMOTE_DEBUG_PDU_ROUTED_TYPE_HEADER_SIZE, pydbg_pdu_length);
                GaiaFramework_SendError(t, GAIA_DEBUG_FEATURE_ID, debug_tunnel_to_chip, debug_plugin_status_invalid_parameters);
                return;
            }

            pydbg_pdu_info.cmd_id       = REMOTE_DEBUG_ROUTED_CMD_PAYLOAD_ROUTE_COMMAND_GET(cmd_payload);
            pydbg_pdu_info.pydbg_seq_no = REMOTE_DEBUG_ROUTED_CMD_PAYLOAD_TAG_GET(cmd_payload);

            if (0 < payload_length)
                payload = &pydbg_pdu[PYDBG_REMOTE_DEBUG_PDU_ROUTED_TYPE_HEADER_SIZE];

            DEBUG_LOG_V_VERBOSE("GaiaDebugPlugin PydbgRoutingHandler(Rt):  %02X %02X Type:%02X Cmd:%02X Len:%04X Seq:%04X", \
                                gaia_client_id, gaia_tag, pdu_type, pydbg_pdu_info.cmd_id, payload_length, pydbg_pdu_info.pydbg_seq_no);
            gaiaDebugPlugin_PydbgRoutedCommandHandler(&pydbg_pdu_info, pydbg_pdu_info.cmd_id, payload_length, payload);
            break;
        }
        
        default:
            /* The chip code does not expect other than the 'Debug Type' or 'Routed Type'.
            * (NB: 'REMOTE_DEBUG_CMD_TYPE_TRANSPORT_CMD' is expected to be used between the mobile app and PyDbg running on PC.) */
            DEBUG_LOG_WARN("GaiaDebugPlugin PydbgRoutingHandler(N/A): ERROR! Invalid PyDbg PDU Type:%d", pdu_type);
            GaiaFramework_SendError(t, GAIA_DEBUG_FEATURE_ID, debug_tunnel_to_chip, debug_plugin_status_invalid_parameters);
            break;
    }
}


/*! \brief Handle PyDbg 'Routed-Type' command and execute the request.

    \param pdu_info         GAIA Transport and PDU header information.

    \param cmd_id           Pydbg Remote Debug 'Routed-Type' command ID.

    \param payload_length   Payload size of the PyDbg command payload (in bytes).

    \param payload          PyDbg command payload data.

    The expected payload format looks like this:
        PyDbg Remote Debug Command Format (Type = 2:IP Protocol 'Routed' Type)
             0        1        2        3        4       ...       N    (Byte)
         +--------+--------+--------+--------+--------+--------+--------+
         | Request Routing | RtType | RtCmdID|     Payload (if any)     |
         +--------+--------+--------+--------+--------+--------+--------+
         |<----- 'Routed' Type Header  ----->|<----- RtCmd Payload ---->|

    \note  This runs only when the device is the Primary role.
*/
static void gaiaDebugPlugin_PydbgRoutedCommandHandler(pydbg_remote_debug_pdu_info_t *pdu_info, REMOTE_DEBUG_ROUTE_CMD cmd_id, uint16 payload_length, const uint8 *payload)
{
    switch (cmd_id)
    {
        case REMOTE_DEBUG_ROUTE_CMD_ROUTED_REQUEST:
        {
            REMOTE_DEBUG_ROUTED_REQ_T *routed_req_payload = (REMOTE_DEBUG_ROUTED_REQ_T*) payload;

            if (payload_length < REMOTE_DEBUG_ROUTED_REQ_T_PAYLOAD_BYTE_OFFSET)
            {
                DEBUG_LOG_WARN("GaiaDebugPlugin PydbgRoutedCommandHandler: ERROR! Routed Req payload is too small (%d:less than 4)", payload_length);
                GaiaFramework_SendError(pdu_info->gaia_transport, GAIA_DEBUG_FEATURE_ID, debug_tunnel_to_chip, debug_plugin_status_invalid_parameters);
                return;
            }

            pdu_info->routed_pdu_type = REMOTE_DEBUG_ROUTED_REQ_T_ROUTED_TYPE_GET(routed_req_payload);
            switch (pdu_info->routed_pdu_type)
            {
                case REMOTE_DEBUG_CMD_TYPE_DEBUG_CMD:
                {
                    pdu_info->routed_req_to = REMOTE_DEBUG_ROUTED_REQ_T_REQUEST_ROUTING_GET(routed_req_payload);
                    pdu_info->routed_cmd_id = REMOTE_DEBUG_ROUTED_REQ_T_ROUTED_CMD_ID_GET(routed_req_payload);
                    DEBUG_LOG_DEBUG("GaiaDebugPlugin PydbgRoutedCommandHandler:  - routed_cmd_id:   0x%02X", pdu_info->routed_cmd_id);
                    DEBUG_LOG_DEBUG("GaiaDebugPlugin PydbgRoutedCommandHandler:  - routed_pdu_type: 0x%02X", pdu_info->routed_pdu_type);
#if !defined(GAIA_DEBUG_DEBUG_LOG_REQ_ROUTING)
                    DEBUG_LOG_DEBUG("GaiaDebugPlugin PydbgRoutedCommandHandler:  - routed_req_to:   0x%04X", pdu_info->routed_req_to);
#else
                    GAIA_DEBUG_DEBUG_LOG_REQ_ROUTING(pdu_info->routed_req_to, DEBUG);
#endif
                    gaiaDebugPlugin_PydbgRoutingSaveContext(pdu_info);

                    if (gaiaDebugPlugin_IsThisTheDestinedDevice(pdu_info->routed_req_to))
                    {
                        /* No need to forward the request to the Secondary Earbud. */
                        const uint8 *dbg_cmd_payload = NULL;
                        uint16 dbg_cmd_payload_size = payload_length - REMOTE_DEBUG_ROUTED_REQ_T_PAYLOAD_BYTE_OFFSET;

                        if (0 < dbg_cmd_payload_size)
                        {
                            dbg_cmd_payload = &payload[REMOTE_DEBUG_ROUTED_REQ_T_PAYLOAD_BYTE_OFFSET];
                        }
                        DEBUG_LOG_V_VERBOSE("GaiaDebugPlugin PydbgRoutedCommandHandler:    Route:%04X R-Type:%02X R-Cmd:%02X", \
                                            pdu_info->routed_req_to, pdu_info->routed_pdu_type, pdu_info->routed_cmd_id);

                        GaiaDebugPlugin_PyDbgDebugCommandHandler(pdu_info->pdu_type, pdu_info->routed_cmd_id, dbg_cmd_payload_size, dbg_cmd_payload);
                    }
                    else
#ifdef INCLUDE_L2CAP_MANAGER
                    {
                        DEBUG_LOG_DEBUG("GaiaDebugPlugin PydbgRoutedCommandHandler: Size:%d", payload_length);
                        GAIA_DEBUG_DEBUG_LOG_FORMATTED_ARRAY(payload_length, payload, V_VERBOSE);
                        
                        /* The Primary device is sending a Pydbg command to the Secondary. */
                        gaia_debug_l2cap_peer_link_send_status_t result = GaiaDebugPlugin_L2capPeerLinkSend(link_message_command_req, payload_length, payload);

                        switch (result)
                        {
                            case gaia_debug_l2cap_peer_link_send_status_success:
                                /* Fall through */
                            case gaia_debug_l2cap_peer_link_send_status_pending:
                                /*
                                 * Do nothing here.
                                 * If the Primary fails to connect to the Secondary or the existing link is lost,
                                 * the Primary will receive CONNECT_CFM with a failure status code DISCONNECT_IND.
                                 * Respective handler functions send a 'Unroutable Response' back to the mobile app.
                                 * 
                                 * In case, the link is live and the Secondary just not send the response to a command,
                                 * the Primary justs wait for the response indefinitely. This must result in timeout
                                 * at the Pydbg running at the host.
                                 */
                                break;

                            case gaia_debug_l2cap_peer_link_send_status_rejected_due_to_ongoing_handover:
                                DEBUG_LOG_DEBUG("GaiaDebugPlugin PydbgRoutedCommandHandler: Ongoing handover: Unable to send the message!");
                                GaiaDebugPlugin_L2capPeerLinkDiscardTxBufferredData();
                                gaiaDebugPlugin_PydbgRoutingSendUnroutableTunnelPduResponse(REMOTE_DEBUG_ROUTED_REASON_HANDOVER);
                                break;

                            case gaia_debug_l2cap_peer_link_send_status_failure_peer_unreachable:
                                DEBUG_LOG_DEBUG("GaiaDebugPlugin PydbgRoutedCommandHandler: The peer device is unreachable!");
                                GaiaDebugPlugin_L2capPeerLinkDiscardTxBufferredData();
                                gaiaDebugPlugin_PydbgRoutingSendUnroutableTunnelPduResponse(REMOTE_DEBUG_ROUTED_REASON_LINK_LOST);
                                break;

                            case gaia_debug_l2cap_peer_link_send_status_not_a_pair_type_device:
                                /* If Pydbg tries to send a 'Routed' command to the Headset
                                 * application, an error response is sent back to the host. */
                                DEBUG_LOG_WARN("GaiaDebugPlugin PydbgRoutedCommandHandler: WARNING! Invalid command for non Pair-Type device!");
                                GaiaDebugPlugin_L2capPeerLinkDiscardTxBufferredData();
                                gaiaDebugPlugin_PydbgRoutingSendUnroutableTunnelPduResponse(REMOTE_DEBUG_ROUTED_REASON_NOT_SUPPORTED);
                                break;

                            default:
                                DEBUG_LOG_ERROR("GaiaDebugPlugin PydbgRoutedCommandHandler: ERROR! Invalid status code: %d", result);
                                Panic();
                                break;
                        }
                    }
#else /* INCLUDE_L2CAP_MANAGER */
                    {
                        DEBUG_LOG_WARN("GaiaDebugPlugin PydbgRoutedCommandHandler: WARNING! Headset application does not support routing commands!");
                        gaiaDebugPlugin_PydbgRoutingSendUnroutableTunnelPduResponse(REMOTE_DEBUG_ROUTED_REASON_NOT_SUPPORTED);
                    }
#endif /* INCLUDE_L2CAP_MANAGER */
                    break;
                }

                default:
                    DEBUG_LOG_WARN("GaiaDebugPlugin PydbgRoutedCommandHandler: ERROR! Not supported Routed PDU-Type:%d", pdu_info->routed_pdu_type);
                    GaiaFramework_SendError(pdu_info->gaia_transport, GAIA_DEBUG_FEATURE_ID, debug_tunnel_to_chip, debug_plugin_status_invalid_parameters);
                    return;
            }
            break;
        }

        default: 
            DEBUG_LOG_WARN("GaiaDebugPlugin PydbgRoutedCommandHandler: ERROR! CmdID must be 'Routed Req' but %d", cmd_id);
            GaiaFramework_SendError(pdu_info->gaia_transport, GAIA_DEBUG_FEATURE_ID, debug_tunnel_to_chip, debug_plugin_status_invalid_parameters);
            return;
    }
}


#ifdef PRETEND_AS_IF_THE_RESPONSE_COMES_FROM_THE_REQUESTED_DEVICE_BY_PYDBG
/*! \brief Check if this device is the destination of a Pydbg request is sent or not.

    \param req_to   The destination of a Pydbg Remote Debug PDU is sent to.

    \return TRUE if this device is Primary or the device specified by Left/Right earbud. Otherwise FALSE.
*/
static bool gaiaDebugPlugin_IsThisTheDestinedDevice(REMOTE_DEBUG_ROUTED_REQ_ROUTE_T req_to)
{
    UNUSED(req_to);

    return TRUE;    /* Behave as if this is the device the command is sent regardless the destination. */
}

#else /* PRETEND_AS_IF_THE_RESPONSE_COMES_FROM_THE_REQUESTED_DEVICE_BY_PYDBG */

/*! \brief Check if this device is the destination of a Pydbg request is sent or not.

    \param req_to   The destination of a Pydbg Remote Debug PDU is sent to.

    \return TRUE if this device is Primary or the device specified by Left/Right earbud. Otherwise FALSE.
*/
static bool gaiaDebugPlugin_IsThisTheDestinedDevice(REMOTE_DEBUG_ROUTED_REQ_ROUTE_T req_to)
{
    bool result = FALSE;

    if (Multidevice_IsPair())
    {
        switch (req_to)
        {
            case REMOTE_DEBUG_ROUTED_REQ_ROUTE_T_PRIMARY:
                if (BtDevice_IsMyAddressPrimary())
                    result = TRUE;
                break;
            case REMOTE_DEBUG_ROUTED_REQ_ROUTE_T_SECONDARY:
                if (BtDevice_IsMyAddressPrimary() == FALSE)
                    result = TRUE;
                break;

            case REMOTE_DEBUG_ROUTED_REQ_ROUTE_T_LEFT:
                if (Multidevice_IsLeft())
                    result = TRUE;
                break;
            case REMOTE_DEBUG_ROUTED_REQ_ROUTE_T_RIGHT:
                if (Multidevice_IsLeft() == FALSE)
                    result = TRUE;
                break;

            default:
                DEBUG_LOG_ERROR("GaiaDebugPlugin IsThisTheDestinedDevice: ERROR! Invalid param: 0x%X", req_to);
                break;
        }
    }
    else
    {
        DEBUG_LOG_DEBUG("GaiaDebugPlugin IsThisTheDestinedDevice: This is Headset.");
        result = TRUE;
    }

    DEBUG_LOG_DEBUG("GaiaDebugPlugin IsThisTheDestinedDevice: %s", result ? yes : no);

    return result;
}
#endif /* PRETEND_AS_IF_THE_RESPONSE_COMES_FROM_THE_REQUESTED_DEVICE_BY_PYDBG */


#ifdef PRETEND_AS_IF_THE_RESPONSE_COMES_FROM_THE_REQUESTED_DEVICE_BY_PYDBG
/*! \brief Return the route which the response is sent from.

    \param req_to   The destination of a Pydbg Remote Debug PDU is sent to.     [FixMe] Tentative parameter to be removed.

    \return Primary/Secondary and Left/Right defined in #REMOTE_DEBUG_ROUTED_REQ_ROUTE_T.
*/
static REMOTE_DEBUG_ROUTED_RESP_ROUTE_T gaiaDebugPlugin_GetRespRouting(REMOTE_DEBUG_ROUTED_REQ_ROUTE_T req_to)      /* FixMe: Remove the parameter! */
{
    gaia_debug_device_type_t dev_type = GaiaDebugPlugin_GetDeviceType();

    switch (req_to)
    {
        case REMOTE_DEBUG_ROUTED_REQ_ROUTE_T_PRIMARY:
            switch (dev_type)
            {
                case gaia_debug_device_type_earbud_left_primary:
                    return REMOTE_DEBUG_ROUTED_RESP_ROUTE_T_LEFT_PRIMARY;
                case gaia_debug_device_type_earbud_right_primary:
                    return REMOTE_DEBUG_ROUTED_RESP_ROUTE_T_RIGHT_PRIMARY;
                case gaia_debug_device_type_earbud_left_secondary:
                    DEBUG_LOG_WARN("GaiaDebugPlugin GetRespRouting: Sorry, but actually from PRIMARY! (L)");
                    return REMOTE_DEBUG_ROUTED_RESP_ROUTE_T_LEFT_PRIMARY;
                case gaia_debug_device_type_earbud_right_secondary:
                    DEBUG_LOG_WARN("GaiaDebugPlugin GetRespRouting: Sorry, but actually from PRIMARY! (R)");
                    return REMOTE_DEBUG_ROUTED_RESP_ROUTE_T_RIGHT_PRIMARY;
                default:
                    DEBUG_LOG_ERROR("GaiaDebugPlugin GetRespRouting: Device unknown. Might be Headset?");
            }
            return 0xFF;    /* FixMe: No code for Headset! */

        case REMOTE_DEBUG_ROUTED_REQ_ROUTE_T_SECONDARY:
            switch (dev_type)
            {
                case gaia_debug_device_type_earbud_left_primary:
                    DEBUG_LOG_WARN("GaiaDebugPlugin GetRespRouting: Sorry, but actually from PRIMARY! (L)");
                    return REMOTE_DEBUG_ROUTED_RESP_ROUTE_T_LEFT_SECONDARY;
                case gaia_debug_device_type_earbud_right_primary:
                    DEBUG_LOG_WARN("GaiaDebugPlugin GetRespRouting: Sorry, but actually from PRIMARY! (R)");
                    return REMOTE_DEBUG_ROUTED_RESP_ROUTE_T_RIGHT_SECONDARY;
                case gaia_debug_device_type_earbud_left_secondary:
                    return REMOTE_DEBUG_ROUTED_RESP_ROUTE_T_LEFT_SECONDARY;
                case gaia_debug_device_type_earbud_right_secondary:
                    return REMOTE_DEBUG_ROUTED_RESP_ROUTE_T_RIGHT_SECONDARY;
                default:
                    DEBUG_LOG_ERROR("GaiaDebugPlugin GetRespRouting: Device unknown. Might be Headset?");
            }
            return 0xFF;    /* FixMe: No code for Headset! */

        case REMOTE_DEBUG_ROUTED_REQ_ROUTE_T_LEFT:
            switch (dev_type)
            {
                case gaia_debug_device_type_earbud_left_primary:
                    return REMOTE_DEBUG_ROUTED_RESP_ROUTE_T_LEFT_PRIMARY;
                case gaia_debug_device_type_earbud_right_primary:
                    DEBUG_LOG_WARN("GaiaDebugPlugin GetRespRouting: Sorry, but actually from RIGHT! (P)");
                    return REMOTE_DEBUG_ROUTED_RESP_ROUTE_T_LEFT_PRIMARY;
                case gaia_debug_device_type_earbud_left_secondary:
                    return REMOTE_DEBUG_ROUTED_RESP_ROUTE_T_LEFT_SECONDARY;
                case gaia_debug_device_type_earbud_right_secondary:
                    DEBUG_LOG_WARN("GaiaDebugPlugin GetRespRouting: Sorry, but actually from RIGHT! (S)");
                    return REMOTE_DEBUG_ROUTED_RESP_ROUTE_T_LEFT_SECONDARY;
                default:
                    DEBUG_LOG_ERROR("GaiaDebugPlugin GetRespRouting: Device unknown. Might be Headset?");
            }
            return 0xFF;    /* FixMe: No code for Headset! */

        case REMOTE_DEBUG_ROUTED_REQ_ROUTE_T_RIGHT:
            switch (dev_type)
            {
                case gaia_debug_device_type_earbud_left_primary:
                    DEBUG_LOG_WARN("GaiaDebugPlugin GetRespRouting: Sorry, but actually from LEFT! (P)");
                    return REMOTE_DEBUG_ROUTED_RESP_ROUTE_T_RIGHT_PRIMARY;
                case gaia_debug_device_type_earbud_right_primary:
                    return REMOTE_DEBUG_ROUTED_RESP_ROUTE_T_RIGHT_PRIMARY;
                case gaia_debug_device_type_earbud_left_secondary:
                    DEBUG_LOG_WARN("GaiaDebugPlugin GetRespRouting: Sorry, but actually from LEFT! (S)");
                    return REMOTE_DEBUG_ROUTED_RESP_ROUTE_T_RIGHT_SECONDARY;
                case gaia_debug_device_type_earbud_right_secondary:
                    return REMOTE_DEBUG_ROUTED_RESP_ROUTE_T_RIGHT_SECONDARY;
                default:
                    DEBUG_LOG_ERROR("GaiaDebugPlugin GetRespRouting: Device unknown. Might be Headset?");
            }
            return 0xFF;    /* FixMe: No code for Headset! */

        default:
            DEBUG_LOG_ERROR("GaiaDebugPlugin GetRespRouting: ERROR! Invalid Device Type: %d", dev_type);
            break;
    }

    return 0xEE;  /* FixMe: No code for Headset! */
}

#else /* PRETEND_AS_IF_THE_RESPONSE_COMES_FROM_THE_REQUESTED_DEVICE_BY_PYDBG */

/*! \brief Return the route which the response is sent from.

    \return Primary/Secondary and Left/Right defined in #REMOTE_DEBUG_ROUTED_REQ_ROUTE_T.
*/
static REMOTE_DEBUG_ROUTED_RESP_ROUTE_T gaiaDebugPlugin_GetRespRouting(void)
{
    gaia_debug_device_type_t dev_type = GaiaDebugPlugin_GetDeviceType();

    switch (dev_type)
    {
        case gaia_debug_device_type_earbud_left_primary:
            return REMOTE_DEBUG_ROUTED_RESP_ROUTE_T_LEFT_PRIMARY;
        case gaia_debug_device_type_earbud_left_secondary:
            return REMOTE_DEBUG_ROUTED_RESP_ROUTE_T_LEFT_SECONDARY;
        case gaia_debug_device_type_earbud_right_primary:
            return REMOTE_DEBUG_ROUTED_RESP_ROUTE_T_RIGHT_PRIMARY;
        case gaia_debug_device_type_earbud_right_secondary:
            return REMOTE_DEBUG_ROUTED_RESP_ROUTE_T_RIGHT_SECONDARY;

        case gaia_debug_device_type_headset:
            return REMOTE_DEBUG_ROUTED_RESP_ROUTE_T_SECONDARY_NOT_SUPPORTED;
        default:
            DEBUG_LOG_ERROR("GaiaDebugPlugin GetRespRouting: ERROR! Invalid Device Type: %d", dev_type);
            Panic();
            break;
    }

    return REMOTE_DEBUG_ROUTED_RESP_ROUTE_T_INVALID;
}
#endif /* PRETEND_AS_IF_THE_RESPONSE_COMES_FROM_THE_REQUESTED_DEVICE_BY_PYDBG */

#endif /* INCLUDE_GAIA_PYDBG_REMOTE_DEBUG */

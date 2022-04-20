/*!
\copyright  Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for the gaia framework earbud plugin
*/

#ifdef INCLUDE_GAIA
#include "earbud_gaia_plugin.h"

#include <logging.h>
#include <panic.h>
#include <peer_signalling.h>
#include <device_info.h>

#include <gaia_framework_feature.h>
#include <earbud_config.h>
#include <earbud_gaia_typedef.h>
#include <earbud_gaia_marshal_typedef.h>
#include <earbud_sm.h>

#define EARBUD_GAIA_PEER_TX(msg, type) \
    appPeerSigMarshalledMsgChannelTx(&earbud_gaia_peer_signal_task, PEER_SIG_MSG_CHANNEL_GAIA, msg, MARSHAL_TYPE_##type);

static void earbudGaiaPlugin_PeerSignalHander(Task task, MessageId id, Message message);

static TaskData earbud_gaia_peer_signal_task = {earbudGaiaPlugin_PeerSignalHander};


void EarbudGaiaPlugin_PrimaryAboutToChange(uint8 delay)
{
    /* Inform the mobile app that the device is going to Primary/Secondary swap */
    gaia_transport_index index = 0;
    gaia_transport *t = Gaia_TransportIterate(&index);
    while (t)
    {
        if (Gaia_TransportIsConnected(t))
        {
            earbud_plugin_handover_types_t handover_type = Gaia_TransportHasFeature(t, GAIA_TRANSPORT_FEATURE_DYNAMIC_HANDOVER) ? dynamic_handover : static_handover;
            DEBUG_LOG_INFO("EarbudGaiaPlugin_PrimaryAboutToChange, handover_type %u, delay %u", handover_type, delay);
            if (handover_type == static_handover)
            {
                uint8 payload[2] = {handover_type, delay};
                GaiaFramework_SendNotificationWithTransport(t, GAIA_EARBUD_FEATURE_ID, primary_earbud_about_to_change, sizeof(payload), payload);
            }
        }
        t = Gaia_TransportIterate(&index);
    }
}


void EarbudGaiaPlugin_RoleChanged(tws_topology_role role)
{
    if (role == tws_topology_role_primary)
    {
        uint8 value = appConfigIsLeft() ? 0 : 1;
        GaiaFramework_SendNotification(GAIA_EARBUD_FEATURE_ID, primary_earbud_changed, sizeof(value), &value);
    }
}


static void earbudGaiaPlugin_IsPrimaryLeftOrRight(GAIA_TRANSPORT *t)
{
    /* Left earbud sets value to 0, right sets value to 1 */
    uint8 value = appConfigIsLeft() ? 0 : 1;
    DEBUG_LOG("earbudGaiaPlugin_WhichEarbudIsPrimary, %d", value);

    GaiaFramework_SendResponse(t, GAIA_EARBUD_FEATURE_ID, is_primary_left_or_right, sizeof(value), &value);
}


static void earbudGaiaPlugin_HandlePeerRequestGetSerialNumber(earbud_gaia_request_t *req)
{
    earbud_plugin_peer_req_status_t status = peer_req_failure;
    const char *sn = "";
    uint8 size_sn = 0;
    earbud_gaia_response_t *response;

    sn = DeviceInfo_GetSerialNumber();
    if (sn)
    {
        size_sn = strlen(sn);
        if (size_sn > sizeof response->data)
        {
            size_sn = sizeof response->data;
            status = peer_req_truncated;
            DEBUG_LOG_WARN("earbudGaiaPlugin_HandlePeerRequestGetSerialNumber: truncated");
        }
        else
        {
            status = peer_req_success;
        }
    }

    DEBUG_LOG_DEBUG("earbudGaiaPlugin_HandlePeerRequestGetSerialNumber: status=enum:earbud_plugin_peer_req_status_t:%u", status);

    response = PanicNull(malloc(sizeof (earbud_gaia_response_t)));
    response->context = req->context;
    response->request_id = req->request_id;
    response->status = status;
    response->size_data = size_sn;
    memcpy(response->data, sn, size_sn);

    EARBUD_GAIA_PEER_TX(response, earbud_gaia_response_t);
}

static void earbudGaiaPlugin_HandlePeerRequest(earbud_gaia_request_t *req)
{
    switch (req->request_id)
    {
    case peer_req_get_serial_number:
        earbudGaiaPlugin_HandlePeerRequestGetSerialNumber(req);
        break;

    default:
        DEBUG_LOG("earbudGaiaPlugin_HandlePeerRequest: unknown req %u", req->request_id);
        break;
    }
}


static void earbudGaiaPlugin_HandlePeerResponse(earbud_gaia_response_t *rsp)
{
    GAIA_TRANSPORT *t = (GAIA_TRANSPORT *) rsp->context;

    switch (rsp->request_id)
    {
    case peer_req_get_serial_number:
        GaiaFramework_SendResponse(t, GAIA_EARBUD_FEATURE_ID, get_secondary_serial_number, rsp->size_data, rsp->data);
        break;

    default:
        DEBUG_LOG("earbudGaiaPlugin_HandlePeerResponse: unknown request_id %u", rsp->request_id);
        break;
    }
}


static void earbudGaiaPlugin_HandlePeerSigRxInd(PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND_T *ind)
{
    if (ind->channel == PEER_SIG_MSG_CHANNEL_GAIA)
    {
        switch(ind->type)
        {
        case MARSHAL_TYPE_earbud_gaia_request_t:
            earbudGaiaPlugin_HandlePeerRequest((earbud_gaia_request_t *) ind->msg);
            break;

        case MARSHAL_TYPE_earbud_gaia_response_t:
            earbudGaiaPlugin_HandlePeerResponse((earbud_gaia_response_t *) ind->msg);
            break;

        default:
            DEBUG_LOG_DEBUG("earbudGaiaPlugin_HandlePeerSigRxInd: unknown type %u", ind->type);
            break;
        }
    }

    /* Free unmarshalled message after use. */
    free(ind->msg);
}


static void earbudGaiaPlugin_NotifySecondaryConnectionState(gaia_transport *t, uint8 state)
{
    DEBUG_LOG_INFO("earbudGaiaPlugin_NotifySecondaryConnectionState: transport=%p state=%u", t, state);
    GaiaFramework_SendNotificationWithTransport(t, GAIA_EARBUD_FEATURE_ID, secondary_earbud_connection_state, 1, &state);
}


/* Inform each host of the connection state with the secondary */
static void earbudGaiaPlugin_NotifyAllSecondaryConnectionState(uint8 state)
{
    DEBUG_LOG_INFO("earbudGaiaPlugin_NotifyAllSecondaryConnectionState: state=%u", state);
    GaiaFramework_SendNotification(GAIA_EARBUD_FEATURE_ID, secondary_earbud_connection_state, 1, &state);
}


static void earbudGaiaPlugin_HandlePeerSigConnectionInd(PEER_SIG_CONNECTION_IND_T *ind)
{
    if (appSmIsPrimary())
    {
        if (ind->status == peerSigStatusConnected)
        {
            earbudGaiaPlugin_NotifyAllSecondaryConnectionState(gaia_earbud_secondary_connected);
        }
        else if (ind->status == peerSigStatusDisconnected || ind->status == peerSigStatusLinkLoss)
        {
            earbudGaiaPlugin_NotifyAllSecondaryConnectionState(gaia_earbud_secondary_disconnected);
        }
    }
}


static void earbudGaiaPlugin_PeerSignalHander(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch (id)
    {
    case PEER_SIG_CONNECTION_IND:
    /*  The peer earbud has connected or disconnected */
        earbudGaiaPlugin_HandlePeerSigConnectionInd((PEER_SIG_CONNECTION_IND_T *) message);
        break;

    case PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND:
        earbudGaiaPlugin_HandlePeerSigRxInd((PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND_T *) message);
        break;

    default:
        break;
    }

}


static void earbudGaiaPlugin_GetSecondarySerialNumber(GAIA_TRANSPORT *t)
{
    DEBUG_LOG_DEBUG("earbudGaiaPlugin_GetSecondarySerialNumber");

    if (appPeerSigIsConnected())
    {
        earbud_gaia_request_t *req = PanicUnlessMalloc(sizeof(earbud_gaia_request_t));

        req->context = (uint32) t;
        req->request_id = peer_req_get_serial_number;
        EARBUD_GAIA_PEER_TX(req, earbud_gaia_request_t);
    }
    else
    {
        DEBUG_LOG_DEBUG("earbudGaiaPlugin_GetSecondarySerialNumber: peer not connected");
        GaiaFramework_SendError(t, GAIA_EARBUD_FEATURE_ID, get_secondary_serial_number, failed_insufficient_resources);
    }
}


static void earbudGaiaPlugin_SendAllNotifications(GAIA_TRANSPORT *t)
{
    DEBUG_LOG("earbudGaiaPlugin_SendAllNotifications");
    earbudGaiaPlugin_NotifySecondaryConnectionState(t, appDeviceIsPeerConnected() ?
                                                        gaia_earbud_secondary_connected :
                                                        gaia_earbud_secondary_disconnected);
}


static void earbudGaiaPlugin_HandoverComplete(GAIA_TRANSPORT *t, bool is_primary)
{
    DEBUG_LOG("earbudGaiaPlugin_HandoverComplete, is_primary %u", is_primary);
    UNUSED(t);
}


static gaia_framework_command_status_t earbudGaiaPlugin_MainHandler(GAIA_TRANSPORT *t, uint8 pdu_id, uint16 payload_length, const uint8 *payload)
{
    DEBUG_LOG("earbudGaiaPlugin_MainHandler, called for %d", pdu_id);

    UNUSED(payload_length);
    UNUSED(payload);

    switch (pdu_id)
    {
        case is_primary_left_or_right:
            earbudGaiaPlugin_IsPrimaryLeftOrRight(t);
            break;

        case get_secondary_serial_number:
            earbudGaiaPlugin_GetSecondarySerialNumber(t);
            break;
            
        default:
            DEBUG_LOG_ERROR("earbudGaiaPlugin_MainHandler, unhandled call for %d", pdu_id);
            return command_not_handled;
    }

    return command_handled;
}

void EarbudGaiaPlugin_Init(void)
{
    static const gaia_framework_plugin_functions_t functions =
    {
        .command_handler = earbudGaiaPlugin_MainHandler,
        .send_all_notifications = earbudGaiaPlugin_SendAllNotifications,
        .transport_connect = NULL,
        .transport_disconnect = NULL,
        .handover_complete = earbudGaiaPlugin_HandoverComplete,
    };

    DEBUG_LOG("EarbudGaiaPlugin_Init");
    GaiaFramework_RegisterFeature(GAIA_EARBUD_FEATURE_ID, EARBUD_GAIA_PLUGIN_VERSION, &functions);

    /* Register with Peer Signalling */
    appPeerSigClientRegister(&earbud_gaia_peer_signal_task);

    appPeerSigMarshalledMsgChannelTaskRegister(&earbud_gaia_peer_signal_task,
                                               PEER_SIG_MSG_CHANNEL_GAIA,
                                               earbud_gaia_marshal_type_descriptors,
                                               NUMBER_OF_EARBUD_GAIA_MARSHAL_TYPES);




}

#endif /* INCLUDE_GAIA */

/*************************************************************************
Copyright (c) 2010 - 2020 Qualcomm Technologies International, Ltd.


FILE
    gaia.c

DESCRIPTION
    Generic Application Interface Architecture
    This library implements the GAIA protocol for a server- or client-side
    SPP connection.

    The library exposes a functional downstream API and a message-based upstream API.
*/

#define DEBUG_LOG_MODULE_NAME gaia
#include <logging.h>
DEBUG_LOG_DEFINE_LEVEL_VAR

#include <panic.h>
#include <rwcp_server.h>
#include <message.h>

#include "gaia_private.h"
#include "gaia_register_custom_sdp.h"
#include "gaia_db.h"

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_TYPE(gaia_internal_message)
LOGGING_PRESERVE_MESSAGE_TYPE(GaiaMessageId)

/*
 * A macro for obtaining the number of elements in an array.
 * This macros is not present in BlueLab so provide it if not present so that
 * it is available for host testing.
 */
#ifndef ARRAY_DIM
#define ARRAY_DIM(x) (sizeof ((x)) / sizeof ((x)[0]))
#endif

#ifndef MIN
#define MIN(a,b)    (((a)<(b))?(a):(b))
#endif

/*  Commands which can be handled by the library but can be supplanted
 *  by an application implementation.
 */
static const uint16 lib_commands[] =
{
    GAIA_COMMAND_DEVICE_RESET,
    GAIA_COMMAND_GET_API_VERSION,
};

GAIA_T gaia;

/*************************************************************************
NAME
    find_locus_bit

DESCRIPTION
    Determine if command can be handled by the library
    Return index into lib_commands array or GAIA_INVALID_ID
*/
static uint16 find_locus_bit(uint16 command_id)
{
    uint16 idx;

    for (idx = 0;
        idx < ARRAY_DIM(lib_commands);
        ++idx)
    {
        if (lib_commands[idx] == command_id)
        {
            return idx;
        }
    }

    return GAIA_INVALID_ID;
}



/*************************************************************************
NAME
    gaiaProcessCommand

DESCRIPTION
    Process a GAIA command or pass it up as unhandled
*/
void Gaia_ProcessCommand(gaia_transport *transport, uint16 vendor_id, uint16 command_id, uint16 size_payload, const uint8 *payload)
{
    if (gaia.app_version == gaia_app_version_3)
    {
        DEBUG_LOG_VERBOSE("gaiaProcessCommand, send command message, vendor_id %u, command_id %u", vendor_id, command_id);
        MESSAGE_MAKE(msg, GAIA_COMMAND_IND_T);
        msg->transport = transport;
        msg->vendor_id = vendor_id;
        msg->command_id = command_id;
        msg->size_payload = size_payload;
        msg->payload = payload;
        MessageSend(gaia.app_task, GAIA_COMMAND_IND, msg);
    }
    else
    {
        DEBUG_LOG_ERROR("gaiaProcessCommand, unsupported app version %u", gaia.app_version);
        send_ack(transport, vendor_id, command_id, GAIA_STATUS_NOT_SUPPORTED, 0, NULL);
        Gaia_TransportPacketHandled(transport, size_payload, payload);
    }
}



/*************************************************************************
 *                                                                       *
 *  Public interface functions                                           *
 *                                                                       *
 *************************************************************************/

/*************************************************************************
NAME
    GaiaInit

DESCRIPTION
    Initialise the Gaia protocol handler library
*/
void GaiaInit(Task app_task, uint16 max_connections)
{
    DEBUG_LOG("GaiaInit, max_connections %u", max_connections);

    gaia.app_task = app_task;

    gaia.gaia_v3_command_handler = NULL;
    gaia.app_version = gaia_app_version_2;
    gaia.command_locus_bits = 0;

    /* Default API minor version (may be overridden by GaiaSetApiMinorVersion()) */
    gaia.api_minor = GAIA_API_VERSION_MINOR;

    /* Send confirmation back */
    MESSAGE_PMAKE(status, GAIA_INIT_CFM_T);
    status->success = TRUE;
    MessageSend(gaia.app_task, GAIA_INIT_CFM, status);

}


/*************************************************************************
NAME
    GaiaDisconnectRequest

DESCRIPTION
    Disconnect from host
*/
void GaiaDisconnectRequest(GAIA_TRANSPORT *t)
{
    Gaia_TransportDisconnectReq((gaia_transport *)t);
}


/*************************************************************************
NAME
    GaiaDisconnectResponse

DESCRIPTION
    Indicates that the client has processed a GAIA_DISCONNECT_IND message
*/
void GaiaDisconnectResponse(GAIA_TRANSPORT *t)
{
    UNUSED(t);
    //gaiaTransportTidyUpOnDisconnection((gaia_transport *) transport);
}


/*************************************************************************
NAME
    GaiaSetApiMinorVersion

DESCRIPTION
    Changes the API Minor Version reported by GAIA_COMMAND_GET_API_VERSION
    Returns TRUE on success, FALSE if the value is out of range (0..15) or
    the GAIA storage is not allocated
*/
bool GaiaSetApiMinorVersion(uint8 version)
{
    if (version > GAIA_API_VERSION_MINOR_MAX)
        return FALSE;

    gaia.api_minor = version;
    return TRUE;
}


/*************************************************************************
NAME
    GaiaSetAppWillHandleCommand

DESCRIPTION
    Request that the given command be passed to application code
    rather than being handled by the library.  Returns TRUE on success.
*/
bool GaiaSetAppWillHandleCommand(uint16 command_id, bool value)
{
    uint16 idx;

    idx = find_locus_bit(command_id);

    if (idx != GAIA_INVALID_ID)
    {
        if (value)
            gaia.command_locus_bits |= ULBIT(idx);

        else
            gaia.command_locus_bits &= ~ULBIT(idx);

        return TRUE;
    }

    return FALSE;
}


/*************************************************************************
NAME
    GaiaSendPacket

DESCRIPTION
    Build a Gaia packet in the transport sink and flush it
    The payload is an array of uint8s; contrast GaiaBuildAndSendSynch16()
    The function completes synchronously and no confirmation message is
    sent to the calling task
*/
void GaiaSendPacket(GAIA_TRANSPORT *transport,
                    uint16 vendor_id, uint16 command_id, uint8 status,
                    uint16 size_payload, const uint8 *payload)
{
    gaia_transport *t = (gaia_transport *)transport;
    if (t && Gaia_TransportIsConnected(t))
        Gaia_TransportSendPacket(t, vendor_id, command_id, status, size_payload, payload);
    else
        DEBUG_LOG_ERROR("GaiaSendPacket, no transport, or transport not connected");
}


void Gaia_SendDataPacket(GAIA_TRANSPORT *transport,
                         uint16 vendor_id, uint16 command_id, uint8 status,
                         uint16 size_payload, const uint8 *payload)
{
    gaia_transport *t = (gaia_transport *)transport;
    if (t && Gaia_TransportIsConnected(t))
        Gaia_TransportSendDataPacket(t, vendor_id, command_id, status, size_payload, payload);
    else
        DEBUG_LOG_ERROR("Gaia_SendDataPacket, no transport, or transport not connected");

}

/*************************************************************************
NAME
    GaiaTransportGetType

DESCRIPTION
    Returns the transport type for the given transport instance
*/
gaia_transport_type GaiaTransportGetType(const GAIA_TRANSPORT *transport)
{
    const gaia_transport *t = (gaia_transport *)transport;
    PanicZero(t);
    return t->type;
}


void GaiaSetAppVersion(gaia_app_version_t version)
{
    gaia.app_version = version;
}


gaia_app_version_t GaiaGetAppVersion(void)
{
    return gaia.app_version;
}



/*************************************************************************
NAME
    GaiaSetDataEndpointMode

DESCRIPTION
    Responds to a GAIA_COMMAND_SET_DATA_ENDPOINT_MODE command.
    Sets the data endpoint mode
*/
bool Gaia_SetDataEndpointMode(GAIA_TRANSPORT *transport, gaia_data_endpoint_mode_t mode)
{
    gaia_transport *t = (gaia_transport *)transport;
    PanicNull(t);
    return Gaia_TransportSetDataEndpointMode(t, mode);
}

gaia_data_endpoint_mode_t Gaia_GetDataEndpointMode(GAIA_TRANSPORT *transport)
{
    gaia_transport *t = (gaia_transport *)transport;
    PanicNull(t);
    return Gaia_TransportGetDataEndpointMode(t);
}

gaia_data_endpoint_mode_t Gaia_GetPayloadDataEndpointMode(GAIA_TRANSPORT *transport, uint16 size_payload, const uint8 *payload)
{
    gaia_transport *t = (gaia_transport *)transport;
    PanicNull(t);
    return Gaia_TransportGetPayloadDataEndpointMode(t, size_payload, payload);
}

void Gaia_CommandResponse(GAIA_TRANSPORT *transport, uint16 payload_size, const void *payload)
{
    gaia_transport *t = (gaia_transport *)transport;
    PanicNull(t);
    Gaia_TransportPacketHandled(t, payload_size, payload);
}




bool Gaia_IsGattCommandEndpoint(uint16 handle)
{
    return ((handle == HANDLE_GAIA_COMMAND_ENDPOINT) ? TRUE : FALSE);
}

bool Gaia_IsGattDataEndpoint(uint16 handle)
{
    return ((handle == HANDLE_GAIA_DATA_ENDPOINT) ? TRUE : FALSE);
}

bool Gaia_IsGattResponseClientConfig(uint16 handle)
{
    return ((handle == HANDLE_GAIA_RESPONSE_CLIENT_CONFIG) ? TRUE : FALSE);
}

bool Gaia_IsGattDataClientConfig(uint16 handle)
{
    return ((handle == HANDLE_GAIA_DATA_CLIENT_CONFIG) ? TRUE : FALSE);
}

bool Gaia_IsGattResponseEndpoint(uint16 handle)
{
    return ((handle == HANDLE_GAIA_RESPONSE_ENDPOINT) ? TRUE : FALSE);
}

uint16 Gaia_GetGattResponseEndpoint(void)
{
    return HANDLE_GAIA_RESPONSE_ENDPOINT;
}

uint16 Gaia_GetGattDataEndpoint(void)
{
    return HANDLE_GAIA_DATA_ENDPOINT;
}

uint16 Gaia_GetGattGaiaService(void)
{
    return HANDLE_GAIA_SERVICE;
}

uint16 Gaia_GetGattGaiaServiceEnd(void)
{
    return HANDLE_GAIA_SERVICE_END;
}


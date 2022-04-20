/*****************************************************************
Copyright (c) 2011 - 2018 Qualcomm Technologies International, Ltd.
 */

#define DEBUG_LOG_MODULE_NAME gaia_transport
#include <logging.h>

#include "gaia_private.h"
#include <stdlib.h>

static const gaia_transport_functions_t *gaia_transport_functions[gaia_transport_max];
static gaia_transport *gaia_transports;

static void gaia_TransportAdd(gaia_transport *t)
{
    t->next = gaia_transports;
    gaia_transports = t;
}

static void gaia_TransportRemove(gaia_transport *t)
{
    gaia_transport **tpp, *tp;

     for (tpp = &gaia_transports; (tp = *tpp) != NULL;)
     {
        if (tp == t)
        {
            *tpp = tp->next;
            break;
        }
        else
            tpp = &tp->next;
    }

    /* Ensure any message still to be delivered are flushed */
    MessageFlushTask(&t->task);
}




bool Gaia_TransportSendPacket(gaia_transport *t,
                              uint16 vendor_id, uint16 command_id, uint16 status,
                              uint16 payload_length, const uint8 *payload)
{
    PanicNull(t);
    return t->functions->send_command_packet(t, vendor_id, command_id, status,
                                             payload_length, payload);
}

bool Gaia_TransportSendDataPacket(gaia_transport *t,
                                  uint16 vendor_id, uint16 command_id, uint16 status,
                                  uint16 payload_length, const uint8 *payload)
{
    PanicNull(t);
    if (t->functions->send_data_packet)
        return t->functions->send_data_packet(t, vendor_id, command_id, status,
                                              payload_length, payload);
    else
        return t->functions->send_command_packet(t, vendor_id, command_id, status,
                                                 payload_length, payload);
}


uint16 Gaia_TransportGetPacketSpace(gaia_transport *t)
{
    PanicNull(t);
    if (t->functions->get_packet_space)
        return t->functions->get_packet_space(t);
    else if (t->functions->get_info)
    {
        /* Fall back to pmalloc'ed packet that size is limited. */
        uint32 value;
        uint16 packet_size;
        uint16 payload_size;

        t->functions->get_info(t, GAIA_TRANSPORT_MAX_TX_PACKET, &value);
        packet_size = (uint16) value;
        t->functions->get_info(t, GAIA_TRANSPORT_PAYLOAD_SIZE, &value);
        payload_size = (uint16) value;

        /* Limit the max payload/packet size as this uses the heap memory. */
        if (packet_size <= GAIA_TRANSPORT_MAX_MALLOC_TX_PACKET_SIZE)
            return payload_size;
        else
            return GAIA_TRANSPORT_MAX_MALLOC_TX_PACKET_SIZE - (packet_size - payload_size);
    }
    else
        return 0;
}

uint8* Gaia_TransportCreatePacket(gaia_transport *t, const uint16 vendor_id, const uint16 command_id, const uint16 payload_size_requested)
{
    PanicNull(t);
    if (t->functions->create_packet)
        return t->functions->create_packet(t, vendor_id, command_id, payload_size_requested);
    else if (t->functions->get_info)
    {
        /* Fall back to pmalloc'ed packet that size is limited. */
        uint32 value;
        uint16 packet_size;
        uint16 payload_size;

        /* Check if the requested payload size is within the limits. */
        t->functions->get_info(t, GAIA_TRANSPORT_MAX_TX_PACKET, &value);
        packet_size = (uint16) value;
        t->functions->get_info(t, GAIA_TRANSPORT_PAYLOAD_SIZE, &value);
        payload_size = (uint16) value;
        if (GAIA_TRANSPORT_MAX_MALLOC_TX_PACKET_SIZE < packet_size)
            payload_size = GAIA_TRANSPORT_MAX_MALLOC_TX_PACKET_SIZE - (packet_size - payload_size);

        if (payload_size < payload_size_requested)
        {
            DEBUG_LOG_ERROR("Gaia_TransportCreatePacket, ERROR: The requested size (%u) must not exceed %u!", payload_size_requested, payload_size);
            Panic();
        }

        if (t->tx_pkt_buf != NULL)
        {
            DEBUG_LOG_ERROR("Gaia_TransportCreatePacket, ERROR: Previously allocated memory (%p) is not freed!", t->tx_pkt_buf);
            Panic();
        }

        t->tx_pkt_buf = (gaia_transport_tx_packet_buffer_t*) malloc(sizeof(gaia_transport_tx_packet_buffer_t) + payload_size_requested);
        PanicNull(t->tx_pkt_buf);

        t->tx_pkt_buf->vendor_id      = vendor_id;
        t->tx_pkt_buf->command_id     = command_id;
        t->tx_pkt_buf->payload_length = payload_size_requested;
        return &t->tx_pkt_buf->payload[0];
    }
    return NULL;
}

void Gaia_TransportFlushPacket(gaia_transport *t, const uint16 size_payload, const uint8 *payload)
{
    PanicNull(t);
    if (t->functions->flush_packet)
        t->functions->flush_packet(t, size_payload, payload);
    else
    {
        /* Fall back to pmalloc'ed packet that size is limited. */
        if (t->tx_pkt_buf)
        {
            PanicNull((void*)(t->functions->send_command_packet));
            t->functions->send_command_packet(t, t->tx_pkt_buf->vendor_id,
                                              t->tx_pkt_buf->command_id, GAIA_STATUS_NONE,
                                              t->tx_pkt_buf->payload_length,
                                              &t->tx_pkt_buf->payload[0]);
            free(t->tx_pkt_buf);
            t->tx_pkt_buf = NULL;
        }
        else
        {
            DEBUG_LOG_ERROR("Gaia_TransportFlushPacket, ERROR: This function is called without setting the packet in the buffer!");
            Panic();
        }
    }
}


/*! @brief Register GAIA server for a given transport.
 */
void Gaia_TransportRegister(gaia_transport_type type, const gaia_transport_functions_t *functions)
{
    DEBUG_LOG_INFO("gaiaTransportRegister, type %u", type);
    PanicFalse(gaia_transport_functions[type] == NULL);
    gaia_transport_functions[type] = functions;
}


/*! @brief Start GAIA server on a given transport.
 */
void Gaia_TransportStartService(gaia_transport_type type)
{
    DEBUG_LOG_INFO("gaiaTransportStartService, type %u", type);
    PanicFalse(gaia_transport_functions[type]);
    PanicZero(gaia_transport_functions[type]->start_service);

    /* Only malloc enough memory for transport type */
    gaia_transport *t = calloc(1, gaia_transport_functions[type]->service_data_size);
    if (t)
    {
        t->functions = gaia_transport_functions[type];
        t->type = type;
        t->client_data = 0;
        t->state = GAIA_TRANSPORT_STARTING;
        BdaddrTpSetEmpty(&t->tp_bd_addr);
        t->tx_pkt_buf = NULL;
        gaia_TransportAdd(t);

        /* Attempt to start service */
        t->functions->start_service(t);
    }
    else
    {
        DEBUG_LOG_ERROR("gaiaTransportStartService, failed to allocate instance");
        gaia_TransportSendGaiaStartServiceCfm(type, NULL, FALSE);
    }
}


/*! @brief Called from transport to confirm if server is started or not.
 */
void Gaia_TransportStartServiceCfm(gaia_transport *t, bool success)
{
    DEBUG_LOG_INFO("gaiaTransportStartServiceCfm, type %u, success %u", t->type, success);
    PanicFalse(t);

    gaia_TransportSendGaiaStartServiceCfm(t->type, success ? t : NULL, success);

    if (success)
        t->state = GAIA_TRANSPORT_STARTED;
    else
    {
        /* Remove transport and free instance */
        gaia_TransportRemove(t);
        free(t);
    }
}


/*! @brief Stop GAIA server on a given transport.
 */
void Gaia_TransportStopService(gaia_transport *t)
{
    DEBUG_LOG_INFO("gaiaTransportStopService, transport %p", t);

    PanicZero(t->functions->stop_service);
    t->functions->stop_service(t);
}


/*! @brief Called from transport to confirm if server is stopped or not.
 */
void Gaia_TransportStopServiceCfm(gaia_transport *t, bool success)
{
    DEBUG_LOG_ERROR("gaiaTransportStopServiceCfm, type %u, success %u", t->type, success);
    PanicFalse(t);
    gaia_TransportSendGaiaStopServiceCfm(t->type, t, success);
    if (success)
    {
        if (t->tx_pkt_buf)
        {
            free(t->tx_pkt_buf);
            t->tx_pkt_buf = NULL;
        }
        gaia_TransportRemove(t);
        free(t);
    }
    else
        Panic();
}


/*! @brief Connect GAIA server on a given transport.
 */
void Gaia_TransportConnectReq(gaia_transport_type type, const tp_bdaddr *tp_bd_addr)
{
    gaia_transport_index index = 0;
    gaia_transport *t = Gaia_TransportIterate(&index);
    while (t)
    {
        /* Find transport that is in started state and has connect function */
        if ((t->type == type) && (t->state == GAIA_TRANSPORT_STARTED) && t->functions->connect_req)
        {
            t->functions->connect_req(t, tp_bd_addr);
            return;
        }
        t = Gaia_TransportIterate(&index);
    }

    gaia_TransportSendGaiaConnectCfm(t, FALSE, tp_bd_addr);
}


/*! @brief Called from transport to indicate new connection.
 */
void Gaia_TransportConnectInd(gaia_transport *t, bool success, const tp_bdaddr *tp_bd_addr)
{
    PanicNull(t);

    if (success && tp_bd_addr)
        t->tp_bd_addr = *tp_bd_addr;
    else
        BdaddrTpSetEmpty(&t->tp_bd_addr);

    DEBUG_LOG_DEBUG("Gaia_TransportConnectInd, transport %p, success %u, state %u", t, success, t->state);

    if (t->state == GAIA_TRANSPORT_CONNECTING)
        gaia_TransportSendGaiaConnectCfm(t, success, tp_bd_addr);
    else
        gaia_TransportSendGaiaConnectInd(t, success, tp_bd_addr);

    t->state = success ? GAIA_TRANSPORT_CONNECTED : GAIA_TRANSPORT_STARTED;
}


void Gaia_TransportHandoverInd(gaia_transport *t, bool success, bool is_primary)
{
    PanicNull(t);

    DEBUG_LOG_DEBUG("Gaia_TransportHandoverInd, transport %p, success %u, state %u, is_primary %u", t, success, t->state, is_primary);
    gaia_TransportSendGaiaHandoverInd(t, success, is_primary);
}


bool Gaia_TransportIsConnected(gaia_transport *t)
{
    PanicNull(t);
    return (t->state == GAIA_TRANSPORT_CONNECTED);
}


/*! @brief Disconnect GAIA server on a given transport.
 */
void Gaia_TransportDisconnectReq(gaia_transport *t)
{
    PanicNull(t);
    if (t->functions->disconnect_req && t->state == GAIA_TRANSPORT_CONNECTED)
    {
        t->state = GAIA_TRANSPORT_DISCONNECTING;
        t->functions->disconnect_req(t);
    }
    else
    {
        BdaddrTpSetEmpty(&t->tp_bd_addr);
        gaia_TransportSendGaiaDisconnectCfm(t);
    }
}


/*! @brief Called from transport to indicate disconnection.
 */
void Gaia_TransportDisconnectInd(gaia_transport *t)
{
    PanicNull(t);
    if (t->state == GAIA_TRANSPORT_DISCONNECTING)
        gaia_TransportSendGaiaDisconnectCfm(t);
    else
        gaia_TransportSendGaiaDisconnectInd(t);

    BdaddrTpSetEmpty(&t->tp_bd_addr);
    if (t->tx_pkt_buf)
    {
        free(t->tx_pkt_buf);
        t->tx_pkt_buf = NULL;
    }
    t->state = GAIA_TRANSPORT_STARTED;
}


void Gaia_TransportErrorInd(gaia_transport *t, gaia_transport_error error)
{
    UNUSED(t);
    UNUSED(error);

    PanicNull(t);
    PanicZero(t->functions->error);
    t->state = GAIA_TRANSPORT_ERROR;
    t->functions->error(t);
}


gaia_transport *Gaia_TransportIterate(gaia_transport_index *index)
{
    gaia_transport *t;

    if (*index)
        t = (*index)->next;
    else
        t = gaia_transports;

    *index = t;

    return t;
}


bool Gaia_TransportHasFeature(gaia_transport *t, uint8 feature)
{
    PanicNull(t);
    PanicZero(t->functions->features);

    return (t->functions->features(t) & feature) ? TRUE : FALSE;
}


gaia_transport *Gaia_TransportFindService(gaia_transport_type type, gaia_transport_index *index)
{
    gaia_transport *t = Gaia_TransportIterate(index);
    while (t)
    {
        if (t->type == type)
            return t;

        t = Gaia_TransportIterate(index);
    }

    return NULL;
}


bool Gaia_TransportSetDataEndpointMode(gaia_transport *t, gaia_data_endpoint_mode_t mode)
{
    PanicNull(t);
    if (t->functions->set_data_endpoint)
        return t->functions->set_data_endpoint(t, mode);
    else
        return FALSE;
}


gaia_data_endpoint_mode_t Gaia_TransportGetDataEndpointMode(gaia_transport *t)
{
    PanicNull(t);
    if (t->functions->get_data_endpoint)
        return t->functions->get_data_endpoint(t);
    else
        return GAIA_DATA_ENDPOINT_MODE_NONE;
}

gaia_data_endpoint_mode_t Gaia_TransportGetPayloadDataEndpointMode(gaia_transport *t, uint16 size_payload, const uint8 *payload)
{
    PanicNull(t);
    if (t->functions->get_payload_data_endpoint)
        return t->functions->get_payload_data_endpoint(t, size_payload, payload);
    else
        return GAIA_DATA_ENDPOINT_MODE_NONE;
}


bool Gaia_TransportGetInfo(gaia_transport *t, gaia_transport_info_key_t key, uint32 *value)
{
    PanicNull(t);
    PanicNull(value);
    return t->functions->get_info ? t->functions->get_info(t, key, value) : FALSE;
}


bool Gaia_TransportSetParameter(gaia_transport *t, gaia_transport_info_key_t key, uint32 *value)
{
    PanicNull(t);
    PanicNull(value);
    return t->functions->get_info ? t->functions->set_parameter(t, key, value) : FALSE;
}


uint32 Gaia_TransportGetClientData(gaia_transport *t)
{
    PanicNull(t);
    return t->client_data;
}


void Gaia_TransportSetClientData(gaia_transport *t, uint32 client_data)
{
    PanicNull(t);
    t->client_data = client_data;
}


void Gaia_TransportPacketHandled(gaia_transport *t, uint16 payload_size, const void *payload)
{
    PanicNull(t);
    t->functions->packet_handled(t, payload_size, payload);
}


gaia_transport *Gaia_TransportFindByTpBdAddr(const tp_bdaddr *tp_bd_addr, gaia_transport_index *index)
{
    gaia_transport *t = Gaia_TransportIterate(index);
    while (t)
    {
        if (BdaddrTpIsSame(tp_bd_addr, &t->tp_bd_addr))
            return t;

        t = Gaia_TransportIterate(index);
    }

    return NULL;
}



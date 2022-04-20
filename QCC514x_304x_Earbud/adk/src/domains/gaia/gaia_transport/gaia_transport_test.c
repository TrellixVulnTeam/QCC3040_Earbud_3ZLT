/*****************************************************************
Copyright (c) 2011 - 2020 Qualcomm Technologies International, Ltd.
*/
#define DEBUG_LOG_MODULE_NAME gaia_transport
#include <logging.h>
DEBUG_LOG_DEFINE_LEVEL_VAR

#include "gaia.h"
#include "gaia_transport.h"

#include <source.h>
#include <sink.h>
#include <stream.h>
#include <panic.h>
#include <pmalloc.h>

#define GAIA_TRANSPORT_TEST_DEFAULT_PROTOCOL_VERSION  (3)
#define GAIA_TRANSPORT_TEST_MAX_PROTOCOL_VERSION      (4)

#define GAIA_TEST_TRASPORT_PAYLOAD_SIZE (65535)

#define GAIA_TRANSPORT_TEST_MAX_RX_PENDING_PKTS  (2)

typedef struct
{
    gaia_transport common;
    Sink pipe_host;
    Sink pipe_device;
    uint8 protocol_version;
    uint8  rx_packets_pending;
    uint16 rx_data_pending;
} gaia_transport_test_t;

static void gaiaTransport_TestHandleMessage(Task task, MessageId id, Message message);


static bool gaiaTransport_TestSendPacket(gaia_transport *t, uint16 vendor_id, uint16 command_id,
                                         uint8 status, uint16 size_payload, const void *payload)
{
    gaia_transport_test_t *tt = (gaia_transport_test_t *)t;
    const uint16 pkt_length = Gaia_TransportCommonCalcTxPacketLength(size_payload, status);
    const uint16 sink_space = SinkSlack(tt->pipe_host);
    if (sink_space >= pkt_length)
    {
        uint8 *sink_ptr = SinkMap(tt->pipe_host);
        uint16 claimed = SinkClaim(tt->pipe_host, pkt_length);
        if (sink_ptr && (claimed != 0xFFFF))
        {
            uint8 *pkt_buf = sink_ptr + claimed;

            /* Build packet into buffer */
            Gaia_TransportCommonBuildPacket(tt->protocol_version, pkt_buf, pkt_length, vendor_id, command_id, status, size_payload, payload);

            /* Send packet */
            if (SinkFlush(tt->pipe_host, pkt_length))
            {
                DEBUG_LOG_VERBOSE("gaiaTransportTestSendPacket, sending, vendor_id %u, command_id %u, pkt_length %u", vendor_id, command_id, pkt_length);
                DEBUG_LOG_DATA_V_VERBOSE(pkt_buf, pkt_length);
                return TRUE;
            }
        }
    }
    else
    {
        DEBUG_LOG_ERROR("gaiaTransportTestSendPacket, not enough space %u", pkt_length);
        Gaia_TransportErrorInd(&tt->common, GAIA_TRANSPORT_INSUFFICENT_BUFFER_SPACE);
    }

    return FALSE;
}


static bool gaiaTransport_TestProcessCommand(gaia_transport *t, uint16 pkt_size, uint16 vendor_id, uint16 command_id, uint16 size_payload, const uint8 *payload)
{
    gaia_transport_test_t *tt = (gaia_transport_test_t *)t;
    tt->rx_data_pending += pkt_size;
    tt->rx_packets_pending += 1;
    Gaia_ProcessCommand(t, vendor_id, command_id, size_payload, payload);
    return tt->rx_packets_pending < GAIA_TRANSPORT_TEST_MAX_RX_PENDING_PKTS;
}


static void gaiaTransport_TestReceivePacket(gaia_transport_test_t *tt)
{
    const uint16 data_length = SourceSize(StreamSourceFromSink(tt->pipe_host));
    const uint8 *data_buf = SourceMap(StreamSourceFromSink(tt->pipe_host));

    if (tt->rx_data_pending == 0)
    {
        DEBUG_LOG_VERBOSE("gaiaTransportTestReceivePacket, data_length %u", data_length);
        if (data_length)
            Gaia_TransportCommonReceivePacket(&tt->common, tt->protocol_version, data_length, data_buf, gaiaTransport_TestProcessCommand);
    }
    else
        DEBUG_LOG_WARN("gaiaTransportTestReceivePacket, receive data being processed");
}


static void gaiaTransport_TestPacketHandled(gaia_transport *t, uint16 size_payload, const void *payload)
{
    gaia_transport_test_t *tt = (gaia_transport_test_t *)t;
    PanicNull(tt);
    UNUSED(payload);

    /* Decrement number of packets pending */
    PanicFalse(tt->rx_packets_pending > 0);
    tt->rx_packets_pending -= 1;

    DEBUG_LOG_VERBOSE("gaiaTransport_TestPacketHandled, size %u, remaining %u", size_payload, tt->rx_packets_pending);

    /* Wait until all packets have been processed before removing from buffer as we can't be
     * certain that packets will be handled in order they are received */
    if (tt->rx_packets_pending == 0)
    {
        DEBUG_LOG_VERBOSE("gaiaTransportTestReceivePacket, all data processed");

        SourceDrop(StreamSourceFromSink(tt->pipe_host), tt->rx_data_pending);
        tt->rx_data_pending = 0;

        /* Check if more data has arrived since we started processing */
        gaiaTransport_TestReceivePacket(tt);
    }
}



/*! @brief Called from GAIA transport to start test service
 *
 *  @result Pointer to transport instance
 */
static void gaiaTransport_TestStartService(gaia_transport *t)
{
    gaia_transport_test_t *tt = (gaia_transport_test_t *)t;
    PanicNull(tt);
    DEBUG_LOG_INFO("gaiaTransportTestStartService");

    /* Initialise task */
    t->task.handler = gaiaTransport_TestHandleMessage;

    /* Initialise default parameters */
    tt->protocol_version = GAIA_TRANSPORT_TEST_DEFAULT_PROTOCOL_VERSION;

    /* Initialsiation done, confirm start */
    Gaia_TransportStartServiceCfm(t, TRUE);
}


/*! @brief Called from GAIA transport to stop test service
 *
 *  @param Pointer to transport instance
 */
static void gaiaTransport_TestStopService(gaia_transport *t)
{
    gaia_transport_test_t *tt = (gaia_transport_test_t *)t;
    PanicNull(tt);
    DEBUG_LOG_INFO("gaiaTransportTestStopService");

    /* Confirm stop */
    Gaia_TransportStopServiceCfm(t, TRUE);
}


static void gaiaTransport_TestConnectReq(gaia_transport *t, const tp_bdaddr *tp_bd_addr)
{
    DEBUG_LOG_INFO("gaiaTransportTestConnectReq");
    UNUSED(tp_bd_addr);

    Gaia_TransportConnectInd(t, TRUE, NULL);
}


static void gaiaTransport_TestDisconnectReq(gaia_transport *t)
{
    DEBUG_LOG_INFO("gaiaTransportTestDisconnectReq");

    Gaia_TransportDisconnectInd(t);
}


static void gaiaTransport_TestError(gaia_transport *t)
{
    gaia_transport_test_t *tt = (gaia_transport_test_t *)t;
    PanicNull(tt);
    DEBUG_LOG_INFO("gaiaTransportTestError");

    SourceDrop(StreamSourceFromSink(tt->pipe_host),   SourceSize(StreamSourceFromSink(tt->pipe_host)));
    SourceDrop(StreamSourceFromSink(tt->pipe_device), SourceSize(StreamSourceFromSink(tt->pipe_device)));

    Gaia_TransportDisconnectInd(&tt->common);
}


static uint8 gaiaTransport_TestFeatures(gaia_transport *t)
{
    UNUSED(t);

    return 0;
}


static bool gaiaTransport_TestGetInfo(gaia_transport *t, gaia_transport_info_key_t key, uint32 *value)
{
    gaia_transport_test_t *tt = (gaia_transport_test_t *)t;
    PanicNull(tt);
    DEBUG_LOG_INFO("gaiaTransport_TestGetInfo");

    switch (key)
    {
        case GAIA_TRANSPORT_MAX_TX_PACKET:
        case GAIA_TRANSPORT_OPTIMUM_TX_PACKET:
        case GAIA_TRANSPORT_MAX_RX_PACKET:
        case GAIA_TRANSPORT_OPTIMUM_RX_PACKET:
            if (tt->protocol_version >= 4)
                *value = 1024;
            else
                *value = 255;
            break;
        case GAIA_TRANSPORT_TX_FLOW_CONTROL:
        case GAIA_TRANSPORT_RX_FLOW_CONTROL:
            *value =1;
            break;

        case GAIA_TRANSPORT_PROTOCOL_VERSION:
            *value = tt->protocol_version;
            break;

        case GAIA_TRANSPORT_PAYLOAD_SIZE:
            *value = GAIA_TEST_TRASPORT_PAYLOAD_SIZE;
            break;

        default:
            return FALSE;
    }
    return TRUE;
}


static bool gaiaTransport_TestSetParameter(gaia_transport *t, gaia_transport_info_key_t key, uint32 *value)
{
    gaia_transport_test_t *tt = (gaia_transport_test_t *)t;
    PanicNull(tt);
    DEBUG_LOG_INFO("gaiaTransport_TestSetParameter");

    switch (key)
    {
        case GAIA_TRANSPORT_PROTOCOL_VERSION:
            if ((*value >= GAIA_TRANSPORT_TEST_DEFAULT_PROTOCOL_VERSION) && (*value <= GAIA_TRANSPORT_TEST_MAX_PROTOCOL_VERSION))
                tt->protocol_version = *value;
            *value = tt->protocol_version;
            break;

        default:
            /* Ignore any request to set parameters, just return current value */
            return gaiaTransport_TestGetInfo(t, key, value);
    }

    return TRUE;
}


static void gaiaTransport_TestHandleMessage(Task task, MessageId id, Message message)
{
    gaia_transport_test_t *tt = (gaia_transport_test_t *)task;

    switch (id)
    {
        case MESSAGE_MORE_DATA:
            gaiaTransport_TestReceivePacket(tt);
            break;

        case MESSAGE_MORE_SPACE:
            break;

        default:
            DEBUG_LOG_ERROR("gaiaTransportTestHandleMessage, unhandled message MESSAGE:0x%04x", id);
            DEBUG_LOG_DATA_ERROR(message, psizeof(message));
            break;
    }
}


void GaiaTransport_TestInit(void)
{
    static const gaia_transport_functions_t functions =
    {
        .service_data_size      = sizeof(gaia_transport_test_t),
        .start_service          = gaiaTransport_TestStartService,
        .stop_service           = gaiaTransport_TestStopService,
        .packet_handled         = gaiaTransport_TestPacketHandled,
        .send_command_packet    = gaiaTransport_TestSendPacket,
        .send_data_packet       = NULL,
        .connect_req            = gaiaTransport_TestConnectReq,
        .disconnect_req         = gaiaTransport_TestDisconnectReq,
        .set_data_endpoint      = NULL,
        .get_data_endpoint      = NULL,
        .error                  = gaiaTransport_TestError,
        .features               = gaiaTransport_TestFeatures,
        .get_info               = gaiaTransport_TestGetInfo,
        .set_parameter          = gaiaTransport_TestSetParameter,
#if defined(ENABLE_GAIA_DYNAMIC_HANDOVER) && defined(INCLUDE_MIRRORING)
        .handover_veto          = NULL,
        .handover_marshal       = NULL,
        .handover_unmarshal     = NULL,
        .handover_commit        = NULL,
#endif /* ENABLE_GAIA_DYNAMIC_HANDOVER && INCLUDE_MIRRORING */
    };

    /* Register this transport with GAIA */
    Gaia_TransportRegister(gaia_transport_test, &functions);
}


/* Move all the following functions into the KEEP_PM section to ensure they are
 * not removed during garbage collection */
#pragma unitcodesection KEEP_PM


extern bool GaiaTransport_TestHostConnect(void)
{
    gaia_transport_index index = 0;
    gaia_transport *t = Gaia_TransportFindService(gaia_transport_test, &index);
    if (t && t->state == GAIA_TRANSPORT_STARTED)
    {
        gaia_transport_test_t *tt = (gaia_transport_test_t *)t;

        Gaia_TransportConnectInd(t, TRUE, NULL);

        /* Create pipes to/from host */
        PanicFalse(StreamPipePair(&tt->pipe_host, &tt->pipe_device, 1024, 1024));
        MessageStreamTaskFromSource(StreamSourceFromSink(tt->pipe_host), &t->task);

        /* Check if any data has already arrived */
        gaiaTransport_TestReceivePacket(tt);
        return TRUE;
    }
    else
        return FALSE;
}


extern bool GaiaTransport_TestHostIsConnected(void)
{
    gaia_transport_index index = 0;
    gaia_transport *t = Gaia_TransportFindService(gaia_transport_test, &index);
    return (t && t->state == GAIA_TRANSPORT_CONNECTED);
}


extern bool GaiaTransport_TestHostDisconnect(void)
{
    gaia_transport_index index = 0;
    gaia_transport *t = Gaia_TransportFindService(gaia_transport_test, &index);
    if (t && t->state == GAIA_TRANSPORT_CONNECTED)
    {
        gaia_transport_test_t *tt = (gaia_transport_test_t *)t;

        Gaia_TransportDisconnectInd(t);

        /* Clear and close pipes */
        SourceDrop(StreamSourceFromSink(tt->pipe_host),   SourceSize(StreamSourceFromSink(tt->pipe_host)));
        SourceDrop(StreamSourceFromSink(tt->pipe_device), SourceSize(StreamSourceFromSink(tt->pipe_device)));
        SourceClose(StreamSourceFromSink(tt->pipe_host));
        SourceClose(StreamSourceFromSink(tt->pipe_device));
        SinkClose(tt->pipe_host);
        SinkClose(tt->pipe_device);

        return TRUE;
    }
    else
        return FALSE;
}


extern Sink GaiaTransport_TestHostToDeviceSink(void)
{
    gaia_transport_index index = 0;
    gaia_transport *t = Gaia_TransportFindService(gaia_transport_test, &index);
    gaia_transport_test_t *tt = (gaia_transport_test_t *)t;
    return (t && t->state == GAIA_TRANSPORT_CONNECTED) ? tt->pipe_device : 0;
}


extern Source GaiaTransport_TestDeviceToHostSource(void)
{
    gaia_transport_index index = 0;
    gaia_transport *t = Gaia_TransportFindService(gaia_transport_test, &index);
    gaia_transport_test_t *tt = (gaia_transport_test_t *)t;
    return (t && t->state == GAIA_TRANSPORT_CONNECTED) ? StreamSourceFromSink(tt->pipe_device) : 0;
}

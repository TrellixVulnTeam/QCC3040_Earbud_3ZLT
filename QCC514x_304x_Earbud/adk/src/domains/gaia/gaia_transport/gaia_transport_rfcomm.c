/*****************************************************************
Copyright (c) 2011 - 2020 Qualcomm Technologies International, Ltd.
*/

#define DEBUG_LOG_MODULE_NAME gaia_transport
#include <logging.h>
#include <pmalloc.h>

#include "gaia.h"
#include "gaia_transport.h"

#include "app_handover_if.h"

#include "connection_abstraction.h"
#include <source.h>
#include <sink.h>
#include <stdlib.h>
#include <stream.h>
#include <panic.h>
#include <connection.h>
#include <pmalloc.h>
#include <vm.h>


#define GAIA_TRANSPORT_RFCOMM_DEFAULT_PROTOCOL_VERSION  (3)
#define GAIA_TRANSPORT_RFCOMM_MAX_PROTOCOL_VERSION      (4)

#define GAIA_TRANSPORT_RFCOMM_8BIT_LENGTH_MAX_PKT_SIZE  (254)

#define GAIA_TRANSPORT_RFCOMM_DEFAULT_TX_PKT_SIZE     (48)
#define GAIA_TRANSPORT_RFCOMM_V4_MAX_TX_PKT_SIZE      (1860)
#define GAIA_TRANSPORT_RFCOMM_V3_MAX_TX_PKT_SIZE      (48)    /* Keep this low as packets to send are malloc'ed at the moment */

#define GAIA_TRANSPORT_RFCOMM_DEFAULT_RX_PKT_SIZE     (48)
#define GAIA_TRANSPORT_RFCOMM_V4_MAX_RX_PKT_SIZE      (1600)
#define GAIA_TRANSPORT_RFCOMM_V3_MAX_RX_PKT_SIZE      (254)
#define GAIA_TRANSPORT_RFCOMM_V4_OPT_RX_PKT_SIZE      (850)
#define GAIA_TRANSPORT_RFCOMM_V3_OPT_RX_PKT_SIZE      (254)

#define GAIA_TRANSPORT_RFCOMM_MAX_RX_PENDING_PKTS     (2)

#define GAIA_RFCOMM_REGISTER_SERVICE_RECORD(task, rec_size, rec)    ConnectionRegisterServiceRecord(task, rec_size, rec);
#define GAIA_RFCOMM_DEREGISTER_SERVICE_RECORD(task, handle)         ConnectionUnregisterServiceRecord(task, handle);
#define IS_SDP_STATUS_SUCCESS(status) (status == sds_status_success)

typedef struct
{
    gaia_transport common;
    uint8 channel;          /*!< RFCOMM channel used by this transport. */
    Sink sink;              /*!< Stream sink of this transport. */
    uint32 service_handle;  /*!< Service record handle. */
    unsigned max_tx_size:12;
    unsigned protocol_version:4;
    uint8  rx_packets_pending;
    uint16 rx_data_pending;
    uint8 *tx_pkt_claimed;
    uint16 tx_pkt_claimed_size;
} gaia_transport_rfcomm_t;

typedef struct
{
    uint8 channel;
    uint16 max_tx_size;
    uint8 protocol_version;
} gaia_transport_rfcomm_marshalled_t;

static void gaiaTransport_RfcommHandleMessage(Task task, MessageId id, Message message);

static const uint8 gaia_transport_rfcomm_service_record[] =
{
    0x09, 0x00, 0x01,           /*  0  1  2  ServiceClassIDList(0x0001) */
    0x35,   17,                 /*  3  4     DataElSeq 17 bytes */
    0x1C, 0x00, 0x00, 0x11, 0x07, 0xD1, 0x02, 0x11, 0xE1, 0x9B, 0x23, 0x00, 0x02, 0x5B, 0x00, 0xA5, 0xA5,
                                /*  5 .. 21  UUID GAIA (0x00001107-D102-11E1-9B23-00025B00A5A5) */
    0x09, 0x00, 0x04,           /* 22 23 24  ProtocolDescriptorList(0x0004) */
    0x35,   12,                 /* 25 26     DataElSeq 12 bytes */
    0x35,    3,                 /* 27 28     DataElSeq 3 bytes */
    0x19, 0x01, 0x00,           /* 29 30 31  UUID L2CAP(0x0100) */
    0x35,    5,                 /* 32 33     DataElSeq 5 bytes */
    0x19, 0x00, 0x03,           /* 34 35 36  UUID RFCOMM(0x0003) */
    0x08, SPP_DEFAULT_CHANNEL,  /* 37 38     uint8 RFCOMM channel */
#define GAIA_RFCOMM_SR_CH_IDX (38)
    0x09, 0x00, 0x06,           /* 39 40 41  LanguageBaseAttributeIDList(0x0006) */
    0x35,    9,                 /* 42 43     DataElSeq 9 bytes */
    0x09,  'e',  'n',           /* 44 45 46  Language: English */
    0x09, 0x00, 0x6A,           /* 47 48 49  Encoding: UTF-8 */
    0x09, 0x01, 0x00,           /* 50 51 52  ID base: 0x0100 */
    0x09, 0x01, 0x00,           /* 53 54 55  ServiceName 0x0100, base + 0 */
    0x25,   4,                  /* 56 57     String length 4 */
    'G', 'A', 'I', 'A',         /* 58 59 60 61  "GAIA" */
};

static const uint8 gaia_transport_spp_service_record[] =
{
    0x09, 0x00, 0x01,           /*  0  1  2  ServiceClassIDList(0x0001) */
    0x35,    3,                 /*  3  4     DataElSeq 3 bytes */
    0x19, 0x11, 0x01,           /*  5  6  7  UUID SerialPort(0x1101) */
    0x09, 0x00, 0x04,           /*  8  9 10  ProtocolDescriptorList(0x0004) */
    0x35,   12,                 /* 11 12     DataElSeq 12 bytes */
    0x35,    3,                 /* 13 14     DataElSeq 3 bytes */
    0x19, 0x01, 0x00,           /* 15 16 17  UUID L2CAP(0x0100) */
    0x35,    5,                 /* 18 19     DataElSeq 5 bytes */
    0x19, 0x00, 0x03,           /* 20 21 22  UUID RFCOMM(0x0003) */
    0x08, SPP_DEFAULT_CHANNEL,  /* 23 24     uint8 RFCOMM channel */
#define GAIA_SR_CH_IDX (24)
    0x09, 0x00, 0x06,           /* 25 26 27  LanguageBaseAttributeIDList(0x0006) */
    0x35,    9,                 /* 28 29     DataElSeq 9 bytes */
    0x09,  'e',  'n',           /* 30 31 32  Language: English */
    0x09, 0x00, 0x6A,           /* 33 34 35  Encoding: UTF-8 */
    0x09, 0x01, 0x00,           /* 36 37 38  ID base: 0x0100 */
    0x09, 0x01, 0x00,           /* 39 40 41  ServiceName 0x0100, base + 0 */
    0x25,   4,                  /* 42 43     String length 4 */
     'G',  'A',  'I',  'A',     /* 44 45 46 47 "GAIA" */
    0x09, 0x00, 0x09,           /* 48 49 50  BluetoothProfileDescriptorList(0x0009) */
    0x35, 0x08,                 /* 51 52     DataElSeq 8 bytes [List size] */
    0x35, 0x06,                 /* 53 54     DataElSeq 6 bytes [List item] */
    0x19, 0x11, 0x01,           /* 55 56 57  UUID SerialPort(0x1101) */
    0x09, 0x01, 0x02,           /* 58 59 60  SerialPort Version (0x0102) */
};


/*! @brief Send a GAIA packet over RFCOMM
 */
static bool gaiaTransport_RfcommSendPacket(gaia_transport *t, uint16 vendor_id, uint16 command_id,
                                           uint8 status, uint16 size_payload, const void *payload)
{
    gaia_transport_rfcomm_t *tr = (gaia_transport_rfcomm_t *)t;
    uint16 trans_info = tr->channel;
    const uint16 pkt_length = Gaia_TransportCommonCalcTxPacketLength(size_payload, status);
    const uint16 trans_space = TransportMgrGetAvailableSpace(transport_mgr_type_rfcomm, trans_info);
    if (trans_space >= pkt_length)
    {
        uint8 *pkt_buf = TransportMgrClaimData(transport_mgr_type_rfcomm, trans_info, pkt_length);
        if (pkt_buf)
        {
            /* Build packet into buffer */
            Gaia_TransportCommonBuildPacket(tr->protocol_version, pkt_buf, pkt_length, vendor_id, command_id, status, size_payload, payload);

            /* Send packet */
            if (TransportMgrDataSend(transport_mgr_type_rfcomm, trans_info, pkt_length))
            {
                DEBUG_LOG_VERBOSE("gaiaTransportRfcommSendPacket, sending, vendor_id %u, command_id %u, pkt_length %u", vendor_id, command_id, pkt_length);
                DEBUG_LOG_DATA_V_VERBOSE(pkt_buf, pkt_length);
                return TRUE;
            }
        }
    }
    else
    {
        DEBUG_LOG_ERROR("gaiaTransportRfcommSendPacket, not enough space %u", pkt_length);
        Gaia_TransportErrorInd(t, GAIA_TRANSPORT_INSUFFICENT_BUFFER_SPACE);
    }

    return FALSE;
}


static bool gaiaTransport_RfcommProcessCommand(gaia_transport *t, uint16 pkt_size, uint16 vendor_id, uint16 command_id, uint16 size_payload, const uint8 *payload)
{
    gaia_transport_rfcomm_t *tr = (gaia_transport_rfcomm_t *)t;
    tr->rx_data_pending += pkt_size;
    tr->rx_packets_pending += 1;
    Gaia_ProcessCommand(t, vendor_id, command_id, size_payload, payload);
    return tr->rx_packets_pending < GAIA_TRANSPORT_RFCOMM_MAX_RX_PENDING_PKTS;
}


/*! @brief Received GAIA packet over RFCOMM
 */
static void gaiaTransport_RfcommReceivePacket(gaia_transport_rfcomm_t *tr)
{
    if (tr->rx_data_pending == 0)
    {
        const uint16 trans_info = tr->channel;
        const uint16 data_length = TransportMgrGetAvailableDataSize(transport_mgr_type_rfcomm, trans_info);
        const uint8 *data_buf = (const uint8 *)TransportMgrReadData(transport_mgr_type_rfcomm, trans_info);

        if (data_length && data_buf)
        {
            DEBUG_LOG_VERBOSE("gaiaTransportRfcommReceivePacket, channel %u, data_length %u", trans_info, data_length);
            Gaia_TransportCommonReceivePacket(&tr->common, tr->protocol_version, data_length, data_buf, gaiaTransport_RfcommProcessCommand);
        }
        else
            DEBUG_LOG_V_VERBOSE("gaiaTransportRfcommReceivePacket, channel %u, data_length %u", trans_info, data_length);
    }
    else
        DEBUG_LOG_WARN("gaiaTransportRfcommReceivePacket, receive data being processed");
}

/*! @brief Dispose the remaining data
 */
static void gaiaTransport_RfcommFlushInput(gaia_transport_rfcomm_t *tr)
{
    const uint16 trans_info = tr->channel;
    const uint16 data_length = TransportMgrGetAvailableDataSize(transport_mgr_type_rfcomm, trans_info);
    DEBUG_LOG_VERBOSE("gaiaTransport_RfcommFlushInput, Flushing data_length %u", data_length);
    if (data_length)
    {
        TransportMgrDataConsumed(transport_mgr_type_rfcomm, trans_info, data_length);
    }
}

/*! @brief Received GAIA packet has now been handled by upper layers
 */
static void gaiaTransport_RfcommPacketHandled(gaia_transport *t, uint16 size_payload, const void *payload)
{
    gaia_transport_rfcomm_t *tr = (gaia_transport_rfcomm_t *)t;
    PanicNull(tr);
    UNUSED(payload);

    /* Decrement number of packets pending */
    PanicFalse(tr->rx_packets_pending > 0);
    tr->rx_packets_pending -= 1;

    DEBUG_LOG_VERBOSE("gaiaTransport_RfcommPacketHandled, size %u, remaining %d", size_payload, tr->rx_packets_pending);

    /* Wait until all packets have been processed before removing from buffer as we can't be
     * certain that packets will be handled in order they are received */
    if (tr->rx_packets_pending == 0)
    {
        DEBUG_LOG_VERBOSE("gaiaTransport_RfcommPacketHandled, all data processed");

        /* Inform transport manager we've consumed the data up to end of packet */
        TransportMgrDataConsumed(transport_mgr_type_rfcomm, tr->channel, tr->rx_data_pending);
        tr->rx_data_pending = 0;

        /* Flush the remaining data if the transport has already been disconnected */
        if(!Gaia_TransportIsConnected(t))
        {
            gaiaTransport_RfcommFlushInput(tr);
        }

        /* Check if more data has arrived since we started processing */
        gaiaTransport_RfcommReceivePacket(tr);
    }
}

/*! @brief Get the available space size (in bytes) in the stream buffer
 *
 *  @param[in] t Pointer to transport
 * 
 *  @return The size of available payload space in bytes.
 */
static uint16 gaiaTransport_RfcommGetPacketSpace(gaia_transport *t)
{
    gaia_transport_rfcomm_t *tr = (gaia_transport_rfcomm_t *)t;
    PanicNull(tr);
    uint16 trans_info = tr->channel;
    uint16 available_payload_space = 0;
    uint16 trans_space = TransportMgrGetAvailableSpace(transport_mgr_type_rfcomm, trans_info);
    uint16 header_space = Gaia_TransportCommonCalcPacketHeaderLength();

    /* Ensure that the packet size does not exceed the protocol (v3) limit. */
    if (tr->protocol_version < 4)
        trans_space = MIN(trans_space, GAIA_TRANSPORT_RFCOMM_8BIT_LENGTH_MAX_PKT_SIZE);

    /* Also, make sure that it does not exceed the configured max Tx packet size. */
    trans_space = MIN(trans_space, tr->max_tx_size);

    if (header_space < trans_space)
        available_payload_space = trans_space - header_space;

    return available_payload_space;
}

/*! @brief Create a packet with the specified payload size in the stream buffer
 *
 *  @param[in] t Pointer to transport
 *  @param[in] vendor_id  GAIA Vendor ID.
 *  @param[in] command_id GAIA Command ID (which is Feature-ID, PDU-Type and PDU-specific-ID).
 *  @param[in] size_payload Size of payload to be claimed.
 * 
 *  @return A pointer to the start of the payload in the stream buffer.
 */
static uint8* gaiaTransport_RfcommCreatePacket(gaia_transport *t, const uint16 vendor_id, const uint16 command_id, const uint16 size_payload)
{
    gaia_transport_rfcomm_t *tr = (gaia_transport_rfcomm_t *)t;
    PanicNull(tr);
    uint16 trans_info = tr->channel;
    tr->tx_pkt_claimed_size = Gaia_TransportCommonCalcTxPacketLength(size_payload, GAIA_STATUS_NONE);
    tr->tx_pkt_claimed = TransportMgrClaimData(transport_mgr_type_rfcomm, trans_info, tr->tx_pkt_claimed_size);

    if (tr->tx_pkt_claimed != NULL)
        return Gaia_TransportCommonSetPacketHeader(tr->protocol_version, tr->tx_pkt_claimed, tr->tx_pkt_claimed_size, vendor_id, command_id, size_payload, FALSE);

    /* It fails if the sink becomes invalid (e.g. the link is disconnected). */
    DEBUG_LOG_WARN("gaiaTransport_RfcommCreatePacket, Failed to claim %u bytes of space (payload size:%u) in the buffer!", tr->tx_pkt_claimed_size, size_payload);
    tr->tx_pkt_claimed_size = 0;
    return NULL;
}

/*! @brief Flush a packet in the stream buffer
 *
 *  @param[in] t Pointer to transport
 *  @param[in] size_payload Size of payload to be claimed.
 *  @param[in] payload Pointer to the start of the payload.
 * 
 *  @return TRUE if the packet is flushed successfully, otherwise FALSE.
 */
static bool gaiaTransport_RfcommFlushPacket(gaia_transport *t, const uint16 size_payload, const uint8 *payload)
{
    UNUSED(payload);

    gaia_transport_rfcomm_t *tr = (gaia_transport_rfcomm_t *)t;
    PanicNull(tr);
    uint16 trans_info = tr->channel;
    uint16 pkt_length = Gaia_TransportCommonCalcTxPacketLength(size_payload, GAIA_STATUS_NONE);

    if (pkt_length != tr->tx_pkt_claimed_size)
    {
        PanicFalse(pkt_length < tr->tx_pkt_claimed_size);
        Gaia_TransportCommonUpdatePacketLength(tr->tx_pkt_claimed, pkt_length);
    }
    tr->tx_pkt_claimed = NULL;
    tr->tx_pkt_claimed_size = 0;

    /* Send packet */
    if (transport_mgr_status_success == TransportMgrDataSend(transport_mgr_type_rfcomm, trans_info, pkt_length))
    {
        DEBUG_LOG_VERBOSE("gaiaTransport_RfcommFlushPacket, Sending, pkt_length:%u, payload_size:%u", pkt_length, size_payload);
        return TRUE;
    }

    /* It fails if the sink becomes invalid (e.g. the link is disconnected). */
    DEBUG_LOG_WARN("gaiaTransport_RfcommFlushPacket, Failed to send, pkt_length:%u, payload_size:%u", pkt_length, size_payload);
    return FALSE;
}


/*! @brief Utility function to construct a GAIA SDP record
 *
 *  @param record The constant record to use as a base
 *  @param size_record The size of the base record
 *  @param channel_offset The channel offset in the base record
 *  @param channel The channel to advertise in the SDP record
 */
static const uint8 *gaiaTransport_RfcommAllocateServiceRecord(const uint8 *record, uint16 size_record, uint8 channel_offset, uint8 channel)
{
    uint8 *sr;

    /* TODO: Parse service record to find location of RFCOMM server channel number, rather
     * than using pre-defined offset
     * see SdpParseGetMultipleRfcommServerChannels() & SdpParseInsertRfcommServerChannel()
     */

    /* If channel in record matches, nothing needs to be done, use const version */
    if (channel == record[channel_offset])
        return record;

    /* Allocate a dynamic record */
    sr = PanicUnlessMalloc(size_record);

    /* Copy in the record and set the channel */
    memcpy(sr, record, size_record);
    sr[channel_offset] = channel;
    return (const uint8 *)sr;
}


/*! @brief Register SDP record for transport
 *
 *  @param t Pointer to transport
 *  @param channel The channel to advertise in the SDP record
 */
static void gaiaTransport_RfcommSdpRegister(gaia_transport_rfcomm_t *tr)
{
    const uint8 *sr;
    uint16 size_of_rec;

    if (tr->common.type == gaia_transport_rfcomm)
    {
        /* Default to use const record */
        size_of_rec = sizeof(gaia_transport_rfcomm_service_record);
        sr = gaiaTransport_RfcommAllocateServiceRecord(gaia_transport_rfcomm_service_record, size_of_rec, GAIA_RFCOMM_SR_CH_IDX, tr->channel);
    }
    else
    {
        /* Default to use const record */
        size_of_rec = sizeof(gaia_transport_spp_service_record);
        sr = gaiaTransport_RfcommAllocateServiceRecord(gaia_transport_spp_service_record, size_of_rec, GAIA_SR_CH_IDX, tr->channel);
    }

    DEBUG_LOG_INFO("gaiaTransportRfcommSdpRegister, channel %u", tr->channel);

    /* Register the SDP record */
    GAIA_RFCOMM_REGISTER_SERVICE_RECORD(&tr->common.task, size_of_rec, sr);
}


/*! @brief Reset the parameters to the default values
 *
 *  @param t Pointer to transport
 */
static void gaiaTransport_RfcommResetParams(gaia_transport_rfcomm_t *tr)
{
    PanicNull(tr);

    /* Initialise default parameters */
    tr->max_tx_size         = GAIA_TRANSPORT_RFCOMM_DEFAULT_TX_PKT_SIZE;
    tr->protocol_version    = GAIA_TRANSPORT_RFCOMM_DEFAULT_PROTOCOL_VERSION;
    tr->tx_pkt_claimed      = NULL;
    tr->tx_pkt_claimed_size = 0;
}


/*! @brief Called from GAIA transport to start RFCOMM service
 *
 *  @result Pointer to transport instance
 */
static void gaiaTransport_RfcommStartService(gaia_transport *t)
{
    PanicNull(t);
    gaia_transport_rfcomm_t *tr = (gaia_transport_rfcomm_t *)t;
    DEBUG_LOG_INFO("gaiaTransportRfcommStartService");

    /* Initialise task */
    t->task.handler = gaiaTransport_RfcommHandleMessage;

    /* Initialise default parameters */
    gaiaTransport_RfcommResetParams(tr);

    /* Register with transport manager */
    transport_mgr_link_cfg_t link_cfg;
    link_cfg.type = transport_mgr_type_rfcomm;
    link_cfg.trans_info.non_gatt_trans.trans_link_id = SPP_DEFAULT_CHANNEL;
    TransportMgrRegisterTransport(&t->task, &link_cfg);

    /* Wait for TRANSPORT_MGR_REGISTER_CFM before informing GAIA */
}


/*! @brief Called from GAIA transport to stop RFCOMM service
 *
 *  @param Pointer to transport instance
 */
static void gaiaTransport_RfcommStopService(gaia_transport *t)
{
    PanicNull(t);
    gaia_transport_rfcomm_t *tr = (gaia_transport_rfcomm_t *)t;

    /* Only allow stopping service in started state (i.e not connected) */
    if (t->state == GAIA_TRANSPORT_STARTED)
    {
        DEBUG_LOG_INFO("gaiaTransportRfcommStopService, stopping");
        t->state = GAIA_TRANSPORT_STOPPING;

        /* Unregister with transport manager */
        TransportMgrDeRegisterTransport(&tr->common.task, transport_mgr_type_rfcomm, tr->channel);
    }
    else
    {
     DEBUG_LOG_WARN("gaiaTransportRfcommStopService, can't stop service in state %u", t->state);
     Gaia_TransportStopServiceCfm(&tr->common, FALSE);
    }
}


static void gaiaTransport_RfcommDisconnectReq(gaia_transport *t)
{
    gaia_transport_rfcomm_t *tr = (gaia_transport_rfcomm_t *)t;
    PanicNull(tr);
    DEBUG_LOG_INFO("gaiaTransportRfcommDisconnectReq, sink %04x", tr->sink);

    /* Initiate disconnect */
    TransportMgrDisconnect(transport_mgr_type_rfcomm, tr->sink);
}


static void gaiaTransport_RfcommError(gaia_transport *t)
{
    gaia_transport_rfcomm_t *tr = (gaia_transport_rfcomm_t *)t;
    PanicNull(tr);
    DEBUG_LOG_ERROR("gaiaTransportRfcommError, sink %04x", tr->sink);

    /* Initiate disconnect */
    TransportMgrDisconnect(transport_mgr_type_rfcomm, tr->sink);
}


static uint8 gaiaTransport_RfcommFeatures(gaia_transport *t)
{
    gaia_transport_rfcomm_t *tr = (gaia_transport_rfcomm_t *)t;
    PanicNull(tr);
    DEBUG_LOG_DEBUG("gaiaTransportRfcommFeatures");

#ifdef ENABLE_GAIA_DYNAMIC_HANDOVER
    /* RFCOMM supports dynamic handover and should be treated like a profile */
    return GAIA_TRANSPORT_FEATURE_DYNAMIC_HANDOVER | GAIA_TRANSPORT_FEATURE_PROFILE | GAIA_TRANSPORT_FEATURE_MULTIPOINT;
#else
    return GAIA_TRANSPORT_FEATURE_STATIC_HANDOVER | GAIA_TRANSPORT_FEATURE_PROFILE | GAIA_TRANSPORT_FEATURE_MULTIPOINT;
#endif
}


static bool gaiaTransport_RfcommGetInfo(gaia_transport *t, gaia_transport_info_key_t key, uint32 *value)
{
    gaia_transport_rfcomm_t *tr = (gaia_transport_rfcomm_t *)t;
    PanicNull(tr);

    switch (key)
    {
        case GAIA_TRANSPORT_MAX_TX_PACKET:
        case GAIA_TRANSPORT_OPTIMUM_TX_PACKET:
            *value = tr->max_tx_size;
            break;
        case GAIA_TRANSPORT_MAX_RX_PACKET:
            if (tr->protocol_version >= 4)
                *value = GAIA_TRANSPORT_RFCOMM_V4_MAX_RX_PKT_SIZE;
            else
                *value = GAIA_TRANSPORT_RFCOMM_V3_MAX_RX_PKT_SIZE;
            break;
        case GAIA_TRANSPORT_OPTIMUM_RX_PACKET:
            if (tr->protocol_version >= 4)
                *value = GAIA_TRANSPORT_RFCOMM_V4_OPT_RX_PKT_SIZE;
            else
                *value = GAIA_TRANSPORT_RFCOMM_V3_OPT_RX_PKT_SIZE;
            break;
        case GAIA_TRANSPORT_TX_FLOW_CONTROL:
        case GAIA_TRANSPORT_RX_FLOW_CONTROL:
            *value =1;
            break;

        case GAIA_TRANSPORT_PROTOCOL_VERSION:
            *value = tr->protocol_version;
            break;

        case GAIA_TRANSPORT_PAYLOAD_SIZE:
            *value = tr->max_tx_size - Gaia_TransportCommonCalcPacketHeaderLength();
            break;

        default:
            DEBUG_LOG_WARN("gaiaTransport_RfcommGetInfo, unknown key %u", key);
            return FALSE;
    }
    DEBUG_LOG_DEBUG("gaiaTransport_RfcommGetInfo, key %u, value %u", key, *value);
    return TRUE;
}


static bool gaiaTransport_RfcommSetParameter(gaia_transport *t, gaia_transport_info_key_t key, uint32 *value)
{
    gaia_transport_rfcomm_t *tr = (gaia_transport_rfcomm_t *)t;
    PanicNull(tr);
    DEBUG_LOG_DEBUG("gaiaTransport_RfcommSetParameter, key %u, value %u", key, *value);

    switch (key)
    {
        case GAIA_TRANSPORT_MAX_TX_PACKET:
            if (tr->protocol_version >= 4)
                tr->max_tx_size = MIN(*value, GAIA_TRANSPORT_RFCOMM_V4_MAX_TX_PKT_SIZE);
            else
                tr->max_tx_size = MIN(*value, GAIA_TRANSPORT_RFCOMM_V3_MAX_TX_PKT_SIZE);
            *value = tr->max_tx_size;
            break;

        case GAIA_TRANSPORT_PROTOCOL_VERSION:
            if ((*value >= GAIA_TRANSPORT_RFCOMM_DEFAULT_PROTOCOL_VERSION) && (*value <= GAIA_TRANSPORT_RFCOMM_MAX_PROTOCOL_VERSION))
                tr->protocol_version = *value;
            *value = tr->protocol_version;
            break;

        default:
            /* Ignore any request to set parameters, just return current value */
            return gaiaTransport_RfcommGetInfo(t, key, value);
    }

    return TRUE;
}

/*! @brief Veto handover if the transport is in transitional state
 *
 *  @param[in] t    Pointer to transport instance.
 *
 *  @return TRUE: Veto the handover.
 *          FALSE: Agree to proceeding the handover.
 */
static bool gaiaTransport_RfcommHandoverVeto(gaia_transport *t)
{
    gaia_transport_rfcomm_t *tr = (gaia_transport_rfcomm_t*) t;
    PanicNull(t);

    /* Veto if pending messages */
    if (MessagesPendingForTask(&t->task, NULL))
    {
        DEBUG_LOG_INFO("gaiaTransport_RfcommHandoverVeto, veto as messages pending for task");
        return TRUE;
    }

    /* Veto if received packet being processed */
    if (tr->rx_packets_pending)
    {
        DEBUG_LOG_INFO("gaiaTransport_RfcommHandoverVeto, veto as connected with %u packets pending", tr->rx_packets_pending);
        return TRUE;
    }

    switch (tr->common.state)
    {
        case GAIA_TRANSPORT_STARTED:
            DEBUG_LOG_DEBUG("gaiaTransport_RfcommHandoverVeto, not connected");
            break;

        case GAIA_TRANSPORT_CONNECTED:
            DEBUG_LOG_DEBUG("gaiaTransport_RfcommHandoverVeto, connected");
            break;

        case GAIA_TRANSPORT_PRE_COMMIT_PRIMARY:
        case GAIA_TRANSPORT_PRE_COMMIT_SECONDARY:
            DEBUG_LOG_DEBUG("gaiaTransport_RfcommHandoverVeto, pre-commit");
            break;

        default:
            DEBUG_LOG_INFO("gaiaTransport_RfcommHandoverVeto, veto as state %u", t->state);
            return TRUE;
    }

    return FALSE;
}

/*! @brief Marshal the data associated with the specified connection
 *
 *  @param[in] t            Pointer to transport instance.
 *  @param[in] bd_addr      Bluetooth address of the link to be marshalled.
 *  @param[out] marshal_obj Holds address of data to be marshalled.
 *
 *  @return TRUE: Required data has been copied to the marshal_obj.
 *          FALSE: No data is required to be marshalled. marshal_obj is set to NULL.
 */
static bool gaiaTransport_RfcommHandoverMarshal(gaia_transport *t, uint8 *buf, uint16 buf_length, uint16 *written)
{
    gaia_transport_rfcomm_t *tr = (gaia_transport_rfcomm_t*) t;
    PanicNull(t);

    if (buf_length >= sizeof(gaia_transport_rfcomm_marshalled_t))
    {
        gaia_transport_rfcomm_marshalled_t *md = PanicUnlessNew(gaia_transport_rfcomm_marshalled_t);
        md->channel          = tr->channel;
        md->max_tx_size      = tr->max_tx_size;
        md->protocol_version = tr->protocol_version;
        memcpy(buf, md, sizeof(gaia_transport_rfcomm_marshalled_t));
        free(md);

        *written = sizeof(gaia_transport_rfcomm_marshalled_t);

        DEBUG_LOG_DEBUG("gaiaTransport_RfcommHandoverMarshal, marshalled");
        return TRUE;
    }
    else
    {
        DEBUG_LOG_WARN("gaiaTransport_RfcommHandoverMarshal, not marshalled");
        return FALSE;
    }
}

/*! @brief Unmarshal the data associated with the specified connection
 *
 *  @param[in] t             Pointer to transport instance.
 *  @param[in] bd_addr       Bluetooth address of the link to be unmarshalled.
 *  @param[in] unmarshal_obj Address of the unmarshalled object.
 *
 *  @return TRUE: Unmarshalled the data successfully.
 *                The caller can free the marshalling object.
 *          FALSE: Failed to unmarshal the data.
 */
static bool gaiaTransport_RfcommHandoverUnmarshal(gaia_transport *t, const uint8 *buf, uint16 buf_length, uint16 *consumed)
{
    gaia_transport_rfcomm_t *tr = (gaia_transport_rfcomm_t*) t;
    PanicNull(tr);

    if (buf_length >= sizeof(gaia_transport_rfcomm_marshalled_t))
    {
        gaia_transport_rfcomm_marshalled_t md;
        memcpy(&md, buf, sizeof(gaia_transport_rfcomm_marshalled_t));
        if (md.channel == tr->channel)
        {
            tr->max_tx_size         = md.max_tx_size;
            tr->protocol_version    = md.protocol_version;
            *consumed = sizeof(gaia_transport_rfcomm_marshalled_t);

            DEBUG_LOG_DEBUG("gaiaTransport_RfcommHandoverUnmarshal, unmarshalled");
            return TRUE;
        }
        else
        {
            /* RFCOMM channel number doesn't match, so don't unmarshall into this instance */
            DEBUG_LOG_DEBUG("gaiaTransport_RfcommHandoverUnmarshal, wrong server channel, not unmarshalled");
            return FALSE;
        }
    }
    else
    {
        DEBUG_LOG_WARN("gaiaTransport_RfcommHandoverUnmarshal, not unmarshalled");
        return FALSE;
    }
}

/*! @brief Commits to the specified role
 *
 *  @param[in] t            Pointer to transport instance.
 *  @param[in] is_primary   TRUE if device role is primary, else secondary.
 */
static void gaiaTransport_RfcommHandoverCommit(gaia_transport *t, bool is_primary)
{
    gaia_transport_rfcomm_t *tr = (gaia_transport_rfcomm_t *)t;
    if (is_primary)
    {
        /* Get Sink using RFCOMM server channel */
        tr->sink = StreamRfcommSinkFromServerChannel(&t->tp_bd_addr, tr->channel);

        /* TODO: Most of the stuff below should go into transport_mgr */

        /* Set the task for connection */
        uint16 conn_id = SinkGetRfcommConnId(tr->sink);
        PanicFalse(VmOverrideRfcommConnContext(conn_id, (conn_context_t)&tr->common.task));

        DEBUG_LOG_DEBUG("gaiaTransport_RfcommHandoverCommit, primary, sink %u, channel %u, conn_id %u",
                        tr->sink, tr->channel, conn_id);

        /* Stitch the RFCOMM sink and the Transport Manager task.
        * This just does 'MessageStreamTaskFromSink(sink, task)'. */
        TransportMgrConfigureSink(tr->sink);

        /* Configure Sink */
        SinkConfigure(tr->sink, VM_SINK_MESSAGES, VM_MESSAGES_ALL);

        /* Create link data object in Transport Manager */
        transport_mgr_link_cfg_t link_cfg;

        link_cfg.type = transport_mgr_type_rfcomm;
        link_cfg.trans_info.non_gatt_trans.trans_link_id = tr->channel;

        if (TransportMgrCreateTransLinkData(&tr->common.task, link_cfg, tr->sink))
        {
            /* Existing RFCOMM channel was used for GAIA, deregister SDP record
            * with current RFCOMM server handle so that new can be created. */
            ConnectionUnregisterServiceRecord(&tr->common.task, tr->service_handle);
        }

        Source src = StreamSourceFromSink(tr->sink);
        SourceConfigure(src, STREAM_SOURCE_HANDOVER_POLICY, SOURCE_HANDOVER_ALLOW_WITHOUT_DATA);
    }
    else
    {
        DEBUG_LOG_DEBUG("gaiaTransport_RfcommHandoverCommit, secondary");
    }
}


static void gaiaTransport_RfcommHandoverAbort(gaia_transport *t)
{
    UNUSED(t);
    DEBUG_LOG_DEBUG("gaiaTransport_RfcommHandoverAbort");
}

static void gaiaTransport_RfcommHandoverComplete(gaia_transport *t, bool is_primary)
{
    gaia_transport_rfcomm_t *tr = (gaia_transport_rfcomm_t *)t;
    if (is_primary)
    {
        DEBUG_LOG_DEBUG("gaiaTransport_RfcommHandoverComplete, primary, connected");
        Gaia_TransportConnectInd(&tr->common, TRUE, &t->tp_bd_addr);
    }
    else
        DEBUG_LOG_DEBUG("gaiaTransport_RfcommHandoverComplete, secondary");    
}

static void gaiaTransport_RfcommHandleTransportMgrRegisterCfm(gaia_transport_rfcomm_t *tr, const TRANSPORT_MGR_REGISTER_CFM_T *cfm)
{
    DEBUG_LOG_INFO("gaiaRfcommHandleTransportMgrRegisterCfm, channel %u, status %u",
                   cfm->link_cfg.trans_info.non_gatt_trans.trans_link_id, cfm->status);

    if (cfm->status)
    {
        tr->channel = cfm->link_cfg.trans_info.non_gatt_trans.trans_link_id;
        gaiaTransport_RfcommSdpRegister(tr);
    }
    else
        Gaia_TransportStartServiceCfm(&tr->common, FALSE);
}


static void gaiaTransport_RfcommHandleTransportMgrDeregisterCfm(gaia_transport_rfcomm_t *tr, const TRANSPORT_MGR_DEREGISTER_CFM_T *cfm)
{
    DEBUG_LOG_INFO("gaiaRfcommHandleTransportMgrDeregisterCfm, channel %u, status %u",
                   cfm->trans_link_id, cfm->status);

    /* Unregister SDP record */
    if (tr->service_handle)
    {
        GAIA_RFCOMM_DEREGISTER_SERVICE_RECORD(&tr->common.task, tr->service_handle);
        tr->service_handle = 0;
    }
    else
        Panic();
}


static void gaiaTransport_RfcommHandleTransportMgrLinkCreatedCfm(gaia_transport_rfcomm_t *tr, const TRANSPORT_MGR_LINK_CREATED_CFM_T *cfm)
{
    DEBUG_LOG_INFO("gaiaTransportRfcommHandleTransportMgrLinkCreatedCfm, status %u", cfm->status);

    if (cfm->status)
    {
        tr->sink = cfm->trans_sink;
#if defined(ENABLE_GAIA_DYNAMIC_HANDOVER) && defined(INCLUDE_MIRRORING)
        {
            Source src = StreamSourceFromSink(tr->sink);
            SourceConfigure(src, STREAM_SOURCE_HANDOVER_POLICY, SOURCE_HANDOVER_ALLOW_WITHOUT_DATA);
        }
#endif /* ENABLE_GAIA_DYNAMIC_HANDOVER && INCLUDE_MIRRORING */

        /* Unregister SDP record now that we're connected */
        if (tr->service_handle)
        {
            GAIA_RFCOMM_DEREGISTER_SERVICE_RECORD(&tr->common.task, tr->service_handle);
            tr->service_handle = 0;
        }
    }

    Gaia_TransportConnectInd(&tr->common, cfm->status, &cfm->addr);

    /* Check if any data has already arrived */
    gaiaTransport_RfcommReceivePacket(tr);
}


static void gaiaTransport_RfcommHandleTransportMgrLinkDisconnectedCfm(gaia_transport_rfcomm_t *tr, const TRANSPORT_MGR_LINK_DISCONNECTED_CFM_T *cfm)
{
    DEBUG_LOG_INFO("gaiaTransportRfcommHandleTransportMgrLinkDisconnectedCfm, status %u", cfm->status);

    if (cfm->status)
    {
        /* Only call Gaia_TransportDisconnectInd if ti->rx_data_pending == 0, otherwise transport
         * could be destroyed when there are packets still being processed */
        if (tr->rx_data_pending)
        {
            MESSAGE_MAKE(msg, TRANSPORT_MGR_LINK_DISCONNECTED_CFM_T);
            *msg = *cfm;
            MessageSendConditionally(&tr->common.task, TRANSPORT_MGR_LINK_DISCONNECTED_CFM, msg, &tr->rx_data_pending);
        }
        else
        {
            /* Reset the parameters to initial state. */
            gaiaTransport_RfcommResetParams(tr);

            /* Re-register SDP record */
            gaiaTransport_RfcommSdpRegister(tr);

            /* Ensure any data in RFCOMM buffer is flushed so that stream will be destroyed */
            gaiaTransport_RfcommFlushInput(tr);

            /* Send disconnect indication to framework */
            Gaia_TransportDisconnectInd(&tr->common);

            /* If stream is not closed, StreamConnectDispose should either close the stream or connect to transform for data discard */
            StreamConnectDispose(StreamSourceFromSink(cfm->trans_sink));

        }
    }
}

static void gaiaTransport_RfcommHandleSdpRegisterCfm(gaia_transport_rfcomm_t *tr, uint16 status, uint32 service_handle)
{
    DEBUG_LOG_INFO("gaiaTransport_RfcommHandleSdpRegisterCfm, status %u, state %u", status, tr->common.state);

    if (IS_SDP_STATUS_SUCCESS(status))
    {
        /* Send CFM if service is starting */
        if (tr->common.state == GAIA_TRANSPORT_STARTING)
            Gaia_TransportStartServiceCfm(&tr->common, TRUE);

        tr->service_handle = service_handle;
    }
    else if (tr->common.state == GAIA_TRANSPORT_STARTING)
        Gaia_TransportStartServiceCfm(&tr->common, FALSE);
}


static void gaiaTransport_RfcommHandleSdpUnregisterCfm(gaia_transport_rfcomm_t *tr, uint16 status)
{
    DEBUG_LOG_INFO("gaiaTransport_RfcommHandleSdpUnregisterCfm, status %u, state %u", status, tr->common.state);

    if (tr->common.state == GAIA_TRANSPORT_STOPPING && IS_SDP_STATUS_SUCCESS(status))
    {
        /* Complete unregistered (both SDP and transport manager, so tell GAIA we're done */
        Gaia_TransportStopServiceCfm(&tr->common, TRUE);
    }
}



static void gaiaTransport_RfcommHandleMessage(Task task, MessageId id, Message message)
{
    gaia_transport_rfcomm_t *tr = (gaia_transport_rfcomm_t *)task;

    switch (id)
    {
        case TRANSPORT_MGR_MORE_DATA:
            gaiaTransport_RfcommReceivePacket(tr);
            break;

        case TRANSPORT_MGR_MORE_SPACE:
            break;

        case TRANSPORT_MGR_REGISTER_CFM:
            gaiaTransport_RfcommHandleTransportMgrRegisterCfm(tr, (TRANSPORT_MGR_REGISTER_CFM_T *)message);
            break;

        case TRANSPORT_MGR_DEREGISTER_CFM:
            gaiaTransport_RfcommHandleTransportMgrDeregisterCfm(tr, (TRANSPORT_MGR_DEREGISTER_CFM_T *)message);
            break;

        case TRANSPORT_MGR_LINK_CREATED_CFM:
            gaiaTransport_RfcommHandleTransportMgrLinkCreatedCfm(tr, (TRANSPORT_MGR_LINK_CREATED_CFM_T *)message);
            break;

        case TRANSPORT_MGR_LINK_DISCONNECTED_CFM:
            gaiaTransport_RfcommHandleTransportMgrLinkDisconnectedCfm(tr, (TRANSPORT_MGR_LINK_DISCONNECTED_CFM_T *)message);
            break;

        case CL_SDP_REGISTER_CFM:
            gaiaTransport_RfcommHandleSdpRegisterCfm(tr, (uint16) ((CL_SDP_REGISTER_CFM_T *)message)->status,
                                                     ((CL_SDP_REGISTER_CFM_T *) message)->service_handle);
            break;

        case CL_SDP_UNREGISTER_CFM:
            gaiaTransport_RfcommHandleSdpUnregisterCfm(tr, (uint16) ((CL_SDP_UNREGISTER_CFM_T *)message)->status);
            break;


        default:
            DEBUG_LOG_ERROR("gaiaTransportRfcommHandleMessage, unhandled message MESSAGE:0x%04x", id);
            DEBUG_LOG_DATA_ERROR(message, psizeof(message));
            break;
    }
}


void GaiaTransport_RfcommInit(void)
{
    static const gaia_transport_functions_t functions =
    {
        .service_data_size      = sizeof(gaia_transport_rfcomm_t),
        .start_service          = gaiaTransport_RfcommStartService,
        .stop_service           = gaiaTransport_RfcommStopService,
        .packet_handled         = gaiaTransport_RfcommPacketHandled,
        .send_command_packet    = gaiaTransport_RfcommSendPacket,
        .send_data_packet       = NULL,
        .get_packet_space       = gaiaTransport_RfcommGetPacketSpace,
        .create_packet          = gaiaTransport_RfcommCreatePacket,
        .flush_packet           = gaiaTransport_RfcommFlushPacket,
        .connect_req            = NULL,
        .disconnect_req         = gaiaTransport_RfcommDisconnectReq,
        .set_data_endpoint      = NULL,
        .get_data_endpoint      = NULL,
        .error                  = gaiaTransport_RfcommError,
        .features               = gaiaTransport_RfcommFeatures,
        .get_info               = gaiaTransport_RfcommGetInfo,
        .set_parameter          = gaiaTransport_RfcommSetParameter,
        .handover_veto          = gaiaTransport_RfcommHandoverVeto,
        .handover_marshal       = gaiaTransport_RfcommHandoverMarshal,
        .handover_unmarshal     = gaiaTransport_RfcommHandoverUnmarshal,
        .handover_commit        = gaiaTransport_RfcommHandoverCommit,
        .handover_abort         = gaiaTransport_RfcommHandoverAbort,
        .handover_complete      = gaiaTransport_RfcommHandoverComplete,
    };

    /* Register this transport with GAIA */
    /*Gaia_TransportRegister(gaia_transport_rfcomm, &functions);*/
    Gaia_TransportRegister(gaia_transport_spp, &functions);
}

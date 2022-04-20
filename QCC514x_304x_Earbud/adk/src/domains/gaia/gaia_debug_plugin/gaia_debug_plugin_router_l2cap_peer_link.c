/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       gaia_debug_plugin_router_l2cap_peer_link.c
\brief      Part of the router that manages L2CAP Peer Link (to the Secondary device).
*/

#if defined(INCLUDE_GAIA_PYDBG_REMOTE_DEBUG) && defined(INCLUDE_L2CAP_MANAGER)

#include "gaia_debug_plugin_router_l2cap_peer_link.h"
#include "gaia_debug_plugin_router_private.h"

#include <panic.h>

/* Enable debug log outputs with per-module debug log levels.
 * The log output level for this module can be changed with the PyDbg command:
 *      >>> apps1.log_level("gaia_debug_plugin_router", 3)
 * Where the second parameter value means:
 *      0:ERROR, 1:WARN, 2:NORMAL(= INFO), 3:VERBOSE(= DEBUG), 4:V_VERBOSE(= VERBOSE), 5:V_V_VERBOSE(= V_VERBOSE)
 * See 'logging.h' and PyDbg 'log_level()' command descriptions for details. */
#define DEBUG_LOG_MODULE_NAME gaia_debug_plugin_router
#include <logging.h>

#include "l2cap_manager.h"
#include <bt_device.h>
#include <marshal.h>
#include <message.h>
#include <multidevice.h>
#include <sdp.h>
#include <sink.h>
#include <source.h>
#include <stream.h>



/******************************************************************************
 * Macros/Defines used within this source file.
 ******************************************************************************/
/*! \brief The L2CAP local MTU size (incoming) for the GAIA Debug L2CAP Peer Link. */
#define GAIA_DEBUG_L2CAP_PEER_LINK_L2CAP_MTU_IN_SIZE        (672)

/*! \brief The L2CAP remote MTU size (outcoming) for the GAIA Debug L2CAP Peer Link. */
#define GAIA_DEBUG_L2CAP_PEER_LINK_L2CAP_MTU_OUT_SIZE       (48)


/*! \brief The size of the 'Command' field of the GAIA Debug L2CAP Peer Link header. */
#define GAIA_DEBUG_L2CAP_PEER_LINK_HEADER_CMD_FIELD_SIZE    (1)

/*! \brief The size of the 'Length' field of the GAIA Debug L2CAP Peer Link header. */
#define GAIA_DEBUG_L2CAP_PEER_LINK_HEADER_LENGTH_FIELD_SIZE (2)

/*! \brief The header size of the GAIA Debug L2CAP Peer Link. */
#define GAIA_DEBUG_L2CAP_PEER_LINK_HEADER_SIZE      (GAIA_DEBUG_L2CAP_PEER_LINK_HEADER_CMD_FIELD_SIZE + GAIA_DEBUG_L2CAP_PEER_LINK_HEADER_LENGTH_FIELD_SIZE)


/*! \brief The MTU size of GAIA Debug L2CAP Peer Link messages. */
#define GAIA_DEBUG_L2CAP_PEER_LINK_L2CAP_MESSAGE_MTU_SIZE   (GAIA_DEBUG_L2CAP_PEER_LINK_HEADER_SIZE + GAIA_DEBUG_TRANSPORT_PDU_SIZE)
#if GAIA_DEBUG_L2CAP_PEER_LINK_L2CAP_MTU_IN_SIZE < GAIA_DEBUG_L2CAP_PEER_LINK_L2CAP_MESSAGE_MTU_SIZE
#error The maximum message size of the GAIA Debug L2CAP Link must be smaller than its MTU size!
#endif


typedef enum
{
    gaia_debug_l2cap_peer_link_state_null,                  /*!< Initial state. */

    gaia_debug_l2cap_peer_link_state_registering,           /*!< Registering L2CAP PSM & SDP service record. */
    gaia_debug_l2cap_peer_link_state_disconnected,          /*!< Idle state. */
    gaia_debug_l2cap_peer_link_state_connecting,            /*!< Trying to connect to the peer device. */
    gaia_debug_l2cap_peer_link_state_connected,             /*!< The peer link is live. */
    gaia_debug_l2cap_peer_link_state_disconnecting,         /*!< Orderly disconnection is in progress. */

    gaia_debug_l2cap_peer_link_state_error          = 0xFF  /*!< Error state (unrecoverable == Panic()). */

} gaia_debug_l2cap_peer_link_state_t;



/*
    GAIA Debug L2CAP Peer Link message PDU format:
             0        1        2        3        4       ...       N    (Byte)
        +--------+--------+--------+--------+--------+--------+--------+
        | Command| (LSB)Length(MSB)|          Payload (if any)         |
        +--------+--------+--------+--------+--------+--------+--------+
        |<--- Peer Link Header --->|<------  Peer Link Payload  ------>|
*/
/*! \brief The data struct for the L2CAP Peer Link message PDUs. */
typedef struct
{
    /*! The command field of the message PDU for the GAIA Debug L2CAP Peer Link. */
    uint8   command;

    /*! The length field of the message PDU for the GAIA Debug L2CAP Peer Link.
        (NB: A single 16-bit length field is *intentionally* defined as two
             8-bit octets because this struct can be casted to an element of a
             uint8 array, that does not guarantee a uint16 member variable is
             aligned to the 16-bit memory boundary.) */
    uint8   length_lsb;
    uint8   length_msb;

    /*! The first octet of the payload of the message PDU for the GAIA Debug L2CAP Peer Link. */
    uint8   payload[1];

} gaia_debug_l2cap_peer_link_message_pdu_t;


/*! \brief The task data for the GAIA Debug L2CAP Peer Link sub-module. */
typedef struct
{
    TaskData                                        task;
    l2cap_manager_instance_id                       psm_instance_id;
    gaia_debug_l2cap_peer_link_state_t              state;
    Sink                                            sink;
    Source                                          source;

    const gaia_debug_l2cap_peer_link_functions_t   *functions;

    uint16                                          rcv_buf_size;
    uint8                                          *rcv_buf;

    bool                                            tx_buf_in_use;
    uint16                                          tx_buf_msg_length;
    uint16                                          tx_buf_size;
    uint8                                          *tx_buf;

} gaia_debug_l2cap_peer_link_task_data_t;


/******************************************************************************
 * File-scope variables
 ******************************************************************************/
/*! \brief The task data for GAIA Debug L2CAP Peer Link task.
           Note that the task data is placed in the heap (i.e. malloc'd) rather
           than static, because the Headset application does not need this. */
static gaia_debug_l2cap_peer_link_task_data_t *gaia_debug_peer_link_data = NULL;

/*! \brief The macro that returns the pointer to the task data. */
#define gaiaDebugPlugin_GetL2capPeerLinkTaskData()      (gaia_debug_peer_link_data)



/******************************************************************************
 * Misc. functions.
 ******************************************************************************/
static bool gaiaDebugPlugin_GetPeerBdAddr(tp_bdaddr *tpaddr)
{
    bdaddr self;
    bdaddr primary;
    bdaddr secondary;

    if(appDeviceGetPrimaryBdAddr(&primary) && appDeviceGetMyBdAddr(&self))
    {
        if (BdaddrIsSame(&primary, &self))
        {
            /* The peer device is the Secondary. */
            if (appDeviceGetSecondaryBdAddr(&secondary))
            {
                tpaddr->transport  = TRANSPORT_BREDR_ACL;
                tpaddr->taddr.type = TYPED_BDADDR_PUBLIC;
                tpaddr->taddr.addr = secondary;
                DEBUG_LOG_DEBUG("GaiaDebugPlugin GetPeerBdAddr: %04X-%02X-%06X (Secondary)",
                               secondary.nap, secondary.uap, secondary.lap);
                return TRUE;
            }
        }
        else
        {
            /* The peer device is the Primary. */
            tpaddr->transport  = TRANSPORT_BREDR_ACL;
            tpaddr->taddr.type = TYPED_BDADDR_PUBLIC;
            tpaddr->taddr.addr = primary;
            DEBUG_LOG_DEBUG("GaiaDebugPlugin GetPeerBdAddr: %04X-%02X-%06X (Primary)",
                            primary.nap, primary.uap, primary.lap);
            return TRUE;
        }
    }

    DEBUG_LOG_DEBUG("GaiaDebugPlugin: GetPeerBdAddr: WARNING! Failed to get the peer BD-ADDR!");
    return FALSE;
}


/******************************************************************************
 * Functions for transmitting/receiving data over L2CAP Peer Link.
 ******************************************************************************/
static bool gaiaDebugPlugin_TransmitToPeerDevice(int16 pdu_length, const uint8 *pdu)
{
    gaia_debug_l2cap_peer_link_task_data_t *task_data = gaiaDebugPlugin_GetL2capPeerLinkTaskData();
    PanicNull(task_data);
    Sink sink = task_data->sink;

    DEBUG_LOG_DEBUG("GaiaDebugPlugin TransmitToPeerDevice: (Size:%d, %p)", pdu_length, pdu);

    if (task_data->state != gaia_debug_l2cap_peer_link_state_connected)
    {
        DEBUG_LOG_WARN("GaiaDebugPlugin TransmitToPeerDevice: WARNING! Failed to transmit. Not connected! (State:%d, Size:%d)", task_data->state, pdu_length);
    }
    else if (SinkIsValid(sink))
    {
        uint8 *dest           = SinkMap(sink);
        uint16 size_available = SinkSlack(sink);
        uint16 size_claimed   = SinkClaim(sink, 0);
        uint16 offset = 0;

        PanicNull(dest);
        if (pdu_length < (size_available + size_claimed))
        {
            if (size_claimed < pdu_length)
                offset = SinkClaim(sink, (pdu_length - size_claimed));
            else
                offset = size_claimed;
        }

        if (offset != 0xFFFF)       /* 0xFFFF: Invalid sink offset. */
        {
            uint8 *sink_ptr = (dest + offset - size_claimed);
            PanicNull(sink_ptr);
            memmove(sink_ptr, pdu, pdu_length);
            SinkFlush(sink, pdu_length);
            return TRUE;
        }
        else
            DEBUG_LOG_WARN("GaiaDebugPlugin TransmitToPeerDevice: WARNING! Failed to claim the sink space!");
    }
    else
        DEBUG_LOG_WARN("GaiaDebugPlugin TransmitToPeerDevice: WARNING! The sink is not valid!");

    return FALSE;
}


/*! \brief Put a Peer Link message to the Tx buffer.
    \note  The payload can be NULL. */
static bool gaiaDebugPlugin_PutTransmitBufferToPeerDevice(gaia_debug_l2cap_peer_link_message_command_t peer_link_cmd, const uint16 payload_length, const uint8 *payload)
{
    gaia_debug_l2cap_peer_link_task_data_t *task_data = gaiaDebugPlugin_GetL2capPeerLinkTaskData();
    PanicNull(task_data);
    gaia_debug_l2cap_peer_link_message_pdu_t *msg_pdu = (gaia_debug_l2cap_peer_link_message_pdu_t*) task_data->tx_buf;

    DEBUG_LOG_DEBUG("GaiaDebugPlugin PutTransmitBufferToPeerDevice: (Cmd:%d, Size:%d, %p)", peer_link_cmd, payload_length, payload);
    if (task_data->tx_buf_in_use)
    {
        /* Reject the request as a message has been in the Tx buffer. */
        DEBUG_LOG_WARN("GaiaDebugPlugin PutTransmitBufferToPeerDevice: WARNING! Failed to put a message as the Tx buffer is in use!: (LinkCmd:%d, Size:%d)", peer_link_cmd, payload_length);
    }
    else
    {
        msg_pdu->command = peer_link_cmd;
        msg_pdu->length_lsb = 0;
        msg_pdu->length_msb = 0;
        if (payload != NULL && payload_length != 0)
        {
            if ((GAIA_DEBUG_L2CAP_PEER_LINK_HEADER_SIZE + payload_length) <= task_data->tx_buf_size)
            {
                msg_pdu->length_lsb = (uint8) (payload_length & 0xFF);
                msg_pdu->length_msb = (uint8) (payload_length >> 8);
                memmove(&msg_pdu->payload, payload, payload_length);
                task_data->tx_buf_msg_length = GAIA_DEBUG_L2CAP_PEER_LINK_HEADER_SIZE + payload_length;
                task_data->tx_buf_in_use     = TRUE;
                return TRUE;
            }
            else
            {
                DEBUG_LOG_WARN("GaiaDebugPlugin PutTransmitBufferToPeerDevice: WARNING! The Tx buffer is too small (BufSize:%d < DataSize:%d)", task_data->tx_buf_size, payload_length);
            }
        }
    }
    return FALSE;
}


static bool gaiaDebugPlugin_SendMessageToPeer(gaia_debug_l2cap_peer_link_message_command_t peer_link_cmd, uint16 payload_length, const uint8 *payload)
{
    gaia_debug_l2cap_peer_link_task_data_t *task_data = gaiaDebugPlugin_GetL2capPeerLinkTaskData();
    PanicNull(task_data);

    /* Put the message to the Tx buffer to add the Peer Link PDU header. */
    if (gaiaDebugPlugin_PutTransmitBufferToPeerDevice(peer_link_cmd, payload_length, payload))
    {
        uint16 pdu_length = GAIA_DEBUG_L2CAP_PEER_LINK_HEADER_SIZE + payload_length;

        if (gaiaDebugPlugin_TransmitToPeerDevice(pdu_length, task_data->tx_buf))
        {
            task_data->tx_buf_msg_length = 0;
            task_data->tx_buf_in_use     = FALSE;
            return TRUE;    /* Successfully send the message to the peer device. */
        }
        else
            DEBUG_LOG_WARN("GaiaDebugPlugin SendMessageToPeer: WARNING! Failed to send a message to Peer: (LinkCmd:%d, Size:%d)", peer_link_cmd, payload_length);
    }
    else
    {
        DEBUG_LOG_ERROR("GaiaDebugPlugin SendMessageToPeer: ERROR! Failed to put the message to the Tx buffer!: (LinkCmd:%d, Size:%d)", peer_link_cmd, payload_length);
        Panic();
    }
    return FALSE;
}


/*! \brief Receive message received from the peer device. */
static uint16 gaiaDebugPlugin_ReceiveMessageFromPeer(const uint16 buffer_size, uint8 *buffer)
{
    gaia_debug_l2cap_peer_link_task_data_t *task_data = gaiaDebugPlugin_GetL2capPeerLinkTaskData();
    PanicNull(task_data);
    Source source = task_data->source;
    uint16 readable_size = 0;
    uint16 data_size = 0;

    DEBUG_LOG_DEBUG("GaiaDebugPlugin ReceiveMessageFromPeer: (BufSize:%d, Buf:0x%p)", buffer_size, buffer);

    while ((readable_size = SourceBoundary(source)) != 0)
    {
        const uint8 *data = SourceMap(source);

        if ((data_size + readable_size) <= buffer_size)
        {
            memmove(buffer + data_size, data, readable_size);
            data_size += readable_size;
            SourceDrop(source, readable_size);
        }
        else
        {
            /* There are more data in the sink than the buffer size! */
            DEBUG_LOG_WARN("GaiaDebugPlugin ReceiveMessageFromPeer: WARNING! More data in the sink BufSize:%d < (Read:%d, Available:%d)",
                           buffer_size, data_size, readable_size);
            break;
        }
    }
    SourceClose(source);

    DEBUG_LOG_V_VERBOSE("GaiaDebugPlugin ReceiveMessageFromPeer: (Received size:%d)", data_size);
    GAIA_DEBUG_DEBUG_LOG_FORMATTED_ARRAY(data_size, buffer, V_VERBOSE);

    return data_size;
}


/*! \brief Parse a message received from the peer device. */
static bool gaiaDebugPlugin_ParseReceivedMessage(const uint16 rcv_data_size, uint8 *rcv_data, gaia_debug_l2cap_peer_link_message_command_t *peer_link_cmd, uint16 *msg_length, uint8 **msg, uint8 **next)
{
    gaia_debug_l2cap_peer_link_task_data_t *task_data = gaiaDebugPlugin_GetL2capPeerLinkTaskData();
    PanicNull(task_data);

    DEBUG_LOG_DEBUG("GaiaDebugPlugin ParseReceivedMessage: (Data size:%d)", rcv_data_size);
    if (GAIA_DEBUG_L2CAP_PEER_LINK_HEADER_SIZE <= rcv_data_size)
    {
        gaia_debug_l2cap_peer_link_message_pdu_t *msg_pdu = (gaia_debug_l2cap_peer_link_message_pdu_t*) rcv_data;
        *peer_link_cmd = msg_pdu->command;
        *msg_length    = ((uint16)msg_pdu->length_msb << 8) | msg_pdu->length_lsb;
        *msg           = msg_pdu->payload;
        if ((GAIA_DEBUG_L2CAP_PEER_LINK_HEADER_SIZE + *msg_length) < rcv_data_size)
            *next = &msg_pdu->payload[*msg_length];
        else
            *next = NULL;
        return TRUE;
    }
    else if (rcv_data_size)
    {
        DEBUG_LOG_WARN("GaiaDebugPlugin ParseReceivedMessage: WARNING! Invalid Peer Link Header: Received size:%d", rcv_data_size);
        *peer_link_cmd = link_message_command_invalid;
        *msg_length    = 0;
        *msg           = NULL;
        *next          = NULL;
    }
    return FALSE;
}


/******************************************************************************
 * Callback functions (called by the L2CAP Manager)
 ******************************************************************************/
static void gaiaDebugPlugin_HandleRegisteredInd(l2cap_manager_status_t status)
{
    UNUSED(status);
    gaia_debug_l2cap_peer_link_task_data_t *task_data = gaiaDebugPlugin_GetL2capPeerLinkTaskData();
    PanicNull(task_data);

    if (status == l2cap_manager_status_success)
    {
        DEBUG_LOG_DEBUG("GaiaDebugPlugin HandleRegisteredInd: OK");
        task_data->state = gaia_debug_l2cap_peer_link_state_disconnected;
    }
    else
    {
        DEBUG_LOG_DEBUG("GaiaDebugPlugin HandleRegisteredInd: ERROR! Failed to register L2CAP PSM!");
        task_data->state = gaia_debug_l2cap_peer_link_state_error;
        Panic();
    }
}


static l2cap_manager_status_t gaiaDebugPlugin_GetSdpRecord(uint16 local_psm, l2cap_manager_sdp_record_t *sdp_record)
{
    UNUSED(local_psm);
    uint16 length = 0;

    DEBUG_LOG_DEBUG("GaiaDebugPlugin GetSdpRecord");
    sdp_record->service_record      = sdp_GetGaiaDebugPeerLinkServiceRecord(&length);
    sdp_record->service_record_size = length;
    sdp_record->offset_to_psm       = appSdpGetGaiaDebugPeerLinkServiceRecordPsmOffset();

    return l2cap_manager_status_success;
}


static l2cap_manager_status_t gaiaDebugPlugin_GetSdpSearchPattern(const tp_bdaddr *tpaddr, l2cap_manager_sdp_search_pattern_t *sdp_search_pattern)
{
    UNUSED(tpaddr);
    uint16 length;

    DEBUG_LOG_DEBUG("GaiaDebugPlugin GetSdpSearchPattern");
    sdp_search_pattern->max_num_of_retries = 3;

    sdp_search_pattern->search_pattern      = appSdpGetGaiaDebugPeerLinkServiceSearchRequest(&length);
    sdp_search_pattern->search_pattern_size = length;
    sdp_search_pattern->max_attributes      = 0x32;
    sdp_search_pattern->attribute_list      = appSdpGetGaiaDebugPeerLinkAttributeSearchRequest(&length);
    sdp_search_pattern->attribute_list_size = length;

    return l2cap_manager_status_success;
}


static l2cap_manager_status_t gaiaDebugPlugin_GetL2capLinkConfig(const tp_bdaddr *tpaddr, l2cap_manager_l2cap_link_config_t *config)
{
    UNUSED(tpaddr);

    static const uint16 gaia_debug_l2cap_peer_link_conftab[] =
    {
        /* Configuration Table must start with a separator. */
        L2CAP_AUTOPT_SEPARATOR,

        /* Flow & Error Control Mode. */
        L2CAP_AUTOPT_FLOW_MODE,
        /* Set to Basic mode with no fallback mode. */
            BKV_16_FLOW_MODE(FLOW_MODE_BASIC, 0),
        /* Local MTU exact value (incoming). */
        L2CAP_AUTOPT_MTU_IN,
        /*  Exact MTU for this L2CAP connection. */
            GAIA_DEBUG_L2CAP_PEER_LINK_L2CAP_MTU_IN_SIZE,
        /* Remote MTU Minumum value (outgoing). */
        L2CAP_AUTOPT_MTU_OUT,
        /*  Minimum MTU accepted from the Remote device. */
            GAIA_DEBUG_L2CAP_PEER_LINK_L2CAP_MTU_OUT_SIZE,
        /* Local Flush Timeout - Accept Non-default Timeout. */
        L2CAP_AUTOPT_FLUSH_OUT,
            BKV_UINT32R(DEFAULT_L2CAP_FLUSH_TIMEOUT, 0),

        /* Configuration Table must end with a terminator. */
        L2CAP_AUTOPT_TERMINATOR
    };

    DEBUG_LOG_DEBUG("GaiaDebugPlugin GetL2capLinkConfig");
    config->conftab_length = CONFTAB_LEN(gaia_debug_l2cap_peer_link_conftab);
    config->conftab        = gaia_debug_l2cap_peer_link_conftab;

    return l2cap_manager_status_success;
}


static l2cap_manager_status_t gaiaDebugPlugin_RespondConnectInd(const l2cap_manager_connect_ind_t *ind, l2cap_manager_connect_rsp_t *rsp, void **context)
{
    void *ptr = (void*) 0x789ABCDE;     /* GAIA Debug does not use the 'context' pointer. Set a magic value for checking. (Secondary) */
    *context = ptr;

    static const uint16 gaia_debug_l2cap_peer_link_conftab_response[] =
    {
        /* Configuration Table must start with a separator. */
        L2CAP_AUTOPT_SEPARATOR,
    
        /* Local Flush Timeout - Accept Non-default Timeout. */
        L2CAP_AUTOPT_FLUSH_OUT,
            BKV_UINT32R(DEFAULT_L2CAP_FLUSH_TIMEOUT, 0),

        L2CAP_AUTOPT_TERMINATOR
    };
    tp_bdaddr remote_tpaddr;

    DEBUG_LOG_DEBUG("GaiaDebugPlugin RespondConnectInd: (Context = 0x%p)", *context);
    /* Check if the connection request is originated from the peer device or not. */
    if (gaiaDebugPlugin_GetPeerBdAddr(&remote_tpaddr))
    {
        if (BdaddrIsSame(&ind->tpaddr.taddr.addr, &remote_tpaddr.taddr.addr))
        {
            rsp->response       = TRUE;     /* Accept the connection request. */
            rsp->conftab_length = CONFTAB_LEN(gaia_debug_l2cap_peer_link_conftab_response);
            rsp->conftab        = gaia_debug_l2cap_peer_link_conftab_response;
        }
        else
        {
            rsp->response       = FALSE;    /* Reject the request. */
            rsp->conftab_length = 0;
            rsp->conftab        = NULL;
        }
    }
    else
    {
        DEBUG_LOG_ERROR("GaiaDebugPlugin RespondConnectInd: ERROR! Failed to get the peer BD-ADDR!");
        Panic();
        return l2cap_manager_status_failure;
    }

    return l2cap_manager_status_success;
}


static l2cap_manager_status_t gaiaDebugPlugin_HandleConnectCfm(const l2cap_manager_connect_cfm_t *cfm, void *context)
{
    gaia_debug_l2cap_peer_link_task_data_t *task_data = gaiaDebugPlugin_GetL2capPeerLinkTaskData();
    PanicNull(task_data);

    DEBUG_LOG_DEBUG("GaiaDebugPlugin HandleConnectCfm: (Status: 0x%X, Context:0x%p)", cfm->status, context);
    DEBUG_LOG_DEBUG("GaiaDebugPlugin HandleConnectCfm: Local PSM:            0x%04X", cfm->local_psm);
    DEBUG_LOG_VERBOSE("GaiaDebugPlugin HandleConnectCfm: Remote PSM:           0x%04X", cfm->remote_psm);
    DEBUG_LOG_DEBUG("GaiaDebugPlugin HandleConnectCfm: sink:                 0x%04X", cfm->sink);
    DEBUG_LOG_VERBOSE("GaiaDebugPlugin HandleConnectCfm: Connection ID:        0x%04X", cfm->connection_id);
    DEBUG_LOG_DEBUG("GaiaDebugPlugin HandleConnectCfm: Remote BD-ADDR:       %04X-%02X-%06X",
                    cfm->tpaddr.taddr.addr.nap, cfm->tpaddr.taddr.addr.uap, cfm->tpaddr.taddr.addr.lap);
    DEBUG_LOG_VERBOSE("GaiaDebugPlugin HandleConnectCfm: Remote MTU:           0x%04X", cfm->mtu_remote);
    DEBUG_LOG_VERBOSE("GaiaDebugPlugin HandleConnectCfm: Remote Flush Timeout: 0x%04X", cfm->flush_timeout_remote);
    DEBUG_LOG_VERBOSE("GaiaDebugPlugin HandleConnectCfm: Flow Mode:            0x%04X", cfm->mode);

    if (l2cap_connect_success == cfm->status)
    {
        PanicNull(cfm->sink);
        task_data->sink = cfm->sink;
        task_data->source = StreamSourceFromSink(cfm->sink);

        /* Set the sink in the marshal_common module */
        MarshalCommon_SetSink(task_data->sink);

        MessageStreamTaskFromSink(task_data->sink, &task_data->task);
        MessageStreamTaskFromSource(task_data->source, &task_data->task);

        PanicFalse(SinkConfigure(task_data->sink, VM_SINK_MESSAGES, VM_MESSAGES_ALL));
        PanicFalse(SourceConfigure(task_data->source, VM_SOURCE_MESSAGES, VM_MESSAGES_ALL));

        DEBUG_LOG_DEBUG("GaiaDebugPlugin HandleConnectCfm: Connected!");
        task_data->state = gaia_debug_l2cap_peer_link_state_connected;

        if (task_data->tx_buf_in_use)
        {
            if (gaiaDebugPlugin_TransmitToPeerDevice(task_data->tx_buf_msg_length, task_data->tx_buf))
            {
                /* Successfully send the message to the peer device. */
                DEBUG_LOG_DEBUG("GaiaDebugPlugin HandleConnectCfm: Message in the Tx buffer is sent (Size:%d)", task_data->tx_buf_msg_length);
                task_data->tx_buf_msg_length = 0;
                task_data->tx_buf_in_use     = FALSE;
            }
        }
    }
    else
    {
        if (l2cap_manager_connect_status_failed_sdp_search == cfm->status)
            DEBUG_LOG_WARN("GaiaDebugPlugin HandleConnectCfm: WARNING! Failed: SDP Search.");
        else
            DEBUG_LOG_WARN("GaiaDebugPlugin HandleConnectCfm: WARNING! Failed to connect! (Status: 0x%X)", cfm->status);
        DEBUG_LOG_WARN("GaiaDebugPlugin HandleConnectCfm: *** Make sure that the Secondary device is connectable! ***");

        task_data->tx_buf_msg_length = 0;
        task_data->tx_buf_in_use     = FALSE;
        task_data->state             = gaia_debug_l2cap_peer_link_state_disconnected;

        /* Notify the router that attempt to connect to the peer is failed. */
        if (task_data->functions->handle_peer_link_failed_to_connect)
            (task_data->functions->handle_peer_link_failed_to_connect)();
    }

    return l2cap_manager_status_success;
}


static l2cap_manager_status_t gaiaDebugPlugin_RespondDisconnectInd(const l2cap_manager_disconnect_ind_t *ind, void *context)
{
    gaia_debug_l2cap_peer_link_task_data_t *task_data = gaiaDebugPlugin_GetL2capPeerLinkTaskData();
    PanicNull(task_data);

    if (ind->status == l2cap_manager_disconnect_successful)
        DEBUG_LOG_DEBUG("GaiaDebugPlugin RespondDisconnectInd: Success. (Context: 0x%p)", context);
    else if (ind->status == l2cap_manager_disconnect_timed_out)
        DEBUG_LOG_DEBUG("GaiaDebugPlugin RespondDisconnectInd: Timed out.");
    else if (ind->status == l2cap_manager_disconnect_link_loss)
        DEBUG_LOG_DEBUG("GaiaDebugPlugin RespondDisconnectInd: Link loss.");
    else
        DEBUG_LOG_DEBUG("GaiaDebugPlugin RespondDisconnectInd: (Status:%d)", ind->status);

    PanicFalse(task_data->sink == ind->sink);

    /* Even though there are remaining data in the source, leave them just to
       be discarded, because the data might be incomplete. */

    task_data->sink              = 0;
    task_data->source            = 0;
    task_data->tx_buf_in_use     = FALSE;
    task_data->tx_buf_msg_length = 0;
    task_data->state = gaia_debug_l2cap_peer_link_state_disconnected;

    /* Notify the router that we have lost the link to the peer device. */
    if (task_data->functions->handle_peer_link_disconnect_ind)
        (task_data->functions->handle_peer_link_disconnect_ind)();

    return l2cap_manager_status_success;
}


static l2cap_manager_status_t gaiaDebugPlugin_HandleDisconnectCfm(const l2cap_manager_disconnect_cfm_t *cfm, void *context)
{
    gaia_debug_l2cap_peer_link_task_data_t *task_data = gaiaDebugPlugin_GetL2capPeerLinkTaskData();
    PanicNull(task_data);

    if (cfm->status == l2cap_manager_disconnect_successful)
        DEBUG_LOG_DEBUG("GaiaDebugPlugin HandleDisconnectCfm: Success. (Context: 0x%p)", context);
    else if (cfm->status == l2cap_manager_disconnect_timed_out)
        DEBUG_LOG_DEBUG("GaiaDebugPlugin HandleDisconnectCfm: Timed out.");
    else if (cfm->status == l2cap_manager_disconnect_link_loss)
        DEBUG_LOG_DEBUG("GaiaDebugPlugin HandleDisconnectCfm: Link loss.");
    else
        DEBUG_LOG_DEBUG("GaiaDebugPlugin HandleDisconnectCfm: (Status:%d)", cfm->status);

    /* No actions are needed. No response will result in the timeout, 
       and 'Unroutable Response' will be sent by the timeout code. */

    task_data->sink              = 0;
    task_data->source            = 0;
    task_data->tx_buf_in_use     = FALSE;
    task_data->tx_buf_msg_length = 0;
    task_data->state = gaia_debug_l2cap_peer_link_state_disconnected;

    return l2cap_manager_status_success;
}

/******************************************************************************
 * The message handler functions
 ******************************************************************************/
static void gaiaDebugPlugin_HandleMoreData(const MessageMoreData *msg_more_data)
{
    gaia_debug_l2cap_peer_link_task_data_t *task_data = gaiaDebugPlugin_GetL2capPeerLinkTaskData();
    PanicNull(task_data);
    Source source = task_data->source;

    DEBUG_LOG_DEBUG("gaiaDebugPlugin HandleMoreData");
    if (source == msg_more_data->source)
    {
        uint8 *raw_msg_ptr;
        uint16 raw_msg_size;
        gaia_debug_l2cap_peer_link_message_command_t peer_link_cmd = link_message_command_invalid;
        uint16 message_length = 0;
        uint8 *message = NULL;
        uint8 *next = NULL;
        uint16 received_size = gaiaDebugPlugin_ReceiveMessageFromPeer(task_data->rcv_buf_size, task_data->rcv_buf);

        raw_msg_ptr = task_data->rcv_buf;
        raw_msg_size = received_size;
        gaiaDebugPlugin_ParseReceivedMessage(raw_msg_size, raw_msg_ptr, &peer_link_cmd, &message_length, &message, &next);
        GAIA_DEBUG_DEBUG_LOG_FORMATTED_ARRAY(message_length, message, V_VERBOSE);

        /* Forward the received message to the router. */
        if (task_data->functions->handle_peer_link_received_messages)
            (task_data->functions->handle_peer_link_received_messages)(peer_link_cmd, message_length, message);
    }
    else
    {
        DEBUG_LOG_ERROR("GaiaDebugPlugin HandleMoreData: ERROR! Message More Data from unmatched link!");
        Panic();
    }
}


static void gaiaDebugPlugin_HandleMoreSpace(const MessageMoreSpace *msg_more_space)
{
    gaia_debug_l2cap_peer_link_task_data_t *task_data = gaiaDebugPlugin_GetL2capPeerLinkTaskData();
    PanicNull(task_data);
    Sink sink = task_data->sink;

    DEBUG_LOG_DEBUG("GaiaDebugPlugin HandleMoreSpace");
    PanicFalse(sink == msg_more_space->sink);

    if (task_data->tx_buf_in_use)
    {
        if (gaiaDebugPlugin_TransmitToPeerDevice(task_data->tx_buf_msg_length, task_data->tx_buf))
        {
            /* Successfully send the message to the peer device. */
            DEBUG_LOG_DEBUG("GaiaDebugPlugin HandleMoreSpace: Message in the Tx buffer is sent (Size:%d)", task_data->tx_buf_msg_length);
            task_data->tx_buf_msg_length = 0;
            task_data->tx_buf_in_use     = FALSE;
        }
    }
}


/******************************************************************************
 * The main message handler of the GAIA Debug L2CAP Peer Link
 ******************************************************************************/
/*! \brief GAIA Debug L2CAP Peer Link task message handler.

    \note The connection library dependent function.
 */
static void gaiaDebugPlugin_L2capPeerLinkHandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch (id)
    {
        /* Connection library messages */
        case MESSAGE_MORE_DATA:
            gaiaDebugPlugin_HandleMoreData((const MessageMoreData*)message);
            break;

        case MESSAGE_MORE_SPACE:
            gaiaDebugPlugin_HandleMoreSpace((const MessageMoreSpace*)message);
            break;

        default:
            DEBUG_LOG_WARN("GaiaDebugPlugin L2capPeerLinkHandleMessage: Unhandled message: 0x%04X", id);
            break;
    }
}


/******************************************************************************
 * Callback functions for handling disconnection & handover events.
 ******************************************************************************/
void GaiaDebugPlugin_L2capPeerLinkCbGaiaLinkConnect(GAIA_TRANSPORT *t)
{
    UNUSED(t);
    DEBUG_LOG_DEBUG("GaiaDebugPlugin L2capPeerLinkCbGaiaLinkConnect");
}


void GaiaDebugPlugin_L2capPeerLinkCbGaiaLinkDisconnect(GAIA_TRANSPORT *t)
{
    UNUSED(t);

    DEBUG_LOG_DEBUG("GaiaDebugPlugin L2capPeerLinkCbGaiaLinkDisconnect");

    if (Multidevice_IsPair())
    {
        if (BtDevice_IsMyAddressPrimary())
        {
            /* The GAIA link to the mobile has been disconnected.
             * This is not caused by handover. So, disconnect the L2CAP Peer Link. */
            DEBUG_LOG_DEBUG("GaiaDebugPlugin L2capPeerLinkCbGaiaLinkDisconnect: Disconnected from the mobile app.");
            GaiaDebugPlugin_L2capPeerLinkDisconnect();
        }
    }
}


bool GaiaDebugPlugin_L2capPeerLinkCbHandoverVeto(GAIA_TRANSPORT *t)
{
    UNUSED(t);
    gaia_debug_l2cap_peer_link_task_data_t *task_data = gaiaDebugPlugin_GetL2capPeerLinkTaskData();
    PanicNull(task_data);

    DEBUG_LOG_DEBUG("GaiaDebugPlugin L2capPeerLinkCbHandoverVeto: (veto:FALSE)");

    if (task_data->functions->handle_peer_link_handover_veto)
        (task_data->functions->handle_peer_link_handover_veto)();

    return FALSE;
}


void GaiaDebugPlugin_L2capPeerLinkCbHandoverComplete(GAIA_TRANSPORT *t, bool is_primary)
{
    UNUSED(t);
    gaia_debug_l2cap_peer_link_task_data_t *task_data = gaiaDebugPlugin_GetL2capPeerLinkTaskData();
    PanicNull(task_data);

    DEBUG_LOG_DEBUG("GaiaDebugPlugin L2capPeerLinkCbHandoverComplete: (is_primary:%d)", is_primary);

    if (task_data->functions->handle_peer_link_handover_complete)
        (task_data->functions->handle_peer_link_handover_complete)(t, is_primary);
}


/******************************************************************************
 * Functions accessible by the GAIA Debug router.
 ******************************************************************************/
void GaiaDebugPlugin_L2capPeerLinkInit(void)
{
    static const l2cap_manager_functions_t functions =
    {
        .registered_ind         = gaiaDebugPlugin_HandleRegisteredInd,
        .get_sdp_record         = gaiaDebugPlugin_GetSdpRecord,
        .get_sdp_search_pattern = gaiaDebugPlugin_GetSdpSearchPattern,
        .get_l2cap_link_config  = gaiaDebugPlugin_GetL2capLinkConfig,
        .respond_connect_ind    = gaiaDebugPlugin_RespondConnectInd,
        .handle_connect_cfm     = gaiaDebugPlugin_HandleConnectCfm,
        .respond_disconnect_ind = gaiaDebugPlugin_RespondDisconnectInd,
        .handle_disconnect_cfm  = gaiaDebugPlugin_HandleDisconnectCfm,
        .process_more_data      = NULL,
        .process_more_space     = NULL,
    };

    DEBUG_LOG_VERBOSE("GaiaDebugPlugin L2capPeerLinkInit");
    DEBUG_LOG_VERBOSE("GaiaDebugPlugin L2capPeerLinkInit: functions:                    %p", &functions);
    DEBUG_LOG_VERBOSE("GaiaDebugPlugin L2capPeerLinkInit: functions.get_sdp_record:     %p", functions.get_sdp_record);
    DEBUG_LOG_VERBOSE("GaiaDebugPlugin L2capPeerLinkInit: gaiaDebugPlugin_GetSdpRecord: %p", gaiaDebugPlugin_GetSdpRecord);

    gaia_debug_peer_link_data = (gaia_debug_l2cap_peer_link_task_data_t*) PanicUnlessMalloc(sizeof(gaia_debug_l2cap_peer_link_task_data_t));
    gaia_debug_peer_link_data->task.handler      = gaiaDebugPlugin_L2capPeerLinkHandleMessage;
    gaia_debug_peer_link_data->psm_instance_id   = L2CAP_MANAGER_PSM_INSTANCE_ID_INVALID;
    gaia_debug_peer_link_data->functions         = NULL;

    gaia_debug_peer_link_data->rcv_buf_size      = GAIA_DEBUG_L2CAP_PEER_LINK_L2CAP_MESSAGE_MTU_SIZE;
    gaia_debug_peer_link_data->rcv_buf           = (uint8*) PanicUnlessMalloc(GAIA_DEBUG_L2CAP_PEER_LINK_L2CAP_MESSAGE_MTU_SIZE);
    gaia_debug_peer_link_data->tx_buf_in_use     = FALSE;
    gaia_debug_peer_link_data->tx_buf_msg_length = 0;
    gaia_debug_peer_link_data->tx_buf_size       = GAIA_DEBUG_L2CAP_PEER_LINK_L2CAP_MESSAGE_MTU_SIZE;
    gaia_debug_peer_link_data->tx_buf            = (uint8*) PanicUnlessMalloc(GAIA_DEBUG_L2CAP_PEER_LINK_L2CAP_MESSAGE_MTU_SIZE);
    gaia_debug_peer_link_data->state             = gaia_debug_l2cap_peer_link_state_registering;

    L2capManager_Register(L2CAP_MANAGER_PSM_DYNAMIC_ALLOCATION, &functions, &gaia_debug_peer_link_data->psm_instance_id);
}


void GaiaDebugPlugin_L2capPeerLinkRegisterCallbackFunctions(const gaia_debug_l2cap_peer_link_functions_t *functions)
{
    gaia_debug_l2cap_peer_link_task_data_t *task_data = gaiaDebugPlugin_GetL2capPeerLinkTaskData();
    PanicNull(task_data);

    DEBUG_LOG_DEBUG("GaiaDebugPlugin L2capPeerLinkRegisterCallbackFunctions: (functions:%p)", functions);
    task_data->functions = functions;
}


gaia_debug_l2cap_peer_link_send_status_t GaiaDebugPlugin_L2capPeerLinkSend(gaia_debug_l2cap_peer_link_message_command_t peer_link_cmd, uint16 payload_length, const uint8 *payload)
{
    gaia_debug_l2cap_peer_link_task_data_t *task_data = gaiaDebugPlugin_GetL2capPeerLinkTaskData();
    PanicNull(task_data);

    DEBUG_LOG_VERBOSE("GaiaDebugPlugin L2capPeerLinkSend: (link_cmd:%d, len:%d)", peer_link_cmd, payload_length);
    if (Multidevice_IsPair() == FALSE)
        return gaia_debug_l2cap_peer_link_send_status_not_a_pair_type_device;

    switch (task_data->state)
    {
        case gaia_debug_l2cap_peer_link_state_connected:
            if (gaiaDebugPlugin_SendMessageToPeer(peer_link_cmd, payload_length, payload))
                return gaia_debug_l2cap_peer_link_send_status_success;
            else
                return gaia_debug_l2cap_peer_link_send_status_pending;

        case gaia_debug_l2cap_peer_link_state_disconnected:
        {
            tp_bdaddr tpaddr;

            if (gaiaDebugPlugin_GetPeerBdAddr(&tpaddr))
            {
                void *ptr = (void*) 0x12345678;     /* GAIA Debug does not use the 'context' pointer. Set a magic value for checking. (Primary) */
                l2cap_manager_status_t result = L2capManager_Connect(&tpaddr, task_data->psm_instance_id, ptr);
                if (result == l2cap_manager_status_success)
                {
                    task_data->state = gaia_debug_l2cap_peer_link_state_connecting;

                    /* The message need to be saved for sending it to the peer till the link is established. */
                    gaiaDebugPlugin_PutTransmitBufferToPeerDevice(peer_link_cmd, payload_length, payload);
                    return gaia_debug_l2cap_peer_link_send_status_pending;
                }
                else if (result == l2cap_manager_status_rejected_due_to_ongoing_handover)
                    return gaia_debug_l2cap_peer_link_send_status_rejected_due_to_ongoing_handover;
            }
            else
                return gaia_debug_l2cap_peer_link_send_status_failed_to_get_peer_bdaddr;
        }

        case gaia_debug_l2cap_peer_link_state_connecting:
            /* The message need to be saved for sending it to the peer till the link is established. */
            gaiaDebugPlugin_PutTransmitBufferToPeerDevice(peer_link_cmd, payload_length, payload);
            return gaia_debug_l2cap_peer_link_send_status_pending;

        case gaia_debug_l2cap_peer_link_state_disconnecting:
            return gaia_debug_l2cap_peer_link_send_status_failure_peer_unreachable;

        default:
            DEBUG_LOG_ERROR("GaiaDebugPlugin L2capPeerLinkSend: ERROR! Invalid L2CAP Peer Link State: %d", task_data->state);
            Panic();
            break;
    }

    return gaia_debug_l2cap_peer_link_send_status_failure_with_unknown_reason;
}


bool GaiaDebugPlugin_L2capPeerLinkDisconnect(void)
{
    gaia_debug_l2cap_peer_link_task_data_t *task_data = gaiaDebugPlugin_GetL2capPeerLinkTaskData();
    bool disconnected = FALSE;

    DEBUG_LOG_DEBUG("GaiaDebugPlugin L2capPeerLinkDisconnect");
    if (task_data->state == gaia_debug_l2cap_peer_link_state_disconnected)
    {
        DEBUG_LOG_DEBUG("GaiaDebugPlugin L2capPeerLinkDisconnect: Already disconnected.");
        return TRUE;
    }

    switch (task_data->state)
    {
        case gaia_debug_l2cap_peer_link_state_connected:
        {
            l2cap_manager_status_t result = L2capManager_Disconnect(task_data->sink, task_data->psm_instance_id);

            if (result == l2cap_manager_status_success)
                disconnected = TRUE;
            else
                DEBUG_LOG_WARN("GaiaDebugPlugin L2capPeerLinkDisconnect: WARNING! Failed to disconnect: (Result:%d)", result);
            break;
        }

        case gaia_debug_l2cap_peer_link_state_disconnecting:
            disconnected = TRUE;
            break;
        
        default:
            DEBUG_LOG_WARN("GaiaDebugPlugin L2capPeerLinkDisconnect: WARNING! Failed to disconnect: (State:%d)", task_data->state);
            break;
    }

    if (disconnected)
        task_data->state = gaia_debug_l2cap_peer_link_state_disconnecting;

    return disconnected;
}


void GaiaDebugPlugin_L2capPeerLinkDiscardTxBufferredData(void)
{
    gaia_debug_l2cap_peer_link_task_data_t *task_data = gaiaDebugPlugin_GetL2capPeerLinkTaskData();

    task_data->tx_buf_msg_length = 0;
    task_data->tx_buf_in_use     = FALSE;
}

#endif /* INCLUDE_GAIA_PYDBG_REMOTE_DEBUG && INCLUDE_L2CAP_MANAGER */

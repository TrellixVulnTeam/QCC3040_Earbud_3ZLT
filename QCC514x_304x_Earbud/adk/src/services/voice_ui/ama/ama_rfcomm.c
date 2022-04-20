/*!
\copyright  Copyright (c) 2019 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_rfcomm.c
\brief  Implementation of RFCOMM transport functionality for Amazon Voice Service
*/

#ifdef INCLUDE_AMA
#include "ama_rfcomm.h"
#include "ama_connect_state.h"
#include "ama_data.h"
#include "ama_protocol.h"
#include "ama_tws.h"

#include "phy_state.h"

#include <bdaddr.h>
#include <connection.h>
#include <message.h>
#include <panic.h>
#include <source.h>
#include <sink.h>
#include <stream.h>
#include <stdio.h>
#include "connection_abstraction.h"
#include <stdlib.h>

#define AMA_RFCOMM_CHANNEL 19
#define AMA_RFCOMM_CHANNEL_INVALID 0xFF
#define AMA_RFCOMM_DEFAULT_CONFIG  (0)

static const uint8 ama_rfcomm_service_record[] =
{
    /* ServiceClassIDList(0x0001) */
    0x09,                                   /*       #define ATTRIBUTE_HEADER_16BITS   0x09 */
        0x00, 0x01,
    /* DataElSeq 17 bytes */
    0x35,         /*  #define DATA_ELEMENT_SEQUENCE  0x30    ,    #define DE_TYPE_SEQUENCE       0x01     #define DE_TYPE_INTEGER        0x03 */
    0x11,        /*   size  */
        /* 16 byte uuid      931C7E8A-540F-4686-B798-E8DF0A2AD9F7 */
        0x1c,
        0x93, 0x1c, 0x7e, 0x8a, 0x54, 0x0f, 0x46, 0x86,
        0xb7, 0x98, 0xe8, 0xdf, 0x0a, 0x2a, 0xd9, 0xf7,
    /* ProtocolDescriptorList(0x0004) */
    0x09,
        0x00, 0x04,
    /* DataElSeq 12 bytes */
    0x35,
    0x0c,
        /* DataElSeq 3 bytes */
        0x35,
        0x03,
            /* uuid L2CAP(0x0100) */
            0x19,
            0x01, 0x00,
        /* DataElSeq 5 bytes */
        0x35,
        0x05,
            /* uuid RFCOMM(0x0003) */
            0x19,
            0x00, 0x03,
            /* uint8 RFCOMM_DEFAULT_CHANNEL */
            0x08,
                AMA_RFCOMM_CHANNEL
};

/* local data structure for RFCOMM transport */
typedef struct
{
    uint8* sdp_record;
    Sink data_sink;
    uint8 server_channel;
    bool connections_allowed;
    ama_local_disconnect_reason_t   reason;
}ama_rfcomm_data_t;


static ama_rfcomm_data_t ama_rfcomm_data;

/* Forward declaration */
static void amaRfcomm_MessageHandler(Task task, MessageId id, Message message);

static const TaskData ama_rfcomm_task = {amaRfcomm_MessageHandler};

Task AmaRfcomm_GetTask(void)
{
    return ((Task)&ama_rfcomm_task);
}


/***************************************************************************/
static void amaRfcomm_SetSdpRecord(const uint8* record, uint16 record_size)
{
    if(ama_rfcomm_data.sdp_record)
    {
        free(ama_rfcomm_data.sdp_record);
        ama_rfcomm_data.sdp_record = NULL;
    }

    if(record && record_size)
    {
        ama_rfcomm_data.sdp_record = (uint8*)PanicUnlessMalloc(sizeof(uint8)*record_size);
        memmove(ama_rfcomm_data.sdp_record, record, record_size);
    }
}


/*********************************************************************************/
static void amaRfcomm_RegisterSdp(uint8 server_channel)
{
    /* update the service record */
    if (server_channel != AMA_RFCOMM_CHANNEL)
    {
        amaRfcomm_SetSdpRecord(ama_rfcomm_service_record, sizeof ama_rfcomm_service_record);
        ama_rfcomm_data.sdp_record[sizeof ama_rfcomm_service_record - 1] = server_channel;
    }
    else
        ama_rfcomm_data.sdp_record = (uint8 *) ama_rfcomm_service_record;

    ama_rfcomm_data.server_channel = server_channel;

    ConnectionRegisterServiceRecord(AmaRfcomm_GetTask(), sizeof(ama_rfcomm_service_record), ama_rfcomm_data.sdp_record);
}

/*********************************************************************************/
static void amaRfcomm_LinkConnectedCfm(CL_RFCOMM_SERVER_CONNECT_CFM_T *cfm)
{
    DEBUG_LOG("amaRfcomm_LinkCreatedCfm: status=%d server_channel=%d payload_size=%d  sink=%p", cfm->status, cfm->server_channel, cfm->payload_size, cfm->sink);

    if (cfm->status ==rfcomm_connect_success && SinkIsValid(cfm->sink))
    {
        if(cfm->server_channel == ama_rfcomm_data.server_channel)
        {
            VoiceUi_AssistantConnected();
            ama_rfcomm_data.data_sink = cfm->sink;
            MessageStreamTaskFromSource(StreamSourceFromSink(ama_rfcomm_data.data_sink), AmaRfcomm_GetTask());
            SourceConfigure(StreamSourceFromSink(ama_rfcomm_data.data_sink), VM_SOURCE_MESSAGES, VM_MESSAGES_ALL);
            Ama_TransportConnected(ama_transport_rfcomm, &cfm->addr);
        }
        else
        {
            Panic();
        }
    }
}

/*********************************************************************************/
static void amaRfcomm_LinkDisconnectedCfm(Sink sink)
{
    DEBUG_LOG("amaRfcomm_LinkDisconnectedCfm");

    if (sink == ama_rfcomm_data.data_sink)
    {
        MessageStreamTaskFromSink(ama_rfcomm_data.data_sink, NULL);
        ama_rfcomm_data.data_sink = NULL;
    }
}

static void amaRfcomm_HandleMoreData(MessageMoreData * msg)
{
    uint16 len;
    while((len = SourceSize(msg->source))>0)
    {
        uint8* src = (uint8*)SourceMap(msg->source);
        AmaProtocol_ParseData((uint8*)src, len);
        SourceDrop(msg->source, len);
    }

}

/*********************************************************************************/
static void amaRfcomm_MessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch (id)
    {
        case CL_RFCOMM_REGISTER_CFM:
        {
            DEBUG_LOG("AMA_RFCOMM CL_RFCOMM_REGISTER_CFM");
            CL_RFCOMM_REGISTER_CFM_T *m   = (CL_RFCOMM_REGISTER_CFM_T*) message;
            if(m->status == success)
            {
                amaRfcomm_RegisterSdp(m->server_channel);
            }
        }
        break;

        case CL_SDP_REGISTER_CFM:
            DEBUG_LOG("AMA_RFCOMM CL_SDP_REGISTER_CFM");
            break;

        case CL_RFCOMM_CONNECT_IND:
        {
            DEBUG_LOG("AMA_RFCOMM CL_RFCOMM_CONNECT_IND connections_allowed %d",
                      ama_rfcomm_data.connections_allowed);
            CL_RFCOMM_CONNECT_IND_T *m = (CL_RFCOMM_CONNECT_IND_T*) message;
            bool is_new_or_same = BdaddrIsZero(Ama_GetBtAddress()) ||
                                  BdaddrIsSame(Ama_GetBtAddress(), &m->bd_addr);
            bool response = ama_rfcomm_data.connections_allowed && is_new_or_same;
            ConnectionRfcommConnectResponse(task, response,
                                        m->sink, m->server_channel,
                                        AMA_RFCOMM_DEFAULT_CONFIG);
        }
        break;

        case CL_RFCOMM_SERVER_CONNECT_CFM:
            DEBUG_LOG("AMA_RFCOMM CL_RFCOMM_SERVER_CONNECT_CFM");
            amaRfcomm_LinkConnectedCfm((CL_RFCOMM_SERVER_CONNECT_CFM_T*) message);
            break;

        case CL_RFCOMM_DISCONNECT_IND:
        {
            DEBUG_LOG("AMA_RFCOMM CL_RFCOMM_DISCONNECT_IND");
            CL_RFCOMM_DISCONNECT_IND_T *m = (CL_RFCOMM_DISCONNECT_IND_T*) message;
            ConnectionRfcommDisconnectResponse(m->sink);
            amaRfcomm_LinkDisconnectedCfm(m->sink);
            Ama_TransportDisconnected(ama_transport_rfcomm);
        }
        break;

        case CL_RFCOMM_DISCONNECT_CFM:
        {
            DEBUG_LOG("AMA_RFCOMM CL_RFCOMM_DISCONNECT_CFM");
            CL_RFCOMM_DISCONNECT_CFM_T *m   = (CL_RFCOMM_DISCONNECT_CFM_T*) message;
            amaRfcomm_LinkDisconnectedCfm(m->sink);
            Ama_TransportDisconnected(ama_transport_rfcomm);
            if(ama_rfcomm_data.connections_allowed == FALSE)
            {
                /* i.e. if we initiated the Disconnection */
                AmaTws_HandleLocalDisconnectionCompleted();
            }
            if(ama_rfcomm_data.reason == ama_local_disconnect_reason_forced)
            {
                /* Disconnect was forced, so don't allow reconnections */
                /* until something decides later that it's safe to do so. */
                ama_rfcomm_data.connections_allowed = FALSE;
            }
            else
            {
                ama_rfcomm_data.connections_allowed = TRUE;
            }
        }
        break;

        case CL_RFCOMM_PORTNEG_IND:
        {
            DEBUG_LOG("AMA_RFCOMM CL_RFCOMM_PORTNEG_IND");
            CL_RFCOMM_PORTNEG_IND_T *m = (CL_RFCOMM_PORTNEG_IND_T*)message;
            /* If this was a request send our default port params, otherwise accept any requested changes */
            ConnectionRfcommPortNegResponse(task, m->sink, m->request ? NULL : &m->port_params);
        }
        break;

        case MESSAGE_MORE_DATA:
        {
            DEBUG_LOG("AMA_RFCOMM RFCOMM MESSAGE_MORE_DATA");
            MessageMoreData *msg = (MessageMoreData *) message;
            amaRfcomm_HandleMoreData(msg);
        }
        break;

        case AMA_RFCOMM_LOCAL_DISCONNECT_REQ:
        {
            AMA_RFCOMM_LOCAL_DISCONNECT_REQ_T *m = (AMA_RFCOMM_LOCAL_DISCONNECT_REQ_T*)message;
            DEBUG_LOG("AMA_RFCOMM AMA_RFCOMM_LOCAL_DISCONNECT_REQ : enum:ama_local_disconnect_reason_t:%d",m->reason);
            ConnectionRfcommDisconnectRequest(task, ama_rfcomm_data.data_sink);

            ama_rfcomm_data.reason = m->reason;
        }
        break;

        case AMA_RFCOMM_LOCAL_ALLOW_CONNECTIONS_IND:
        {
            DEBUG_LOG("AMA_RFCOMM AMA_RFCOMM_LOCAL_ALLOW_CONNECTIONS_IND");
            ama_rfcomm_data.connections_allowed = TRUE;
            ama_rfcomm_data.reason = ama_local_disconnect_reason_normal;
        }
        break;

        case AMA_RFCOMM_LOCAL_BLOCK_CONNECTIONS_IND:
        {
            DEBUG_LOG("AMA_RFCOMM AMA_RFCOMM_LOCAL_BLOCK_CONNECTIONS_IND");
            ama_rfcomm_data.connections_allowed = FALSE;
        }
        break;

        case PHY_STATE_CHANGED_IND:
        {
            PHY_STATE_CHANGED_IND_T *msg = (PHY_STATE_CHANGED_IND_T *) message;
            DEBUG_LOG("AMA_RFCOMM RFCOMM PHY_STATE_CHANGED_IND state=enum:phyState:%d", msg->new_state);
            if (msg->new_state == PHY_STATE_IN_CASE)
            {
                ConnectionRfcommDisconnectRequest(task, ama_rfcomm_data.data_sink);
            }
        }
        break;
            
        case MESSAGE_MORE_SPACE:
            break;

        default:
            DEBUG_LOG("amaRfcomm_MessageHandler: unhandled MESSAGE:0x%x", id);
            break;
    }
}

bool AmaRfcomm_IsConnected(void)
{
    return (ama_transport_rfcomm == AmaData_GetActiveTransport());
}

bool AmaRfcomm_SendData(uint8* data, uint16 length)
{
    #define BAD_SINK_CLAIM (0xFFFF)
    bool status = FALSE;

    if (ama_rfcomm_data.data_sink)
    {
        Sink sink =  ama_rfcomm_data.data_sink;
        uint16 offset = SinkClaim(sink, length);

        if (offset != BAD_SINK_CLAIM)
        {
            uint8 *sink_data = SinkMap(sink);

            if (sink_data)
            {
                sink_data += offset;
                memmove(sink_data, data, length);
                status = SinkFlush(sink, length + offset);
            }
        }
    }

    if(status)
    {
        DEBUG_LOG_V_VERBOSE("AmaRfcomm_SendData: %d bytes send", length);
    }
    else
    {
        DEBUG_LOG_WARN("AmaRfcomm_SendData: Failed to send %d bytes", length);
    }

    return status;
}

void AmaRfcomm_Init(void)
{
    appPhyStateRegisterClient(AmaRfcomm_GetTask());
    ama_rfcomm_data.connections_allowed = TRUE;
    ama_rfcomm_data.reason = ama_local_disconnect_reason_normal;
    ConnectionRfcommAllocateChannel(AmaRfcomm_GetTask(), AMA_RFCOMM_CHANNEL);
}

#endif /* INCLUDE_AMA */


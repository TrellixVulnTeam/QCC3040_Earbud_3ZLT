/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       fast_pair_rfcomm.c
\brief      Implementation of RFCOMM transport functionality for Fast Pair Service
*/

#include "phy_state.h"
#include "bt_device.h"

#include <bdaddr.h>
#include <connection.h>
#include <message.h>
#include <panic.h>
#include <source.h>
#include <sink.h>
#include <stream.h>
#include <stdio.h>
#include <connection_no_ble.h>
#include <stdlib.h>
#include <vm.h>

#include "fast_pair.h"
#include "fast_pair_rfcomm.h"
#include "fast_pair_msg_stream.h"

#define FASTPAIR_RFCOMM_CHANNEL             22
#define FASTPAIR_RFCOMM_CHANNEL_INVALID     0xFF
#define FASTPAIR_RFCOMM_DEFAULT_CONFIG      (0)

static const uint8 fast_pair_rfcomm_service_record[] =
{
    /* ServiceClassIDList(0x0001) */
    0x09,                                   /*       #define ATTRIBUTE_HEADER_16BITS   0x09 */
        0x00, 0x01,
    /* DataElSeq 17 bytes */
    0x35,         /*  #define DATA_ELEMENT_SEQUENCE  0x30    ,    #define DE_TYPE_SEQUENCE       0x01     #define DE_TYPE_INTEGER        0x03 */
    0x11,        /*   size  */
    /* 16 byte fast pair message stream uuid: df21fe2c-2515-4fdb-8886-f12c4d67927c */
    0x1c,
    0xdf, 0x21, 0xfe, 0x2c, 0x25, 0x15, 0x4f, 0xdb,
    0x88, 0x86, 0xf1, 0x2c, 0x4d, 0x67, 0x92, 0x7c,
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
                FASTPAIR_RFCOMM_CHANNEL
};

/* Forward declaration */
static void fastPair_RfcommHandleMoreData(MessageMoreData * msg);
static void fastPair_RfcommMessageHandler(Task task, MessageId id, Message message);

/* Array list of FP RFCOMM data */
fast_pair_rfcomm_data_t *fast_pair_rfcomm_data[FASTPAIR_RFCOMM_CONNECTIONS_MAX];
/*! Global variables to remember the fp seeker number to send the ACK msg or data to the correct seeker
 * Eg.We support upto 2 RFCOMM connections simultaneously, we should send the ACK msg or data to the correct 
 * seeker from which we received MESSASE_MORE_DATA or link connected cfm so at the time of receiving 
 * message more data or link connected cfm we should make sure to remember the fp seeker (it could be 1 or 2).
 */
uint8 ack_msg_to_fp_seeker_number = 0;
uint8 send_data_to_fp_seeker_number = 0;
static const TaskData fast_pair_rfcomm_task = {fastPair_RfcommMessageHandler};
static fastPair_RfcommMsgCallBack fast_pair_rfcomm_msg_call_back;


static Task fastPair_RfcommGetTask(void)
{
    return ((Task)&fast_pair_rfcomm_task);
}

void fastPair_RfcommRegisterMessage(fastPair_RfcommMsgCallBack msgCallBack)
{
    fast_pair_rfcomm_msg_call_back = msgCallBack;
}


/***************************************************************************/
/*! Check if the server channel is allocated and registered with CM.
 * Parse through each sdp record and validate server channel to support multiple connections
 */
static bool fastPair_RfcommIsRegisteredServerChannel(uint8 server_channel)
{
    if (
            (fast_pair_rfcomm_service_record &&
            (fast_pair_rfcomm_service_record[sizeof(fast_pair_rfcomm_service_record)-1] == server_channel))
        )
    {
        return TRUE;
    }
    return FALSE;
}

/*********************************************************************************/
static void fastPair_RfcommRegisterSdp(uint8 server_channel)
{
    DEBUG_LOG("fastPair_RfcommRegisterSdp: server_channel %d",server_channel);

    /* update the service record */
    if (server_channel != FASTPAIR_RFCOMM_CHANNEL)
    {
        uint8 *server_channel_field = (uint8*)&fast_pair_rfcomm_service_record[sizeof(fast_pair_rfcomm_service_record) - 1];
        *server_channel_field = server_channel;
    }
    ConnectionRegisterServiceRecord(fastPair_RfcommGetTask(), sizeof(fast_pair_rfcomm_service_record), fast_pair_rfcomm_service_record);
}

/*! Check if the rfcomm instance is connected or not */
static bool fastPair_RfcommInstanceIsConnected(fast_pair_rfcomm_data_t *instance)
{
    if(!instance)
    {
        return FALSE;
    }

    if (instance->conn_state == RFCOMM_CONN_STATE_CONNECTED)
    {
        return TRUE;
    }
    return FALSE;
}

/*! Check if the incoming/outgoing rfcomm connection is allowed or not.
 * \return TRUE, if any more rfcomm connections are allowed. FALSE otherwise
 */
static bool fastPair_RfcommIsConnectionAllowed(void)
{
    uint8 no_of_active_connections = 0;
    uint8 instance;

    for(instance = 0; instance < FASTPAIR_RFCOMM_CONNECTIONS_MAX; instance++)
    {
        if (fastPair_RfcommInstanceIsConnected(fast_pair_rfcomm_data[instance]))
        {
            no_of_active_connections++;
        }
    }

    return (no_of_active_connections < FASTPAIR_RFCOMM_CONNECTIONS_MAX);
}

/*! Set rfcomm connection state */
static void fastPair_RfcommSetConnectionState(fast_pair_rfcomm_data_t *instance, rfcomm_conn_state_t new_state)
{
    if (instance->conn_state != new_state)
    {
        DEBUG_LOG("fastPair_RfcommSetConnectionState: addr[0x%06x], enum:rfcomm_conn_state_t:old_state[%d] to enum:rfcomm_conn_state_t:new_state[%d]", instance->device_addr.lap, instance->conn_state, new_state);
        instance->conn_state = new_state;
    }
    else
    {
        DEBUG_LOG("fastPair_RfcommSetConnectionState: addr[0x%06x], already in enum:rfcomm_conn_state_t:state[%d]", instance->device_addr.lap, new_state);
    }
}

/*! Get rfcomm connection instance using bluetooth address.
 */
fast_pair_rfcomm_data_t* fastPair_RfcommGetInstance(bdaddr *addr)
{
    uint8 instance;
    for(instance = 0;instance < FASTPAIR_RFCOMM_CONNECTIONS_MAX; instance++)
    {
        if (
                !BdaddrIsZero(addr) && (fast_pair_rfcomm_data[instance]) &&
                BdaddrIsSame(&(fast_pair_rfcomm_data[instance])->device_addr, addr)
            )
        {
            return (fast_pair_rfcomm_data[instance]);
        }
    }

    return NULL;
}

/*! Create rfcomm instance if bdaddr matching instance is not already present.
 */
fast_pair_rfcomm_data_t* fastPair_RfcommCreateInstance(bdaddr *addr)
{
    fast_pair_rfcomm_data_t* instance = fastPair_RfcommGetInstance(addr);
    if (!instance && fastPair_RfcommIsConnectionAllowed())
    {
        fast_pair_rfcomm_data_t* fast_pair_rfcomm_data_instance = PanicUnlessMalloc(sizeof(fast_pair_rfcomm_data_t));
        uint8 instance_count;

        fast_pair_rfcomm_data_instance->device_addr = *addr;
        fast_pair_rfcomm_data_instance->connections_allowed = TRUE;
        fast_pair_rfcomm_data_instance->server_channel = FASTPAIR_RFCOMM_CHANNEL_INVALID;
        instance = fast_pair_rfcomm_data_instance;

        /* Link the data instance to the array list */
        for(instance_count = 0; instance_count < FASTPAIR_RFCOMM_CONNECTIONS_MAX; instance_count++)
        {
            if(fast_pair_rfcomm_data[instance_count] == NULL)
            {
                fast_pair_rfcomm_data[instance_count] = fast_pair_rfcomm_data_instance;
                break;
            }
        }
    }
    else
    {
        Panic();
    }
    return instance;
}

/*! Destroy rfcomm instance.
 */
void fastPair_RfcommDestroyInstance(fast_pair_rfcomm_data_t *instance)
{

    if(instance)
    {
        uint8 instance_count;
        DEBUG_LOG("fastPair_RfcommDestroyInstance. %d", instance);
        for(instance_count = 0; instance_count < FASTPAIR_RFCOMM_CONNECTIONS_MAX; instance_count++)
        {
            if(instance == fast_pair_rfcomm_data[instance_count])
            {
                free(instance);
                fast_pair_rfcomm_data[instance_count] = NULL;
                break;
            }
        }
    }
    else
    {
        DEBUG_LOG("Can't destroy a NULL instance.");
    }
}

/*! Destroy all the rfcomm connection instances.
 */
void fastPair_RfcommDestroyAllInstances(void)
{
    uint8 instance;
    for(instance = 0; instance < FASTPAIR_RFCOMM_CONNECTIONS_MAX; instance++)
    {
        fastPair_RfcommDestroyInstance(fast_pair_rfcomm_data[instance]);
    }
}

/*! Get rfcomm connection instance matching with sink.
 */
static fast_pair_rfcomm_data_t* fastPair_RfcommGetInstanceFromSink(Sink sink)
{
    uint8 instance;

    for(instance = 0; instance < FASTPAIR_RFCOMM_CONNECTIONS_MAX; instance++)
    {
        if (fast_pair_rfcomm_data[instance] && (fast_pair_rfcomm_data[instance])->data_sink && ((fast_pair_rfcomm_data[instance])->data_sink == sink))
        {
            return (fast_pair_rfcomm_data[instance]);
        }
    }
    return NULL;
}

/*********************************************************************************/
static void fastPair_RfcommLinkConnectedCfm(CL_RFCOMM_SERVER_CONNECT_CFM_T *cfm)
{
    DEBUG_LOG("fastPair_RfcommLinkConnectedCfm: status=%d server_channel=%d payload_size=%d  sink=%p", cfm->status, cfm->server_channel, cfm->payload_size, cfm->sink);
    DEBUG_LOG("fastPair_RfcommLinkConnectedCfm BD ADDR [%04x,%02x,%06lx]", cfm->addr.nap, cfm->addr.uap, cfm->addr.lap);

    if (cfm->status ==rfcomm_connect_success && SinkIsValid(cfm->sink))
    {
        if(fastPair_RfcommIsRegisteredServerChannel(cfm->server_channel))
        {
            MessageMoreData msg;
            fast_pair_rfcomm_data_t *theInstance = fastPair_RfcommCreateInstance(&cfm->addr);
            theInstance->data_sink = cfm->sink;
            theInstance->server_channel = cfm->server_channel;
            fastPair_RfcommSetConnectionState(theInstance, RFCOMM_CONN_STATE_CONNECTED);
            MessageStreamTaskFromSource(StreamSourceFromSink(theInstance->data_sink), fastPair_RfcommGetTask());
            SourceConfigure(StreamSourceFromSink(theInstance->data_sink), VM_SOURCE_MESSAGES, VM_MESSAGES_ALL);
#ifdef INCLUDE_MIRRORING
            PanicFalse(SourceConfigure(StreamSourceFromSink(theInstance->data_sink), STREAM_SOURCE_HANDOVER_POLICY, SOURCE_HANDOVER_ALLOW_WITHOUT_DATA));
#endif
            if(fast_pair_rfcomm_msg_call_back)
            {
                uint8 instance_count;
                /* Remember FP Seeker number (1 or 2) so that data eg. model ID, BLE Address, Battery info can be sent to the correct FP Seeker */
                for(instance_count = 0; instance_count < FASTPAIR_RFCOMM_CONNECTIONS_MAX; instance_count++)
                {
                    if(fast_pair_rfcomm_data[instance_count] && StreamSourceFromSink(theInstance->data_sink) == StreamSourceFromSink(fast_pair_rfcomm_data[instance_count]->data_sink))
                    {
                        send_data_to_fp_seeker_number = instance_count + 1;
                    }
                }

                fast_pair_rfcomm_msg_call_back(FASTPAIR_RFCOMM_MESSAGE_TYPE_SERVER_CONNECT_CFM,NULL,0);

                /* Reset the fp seeker number to the default (0) after sending the data to correct seeker
                 to avoid sending same data again. */
                send_data_to_fp_seeker_number = 0;
            }

            msg.source = StreamSourceFromSink(theInstance->data_sink);
            fastPair_RfcommHandleMoreData(&msg);
        }
        else
        {
            Panic();
        }
    }
}

/*********************************************************************************/
static void fastPair_RfcommLinkDisconnectedCfm(Sink sink)
{
    DEBUG_LOG("fastPair_RfcommLinkDisconnectedCfm");

    fast_pair_rfcomm_data_t *theInstance = fastPair_RfcommGetInstanceFromSink(sink);
    PanicNull(theInstance);

    if (fastPair_RfcommInstanceIsConnected(theInstance))
    {
        MessageStreamTaskFromSink(theInstance->data_sink, NULL);
        fastPair_RfcommSetConnectionState(theInstance, RFCOMM_CONN_STATE_DISCONNECTED);
        fastPair_RfcommDestroyInstance(theInstance);
    }
}

static void fastPair_RfcommHandleMoreData(MessageMoreData * msg)
{
    uint16 len,processed_len;
    uint16 i;
    uint8 instance_count;
    DEBUG_LOG("fastPair_RfcommHandleMoreData: Receieved data");
    if((len = SourceSize(msg->source))>0)
    {
        DEBUG_LOG("LEN %d ", len);
        uint8* src = (uint8*)SourceMap(msg->source);
        for(i=0;i<len;++i)
        {
            DEBUG_LOG_V_VERBOSE(" %02x",src[i]);
        }

        processed_len = 0;
        if(fast_pair_rfcomm_msg_call_back)
        {
            /* Remember the fp seeker number from which more data is received,
             So that ACK message can be sent to the correct FP Seeker */
            for(instance_count = 0; instance_count < FASTPAIR_RFCOMM_CONNECTIONS_MAX; instance_count++)
            {
                if(fast_pair_rfcomm_data[instance_count] && msg->source == StreamSourceFromSink(fast_pair_rfcomm_data[instance_count]->data_sink))
                {
                    ack_msg_to_fp_seeker_number = instance_count + 1;
                }
            }

            processed_len = fast_pair_rfcomm_msg_call_back(FASTPAIR_RFCOMM_MESSAGE_TYPE_INCOMING_DATA,src,len);

            /* Reset the fp seeker number to the default (0) after sending the data to correct seeker
             to avoid sending same data again. */
            ack_msg_to_fp_seeker_number = 0;
        }
        if(processed_len > 0)
        {
            SourceDrop(msg->source, processed_len);
        }
    }
}

static void fastPair_RfcommFlushData(Sink data_sink)
{
    uint16 len;
    uint16 i;
    Source data_src;

    DEBUG_LOG("fastPair_RfcommFlushData: Flush any remaining data");

    data_src = StreamSourceFromSink(data_sink);
    if((len = SourceSize(data_src))>0)
    {
        DEBUG_LOG("LEN %d ", len);
        uint8* src = (uint8*)SourceMap(data_src);
        for(i=0;i<len;++i)
        {
            DEBUG_LOG_V_VERBOSE(" %02x",src[i]);
        }
        /* if there are any pending complete messages process them */
        if(fast_pair_rfcomm_msg_call_back)
        {
            fast_pair_rfcomm_msg_call_back(FASTPAIR_RFCOMM_MESSAGE_TYPE_INCOMING_DATA,src,len);
        }
        /* Drop any pending messages(even incomplete ones) as we dont expect any more data to arrive in RFCOMM */
        SourceDrop(data_src, len);
    }
}

#ifndef INCLUDE_MIRRORING
/*! Disconnect all the rfcomm connections.
 */
static void fastPair_RfcommDisconnectAll(void)
{
    uint8 instance;

    for(instance = 0; instance < FASTPAIR_RFCOMM_CONNECTIONS_MAX; instance++)
    {
        if (fastPair_RfcommInstanceIsConnected(fast_pair_rfcomm_data[instance]))
        {
            ConnectionRfcommDisconnectRequest(fastPair_RfcommGetTask(), (fast_pair_rfcomm_data[instance])->data_sink);
            (fast_pair_rfcomm_data[instance])->connections_allowed = FALSE;
        }
    }
}
#endif


/*********************************************************************************/
static void fastPair_RfcommMessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);

    DEBUG_LOG("fastPair_RfcommMessageHandler id %x", id);

    switch (id)
    {
        case CL_RFCOMM_REGISTER_CFM:
        {
            DEBUG_LOG("FASTPAIR_RFCOMM CL_RFCOMM_REGISTER_CFM");
            CL_RFCOMM_REGISTER_CFM_T *m   = (CL_RFCOMM_REGISTER_CFM_T*) message;
            if(m->status == success)
            {
                fastPair_RfcommRegisterSdp(m->server_channel);
            }
            else
            {
                DEBUG_LOG_WARN("fastPair_RfcommMessageHandler: CL_RFCOMM_REGISTER_CFM failed with error %d for channel %d",
                               m->status,m->server_channel);
            }
        }
        break;

        case CL_SDP_REGISTER_CFM:
            DEBUG_LOG("FASTPAIR_RFCOMM CL_SDP_REGISTER_CFM");
        break;

        case CL_RFCOMM_CONNECT_IND:
        {
            CL_RFCOMM_CONNECT_IND_T *m = (CL_RFCOMM_CONNECT_IND_T*) message;
            bool response = FALSE;
            if (fastPair_RfcommIsConnectionAllowed())
            {
                /* Consider "connetions_allowed" settings from rfcomm connection, if exists already.
                 * There could be chance that disconnection might be initiated, meanwhile connection indication
                 * is received. Otherwise always accept the rfcomm connection */
                fast_pair_rfcomm_data_t *theInstance = fastPair_RfcommGetInstance(&m->bd_addr);
                if (theInstance)
                {
                    response = theInstance->connections_allowed;
                }
                else
                {
                    /* accept it as there is no disconnect-connect race conditions */
                    response = TRUE;
                }
            }
            DEBUG_LOG("FASTPAIR_RFCOMM CL_RFCOMM_CONNECT_IND connections_allowed %d", response);
            ConnectionRfcommConnectResponse(task, response,
                                        m->sink, m->server_channel,
                                        FASTPAIR_RFCOMM_DEFAULT_CONFIG);
            if(fast_pair_rfcomm_msg_call_back && response)
            {
                fast_pair_rfcomm_msg_call_back(FASTPAIR_RFCOMM_MESSAGE_TYPE_CONNECT_IND,NULL,0);
            }
        }
        break;

        case CL_RFCOMM_SERVER_CONNECT_CFM:
            DEBUG_LOG("FASTPAIR_RFCOMM CL_RFCOMM_SERVER_CONNECT_CFM");
            fastPair_RfcommLinkConnectedCfm((CL_RFCOMM_SERVER_CONNECT_CFM_T*) message);
        break;

        case CL_RFCOMM_DISCONNECT_IND:
        {
            CL_RFCOMM_DISCONNECT_IND_T *m = (CL_RFCOMM_DISCONNECT_IND_T*) message;
            DEBUG_LOG("FASTPAIR_RFCOMM CL_RFCOMM_DISCONNECT_IND. Status %d",m->status);
            /* Set sink to NULL so that no ACK/response is send for any pending messages to be processed */
            fastPair_RfcommLinkDisconnectedCfm(m->sink);
            fastPair_RfcommFlushData(m->sink);
            ConnectionRfcommDisconnectResponse(m->sink);
            if(fast_pair_rfcomm_msg_call_back)
            {
                fast_pair_rfcomm_msg_call_back(FASTPAIR_RFCOMM_MESSAGE_TYPE_DISCONNECT_IND,NULL,0);
            }
        }
        break;

        case CL_RFCOMM_DISCONNECT_CFM:
        {
            DEBUG_LOG("FASTPAIR_RFCOMM CL_RFCOMM_DISCONNECT_CFM");
            CL_RFCOMM_DISCONNECT_CFM_T *m   = (CL_RFCOMM_DISCONNECT_CFM_T*) message;
            fastPair_RfcommLinkDisconnectedCfm(m->sink);
            if(fast_pair_rfcomm_msg_call_back)
            {
                fast_pair_rfcomm_msg_call_back(FASTPAIR_RFCOMM_MESSAGE_TYPE_DISCONNECT_CFM,NULL,0);
            }
        }
        break;

        case CL_RFCOMM_PORTNEG_IND:
        {
            DEBUG_LOG("FASTPAIR_RFCOMM CL_RFCOMM_PORTNEG_IND");
            CL_RFCOMM_PORTNEG_IND_T *m = (CL_RFCOMM_PORTNEG_IND_T*)message;
            /* If this was a request send our default port params, otherwise accept any requested changes */
            ConnectionRfcommPortNegResponse(task, m->sink, m->request ? NULL : &m->port_params);
        }
        break;
        case MESSAGE_MORE_DATA:
        {
            DEBUG_LOG("FastPair_RFCOMM RFCOMM MESSAGE_MORE_DATA");
            MessageMoreData *msg = (MessageMoreData *) message;
            DEBUG_LOG("MESSAGE_MORE_DATA Source %d",msg->source);
            fastPair_RfcommHandleMoreData(msg);
        }
        break;

        case PHY_STATE_CHANGED_IND:
        {
            PHY_STATE_CHANGED_IND_T *msg = (PHY_STATE_CHANGED_IND_T *) message;
            DEBUG_LOG("FASTPAIR_RFCOMM RFCOMM PHY_STATE_CHANGED_IND state=%u", msg->new_state);
            if (msg->new_state == PHY_STATE_IN_CASE)
            {
#ifndef INCLUDE_MIRRORING
                fastPair_RfcommDisconnectAll();
#endif
            }
        }
        break;

        default:
            DEBUG_LOG("FASTPAIR_RFCOMM rfCommMessageHandler unknown message=%x", id);
        break;
    }

}

bool fastPair_RfcommSendData(uint8* data, uint16 length)
{
    #define BAD_SINK_CLAIM (0xFFFF)
    bool status = FALSE;

    if(send_data_to_fp_seeker_number !=0 )
    {
        DEBUG_LOG("Sending device info data. %d", send_data_to_fp_seeker_number);
        if (fast_pair_rfcomm_data[send_data_to_fp_seeker_number - 1]->data_sink)
        {
            Sink sink =  fast_pair_rfcomm_data[send_data_to_fp_seeker_number - 1]->data_sink;
            uint16 offset = SinkClaim(sink, length);

            if (offset != BAD_SINK_CLAIM)
            {
                uint8 *sink_data = SinkMap(sink);

                if (sink_data)
                {
                    sink_data += offset;
                    memmove(sink_data, data, length);
                    status = SinkFlush(sink, length + offset);
                    DEBUG_LOG("fastPair_RfcommSendData: Sent data of len %d", length);
                }
            }
        }
        if(status)
        {
            DEBUG_LOG("fastPair_RfcommSendData: %d bytes send", length);
        }
        else
        {
            DEBUG_LOG_WARN("fastPair_RfcommSendData: Failed to send %d bytes", length);
        }
    }

    if(ack_msg_to_fp_seeker_number !=0)
    {
        DEBUG_LOG("Sending ACK for msg more data. %d", ack_msg_to_fp_seeker_number);
        if (fast_pair_rfcomm_data[ack_msg_to_fp_seeker_number - 1]->data_sink)
        {
            Sink sink =  fast_pair_rfcomm_data[ack_msg_to_fp_seeker_number - 1]->data_sink;
            uint16 offset = SinkClaim(sink, length);

            if (offset != BAD_SINK_CLAIM)
            {
                uint8 *sink_data = SinkMap(sink);

                if (sink_data)
                {
                    sink_data += offset;
                    memmove(sink_data, data, length);
                    status = SinkFlush(sink, length + offset);
                    DEBUG_LOG("fastPair_RfcommSendData: Sent data of len %d", length);
                }
            }
        }
        if(status)
        {
            DEBUG_LOG("fastPair_RfcommSendData: %d bytes send", length);
        }
        else
        {
            DEBUG_LOG_WARN("fastPair_RfcommSendData: Failed to send %d bytes", length);
        }
    }

    return status;
}

uint8 fastPair_RfcommGetRFCommChannel(bdaddr *addr)
{
    uint8 server_channel = FASTPAIR_RFCOMM_CHANNEL_INVALID;
    fast_pair_rfcomm_data_t *theInstance = fastPair_RfcommGetInstance(addr);
    if (theInstance)
    {
        server_channel = theInstance->server_channel;
    }
    return server_channel;
}

/* Check if any of rfcomm instance is connected.
 */
bool fastPair_RfcommIsConnected(void)
{
    uint8 instance;
    bool is_connected = FALSE;
    for(instance = 0; instance < FASTPAIR_RFCOMM_CONNECTIONS_MAX; instance++)
    {
        is_connected = fastPair_RfcommInstanceIsConnected(fast_pair_rfcomm_data[instance]);
    }

    return is_connected;
}

uint8 fastPair_RfcommGetRFCommConnectedInstances(void)
{
    uint8 instance;
    uint8 no_of_instances = 0;

    for (instance = 0; instance < FASTPAIR_RFCOMM_CONNECTIONS_MAX; instance++)
    {
        if ( fastPair_RfcommInstanceIsConnected(fast_pair_rfcomm_data[instance]) == TRUE)
        {
            no_of_instances += 1;
        }
    }

    return no_of_instances;
}

bool fastPair_RfcommIsConnectedForAddr(bdaddr *addr)
{
    fast_pair_rfcomm_data_t* theInstance = fastPair_RfcommGetInstance(addr);
    if (theInstance && fastPair_RfcommInstanceIsConnected(theInstance))
    {
        return TRUE;
    }
    return FALSE;
}

bool fastPair_RfcommRestoreAfterHandover(bdaddr *addr)
{
    fast_pair_rfcomm_data_t *theInstance = fastPair_RfcommGetInstance(addr);
    if(!theInstance)
    {
        DEBUG_LOG_ERROR("fastPair_RfcommRestoreAfterHandover: Instance not found, addr[0x%06x]", addr->lap);
        return FALSE;
    }

    tp_bdaddr tpaddr;
    tpaddr.transport = TRANSPORT_BREDR_ACL;
    tpaddr.taddr.type = TYPED_BDADDR_PUBLIC;
    tpaddr.taddr.addr = theInstance->device_addr;

    /* Set rfcomm connected status */
    fastPair_RfcommSetConnectionState(theInstance, RFCOMM_CONN_STATE_CONNECTED);
    theInstance->data_sink = StreamRfcommSinkFromServerChannel(&tpaddr, theInstance->server_channel);
    if (!theInstance->data_sink)
    {
        DEBUG_LOG_WARN("fastPair_RfcommRestoreAfterHandover sink not found ch=%d", theInstance->server_channel);
        return FALSE;
    }

    uint16 conn_id = PanicZero(SinkGetRfcommConnId(theInstance->data_sink));
    PanicFalse(VmOverrideRfcommConnContext(conn_id, (conn_context_t)fastPair_RfcommGetTask()));

    PanicFalse(SinkConfigure(theInstance->data_sink, VM_SINK_MESSAGES, VM_MESSAGES_NONE));

    Source source = StreamSourceFromSink(theInstance->data_sink);
    MessageStreamTaskFromSource(source, fastPair_RfcommGetTask());
    PanicFalse(SourceConfigure(source, VM_SOURCE_MESSAGES, VM_MESSAGES_ALL));
    PanicFalse(SourceConfigure(source, STREAM_SOURCE_HANDOVER_POLICY, SOURCE_HANDOVER_ALLOW_WITHOUT_DATA));

    DEBUG_LOG("fastPair_RfcommRestoreAfterHandover restored ch=%d", theInstance->server_channel);
    return TRUE;
}

bool fastPair_RfcommDisconnectInstance(fast_pair_rfcomm_data_t *instance)
{
    uint8 instance_count;
    bool instance_found = FALSE;

    DEBUG_LOG("fastPair_RfcommDisconnectInstance. %d", instance);
    for(instance_count = 0; instance_count < FASTPAIR_RFCOMM_CONNECTIONS_MAX; instance_count++)
    {
        if(instance == fast_pair_rfcomm_data[instance_count])
        {
            if (fastPair_RfcommInstanceIsConnected(fast_pair_rfcomm_data[instance_count]))
            {
                ConnectionRfcommDisconnectRequest(fastPair_RfcommGetTask(), (fast_pair_rfcomm_data[instance_count])->data_sink);
                (fast_pair_rfcomm_data[instance_count])->connections_allowed = FALSE;
                instance_found = TRUE;
            }
        }
    }

    if(!instance_found)
    {
        Panic();
    }

    return instance_found;
}

void fastPair_RfcommInit(void)
{
    appPhyStateRegisterClient(fastPair_RfcommGetTask());
    ConnectionRfcommAllocateChannel(fastPair_RfcommGetTask(), FASTPAIR_RFCOMM_CHANNEL);

    /* Array list to support upto 2 active instances of RFCOMM data */
    fast_pair_rfcomm_data[0] = NULL;
    fast_pair_rfcomm_data[1] = NULL;

    fast_pair_rfcomm_msg_call_back = NULL;
}

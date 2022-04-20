/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       fast_pair_msg_stream.c
\brief      Implementation of Fast Pair Message Stream
*/

#include <panic.h>

#include "fast_pair.h"
#include "fast_pair_rfcomm.h"
#include "fast_pair_profile.h"
#include "fast_pair_msg_stream.h"
#include "fast_pair_msg_stream_dev_info.h"
#include "fast_pair_msg_stream_dev_action.h"

/********* MESSAGE STREAM PROTOCOL **************/
/*
Octet   Data Type      Description               Mandatory?
0       uint8          Message group             Mandatory
1       uint8          Message code              Mandatory
2 - 3   uint16         Additional data length    Mandatory
4 - n                  Additional data           Optional
The additional data length and additional data fields should be big endian.
*/

#define FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_INDEX 0
#define FASTPAIR_MESSAGESTREAM_MESSAGE_CODE_INDEX  1
#define FASTPAIR_MESSAGESTREAM_MESSAGE_ADD_DATA_LEN_UPPER_INDEX  2
#define FASTPAIR_MESSAGESTREAM_MESSAGE_ADD_DATA_LEN_LOWER_INDEX  3
#define FASTPAIR_MESSAGESTREAM_MESSAGE_ADD_DATA_INDEX 4
#define FASTPAIR_MESSAGESTREAM_MESSAGE_LENGTH_MINIMUM FASTPAIR_MESSAGESTREAM_MESSAGE_ADD_DATA_INDEX

/* Message group data */
typedef struct
{
    fastPair_MsgStreamMsgCallBack bluetooth_event_msgs_callback;
    fastPair_MsgStreamMsgCallBack companion_app_event_msgs_callback;
    fastPair_MsgStreamMsgCallBack dev_info_event_msgs_callback;
    fastPair_MsgStreamMsgCallBack dev_action_event_callback;

    bool is_msg_stream_busy_incoming_data;
    bool is_msg_stream_busy_outgoing_data;
} fast_pair_msg_stream_data_t;

static fast_pair_msg_stream_data_t fast_pair_msg_stream_data;

void fastPair_MsgStreamSendData(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP group, uint8 msg_code, uint8 *add_data, uint16 add_data_len)
{
    uint8 *msg_stream_data;
    uint16 msg_stream_data_len = (FASTPAIR_MESSAGESTREAM_MESSAGE_LENGTH_MINIMUM+add_data_len);

    fast_pair_msg_stream_data.is_msg_stream_busy_outgoing_data = TRUE;

    msg_stream_data = (uint8 *)PanicUnlessMalloc(msg_stream_data_len*sizeof(uint8));
    if(NULL == msg_stream_data)
    {
        DEBUG_LOG_WARN("fastPair_MsgStreamSendData: Could not allocate memory");
        fast_pair_msg_stream_data.is_msg_stream_busy_outgoing_data = FALSE;
        return;
    }

    msg_stream_data[FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_INDEX] = group;
    msg_stream_data[FASTPAIR_MESSAGESTREAM_MESSAGE_CODE_INDEX]  = msg_code;
    msg_stream_data[FASTPAIR_MESSAGESTREAM_MESSAGE_ADD_DATA_LEN_UPPER_INDEX] = ((add_data_len&0xFF00)>>8)&0xFF;
    msg_stream_data[FASTPAIR_MESSAGESTREAM_MESSAGE_ADD_DATA_LEN_LOWER_INDEX] = add_data_len&0xFF;
    if(add_data_len > 0)
    {
        memcpy(msg_stream_data+FASTPAIR_MESSAGESTREAM_MESSAGE_LENGTH_MINIMUM,add_data,add_data_len);
    }

    DEBUG_LOG("fastPair_MsgStreamSendData: Length %d Data is ",msg_stream_data_len);
    for(int i=0;i < msg_stream_data_len;++i)
    {
        DEBUG_LOG_V_VERBOSE(" %02X",msg_stream_data[i]);
    }

    fastPair_RfcommSendData(msg_stream_data,msg_stream_data_len);
    free(msg_stream_data);
    fast_pair_msg_stream_data.is_msg_stream_busy_outgoing_data = FALSE;
}

#define FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_ACKNOWLEDGEMENT 0xFF
/* Message Code for Acknowledgement group */
typedef enum
{
    FASTPAIR_MESSAGESTREAM_ACKNOWLEDGEMENT_CODE_ACK = 0x01,
    FASTPAIR_MESSAGESTREAM_ACKNOWLEDGEMENT_CODE_NAK = 0x02
} FASTPAIR_MESSAGESTREAM_ACKNOWLEDGEMENT_CODE;

#define MESSAGE_STREAM_ACKNOWLEDGEMENT_ACK_DATA_LEN (2)
#define MESSAGE_STREAM_ACKNOWLEDGEMENT_NAK_DATA_LEN (3)

/* Send acknowledge message stream packet */

void fastPair_MsgStreamSendACK(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP msg_group, uint8 msg_code)
{
    uint8 data_ack[MESSAGE_STREAM_ACKNOWLEDGEMENT_ACK_DATA_LEN];
    data_ack[0] = (uint8)msg_group;
    data_ack[1] = msg_code;
    fastPair_MsgStreamSendData(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_ACKNOWLEDGEMENT,FASTPAIR_MESSAGESTREAM_ACKNOWLEDGEMENT_CODE_ACK,
                               data_ack,MESSAGE_STREAM_ACKNOWLEDGEMENT_ACK_DATA_LEN);
}

/* Send NAK message stream packet */
void fastPair_MsgStreamSendNAK(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP msg_group, uint8 msg_code,FASTPAIR_MESSAGESTREAM_NAK_REASON nak_reason)
{
    uint8 data_nak[MESSAGE_STREAM_ACKNOWLEDGEMENT_NAK_DATA_LEN];
    data_nak[0] = (uint8)nak_reason;
    data_nak[1] = (uint8)msg_group;
    data_nak[2] = msg_code;
    fastPair_MsgStreamSendData(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_ACKNOWLEDGEMENT,FASTPAIR_MESSAGESTREAM_ACKNOWLEDGEMENT_CODE_NAK,
                               data_nak,MESSAGE_STREAM_ACKNOWLEDGEMENT_NAK_DATA_LEN);
}

void fastPair_MsgStreamSendRsp(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP msg_group, uint8 msg_code, uint8 *data, uint8 data_len)
{
    uint8 data_ack[MESSAGE_STREAM_ACKNOWLEDGEMENT_ACK_DATA_LEN+data_len];
    data_ack[0]=(uint8)msg_group;
    data_ack[1]=msg_code;
    if(data_len > 0)
    {
        memcpy(&data_ack[2], data, data_len);
    }
    fastPair_MsgStreamSendData(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_ACKNOWLEDGEMENT,FASTPAIR_MESSAGESTREAM_ACKNOWLEDGEMENT_CODE_ACK,
                               data_ack,MESSAGE_STREAM_ACKNOWLEDGEMENT_ACK_DATA_LEN+data_len);
}

static void msgStream_InitData(void)
{
    fast_pair_msg_stream_data.is_msg_stream_busy_incoming_data=FALSE;
    fast_pair_msg_stream_data.is_msg_stream_busy_outgoing_data=FALSE;
}

static void msgStream_InitCallbacks(void)
{
    fast_pair_msg_stream_data.bluetooth_event_msgs_callback = NULL;
    fast_pair_msg_stream_data.companion_app_event_msgs_callback = NULL;
    fast_pair_msg_stream_data.dev_info_event_msgs_callback = NULL;
    fast_pair_msg_stream_data.dev_action_event_callback = NULL;
}

static void msgStream_MessageDataToGroup(uint8 message_group, uint8 *data,uint16 data_len)
{
    switch(message_group)
    {
        case FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_BLUETOOTH_EVENT:
        {
            if(fast_pair_msg_stream_data.bluetooth_event_msgs_callback)
            {
                fast_pair_msg_stream_data.bluetooth_event_msgs_callback(FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_INCOMING_DATA,data,data_len);
            }
            else
            {
                DEBUG_LOG_WARN("msgStream_MessageDataToGroup: bluetooth_event_msgs_callback not registered");
            }
        }
        break;
        case FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_COMPANION_APP_EVENT:
        {
            if(fast_pair_msg_stream_data.companion_app_event_msgs_callback)
            {
                fast_pair_msg_stream_data.companion_app_event_msgs_callback(FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_INCOMING_DATA,data,data_len);
            }
            else
            {
                DEBUG_LOG_WARN("msgStream_MessageDataToGroup: companion_app_event_msgs_callback not registered");
            }
        }
        break;
        case FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_DEVICE_INFORMATION_EVENT:
        {
            if(fast_pair_msg_stream_data.dev_info_event_msgs_callback)
            {
                fast_pair_msg_stream_data.dev_info_event_msgs_callback(FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_INCOMING_DATA,data,data_len);
            }
            else
            {
                DEBUG_LOG_WARN("msgStream_MessageDataToGroup: dev_info_msgs_callback not registered");
            }
        }
        break;
        case FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_DEVCIE_ACTION_EVENT:
        {
            if(fast_pair_msg_stream_data.dev_action_event_callback)
            {
                fast_pair_msg_stream_data.dev_action_event_callback(FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_INCOMING_DATA,data,data_len);
            }
            else
            {
                DEBUG_LOG_WARN("msgStream_MessageDataToGroup: dev_action_event_callback not registered");
            }
        }
        break;
        default:
        {
            DEBUG_LOG_WARN("msgStream_MessageDataToGroup: Data arrived on Unsupported group no %d",message_group );
        }
    }
}

static bool msgStream_IsValidMeassageGroup(uint8 message_group)
{
    if((message_group > FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_UNKNOWN) && (message_group < FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_MAX))
        return TRUE;
    else
        return FALSE;
}

static uint16 msgStream_HandleIncomingData(const uint8 *data, uint16 data_len)
{
    uint8 message_group;
    uint8 message_code;
    uint8 additional_data_len;
    int i;
    uint16 processed_len = 0;
    uint8 *received_data;
    uint16 received_data_len;

    if((NULL == data)||(0 == data_len))
    {
        DEBUG_LOG_WARN("msgStream_HandleIncomingData: Length is 0 or data is NULL");
        return processed_len;
    }

    fast_pair_msg_stream_data.is_msg_stream_busy_incoming_data = TRUE;
    received_data = (uint8 *)data;
    received_data_len = data_len;

    /* Check if the data fits is good enough*/

    DEBUG_LOG("msgStream_HandleIncomingData: received_data values received_data_len %d ",received_data_len);
    for(i =0;i<received_data_len;++i)
        DEBUG_LOG_V_VERBOSE("%02x",received_data[i]);

    while(received_data_len >= FASTPAIR_MESSAGESTREAM_MESSAGE_LENGTH_MINIMUM)
    {
        message_group = received_data[FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_INDEX];           
        message_code  = received_data[FASTPAIR_MESSAGESTREAM_MESSAGE_CODE_INDEX];
        additional_data_len = (received_data[FASTPAIR_MESSAGESTREAM_MESSAGE_ADD_DATA_LEN_UPPER_INDEX]<<8)+received_data[FASTPAIR_MESSAGESTREAM_MESSAGE_ADD_DATA_LEN_LOWER_INDEX];

        DEBUG_LOG("msgStream_HandleIncomingData: message_group %d, message_code %d, additional_data_len %d",
                   received_data[FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_INDEX],received_data[FASTPAIR_MESSAGESTREAM_MESSAGE_CODE_INDEX],additional_data_len );

        /* If received Message group is invalid dump the entire content in RFCOMM source buffer */
        if (!msgStream_IsValidMeassageGroup(message_group))
        {
            additional_data_len = 0;
        }
        
        if(received_data_len < (additional_data_len+FASTPAIR_MESSAGESTREAM_MESSAGE_LENGTH_MINIMUM))
        {
            DEBUG_LOG("msgStream_HandleIncomingData: received_data_len %d additional_data_len %d",
                        received_data_len,additional_data_len);
            fast_pair_msg_stream_data.is_msg_stream_busy_incoming_data = FALSE;
            return processed_len;
        }

         /* Call data Handler here */
        msgStream_MessageDataToGroup(message_group,&received_data[FASTPAIR_MESSAGESTREAM_MESSAGE_CODE_INDEX],(additional_data_len + FASTPAIR_MESSAGESTREAM_MESSAGE_LENGTH_MINIMUM-1));

        /* If received Message group is invalid dump the entire content in RFCOMM source buffer */
        if(!msgStream_IsValidMeassageGroup(message_group))
        {
            processed_len = data_len;
            fast_pair_msg_stream_data.is_msg_stream_busy_incoming_data = FALSE;
            return processed_len;
        }
        /* Update processed length as we have a valid mesage */
        processed_len += additional_data_len + FASTPAIR_MESSAGESTREAM_MESSAGE_LENGTH_MINIMUM;

        DEBUG_LOG("msgStream_HandleIncomingData: Processed message_group %d message_code %d processed_len %d new_len %d",
                    message_group,message_code,processed_len,(data_len - processed_len) );

        /* As a valid message is processed, data pointer and data length */
        received_data = (uint8 *)data + processed_len;
        received_data_len = data_len - processed_len;

    }
    fast_pair_msg_stream_data.is_msg_stream_busy_incoming_data = FALSE;
    return processed_len;
}

static void msgStream_MessageMulticastToClients(FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE msg_type)
{
    if(fast_pair_msg_stream_data.bluetooth_event_msgs_callback)
        fast_pair_msg_stream_data.bluetooth_event_msgs_callback(msg_type,NULL,0);
    if(fast_pair_msg_stream_data.companion_app_event_msgs_callback)
        fast_pair_msg_stream_data.companion_app_event_msgs_callback(msg_type,NULL,0);
    if(fast_pair_msg_stream_data.dev_info_event_msgs_callback)
        fast_pair_msg_stream_data.dev_info_event_msgs_callback(msg_type,NULL,0);
    if(fast_pair_msg_stream_data.dev_action_event_callback)
        fast_pair_msg_stream_data.dev_action_event_callback(msg_type,NULL,0);
}

bool fastPair_MsgStreamRegisterGroupMessages(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP msg_group, fastPair_MsgStreamMsgCallBack msgCallBack)
{
    switch(msg_group)
    {
        case FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_BLUETOOTH_EVENT:
        {
            fast_pair_msg_stream_data.bluetooth_event_msgs_callback = msgCallBack;
        }
        break;
        case FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_COMPANION_APP_EVENT:
        {
            fast_pair_msg_stream_data.companion_app_event_msgs_callback = msgCallBack;
        }
        break;
        case FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_DEVICE_INFORMATION_EVENT:
        {
            fast_pair_msg_stream_data.dev_info_event_msgs_callback = msgCallBack;
        }
        break;
        case FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_DEVCIE_ACTION_EVENT:
        {
            fast_pair_msg_stream_data.dev_action_event_callback = msgCallBack;
        }
        break;
        default:
        {
            DEBUG_LOG_WARN("fastPair_MsgStreamRegisterGroupMessages: Unsupported group no %d",msg_group );
            return FALSE;
        }
    }
    return TRUE;
}

static uint16 msgStream_MessageHandler(FASTPAIR_RFCOMM_MESSAGE_TYPE msg_type,const uint8 *msg_data, uint16 msg_len)
{
    uint16 ret_val = 0;

    switch(msg_type)
    {
        case FASTPAIR_RFCOMM_MESSAGE_TYPE_CONNECT_IND:
             msgStream_MessageMulticastToClients(FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_CONNECT_IND);
        break;
        case FASTPAIR_RFCOMM_MESSAGE_TYPE_SERVER_CONNECT_CFM:
              msgStream_MessageMulticastToClients(FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_SERVER_CONNECT_CFM);
              msgStream_InitData();
        break;
        case FASTPAIR_RFCOMM_MESSAGE_TYPE_INCOMING_DATA:
             ret_val = msgStream_HandleIncomingData(msg_data, msg_len);
        break;
        case FASTPAIR_RFCOMM_MESSAGE_TYPE_DISCONNECT_IND:
             msgStream_MessageMulticastToClients(FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_DISCONNECT_IND);
             msgStream_InitData();
        case FASTPAIR_RFCOMM_MESSAGE_TYPE_DISCONNECT_CFM:
             msgStream_MessageMulticastToClients(FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_DISCONNECT_CFM);
             msgStream_InitData();
        break;
        default:
             DEBUG_LOG("msgStream_MessageHandler: Unknown message=%x", msg_type);
        break;
    }
    return ret_val;
}

bool fastPair_MsgStreamIsConnected(void)
{
    return fastPair_RfcommIsConnected();
}

bool fastPair_MsgStreamIsBusy(void)
{
   return (fast_pair_msg_stream_data.is_msg_stream_busy_incoming_data|| fast_pair_msg_stream_data.is_msg_stream_busy_outgoing_data) ;
}

void fastPair_MsgStreamInit(void)
{
    msgStream_InitCallbacks();
    msgStream_InitData();

    fastPair_RfcommInit();
    fastPair_RfcommRegisterMessage(msgStream_MessageHandler);

    fastPair_ProfileInit();

    /* Register Message Group handlers here */
    fastPair_MsgStreamDevInfo_Init();
    fastPair_MsgStreamDevAction_Init();
}

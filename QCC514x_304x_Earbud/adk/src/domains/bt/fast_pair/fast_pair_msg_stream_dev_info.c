/*!
\copyright  Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       fast_pair_msg_stream_dev_info.c
\brief      Implementation of Fast Pair Device Information Message Stream
*/

#include <bdaddr.h>
#include <panic.h>

#include "fast_pair.h"
#include "fast_pair_config.h"
#include "fast_pair_msg_stream.h"
#include "fast_pair_msg_stream_dev_info.h"
#include "fast_pair_battery_notifications.h"
#include "multidevice.h"
#include "fast_pair_msg_stream_dev_action.h"

#define FASTPAIR_LEFT_RIGHT_ACTIVE      (0x03)
#define FASTPAIR_SINGLE_ACTIVE          (0x01)


/* Message Code for Device information event group */
typedef enum
{
    FASTPAIR_MESSAGESTREAM_DEVINFO_EVENT_CODE_MODEL_ID = 0x01,
    FASTPAIR_MESSAGESTREAM_DEVINFO_EVENT_CODE_BLE_ADDRESS_UPDATED = 0x02,
    FASTPAIR_MESSAGESTREAM_DEVINFO_EVENT_CODE_BATTERY_UPDATED = 0x03,
    FASTPAIR_MESSAGESTREAM_DEVINFO_EVENT_CODE_REMAINING_BATTERY_TIME = 0x04,
    FASTPAIR_MESSAGESTREAM_DEVINFO_EVENT_CODE_ACTIVE_COMPONENTS_REQ = 0x05,
    FASTPAIR_MESSAGESTREAM_DEVINFO_EVENT_CODE_ACTIVE_COMPONENTS_RSP = 0x06,
    FASTPAIR_MESSAGESTREAM_DEVINFO_EVENT_CODE_CAPABILITIES = 0x07,
    FASTPAIR_MESSAGESTREAM_DEVINFO_EVENT_CODE_PLATFORM_TYPE = 0x08
} FASTPAIR_MESSAGESTREAM_DEVINFO_EVENT_CODE;

typedef struct
{
    fast_pair_msg_stream_dev_info dev_info;
    bdaddr fast_pair_bdaddr;
    bool is_fast_pair_bdaddr_received;
} fast_pair_msg_stream_dev_info_data_t;

static fast_pair_msg_stream_dev_info_data_t fast_pair_msg_stream_dev_info_data;
static void devInfo_SysMessageHandler(Task task, MessageId id, Message message);
static const TaskData dev_info_msg_stream_task = {devInfo_SysMessageHandler};


static void devInfo_SysMessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch(id)
    {

        case CL_SM_BLE_READ_RANDOM_ADDRESS_CFM:
        {
            CL_SM_BLE_READ_RANDOM_ADDRESS_CFM_T *msg =  (CL_SM_BLE_READ_RANDOM_ADDRESS_CFM_T *)message;
            tp_bdaddr  fast_pair_tp_bdaddr = msg->random_tpaddr;
            DEBUG_LOG("CL_SM_BLE_READ_RANDOM_ADDRESS_CFM: Addr %04x,%02x,%06x,type %d",fast_pair_tp_bdaddr.taddr.addr.nap,
                  fast_pair_tp_bdaddr.taddr.addr.uap,fast_pair_tp_bdaddr.taddr.addr.lap,fast_pair_tp_bdaddr.taddr.type );
            fast_pair_msg_stream_dev_info_data.fast_pair_bdaddr = fast_pair_tp_bdaddr.taddr.addr;
            fast_pair_msg_stream_dev_info_data.is_fast_pair_bdaddr_received = TRUE;
        }
        break;

        default:
        {
            DEBUG_LOG_WARN("devInfo_SysMessageHandler: UNHANDLED msg id %d. ",id);
        }
    }
}

/* Send Model-Id to Seeker */
#define MESSAGE_STREAM_DEV_INFO_MODEL_ID_ADD_DATA_LEN (3)
static void msgStream_SendDeviceInformation_ModelId(void)
{
    uint8 data_model_id[MESSAGE_STREAM_DEV_INFO_MODEL_ID_ADD_DATA_LEN];
    uint32 model_id = fastPair_GetModelId();

    data_model_id[0]= (model_id >> 16) & 0xFF;
    data_model_id[1]= (model_id >> 8) & 0xFF;
    data_model_id[2]= model_id & 0xFF;

    fastPair_MsgStreamSendData(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_DEVICE_INFORMATION_EVENT,
         FASTPAIR_MESSAGESTREAM_DEVINFO_EVENT_CODE_MODEL_ID,data_model_id,MESSAGE_STREAM_DEV_INFO_MODEL_ID_ADD_DATA_LEN);
}

/* Send BLE-address to Seeker */
#define MESSAGE_STREAM_DEV_INFO_BLE_ADDRESS_ADD_DATA_LEN (6)
static void msgStream_SendDeviceInformation_BLEAddress(void)
{
    uint8 data_addr[MESSAGE_STREAM_DEV_INFO_BLE_ADDRESS_ADD_DATA_LEN];

    data_addr[0] = ((fast_pair_msg_stream_dev_info_data.fast_pair_bdaddr.nap)>> 8) & 0xFF;
    data_addr[1] =  (fast_pair_msg_stream_dev_info_data.fast_pair_bdaddr.nap) & 0xFF;
    data_addr[2] =  (fast_pair_msg_stream_dev_info_data.fast_pair_bdaddr.uap) & 0xFF;
    data_addr[3] = ((fast_pair_msg_stream_dev_info_data.fast_pair_bdaddr.lap)>> 16) & 0xFF;
    data_addr[4] = ((fast_pair_msg_stream_dev_info_data.fast_pair_bdaddr.lap)>> 8) & 0xFF;
    data_addr[5] =  (fast_pair_msg_stream_dev_info_data.fast_pair_bdaddr.lap) & 0xFF;

    fastPair_MsgStreamSendData(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_DEVICE_INFORMATION_EVENT,
         FASTPAIR_MESSAGESTREAM_DEVINFO_EVENT_CODE_BLE_ADDRESS_UPDATED,data_addr,MESSAGE_STREAM_DEV_INFO_BLE_ADDRESS_ADD_DATA_LEN);
}

/* Send Battery Info to Seeker */
#define MESSAGE_STREAM_DEV_INFO_BATTERY_ADD_DATA_LEN (3)
static void msgStream_SendDeviceInformation_Battery(void)
{
#if defined(INCLUDE_CASE_COMMS) || defined (INCLUDE_TWS)
    uint8 *data_battery = fastPair_BatteryGetData();

    if(NULL == data_battery)
    {
        DEBUG_LOG_WARN("msgStream_SendDeviceInformation_Battery: Battery info NULL");
        return;
    }
    DEBUG_LOG("msgStream_SendDeviceInformation_Battery. left %d, right %d,case %d",data_battery[FP_BATTERY_NTF_DATA_LEFT_STATE_OFFSET],
              data_battery[FP_BATTERY_NTF_DATA_RIGHT_STATE_OFFSET], data_battery[FP_BATTERY_NTF_DATA_CASE_STATE_OFFSET] );

    fastPair_MsgStreamSendData(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_DEVICE_INFORMATION_EVENT,
         FASTPAIR_MESSAGESTREAM_DEVINFO_EVENT_CODE_BATTERY_UPDATED,&data_battery[FP_BATTERY_NTF_DATA_LEFT_STATE_OFFSET],MESSAGE_STREAM_DEV_INFO_BATTERY_ADD_DATA_LEN);
#endif
}

static void devInfo_HandleConnection(void)
{
    msgStream_SendDeviceInformation_ModelId();
    if(TRUE == fast_pair_msg_stream_dev_info_data.is_fast_pair_bdaddr_received)
    {
        msgStream_SendDeviceInformation_BLEAddress();
    }
    else
    {
        DEBUG_LOG_WARN("msgStream_SendDeviceInformation_BLEAddress: BLE Addr not yet received. So, not sending it.");
    }

    msgStream_SendDeviceInformation_Battery();
}

void fastPair_MsgStreamDevInfo_BatteryUpdateAvailable(void)
{
    DEBUG_LOG("fastPairMsgStream_DevInfo_BatteryUpdatePowerState called.");

    /* If connected, send battery update */
    if(fastPair_MsgStreamIsConnected())
        msgStream_SendDeviceInformation_Battery();
}


/********* MESSAGE STREAM PROTOCOL **************/
/*
Octet   Data Type      Description               Mandatory?
0       uint8          Message group             Mandatory
1       uint8          Message code              Mandatory
2 - 3   uint16         Additional data length    Mandatory
4 - n                  Additional data           Optional
The additional data length and additional data fields should be big endian.
*/
/* The present incoming data does not contain message group. */

#define FASTPAIR_DEVINFO_CODE_INDEX  0
#define FASTPAIR_DEVINFO_ADD_DATA_LEN_UPPER_INDEX  1
#define FASTPAIR_DEVINFO_ADD_DATA_LEN_LOWER_INDEX  2
#define FASTPAIR_DEVINFO_ADD_DATA_INDEX 3

#define FASTPAIR_DEVINFO_ACTIVE_COMPONENTS_ADD_DATA_LEN 0
#define FASTPAIR_DEVINFO_ACTIVE_COMPONENTS_RSP_ADD_DATA_LEN 1
#define FASTPAIR_DEVINFO_CAPABILITIES_ADD_DATA_LEN 1
#define FASTPAIR_DEVINFO_PLATFORM_TYPE_ADD_DATA_LEN 2

static void devInfo_HandleIncomingData(const uint8 *msg_data, uint16 len)
{
    uint8 rsp_data = 0;
    uint8 msg_code;
    uint16 additional_data_len;
    if((NULL == msg_data)||(len < FASTPAIR_DEVINFO_ADD_DATA_INDEX))
    {
        DEBUG_LOG_WARN("devInfo_HandleIncomingData: UNEXPECTED ERROR - Length is %d is less than minimum of %d or data is NULL",len,FASTPAIR_DEVINFO_ADD_DATA_INDEX);
        return;
    }

    additional_data_len = msg_data[FASTPAIR_DEVINFO_ADD_DATA_LEN_UPPER_INDEX];
    additional_data_len = (additional_data_len<<8)+ msg_data[FASTPAIR_DEVINFO_ADD_DATA_LEN_LOWER_INDEX];

    if(len != (FASTPAIR_DEVINFO_ADD_DATA_INDEX + additional_data_len))
    {
        DEBUG_LOG_WARN("devInfo_HandleIncomingData: UNEXPECTED length ERROR. Received data length is %d. Should be %d",len,(FASTPAIR_DEVINFO_ADD_DATA_INDEX + additional_data_len));
        return;
    }
    msg_code = msg_data[FASTPAIR_DEVINFO_CODE_INDEX];
    switch(msg_code)
    {
        case FASTPAIR_MESSAGESTREAM_DEVINFO_EVENT_CODE_ACTIVE_COMPONENTS_REQ:
        {
             if(additional_data_len != FASTPAIR_DEVINFO_ACTIVE_COMPONENTS_ADD_DATA_LEN)
             {
                 DEBUG_LOG_WARN("devInfo_HandleIncomingData-capabilities: Additional data length is %d, should be %d",
                                 additional_data_len, FASTPAIR_DEVINFO_ACTIVE_COMPONENTS_ADD_DATA_LEN );
                 return;
             }
             if(Multidevice_IsPair())
             {
                DEBUG_LOG("devInfo Active Components Left & Right active EB");

                /* Both Left and Right Buds are active */ 
                rsp_data = FASTPAIR_LEFT_RIGHT_ACTIVE; 
             }
             else
             {
                DEBUG_LOG("devInfo Active Components Single active HS");

                /* A single device component */ 
                rsp_data = FASTPAIR_SINGLE_ACTIVE;
             }
             /* Send response message */
             fastPair_MsgStreamSendRsp(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_DEVICE_INFORMATION_EVENT,
                                       FASTPAIR_MESSAGESTREAM_DEVINFO_EVENT_CODE_ACTIVE_COMPONENTS_RSP,
                                       &rsp_data,
                                       (uint8) FASTPAIR_DEVINFO_ACTIVE_COMPONENTS_RSP_ADD_DATA_LEN);

             /* When ring device is already initiated from AG1, send ring device message to AG2 when FMA UI
                on that AG is opened for the first time after it has been connected with Headset or Earbud. */
             if (dev_action_data.ring_component != FASTPAIR_DEVICEACTION_STOP_RING)
             {
                 fastPair_MsgStreamSendData(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_DEVCIE_ACTION_EVENT,
                                            FASTPAIR_MESSAGESTREAM_DEVACTION_RING_EVENT,
                                            &dev_action_data.ring_component,
                                            FASTPAIR_DEVICEACTION_RING_RSP_ADD_DATA_LEN);
             }

        }
        break;
        case FASTPAIR_MESSAGESTREAM_DEVINFO_EVENT_CODE_CAPABILITIES:
        {
             if(additional_data_len != FASTPAIR_DEVINFO_CAPABILITIES_ADD_DATA_LEN)
             {
                 DEBUG_LOG_WARN("devInfo_HandleIncomingData-capabilities: Additional data length is %d, should be %d",
                                 additional_data_len, FASTPAIR_DEVINFO_CAPABILITIES_ADD_DATA_LEN );
                 return;
             }
             fast_pair_msg_stream_dev_info_data.dev_info.dev_info_capabilities = msg_data[FASTPAIR_DEVINFO_ADD_DATA_INDEX];
             DEBUG_LOG("devInfo_HandleIncomingData-capabilities: Companion App %d, Silence mode %d",
                       fast_pair_msg_stream_dev_info_data.dev_info.dev_info_capabilities&FASTPAIR_MESSAGESTREAM_DEVINFO_CAPABILITIES_SILENCE_MODE_SUPPORTED,
                       (fast_pair_msg_stream_dev_info_data.dev_info.dev_info_capabilities&FASTPAIR_MESSAGESTREAM_DEVINFO_CAPABILITIES_COMPANION_APP_INSTALLED)>>1);
             /* Acknowledge message */
             fastPair_MsgStreamSendACK(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_DEVICE_INFORMATION_EVENT,FASTPAIR_MESSAGESTREAM_DEVINFO_EVENT_CODE_CAPABILITIES);
        }
        break;
        case FASTPAIR_MESSAGESTREAM_DEVINFO_EVENT_CODE_PLATFORM_TYPE:
        {
            if(additional_data_len != FASTPAIR_DEVINFO_PLATFORM_TYPE_ADD_DATA_LEN)
            {
                DEBUG_LOG_WARN("devInfo_HandleIncomingData-platform type: Additional data length is %d, should be %d",
                            additional_data_len, FASTPAIR_DEVINFO_PLATFORM_TYPE_ADD_DATA_LEN );
                return;
            }
            DEBUG_LOG("devInfo_HandleIncomingData-Platrform Type: Platform %d, SDK Ver %d",
                      msg_data[FASTPAIR_DEVINFO_ADD_DATA_INDEX],msg_data[FASTPAIR_DEVINFO_ADD_DATA_INDEX+1]);

            /* Acknowledge message */
            fastPair_MsgStreamSendACK(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_DEVICE_INFORMATION_EVENT,FASTPAIR_MESSAGESTREAM_DEVINFO_EVENT_CODE_PLATFORM_TYPE);
        }
        break;
        default:
        {
            /* Acknowledge message */
            fastPair_MsgStreamSendACK(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_DEVICE_INFORMATION_EVENT,msg_code);

            DEBUG_LOG_WARN("devInfo_HandleIncomingData: UNHANDLED code %d. ",msg_code);
            return;
        }
    }
}

/* Handle messages from Message stream */
static void devInfo_MsgStreamMessageHandler(FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE msg_type, const uint8 *msg_data, uint16 len)
{
    switch(msg_type)
    {
        case FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_CONNECT_IND:
             /* Request for BLE address */
             fast_pair_msg_stream_dev_info_data.is_fast_pair_bdaddr_received = FALSE;
             ConnectionSmBleReadRandomAddressTaskReq((Task)&dev_info_msg_stream_task, ble_read_random_address_local,NULL);
        break;
        case FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_SERVER_CONNECT_CFM:
              devInfo_HandleConnection();
              fast_pair_msg_stream_dev_info_data.dev_info.dev_info_capabilities = 0;
        break;
        case FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_INCOMING_DATA:
             devInfo_HandleIncomingData(msg_data, len);
        break;
        case FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_DISCONNECT_IND:
        case FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_DISCONNECT_CFM:
        break;
        default:
             DEBUG_LOG("devInfo_MsgStreamMessageHandler: unknown message=%x", msg_type);
        break;
    }
}

fast_pair_msg_stream_dev_info fastPair_MsgStreamDevInfo_Get(void)
{
    return fast_pair_msg_stream_dev_info_data.dev_info;
}

void fastPair_MsgStreamDevInfo_Set(fast_pair_msg_stream_dev_info dev_info)
{
    fast_pair_msg_stream_dev_info_data.dev_info = dev_info;
}

void fastPair_MsgStreamDevInfo_Init(void)
{
    fast_pair_msg_stream_dev_info_data.dev_info.dev_info_capabilities = 0;
    fast_pair_msg_stream_dev_info_data.is_fast_pair_bdaddr_received = FALSE;

    /* Handle Device Information messages from Message stream */
    fastPair_MsgStreamRegisterGroupMessages(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_DEVICE_INFORMATION_EVENT,devInfo_MsgStreamMessageHandler);
}

/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       fast_pair_pname_sync.c
\defgroup   fast_pair
\brief      Component handling synchronization of fast pair personalized name between peers

*/

#include "fast_pair_pname_sync.h"
#include "fast_pair_session_data.h"
#include "fast_pair.h"
#include "device_properties.h"

#include <marshal_common.h>
#include <marshal.h>
#include <peer_signalling.h>
#include <bt_device.h>
#include <device_list.h>
#include <task_list.h>
#include <logging.h>
#include <panic.h>
#include <stdlib.h>
#include <stdio.h>

/*! Global Instance of Personalized Name Sync Task Data */
fp_pname_sync_task_data_t pname_sync;

/*!Definition of marshalled messages used by personalized name Sync. */
const marshal_member_descriptor_t fp_pname_sync_req_member_descriptors[] =
{
    MAKE_MARSHAL_MEMBER_ARRAY(fast_pair_pname_sync_req_t, uint8, pname, (FAST_PAIR_PNAME_STORAGE_LEN) ),
};

const marshal_type_descriptor_t marshal_type_descriptor_fast_pair_pname_sync_req_t =
    MAKE_MARSHAL_TYPE_DEFINITION(fast_pair_pname_sync_req_t, fp_pname_sync_req_member_descriptors);

const marshal_type_descriptor_t marshal_type_descriptor_fast_pair_pname_sync_cfm_t =
    MAKE_MARSHAL_TYPE_DEFINITION_BASIC(sizeof(fast_pair_pname_sync_cfm_t));

/*! X-Macro generate personalized name sync marshal type descriptor set that can be passed to a (un)marshaller to initialise it.
 */
#define EXPAND_AS_TYPE_DEFINITION(type) (const marshal_type_descriptor_t *)&marshal_type_descriptor_##type,
const marshal_type_descriptor_t * const fp_pname_sync_marshal_type_descriptors[NUMBER_OF_MARSHAL_PNAME_SYNC_OBJECT_TYPES] = {
    MARSHAL_COMMON_TYPES_TABLE(EXPAND_AS_TYPE_DEFINITION)
    MARSHAL_TYPES_TABLE_PNAME_SYNC(EXPAND_AS_TYPE_DEFINITION)
};
#undef EXPAND_AS_TYPE_DEFINITION

/*! \brief Send the marshalled data to the peer
 */
static void fastPair_PNameSync_SendMarshalledData(fast_pair_pname_sync_req_t *sync_data)
{
    bdaddr peer_addr;

    if(appDeviceGetPeerBdAddr(&peer_addr))
    {       
        DEBUG_LOG_DEBUG("fastPair_PNameSync_SendMarshalledData. Send Marshalled Data to the peer.");
        /*! send the personalized name to counterpart on other earbud */
        appPeerSigMarshalledMsgChannelTx(fpPNameSync_GetTask(),
                                         PEER_SIG_MSG_CHANNEL_FP_PNAME_SYNC,
                                         sync_data, MARSHAL_TYPE_fast_pair_pname_sync_req_t);
    }
    else
    {
        DEBUG_LOG_DEBUG("fastPair_PNameSync_SendMarshalledData. No Peer to send to.");
    }
}

/*! \brief Send the confirmation of synchronization to primary device
 */
static void fastPair_PNameSync_SendConfirmation(bool synced)
{
    bdaddr peer_addr;

    if (appDeviceGetPeerBdAddr(&peer_addr))
    {
        fast_pair_pname_sync_cfm_t* cfm = PanicUnlessMalloc(sizeof(fast_pair_pname_sync_cfm_t));
        cfm->synced = synced;
        DEBUG_LOG("fastPair_PNameSync_SendConfirmation. Send confirmation to the peer.");
        /*! send confirmation of personalized received */
        appPeerSigMarshalledMsgChannelTx(fpPNameSync_GetTask(),
                                         PEER_SIG_MSG_CHANNEL_FP_PNAME_SYNC,
                                         cfm, MARSHAL_TYPE_fast_pair_pname_sync_cfm_t);
    }
    else
    {
        DEBUG_LOG("fastPair_PNameSync_SendConfirmation. No Peer to send to.");
    }
}

/*! \brief Handle confirmation of transmission of a marshalled message.
 */
static void fastPair_PNameSync_HandleMarshalledMsgChannelTxCfm(const PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM_T* cfm)
{
    DEBUG_LOG("fastPair_PNameSync_HandleMarshalledMsgChannelTxCfm channel %u status %u", cfm->channel, cfm->status);
}

/*! \brief Handle incoming marshalled messages from peer Personalized Name sync component.
 */
static void fastPair_PNameSync_HandleMarshalledMsgChannelRxInd(PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND_T* ind)
{
    switch (ind->type)
    {
        case MARSHAL_TYPE_fast_pair_pname_sync_req_t:
        {
            bool synced;
            fast_pair_pname_sync_req_t* req = (fast_pair_pname_sync_req_t*)ind->msg;
            DEBUG_LOG("fastPair_PNameSyncSync_HandleMarshalledMsgChannelRxInd RX Personalized Name ");
            /*! Store the personalized name and send the confirmation to the peer */
            synced = fastPair_StorePNameInPSStore(req->pname);
            fastPair_PNameSync_SendConfirmation(synced);
            free(req);
        }
        break;

        case MARSHAL_TYPE_fast_pair_pname_sync_cfm_t:
        {
            fast_pair_pname_sync_cfm_t *cfm = (fast_pair_pname_sync_cfm_t*)ind->msg;
            if(!cfm->synced)
            {
                DEBUG_LOG("fastPair_PNameSync_HandleMarshalledMsgChannelRxInd. Failed to Synchronize.");
            }
            else
            {
                DEBUG_LOG("fastPair_PNameSync_HandleMarshalledMsgChannelRxInd. Synchronized successfully.");
            }
            free(cfm);
        }
        break;

        default:
            break;
    }
}

/*!\brief Fast Pair Personalized Name Sync Message Handler
 */
static void fastPair_PNameSync_HandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch (id)
    {
            /* marshalled messaging */
        case PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND:
            DEBUG_LOG("fastPair_PNameSync_HandleMessage. PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND");
            fastPair_PNameSync_HandleMarshalledMsgChannelRxInd((PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND_T*)message);
            break;
        case PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM:
            DEBUG_LOG("fastPair_PNameSync_HandleMessage. PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM");
            fastPair_PNameSync_HandleMarshalledMsgChannelTxCfm((PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM_T*)message);
            break;

        default:
            break;
    }
}

/*! \brief Fast Pair Personalized Name Sync Initialization
 */
void fastPair_PNameSync_Init(void)
{
    DEBUG_LOG("fastPair_PNameSync_Init");
    fp_pname_sync_task_data_t *key_sync = fpPNameSync_GetTaskData();

    /* Initialize component task data */
    memset(key_sync, 0, sizeof(*key_sync));
    key_sync->task.handler = fastPair_PNameSync_HandleMessage;

    /* Register with peer signalling to use the personalized name sync msg channel */
    appPeerSigMarshalledMsgChannelTaskRegister(fpPNameSync_GetTask(),
                                               PEER_SIG_MSG_CHANNEL_FP_PNAME_SYNC,
                                               fp_pname_sync_marshal_type_descriptors,
                                               NUMBER_OF_MARSHAL_PNAME_SYNC_OBJECT_TYPES);
    DEBUG_LOG("fastPair_PNameSync_Init. Initialized successfully. ");
}

/*! \brief Fast Pair Personalized Name Synchronization API
 */
void FastPair_PNameSync_Sync(void)
{
    DEBUG_LOG("FastPair_PNameSync_Sync. Synchronization starts.");
    deviceType type = DEVICE_TYPE_SELF;
    device_t my_device = DeviceList_GetFirstDeviceWithPropertyValue(device_property_type, &type, sizeof(deviceType));

    void *pname_value = NULL;

    size_t pname_size = 0;

    if(my_device)
    {
        if(appPeerSigIsConnected())
        {
            if(Device_GetProperty(my_device, device_property_fast_pair_personalized_name, &pname_value, &pname_size))
            {
                fast_pair_pname_sync_req_t *sync_data = PanicUnlessMalloc(sizeof(fast_pair_pname_sync_req_t));

                memcpy(sync_data->pname, pname_value, sizeof(sync_data->pname));
                fastPair_PNameSync_SendMarshalledData(sync_data);
            }
            else
            {
                DEBUG_LOG("FastPair_PNameSync_Sync. Should not reach here.Unexpected Data.");
            }
        }
        else
        {
            DEBUG_LOG("FastPair_PNameSync_Sync. Peer Signaling not connected");
        }
    }
    else
    {
        DEBUG_LOG("FastPair_PNameSync_Sync. SELF device does not exist.");
    }
}

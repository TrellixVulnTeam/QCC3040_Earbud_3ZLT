/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    device_sync
\brief      Main component responsible for device data synchronisation.

Only one client is supported at the moment.

*/

#include "device_sync.h"
#include "device_sync_marshal_desc.h"

#include <device_properties.h>
#include <device_db_serialiser.h>
#include <bt_device.h>
#include <peer_signalling.h>
#include <key_sync.h>
#include <app_handover_if.h>

#include <device_list.h>
#include <task_list.h>
#include <logging.h>

#include <panic.h>

#include <stdlib.h>

LOGGING_PRESERVE_MESSAGE_TYPE(device_sync_messages_t)
ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(DEVICE_SYNC, DEVICE_SYNC_MESSAGE_END)


typedef enum
{
    DEVICE_SYNC_INTERNAL_PERSIST_DATA = 0,
} device_sync_internal_messages_t;

#define DATA_PERSIST_DELAY_MS 500

typedef struct
{
    const device_sync_callback_t *fn;
    device_sync_client_id_t client_id;
} device_sync_client_t;

typedef struct
{
    TaskData task;
    device_sync_client_t clients;
    task_list_t *listeners;
} device_sync_t;

static device_sync_t device_sync;

static const earbud_device_property_t properties_to_sync[] = {
        device_property_connected_profiles,
        device_property_supported_profiles,
        /* Need to sync upgrade_transport_connected property so secondary earbud will know 
           which handset is doing UPGRADE. so it can set the MRU flag for upgrade handset 
           before DFU reboot.
           After reboot, any earbud (old secondary can become PRIMARY) can become primary.
           if property is not synced then after reboot, when old SECONDARY became PRIMARY,
           before reboot secondary EB would have not SET the correct device with MRU flag. */
        device_property_upgrade_transport_connected,
        device_property_mru,
        device_property_battery_server_config_l,
        device_property_battery_server_config_r,
        device_property_gatt_server_config,
};

static const earbud_device_property_t self_properties_to_sync[] = {
        device_property_headset_service_config,
        device_property_va_flags,
        device_property_va_locale,
        device_property_ui_user_gesture_table
};

static bool deviceSync_Veto(void)
{
    if(MessageCancelAll((Task)&device_sync.task, DEVICE_SYNC_INTERNAL_PERSIST_DATA))
    {
        DEBUG_LOG_INFO("deviceSync_Veto Persist data to not hold handover for too long");
        DeviceDbSerialiser_Serialise();
    }

    if (MessagesPendingForTask((Task)&device_sync.task, NULL))
    {
        DEBUG_LOG_INFO("deviceSync_Veto, Messages pending for Data sync task");
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

static void deviceSync_Commit(bool is_primary)
{
    UNUSED(is_primary);
}

REGISTER_HANDOVER_INTERFACE_NO_MARSHALLING(DEVICE_SYNC, deviceSync_Veto, deviceSync_Commit);

static void deviceSync_SendPropertyUpdateInd(device_t device, uint8 property_id)
{
    MESSAGE_MAKE(msg, DEVICE_SYNC_PROPERTY_UPDATE_IND_T);
    msg->device = device;
    msg->property_id = property_id;

    TaskList_MessageSendWithSize(device_sync.listeners, DEVICE_SYNC_PROPERTY_UPDATE_IND, msg, sizeof(DEVICE_SYNC_PROPERTY_UPDATE_IND_T));
}

static void deviceSync_PropertySyncRxInd(void *message)
{
    device_property_sync_t *msg = (device_property_sync_t *)message;
    device_t device = BtDevice_GetDeviceForBdAddr(&msg->addr);
    if(device)
    {
        deviceType device_type = BtDevice_GetDeviceType(device);

        if(device_type == DEVICE_TYPE_HANDSET)
        {
            if(msg->id == device_property_mru)
            {
               /* MRU is a special case. It only matters when it is set to TRUE.
                  Properties are also not set directly but using BtDevice API.
               */
               if(msg->size == 1 && msg->data[0] == TRUE)
               {
                   appDeviceUpdateMruDevice(&msg->addr);
               }
            }
            else
            {
                Device_SetProperty(device, msg->id, (void *)msg->data, msg->size);
            }


            MessageCancelAll((Task)&device_sync.task, DEVICE_SYNC_INTERNAL_PERSIST_DATA);
            MessageSendLater((Task)&device_sync.task, DEVICE_SYNC_INTERNAL_PERSIST_DATA, NULL, DATA_PERSIST_DELAY_MS);
        }
        else if(device_type == DEVICE_TYPE_EARBUD)
        {
            DEBUG_LOG_ALWAYS("deviceSync_PropertySyncRxInd device type enum:deviceType:%d, property enum:earbud_device_property_t:%d msg data 0x%x, msg size %d",
                    device_type, msg->id, msg->data[0], msg->size);

            deviceType self_device_type = DEVICE_TYPE_SELF;

            device_t self_device = DeviceList_GetFirstDeviceWithPropertyValue(device_property_type, &self_device_type, sizeof(deviceType));
            if(self_device)
            {
                Device_SetProperty(self_device, msg->id, (void *)msg->data, msg->size);
                deviceSync_SendPropertyUpdateInd(self_device, msg->id);
            }

            MessageCancelAll((Task)&device_sync.task, DEVICE_SYNC_INTERNAL_PERSIST_DATA);
            MessageSendLater((Task)&device_sync.task, DEVICE_SYNC_INTERNAL_PERSIST_DATA, NULL, DATA_PERSIST_DELAY_MS);
        }
    }
}

static void deviceSync_SendPropertySyncMessage(device_t device, device_property_t id, const uint8 *value, size_t size)
{
    device_property_sync_t *sync_msg = PanicUnlessMalloc(sizeof(device_property_sync_t) + sizeof(uint8) * size);

    sync_msg->addr = DeviceProperties_GetBdAddr(device);
    sync_msg->client_id = device_sync_client_core;
    sync_msg->id = id;
    sync_msg->size = size;
    memcpy(sync_msg->data, value, size);

    DeviceSync_SyncData(sync_msg);
}

static void deviceSync_SendAllPropertiesForDevice(device_t device)
{
    if(BtDevice_IsMyAddressPrimary())
    {
        uint8 i;

        for(i = 0; i < ARRAY_DIM(properties_to_sync); ++i)
        {
            uint8 *ptr = NULL;
            size_t property_size = 0;

            if(Device_GetProperty(device, properties_to_sync[i], (void *)&ptr, &property_size))
            {
                deviceSync_SendPropertySyncMessage(device, properties_to_sync[i], ptr, property_size);
            }
        }
    }
}

inline static void deviceSync_SendAllPropertiesForSelfDevice(void)
{
    if(BtDevice_IsMyAddressPrimary())
    {
        uint8 i;

        deviceType device_type = DEVICE_TYPE_SELF;
        device_t device = DeviceList_GetFirstDeviceWithPropertyValue(device_property_type, &device_type, sizeof(deviceType));

        if(device)
        {
            for(i = 0; i < ARRAY_DIM(self_properties_to_sync); ++i)
            {
                uint8 *ptr = NULL;
                size_t property_size = 0;

                if(Device_GetProperty(device, self_properties_to_sync[i], (void *)&ptr, &property_size))
                {
                    deviceSync_SendPropertySyncMessage(device, self_properties_to_sync[i], ptr, property_size);
                }
            }
        }
    }
}

static void deviceSync_HandlePeerSigConnected(void)
{
    if(BtDevice_IsMyAddressPrimary())
    {
        DEBUG_LOG_VERBOSE("deviceSync_HandlePeerSigConnected");

        deviceType device_type = DEVICE_TYPE_HANDSET;
        device_t* devices = NULL;
        unsigned num_devices = 0;

        DeviceList_GetAllDevicesWithPropertyValue(device_property_type, &device_type, sizeof(deviceType), &devices, &num_devices);
        if (devices && num_devices)
        {
            DEBUG_LOG_VERBOSE("deviceSync_HandlePeerSigConnected It seems that there is %d handsets", num_devices);
            for (unsigned i=0; i< num_devices; i++)
            {
                deviceSync_SendAllPropertiesForDevice(devices[i]);
            }
        }
        free(devices);
        devices = NULL;

        deviceSync_SendAllPropertiesForSelfDevice();
    }
}

static bool deviceSync_IsPropertyOnSyncList(device_property_t id)
{
    uint8 i;

    for(i = 0; i < ARRAY_DIM(properties_to_sync); ++i)
    {
        if(id == properties_to_sync[i])
        {
            return TRUE;
        }
    }

    return FALSE;
}

static bool deviceSync_IsPropertyOnSelfSyncList(device_property_t id)
{
    uint8 i;

    for(i = 0; i < ARRAY_DIM(self_properties_to_sync); ++i)
    {
        if(id == self_properties_to_sync[i])
        {
            return TRUE;
        }
    }

    return FALSE;
}

static void deviceSync_PropertyChangedHandler(device_t device, device_property_t id, const void *value, size_t size)
{
    if(BtDevice_IsMyAddressPrimary())
    {
        DEBUG_LOG_VERBOSE("deviceSync_PropertyChangedHandler device 0x%x, enum:earbud_device_property_t:%d", device, id);
        if(deviceSync_IsPropertyOnSyncList(id))
        {
            if(BtDevice_GetDeviceType(device) == DEVICE_TYPE_HANDSET)
            {
                deviceSync_SendPropertySyncMessage(device, id, value, size);
            }
        }

        if(deviceSync_IsPropertyOnSelfSyncList(id))
        {
            if(BtDevice_GetDeviceType(device) == DEVICE_TYPE_SELF)
            {
                deviceSync_SendPropertySyncMessage(device, id, value, size);
            }
        }
    }
}

static void deviceSync_MessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch(id)
    {
        /* Messages handled on receiving peer */
        case PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND:
        {
            PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND_T *ind = (PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND_T *)message;
            if(ind->type == MARSHAL_TYPE_device_property_sync_t)
            {
                device_property_sync_t *msg = (device_property_sync_t *)ind->msg;
                if(msg->client_id == device_sync_client_core)
                {
                    deviceSync_PropertySyncRxInd(ind->msg);
                }
                else if(msg->client_id == device_sync_client_device_pskey)
                {
                    DEBUG_LOG_VERBOSE("deviceSync_MessageHandler MARSHAL_TYPE_device_pskey_sync_t");
                    if(device_sync.clients.fn)
                    {
                        if(device_sync.clients.fn->SyncRxIndCallback(ind->msg))
                        {
                            device_property_sync_cfm_t *cfm_msg = PanicUnlessMalloc(sizeof(device_property_sync_cfm_t));

                            cfm_msg->addr = msg->addr;
                            cfm_msg->client_id = msg->client_id;
                            cfm_msg->id = msg->id;

                            appPeerSigMarshalledMsgChannelTx(&device_sync.task,
                                            PEER_SIG_MSG_CHANNEL_DEVICE_SYNC,
                                            cfm_msg, MARSHAL_TYPE_device_property_sync_cfm_t);
                        }
                    }
                }
            }
            else if(ind->type == MARSHAL_TYPE_device_property_sync_cfm_t)
            {
                /* Message handled on peer sending updates */
                device_property_sync_cfm_t *msg = (device_property_sync_cfm_t *)ind->msg;
                if(device_sync.clients.fn)
                {
                    device_sync.clients.fn->SyncCfmCallback(BtDevice_GetDeviceForBdAddr(&msg->addr), msg->id);
                }
            }
            free(ind->msg);
        }
        break;

        case DEVICE_SYNC_INTERNAL_PERSIST_DATA:
        {
            DEBUG_LOG_VERBOSE("deviceSync_MessageHandler DEVICE_SYNC_INTERANL_PERSIST_DATA");
            DeviceDbSerialiser_Serialise();

        }
        break;

        /* Messages handled on peer sending updates */
        case PEER_SIG_CONNECTION_IND:
        {
            PEER_SIG_CONNECTION_IND_T *ind = (PEER_SIG_CONNECTION_IND_T *)message;
            if (ind->status == peerSigStatusConnected)
            {
                deviceSync_HandlePeerSigConnected();

                if(device_sync.clients.fn)
                {
                    device_sync.clients.fn->PeerConnectedCallback();
                }
            }
        }
        break;

        case KEY_SYNC_DEVICE_COMPLETE_IND:
        {
            KEY_SYNC_DEVICE_COMPLETE_IND_T *msg = (KEY_SYNC_DEVICE_COMPLETE_IND_T *)message;

            DEBUG_LOG_VERBOSE("deviceSync_MessageHandler KEY_SYNC_DEVICE_COMPLETE_IND");
            device_t device = BtDevice_GetDeviceForBdAddr(&msg->bd_addr);

            /* Key sync only apply to handset devices, so this can be only handset */

            deviceSync_SendAllPropertiesForDevice(device);

            if(device_sync.clients.fn)
            {
                device_sync.clients.fn->DeviceAddedToPeerCallback(device);
            }
        }
        break;

        default:
            break;
    }
}

bool DeviceSync_Init(Task init_task)
{
    UNUSED(init_task);

    memset(&device_sync, 0, sizeof(device_sync));

    device_sync.task.handler = deviceSync_MessageHandler;

    device_sync.listeners = TaskList_CreateWithCapacity(1);

    appPeerSigClientRegister((Task)&device_sync.task);

    appPeerSigMarshalledMsgChannelTaskRegister(&device_sync.task,
            PEER_SIG_MSG_CHANNEL_DEVICE_SYNC,
            device_sync_marshal_type_descriptors,
            NUMBER_OF_MARSHAL_OBJECT_TYPES);

    KeySync_RegisterListener((Task)&device_sync.task);

    Device_RegisterOnPropertySetHandler(deviceSync_PropertyChangedHandler);

    return TRUE;
}

void DeviceSync_RegisterCallback(device_sync_client_id_t client_id, const device_sync_callback_t *callback)
{
    device_sync.clients.fn = callback;
    device_sync.clients.client_id = client_id;
}

void DeviceSync_RegisterForNotification(Task listener)
{
    TaskList_AddTask(device_sync.listeners, listener);
}

void DeviceSync_SyncData(void *msg)
{
    appPeerSigMarshalledMsgChannelTx(&device_sync.task,
                PEER_SIG_MSG_CHANNEL_DEVICE_SYNC,
                msg, MARSHAL_TYPE_device_property_sync_t);
}

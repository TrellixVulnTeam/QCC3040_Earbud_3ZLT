/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Extensions of device_sync for synchronisation of device ps keys.

Currently only remote name is supported.

*/

#include "device_sync_pskey.h"

#include <device_pskey.h>
#include <device_sync.h>
#include <bt_device.h>
#include <device_db_serialiser.h>
#include <device_list.h>
#include <device_properties.h>
#include <key_sync.h>

#include <panic.h>
#include <logging.h>

static void deviceSyncPsKey_Sync(device_t device, device_pskey_data_id_t data_id, uint8 *data, uint16 data_size)
{
    device_property_sync_t *key_sync = PanicUnlessMalloc(sizeof(device_property_sync_t) + sizeof(uint8)*data_size);

    DEBUG_LOG_VERBOSE("DeviceSyncPskey_Sync");

    key_sync->addr = DeviceProperties_GetBdAddr(device);
    key_sync->client_id = device_sync_client_device_pskey;
    key_sync->id = data_id;
    key_sync->size = data_size;
    memcpy(key_sync->data, data, data_size);

    DeviceSync_SyncData(key_sync);
}

static bool deviceSyncPsKey_SyncRxIndCallback(void *message)
{
    device_property_sync_t *sync_msg = (device_property_sync_t *)message;
    DevicePsKey_Write(BtDevice_GetDeviceForBdAddr(&sync_msg->addr), sync_msg->id, sync_msg->data, sync_msg->size);

    return TRUE;
}

static void deviceSyncPsKey_SyncCfm(device_t device, uint8 id)
{
    DevicePsKey_ClearFlag(device, id, device_ps_key_flag_needs_sync);
    DeviceDbSerialiser_SerialiseDevice(device);
}

static void deviceSyncPsKey_ReadAndSendPskey(device_t device, device_pskey_data_id_t data_id)
{
    uint16 data_size = 0;
    uint8 *data = DevicePsKey_Read(device, data_id, &data_size);
    if(data)
    {
        deviceSyncPsKey_Sync(device, data_id, data, data_size);
        free(data);
    }
}

static void deviceSyncPsKey_PeerConnectedCallback(void)
{
    DEBUG_LOG_VERBOSE("deviceSyncPsKey_PeerConnectedCallback PEER_SIG_CONNECTION_IND");
    if(BtDevice_IsMyAddressPrimary())
    {
        DEBUG_LOG_VERBOSE("deviceSyncPsKey_PeerConnectedCallback I'm primary");

        deviceType type = DEVICE_TYPE_HANDSET;
        device_t* devices = NULL;
        unsigned num_devices = 0;

        DeviceList_GetAllDevicesWithPropertyValue(device_property_type, &type, sizeof(deviceType), &devices, &num_devices);
        if (devices && num_devices)
        {
            DEBUG_LOG_VERBOSE("deviceSyncPsKey_PeerConnectedCallback It seems that there is %d handsets", num_devices);
            for (unsigned i=0; i< num_devices; i++)
            {
                if(DevicePsKey_IsFlagSet(devices[i], device_pskey_remote_device_name, device_ps_key_flag_needs_sync))
                {
                    if(KeySync_IsDeviceInSync(devices[i]))
                    {
                        deviceSyncPsKey_ReadAndSendPskey(devices[i], device_pskey_remote_device_name);
                    }
                }
            }
        }
        free(devices);
        devices = NULL;
    }
}


static void deviceSyncPsKey_DeviceAddedToPeerCallback(device_t device)
{
    if(BtDevice_IsMyAddressPrimary())
    {
        deviceSyncPsKey_ReadAndSendPskey(device, device_pskey_remote_device_name);
    }
}

static const device_sync_callback_t sync_callback = {
        .SyncRxIndCallback = deviceSyncPsKey_SyncRxIndCallback,
        .SyncCfmCallback = deviceSyncPsKey_SyncCfm,
        .PeerConnectedCallback = deviceSyncPsKey_PeerConnectedCallback,
        .DeviceAddedToPeerCallback = deviceSyncPsKey_DeviceAddedToPeerCallback
};

static void deviceSyncPsKey_WriteCallback(device_t device, device_pskey_data_id_t data_id, uint8 *data, uint16 data_size)
{
    if(BtDevice_IsMyAddressPrimary())
    {
        DevicePsKey_SetFlag(device, data_id, device_ps_key_flag_needs_sync);
        deviceSyncPsKey_Sync(device, data_id, data, data_size);
    }
}

static const device_pskey_callback_t write_callback = {
        .Write = deviceSyncPsKey_WriteCallback
};

bool DeviceSyncPsKey_Init(Task init_task)
{
    UNUSED(init_task);

    DeviceSync_RegisterCallback(device_sync_client_device_pskey, &sync_callback);
    DevicePsKey_RegisterCallback(&write_callback);

    return TRUE;
}

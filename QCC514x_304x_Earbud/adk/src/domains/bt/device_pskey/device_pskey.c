/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Provides access to ps keys associated with a device.

Only one data_id is supported at the moment.

*/

#include "device_pskey.h"

#include <ps_key_map.h>
#include <pddu_map.h>
#include <device_list.h>
#include <device_db_serialiser.h>
#include <device_properties.h>
#include <bt_device.h>

#include <logging.h>
#include <panic.h>
#include <ps.h>

#include <stdlib.h>


typedef struct
{
    uint8 data_id;
    uint8 flags;
    uint16 ps_key;
} device_pskey_entry_t;


device_pskey_callback_t *client_callback;

static uint8 devicePsKey_GetPdduSize(device_t device)
{
    UNUSED(device);
    DEBUG_LOG_VERBOSE("devicePsKey_GetPdduSize lap 0x%x", DeviceProperties_GetBdAddr(device).lap);
    if(BtDevice_GetDeviceType(device) == DEVICE_TYPE_HANDSET)
    {
        return sizeof(device_pskey_entry_t);
    }
    else
    {
        return 0;
    }
}

static void devicePsKey_Serialise(device_t device, void *buf, uint8 offset)
{
    void *value = NULL;
    size_t size = sizeof(device_pskey_entry_t);
    device_pskey_entry_t entry = {0};

    UNUSED(offset);

    DEBUG_LOG_VERBOSE("devicePsKey_Serialise device 0x%x, buf %p", device, buf);

    if (Device_GetProperty(device, device_property_pskeys, &value, &size))
    {
        entry = *(device_pskey_entry_t *)value;
    }

    memcpy(buf, &entry, size);

    DEBUG_LOG_VERBOSE("devicePsKey_Serialise lap 0x%x offset %d", DeviceProperties_GetBdAddr(device).lap, offset);
}

static void devicePsKey_Deserialise(device_t device, void *buf, uint8 data_length, uint8 offset)
{
    UNUSED(device);
    UNUSED(buf);
    UNUSED(data_length);
    UNUSED(offset);

    PanicFalse(data_length >= sizeof(device_pskey_entry_t));

    device_pskey_entry_t entry;

    memcpy(&entry, buf, sizeof(device_pskey_entry_t));

    Device_SetProperty(device, device_property_pskeys, &entry, sizeof(device_pskey_entry_t));

    DEBUG_LOG_VERBOSE("devicePsKey_Deserialise lap 0x%x data len %d offset %d", DeviceProperties_GetBdAddr(device).lap, data_length, offset);
}

void DevicePsKey_RegisterPddu(void)
{
    DeviceDbSerialiser_RegisterPersistentDeviceDataUser(PDDU_ID_DEVICE_PSKEY, devicePsKey_GetPdduSize,
            devicePsKey_Serialise, devicePsKey_Deserialise);
}

void DevicePsKey_RegisterCallback(const device_pskey_callback_t *callback)
{
    client_callback = (device_pskey_callback_t *)callback;
}

static uint16 devicePsKey_AllocatePskey(uint16 first_pskey, uint16 last_pskey)
{
    uint16 pskey_id;
    uint8 i;
    device_t* devices = NULL;
    unsigned num_devices = 0;
    uint16 used_pskeys[8] = {0};

    void *value = NULL;
    size_t size = sizeof(device_pskey_entry_t);
    device_pskey_entry_t entry;

    DeviceList_GetAllDevicesWithProperty(device_property_pskeys, &devices, &num_devices);

    for(i = 0; i < num_devices; ++i)
    {
        if (Device_GetProperty(devices[i], device_property_pskeys, &value, &size))
        {
            entry = *(device_pskey_entry_t *)value;
            used_pskeys[i] = entry.ps_key;
        }
    }

    free(devices);

    for(pskey_id = first_pskey; pskey_id <= last_pskey; ++pskey_id)
    {
        bool found = FALSE;
        for(i = 0; i < num_devices; ++i)
        {
            if(pskey_id == used_pskeys[i])
            {
                found = TRUE;
            }
        }
        if(!found)
        {
            return pskey_id;
        }
    }

    DEBUG_LOG_VERBOSE("devicePsKey_AllocatePskey no ps key");
    return 0;
}

static device_pskey_entry_t devicePsKey_GetEntry(device_t device, device_pskey_data_id_t data_id, bool serialise)
{
    void *value = NULL;
    size_t size = 0;
    device_pskey_entry_t *entry;

    if(device == 0)
    {
        DEBUG_LOG_WARN("devicePsKey_GetEntry device is invalid");
        Panic();
    }

    DEBUG_LOG_VERBOSE("devicePsKey_GetEntry device 0x%x, lap 0x%x, data_id 0x%x", device, DeviceProperties_GetBdAddr(device).lap, data_id);

    if (Device_GetProperty(device, device_property_pskeys, &value, &size))
    {
        entry = (device_pskey_entry_t *)value;

        DEBUG_LOG_VERBOSE("devicePsKey_GetEntry property found, data_id 0x%x, ps key %d ", entry->data_id, entry->ps_key);

        if(entry->data_id == data_id)
        {
            if(entry->ps_key < PS_KEY_DEVICE_PS_KEY_FIRST || entry->ps_key > PS_KEY_DEVICE_PS_KEY_LAST)
            {
                Panic();
            }

        }
        else
        {
            Panic();
        }
    }
    else
    {
        device_pskey_entry_t new_entry;
        new_entry.ps_key = devicePsKey_AllocatePskey(PS_KEY_DEVICE_PS_KEY_FIRST, PS_KEY_DEVICE_PS_KEY_LAST);
        new_entry.data_id = data_id;
        new_entry.flags = 0;

        DEBUG_LOG_VERBOSE("devicePsKey_GetEntry property not found, allocated data_id %d, ps key %d ", new_entry.data_id, new_entry.ps_key);

        if(new_entry.ps_key < PS_KEY_DEVICE_PS_KEY_FIRST || new_entry.ps_key > PS_KEY_DEVICE_PS_KEY_LAST)
        {
            Panic();
        }

        Device_SetProperty(device, device_property_pskeys, &new_entry, sizeof(new_entry));

        DEBUG_LOG_VERBOSE("devicePsKey_GetEntry deleting ps key %d", new_entry.ps_key);
        /* Delete just allocated ps key in case it was used before */
        PsStore(new_entry.ps_key, NULL, 0);

        if(serialise)
        {
            DeviceDbSerialiser_SerialiseDevice(device);
        }

        Device_GetProperty(device, device_property_pskeys, &value, &size);

        entry = (device_pskey_entry_t *)value;
    }

    return *entry;
}

uint16 DevicePsKey_Write(device_t device, device_pskey_data_id_t data_id, uint8 *data, uint16 data_size)
{
    uint16 written_words = 0;
    uint16 pskey_id = 0;
    device_pskey_entry_t entry = devicePsKey_GetEntry(device, data_id, FALSE);

    pskey_id = entry.ps_key;

    if(pskey_id)
    {
        uint16 num_of_words = PS_SIZE_ADJ(data_size);
        uint8 *buffer = PanicUnlessMalloc(num_of_words*sizeof(uint16));

        memset(buffer, 0, num_of_words*sizeof(uint16));
        memcpy(buffer, data, data_size);
        written_words = PsStore(pskey_id, buffer, num_of_words);

        free(buffer);

        entry.flags |= device_ps_key_flag_contains_data;
        Device_SetProperty(device, device_property_pskeys, &entry, sizeof(entry));

        if(client_callback)
        {
            client_callback->Write(device, data_id, data, data_size);
        }

        DeviceDbSerialiser_SerialiseDevice(device);

        DEBUG_LOG_VERBOSE("DevicePsKey_Write pskey %d data_size %d, num_of_words %d, written_words %d", pskey_id, data_size, num_of_words, written_words);
    }
    else
    {
        DEBUG_LOG_VERBOSE("DevicePsKey_Write can't find pskey");
    }

    return written_words;
}

uint8 *DevicePsKey_Read(device_t device, device_pskey_data_id_t data_id, uint16 *data_size)
{
    uint16 read_bytes = 0;
    uint16 pskey_id = 0;
    device_pskey_entry_t entry = devicePsKey_GetEntry(device, data_id, TRUE);

    pskey_id = entry.ps_key;

    DEBUG_LOG_VERBOSE("DevicePsKey_Read pskey_id %d", pskey_id);

    if(pskey_id)
    {
        uint16 num_of_words = PsRetrieve(pskey_id, NULL, 0);
        if(num_of_words > 0)
        {
            uint16 *buffer = PanicUnlessMalloc(num_of_words*sizeof(uint16));

            read_bytes = PsRetrieve(pskey_id, buffer, num_of_words) * sizeof(uint16);

            DEBUG_LOG_VERBOSE("DevicePsKey_Read num words %d, read bytes %d", num_of_words, read_bytes);

            *data_size = read_bytes;
            return (uint8 *)buffer;
        }
    }

    *data_size = 0;
    return NULL;
}

void DevicePsKey_SetFlag(device_t device, device_pskey_data_id_t data_id, device_pskey_flags_t flag)
{
    DEBUG_LOG_VERBOSE("DevicePsKey_SetFlag data_id 0x%x, flag 0x%x", data_id, flag);

    device_pskey_entry_t entry = devicePsKey_GetEntry(device, data_id, FALSE);

    entry.flags |= flag;
    Device_SetProperty(device, device_property_pskeys, &entry, sizeof(entry));

}

void DevicePsKey_ClearFlag(device_t device, device_pskey_data_id_t data_id, device_pskey_flags_t flag)
{
    DEBUG_LOG_VERBOSE("DevicePsKey_ClearFlag data_id 0x%x, flag 0x%x", data_id, flag);

    device_pskey_entry_t entry = devicePsKey_GetEntry(device, data_id, FALSE);
    entry.flags &= ~flag;
    Device_SetProperty(device, device_property_pskeys, &entry, sizeof(entry));
}

bool DevicePsKey_IsFlagSet(device_t device, device_pskey_data_id_t data_id, device_pskey_flags_t flag)
{
    DEBUG_LOG_VERBOSE("DevicePsKey_IsFlagSet data_id 0x%x, flag 0x%x", data_id, flag);

    device_pskey_entry_t entry = devicePsKey_GetEntry(device, data_id, TRUE);

    if(entry.flags & flag)
    {
        return TRUE;
    }

    return FALSE;
}

void DevicePsKey_ClearFlagInAllDevices(device_pskey_data_id_t data_id, device_pskey_flags_t flag)
{
    uint8 i;
    device_t* devices = NULL;
    unsigned num_devices = 0;

    void *value = NULL;
    size_t size = sizeof(device_pskey_entry_t);
    device_pskey_entry_t entry;

    UNUSED(data_id);

    DeviceList_GetAllDevicesWithProperty(device_property_pskeys, &devices, &num_devices);

    for(i = 0; i < num_devices; ++i)
    {
        if (Device_GetProperty(devices[i], device_property_pskeys, &value, &size))
        {
            entry = *(device_pskey_entry_t *)value;
            entry.flags &= ~flag;
            Device_SetProperty(devices[i], device_property_pskeys, &entry, sizeof(entry));
        }
    }

    free(devices);
}

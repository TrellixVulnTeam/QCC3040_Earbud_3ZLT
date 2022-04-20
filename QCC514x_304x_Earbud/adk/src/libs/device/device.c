/*!
\copyright  Copyright (c) 2018-2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      A device instance that represents a collection of profiles/services/etc.

    A device will usually be a connected remote device connected to the local
    device, but it could also be used to store the properties of the local device.
*/

#include <string.h>

#include <panic.h>

#include <device.h>

Device_OnPropertySet onPropertySetHandler;

static bool device_Add(device_t device, device_property_t id, const void *value, size_t size)
{
    if (KeyValueList_Add(device, id, value, size))
    {
        if(onPropertySetHandler)
        {
            onPropertySetHandler(device, id, value, size);
        }
        return TRUE;
    }
    return FALSE;
}

static bool device_UpdatePropertyIfExistingHelper(device_t device, device_property_t id, uint32 value, size_t size)
{
    if (!device_Add(device, id, &value, size))
    {
        Device_RemoveProperty(device, id);
        if (!device_Add(device, id, &value, size))
        {
            Panic();
        }
    }
    return TRUE;
}

/*****************************************************************************/

device_t Device_Create(void)
{
    return KeyValueList_Create();
}

void Device_Destroy(device_t* device)
{
    KeyValueList_Destroy(device);
}

bool Device_IsPropertySet(device_t device, device_property_t id)
{
    return KeyValueList_IsSet(device, id);
}

void Device_RemoveProperty(device_t device, device_property_t id)
{
    KeyValueList_Remove(device, id);
}

bool Device_SetProperty(device_t device, device_property_t id, const void *value, size_t size)
{
    if (!device_Add(device, id, value, size))
    {
        Device_RemoveProperty(device, id);
        PanicFalse(device_Add(device, id, value, size));
    }
    return TRUE;
}

bool Device_GetProperty(device_t device, device_property_t id, void **value, size_t *size)
{
    return KeyValueList_Get(device, id, value, size);
}

void *Device_GetPropertySized(device_t device, device_property_t id, size_t size)
{
    return KeyValueList_GetSized(device, id, size);
}

bool Device_SetPropertyPtr(device_t device, device_property_t id, const void *value)
{
    return device_Add(device, id, &value, sizeof(value));
}

void *Device_GetPropertyPtr(device_t device, device_property_t id)
{
    void **value = KeyValueList_GetSized(device, id, sizeof(value));
    return value ? *value : NULL;
}

bool Device_SetPropertyU32(device_t device, device_property_t id, uint32 value)
{
    return device_UpdatePropertyIfExistingHelper(device, id, value, sizeof(value));
}

bool Device_GetPropertyU32(device_t device, device_property_t id, uint32 *value)
{
    uint32 *u32 = KeyValueList_GetSized(device, id, sizeof(*u32));
    if (u32)
    {
        *value = *u32;
        return TRUE;
    }
    return FALSE;
}

bool Device_SetPropertyU16(device_t device, device_property_t id, uint16 value)
{
    return device_UpdatePropertyIfExistingHelper(device, id, value, sizeof(value));
}

bool Device_GetPropertyU16(device_t device, device_property_t id, uint16 *value)
{
    uint16 *u16 = KeyValueList_GetSized(device, id, sizeof(*u16));
    if (u16)
    {
        *value = *u16;
        return TRUE;
    }
    return FALSE;
}

bool Device_SetPropertyU8(device_t device, device_property_t id, uint8 value)
{
    return device_UpdatePropertyIfExistingHelper(device, id, value, sizeof(value));
}

bool Device_GetPropertyU8(device_t device, device_property_t id, uint8 *value)
{
    uint8 *u8 = KeyValueList_GetSized(device, id, sizeof(*u8));
    if (u8)
    {
        *value = *u8;
        return TRUE;
    }
    return FALSE;
}

void Device_RegisterOnPropertySetHandler(Device_OnPropertySet handler)
{
    if(onPropertySetHandler != NULL && handler != NULL)
    {
        /* Only one client is supported, however it allowed to set onPropertySetHandler to NULL*/
        Panic();
    }
    onPropertySetHandler = handler;
}

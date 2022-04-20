/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   ui_domain UI
\ingroup    domains
\brief      User-defined touchpad gesture to UI Input mapping configuration.
*/

#include "ui_user_config.h"
#include "ui_user_config_context_id_to_ui_provider_map.h"

#include <device_db_serialiser.h>
#include <device_list.h>
#include <device_properties.h>
#include <device_types.h>
#include <macros.h>
#include <panic.h>
#include <pddu_map.h>

static uint8 uiUserConfig_GetDeviceDataLen(device_t device)
{
    void *user_config_table = NULL;
    size_t user_config_table_size = 0;

    Device_GetProperty(device, device_property_ui_user_gesture_table, &user_config_table, &user_config_table_size);

    return user_config_table_size;
}

static void uiUserConfig_SerialisePersistentDeviceData(device_t device, void *buf, uint8 offset)
{
    UNUSED(offset);

    void *user_config_table = NULL;
    size_t user_config_table_size = 0;

    if (Device_GetProperty(device, device_property_ui_user_gesture_table, &user_config_table, &user_config_table_size))
    {
        memcpy(buf, user_config_table, user_config_table_size);
    }
}

static void uiUserConfig_DeserialisePersistentDeviceData(device_t device, void *buffer, uint8 data_length, uint8 offset)
{
    UNUSED(offset);

    Device_SetProperty(device, device_property_ui_user_gesture_table, buffer, data_length);
}

bool UiUserConfig_Init(Task init_task)
{
    UNUSED(init_task);

    return TRUE;
}

void UiUserConfig_RegisterPddu(void)
{
    DeviceDbSerialiser_RegisterPersistentDeviceDataUser(
        PDDU_ID_UI_USER_CONFIG,
        uiUserConfig_GetDeviceDataLen,
        uiUserConfig_SerialisePersistentDeviceData,
        uiUserConfig_DeserialisePersistentDeviceData);
}

void UiUserConfig_RegisterContextIdMap(
        ui_providers_t provider,
        const ui_user_config_context_id_map * map,
        uint8 map_length)
{
    PanicFalse(provider < ui_providers_max);
    PanicNull((ui_user_config_context_id_map *)map);
    PanicFalse(map_length != 0);

    UiUserConfig_AddProviderMap(provider, map, map_length);
}

void UiUserConfig_SetUserGestureConfiguration(ui_user_gesture_table_content_t * table, size_t size)
{
    deviceType type = DEVICE_TYPE_SELF;
    device_t device = DeviceList_GetFirstDeviceWithPropertyValue(device_property_type, &type, sizeof(deviceType));
    Device_SetProperty(device,
                       device_property_ui_user_gesture_table,
                       table,
                       size);
}

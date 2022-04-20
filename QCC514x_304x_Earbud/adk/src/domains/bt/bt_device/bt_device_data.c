/*!
\copyright  Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Setting and getting device properties to/from persistent data.

*/

#include "bt_device.h"
#include "bt_device_typedef.h"
#include <device_properties.h>

#include <logging.h>
#include <panic.h>

void BtDevice_GetDeviceData(device_t device, bt_device_pdd_t *device_data)
{
    DEBUG_LOG("BtDevice_GetDeviceData");

    void *property_value = NULL;
    size_t size;

    Device_GetPropertyU8(device, device_property_audio_volume, &device_data->a2dp_volume);
    Device_GetPropertyU8(device, device_property_voice_volume, &device_data->hfp_volume);
    Device_GetPropertyU8(device, device_property_hfp_mic_gain, &device_data->hfp_mic_gain);
    Device_GetPropertyU8(device, device_property_hfp_profile, &device_data->hfp_profile);
    device_data->reserved_1 = 0;
    device_data->reserved_2 = 0;
    Device_GetPropertyU32(device, device_property_supported_profiles, &device_data->supported_profiles);

    Device_GetPropertyU16(device, device_property_flags, &device_data->flags);

    if (Device_GetProperty(device, device_property_type, &property_value, &size))
    {
        PanicFalse(size == sizeof(deviceType));
        device_data->type = *((deviceType *)property_value);
    }

    if (Device_GetProperty(device, device_property_link_mode, &property_value, &size))
    {
        PanicFalse(size == sizeof(deviceLinkMode));
        device_data->link_mode = *((deviceLinkMode *)property_value);
    }

    Device_GetPropertyU16(device, device_property_sco_fwd_features, &device_data->sco_fwd_features);
    Device_GetPropertyU16(device, device_property_battery_server_config_l, &device_data->battery_server_config_l);
    Device_GetPropertyU16(device, device_property_battery_server_config_r, &device_data->battery_server_config_r);
    Device_GetPropertyU16(device, device_property_gatt_server_config, &device_data->gatt_server_config);
    Device_GetPropertyU8(device, device_property_gatt_server_services_changed, &device_data->gatt_server_services_changed);
    Device_GetPropertyU8(device, device_property_voice_assistant, &device_data->voice_assistant);
    Device_GetPropertyU8(device, device_property_device_test_service, &device_data->dts);
    Device_GetPropertyU8(device, device_property_va_flags, &device_data->va_flags);

    if (Device_GetProperty(device, device_property_va_locale, &property_value, &size))
    {
        PanicFalse(size == sizeof device_data->va_locale);
        memcpy(device_data->va_locale, property_value, size);
    }

    Device_GetPropertyU32(device, device_property_headset_service_config, &device_data->headset_service_config);
    Device_GetPropertyU8(device, device_property_analog_audio_volume, &device_data->analog_audio_volume);
}

void BtDevice_SetDeviceData(device_t device, const bt_device_pdd_t *device_data)
{
    DEBUG_LOG("BtDevice_SetDeviceData device-type %d", device_data->type);

    Device_SetProperty(device, device_property_type, &device_data->type, sizeof(device_data->type));
    Device_SetPropertyU16(device, device_property_flags, device_data->flags);
    Device_SetProperty(device, device_property_link_mode, &device_data->link_mode, sizeof(device_data->link_mode));
    Device_SetPropertyU8(device, device_property_voice_assistant, device_data->voice_assistant);
    Device_SetPropertyU8(device, device_property_va_flags, device_data->va_flags);
    Device_SetProperty(device, device_property_va_locale, device_data->va_locale, sizeof device_data->va_locale);

    if (device_data->dts)
    {
        Device_SetPropertyU8(device, device_property_device_test_service, device_data->dts);
    }

    switch(device_data->type)
    {
        case DEVICE_TYPE_EARBUD:
            Device_SetPropertyU16(device, device_property_sco_fwd_features, device_data->sco_fwd_features);
            Device_SetPropertyU32(device, device_property_supported_profiles, device_data->supported_profiles);
            break;
        /* fall-through */
        case DEVICE_TYPE_SINK:
        case DEVICE_TYPE_HANDSET:
            Device_SetPropertyU8(device, device_property_audio_volume, device_data->a2dp_volume);
            Device_SetPropertyU8(device, device_property_voice_volume, device_data->hfp_volume);
            Device_SetPropertyU8(device, device_property_hfp_mic_gain, device_data->hfp_mic_gain);
            Device_SetPropertyU8(device, device_property_hfp_profile, device_data->hfp_profile);
            Device_SetPropertyU8(device, device_property_gatt_server_services_changed, device_data->gatt_server_services_changed);
            Device_SetPropertyU16(device, device_property_battery_server_config_l, device_data->battery_server_config_l);
            Device_SetPropertyU16(device, device_property_battery_server_config_r, device_data->battery_server_config_r);
            Device_SetPropertyU16(device, device_property_gatt_server_config, device_data->gatt_server_config);
            Device_SetPropertyU32(device, device_property_supported_profiles, device_data->supported_profiles);
            break;

        case DEVICE_TYPE_SELF:
            Device_SetPropertyU32(device, device_property_headset_service_config, device_data->headset_service_config);
            Device_SetPropertyU8(device, device_property_analog_audio_volume, device_data->analog_audio_volume);
            break;

        case DEVICE_TYPE_UNKNOWN:
        default:
            break;
    }

}

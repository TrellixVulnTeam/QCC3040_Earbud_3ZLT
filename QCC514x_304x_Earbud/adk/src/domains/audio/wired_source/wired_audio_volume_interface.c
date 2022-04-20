/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       wired_audio_volume_interface.c
\brief     The wired audio volume interface implementations.
*/

#include "wired_audio_volume_interface.h"
#include "wired_audio_private.h"
#include "audio_sources_list.h"
#include "volume_types.h"
#include <device.h>
#include <bt_device.h>
#include <device_list.h>
#include <device_properties.h>
#include <device_db_serialiser.h>

#define WA_VOLUME_MIN      0
#define WA_VOLUME_MAX      15
#define WA_VOLUME_STEPS    16
#define WA_VOLUME_CONFIG   { .range = { .min = WA_VOLUME_MIN, .max = WA_VOLUME_MAX }, .number_of_steps = WA_VOLUME_STEPS }
#define WA_VOLUME(step)    { .config = WA_VOLUME_CONFIG, .value = step }

static volume_t wiredAudio_GetVolume(audio_source_t source);
static void wiredAudio_SetVolume(audio_source_t source, volume_t volume);

static const audio_source_volume_interface_t wired_source_volume_interface =
{
    .GetVolume = wiredAudio_GetVolume,
    .SetVolume = wiredAudio_SetVolume
};

static volume_t wiredAudio_GetVolume(audio_source_t source)
{
    volume_t volume = WA_VOLUME(WA_VOLUME_MIN);
    switch(source)
    {
        case audio_source_line_in:
            volume.value = WIRED_AUDIO_DEFAULT_VOLUME;
            deviceType type = DEVICE_TYPE_SELF;
            device_t device = DeviceList_GetFirstDeviceWithPropertyValue(device_property_type, &type, sizeof(deviceType));
            if(device)
            {
                bool volume_set = FALSE;
                volume_set = Device_GetPropertyU8(device, device_property_analog_audio_volume, (uint8 *)&volume.value);
                if(!volume_set)
                    Device_SetPropertyU8(device, device_property_analog_audio_volume, volume.value);
            }
            break;

        default:
            break;
    }

    return volume;
}

static void wiredAudio_SetVolume(audio_source_t source, volume_t volume)
{
     switch(source)
    {
        case audio_source_line_in:
            {
                deviceType type = DEVICE_TYPE_SELF;
                device_t device = DeviceList_GetFirstDeviceWithPropertyValue(device_property_type, &type, sizeof(deviceType));
                Device_SetPropertyU8(device, device_property_analog_audio_volume, volume.value);
                DeviceDbSerialiser_Serialise();
            }
            break;

        default:
            break;
    }
}

const audio_source_volume_interface_t * WiredAudioSource_GetWiredVolumeInterface(void)
{
    return &wired_source_volume_interface;
}


/*!
\copyright  Copyright (c) 2019-2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   a2dp_profile
\brief      The audio source volume interface implementation for A2DP sources
*/

#include "a2dp_profile_volume.h"

#include "a2dp_profile.h"
#include "a2dp_profile_config.h"
#include "av.h"
#include "av_instance.h"
#include "audio_sources_list.h"
#include "kymera_config.h"
#include "volume_types.h"

#include <device.h>
#include <device_list.h>
#include <device_properties.h>
#include <mirror_profile.h>

#define A2DP_VOLUME_MIN      0
#define A2DP_VOLUME_MAX      127
#define A2DP_VOLUME_STEPS    16
#define A2DP_VOLUME_CONFIG   { .range = { .min = A2DP_VOLUME_MIN, .max = A2DP_VOLUME_MAX }, .number_of_steps = A2DP_VOLUME_STEPS }
#define A2DP_VOLUME(step)    { .config = A2DP_VOLUME_CONFIG, .value = step }

static volume_t a2dpProfile_GetVolume(audio_source_t source);
static void a2dpProfile_SetVolume(audio_source_t source, volume_t volume);

static const audio_source_volume_interface_t a2dp_volume_interface =
{
    .GetVolume = a2dpProfile_GetVolume,
    .SetVolume = a2dpProfile_SetVolume
};

static volume_t a2dpProfile_GetVolume(audio_source_t source)
{
    volume_t volume = A2DP_VOLUME(A2DP_VOLUME_MIN);
    avInstanceTaskData * theInst = AvInstance_GetSinkInstanceForAudioSource(source);
    if (theInst != NULL)
    {
        volume.value = theInst->volume;
    }
    else if (MirrorProfile_IsConnected())
    {
        bdaddr * mirror_addr = NULL;
        mirror_addr = MirrorProfile_GetMirroredDeviceAddress();
        device_t device = DeviceList_GetFirstDeviceWithPropertyValue(device_property_bdaddr, (void *)mirror_addr, sizeof(bdaddr));
        if (!DeviceProperties_GetAudioVolume(device, volume.config, &volume))
        {
            /* If the audio volume couldn't be read from the device properties
               fallback to the default A2DP volume. */
            volume = A2dpProfile_GetDefaultVolume();
        }
    }
    return volume;
}

static void a2dpProfile_SetVolume(audio_source_t source, volume_t volume)
{
    avInstanceTaskData * theInst = AvInstance_GetSinkInstanceForAudioSource(source);
    if (theInst != NULL)
    {
        theInst->volume = volume.value;
        appAvConfigStore();
    }
    else if (MirrorProfile_IsConnected())
    {
        bdaddr * mirror_addr = NULL;
        mirror_addr = MirrorProfile_GetMirroredDeviceAddress();
        device_t device = DeviceList_GetFirstDeviceWithPropertyValue(device_property_bdaddr, (void *)mirror_addr, sizeof(bdaddr));
        if (device)
        {
            DeviceProperties_SetAudioVolume(device, volume);
            appAvConfigStore();
        }
    }
}

const audio_source_volume_interface_t * A2dpProfile_GetAudioSourceVolumeInterface(void)
{
    return &a2dp_volume_interface;
}

volume_t A2dpProfile_GetDefaultVolume(void)
{
    /* Set default volume as set in configuration */
    const int rangeDb = appConfigMaxVolumedB() - appConfigMinVolumedB();
    const uint8 volume = (uint8)((appConfigDefaultVolumedB() - appConfigMinVolumedB()) * A2DP_VOLUME_MAX / rangeDb);
    volume_t default_volume = A2DP_VOLUME(volume);
    return default_volume;
}

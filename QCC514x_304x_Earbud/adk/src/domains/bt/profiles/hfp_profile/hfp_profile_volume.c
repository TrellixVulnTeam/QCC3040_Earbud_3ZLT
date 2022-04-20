/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   hfp_profile
\brief      The voice source volume interface implementation for HFP sources
*/

#include "hfp_profile_volume.h"

#include "hfp_profile.h"
#include "hfp_profile_config.h"
#include "hfp_profile_instance.h"
#include "hfp_profile_private.h"
#include "voice_sources.h"
#include "voice_sources_list.h"
#include "volume_messages.h"
#include "volume_utils.h"
#include "volume_types.h"

#include <bt_device.h>
#include <device.h>
#include <device_list.h>
#include <device_properties.h>
#include <logging.h>
#include <mirror_profile.h>

#define HFP_VOLUME_MIN      0
#define HFP_VOLUME_MAX      15
#define HFP_VOLUME_CONFIG   { .range = { .min = HFP_VOLUME_MIN, .max = HFP_VOLUME_MAX }, .number_of_steps = ((HFP_VOLUME_MAX - HFP_VOLUME_MIN) + 1) }
#define HFP_VOLUME(step)    { .config = HFP_VOLUME_CONFIG, .value = step }

static volume_t hfpProfile_GetVolume(voice_source_t source);
static void hfpProfile_SetVolume(voice_source_t source, volume_t volume);

static const voice_source_volume_interface_t hfp_volume_interface =
{
    .GetVolume = hfpProfile_GetVolume,
    .SetVolume = hfpProfile_SetVolume
};

static volume_t hfpProfile_GetVolume(voice_source_t source)
{
    volume_t volume = HFP_VOLUME(HFP_VOLUME_MIN);

    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForSource(source);
    if(instance != NULL)
    {
        device_t device = HfpProfileInstance_FindDeviceFromInstance(instance);
        DeviceProperties_GetVoiceVolume(device, volume.config, &volume);
    }
    else if (MirrorProfile_IsConnected())
    {
        bdaddr * mirror_addr = NULL;
        mirror_addr = MirrorProfile_GetMirroredDeviceAddress();
        device_t device = DeviceList_GetFirstDeviceWithPropertyValue(device_property_bdaddr, (void *)mirror_addr, sizeof(bdaddr));
        if (!DeviceProperties_GetVoiceVolume(device, volume.config, &volume))
        {
            /* If the audio volume couldn't be read from the device properties
               fallback to the default A2DP volume. */
            volume = HfpProfile_GetDefaultVolume();
        }
    }

    DEBUG_LOG_VERBOSE("hfpProfile_GetVolume enum:voice_source_t:%d %d",
                      source, volume.value);

    return volume;
}

static void hfpProfile_SetVolume(voice_source_t source, volume_t volume)
{
    DEBUG_LOG_VERBOSE("hfpProfile_SetVolume enum:voice_source_t:%d %d",
                      source, volume.value);

    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForSource(source);
    if(instance != NULL)
    {
        device_t device = HfpProfileInstance_FindDeviceFromInstance(instance);
        DeviceProperties_SetVoiceVolume(device, volume);
        HfpProfile_StoreConfig(device);
    }
    else if (MirrorProfile_IsConnected())
    {
        bdaddr * mirror_addr = NULL;
        mirror_addr = MirrorProfile_GetMirroredDeviceAddress();
        device_t device = BtDevice_GetDeviceForBdAddr(mirror_addr);
        if (device != NULL)
        {
            DeviceProperties_SetVoiceVolume(device, volume);
            HfpProfile_StoreConfig(device);
        }
    }
}

const voice_source_volume_interface_t * HfpProfile_GetVoiceSourceVolumeInterface(void)
{
    return &hfp_volume_interface;
}

void HfpProfileVolume_Init(voice_source_t source, uint8 init_volume)
{
    volume_t volume;
    volume = VoiceSources_GetVolume(source);
    volume.value = init_volume;
    VoiceSources_SetVolume(source, volume);
}

void HfpProfileVolume_NotifyClients(voice_source_t source, uint8 new_volume)
{
    MAKE_HFP_MESSAGE(APP_HFP_VOLUME_IND);
    message->source = source;
    message->volume = new_volume;
    TaskList_MessageSend(TaskList_GetFlexibleBaseTaskList(appHfpGetStatusNotifyList()), APP_HFP_VOLUME_IND, message);
}

volume_t HfpProfile_GetDefaultVolume(void)
{
    volume_t default_volume = HFP_VOLUME(HFP_SPEAKER_GAIN);
    return default_volume;
}

/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      The audio source observer interface implementation provided by Mirror Profile
*/

#ifdef INCLUDE_MIRRORING

#include "mirror_profile_private.h"
#include "mirror_profile_signalling.h"
#include "mirror_profile_volume_observer.h"

static void mirrorProfile_NotifyAbsoluteVolume(audio_source_t source, event_origin_t origin, volume_t volume);

static const audio_source_observer_interface_t mirror_observer_interface =
{
    .OnVolumeChange = mirrorProfile_NotifyAbsoluteVolume,
};

static void mirrorProfile_NotifyAbsoluteVolume(audio_source_t source, event_origin_t origin, volume_t volume)
{
    if(origin != event_origin_peer)
    {
        if(source == MirrorProfile_GetAudioSource())
        {
            MirrorProfile_SendA2dpVolumeToSecondary(source, volume.value);
        }
    }
}

static const audio_source_observer_interface_t * MirrorProfile_GetObserverInterface(void)
{
    return &mirror_observer_interface;
}

void mirrorProfile_RegisterForMirroredSourceVolume(void)
{
    audio_source_t source = MirrorProfile_GetAudioSource();
    if(source != audio_source_none)
    {
        AudioSources_RegisterObserver(source, MirrorProfile_GetObserverInterface());
    }
}

void mirrorProfile_UnregisterForMirroredSourceVolume(void)
{
    audio_source_t source = MirrorProfile_GetAudioSource();
    if(source != audio_source_none)
    {
        AudioSources_UnregisterObserver(source, MirrorProfile_GetObserverInterface());
    }
}

#endif /* INCLUDE_MIRRORING */

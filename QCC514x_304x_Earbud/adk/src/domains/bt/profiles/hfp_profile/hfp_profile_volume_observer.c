/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   hfp_profile
\brief      The voice source observer interface implementation for HFP sources
*/

#include "hfp_profile_volume_observer.h"
#include "hfp_profile.h"
#include "hfp_profile_instance.h"
#include "hfp_profile_private.h"
#include "hfp_profile_volume.h"
#include "hfp_profile_voice_source_link_prio_mapping.h"

static void hfpProfile_OnVolumeChange(voice_source_t source, event_origin_t origin, volume_t volume);

static const voice_source_observer_interface_t voice_source_observer_hfp =
{
    .OnVolumeChange = hfpProfile_OnVolumeChange,
};

static void hfpProfile_OnVolumeChange(voice_source_t source, event_origin_t origin, volume_t volume)
{
    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForSource(source);

    PanicNull(instance);

    if(origin != event_origin_external && HfpProfile_IsScoActiveForInstance(instance))
    {
        HfpVolumeSyncSpeakerGainRequest(HfpProfile_GetHfpLinkPrioForVoiceSource(source), (uint8 *)&volume.value);
    }
    if(origin != event_origin_peer)
    {
        HfpProfileVolume_NotifyClients(source, appHfpGetVolume(instance));
    }
}

const voice_source_observer_interface_t * HfpProfile_GetVoiceSourceObserverInterface(void)
{
    return &voice_source_observer_hfp;
}





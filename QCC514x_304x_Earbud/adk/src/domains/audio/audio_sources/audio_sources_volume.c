/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of the audio_sources_volume composite.
*/

#include "audio_sources.h"

#include <panic.h>

#include "audio_sources_interface_registry.h"
#include "volume_system.h"


/*! \brief Interface obtaining function

    \param source Audio source type

    \return Pointer to the interface
*/
static audio_source_volume_interface_t *audioSources_GetVolumeInterface(audio_source_t source)
{
    interface_list_t interface_list = AudioInterface_Get(source, audio_interface_type_volume_registry);
    return (((audio_source_volume_interface_t **)interface_list.interfaces)[0]);
}


void AudioSources_RegisterVolume(audio_source_t source, const audio_source_volume_interface_t * volume)
{
    /* Retrieve the registered inteface list */
    interface_list_t interface_list = AudioInterface_Get(source, audio_interface_type_volume_registry);
    audio_source_volume_interface_t **volume_interface_list = (audio_source_volume_interface_t **)interface_list.interfaces;

    /* Check if the interface is already registered */
    if(interface_list.number_of_interfaces)
    {
        if(volume == volume_interface_list[0])
            return;

        /* If the interface does not match, unregister the old interface */
        AudioInterface_UnRegister(source, audio_interface_type_volume_registry, volume_interface_list[0]);
    }

    /* Register the interface*/
    AudioInterface_Register(source, audio_interface_type_volume_registry, volume);
}

volume_t AudioSources_GetVolume(audio_source_t source)
{
    volume_t volume = FULL_SCALE_VOLUME;
    audio_source_volume_interface_t * interface = audioSources_GetVolumeInterface(source);

    if((interface != NULL) && interface->GetVolume)
    {
        volume = interface->GetVolume(source);
    }

    return volume;
}

void AudioSources_SetVolume(audio_source_t source, volume_t volume)
{
    audio_source_volume_interface_t * interface = audioSources_GetVolumeInterface(source);

    if((interface != NULL) && interface->SetVolume)
    {
        interface->SetVolume(source, volume);
    }
}

mute_state_t AudioSources_GetMuteState(audio_source_t source)
{
    mute_state_t mute_state = unmute;
    audio_source_volume_interface_t * interface = audioSources_GetVolumeInterface(source);

    if((interface != NULL) && interface->GetMuteState)
    {
        mute_state = interface->GetMuteState(source);
    }

    return mute_state;
}

void AudioSources_SetMuteState(audio_source_t source, mute_state_t mute_state)
{
    audio_source_volume_interface_t * interface = audioSources_GetVolumeInterface(source);

    if((interface != NULL) && interface->SetMuteState)
    {
        interface->SetMuteState(source, mute_state);
    }
}

volume_t AudioSources_CalculateOutputVolume(audio_source_t source)
{
    return Volume_CalculateOutputVolume(AudioSources_GetVolume(source),
                                        AudioSources_GetMuteState(source));
}

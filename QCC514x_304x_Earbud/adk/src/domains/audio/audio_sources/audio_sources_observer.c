/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of the audio_sources_observer composite.
*/

#include "audio_sources.h"

#include <panic.h>

#include "audio_sources_interface_registry.h"

typedef struct
{
    audio_source_observer_interface_t ** interface;
    unsigned number_of_interfaces;
    unsigned next_index;
}observer_iterator_t;

static void audioSources_PopulateNewObserverIterator(audio_source_t source, observer_iterator_t * iterator)
{
    interface_list_t interface_list = AudioInterface_Get(source, audio_interface_type_observer_registry);
    iterator->interface = (audio_source_observer_interface_t **)interface_list.interfaces;
    iterator->number_of_interfaces = interface_list.number_of_interfaces;
    iterator->next_index = 0;
}

static audio_source_observer_interface_t * audioSources_GetNextEntry(observer_iterator_t * iterator)
{
    audio_source_observer_interface_t * interface = NULL;

    PanicNull(iterator);

    if(iterator->next_index < iterator->number_of_interfaces)
    {
        interface = iterator->interface[iterator->next_index++];
    }

    return interface;
}

void AudioSources_RegisterObserver(audio_source_t source, const audio_source_observer_interface_t * observer)
{
    uint8 index = 0;

    /* Retrieve the registered inteface list */
    interface_list_t interface_list = AudioInterface_Get(source, audio_interface_type_observer_registry);
    audio_source_observer_interface_t **observer_interface_list = (audio_source_observer_interface_t **)interface_list.interfaces;

    /* Check if the interface is already registered */
    if(interface_list.number_of_interfaces)
    {
        while(index < interface_list.number_of_interfaces)
        {
            if(observer == observer_interface_list[index])
                return;
            index++;
        }

        if(interface_list.number_of_interfaces == MAX_OBSERVER_INTERFACES)
            Panic();
    }

    /* Register the interface */
    AudioInterface_Register(source, audio_interface_type_observer_registry, observer);
}

void AudioSources_OnVolumeChange(audio_source_t source, event_origin_t origin, volume_t volume)
{
    observer_iterator_t observer_iterator;
    audioSources_PopulateNewObserverIterator(source, &observer_iterator);
    audio_source_observer_interface_t * interface;
    
    while((interface = audioSources_GetNextEntry(&observer_iterator)) != NULL)
    {
        if(interface->OnVolumeChange)
        {
            interface->OnVolumeChange(source, origin, volume);
        }
    }
}

void AudioSources_OnAudioRoutingChange(audio_source_t source, audio_routing_change_t change)
{
    observer_iterator_t observer_iterator;
    audioSources_PopulateNewObserverIterator(source, &observer_iterator);
    audio_source_observer_interface_t * interface;

    while((interface = audioSources_GetNextEntry(&observer_iterator)) != NULL)
    {
        if(interface->OnAudioRoutingChange)
        {
            interface->OnAudioRoutingChange(source, change);
        }
    }
}

void AudioSources_OnMuteChange(audio_source_t source, event_origin_t origin, bool mute_state)
{
    observer_iterator_t observer_iterator;
    audioSources_PopulateNewObserverIterator(source, &observer_iterator);
    audio_source_observer_interface_t * interface;

    while((interface = audioSources_GetNextEntry(&observer_iterator)) != NULL)
    {
        if(interface->OnMuteChange)
        {
            interface->OnMuteChange(source, origin, mute_state);
        }
    }
}

void AudioSources_UnregisterObserver(audio_source_t source, const audio_source_observer_interface_t * observer)
{
    /* Register the interface */
    AudioInterface_UnRegister(source, audio_interface_type_observer_registry, observer);
}

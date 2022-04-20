/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of the voice_sources_observer composite.
*/

#include "voice_sources.h"
#include "volume_types.h"

#include <logging.h>
#include <panic.h>


#define MAX_VOICE_SOURCE_OBSERVERS_PER_SOURCE (2)

/*! \brief List of observers per voice source */
typedef struct
{
    const voice_source_observer_interface_t *observers[MAX_VOICE_SOURCE_OBSERVERS_PER_SOURCE];
} voice_source_observer_list_t;

/*! \brief The voice source observer registry

    References to observer interfaces are stored here as they are registered
*/
static voice_source_observer_list_t voice_source_observers[max_voice_sources];


static inline void voiceSources_ValidateSource(voice_source_t source)
{
    if((source <= voice_source_none) || (source >= max_voice_sources))
    {
        Panic();
    }
}

void VoiceSources_ObserverRegistryInit(void)
{
    memset(voice_source_observers, 0, sizeof(voice_source_observers));
}

void VoiceSources_RegisterObserver(voice_source_t source, const voice_source_observer_interface_t * interface)
{
    voiceSources_ValidateSource(source);
    PanicNull((void *)interface);

    /* Try to add the observer to an empty slot
       * If observer is already registered do nothing
       * If there are no empty slots panic
    */
    const voice_source_observer_interface_t **observer = NULL;

    /* Check if the observer is already registered. */
    ARRAY_FOREACH(observer, voice_source_observers[source].observers)
    {
        if (*observer == interface)
        {
            /* Already registered - return early */
            return;
        }
    }

    ARRAY_FOREACH(observer, voice_source_observers[source].observers)
    {
        if (*observer == NULL)
        {
            *observer = interface;
            return;
        }
    }

    /* Did not manage to register the observer */
    Panic();
}

void VoiceSources_DeregisterObserver(voice_source_t source, const voice_source_observer_interface_t * interface)
{
    voiceSources_ValidateSource(source);

    /* Try to find the matching registered obsver and set it to NULL */
    const voice_source_observer_interface_t **observer = NULL;
    ARRAY_FOREACH(observer, voice_source_observers[source].observers)
    {
        if (*observer == interface)
        {
            *observer = NULL;
            break;
        }
    }
}

void VoiceSources_OnVolumeChange(voice_source_t source, event_origin_t origin, volume_t volume)
{
    voiceSources_ValidateSource(source);

    const voice_source_observer_interface_t **observer = NULL;
    ARRAY_FOREACH(observer, voice_source_observers[source].observers)
    {
        if (*observer && (*observer)->OnVolumeChange)
        {
            (*observer)->OnVolumeChange(source, origin, volume);
        }
    }
}

void VoiceSources_OnMuteChange(voice_source_t source, event_origin_t origin, bool mute_state)
{
    voiceSources_ValidateSource(source);

    const voice_source_observer_interface_t **observer = NULL;
    ARRAY_FOREACH(observer, voice_source_observers[source].observers)
    {
        if (*observer && (*observer)->OnMuteChange)
        {
            (*observer)->OnMuteChange(source, origin, mute_state);
        }
    }
}

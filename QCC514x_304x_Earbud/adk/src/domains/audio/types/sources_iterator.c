/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   select_focus_domains Focus Select
\ingroup    focus_domains
\brief      API for iterating through the active audio sources in the registry.
*/

#include "sources_iterator.h"

#include <source_param_types.h>
#include "audio_sources_list.h"
#include "audio_sources.h"
#include "audio_sources_interface_registry.h"
#include <voice_sources.h>

#include <panic.h>
#include <stdlib.h>
#include <macros.h>

typedef struct generic_source_iterator_tag
{
    generic_source_t active_sources[max_audio_sources+max_voice_sources];
    uint8 num_active_sources;
    uint8 next_index;

} iterator_internal_t;

static unsigned sourcesIterator_GetSourceContext(generic_source_t source)
{
    switch(source.type)
    {
        case source_type_voice:
            return VoiceSources_GetSourceContext(source.u.voice);
        
        case source_type_audio:
            return AudioSources_GetSourceContext(source.u.audio);
        
        default:
            Panic();
            return 0;
    }
}

static bool sourcesIterator_SourceContextIsAllowed(generic_source_t source, const unsigned* contexts, const unsigned num_contexts) 
{
    unsigned i;
    unsigned context;
    
    /* No filtering required */
    if(contexts == NULL || num_contexts == 0)
    {
        return TRUE;
    }
    
    context = sourcesIterator_GetSourceContext(source);
    
    for(i = 0; i < num_contexts; i++)
    {
        if(context == contexts[i])
        {
            return TRUE;
        }
    }
    
    return FALSE;
}

static void sourcesIterator_AddSource(iterator_internal_t * iter, generic_source_t generic_source)
{
    PanicNull(iter);
    
    iter->active_sources[iter->num_active_sources] = generic_source;
    iter->num_active_sources++;
}

static void sourcesIterator_AddVoiceSourcesWithContextFilter(iterator_internal_t * iter, const unsigned* contexts, const unsigned num_contexts) 
{
    voice_source_t source = voice_source_none;
    while(++source < max_voice_sources)
    {
        if (VoiceSources_IsSourceRegisteredForTelephonyControl(source))
        {
            generic_source_t generic_source;
            generic_source.type = source_type_voice;
            generic_source.u.voice = source;
            
            if(sourcesIterator_SourceContextIsAllowed(generic_source, contexts, num_contexts))
            {
                sourcesIterator_AddSource(iter, generic_source);
            }
        }
    }
}

static void sourcesIterator_AddVoiceSources(iterator_internal_t * iter)
{
    sourcesIterator_AddVoiceSourcesWithContextFilter(iter, NULL, 0);
}

static void sourcesIterator_AddAudioSourcesWithContextFilter(iterator_internal_t * iter, const unsigned* contexts, const unsigned num_contexts) 
{
    interface_list_t list = {0};
    audio_source_t source = audio_source_none;
    while(++source < max_audio_sources)
    {
        list = AudioInterface_Get(source, audio_interface_type_media_control);
        if (list.number_of_interfaces >= 1)
        {
            generic_source_t generic_source;
            generic_source.type = source_type_audio;
            generic_source.u.audio = source;
            
            if(sourcesIterator_SourceContextIsAllowed(generic_source, contexts, num_contexts))
            {
                sourcesIterator_AddSource(iter, generic_source);
            }
        }
    }
}

static void sourcesIterator_AddAudioSources(iterator_internal_t * iter) 
{
    sourcesIterator_AddAudioSourcesWithContextFilter(iter, NULL, 0);
}

sources_iterator_t SourcesIterator_Create(source_type_t type)
{
    iterator_internal_t * iter = NULL;

    iter = (iterator_internal_t *)PanicUnlessMalloc(sizeof(iterator_internal_t));
    memset(iter, 0, sizeof(iterator_internal_t));

    if (type == source_type_max)
    {
        sourcesIterator_AddVoiceSources(iter);
        sourcesIterator_AddAudioSources(iter);
    }
    else if (type == source_type_audio)
    {
        sourcesIterator_AddAudioSources(iter);
    }
    else if (type == source_type_voice)
    {
        sourcesIterator_AddVoiceSources(iter);
    }
    return iter;
}

void SourcesIterator_AddSourcesInContextArray(sources_iterator_t iterator, source_type_t type, const unsigned* contexts, const unsigned num_contexts)
{
    PanicNull(iterator);
    
    if(type == source_type_audio)
    {
        sourcesIterator_AddAudioSourcesWithContextFilter(iterator, contexts, num_contexts);
    }
    else if(type == source_type_voice)
    {
        sourcesIterator_AddVoiceSourcesWithContextFilter(iterator, contexts, num_contexts);
    }
}

void SourcesIterator_RemoveSource(sources_iterator_t iterator, generic_source_t source)
{
    bool found = FALSE;
    unsigned index;
    
    PanicNull(iterator);
    
    for(index = 0; index < iterator->num_active_sources; index++)
    {
        found = found || GenericSource_IsSame(iterator->active_sources[index], source);
        
        if(found)
        {
            unsigned next_index = index + 1;
            if(next_index < iterator->num_active_sources)
            {
                iterator->active_sources[index] = iterator->active_sources[index + 1];
            }
        }
    }
    
    if(found)
    {
        iterator->num_active_sources--;
    }
}

void SourcesIterator_RemoveVoiceSource(sources_iterator_t iterator, voice_source_t voice_source)
{
    generic_source_t source;
    source.type = source_type_voice;
    source.u.voice = voice_source;
    SourcesIterator_RemoveSource(iterator, source);
}

void SourcesIterator_RemoveAudioSource(sources_iterator_t iterator, audio_source_t audio_source)
{
    generic_source_t source;
    source.type = source_type_audio;
    source.u.audio = audio_source;
    SourcesIterator_RemoveSource(iterator, source);
}

generic_source_t SourcesIterator_NextGenericSource(sources_iterator_t iterator)
{
    generic_source_t next_source = { .type=source_type_invalid, .u=voice_source_none};
    PanicNull(iterator);

    if (iterator->next_index < iterator->num_active_sources)
    {
        next_source = iterator->active_sources[iterator->next_index++];
    }

    return next_source;
}

void SourcesIterator_Destroy(sources_iterator_t iterator)
{
    PanicNull(iterator);

    memset(iterator, 0, sizeof(iterator_internal_t));
    free(iterator);
}

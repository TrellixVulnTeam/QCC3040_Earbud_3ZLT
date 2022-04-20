/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      This module is used to store/retrieve cached focus status data
*/

#include "focus_select.h"
#include "focus_select_status.h"

#include <audio_sources.h>
#include <logging.h>
#include <panic.h>

static source_cache_data_t* focusSelect_GetCacheDataForVoiceSource(focus_status_t * focus_status, voice_source_t voice)
{
    unsigned index;
    PanicFalse(VoiceSource_IsValid(voice));
    if(VoiceSource_IsValid(voice))
    {
        index = CONVERT_VOICE_SOURCE_TO_ARRAY_INDEX(voice);
        return &focus_status->cache_data_by_voice_source_array[index];
    }
    return NULL;
}

static source_cache_data_t* focusSelect_GetCacheDataForAudioSource(focus_status_t * focus_status, audio_source_t audio)
{
    unsigned index;
    PanicFalse(AudioSource_IsValid(audio));
    if(AudioSource_IsValid(audio))
    {
        index = CONVERT_AUDIO_SOURCE_TO_ARRAY_INDEX(audio);
        return &focus_status->cache_data_by_audio_source_array[index];
    }
    return NULL;
}

static source_cache_data_t* focusSelect_GetCacheDataForSource(focus_status_t * focus_status, generic_source_t source)
{
    if (GenericSource_IsAudio(source))
    {
        return focusSelect_GetCacheDataForAudioSource(focus_status, source.u.audio);
    }
    else if (GenericSource_IsVoice(source))
    {
        return focusSelect_GetCacheDataForVoiceSource(focus_status, source.u.voice);
    }
    else
    {
        Panic();
        return NULL;
    }
}

source_cache_data_t * FocusSelect_SetCacheDataForSource(focus_status_t * focus_status, generic_source_t source, unsigned context, bool has_audio, uint8 priority)
{
    source_cache_data_t * cache = focusSelect_GetCacheDataForSource(focus_status, source);
    
    PanicFalse(source.type != source_type_invalid);
    if(cache)
    {
        cache->has_audio = has_audio;
        cache->context = context;
        cache->priority = priority;
    }
    return cache;
}

static unsigned focusSelect_GetContext(focus_status_t* focus_status, generic_source_t source)
{
    source_cache_data_t* data = PanicNull(focusSelect_GetCacheDataForSource(focus_status, source));
    
    return data->context;
}

bool FocusSelect_IsAudioSourceContextConnected(focus_status_t* focus_status, audio_source_t source)
{
    generic_source_t gen_source = {.type = source_type_audio, .u.audio = source};
    return focusSelect_GetContext(focus_status, gen_source) == context_audio_connected;
}

bool FocusSelect_IsSourceContextHighestPriority(focus_status_t * focus_status, generic_source_t source)
{
    source_cache_data_t* data = PanicNull(focusSelect_GetCacheDataForSource(focus_status, source));
    return (data->priority == focus_status->highest_priority);
}

bool FocusSelect_CompileFocusStatus(sources_iterator_t iter, focus_status_t * focus_status, priority_calculator_fn_t calculate_priority)
{
    bool source_found = FALSE;
    generic_source_t curr_source = SourcesIterator_NextGenericSource(iter);

    while (GenericSource_IsValid(curr_source))
    {
        /* Compute the priority for the source and assign it to the cache. */
        source_cache_data_t * cache = calculate_priority(focus_status, curr_source);

        /* Compare the source priority with the previous highest priority generic source. */
        uint8 previous_highest_priority = focus_status->highest_priority;

        DEBUG_LOG_V_VERBOSE("FocusSelect_CompileFocusStatus src_type=%d src=%d prios this=%x prev_highest=%x",
                            curr_source.type, curr_source.u.voice, cache->priority, previous_highest_priority);

        if (!source_found || cache->priority > previous_highest_priority)
        {
            /* New highest priority source found */
            focus_status->highest_priority_source = curr_source;
            focus_status->highest_priority_context = cache->context;
            focus_status->highest_priority_source_has_audio = cache->has_audio;
            focus_status->highest_priority = cache->priority;
            focus_status->num_highest_priority_sources = 1;
            source_found = TRUE;
        }
        else if (cache->priority == previous_highest_priority)
        {
            /* The sources have equal priority, this may cause a tie break to occur later */
            focus_status->num_highest_priority_sources += 1;
        }
        else
        {
            /* Indicates the source is lower priority than existing, do nothing */
        }

        curr_source = SourcesIterator_NextGenericSource(iter);
    }

    return source_found;
}


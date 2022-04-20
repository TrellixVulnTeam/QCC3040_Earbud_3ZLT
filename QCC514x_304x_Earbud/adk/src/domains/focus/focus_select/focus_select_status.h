/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Focus Select status API
*/
#ifndef FOCUS_SELECT_STATUS_H
#define FOCUS_SELECT_STATUS_H

#include <sources_iterator.h>

#define CONVERT_AUDIO_SOURCE_TO_ARRAY_INDEX(x) ((x)-1)
#define CONVERT_VOICE_SOURCE_TO_ARRAY_INDEX(x) ((x)-1)

typedef struct
{
    unsigned context;
    bool has_audio;
    uint8 priority;
} source_cache_data_t;

/* Used to collect context information from the voice and audio sources available
   in the framework, in a standard form. This data can then be processed to decide
   which source should be assigned foreground focus. */
typedef struct
{
    source_cache_data_t cache_data_by_voice_source_array[CONVERT_VOICE_SOURCE_TO_ARRAY_INDEX(max_voice_sources)];
    source_cache_data_t cache_data_by_audio_source_array[CONVERT_AUDIO_SOURCE_TO_ARRAY_INDEX(max_audio_sources)];
    generic_source_t highest_priority_source;
    bool highest_priority_source_has_audio;
    unsigned highest_priority_context;
    uint8 highest_priority;
    uint8 num_highest_priority_sources;
} focus_status_t;


/* Functions of this type shall compute the priority of the current source and assign the focus status
   cache with the result. They shall return a pointer to the assigned cache struct. */
typedef source_cache_data_t* (*priority_calculator_fn_t)(focus_status_t * focus_status, generic_source_t source);

/*! \brief Set the cache data associated with a source

    \param focus_status - The existing cache data
    \param source - The source to set data for
    \param context - The source's context
    \param has_audio - TRUE if the source has audio available, otherwise FALSE
    \param priority - The priority assigned to the source
    
    \return pointer to the updated cache data
*/
source_cache_data_t* FocusSelect_SetCacheDataForSource(focus_status_t * focus_status, generic_source_t source, unsigned context, bool has_audio, uint8 priority);

/*! \brief Check if an audio source has connected context set in the cache data

    \param focus_status - The existing cache data
    \param source - The source to check
    
    \return TRUE if the context is connected, otherwise FALSE
*/
bool FocusSelect_IsAudioSourceContextConnected(focus_status_t* focus_status, audio_source_t source);

/*! \brief Check if an audio source has highest priority of all sources added to the cache data

    \param focus_status - The existing cache data
    \param source - The source to check
    
    \return TRUE if the source has highest priority, otherwise FALSE
*/
bool FocusSelect_IsSourceContextHighestPriority(focus_status_t * focus_status, generic_source_t source);

/*! \brief Compile the focus status of all sources in the iterator using the priority calculator function

    \param iter - Sources iterator to use
    \param focus_status - The existing cache data
    \param calculate_priority - Callback to calculate the priority of a generic source
    
    \return TRUE if one or more sources were added to the cache data, otherwise FALSE
*/
bool FocusSelect_CompileFocusStatus(sources_iterator_t iter, focus_status_t * focus_status, priority_calculator_fn_t calculate_priority);

#endif /* FOCUS_SELECT_STATUS_H */

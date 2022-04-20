/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of audio router data handling.
*/

#include "audio_router.h"
#include "logging.h"
#include "audio_router_typedef.h"
#include "audio_router_data.h"
#include "panic.h"
#include <stdlib.h>

audio_router_data_container_t audio_router_data_container;

void AudioRouterData_StoreLastRoutedAudio(audio_source_t audio_source)
{
    DEBUG_LOG_FN_ENTRY("AudioRouterData_StoreLastRoutedAudio enum:audio_source_t:%d", audio_source);

    audio_router_data_container.last_routed_audio_source = audio_source;
}

audio_router_data_iterator_t* AudioRouter_CreateDataIterator(void)
{
    audio_router_data_iterator_t *iterator 
        = (audio_router_data_iterator_t*)PanicUnlessMalloc(sizeof(audio_router_data_iterator_t));

    iterator->data = audio_router_data_container.data;
    iterator->max_data = MAX_NUM_SOURCES;
    iterator->next_index = 0;

    return iterator;
}

void AudioRouter_DestroyDataIterator(audio_router_data_iterator_t *iterator)
{
    PanicNull(iterator);
    free(iterator);
}

audio_router_data_t* AudioRouter_GetNextEntry(audio_router_data_iterator_t *iterator)
{
    audio_router_data_t *data = NULL;

    PanicNull(iterator);

    if(iterator->next_index < iterator->max_data)
    {
        data = (iterator->data) + iterator->next_index++;
    }

    return data;
}

void AudioRouter_InitData(void)
{
    DEBUG_LOG_FN_ENTRY("AudioRouter_InitData");

    memset(&audio_router_data_container, 0, sizeof(audio_router_data_container));
    audio_router_data_container.last_routed_audio_source = audio_source_none;
}

audio_source_t AudioRouter_GetLastRoutedAudio(void)
{
    DEBUG_LOG_FN_ENTRY("AudioRouter_GetLastRoutedAudio enum:audio_source_t:%d",
                       audio_router_data_container.last_routed_audio_source);

    return audio_router_data_container.last_routed_audio_source;
}

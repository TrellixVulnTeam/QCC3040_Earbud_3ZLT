/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   audio_router single_entity
\ingroup    audio_domain
\brief

*/

#include "single_entity_data.h"
#include "focus_generic_source.h"
#include "audio_router.h"
#include "logging.h"
#include "panic.h"

typedef bool (*condition_callback)(audio_router_data_t* data);

static bool singleEntityData_FindSourceMatchingConditionHelper(condition_callback is_condition_matched_callback,
                                                               generic_source_t* source);
static bool singleEntityData_IsNew(audio_router_data_t* data);
static bool singleEntityData_IsInTransientState(audio_router_data_t* data);

static bool singleEntityData_IsActive(audio_router_data_t* source_data)
{
    bool active = FALSE;

    switch(source_data->state)
    {
        case audio_router_state_connecting:
        case audio_router_state_connected:
        case audio_router_state_disconnecting:
        case audio_router_state_disconnected_pending:
        case audio_router_state_disconnecting_no_connect:
        case audio_router_state_connected_pending:
        case audio_router_state_interrupting:
        case audio_router_state_interrupted_pending:
        case audio_router_state_to_be_interrupted:
            active = TRUE;
            break;

        case audio_router_state_disconnected:
        case audio_router_state_interrupted:
        case audio_router_state_new_source:
        case audio_router_state_to_be_resumed:
            break;

        default:
            /* invalid state */
            Panic();
    }
    return active;
}

static audio_router_data_t* singleEntityData_FindSourceInData(generic_source_t source)
{
    DEBUG_LOG_V_VERBOSE("singleEntityData_FindSourceInData src=(enum:source_type_t:%d,%d)",
                        source.type, source.u.voice);

    audio_router_data_t* data = NULL;
    audio_router_data_iterator_t *iterator = AudioRouter_CreateDataIterator();

    while(NULL != (data = AudioRouter_GetNextEntry(iterator)))
    {
        if(data->present || singleEntityData_IsActive(data))
        {
            if(SingleEntityData_AreSourcesSame(data->source, source))
            {
                break;
            }
        }
    }

    AudioRouter_DestroyDataIterator(iterator);

    if(data)
    {
        DEBUG_LOG("singleEntityData_FindSourceInData src=(enum:source_type_t:%d,%d) present %d, active %d",
                   data->source.type, (data->source.type == source_type_audio) ? data->source.u.audio : data->source.u.voice, data->present, singleEntityData_IsActive(data));
    }

    return data;
}

static bool singleEntityData_AddSourceToList(generic_source_t source)
{
    bool success = FALSE;
    audio_router_data_t* data;
    audio_router_data_iterator_t *iterator = AudioRouter_CreateDataIterator();

    DEBUG_LOG_V_VERBOSE("singleEntityData_AddSourceToList src=(enum:source_type_t:%d,%d)",
                        source.type, source.u.voice);

    while(NULL != (data = AudioRouter_GetNextEntry(iterator)))
    {
        if(!(data->present || singleEntityData_IsActive(data)))
        {
            data->source = source;
            data->present = TRUE;
            data->state = audio_router_state_new_source;
            success = TRUE;
            break;
        }
    }

    AudioRouter_DestroyDataIterator(iterator);

    return success;
}

static bool singleEntityData_SetSourceNotPresent(generic_source_t source)
{
    audio_router_data_t* source_entry = singleEntityData_FindSourceInData(source);

    if(source_entry && source_entry->present &&
       (source_entry->state != audio_router_state_interrupted) && (source_entry->state != audio_router_state_to_be_interrupted))
    {
        DEBUG_LOG_V_VERBOSE("singleEntityData_SetSourceNotPresent src=(enum:source_type_t:%d,%d)",
                            source.type, source.u.voice);

        source_entry->present = FALSE;
        return TRUE;
    }
    return FALSE;
}

bool singleEntityData_IsNew(audio_router_data_t* data)
{
    return data->present && data->state == audio_router_state_new_source;
}

bool singleEntityData_IsInTransientState(audio_router_data_t* data)
{
    bool is_transient = FALSE;
    switch(data->state)
    {
        case audio_router_state_connecting:
        case audio_router_state_disconnecting:
        case audio_router_state_disconnected_pending:
        case audio_router_state_disconnecting_no_connect:
        case audio_router_state_connected_pending:
            is_transient = TRUE;
            break;
        default:
            break;
    }
    return is_transient;
}

bool singleEntityData_FindSourceMatchingConditionHelper(condition_callback is_condition_matched_callback, generic_source_t* source)
{
    bool condition_matched = FALSE;
    audio_router_data_t* data = NULL;
    audio_router_data_iterator_t *iterator = AudioRouter_CreateDataIterator();

    DEBUG_LOG_FN_ENTRY("singleEntityData_FindSourceMatchingConditionHelper");

    while (NULL != (data = AudioRouter_GetNextEntry(iterator)))
    {
        if (is_condition_matched_callback(data))
        {
            *source = data->source;
            condition_matched = TRUE;
            break;
        }
    }

    AudioRouter_DestroyDataIterator(iterator);

    DEBUG_LOG_VERBOSE("singleEntityData_FindSourceMatchingConditionHelper=%d src=(enum:source_type_t:%d,%d)",
                      condition_matched, source->type, source->u.voice);

    return condition_matched;
}

bool SingleEntityData_AreSourcesSame(generic_source_t source1, generic_source_t source2)
{
    bool same = FALSE;

    DEBUG_LOG_V_VERBOSE("SingleEntityData_AreSourcesSame src1=(enum:source_type_t:%d,%d) src2=(enum:source_type_t:%d,%d)",
                        source1.type, source1.u.voice, source2.type, source2.u.voice);

    if(source1.type == source2.type)
    {
        switch(source1.type)
        {
            case source_type_voice:
                same = (source1.u.voice == source2.u.voice);
                break;

            case source_type_audio:
                same = (source1.u.audio == source2.u.audio);
                break;

            default:
                break;
        }
    }
    return same;
}

bool SingleEntityData_AddSource(generic_source_t source)
{
    DEBUG_LOG_VERBOSE("SingleEntityData_AddSource src=(enum:source_type_t:%d,%d)",
                      source.type, source.u.voice);

    if(!singleEntityData_FindSourceInData(source))
    {
        return singleEntityData_AddSourceToList(source);
    }
    /* Already present */
    return TRUE;
}

bool SingleEntityData_RemoveSource(generic_source_t source)
{
    bool status = singleEntityData_SetSourceNotPresent(source);

    if(status)
    {
        DEBUG_LOG_VERBOSE("SingleEntityData_RemoveSource src=(enum:source_type_t:%d,%d) removed",
                          source.type, source.u.voice);
    }
    else
    {
        DEBUG_LOG_VERBOSE("SingleEntityData_RemoveSource src=(enum:source_type_t:%d,%d) ignored",
                          source.type, source.u.voice);
    }

    return status;
}

bool SingleEntityData_IsSourcePresent(generic_source_t source)
{
    audio_router_data_t* source_entry = singleEntityData_FindSourceInData(source);
    if(source_entry)
    {
        return source_entry->present;
    }
    return FALSE;
}

bool SingleEntityData_SetSourceState(generic_source_t source, audio_router_state_t state)
{
    audio_router_data_t* source_entry = singleEntityData_FindSourceInData(source);

    if(source_entry)
    {
        DEBUG_LOG_INFO("SingleEntityData_SetSourceState setting enum:source_type_t:%d, source=%d to state enum:audio_router_state_t:%d",
                            source.type, source.u.audio, state);

        source_entry->state = state;
    }
    return (source_entry != NULL);
}

audio_router_state_t SingleEntityData_GetSourceState(generic_source_t source)
{
    audio_router_data_t* source_entry = singleEntityData_FindSourceInData(source);

    audio_router_state_t state = audio_router_state_invalid;

    if(source_entry)
    {
        state = source_entry->state;
    }
    return state;
}

bool SingleEntityData_IsSourceActive(generic_source_t source)
{
    audio_router_data_t* source_entry = singleEntityData_FindSourceInData(source);
    if(source_entry)
    {
        return singleEntityData_IsActive(source_entry);
    }
    return FALSE;
}

bool SingleEntityData_GetActiveSource(generic_source_t* source)
{
    DEBUG_LOG_FN_ENTRY("SingleEntityData_GetActiveSource");

    return singleEntityData_FindSourceMatchingConditionHelper(singleEntityData_IsActive, source);
}

bool SingleEntityData_FindTransientSource(generic_source_t* source)
{
    DEBUG_LOG_FN_ENTRY("SingleEntityData_FindTransientSource");

    return singleEntityData_FindSourceMatchingConditionHelper(singleEntityData_IsInTransientState, source);
}

bool SingleEntityData_GetInterruptedSource(generic_source_t* source)
{
    bool have_entry = FALSE;
    audio_router_data_t* data = NULL;
    audio_router_data_iterator_t *iterator = AudioRouter_CreateDataIterator();

    DEBUG_LOG_FN_ENTRY("SingleEntityData_GetInterruptedSource");

    while(NULL != (data = AudioRouter_GetNextEntry(iterator)))
    {
        if(data->present)
        {
            if(SingleEntityData_GetSourceState(data->source) == audio_router_state_interrupted)
            {
                *source = data->source;
                have_entry = TRUE;
            }
        }
    }

    AudioRouter_DestroyDataIterator(iterator);

    return have_entry;
}

bool SingleEntityData_GetSourceToRoute(generic_source_t* source)
{
    bool source_found = FALSE;

    DEBUG_LOG_FN_ENTRY("SingleEntityData_GetSourceToRoute");

    generic_source_t focused_source = Focus_GetFocusedGenericSourceForAudioRouting();

    audio_router_data_t* data = singleEntityData_FindSourceInData(focused_source);
    if (data != NULL && data->present)
    {
        *source = focused_source;
        source_found = TRUE;
    }

    DEBUG_LOG_VERBOSE("SingleEntityData_GetSourceToRoute src=(enum:source_type_t:%d,%d)",
                      source->type, source->u.audio);

    return source_found;
}

bool SingleEntityData_FindNewSource(generic_source_t* source)
{
    DEBUG_LOG_FN_ENTRY("SingleEntityData_FindNewSource");

    return singleEntityData_FindSourceMatchingConditionHelper(singleEntityData_IsNew, source);
}

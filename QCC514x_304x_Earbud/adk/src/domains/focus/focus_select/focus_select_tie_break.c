/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      This module resolves tie breaks between sources
*/

#include "focus_select.h"
#include "focus_select_audio.h"
#include "focus_select_tie_break.h"

#include <audio_router.h>
#include <device_list.h>
#include <device_properties.h>
#include <logging.h>
#include <panic.h>
#include <voice_sources.h>

#ifndef ENABLE_A2DP_BARGE_IN
#define ENABLE_A2DP_BARGE_IN FALSE
#endif

static bool a2dp_barge_in_enabled;

static const focus_select_audio_tie_break_t audio_tie_break_default[] = 
{
    FOCUS_SELECT_AUDIO_A2DP,
    FOCUS_SELECT_AUDIO_USB,
    FOCUS_SELECT_AUDIO_LINE_IN,
    FOCUS_SELECT_AUDIO_LEA_UNICAST,
    FOCUS_SELECT_AUDIO_LEA_BROADCAST
};

COMPILE_TIME_ASSERT(ARRAY_DIM(audio_tie_break_default) == FOCUS_SELECT_AUDIO_MAX_SOURCES,
                    FOCUS_SELECT_invalid_size_audio_tie_break_default);

static const focus_select_voice_tie_break_t voice_tie_break_default[] = 
{
    FOCUS_SELECT_VOICE_HFP,
    FOCUS_SELECT_VOICE_USB,
    FOCUS_SELECT_VOICE_LEA_UNICAST
};

COMPILE_TIME_ASSERT(ARRAY_DIM(voice_tie_break_default) == FOCUS_SELECT_VOICE_MAX_SOURCES,
                    FOCUS_SELECT_invalid_size_voice_tie_break_default);

static const focus_select_audio_tie_break_t * audio_source_tie_break_ordering = NULL;
static const focus_select_voice_tie_break_t * voice_source_tie_break_ordering = NULL;

void FocusSelect_TieBreakInit(void)
{
    audio_source_tie_break_ordering = audio_tie_break_default;
    voice_source_tie_break_ordering = voice_tie_break_default;
    a2dp_barge_in_enabled = ENABLE_A2DP_BARGE_IN;
}

static audio_source_t focusSelect_ConvertAudioTieBreakToSource(focus_select_audio_tie_break_t prio)
{
    audio_source_t source = audio_source_none;
    switch(prio)
    {
    case FOCUS_SELECT_AUDIO_LINE_IN:
        source = audio_source_line_in;
        break;
    case FOCUS_SELECT_AUDIO_USB:
        source = audio_source_usb;
        break;
    case FOCUS_SELECT_AUDIO_A2DP:
        {
            source = AudioRouter_GetLastRoutedAudio();
            
            if(a2dp_barge_in_enabled || source == audio_source_none)
            {
                source = FocusSelect_GetMruAudioSource();
            }
        }
        break;
    case FOCUS_SELECT_AUDIO_LEA_UNICAST:
        source = audio_source_le_audio_unicast;
        break;
    case FOCUS_SELECT_AUDIO_LEA_BROADCAST:
        source = audio_source_le_audio_broadcast;
        break;
    default:
        break;
    }
    return source;
}

void FocusSelect_HandleTieBreak(focus_status_t * focus_status)
{
    audio_source_t last_routed_audio;

    /* Nothing to be done if all audio sources are disconnected or there is no need to tie break */
    if (focus_status->highest_priority_context == context_audio_disconnected ||
        focus_status->num_highest_priority_sources == 1)
    {
        return;
    }

    last_routed_audio = AudioRouter_GetLastRoutedAudio();

    /* A tie break is needed. Firstly, use the last routed audio source, if one exists */
    if (last_routed_audio != audio_source_none && last_routed_audio < max_audio_sources &&
        FocusSelect_IsAudioSourceContextConnected(focus_status, last_routed_audio) )
    {
        focus_status->highest_priority_source.type = source_type_audio;
        focus_status->highest_priority_source.u.audio = last_routed_audio;
    }
    /* Otherwise, run through the prioritisation of audio sources and select the highest */
    else
    {
        generic_source_t curr_source = {.type=source_type_audio, .u.audio=audio_source_none};
        PanicNull((void*)audio_source_tie_break_ordering);
        
        /* Tie break using the Application specified priority. */
        for (int i=0; i<FOCUS_SELECT_AUDIO_MAX_SOURCES; i++)
        {
            curr_source.u.audio = focusSelect_ConvertAudioTieBreakToSource(audio_source_tie_break_ordering[i]);
            
            if (curr_source.u.audio != audio_source_none && FocusSelect_IsSourceContextHighestPriority(focus_status, curr_source))
            {
                focus_status->highest_priority_source = curr_source;
                break;
            }
        }
    }

    DEBUG_LOG("FocusSelect_HandleTieBreak enum:audio_source_t:%d enum:audio_source_provider_context_t:%d",
               focus_status->highest_priority_source.u.audio,
               focus_status->highest_priority_context);
}

static voice_source_t focusSelect_ConvertVoiceTieBreakToSource(focus_status_t * focus_status, focus_select_voice_tie_break_t prio)
{
    voice_source_t source = voice_source_none;
    switch(prio)
    {
    case FOCUS_SELECT_VOICE_USB:
        source = voice_source_usb;
        break;
    case FOCUS_SELECT_VOICE_LEA_UNICAST:
        source = voice_source_le_audio_unicast;
        break;
    case FOCUS_SELECT_VOICE_HFP:
        {
            generic_source_t generic_hfp_1 = {.type = source_type_voice, .u = {.voice = voice_source_hfp_1}};
            generic_source_t generic_hfp_2 = {.type = source_type_voice, .u = {.voice = voice_source_hfp_2}};
            
            bool highest_priority_is_hfp_1 = FocusSelect_IsSourceContextHighestPriority(focus_status, generic_hfp_1);
            bool highest_priority_is_hfp_2 = FocusSelect_IsSourceContextHighestPriority(focus_status, generic_hfp_2);
            
            if(highest_priority_is_hfp_1 && highest_priority_is_hfp_2)
            {
                /* Only use MRU to decide voice_source to return if we are tie breaking between two HFP sources,
                   i.e. not between HFP and USB, for example. */
                generic_source_t mru_source = {.type = source_type_voice};
                mru_source.u.voice = FocusSelect_GetMruVoiceSource();
                
                /* Check voice source associated with the MRU device is not none and a tied voice source that we are tie-breaking. */
                if (mru_source.u.voice != voice_source_none && FocusSelect_IsSourceContextHighestPriority(focus_status, mru_source))
                {
                    DEBUG_LOG_DEBUG("FocusSelect_HandleVoiceTieBreak using MRU device voice source %d", mru_source.u.voice);
                    source = mru_source.u.voice;
                }
                else
                {
                    DEBUG_LOG_DEBUG("FocusSelect_HandleVoiceTieBreak MRU voice source is not in tie, using hfp_1");
                    source = voice_source_hfp_1;
                }
            }
            else if(highest_priority_is_hfp_1)
            {
                source = voice_source_hfp_1;
            }
            else if(highest_priority_is_hfp_2)
            {
                source = voice_source_hfp_2;
            }
            else
            {
                /* HFP is not available or not a tie break source, skip. */
            }
        }
        break;
    default:
        break;
    }
    return source;
}

void FocusSelect_HandleVoiceTieBreak(focus_status_t * focus_status)
{
    /* Nothing to be done if all voice sources are disconnected or there is no need to tie break */
    if (focus_status->highest_priority_context == context_voice_disconnected ||
        focus_status->num_highest_priority_sources == 1)
    {
        return;
    }

    /* Run through the prioritisation of voice sources and select the highest */
    generic_source_t curr_source = {.type = source_type_voice, .u.voice = voice_source_none};
    PanicNull((void*)voice_source_tie_break_ordering);

    /* Tie break using the Application specified priority. */
    for (int i=0; i<FOCUS_SELECT_VOICE_MAX_SOURCES; i++)
    {
        curr_source.u.voice = focusSelect_ConvertVoiceTieBreakToSource(focus_status, voice_source_tie_break_ordering[i]);
        if (curr_source.u.voice != voice_source_none && FocusSelect_IsSourceContextHighestPriority(focus_status, curr_source))
        {
            focus_status->highest_priority_source = curr_source;
            break;
        }
    }

    DEBUG_LOG_VERBOSE("FocusSelect_HandleVoiceTieBreak selected enum:voice_source_t:%d  enum:voice_source_provider_context_t:%d",
                      focus_status->highest_priority_source.u.voice,
                      focus_status->highest_priority_context);
}

void FocusSelect_ConfigureAudioSourceTieBreakOrder(const focus_select_audio_tie_break_t tie_break_prio[FOCUS_SELECT_AUDIO_MAX_SOURCES])
{
    audio_source_tie_break_ordering = tie_break_prio;
}

void FocusSelect_ConfigureVoiceSourceTieBreakOrder(const focus_select_voice_tie_break_t tie_break_prio[FOCUS_SELECT_VOICE_MAX_SOURCES])
{
    voice_source_tie_break_ordering = tie_break_prio;
}

void FocusSelect_EnableA2dpBargeIn(bool barge_in_enable)
{
    a2dp_barge_in_enabled = barge_in_enable;
}

/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   select_focus_domains Focus Select
\ingroup    focus_domains
\brief      Focus Select module API
*/
#ifndef FOCUS_SELECT_H
#define FOCUS_SELECT_H

#include <message.h>
#include <source_param_types.h>

/*! \brief This enumeration is used to specify the priority order by which the audio
           sources shall assume focus. Use in conjunction with the
           FocusSelect_ConfigureAudioSourceTieBreakOrder API. */
typedef enum
{
    FOCUS_SELECT_AUDIO_LINE_IN,
    FOCUS_SELECT_AUDIO_USB,
    FOCUS_SELECT_AUDIO_A2DP,
    FOCUS_SELECT_AUDIO_LEA_UNICAST,
    FOCUS_SELECT_AUDIO_LEA_BROADCAST,
    FOCUS_SELECT_AUDIO_MAX_SOURCES
} focus_select_audio_tie_break_t;

/*! \brief This enumeration is used to specify the priority order by which the voice
           sources shall assume focus. Use in conjunction with the
           FocusSelect_ConfigureVoiceSourceTieBreakOrder API. */
typedef enum
{
    FOCUS_SELECT_VOICE_USB,
    FOCUS_SELECT_VOICE_HFP,
    FOCUS_SELECT_VOICE_LEA_UNICAST,
    FOCUS_SELECT_VOICE_MAX_SOURCES
} focus_select_voice_tie_break_t;

/*! \brief Initialise the Focus Select module.

    This function registers the Focus Select module with the Focus Device interface
    so the Application framework can resolve which audio source or voice source
    should be routed to the Audio subsystem or interact with the UI module.

    \param  init_task Init task - not used, should be passed NULL
    \return True if successful
*/
bool FocusSelect_Init(Task init_task);

/*! \brief Configure the Audio Source prioritisation to use when establishing focus.

    This function configures the prioritisation of Audio Sources which shall be used
    by the Focus Select module for determining which source has the foreground focus,
    in the event of a tie break being needed between multiple audio sources with the
    same prioritisation.

    \param  tie_break_prio - an array, of length FOCUS_SELECT_AUDIO_MAX_SOURCES, specifying
            the order to apply when a tie break is needed. The highest priority audio
            source shall be index 0 in the array. The lowest priority shall be
            (FOCUS_SELECT_AUDIO_MAX_SOURCES-1).
*/
void FocusSelect_ConfigureAudioSourceTieBreakOrder(const focus_select_audio_tie_break_t tie_break_prio[FOCUS_SELECT_AUDIO_MAX_SOURCES]);

/*! \brief Configure the Voice Source prioritisation to use when establishing focus.

    This function configures the prioritisation of Voice Sources which shall be used
    by the Focus Select module for determining which source has the foreground focus,
    in the event of a tie break being needed between multiple voice sources with the
    same prioritisation.

    \param  tie_break_prio - an array, of length FOCUS_SELECT_VOICE_MAX_SOURCES, specifying
            the order to apply when a tie break is needed. The highest priority audio source
            shall be index 0 in the array. The lowest priority shall be
            (FOCUS_SELECT_VOICE_MAX_SOURCES-1).
*/
void FocusSelect_ConfigureVoiceSourceTieBreakOrder(const focus_select_voice_tie_break_t tie_break_prio[FOCUS_SELECT_VOICE_MAX_SOURCES]);

/*! \brief Configure the way focus_select prioritises A2DP audio sources

    This function configures how focus_select resolves conflicts where there are two A2DP
    sources in the same streaming or playing state

    \param  barge_in_enable - When TRUE the most recently started A2DP stream will take the
            foreground pushing any other streams to the background. When FALSE the last 
            routed A2DP stream will keep the foreground, blocking any newly started streams.
*/
void FocusSelect_EnableA2dpBargeIn(bool barge_in_enable);

#endif /* FOCUS_SELECT_H */

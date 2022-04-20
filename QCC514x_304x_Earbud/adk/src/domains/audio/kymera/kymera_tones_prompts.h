/*!
\copyright  Copyright (c) 2021  Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       kymera_prompts.h
\brief      Kymera tones / prompts API
*/

#ifndef KYMERA_TONES_PROMPTS_H
#define KYMERA_TONES_PROMPTS_H

#include <ringtone/ringtone_if.h>
#include <file.h>

#include "kymera.h"

/*! Kymera ringtone generator has a fixed sample rate of 8 kHz */
#define KYMERA_TONE_GEN_RATE (8000)

/*! \brief KYMERA_INTERNAL_TONE_PLAY message content */
typedef struct
{
    /*! Pointer to the ringtone structure to play, NULL for prompt. */
    const ringtone_note *tone;
    /*! The prompt file index to play. FILE_NONE for tone. */
    FILE_INDEX prompt;
    /*! The prompt file format */
    promptFormat prompt_format;
    /*! The tone/prompt sample rate */
    uint32 rate;
    /*! The time to play the tone/prompt, in microseconds. */
    uint32 time_to_play;
    /*! If TRUE, the tone may be interrupted by another event before it is
        completed. If FALSE, the tone may not be interrupted by another event
        and will play to completion. */
    bool interruptible;
    /*! If not NULL, set bits in client_lock_mask will be cleared in client_lock
    when the tone is stopped. */
    uint16 *client_lock;
    /*! The mask of bits to clear in client_lock. */
    uint16 client_lock_mask;
} KYMERA_INTERNAL_TONE_PROMPT_PLAY_T;

/*! \brief Immediately stop playing the tone or prompt */
void appKymeraTonePromptStop(void);

/*! \brief Load downloadable capabilities for the prompt chain in advance.
 */
void Kymera_PromptLoadDownloadableCaps(void);

/*! \brief Undo Kymera_PromptLoadDownloadableCaps.
 */
void Kymera_PromptUnloadDownloadableCaps(void);

/*! \brief Initialise prompt/tones component
*/
void appKymeraTonePromptInit(void);

/*! \brief Check if prompt is being played.
    \return TRUE for yes, FALSE for no.
 */
bool appKymeraIsPlayingPrompt(void);

/*! \brief Handle request to play a tone or prompt.
    \param msg The request message.
*/
void appKymeraHandleInternalTonePromptPlay(const KYMERA_INTERNAL_TONE_PROMPT_PLAY_T *msg);

#endif // KYMERA_TONES_PROMPTS_H

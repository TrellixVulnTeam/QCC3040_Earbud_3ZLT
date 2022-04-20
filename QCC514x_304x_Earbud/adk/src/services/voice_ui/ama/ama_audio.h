/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_audio.h
\brief  File consists of function decalration for Amazon Voice Service's audio specific interface
*/
#ifndef AMA_AUDIO_H
#define AMA_AUDIO_H

#include "ama_config.h"
#include "ama_protocol.h"
#include <voice_ui_va_client_if.h>
#include <va_audio_types.h>

typedef struct _ama_supported_locales
{
    uint8 num_locales;
    const char *name[MAX_AMA_LOCALES];
}ama_supported_locales_t;

typedef enum {
    ama_audio_trigger_tap,
    ama_audio_trigger_press,
    ama_audio_trigger_wake_word
}ama_audio_trigger_t;
/*!
    \brief This triggers Voice Session with AVS
*/
bool AmaAudio_Start(ama_audio_trigger_t type);

/*!
    \brief Stops the voice capture chain
*/
void AmaAudio_Stop(void);

/*!
    \brief Starts the voice capture chain
*/
bool AmaAudio_Provide(const AMA_SPEECH_PROVIDE_IND_T* ind);

/*!
    \brief Ends the AVS speech session
*/
void AmaAudio_End(void);

void AmaAudio_StartWakeWordDetection(void);

void AmaAudio_StopWakeWordDetection(void);

unsigned AmaAudio_HandleVoiceData(Source src);

bool AmaAudio_WakeWordDetected(va_audio_wuw_capture_params_t *capture_params, const va_audio_wuw_detection_info_t *wuw_info);

/*!
    \brief Validates the locale
    \param locale string
    \return bool TRUE if the locale is valid
*/
bool AmaAudio_ValidateLocale(const char *locale);

/*!
    \brief Initialises AMA audio data
*/
void AmaAudio_Init(void);

/*!
    \brief Gets the current locale
*/
const char* AmaAudio_GetCurrentLocale(void);

/*!
    \brief Sets the current locale
*/
void AmaAudio_SetLocale(const char* locale);


/*! \brief Gets the voice assistant locale setting from the Device database.
    \param locale Buffer to hold the locale name.
    \param locale_size Size of locale buffer.
    \return TRUE on success, FALSE otherwise.
 */
bool AmaAudio_GetDeviceLocale(char *locale, uint8 locale_size);

/*!
    \brief Gets all supported locales
*/
void AmaAudio_GetSupportedLocales(ama_supported_locales_t* supported_locales);

/*!
    \brief Gets the stored model for a given locale
*/
const char *AmaAudio_GetModelFromLocale(const char* locale);

/*! \brief Register the locale specific prompt handler with the UI prompts */
void AmaAudio_RegisterLocalePrompts(void);

/*! \brief Deregister the locale specific prompt handler from the UI prompts */
void AmaAudio_DeregisterLocalePrompts(void);

#endif /* AMA_AUDIO_H*/


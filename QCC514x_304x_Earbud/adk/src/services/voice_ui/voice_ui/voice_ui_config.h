/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       voice_ui_config.h
\brief      Voice UI configuration header
*/

#ifndef VOICE_UI_CONFIG_H_
#define VOICE_UI_CONFIG_H_

/*! \brief Specify whether to reboot the device following an update to the VA provider  */
#define NO_REBOOT_AFTER_VA_CHANGE FALSE

/*! \brief Voice Assistant provider names
 * This is a list of voice assistants in priority order
 *
 * IMPORTANT: The integer values assigned to each assistant are part of the GAIA specification
 *            so these values shouldn't be changed or the GAIA specification would have to change as well
 */
typedef enum
{
    voice_ui_provider_none = 0,

#if defined(ENABLE_AUDIO_TUNING_MODE)
    voice_ui_provider_audio_tuning = 1,
    #define VOICE_UI_PROVIDER_AUDIO_TUNING_MODE_INCLUDED (1)
    #if !defined(VOICE_UI_PROVIDER_DEFAULT)
        #define VOICE_UI_PROVIDER_DEFAULT (voice_ui_provider_audio_tuning)
    #endif
#else
    #define VOICE_UI_PROVIDER_AUDIO_TUNING_MODE_INCLUDED (0)
#endif

#if defined(INCLUDE_GAA)
    voice_ui_provider_gaa = 2,
    #define VOICE_UI_PROVIDER_GAA_INCLUDED (1)
    #if !defined(VOICE_UI_PROVIDER_DEFAULT)
        #define VOICE_UI_PROVIDER_DEFAULT (voice_ui_provider_gaa)
    #endif
#else
    #define VOICE_UI_PROVIDER_GAA_INCLUDED (0)
#endif

#if defined(INCLUDE_AMA)
    voice_ui_provider_ama = 3,
    #define VOICE_UI_PROVIDER_AMA_INCLUDED (1)
    #if !defined(VOICE_UI_PROVIDER_DEFAULT)
        #define VOICE_UI_PROVIDER_DEFAULT (voice_ui_provider_ama)
    #endif
#else
    #define VOICE_UI_PROVIDER_AMA_INCLUDED (0)
#endif

} voice_ui_provider_t;

#if !defined(VOICE_UI_PROVIDER_DEFAULT)
    #define VOICE_UI_PROVIDER_DEFAULT (voice_ui_provider_none)
#endif

#define MAX_NO_VA_SUPPORTED ( \
    VOICE_UI_PROVIDER_AUDIO_TUNING_MODE_INCLUDED + \
    VOICE_UI_PROVIDER_GAA_INCLUDED + \
    VOICE_UI_PROVIDER_AMA_INCLUDED \
)

#endif /* VOICE_UI_CONFIG_H_ */

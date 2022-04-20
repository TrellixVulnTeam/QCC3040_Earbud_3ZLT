/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       voice_ui_ANC.h
\brief      API of the voice UI ANC handling.
*/

#ifndef VOICE_UI_ANC_H_
#define VOICE_UI_ANC_H_

#ifdef ENABLE_ANC
#include <csrtypes.h>

typedef enum
{
    VOICE_UI_ENABLE_STATIC_ANC,
    VOICE_UI_ENABLE_LEAKTHROUGH,
    VOICE_UI_DISABLE_ANC_AND_LEAKTHROUGH,
    VOICE_UI_SET_LEAKTHROUGH_GAIN,
} voice_ui_anc_internal_message_t;

typedef struct
{
    uint8 gain_as_percentage;
} VOICE_UI_SET_LEAKTHROUGH_GAIN_T;

/*! \brief Initialisation of Voice UI ANC handling
*/
void VoiceUi_AncInit(void);

/*! \brief Check if static ANC is enabled
 *  \return bool TRUE if static ANC is enabled, otherwise FALSE
*/
bool VoiceUi_IsStaticAncEnabled(void);

/*! \brief Enable static ANC
*/
void VoiceUi_EnableStaticAnc(void);

/*! \brief Disable static ANC
*/
void VoiceUi_DisableStaticAnc(void);

/*! \brief Check if leakthrough is enabled
 *  \return bool TRUE if leakthrough is enabled, otherwise FALSE
*/
bool VoiceUi_IsLeakthroughEnabled(void);

/*! \brief Enable hardware leakthrough
*/
void VoiceUi_EnableLeakthrough(void);

/*! \brief Disable leakthrough
*/
void VoiceUi_DisableLeakthrough(void);

/*! \brief Get leakthrough level as percentage
 *  \return uint8 leakthrough level as percentage
*/
uint8 VoiceUi_GetLeakthroughLevelAsPercentage(void);

/*! \brief Set leakthrough level from a percentage
 *  \param uint8 leakthrough level as percentage
*/
void VoiceUi_SetLeakthroughLevelFromPercentage(uint8 level_as_percentage);

#endif /* ENABLE_ANC */

#endif  /* VOICE_UI_ANC_H_ */

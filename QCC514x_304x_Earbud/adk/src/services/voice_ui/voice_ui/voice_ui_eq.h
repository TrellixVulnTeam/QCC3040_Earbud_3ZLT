/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       voice_ui_eq.h
\brief      API of the voice UI EQ handling.
*/

#ifndef VOICE_UI_EQ_H_
#define VOICE_UI_EQ_H_

#ifdef INCLUDE_MUSIC_PROCESSING
typedef struct
{
    uint8 (*isEqActive)(void);
    uint8 (*getNumberOfActiveBands)(void);
    bool (*setUserEqBands)(uint8 start_band, uint8 end_band, int16 *gains);
    bool (*setPreset)(uint8 preset);
} voice_ui_eq_if_t;

/*! \brief Initialisation of Voice UI EQ handling
*/
void VoiceUi_EqInit(void);

/*! \brief Checks if user EQ is active
    \return TRUE if user EQ is active, FALSE otherwise
*/
bool VoiceUi_IsUserEqActive(void);

/*! \brief Gets gain of the EQs lowest bands
 *  \return The low EQ gain was a percentage
*/
int16 VoiceUi_GetLowEqGain(void);

/*! \brief Sets gain of the EQs lowest bands
    \param gain on a scale of 0-100
*/
void VoiceUi_SetLowEqGain(int16 gain_percentage);

/*! \brief Gets gain of the EQs middle bands
 *  \return The mid EQ gain was a percentage
*/
int16 VoiceUi_GetMidEqGain(void);

/*! \brief Sets gain of the EQs middle bands
    \param gain on a scale of 0-100
*/
void VoiceUi_SetMidEqGain(int16 gain_percentage);

/*! \brief Gets gain of the EQs highest bands
 *  \return The high EQ gain was a percentage
*/
int16 VoiceUi_GetHighEqGain(void);

/*! \brief Sets gain of the EQs highest bands
    \param gain percentage on a scale of 0-100
*/
void VoiceUi_SetHighEqGain(int16 gain_percentage);

/*! \brief Set the voice ui EQ interface pointer
    \param Pointer to the voice ui EQ interface
*/
void VoiceUi_SetEqInterface(voice_ui_eq_if_t * voice_ui_eq_if);
#else
#define VoiceUi_EqInit() ((void)0)
#define VoiceUi_IsUserEqActive() (FALSE)
#define VoiceUi_GetLowEqGain() (50)
#define VoiceUi_SetLowEqGain(gain_percentage) ((void)0)
#define VoiceUi_GetMidEqGain() (50)
#define VoiceUi_SetMidEqGain(gain_percentage) ((void)0)
#define VoiceUi_GetHighEqGain(void) (50)
#define VoiceUi_SetHighEqGain(gain_percentage) ((void)0)
#define VoiceUi_SetEqInterface(voice_ui_eq_if) ((void)0)
#endif /* INCLUDE_MUSIC_PROCESSING */

#endif  /* VOICE_UI_EQ_H_ */

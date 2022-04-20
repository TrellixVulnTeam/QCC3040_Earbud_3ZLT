/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Focus Select audio API
*/
#ifndef FOCUS_SELECT_AUDIO_H
#define FOCUS_SELECT_AUDIO_H

#include <device.h>
#include <focus_audio_source.h>

/*! \brief Get the audio source associated with the most recently used device

    \return The most recently used audio source if successful, otherwise audio_source_none
*/
audio_source_t FocusSelect_GetMruAudioSource(void);

/*! \brief Get the source (voice or audio) which has focus_foreground for audio routing

    \return The focussed voice or audio source if there is one, otherwise the returned
            source type will be source_type_invalid
*/
generic_source_t FocusSelect_GetFocusedSourceForAudioRouting(void);

/*! \brief Get the audio routing focus for an audio source

    \param audio_source - The source to get the focus for
    \return The audio routing focus
*/
focus_t FocusSelect_GetFocusForAudioSource(const audio_source_t audio_source);

/*! \brief Get the voice source associated with the most recently used device

    \return The most recently used voice source if successful, otherwise audio_source_none
*/
voice_source_t FocusSelect_GetMruVoiceSource(void);

/*! \brief Get the audio routing focus for a voice source

    \param voice_source - The source to get the focus for
    \return The audio routing focus
*/
focus_t FocusSelect_GetFocusForVoiceSource(const voice_source_t voice_source);

/*! \brief Check if a device has foreground focus for audio routing

    \param device - The device to check
    \return TRUE if the device has an audio or voice source with foreground focus,
            otherwise FALSE
*/
bool FocusSelect_DeviceHasVoiceAudioFocus(device_t device);

/*! \brief Get the audio routing focus for a device

    \param device - The device to get the focus for
    \return The audio routing focus
*/
focus_t FocusSelect_GetFocusForDevice(const device_t device);

#endif /* FOCUS_SELECT_AUDIO_H */

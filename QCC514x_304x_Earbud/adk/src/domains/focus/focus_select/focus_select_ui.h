/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Focus Select UI API
*/
#ifndef FOCUS_SELECT_UI_H
#define FOCUS_SELECT_UI_H

#include <device.h>
#include <ui_inputs.h>

/*! \brief Get the audio source to use when determining the media context 

    \param audio_source - Pointer to the audio source to use
    \return TRUE if a source was found, otherwise FALSE
*/
bool FocusSelect_GetAudioSourceForContext(audio_source_t * audio_source);

/*! \brief Get the audio source to use when handling a UI input

    \param ui_input - The input to find a source for
    \param audio_source - Pointer to the audio source to use
    \return TRUE if a source was found, otherwise FALSE
*/
bool FocusSelect_GetAudioSourceForUiInput(ui_input_t ui_input, audio_source_t * audio_source);

/*! \brief Get the voice source to use when determining the telephony context 

    \param provider - The UI provider, expected to be ui_provider_telephony
    \param voice_source - Pointer to the voice source to use
    \return TRUE if a source was found, otherwise FALSE
*/
bool FocusSelect_GetVoiceSourceForContext(ui_providers_t provider, voice_source_t * voice_source);

/*! \brief Get the highest priority voice source in one of the requested contexts

    \param provider - The UI provider, expected to be ui_provider_telephony
    \param voice_source - Pointer to the voice source to use
    \param contexts - Requested contexts
    \param num_context - Number of contexts requested
    \return TRUE if a source was found, otherwise FALSE
*/
bool FocusSelect_GetVoiceSourceInContextArray(ui_providers_t provider, voice_source_t * voice_source, const unsigned* contexts, const unsigned num_contexts);

/*! \brief Get the voice source to use when handling a UI input

    \param ui_input - The input to find a source for
    \param voice_source - Pointer to the voice source to use
    \return TRUE if a source was found, otherwise FALSE
*/
bool FocusSelect_GetVoiceSourceForUiInput(ui_input_t ui_input, voice_source_t * voice_source);

/*! \brief Get the device to use when handling a UI input

    \param ui_input - The input to find a source for (usually connection related, eg. ui_input_connect_handset)
    \param device - Pointer to the device to use
    \return TRUE if a device was found, otherwise FALSE
*/
bool FocusSelect_GetDeviceForUiInput(ui_input_t ui_input, device_t * device);

/*! \brief Get the device to use when determining connection context 

    \param provider - The UI provider
    \param device - Pointer to the device to use
    \return TRUE if a device was found, otherwise FALSE
*/
bool FocusSelect_GetDeviceForContext(ui_providers_t provider, device_t* device);

/*! \brief Exclude a device from being returned by FocusSelect_GetDeviceForUiInput. 
           This currently only works for ui_input_connect_handset.

    \param device - The device to exclude
    \return TRUE if successful (ie. device is valid), otherwise FALSE
*/
bool FocusSelect_ExcludeDevice(device_t device);

/*! \brief Include a device in being returned by FocusSelect_GetDeviceForUiInput. 
           This currently only works for ui_input_connect_handset.

    \param device - The device to include
    \return TRUE if successful (ie. device is valid), otherwise FALSE
*/
bool FocusSelect_IncludeDevice(device_t device);

/*! \brief Reset any excluded devices, all devices are included
*/
void FocusSelect_ResetExcludedDevices(void);

#endif /* FOCUS_SELECT_UI_H */

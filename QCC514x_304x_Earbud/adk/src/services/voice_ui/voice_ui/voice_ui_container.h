/*!
\copyright  Copyright (c) 2019 - 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Voice UI container private header
*/

#ifndef VOICE_UI_CONTAINER_H_
#define VOICE_UI_CONTAINER_H_

#include "voice_ui_va_client_if.h"
#include <bt_device.h>

/*! \brief Get the active voice assistant
 */
voice_ui_provider_t VoiceUi_GetSelectedAssistant(void);

/*! \brief Get the available voice assistant
    \param assistants Array of uint8 to be populated with identifiers of the
           available assistants.
    \return Number of supported assistants.
 */
uint16 VoiceUi_GetSupportedAssistants(uint8 *assistants);

/*! \brief Stores the active voice assistant into the Device database
 */
void VoiceUi_SetSelectedAssistant(uint8 voice_ui_provider, bool reboot);

/*! \brief Sets the selected voice assistant interface
 */
void VoiceUi_SetSelectedVoiceAssistantInterface(voice_ui_provider_t voice_ui_provider);

/*! \brief Get the active Voice Assistant
*/
voice_ui_handle_t* VoiceUi_GetActiveVa(void);

/*! \brief Function called by voice assistant to handle ui events.
 */
void VoiceUi_EventHandler(voice_ui_handle_t* va_handle, ui_input_t event_id);

/*! \brief Retrieves the voice assistant locale from the device database.
    \param packed_locale Array of four uint8s to receive the locale
 */
void VoiceUi_GetPackedLocale(uint8 *packed_locale);

/*! \brief Stores the voice assistant locale in the device database.
    \param packed_locale The locale packed into four uint8s
 */
void VoiceUi_SetPackedLocale(uint8 *packed_locale);

feature_manager_handle_t VoiceUi_GetFeatureManagerHandle(void);
void VoiceUi_SetFeatureManagerHandle(feature_manager_handle_t handle);

#ifdef HOSTED_TEST_ENVIRONMENT
void VoiceUi_UnRegister(voice_ui_handle_t *va_handle);
#endif

#endif


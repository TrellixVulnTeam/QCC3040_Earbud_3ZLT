/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      GAIA interface with Voice Assistants.
*/

#ifdef INCLUDE_GAIA

#include "gaia_framework.h"
#include "voice_ui_config.h"
#include "voice_ui_container.h"
#include "voice_ui_gaia_plugin.h"
#include <gaia_features.h>
#include <logging.h>

/* may report voice_ui_provider_none as well as implemented providers */
#define MAX_NO_VA_REPORTED (MAX_NO_VA_SUPPORTED + 1)


static void voiceUiGaiaPlugin_SendInvalidParameter(GAIA_TRANSPORT *t, uint8 pdu_id)
{
    GaiaFramework_SendError(t, GAIA_VOICE_UI_FEATURE_ID, pdu_id, invalid_parameter);
}


static void voiceUiGaiaPlugin_SendResponse(GAIA_TRANSPORT *t, uint8 pdu_id, uint8 length, uint8 * payload)
{
    GaiaFramework_SendResponse(t, GAIA_VOICE_UI_FEATURE_ID, pdu_id, length, payload);
}


static void voiceUiGaiaPlugin_GetSelectedAssistant(GAIA_TRANSPORT *t)
{
    uint8 selected_assistant = VoiceUi_GetSelectedAssistant();
    voiceUiGaiaPlugin_SendResponse(t, voice_ui_gaia_get_selected_assistant, 1, &selected_assistant);
}


static void voiceUiGaiaPlugin_SetSelectedAssistant(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    bool status = FALSE;

    if (payload_length == 1)
    {
        status = VoiceUi_SelectVoiceAssistant(payload[0], voice_ui_reboot_allowed);
    }

    if (status)
    {
        voiceUiGaiaPlugin_SendResponse(t, voice_ui_gaia_set_selected_assistant, 0, NULL);
    }
    else
    {
        voiceUiGaiaPlugin_SendInvalidParameter(t, voice_ui_gaia_set_selected_assistant);
    }
}


static void voiceUiGaiaPlugin_GetSupportedAssistants(GAIA_TRANSPORT *t)
{
    uint16 count;
    uint8 response[MAX_NO_VA_REPORTED + 1];

    count = VoiceUi_GetSupportedAssistants(response + 1);
    response[0] = count;
    voiceUiGaiaPlugin_SendResponse(t, voice_ui_gaia_get_supported_assistants, count + 1, response);
}


static gaia_framework_command_status_t voiceUiGaiaPlugin_MainHandler(GAIA_TRANSPORT *t, uint8 pdu_id, uint16 payload_length, const uint8 *payload)
{
    DEBUG_LOG("voiceUiGaiaPlugin_MainHandler, pdu_id %u", pdu_id);

    switch (pdu_id)
    {
    case voice_ui_gaia_get_selected_assistant:
        voiceUiGaiaPlugin_GetSelectedAssistant(t);
        break;

    case voice_ui_gaia_set_selected_assistant:
        voiceUiGaiaPlugin_SetSelectedAssistant(t, payload_length, payload);
        break;

    case voice_ui_gaia_get_supported_assistants:
        voiceUiGaiaPlugin_GetSupportedAssistants(t);
        break;

    default:
        DEBUG_LOG_ERROR("voiceUiGaiaPlugin_MainHandler, unhandled call for %u", pdu_id);
        return command_not_handled;
    }

    return command_handled;
}


static void voiceUiGaiaPlugin_TransportConnect(GAIA_TRANSPORT *t)
{
    DEBUG_LOG_INFO("voiceUiGaiaPlugin_TransportConnect, transport %p", t);
}

static void upgradeGaiaPlugin_TransportDisconnect(GAIA_TRANSPORT *t)
{
    DEBUG_LOG_INFO("voiceUiGaiaPlugin_TransportDisconnect, transport %p", t);
}

static void voiceUiGaiaPlugin_SendAllNotifications(GAIA_TRANSPORT *transport)
{
    UNUSED(transport);
    DEBUG_LOG("voiceUiGaiaPlugin_SendAllNotifications");
    VoiceUiGaiaPlugin_NotifyAssistantChanged(VoiceUi_GetSelectedAssistant());
}


void VoiceUiGaiaPlugin_Init(void)
{
    static const gaia_framework_plugin_functions_t functions =
    {
        .command_handler = voiceUiGaiaPlugin_MainHandler,
        .send_all_notifications = voiceUiGaiaPlugin_SendAllNotifications,
        .transport_connect = voiceUiGaiaPlugin_TransportConnect,
        .transport_disconnect = upgradeGaiaPlugin_TransportDisconnect,
    };

    GaiaFramework_RegisterFeature(GAIA_VOICE_UI_FEATURE_ID, GAIA_VOICE_UI_VERSION, &functions);
}


void VoiceUiGaiaPlugin_NotifyAssistantChanged(voice_ui_provider_t va_provider)
{
    uint8 provider_id = va_provider;
    GaiaFramework_SendNotification(GAIA_VOICE_UI_FEATURE_ID, voice_ui_gaia_assistant_changed, 1, &provider_id);
}

#endif /* INCLUDE_GAIA */

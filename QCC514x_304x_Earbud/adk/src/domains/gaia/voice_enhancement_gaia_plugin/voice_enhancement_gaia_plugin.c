/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Voice Enhancement gaia plugin component
*/

#if defined(INCLUDE_GAIA) && defined(INCLUDE_CVC_DEMO)

#include <panic.h>
#include <stdlib.h>
#include <byte_utils.h>
#include <kymera.h>

#include "voice_enhancement_gaia_plugin.h"

static Task voiceEnhancementGaiaPlugin_KymeraStateTask(void);
static gaia_framework_command_status_t voiceEnhancementGaiaPlugin_MainHandler(GAIA_TRANSPORT *t, uint8 pdu_id, uint16 payload_length, const uint8 *payload);
static void voiceEnhancementGaiaPlugin_HandleKymeraMessage(Task task, MessageId id, Message message);

static void voiceEnhancementGaiaPlugin_GetSupportedEnhancements(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload);
static void voiceEnhancementGaiaPlugin_SetConfigEnhancement(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload);
static void voiceEnhancementGaiaPlugin_GetConfigEnhancement(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload);

static void voiceEnhancementGaiaPlugin_SendAllNotifications(GAIA_TRANSPORT *t);

static const TaskData kymera_task = {voiceEnhancementGaiaPlugin_HandleKymeraMessage};

bool VoiceEnhancementGaiaPlugin_Init(Task init_task)
{
    UNUSED(init_task);
    DEBUG_LOG_VERBOSE("VoiceEnhancementGaiaPlugin_Init");

    static const gaia_framework_plugin_functions_t functions =
    {
        .command_handler = voiceEnhancementGaiaPlugin_MainHandler,
        .send_all_notifications = voiceEnhancementGaiaPlugin_SendAllNotifications,
        .transport_connect = NULL,
        .transport_disconnect = NULL,
    };

    Kymera_RegisterNotificationListener(voiceEnhancementGaiaPlugin_KymeraStateTask());
    GaiaFramework_RegisterFeature(GAIA_VOICE_ENHANCEMENT_FEATURE_ID, VOICE_ENHANCEMENT_GAIA_PLUGIN_VERSION, &functions);
    return TRUE;
}

static void voiceEnhancementGaiaPlugin_CvcSendFillPayload(uint8 *payload)
{
    kymera_cvc_mode_t mode = 0;
    Kymera_ScoGetCvcSend3MicMicConfig(&payload[1]);
    Kymera_ScoGetCvcPassthroughMode(&mode, &payload[2]);
    Kymera_ScoGetCvcSend3MicModeOfOperation(&payload[3]);
}

/*! \brief Gaia Client will be informed about change in 3Mic cVc send mode of operation */
static void voiceEnhancementGaiaPlugin_CvcSendNotification(uint8 *payload, uint8 length)
{
    DEBUG_LOG_VERBOSE("voiceEnhancementGaiaPlugin_CvcSendNotification: payload");
    DEBUG_LOG_DATA_VERBOSE(payload,length);
    GaiaFramework_SendNotification(GAIA_VOICE_ENHANCEMENT_FEATURE_ID, voice_enhancement_mode_change, length, payload);
}

static void voiceEnhancementGaiaPlugin_CvcSendModeChanged(void)
{
    uint8 response[CVC_SEND_MODE_CHANGE_PAYLOAD_LENGTH] = {0};
    response[0] = VOICE_ENHANCEMENT_CAP_CVC_3MIC;
    voiceEnhancementGaiaPlugin_CvcSendFillPayload(response);
    voiceEnhancementGaiaPlugin_CvcSendNotification(response, CVC_SEND_MODE_CHANGE_PAYLOAD_LENGTH);
}

/*! \brief Function pointer definition for the command handler
    \param transport    Transport type
    \param pdu_id       PDU specific ID for the message
    \param length       Length of the payload
    \param payload      Payload data
    \return Gaia framework command status code
*/
static gaia_framework_command_status_t voiceEnhancementGaiaPlugin_MainHandler(GAIA_TRANSPORT *t, uint8 pdu_id, uint16 payload_length, const uint8 *payload)
{
    switch (pdu_id)
    {
        case get_supported_voice_enhancements:
            voiceEnhancementGaiaPlugin_GetSupportedEnhancements(t, payload_length, payload);
            break;

        case set_config_voice_enhancement:
            voiceEnhancementGaiaPlugin_SetConfigEnhancement(t, payload_length, payload);
            break;

        case get_config_voice_enhancement:
            voiceEnhancementGaiaPlugin_GetConfigEnhancement(t, payload_length, payload);
            break;

        default:
            DEBUG_LOG_VERBOSE("voiceEnhancementGaiaPlugin_MainHandler, unhandled call for %u", pdu_id);
            return command_not_handled;
    }
    return command_handled;
}

/*! \brief Command to read the supported enhancements
    \param transport    Transport type
*/
static void voiceEnhancementGaiaPlugin_GetSupportedEnhancements(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    UNUSED(payload);
    UNUSED(payload_length);
    DEBUG_LOG_VERBOSE("voiceEnhancementGaiaPlugin_GetSupportedEnhancements");

    uint8 response[CVC_SEND_GET_SUPPORTED_PAYLOAD_LENGTH] = {0};
    response[0] = VOICE_ENHANCEMENT_NO_MORE_DATA;
#ifdef INCLUDE_CVC_DEMO
    response[1] = VOICE_ENHANCEMENT_CAP_CVC_3MIC;
#else
    response[1] = VOICE_ENHANCEMENT_CAP_NONE;
#endif
    GaiaFramework_SendResponse(t, GAIA_VOICE_ENHANCEMENT_FEATURE_ID, get_supported_voice_enhancements,
                               CVC_SEND_GET_SUPPORTED_PAYLOAD_LENGTH, response);
}

/*! \brief Command to set the mode of 3Mic cVc send capability
    \param t                Transport type
    \param payload_length   Payload length
    \param payload          Payload
*/
static void voiceEnhancementGaiaPlugin_SetConfigEnhancement(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    DEBUG_LOG_VERBOSE("voiceEnhancementGaiaPlugin_SetConfigEnhancement: payload");
    DEBUG_LOG_DATA_VERBOSE(payload, payload_length);

    if (payload_length >= 3)
    {
        /* 3Mic cVc */
        if(payload[0] == VOICE_ENHANCEMENT_CAP_CVC_3MIC)
        {
            bool setting_changed;
            setting_changed = Kymera_ScoSetCvcSend3MicMicConfig(payload[1]);
            kymera_cvc_mode_t mode = ((payload[1] == 0) ? KYMERA_CVC_SEND_PASSTHROUGH : KYMERA_CVC_SEND_FULL_PROCESSING);
            setting_changed |= Kymera_ScoSetCvcPassthroughMode(mode, payload[2]);
            uint8 response[1] = {0};
            GaiaFramework_SendResponse(t, GAIA_VOICE_ENHANCEMENT_FEATURE_ID, set_config_voice_enhancement, 0, response);
            if(setting_changed)
            {
                voiceEnhancementGaiaPlugin_CvcSendModeChanged();
            }
        }
        else
        {
            DEBUG_LOG_ERROR("voiceEnhancementGaiaPlugin_SetConfigEnhancement: Unknown feature");
            GaiaFramework_SendError(t, GAIA_VOICE_ENHANCEMENT_FEATURE_ID, set_config_voice_enhancement, invalid_parameter);
        }
    }
    else
    {
        DEBUG_LOG_ERROR("voiceEnhancementGaiaPlugin_SetConfigEnhancement: Payload too short. Only %d bytes", payload_length);
        GaiaFramework_SendError(t, GAIA_VOICE_ENHANCEMENT_FEATURE_ID, set_config_voice_enhancement, invalid_parameter);
    }
}

/*! \brief Command to read the mode of 3Mic cVc
    \param transport    Transport type
    \param payload_length   Payload length
    \param payload          Payload
*/
static void voiceEnhancementGaiaPlugin_GetConfigEnhancement(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    DEBUG_LOG_VERBOSE("voiceEnhancementGaiaPlugin_GetConfigEnhancement: payload");
    DEBUG_LOG_DATA_VERBOSE(payload, payload_length);

    if (payload_length > 0)
    {
        /* 3Mic cVc */
        if(payload[0] == VOICE_ENHANCEMENT_CAP_CVC_3MIC)
        {
            uint8 response[CVC_SEND_GET_CONFIG_PAYLOAD_LENGTH] = {0};
            response[0] = VOICE_ENHANCEMENT_CAP_CVC_3MIC;
            voiceEnhancementGaiaPlugin_CvcSendFillPayload(response);
            GaiaFramework_SendResponse(t, GAIA_VOICE_ENHANCEMENT_FEATURE_ID, get_config_voice_enhancement,
                                       CVC_SEND_GET_CONFIG_PAYLOAD_LENGTH, response);
        }
        else
        {
            DEBUG_LOG_ERROR("voiceEnhancementGaiaPlugin_GetConfigEnhancement, unknown feature");
            GaiaFramework_SendError(t, GAIA_VOICE_ENHANCEMENT_FEATURE_ID, get_config_voice_enhancement, invalid_parameter);
        }
    }
    else
    {
        DEBUG_LOG_ERROR("voiceEnhancementGaiaPlugin_GetConfigEnhancement, no valid payload");
        GaiaFramework_SendError(t, GAIA_VOICE_ENHANCEMENT_FEATURE_ID, get_config_voice_enhancement, invalid_parameter);
    }
}

/*! \brief Function that handles mkymera messages
    \param task     Kymera task
    \param id       Message ID
    \param message  Message
*/
static void voiceEnhancementGaiaPlugin_HandleKymeraMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);

    DEBUG_LOG_VERBOSE("voiceEnhancementGaiaPlugin_HandleKymeraMessage ID %d", id);
    switch(id)
    {
        case KYMERA_NOTIFICATION_CVC_SEND_MODE_CHANGED:
            voiceEnhancementGaiaPlugin_CvcSendModeChanged();
            break;

        default:
            break;
    };
}

/*! \brief Function that provides the mkymera message handler
    \return task    Kymera Message Handler
*/
static Task voiceEnhancementGaiaPlugin_KymeraStateTask(void)
{
  return (Task)&kymera_task;
}

/*! \brief Function that sends all available notifications
    \param transport    Transport type
*/
static void voiceEnhancementGaiaPlugin_SendAllNotifications(GAIA_TRANSPORT *t)
{
    UNUSED(t);
    DEBUG_LOG_VERBOSE("voiceEnhancementGaiaPlugin_SendAllNotifications");
    voiceEnhancementGaiaPlugin_CvcSendModeChanged();
}

#endif /* defined(INCLUDE_GAIA) && defined(INCLUDE_CVC_DEMO) */

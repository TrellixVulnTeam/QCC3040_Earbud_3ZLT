/*****************************************************************************
Copyright (c) 2018 - 2020 Qualcomm Technologies International, Ltd.

FILE NAME
    ama_command_handlers.c

DESCRIPTION
    Handlers for ama commands
*/

#ifdef INCLUDE_AMA
#include "ama.h"
#include "ama_connect_state.h"
#include "ama_voice_ui_handle.h"
#include "ama_audio.h"
#include "ama_state.h"
#include "ama_speech.h"
#include "ama_command_handlers.h"
#include "common.pb-c.h"
#include "speech.pb-c.h"
#include "logging.h"
#include "ama_send_envelope.h"
#include "ama_send_command.h"
#include "ama_private.h"
#include "ama_battery.h"
#include "ama_log.h"
#include <string.h>

#if defined (INCLUDE_ACCESSORY)
#include"bt_device.h"
#include "request_app_launch.h"
#endif

#define AMA_SEND_NOTIFY_DEVICE_CFG_DELAY        D_SEC(1)
#define AMA_INTERNAL_MSG_ASSISTANT_OVERRIDEN    1
#define MAKE_DEFAULT_RESPONSE_ENVELOPE(envelope_name, command_id) \
Response response = RESPONSE__INIT;\
response.error_code = ERROR_CODE__SUCCESS;\
ControlEnvelope  envelope_name = CONTROL_ENVELOPE__INIT;\
amaCommandHandlers_ElementPaster(&envelope_name, command_id , &response);

static void amaCommandHandlers_SendDefaultResponse(Command command);
static void amaCommandHandlers_InternalMsgHandler(Task task, MessageId id, Message message);
static TaskData internalMsgTask = {amaCommandHandlers_InternalMsgHandler};

static const SpeechInitiationType speech_initiations[] =
{
    SPEECH_INITIATION_TYPE__TAP,
#ifdef INCLUDE_WUW
    SPEECH_INITIATION_TYPE__WAKEWORD,
#endif
};

#ifdef INCLUDE_WUW
static const char * const wakewords[] =
{
    "alexa",
};
#endif

static void amaCommandHandlers_InternalMsgHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);

    switch(id)
    {
        case AMA_INTERNAL_MSG_ASSISTANT_OVERRIDEN:
            AmaSendCommand_NotifyDeviceConfig(ASSISTANT_OVERRIDEN);
        break;

        default:
            break;
    }
}

static void amaCommandHandlers_ElementPaster(ControlEnvelope* envelope_name,Command command_id, Response* response)
{
    envelope_name->command = command_id;
    envelope_name->u.response = response;
    envelope_name->payload_case = CONTROL_ENVELOPE__PAYLOAD_RESPONSE;
}

static void amaCommandHandlers_NotifyStateMsg(SpeechState state)
{
    ama_speech_state_t speech_state = ama_speech_state_err;

    switch(state)
    {
        case SPEECH_STATE__IDLE:
            speech_state = ama_speech_state_idle;
            break;

        case SPEECH_STATE__LISTENING:
            speech_state = ama_speech_state_listening;
            break;

        case SPEECH_STATE__PROCESSING:
            speech_state = ama_speech_state_processing;
            break;

        case SPEECH_STATE__SPEAKING:
            speech_state = ama_speech_state_speaking;
            break;

        default:
            DEBUG_LOG("AMA Unknown speech state indicated%d", state);
            break;
    }

    if(speech_state != ama_speech_state_err)
    {
        AmaNotifyAppMsg_StateMsg(speech_state);
    }
}

void AmaCommandHandlers_NotifySpeechState(ControlEnvelope *control_envelope_in)
{
    NotifySpeechState *notify_speech_state = control_envelope_in->u.notify_speech_state;

    DEBUG_LOG("AMA COMMAND__NOTIFY_SPEECH_STATE received. state enum:SpeechState:%d", notify_speech_state->state);

    amaCommandHandlers_NotifyStateMsg(notify_speech_state->state);
}


void AmaCommandHandlers_StopSpeech(ControlEnvelope *control_envelope_in)
{
    Dialog *dialog = control_envelope_in->u.stop_speech->dialog;

    DEBUG_LOG("AMA COMMAND__STOP_SPEECH received. Error code enum:ErrorCode:%d, id %ld",
                    control_envelope_in->u.stop_speech->error_code, dialog->id);

    if(dialog->id == AmaSpeech_GetCurrentDialogId())
    {
        AmaNotifyAppMsg_StopSpeechMsg();
    }

    amaCommandHandlers_SendDefaultResponse(control_envelope_in->command);
}

void AmaCommandHandlers_GetLocales(ControlEnvelope *control_envelope_in)
{
    DEBUG_LOG("AMA COMMAND__GET_LOCALES received");

    MAKE_DEFAULT_RESPONSE_ENVELOPE(control_envelope_out, control_envelope_in->command);

    ama_supported_locales_t supported_locales;
    AmaAudio_GetSupportedLocales(&supported_locales);
    Locales locales = LOCALES__INIT;
    const char *current_locale = AmaAudio_GetCurrentLocale();
    Locale locale[MAX_AMA_LOCALES] = {0};
    Locale *p_locale [MAX_AMA_LOCALES] = {0};
    for(uint8 i=0; i<supported_locales.num_locales; i++)
    {
        const Locale locale_init = LOCALE__INIT;
        locale[i] = locale_init;
        locale[i].name = (char*)supported_locales.name[i];
        p_locale[i] = &locale[i];
        if(strcmp(current_locale, locale[i].name) == 0)
        {
            locales.current_locale = p_locale[i];
        }
    }

    if (supported_locales.num_locales == 0)
    {
        const Locale locale_init = LOCALE__INIT;
        locale[0] = locale_init;
        locale[0].name = (char *) current_locale;
        supported_locales.num_locales = 1;
        p_locale[0] = &locale[0];
        locales.current_locale = p_locale[0];
    }

    locales.n_supported_locales = supported_locales.num_locales;
    locales.supported_locales = p_locale;

    DEBUG_LOG("AMA COMMAND__GET_LOCALES number of supported locales: %d", locales.n_supported_locales);
    DEBUG_LOG("AMA COMMAND__GET_LOCALES supported locales:");
    for(uint8 i=0; i<supported_locales.num_locales; i++)
    {
        AmaLog_LogVaArg("\t%s\n", p_locale[i]->name);
    }
    if (locales.current_locale)
    {
        AmaLog_LogVaArg("AMA COMMAND__GET_LOCALES current locale: %s\n", locales.current_locale->name);
    }

    response.u.locales = &locales;
    response.payload_case = RESPONSE__PAYLOAD_LOCALES;
    AmaSendEnvelope_Send(&control_envelope_out);
}

void AmaCommandHandlers_SetLocale(ControlEnvelope *control_envelope_in)
{
    MAKE_DEFAULT_RESPONSE_ENVELOPE(control_envelope_out, control_envelope_in->command);
    char* locale = control_envelope_in->u.set_locale->locale->name;
    const char* model = AmaAudio_GetModelFromLocale(locale);

    DEBUG_LOG("AMA COMMAND__SET_LOCALE received");

    if(AmaAudio_ValidateLocale(model))
    {
        DEBUG_LOG("AMA COMMAND__SET_LOCALE Locale = %c%c%c%c%c is valid",
                  locale[0], locale[1], locale[2], locale[3], locale[4]);
        AmaAudio_SetLocale(locale);
    }
    else
    {
        DEBUG_LOG("AMA COMMAND__SET_LOCALE Model Locale = %c%c%c%c%c is NOT valid",
                  locale[0], locale[1], locale[2], locale[3], locale[4]);
        response.error_code = ERROR_CODE__NOT_FOUND;
    }
    AmaSendEnvelope_Send(&control_envelope_out);
}

void AmaCommandHandlers_LaunchApp(ControlEnvelope *control_envelope_in)
{
    MAKE_DEFAULT_RESPONSE_ENVELOPE(control_envelope_out, control_envelope_in->command);

#if defined (INCLUDE_ACCESSORY)
    char* app_id = control_envelope_in->u.launch_app->app_id;
    DEBUG_LOG("AMA COMMAND__LAUNCH_APP received");
    if (app_id && strlen(app_id))
    {
        const bdaddr * bd_addr = Ama_GetBtAddress();

        if(bd_addr)
        {
            AmaLog_LogVaArg("AMA COMMAND__LAUNCH_APP app_id: %s\n", app_id);
            AccessoryFeature_RequestAppLaunch(*bd_addr, app_id, launch_without_user_alert);
        }
        else
        {
            DEBUG_LOG_ERROR("AmaCommandHandlers_LaunchApp: Unable to get handset Bdaddr");
        }
    }
    else
    {
        DEBUG_LOG_ERROR("AmaCommandHandlers_LaunchApp: NULL app_id %p or zero length string", app_id);
    }
#else
    DEBUG_LOG("AMA COMMAND__LAUNCH_APP not supported");
    response.error_code = ERROR_CODE__UNSUPPORTED;
#endif
    AmaSendEnvelope_Send(&control_envelope_out);
}

void AmaCommandHandlers_PopulateDeviceInformation(DeviceInformation *device_information)
{
    Transport supported_transports[NUMBER_OF_SUPPORTED_TRANSPORTS];
    int index;

    /* Get the AMA device configuration. */
    ama_device_config_t *device_config = AmaProtocol_GetDeviceConfiguration();

    device_information->n_supported_transports = 0;

    device_information->name = device_config->name;
    device_information->device_type = device_config->device_type;
    device_information->serial_number = device_config->serial_number;

    supported_transports[device_information->n_supported_transports++] = TRANSPORT__BLUETOOTH_RFCOMM;
#ifdef INCLUDE_ACCESSORY
    supported_transports[device_information->n_supported_transports++] = TRANSPORT__BLUETOOTH_IAP;
#endif

    device_information->supported_transports = &supported_transports[0];
    device_information->battery = AmaBattery_GetDeviceBattery();

    device_information->n_supported_speech_initiations = ARRAY_DIM(speech_initiations);
    device_information->supported_speech_initiations = (SpeechInitiationType *) speech_initiations;

#ifdef INCLUDE_WUW
    device_information->n_supported_wakewords = ARRAY_DIM(wakewords);
    device_information->supported_wakewords = (char **) wakewords;
#else
    device_information->n_supported_wakewords = 0;
    device_information->supported_wakewords = (char **) NULL;
#endif

    if (debug_log_level__global >= DEBUG_LOG_LEVEL_VERBOSE)
    {
        AmaLog_LogVaArg("AMA DEVICE_INFORMATION name %s\n", device_information->name);
        AmaLog_LogVaArg("AMA DEVICE_INFORMATION device_type %s\n", device_information->device_type);
        AmaLog_LogVaArg("AMA DEVICE_INFORMATION serial_number %s\n", device_information->serial_number);
    }

    DEBUG_LOG_VERBOSE("AMA DEVICE_INFORMATION number of supported transports %lu",
        device_information->n_supported_transports);
    for (index = 0; index < device_information->n_supported_transports; index++)
    {
        DEBUG_LOG_VERBOSE("AMA DEVICE_INFORMATION supported transport[%i]: enum:Transport:%d", 
            index, device_information->supported_transports[index]);
    }

    DEBUG_LOG_VERBOSE("AMA DEVICE_INFORMATION battery: level %lu, scale %lu, status enum:DeviceBattery__Status:%d",
        device_information->battery->level, device_information->battery->scale, device_information->battery->status);

    DEBUG_LOG_VERBOSE("AMA DEVICE_INFORMATION number of supported speech initiations %lu",
        device_information->n_supported_speech_initiations);
    for (index = 0; index < device_information->n_supported_speech_initiations; index++)
    {
        DEBUG_LOG_VERBOSE("AMA DEVICE_INFORMATION speech initiation[%i]: enum:SpeechInitiationType:%d",
            index, device_information->supported_speech_initiations[index]);
    }

    DEBUG_LOG_VERBOSE("AMA DEVICE_INFORMATION number of supported wakewords %lu",
        device_information->n_supported_wakewords);
    if (debug_log_level__global >= DEBUG_LOG_LEVEL_VERBOSE)
    {
        for (index = 0; index < device_information->n_supported_wakewords; index++)
        {
            AmaLog_LogVaArg("AMA DEVICE_INFORMATION wakeword[%i]: %s\n", index,
                device_information->supported_wakewords[index]);
        }
    }
}

#ifdef INCLUDE_AMA_DEVICE_CONTROLS
void AmaCommandHandlers_PopulateDeviceFeatures(DeviceFeatures *device_features)
{
    /*
     * The DeviceFeatures device_attributes, n_feature_properties and feature_properties fields are currently unused.
     * Only the DeviceFeatures features field is used and contains a bitmask of the supported features.
     */
    DEBUG_LOG_VERBOSE("AmaCommandHandlers_PopulateDeviceFeatures: Battery");
    device_features->features |= AMA_DEVICE_FEATURE_BATTERY_LEVEL;
#ifdef ENABLE_ANC
    DEBUG_LOG_VERBOSE("AmaCommandHandlers_PopulateDeviceFeatures: ANC");
    device_features->features |= AMA_DEVICE_FEATURE_ANC;
    DEBUG_LOG_VERBOSE("AmaCommandHandlers_PopulateDeviceFeatures: Passthrough");
    device_features->features |= AMA_DEVICE_FEATURE_PASSTHROUGH;
#endif
#ifdef INCLUDE_WUW
    DEBUG_LOG_VERBOSE("AmaCommandHandlers_PopulateDeviceFeatures: Wake Word");
    device_features->features |= AMA_DEVICE_FEATURE_WAKE_WORD;
    DEBUG_LOG_VERBOSE("AmaCommandHandlers_PopulateDeviceFeatures: Wake Word Privacy");
    device_features->features |= AMA_DEVICE_FEATURE_PRIVACY_MODE;
#endif
#ifdef INCLUDE_MUSIC_PROCESSING
    DEBUG_LOG_VERBOSE("AmaCommandHandlers_PopulateDeviceFeatures: Equalizer");
    device_features->features |= AMA_DEVICE_FEATURE_EQUALIZER;
#endif
    DEBUG_LOG_VERBOSE("AmaCommandHandlers_PopulateDeviceFeatures: features 0x%08lx", device_features->features);
}
#endif /* INCLUDE_AMA_DEVICE_CONTROLS */

void AmaCommandHandlers_GetDeviceInformation(ControlEnvelope *control_envelope_in)
{
    MAKE_DEFAULT_RESPONSE_ENVELOPE(control_envelope_out, control_envelope_in->command);

    DEBUG_LOG("AMA COMMAND__GET_DEVICE_INFORMATION received");
    DeviceInformation device_information = DEVICE_INFORMATION__INIT;
    AmaCommandHandlers_PopulateDeviceInformation(&device_information);

    /* assign resposne union type */
    response.u.device_information = &device_information;

    response.payload_case = RESPONSE__PAYLOAD_DEVICE_INFORMATION;

    AmaSendEnvelope_Send(&control_envelope_out);
}


void AmaCommandHandlers_GetDeviceConfiguration(ControlEnvelope *control_envelope_in)
{
    MAKE_DEFAULT_RESPONSE_ENVELOPE(control_envelope_out, control_envelope_in->command);

    DEBUG_LOG("AMA COMMAND__GET_DEVICE_CONFIGURATION received");

    DeviceConfiguration device_config = DEVICE_CONFIGURATION__INIT;

    bool require_assistant_override = !VoiceUi_IsActiveAssistant(Ama_GetVoiceUiHandle());
    device_config.needs_assistant_override = require_assistant_override;
    device_config.needs_setup = Ama_IsRegistered() ? FALSE : TRUE;

    DEBUG_LOG_VERBOSE("AMA COMMAND__GET_DEVICE_CONFIGURATION needs assistant override %u", device_config.needs_assistant_override);
    DEBUG_LOG_VERBOSE("AMA COMMAND__GET_DEVICE_CONFIGURATION needs setup %u", device_config.needs_setup);

    /* assign response union type */
    response.u.device_configuration = &device_config;

    response.payload_case = RESPONSE__PAYLOAD_DEVICE_CONFIGURATION;
    AmaSendEnvelope_Send(&control_envelope_out);
}


#ifdef INCLUDE_AMA_DEVICE_CONTROLS
void AmaCommandHandlers_GetDeviceFeatures(ControlEnvelope *control_envelope_in)
{
    MAKE_DEFAULT_RESPONSE_ENVELOPE(control_envelope_out, control_envelope_in->command);

    DEBUG_LOG("AMA COMMAND__GET_DEVICE_FEATURES received");

    DeviceFeatures device_features = DEVICE_FEATURES__INIT;
    AmaCommandHandlers_PopulateDeviceFeatures(&device_features);

    /* assign response union type */
    response.u.device_features = &device_features;

    DEBUG_LOG("AMA COMMAND__GET_DEVICE_FEATURES response: features 0x%08lx", response.u.device_features->features);

    response.payload_case = RESPONSE__PAYLOAD_DEVICE_FEATURES;
    AmaSendEnvelope_Send(&control_envelope_out);
}
#endif /* INCLUDE_AMA_DEVICE_CONTROLS */


void AmaCommandHandlers_StartSetup(ControlEnvelope *control_envelope_in)
{
    DEBUG_LOG("AMA COMMAND__START_SETUP received");
    amaCommandHandlers_SendDefaultResponse(control_envelope_in->command);
}

void AmaCommandHandlers_CompleteSetup(ControlEnvelope *control_envelope_in)
{
    DEBUG_LOG("AMA COMMAND__COMPLETE_SETUP received");
    Ama_CompleteSetup();
    amaCommandHandlers_SendDefaultResponse(control_envelope_in->command);
}

static void amaCommandHandlers_BdaddrToArray(uint8 *array, bdaddr *bdaddr_in)
{
    array[1] = (uint8)(bdaddr_in->nap & 0xff);
    array[0] = (uint8)((bdaddr_in->nap>>8) & 0xff);
    array[2] = bdaddr_in->uap;
    array[5] = (uint8)(bdaddr_in->lap) & 0xff;
    array[4] = (uint8)(bdaddr_in->lap>>8) & 0xff ;
    array[3] = (uint8)(bdaddr_in->lap>>16) & 0xff ;
}

void AmaCommandHandlers_UpgradeTransport(ControlEnvelope *control_envelope_in)
{
    uint8 bdaddr_array[6];
    MAKE_DEFAULT_RESPONSE_ENVELOPE(control_envelope_out, control_envelope_in->command);
    ConnectionDetails connection_details = CONNECTION_DETAILS__INIT;
    UpgradeTransport *upgrade_transport = control_envelope_in->u.upgrade_transport;

    DEBUG_LOG("AMA COMMAND__UPGRADE_TRANSPORT received. Transport enum:Transport:%d", upgrade_transport->transport);

    amaCommandHandlers_BdaddrToArray(bdaddr_array, AmaProtocol_GetLocalAddress());

    AmaProtocol_SendAppMsg(AMA_ENABLE_CLASSIC_PAIRING_IND, NULL);

    connection_details.identifier.len = 6;
    connection_details.identifier.data = bdaddr_array;

    DEBUG_LOG_VERBOSE("AMA COMMAND__UPGRADE_TRANSPORT connection details: len %d, data %02x %02x %02x %02x %02x %02x",
        connection_details.identifier.len,
        connection_details.identifier.data[0],
        connection_details.identifier.data[1],
        connection_details.identifier.data[2],
        connection_details.identifier.data[3],
        connection_details.identifier.data[4],
        connection_details.identifier.data[5]);

    response.payload_case = RESPONSE__PAYLOAD_CONNECTION_DETAILS;
    response.u.connection_details = &connection_details;

    AmaProtocol_SendAppMsg(AMA_UPGRADE_TRANSPORT_IND, NULL);

    AmaSendEnvelope_Send(&control_envelope_out);
}

void AmaCommandHandlers_SwitchTransport(ControlEnvelope *control_envelope_in)
{

    MAKE_DEFAULT_RESPONSE_ENVELOPE(control_envelope_out, control_envelope_in->command);

    SwitchTransport* switch_transport = control_envelope_in->u.switch_transport;

    DEBUG_LOG("AMA COMMAND__SWITCH_TRANSPORT received. Transport enum:Transport:%d", switch_transport->new_transport);

    AmaSendEnvelope_Send(&control_envelope_out);

    /* AMA_TODO ... Revisit for future improvement */
    AmaSendCommand_GetCentralInformation();

    AmaNotifyAppMsg_TransportSwitch((ama_transport_t)switch_transport->new_transport);
}

void AmaCommandHandlers_SynchronizeSettings(ControlEnvelope *control_envelope_in)
{
    MAKE_DEFAULT_RESPONSE_ENVELOPE(control_envelope_out, control_envelope_in->command);

    DEBUG_LOG("AMA COMMAND__SYNCHRONIZE_SETTINGS received");

    AmaSendEnvelope_Send(&control_envelope_out);

    AmaSendCommand_GetCentralInformation();

    AmaNotifyAppMsg_SynchronizeSettingMsg();
}

void AmaCommandHandlers_GetState(ControlEnvelope *control_envelope_in)
{
    MAKE_DEFAULT_RESPONSE_ENVELOPE(control_envelope_out, control_envelope_in->command);

    GetState *get_state = control_envelope_in->u.get_state;
    uint32_t feature = get_state->feature;
    uint32 state_value;
    State__ValueCase value_case;

    DEBUG_LOG("AMA COMMAND__GET_STATE feature %x", feature);

    State state = STATE__INIT;

    response.error_code = (ErrorCode) AmaState_GetState(feature, &state_value, (ama_state_value_case_t*)&value_case);

    state.value_case = value_case;
    state.feature = feature;

    if(state.value_case == STATE__VALUE_BOOLEAN)
    {
        state.u.boolean = (protobuf_c_boolean)state_value;
    }
    else if(state.value_case == STATE__VALUE_INTEGER)
    {
        state.u.integer = (uint32_t)state_value;
    }

    DEBUG_LOG_VERBOSE("AMA COMMAND__GET_STATE feature %x, error code enum:ErrorCode:%d",
        state.feature, response.error_code);
    if (response.error_code == ERROR_CODE__SUCCESS)
    {
        DEBUG_LOG_VERBOSE("AMA COMMAND__GET_STATE value case enum:State__ValueCase:%d, value %d",
            state.value_case, state_value);
    }
    response.payload_case = RESPONSE__PAYLOAD_STATE;
    response.u.state = &state;

    AmaSendEnvelope_Send(&control_envelope_out);
}


void AmaCommandHandlers_SetState(ControlEnvelope *control_envelope_in)
{
    MAKE_DEFAULT_RESPONSE_ENVELOPE(control_envelope_out, control_envelope_in->command);

    SetState *set_state = control_envelope_in->u.set_state;
    uint32 feature = (uint32)set_state->state->feature;
    State__ValueCase value_case = set_state->state->value_case;
    uint32 state_value = 0xFFFF;

    if(value_case == STATE__VALUE_BOOLEAN)
    {
        state_value = (uint32)set_state->state->u.boolean;
    }
    else if (value_case == STATE__VALUE_INTEGER)
    {
        state_value = (uint32)set_state->state->u.integer;
    }

    response.error_code = (ErrorCode) AmaState_SetState(feature, state_value, (ama_state_value_case_t) value_case);

    DEBUG_LOG("AMA COMMAND__SET_STATE feature %x, value case enum:State__ValueCase:%d, value %d, error code enum:ErrorCode:%d",
        feature, value_case, state_value, response.error_code);

    AmaSendEnvelope_Send(&control_envelope_out);

}

void AmaCommandHandlers_MediaControl(ControlEnvelope *control_envelope_in)
{

    IssueMediaControl *issue_media_control = control_envelope_in->u.issue_media_control;
    MediaControl control =  issue_media_control->control;

    DEBUG_LOG("AMA COMMAND__ISSUE_MEDIA_CONTROL received. control enum:MediaControl:%d", control);

    AmaProtocol_MediaControl((AMA_MEDIA_CONTROL) control);
    amaCommandHandlers_SendDefaultResponse(control_envelope_in->command);
}

void AmaCommandHandlers_OverrideAssistant(ControlEnvelope *control_envelope_in)
{
    DEBUG_LOG("AMA COMMAND__OVERRIDE_ASSISTANT received");
    AmaNotifyAppMsg_OverrideAssistant();
    amaCommandHandlers_SendDefaultResponse(control_envelope_in->command);

    /* Notify Alexa that the assistant has been overridden */
    MessageSendLater(&internalMsgTask, AMA_INTERNAL_MSG_ASSISTANT_OVERRIDEN,
                     NULL, AMA_SEND_NOTIFY_DEVICE_CFG_DELAY);
}

void AmaCommandHandlers_SynchronizeState(ControlEnvelope *control_envelope_in)
{
    DEBUG_LOG("AMA COMMAND__SYNCHRONIZE_STATE received");
    amaCommandHandlers_SendDefaultResponse(control_envelope_in->command);
}

void AmaCommandHandlers_ProvideSpeech(ControlEnvelope *control_envelope_in)
{
    ProvideSpeech *provide_speech = control_envelope_in->u.provide_speech;
    Dialog* dialog = provide_speech->dialog;

    DEBUG_LOG("AMA COMMAND__PROVIDE_SPEECH - dialog id =%d", dialog->id);

    AmaNotifyAppMsg_ProvideSpeechMsg(dialog->id);
}

void AmaCommandHandlers_EndpointSpeech(ControlEnvelope *control_envelope_in)
{

    DEBUG_LOG("AMA COMMAND__ENDPOINT_SPEECH received");

    if(control_envelope_in->payload_case == CONTROL_ENVELOPE__PAYLOAD_ENDPOINT_SPEECH)
    {
        EndpointSpeech* endpoint_speech = control_envelope_in->u.endpoint_speech;
        Dialog *dialog = endpoint_speech->dialog;

        DEBUG_LOG("AMA COMMAND__ENDPOINT_SPEECH: ENDPOINT_SPEECH: Dialog ID %d", dialog->id);
        if(dialog->id == AmaSpeech_GetCurrentDialogId())
        {
            AmaNotifyAppMsg_StopSpeechMsg();
        }
        else
        {
            DEBUG_LOG_ERROR("AMA COMMAND__ENDPOINT_SPEECH: Dialog Id incorrect. Received %d, should be %d",
                            dialog->id,
                            AmaSpeech_GetCurrentDialogId());
        }
    }
    else if(control_envelope_in->payload_case == CONTROL_ENVELOPE__PAYLOAD_NOTIFY_SPEECH_STATE)
    {
        /* probably we get this case if send end speech when there is no speech going on */
        NotifySpeechState * notify_speech_state = control_envelope_in->u.notify_speech_state;
        SpeechState state = notify_speech_state->state;
        DEBUG_LOG("AMA COMMAND__ENDPOINT_SPEECH: NOTIFY_SPEECH_STATE: state %d", state);
        amaCommandHandlers_NotifyStateMsg(state);
    }
    else
    {
        DEBUG_LOG_ERROR("AMA COMMAND__ENDPOINT_SPEECH: Unexpected payload case enum:ControlEnvelope__PayloadCase:%d",
            control_envelope_in->payload_case);
    }

    amaCommandHandlers_SendDefaultResponse(control_envelope_in->command);

}


static ErrorCode amaCommandHandlers_ProcessForwardAtCommand(char* command)
{
    typedef struct{
        char* at_string;
        ama_at_cmd_t command;
    }at_lookup_t;

    static const at_lookup_t  at_lookup[] = {
        {"ATA",           ama_at_cmd_ata_ind},
        {"AT+CHUP",       ama_at_cmd_at_plus_chup_ind},
        {"AT+BLDN",       ama_at_cmd_at_plus_bldn_ind},
        {"AT+CHLD=0",     ama_at_cmd_at_plus_chld_eq_0_ind},
        {"AT+CHLD=1",     ama_at_cmd_at_plus_chld_eq_1_ind},
        {"AT+CHLD=2",     ama_at_cmd_at_plus_chld_eq_2_ind},
        {"AT+CHLD=3",     ama_at_cmd_at_plus_chld_eq_3_ind},
        {"ATD",           ama_at_cmd_atd_ind}
    };

    uint8 num_of_commands = sizeof(at_lookup) / sizeof(at_lookup[0]);
    uint8 index;

    for(index = 0; index < num_of_commands; index++)
    {
        if(strcmp(at_lookup[index].at_string, command) == 0)
        {
            MAKE_AMA_MESSAGE(AMA_SEND_AT_COMMAND_IND);

            message->at_command = at_lookup[index].command;

            AmaProtocol_SendAppMsg(AMA_SEND_AT_COMMAND_IND, message);

            return ERROR_CODE__SUCCESS;
        }
    }

    return ERROR_CODE__UNKNOWN;
}


void AmaCommandHandlers_ForwardATCommand(ControlEnvelope *control_envelope_in)
{
    MAKE_DEFAULT_RESPONSE_ENVELOPE(control_envelope_out, control_envelope_in->command);

    ForwardATCommand* forward_at_command = control_envelope_in->u.forward_at_command;

    char* forward_command = forward_at_command->command;

    AmaLog_LogVaArg("AMA COMMAND__FORWARD_AT_COMMAND received. Command %s\n", forward_command);

    response.error_code = amaCommandHandlers_ProcessForwardAtCommand(forward_command);

    DEBUG_LOG("AMA COMMAND__FORWARD_AT_COMMAND: Error code enum:ErrorCode:%d", response.error_code);

    AmaSendEnvelope_Send(&control_envelope_out);
}

void AmaCommandHandlers_NotHandled(ControlEnvelope *control_envelope_in)
{
    MAKE_DEFAULT_RESPONSE_ENVELOPE(control_envelope_out, control_envelope_in->command);
    DEBUG_LOG("AMA unhandled command!! %d", control_envelope_in->command);
    response.error_code = ERROR_CODE__UNSUPPORTED;
    AmaSendEnvelope_Send(&control_envelope_out);
}


void AmaCommandHandlers_KeepAlive(ControlEnvelope *control_envelope_in)
{
    DEBUG_LOG("AMA COMMAND__KEEP_ALIVE received");
    amaCommandHandlers_SendDefaultResponse(control_envelope_in->command);
}


static void amaCommandHandlers_SendDefaultResponse(Command command)
{
    MAKE_DEFAULT_RESPONSE_ENVELOPE(control_envelope_out, command);
    AmaSendEnvelope_Send(&control_envelope_out);
}

#endif /* INCLUDE_AMA */

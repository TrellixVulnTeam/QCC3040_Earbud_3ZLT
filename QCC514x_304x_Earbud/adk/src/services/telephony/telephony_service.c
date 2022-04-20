/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of the Telephony Service
*/

#include "telephony_service.h"
#include "telephony_service_call_control.h"

#include "kymera_adaptation.h"
#include "ui.h"
#include "ui_inputs.h"
#include "telephony_messages.h"
#include "voice_sources.h"
#include "voice_sources_list.h"
#include "usb_audio.h"
#include "audio_router.h"

#include <focus_voice_source.h>
#include <panic.h>
#include <logging.h>

static void telephonyService_CallStateNotificationMessageHandler(Task task, MessageId id, Message message);
static void telephonyService_HandleUiInput(Task task, MessageId ui_input, Message message);

static TaskData telephony_message_handler_task = { telephonyService_CallStateNotificationMessageHandler };
static TaskData ui_handler_task = { telephonyService_HandleUiInput };

static const message_group_t ui_inputs[] =
{
    UI_INPUTS_TELEPHONY_MESSAGE_GROUP,
};

static void telephonyService_AddVoiceSource(voice_source_t source)
{
    generic_source_t voice_source = {.type = source_type_voice, .u.voice = source};

    AudioRouter_AddSource(voice_source);
}

static void telephonyService_handleTelephonyAudioConnecting(const TELEPHONY_AUDIO_CONNECTING_T *message)
{
    if(message)
    {
        DEBUG_LOG_INFO("telephonyService_handleTelephonyAudioConnecting enum:voice_source_t:%d", message->voice_source);

        telephonyService_AddVoiceSource(message->voice_source);
    }
}

static void telephonyService_handleTelephonyAudioConnected(const TELEPHONY_AUDIO_CONNECTED_T *message)
{
    if(message)
    {
        DEBUG_LOG_INFO("telephonyService_handleTelephonyAudioConnected enum:voice_source_t:%d", message->voice_source);

        telephonyService_AddVoiceSource(message->voice_source);
    }
}

static void telephonyService_handleTelephonyAudioDisconnected(const TELEPHONY_AUDIO_DISCONNECTED_T *message)
{
    if(message)
    {
        generic_source_t voice_source = {.type = source_type_voice, .u.voice = message->voice_source};

        DEBUG_LOG_INFO("telephonyService_handleTelephonyAudioDisconnected enum:voice_source_t:%d", message->voice_source);

        AudioRouter_RemoveSource(voice_source);
    }
}

static void telephonyService_CallStateNotificationMessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);
    switch(id)
    {
        case TELEPHONY_AUDIO_CONNECTED:
            telephonyService_handleTelephonyAudioConnected((const TELEPHONY_AUDIO_CONNECTED_T*)message);
            break;

        case TELEPHONY_AUDIO_DISCONNECTED:
            telephonyService_handleTelephonyAudioDisconnected((const TELEPHONY_AUDIO_DISCONNECTED_T*)message);
            break;

        case TELEPHONY_AUDIO_CONNECTING:
            telephonyService_handleTelephonyAudioConnecting((const TELEPHONY_AUDIO_CONNECTING_T*)message);
            break;

        case TELEPHONY_INCOMING_CALL:
        case TELEPHONY_INCOMING_CALL_OUT_OF_BAND_RINGTONE:
        case TELEPHONY_CALL_ONGOING:
            AudioRouter_Update();
            break;
    
        case TELEPHONY_CALL_ENDED:
        {
            TelephonyService_ResumeHighestPriorityHeldCallRemaining();
            AudioRouter_Update();
            break;
        }
        
        default:
            DEBUG_LOG_VERBOSE("telephonyService_CallStateNotificationMessageHandler: Unhandled event MESSAGE:0x%x", id);
            break;
    }
}

static void telephonyService_HandleUiInput(Task task, MessageId ui_input, Message message)
{
    UNUSED(task);
    UNUSED(message);

    voice_source_t source = voice_source_none;

    Focus_GetVoiceSourceForUiInput(ui_input, &source);

    if(source == voice_source_none)
    {
        return;
    }

    switch(ui_input)
    {
        case ui_input_voice_call_hang_up:
            TelephonyService_HangUpCall(source);
            break;

        case ui_input_voice_call_accept:
            TelephonyService_AnswerCall(source);
            break;

        case ui_input_voice_call_reject:
            TelephonyService_RejectCall(source);
            break;

        case ui_input_voice_transfer:
            VoiceSources_TransferOngoingCallAudio(source, voice_source_audio_transfer_toggle);
            break;

        case ui_input_voice_transfer_to_ag:
            VoiceSources_TransferOngoingCallAudio(source, voice_source_audio_transfer_to_ag);
            break;

        case ui_input_voice_transfer_to_headset:
            VoiceSources_TransferOngoingCallAudio(source, voice_source_audio_transfer_to_hfp);
            break; 

        case ui_input_voice_dial:
            VoiceSources_InitiateVoiceDial(source);
            break;

        case ui_input_voice_call_last_dialed:
            VoiceSources_InitiateCallLastDialled(source);
            break;

        case ui_input_mic_mute_toggle:
            VoiceSources_ToggleMicrophoneMute(source);
            break;
        
        case ui_input_voice_call_cycle:
            TelephonyService_CycleToNextCall(source);
            break;
        
        case ui_input_voice_call_join_calls:
            TelephonyService_JoinCalls(source, telephony_join_calls_and_stay);
            break;
        
        case ui_input_voice_call_join_calls_and_hang_up:
            TelephonyService_JoinCalls(source, telephony_join_calls_and_leave);
            break;

        default:
            break;
    }
}

static unsigned telephonyService_AddSourceContextToInCall(voice_source_t source)
{
    switch(VoiceSources_GetSourceContext(source))
    {
        case context_voice_ringing_incoming:
        case context_voice_in_call_with_incoming:
            return context_voice_in_call_with_incoming;
        
        case context_voice_ringing_outgoing:
        case context_voice_in_call_with_outgoing:
            return context_voice_in_call_with_outgoing;
        
        case context_voice_call_held:
        case context_voice_in_call_with_held:
            return context_voice_in_call_with_held;
        
        default:
            return context_voice_in_call;
    }
}

static unsigned telephonyService_GetMultiCallContext(unsigned focus_context)
{
    voice_source_t background_source = voice_source_none;
    
    switch(focus_context)
    {
        case context_voice_ringing_incoming:
        {
            if(TelephonyService_FindActiveCall(&background_source))
            {
                return context_voice_in_call_with_incoming;
            }
        }
        break;
        
        case context_voice_ringing_outgoing:
        {
            if(TelephonyService_FindActiveCall(&background_source))
            {
                return context_voice_in_call_with_outgoing;
            }
        }
        break;
        
        case context_voice_in_call:
        {
            if(TelephonyService_FindIncomingOutgoingOrHeldCall(&background_source))
            {
                return telephonyService_AddSourceContextToInCall(background_source);
            }
        }
        break;
        
        default:
        break;
    }
    
    return focus_context;
}

static unsigned telephonyService_GetContext(void)
{
    unsigned context = context_voice_disconnected;
    voice_source_t focused_source = voice_source_none;

    if (Focus_GetVoiceSourceForContext(ui_provider_telephony, &focused_source))
    {
        context = VoiceSources_GetSourceContext(focused_source);
    }
    
    context = telephonyService_GetMultiCallContext(context);

    return context;
}

bool TelephonyService_Init(Task init_task)
{
    UNUSED(init_task);
    Telephony_RegisterForMessages(&telephony_message_handler_task);

    Ui_RegisterUiProvider(ui_provider_telephony, telephonyService_GetContext);

    Ui_RegisterUiInputConsumer(&ui_handler_task, (uint16*)ui_inputs, sizeof(ui_inputs)/sizeof(uint16));
    UsbAudio_ClientRegister(&telephony_message_handler_task,
                              USB_AUDIO_REGISTERED_CLIENT_TELEPHONY);
    return TRUE;
}

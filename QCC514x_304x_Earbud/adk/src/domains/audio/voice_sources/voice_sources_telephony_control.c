/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of the voice_sources_telephony_control composite.
*/

#include "voice_sources.h"

#include <logging.h>
#include <panic.h>
#include <ui.h>

static const voice_source_telephony_control_interface_t * telephony_control_interface[max_voice_sources];

static void voiceSources_ValidateSource(voice_source_t source)
{
    if((source <= voice_source_none) || (source >= max_voice_sources))
    {
        Panic();
    }
}

static bool voiceSources_IsSourceRegistered(voice_source_t source)
{
    return ((telephony_control_interface[source] == NULL) ? FALSE : TRUE);
}

void VoiceSources_TelephonyControlRegistryInit(void)
{
    memset(telephony_control_interface, 0, sizeof(telephony_control_interface));
}

void VoiceSources_RegisterTelephonyControlInterface(voice_source_t source, const voice_source_telephony_control_interface_t * interface)
{
    voiceSources_ValidateSource(source);
    PanicNull((void *)interface);
    telephony_control_interface[source] = interface;
}

void VoiceSources_DeregisterTelephonyControlInterface(voice_source_t source)
{
    voiceSources_ValidateSource(source);
    telephony_control_interface[source] = NULL;
}

void VoiceSources_AcceptIncomingCall(voice_source_t source)
{
    voiceSources_ValidateSource(source);
    if(voiceSources_IsSourceRegistered(source) && (telephony_control_interface[source]->IncomingCallAccept))
    {
        telephony_control_interface[source]->IncomingCallAccept(source);
    }
}

void VoiceSources_RejectIncomingCall(voice_source_t source)
{
    voiceSources_ValidateSource(source);
    if(voiceSources_IsSourceRegistered(source) && (telephony_control_interface[source]->IncomingCallReject))
    {
        telephony_control_interface[source]->IncomingCallReject(source);
    }
}

void VoiceSources_TerminateOngoingCall(voice_source_t source)
{
    voiceSources_ValidateSource(source);
    if(voiceSources_IsSourceRegistered(source) && (telephony_control_interface[source]->OngoingCallTerminate))
    {
        telephony_control_interface[source]->OngoingCallTerminate(source);
    }
}

void VoiceSources_TransferOngoingCallAudio(voice_source_t source, voice_source_audio_transfer_direction_t direction)
{
    voiceSources_ValidateSource(source);
    if(voiceSources_IsSourceRegistered(source) && (telephony_control_interface[source]->OngoingCallTransferAudio))
    {
        telephony_control_interface[source]->OngoingCallTransferAudio(source, direction);
    }
}

void VoiceSources_InitiateCallUsingNumber(voice_source_t source, phone_number_t number)
{
    voiceSources_ValidateSource(source);
    if(voiceSources_IsSourceRegistered(source) && (telephony_control_interface[source]->InitiateCallUsingNumber))
    {
        telephony_control_interface[source]->InitiateCallUsingNumber(source, number);
    }
}

void VoiceSources_InitiateVoiceDial(voice_source_t source)
{
    voiceSources_ValidateSource(source);
    if(voiceSources_IsSourceRegistered(source) && (telephony_control_interface[source]->InitiateVoiceDial))
    {
        telephony_control_interface[source]->InitiateVoiceDial(source);
    }
}

void VoiceSources_InitiateCallLastDialled(voice_source_t source)
{
    voiceSources_ValidateSource(source);
    if(voiceSources_IsSourceRegistered(source) && (telephony_control_interface[source]->InitiateCallLastDialled))
    {
        telephony_control_interface[source]->InitiateCallLastDialled(source);
    }
}

void VoiceSources_ToggleMicrophoneMute(voice_source_t source)
{
    voiceSources_ValidateSource(source);
    if(voiceSources_IsSourceRegistered(source) && (telephony_control_interface[source]->ToggleMicrophoneMute))
    {
        telephony_control_interface[source]->ToggleMicrophoneMute(source);
    }
}

unsigned VoiceSources_GetSourceContext(voice_source_t source)
{
    unsigned context = context_voice_disconnected;

    voiceSources_ValidateSource(source);
    if (voiceSources_IsSourceRegistered(source) && (telephony_control_interface[source]->GetUiProviderContext))
    {
        context = telephony_control_interface[source]->GetUiProviderContext(source);
    }

    DEBUG_LOG("VoiceSources_GetSourceContext enum:voice_source_t:%d enum:voice_source_provider_context_t:%d",
              source, context);

    return context;
}

void VoiceSources_TwcControl(voice_source_t source, voice_source_twc_control_t action)
{
    voiceSources_ValidateSource(source);
    if(voiceSources_IsSourceRegistered(source) && (telephony_control_interface[source]->TwcControl))
    {
        telephony_control_interface[source]->TwcControl(source, action);
    }
}

bool VoiceSources_IsSourceRegisteredForTelephonyControl(voice_source_t source)
{
    voiceSources_ValidateSource(source);
    return voiceSources_IsSourceRegistered(source);
}
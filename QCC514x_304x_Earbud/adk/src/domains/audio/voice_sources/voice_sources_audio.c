/*!
\copyright  Copyright (c) 2018-2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of the voice_sources_audio composite.
*/

#include "voice_sources.h"
#include "hfp_profile.h"
#include <logging.h>

#include <panic.h>

static const voice_source_audio_interface_t * voice_source_audio_interfaces[max_voice_sources];

static void voiceSources_ValidateSource(voice_source_t source)
{
    if((source <= voice_source_none) || (source >= max_voice_sources))
    {
        Panic();
    }
}

static bool voiceSources_IsSourceRegistered(voice_source_t source)
{
    return ((voice_source_audio_interfaces[source] == NULL) ? FALSE : TRUE);
}

void VoiceSources_AudioRegistryInit(void)
{
    memset(voice_source_audio_interfaces, 0, sizeof(voice_source_audio_interfaces));
}

void VoiceSources_RegisterAudioInterface(voice_source_t source, const voice_source_audio_interface_t * interface)
{
    voiceSources_ValidateSource(source);
    PanicNull((void *)interface);
    voice_source_audio_interfaces[source] = interface;
}

bool VoiceSources_GetConnectParameters(voice_source_t source, source_defined_params_t * source_params)
{
    bool return_value = FALSE;
    voiceSources_ValidateSource(source);
    if(voiceSources_IsSourceRegistered(source) && (voice_source_audio_interfaces[source]->GetConnectParameters))
    {
        return_value = voice_source_audio_interfaces[source]->GetConnectParameters(source, source_params);
    }
    return return_value;
}

void VoiceSources_ReleaseConnectParameters(voice_source_t source, source_defined_params_t * source_params)
{
    voiceSources_ValidateSource(source);
    if(voiceSources_IsSourceRegistered(source) && (voice_source_audio_interfaces[source]->ReleaseConnectParameters))
    {
        voice_source_audio_interfaces[source]->ReleaseConnectParameters(source, source_params);
    }
}

bool VoiceSources_GetDisconnectParameters(voice_source_t source, source_defined_params_t * source_params)
{
    bool return_value = FALSE;
    voiceSources_ValidateSource(source);
    if(voiceSources_IsSourceRegistered(source) && (voice_source_audio_interfaces[source]->GetDisconnectParameters))
    {
        return_value = voice_source_audio_interfaces[source]->GetDisconnectParameters(source, source_params);
    }
    return return_value;
}

void VoiceSources_ReleaseDisconnectParameters(voice_source_t source, source_defined_params_t * source_params)
{
    voiceSources_ValidateSource(source);
    if(voiceSources_IsSourceRegistered(source) && (voice_source_audio_interfaces[source]->ReleaseDisconnectParameters))
    {
        voice_source_audio_interfaces[source]->ReleaseDisconnectParameters(source, source_params);
    }
}

bool VoiceSources_IsAudioRouted(voice_source_t source)
{
    bool is_routed = FALSE;
    voiceSources_ValidateSource(source);
    if(voiceSources_IsSourceRegistered(source) && (voice_source_audio_interfaces[source]->IsAudioRouted))
    {
        is_routed = voice_source_audio_interfaces[source]->IsAudioRouted(source);
    }
    return is_routed;
}

bool VoiceSources_IsVoiceChannelAvailable(voice_source_t source)
{
    bool is_available = FALSE;
    voiceSources_ValidateSource(source);
    if(voiceSources_IsSourceRegistered(source) && (voice_source_audio_interfaces[source]->IsVoiceChannelAvailable))
    {
        is_available = voice_source_audio_interfaces[source]->IsVoiceChannelAvailable(source);
    }
    return is_available;
}

source_status_t VoiceSources_SetState(voice_source_t source, source_state_t state)
{
    source_status_t status = source_status_ready;

    DEBUG_LOG_FN_ENTRY("VoiceSources_SetState enum:voice_source_t:%d, enum:source_state_t:%d", source, state);

    if(voiceSources_IsSourceRegistered(source) && (voice_source_audio_interfaces[source]->SetState))
    {
        status = voice_source_audio_interfaces[source]->SetState(source, state);
    }
    return status;
}

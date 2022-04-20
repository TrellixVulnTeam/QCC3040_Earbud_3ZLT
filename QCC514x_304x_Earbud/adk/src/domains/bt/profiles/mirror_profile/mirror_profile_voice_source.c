/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      The voice source interface implementation for Mirror Profile
*/

#ifdef INCLUDE_MIRRORING

#include "mirror_profile_private.h"
#include "mirror_profile_voice_source.h"
#include "mirror_profile_signalling.h"
#include "source_param_types.h"
#include "voice_sources.h"
#include "volume_system.h"
#include "telephony_messages.h"

#include <stdlib.h>
#include <panic.h>

static bool mirrorProfile_GetConnectParameters(voice_source_t source, source_defined_params_t * source_params);
static void mirrorProfile_FreeConnectParameters(voice_source_t source, source_defined_params_t * source_params);
static bool mirrorProfile_GetDisconnectParameters(voice_source_t source, source_defined_params_t * source_params);
static void mirrorProfile_FreeDisconnectParameters(voice_source_t source, source_defined_params_t * source_params);
static bool mirrorProfile_IsVoiceAvailable(voice_source_t source);
static unsigned mirrorProfile_GetCurrentContext(voice_source_t source);

static const voice_source_audio_interface_t mirror_voice_interface =
{
    .GetConnectParameters = mirrorProfile_GetConnectParameters,
    .ReleaseConnectParameters = mirrorProfile_FreeConnectParameters,
    .GetDisconnectParameters = mirrorProfile_GetDisconnectParameters,
    .ReleaseDisconnectParameters = mirrorProfile_FreeDisconnectParameters,
    .IsAudioRouted = mirrorProfile_IsVoiceAvailable,
    .IsVoiceChannelAvailable = mirrorProfile_IsVoiceAvailable,
    .SetState = NULL
};

static const voice_source_telephony_control_interface_t mirror_telephony_interface =
{
    .GetUiProviderContext = mirrorProfile_GetCurrentContext
};


static bool mirrorProfile_GetConnectParameters(voice_source_t source, source_defined_params_t * source_params)
{
    mirror_profile_esco_t *esco = MirrorProfile_GetScoState();
    voice_connect_parameters_t *voice_connect_params = PanicUnlessMalloc(sizeof(*voice_connect_params));
    memset(voice_connect_params, 0, sizeof(voice_connect_parameters_t));

    voice_connect_params->audio_sink = StreamScoSink(esco->conn_handle);
    voice_connect_params->codec_mode = esco->codec_mode;
    voice_connect_params->wesco = esco->wesco;
    voice_connect_params->volume = VoiceSources_CalculateOutputVolume(source);
    voice_connect_params->volume.value = esco->volume;
    voice_connect_params->synchronised_start = TRUE;
    voice_connect_params->started_handler = MirrorProfile_HandleKymeraScoStarted;

    source_params->data = (void *)voice_connect_params;
    source_params->data_length = sizeof(voice_connect_parameters_t);

    UNUSED(source);
    return TRUE;
}

static void mirrorProfile_FreeConnectParameters(voice_source_t source, source_defined_params_t * source_params)
{
    free(source_params->data);
    source_params->data = (void *)NULL;
    source_params->data_length = 0;

    UNUSED(source);
}

static bool mirrorProfile_GetDisconnectParameters(voice_source_t source, source_defined_params_t * source_params)
{
    PanicNull(source_params);
    source_params->data = (void *)NULL;
    source_params->data_length = 0;

    UNUSED(source);
    return TRUE;
}

static void mirrorProfile_FreeDisconnectParameters(voice_source_t source, source_defined_params_t * source_params)
{
    PanicNull(source_params);
    UNUSED(source);
    source_params->data = (void *)NULL;
    source_params->data_length = 0;
}

static inline bool mirrorProfile_IsSecondaryAndEscoActive(void)
{
    return MirrorProfile_IsSecondary() && MirrorProfile_IsEscoActive();
}

static bool mirrorProfile_IsVoiceAvailable(voice_source_t source)
{
    bool is_available = FALSE;
    voice_source_t voice_source = MirrorProfile_GetScoState()->voice_source;
    if(source == voice_source && mirrorProfile_IsSecondaryAndEscoActive())
    {
        /* We infer that Audio is routed if we are on the secondary and have ESCO active */
        is_available = TRUE;
    }
    return is_available;
}

static unsigned mirrorProfile_GetCurrentContext(voice_source_t source)
{
    voice_source_provider_context_t context = context_voice_disconnected;

    if (mirrorProfile_IsVoiceAvailable(source))
    {
        context = context_voice_in_call;
    }
    return context;
}

const voice_source_audio_interface_t * MirrorProfile_GetVoiceInterface(void)
{
    return &mirror_voice_interface;
}

const voice_source_telephony_control_interface_t * MirrorProfile_GetTelephonyControlInterface(void)
{
    return &mirror_telephony_interface;
}

void MirrorProfile_StartScoAudio(void)
{
    voice_source_t voice_source = MirrorProfile_GetScoState()->voice_source;
    Telephony_NotifyCallAudioConnected(voice_source);
}

void MirrorProfile_StopScoAudio(void)
{
    voice_source_t voice_source = MirrorProfile_GetScoState()->voice_source;
    Telephony_NotifyCallAudioDisconnected(voice_source);
}

#endif /* INCLUDE_MIRRORING */


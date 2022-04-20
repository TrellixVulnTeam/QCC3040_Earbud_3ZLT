/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      The voice source audio interface implementation for aghfp voice sources
*/

#include "aghfp_profile_audio.h"
#include "aghfp_profile.h"
#include "aghfp_profile_sm.h"
#include "aghfp_profile_instance.h"

#include "voice_sources.h"
#include "kymera_adaptation_voice_protected.h"
#include "macros.h"
#include "volume_system.h"
#include "logging.h"
#include "panic.h"


#include <stdlib.h>

static bool aghfpProfile_GetConnectParameters(voice_source_t source, source_defined_params_t * source_params);
static void aghfpProfile_FreeConnectParameters(voice_source_t source, source_defined_params_t * source_params);
static bool aghfpProfile_GetDisconnectParameters(voice_source_t source, source_defined_params_t * source_params);
static void aghfpProfile_FreeDisconnectParameters(voice_source_t source, source_defined_params_t * source_params);
static bool aghfpProfile_IsAudioRouted(voice_source_t source);
static bool aghfpProfile_IsVoiceChannelAvailable(voice_source_t source);
static source_status_t aghfpProfile_SetState(voice_source_t source, source_state_t state);

static const voice_source_audio_interface_t aghfp_audio_interface =
{
    .GetConnectParameters = aghfpProfile_GetConnectParameters,
    .ReleaseConnectParameters = aghfpProfile_FreeConnectParameters,
    .GetDisconnectParameters = aghfpProfile_GetDisconnectParameters,
    .ReleaseDisconnectParameters = aghfpProfile_FreeDisconnectParameters,
    .IsAudioRouted = aghfpProfile_IsAudioRouted,
    .IsVoiceChannelAvailable = aghfpProfile_IsVoiceChannelAvailable,
    .SetState = aghfpProfile_SetState
};

static hfp_codec_mode_t aghfpProfile_GetCodecMode(aghfpInstanceTaskData * instance)
{
    hfp_codec_mode_t codec_mode = (instance->using_wbs) ?
                                   hfp_codec_mode_wideband : hfp_codec_mode_narrowband;


    if(instance->qce_codec_mode_id != CODEC_MODE_ID_UNSUPPORTED)
    {
        switch (instance->qce_codec_mode_id)
        {
            case aptx_adaptive_64_2_EV3:
            case aptx_adaptive_64_2_EV3_QHS3:
            case aptx_adaptive_64_QHS3:
                codec_mode = hfp_codec_mode_super_wideband;
                break;

            case aptx_adaptive_128_QHS3:
                codec_mode = hfp_codec_mode_ultra_wideband;
                break;

            default:
                Panic();
                break;
        }
    }

    return codec_mode;
}

static bool aghfpProfile_GetConnectParameters(voice_source_t source, source_defined_params_t * source_params)
{
    aghfpInstanceTaskData * instance = AghfpProfileInstance_GetInstanceForSource(source);

    PanicNull(instance);
    PanicNull(source_params);

    voice_connect_parameters_t * voice_connect_params = (voice_connect_parameters_t *)PanicNull(malloc(sizeof(voice_connect_parameters_t)));
    memset(voice_connect_params, 0, sizeof(voice_connect_parameters_t));

    voice_connect_params->audio_sink = instance->sco_sink;
    voice_connect_params->codec_mode = aghfpProfile_GetCodecMode(instance);
    voice_connect_params->wesco = instance->wesco;
    voice_connect_params->tesco = instance->tesco;
    voice_connect_params->volume = Volume_CalculateOutputVolume(VoiceSources_GetVolume(source), unmute);
    voice_connect_params->pre_start_delay = 0;

    source_params->data = (void *)voice_connect_params;
    source_params->data_length = sizeof(voice_connect_parameters_t);

    return TRUE;
}

static void aghfpProfile_FreeConnectParameters(voice_source_t source, source_defined_params_t * source_params)
{
    PanicNull(source_params);
    PanicFalse(source_params->data_length == sizeof(voice_connect_parameters_t));
    if(source_params->data_length)
    {
        free(source_params->data);
        source_params->data = (void *)NULL;
        source_params->data_length = 0;
    }
    UNUSED(source);
}

static bool aghfpProfile_GetDisconnectParameters(voice_source_t source, source_defined_params_t * source_params)
{
    PanicNull(source_params);
    source_params->data = (void *)NULL;
    source_params->data_length = 0;

    UNUSED(source);
    return TRUE;
}

static void aghfpProfile_FreeDisconnectParameters(voice_source_t source, source_defined_params_t * source_params)
{
    PanicNull(source_params);
    UNUSED(source);
    source_params->data = (void *)NULL;
    source_params->data_length = 0;
}

static bool aghfpProfile_IsAudioRouted(voice_source_t source)
{
    bool is_routed = FALSE;

    aghfpInstanceTaskData * instance = AghfpProfileInstance_GetInstanceForSource(source);

    if(instance && (instance->source_state == source_state_connected))
    {
        is_routed = TRUE;
    }

    DEBUG_LOG_VERBOSE("agHfpProfile_IsAudioRouted source enum:voice_source_t:%d, routed=%d", source, is_routed);

    return is_routed;
}

static bool aghfpProfile_IsVoiceChannelAvailable(voice_source_t source)
{
    bool is_available = FALSE;

    aghfpInstanceTaskData * instance = AghfpProfileInstance_GetInstanceForSource(source);

    if (instance && (AghfpProfile_IsScoActiveForInstance(instance)))
    {
        is_available = TRUE;
    }

    DEBUG_LOG_VERBOSE("aghfpProfile_IsVoiceChannelAvailable source enum:voice_source_t:%d, available=%d", source, is_available);

    return is_available;
}

static void aghfpProfile_TerminateUnroutedCall(aghfpInstanceTaskData* instance, voice_source_t source)
{
    VoiceSources_TransferOngoingCallAudio(source, voice_source_audio_transfer_to_ag);

    switch(AghfpProfile_GetState(instance))
    {
        case AGHFP_STATE_CONNECTED_ACTIVE:
        case AGHFP_STATE_CONNECTED_OUTGOING:
            VoiceSources_TerminateOngoingCall(source);
            break;

        default:
            break;
    }
}

static source_status_t aghfpProfile_SetState(voice_source_t source, source_state_t state)
{
    aghfpInstanceTaskData* instance = AghfpProfileInstance_GetInstanceForSource(source);

    if(instance)
    {
        source_state_t old_state = instance->source_state;
        instance->source_state = state;

        DEBUG_LOG_FN_ENTRY("aghfpProfile_SetState(%p) source enum:voice_source_t:%d, state from enum:source_state_t:%d to enum:source_state_t:%d, hfp_state enum:hfpState:%d",
                           instance, source, old_state, state, AghfpProfile_GetState(instance));

        switch(state)
        {
            case source_state_disconnecting:
                if(old_state == source_state_connected && AghfpProfile_IsScoActiveForInstance(instance))
                {
                    aghfpProfile_TerminateUnroutedCall(instance, source);
                }
                break;

            default:
                break;
        }
    }
    else
    {
        DEBUG_LOG_INFO("aghfpProfile_SetState no hfp instance found for source  enum:voice_source_t:%d", source);
    }

    return source_status_ready;
}

const voice_source_audio_interface_t * AghfpProfile_GetAudioInterface(void)
{
    return &aghfp_audio_interface;
}

void AghfpProfile_StoreConnectParams(const AGHFP_AUDIO_CONNECT_CFM_T *cfm)
{
    uint16 qce_codec_mode_id = cfm->qce_codec_mode_id;
    aghfpInstanceTaskData * instance = AghfpProfileInstance_GetInstanceForAghfp(cfm->aghfp);

    PanicNull(instance);

    instance->using_wbs = cfm->using_wbs;
    instance->codec = cfm->wbs_codec;
    instance->wesco = cfm->wesco;
    instance->tesco = cfm->tesco;
    instance->qce_codec_mode_id = qce_codec_mode_id;
}

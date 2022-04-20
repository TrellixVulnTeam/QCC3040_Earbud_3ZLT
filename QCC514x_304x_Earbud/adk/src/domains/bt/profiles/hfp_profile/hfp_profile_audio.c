/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      The audio interface implementation for hfp voice sources
*/

#include "hfp_profile_audio.h"
#include "hfp_profile_instance.h"
#include "hfp_profile_voice_source_link_prio_mapping.h"
#include "hfp_profile_sm.h"

#include "kymera_adaptation.h"
#include "kymera_adaptation_voice_protected.h"
#include "mirror_profile.h"
#include "source_param_types.h"
#include "state_proxy.h" // this shouldn't be here
#include "voice_sources.h"
#include "volume_system.h"

#include <stdlib.h>
#include <hfp.h>
#include <panic.h>

static bool hfpProfile_GetConnectParameters(voice_source_t source, source_defined_params_t * source_params);
static void hfpProfile_FreeConnectParameters(voice_source_t source, source_defined_params_t * source_params);
static bool hfpProfile_GetDisconnectParameters(voice_source_t source, source_defined_params_t * source_params);
static void hfpProfile_FreeDisconnectParameters(voice_source_t source, source_defined_params_t * source_params);
static bool hfpProfile_IsAudioRouted(voice_source_t source);
static bool hfpProfile_IsVoiceChannelAvailable(voice_source_t source);
static source_status_t hfpProfile_SetState(voice_source_t source, source_state_t state);

static const voice_source_audio_interface_t hfp_audio_interface =
{
    .GetConnectParameters = hfpProfile_GetConnectParameters,
    .ReleaseConnectParameters = hfpProfile_FreeConnectParameters,
    .GetDisconnectParameters = hfpProfile_GetDisconnectParameters,
    .ReleaseDisconnectParameters = hfpProfile_FreeDisconnectParameters,
    .IsAudioRouted = hfpProfile_IsAudioRouted,
    .IsVoiceChannelAvailable = hfpProfile_IsVoiceChannelAvailable,
    .SetState = hfpProfile_SetState
};

static hfp_codec_mode_t hfpProfile_GetCodecMode(hfpInstanceTaskData * instance)
{ 
    hfp_codec_mode_t codec_mode = (instance->codec == hfp_wbs_codec_mask_msbc) ?
                                   hfp_codec_mode_wideband : hfp_codec_mode_narrowband;

#ifdef INCLUDE_SWB
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
#endif
    return codec_mode;
}

static uint8 hfpProfile_GetPreStartDelay(void)
{
    uint8 pre_start_delay = 0;
    return pre_start_delay;
}

static bool hfpProfile_GetConnectParameters(voice_source_t source, source_defined_params_t * source_params)
{
    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForSource(source);

    if (instance == NULL)
    {
        DEBUG_LOG_ERROR("hfpProfile_GetConnectParameters source enum:voice_source_t:%d has no instance", source);
        return FALSE;
    }

    PanicNull(source_params);

    voice_connect_parameters_t * voice_connect_params = (voice_connect_parameters_t *)PanicNull(malloc(sizeof(voice_connect_parameters_t)));
    memset(voice_connect_params, 0, sizeof(voice_connect_parameters_t));

    voice_connect_params->audio_sink = instance->sco_sink;
    voice_connect_params->codec_mode = hfpProfile_GetCodecMode(instance);
    voice_connect_params->wesco = instance->wesco;
    voice_connect_params->tesco = instance->tesco;
    voice_connect_params->volume = VoiceSources_CalculateOutputVolume(source);
    voice_connect_params->pre_start_delay = hfpProfile_GetPreStartDelay();
    voice_connect_params->synchronised_start = MirrorProfile_ShouldEscoAudioStartSynchronously(source);

    source_params->data = (void *)voice_connect_params;
    source_params->data_length = sizeof(voice_connect_parameters_t);

    return TRUE;
}

static void hfpProfile_FreeConnectParameters(voice_source_t source, source_defined_params_t * source_params)
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

static bool hfpProfile_GetDisconnectParameters(voice_source_t source, source_defined_params_t * source_params)
{
    PanicNull(source_params);
    source_params->data = (void *)NULL;
    source_params->data_length = 0;

    UNUSED(source);
    return TRUE;
}

static void hfpProfile_FreeDisconnectParameters(voice_source_t source, source_defined_params_t * source_params)
{
    PanicNull(source_params);
    UNUSED(source);
    source_params->data = (void *)NULL;
    source_params->data_length = 0;
}

static bool hfpProfile_IsAudioRouted(voice_source_t source)
{
    bool is_routed = FALSE;

    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForSource(source);

    if(instance && (instance->source_state == source_state_connected))
    {
        is_routed = TRUE;
    }

    DEBUG_LOG_VERBOSE("hfpProfile_IsAudioRouted source enum:voice_source_t:%d, routed=%d", source, is_routed);

    return is_routed;
}

static bool hfpProfile_IsVoiceChannelAvailable(voice_source_t source)
{
    bool is_available = FALSE;

    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForSource(source);

    if (instance && (HfpProfile_IsScoActiveForInstance(instance) || instance->bitfields.esco_connecting))
    {
        is_available = TRUE;
    }

    DEBUG_LOG_VERBOSE("hfpProfile_IsVoiceChannelAvailable source enum:voice_source_t:%d, available=%d", source, is_available);

    return is_available;
}

static void hfpProfile_TerminateUnroutedCall(hfpInstanceTaskData* instance, voice_source_t source)
{
    VoiceSources_TransferOngoingCallAudio(source, voice_source_audio_transfer_to_ag);
    
    switch(appHfpGetState(instance))
    {
        case HFP_STATE_CONNECTED_ACTIVE:
        case HFP_STATE_CONNECTED_OUTGOING:
            VoiceSources_TerminateOngoingCall(source);
            break;
            
        default:
            break;
    }
}

static source_status_t hfpProfile_SetState(voice_source_t source, source_state_t state)
{
    hfpInstanceTaskData* instance = HfpProfileInstance_GetInstanceForSource(source);

    if(instance)
    {
        source_state_t old_state = instance->source_state;
        instance->source_state = state;

        DEBUG_LOG_FN_ENTRY("hfpProfile_SetState(%p) source enum:voice_source_t:%d, state from enum:source_state_t:%d to enum:source_state_t:%d, hfp_state enum:hfpState:%d",
                           instance, source, old_state, state, appHfpGetState(instance));

        switch(state)
        {
            case source_state_disconnected:
                /* Do not terminate unrouted call if audio has been, or is being, transferred to the AG */
                if(HfpProfile_IsScoActiveForInstance(instance) && !HfpProfile_IsScoDisconnectingForInstance(instance))
                {
                    hfpProfile_TerminateUnroutedCall(instance, source);
                }
                break;

            case source_state_connecting:
                if(HfpProfile_IsScoConnectingForInstance(instance))
                {
                    return source_status_preparing;
                }
                break;

            default:
                break;
        }
    }
    else
    {
        DEBUG_LOG_INFO("hfpProfile_SetState no hfp instance found for source  enum:voice_source_t:%d", source);
    }

    return source_status_ready;
}

const voice_source_audio_interface_t * HfpProfile_GetAudioInterface(void)
{
    return &hfp_audio_interface;
}

void HfpProfile_StoreConnectParams(hfpInstanceTaskData * instance, uint8 codec, uint8 wesco, uint8 tesco, uint16 qce_codec_mode_id)
{
    PanicNull(instance);

    instance->codec = codec;
    instance->wesco = wesco;
    instance->tesco = tesco;

#ifdef INCLUDE_SWB
    instance->qce_codec_mode_id = qce_codec_mode_id;
#else
    UNUSED(qce_codec_mode_id);
#endif
}




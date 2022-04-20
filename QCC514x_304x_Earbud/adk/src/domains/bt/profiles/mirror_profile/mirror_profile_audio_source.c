/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Mirror profile audio source control.
*/

#ifdef INCLUDE_MIRRORING

#include "mirror_profile_private.h"
#include "mirror_profile_audio_source.h"

#include "kymera_adaptation_audio_protected.h"
#include "kymera_adaptation.h"
#include "volume_system.h"
#include "logging.h"
#include "av.h"

#include <panic.h>
#include <stream.h>
#include <stdlib.h>
#include <sink.h>

static bool mirrorProfile_GetConnectParameters(audio_source_t source, source_defined_params_t *source_params);
static void mirrorProfile_FreeParameters(audio_source_t source, source_defined_params_t *source_params);
static bool mirrorProfile_GetDisconnectParameters(audio_source_t source, source_defined_params_t *source_params);
static bool mirrorProfile_IsAudioRouted(audio_source_t source);
source_status_t mirrorProfile_AudioSourceSetState(voice_source_t source, source_state_t state);

static const audio_source_audio_interface_t mirror_audio_interface =
{
    .GetConnectParameters = mirrorProfile_GetConnectParameters,
    .ReleaseConnectParameters = mirrorProfile_FreeParameters,
    .GetDisconnectParameters = mirrorProfile_GetDisconnectParameters,
    .ReleaseDisconnectParameters = mirrorProfile_FreeParameters,
    .IsAudioRouted = mirrorProfile_IsAudioRouted,
    .SetState = mirrorProfile_AudioSourceSetState,
};

static unsigned mirrorProfile_GetContext(audio_source_t source);

static const media_control_interface_t mirror_media_control_interface = {
    .Context = mirrorProfile_GetContext,
};

source_status_t mirrorProfile_AudioSourceSetState(audio_source_t source, source_state_t state)
{
    UNUSED(source);
    DEBUG_LOG("mirrorProfile_AudioSourceSetState enum:audio_source_t:%d enum:source_state_t:%d", source, state);
    switch (state)
    {
        case source_state_connected:
            MirrorProfile_StartA2dpAudioSynchronisation();
            break;
        default:
            break;
    }
    return source_status_ready;
}

static bool mirrorProfile_GetConnectParameters(audio_source_t source, source_defined_params_t *source_params)
{
    a2dp_connect_parameters_t *params = PanicUnlessNew(a2dp_connect_parameters_t);
    mirror_profile_a2dp_t *a2dp_state = MirrorProfile_GetA2dpState();

    memset(params, 0, sizeof(*params));
    params->client_lock = MirrorProfile_GetA2dpStartLockAddr();
    params->client_lock_mask = MIRROR_PROFILE_AUDIO_START_LOCK;
    params->volume = AudioSources_GetVolume(source);
    params->rate = a2dp_state->sample_rate;
    //params->channel_mode = 0;
    params->seid = a2dp_state->seid;
    params->sink = StreamL2capSink(a2dp_state->cid);
    params->content_protection = a2dp_state->content_protection;
    //params->bitpool = 0;
    //params->format = 0;
    params->packet_size = a2dp_state->mtu;
    params->q2q_mode = a2dp_state->q2q_mode;
    params->aptx_features = a2dp_state->aptx_features;

    source_params->data = (void *)params;
    source_params->data_length = sizeof(*params);

    return TRUE;
}

static bool mirrorProfile_GetDisconnectParameters(audio_source_t source, source_defined_params_t *source_params)
{
    mirror_profile_a2dp_t *a2dp_state = MirrorProfile_GetA2dpState();
    a2dp_disconnect_parameters_t *params = PanicUnlessNew(a2dp_disconnect_parameters_t);

    memset(params, 0, sizeof(*params));
    params->source = StreamL2capSource(a2dp_state->cid);
    params->seid = a2dp_state->seid;

    source_params->data = (void *)params;
    source_params->data_length = sizeof(*params);

    UNUSED(source);

    return TRUE;
}

static void mirrorProfile_FreeParameters(audio_source_t source, source_defined_params_t *source_params)
{
    UNUSED(source);

    free(source_params->data);
    source_params->data = NULL;
    source_params->data_length = 0;
}

static bool mirrorProfile_IsAudioRouted(audio_source_t source)
{
    bool is_available = FALSE;
    audio_source_t audio_source = MirrorProfile_GetA2dpState()->audio_source;

    if(audio_source == source && MirrorProfile_IsSecondary())
    {
        switch (MirrorProfile_GetState())
        {
            case MIRROR_PROFILE_STATE_A2DP_CONNECTING:
            case MIRROR_PROFILE_STATE_A2DP_CONNECTED:
            case MIRROR_PROFILE_STATE_A2DP_ROUTED:
                is_available = TRUE;
            break;
            default:
            break;
        }
    }
    return is_available;
}

static bool mirrorProfile_StoreAudioConnectParameters(audio_source_t source, const a2dp_connect_parameters_t *params)
{
    mirror_profile_a2dp_t *a2dp = MirrorProfile_GetA2dpState();
    PanicNull((void*)params);

    a2dp->cid = SinkGetL2capCid(params->sink);

    if (a2dp->cid)
    {
        a2dp->audio_source = source;
        a2dp->mtu = params->packet_size;
        a2dp->seid = params->seid;
        a2dp->sample_rate = params->rate;
        a2dp->content_protection = params->content_protection;
        a2dp->q2q_mode = params->q2q_mode;
        a2dp->aptx_features = params->aptx_features;
        Source src = StreamSourceFromSink(params->sink);
        SourceConfigure(src, STREAM_SOURCE_HANDOVER_POLICY, 0x1);
        DEBUG_LOG("mirrorProfile_StoreAudioConnectParameters sink:0x%x cid:0x%x mtu:%d seid:%d rate:%d cp:%d q2q:%d aptx features:0x%x",
        params->sink, a2dp->cid, a2dp->mtu, a2dp->seid, a2dp->sample_rate, a2dp->content_protection, a2dp->q2q_mode, a2dp->aptx_features);
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

bool MirrorProfile_StoreAudioSourceParameters(audio_source_t source)
{
    source_defined_params_t source_params = {0, NULL};
    a2dp_connect_parameters_t *audio_params;
    bool parameters_valid = FALSE;

    if(AudioSources_GetConnectParameters(source, &source_params))
    {
        audio_params = source_params.data;
        if (mirrorProfile_StoreAudioConnectParameters(source, audio_params))
        {
            parameters_valid = TRUE;
        }
        else
        {
            DEBUG_LOG("MirrorProfile_StoreAudioSourceParameters could not store connect parameters");
        }
        AudioSources_ReleaseConnectParameters(source, &source_params);
    }
    else
    {
        DEBUG_LOG("MirrorProfile_StoreAudioSourceParameters connect_params not valid");
    }

    return parameters_valid;
}

static void mirrorProfile_SendAvStatusMessage(bool connected)
{
    MessageId id = connected ? AV_A2DP_AUDIO_CONNECTED : AV_A2DP_AUDIO_DISCONNECTED;
    MESSAGE_MAKE(message, AV_A2DP_AUDIO_CONNECT_MESSAGE_T);
    message->audio_source = MirrorProfile_GetA2dpState()->audio_source;
    appAvSendStatusMessage(id, (void *)message, sizeof(AV_A2DP_AUDIO_CONNECT_MESSAGE_T));
}

void MirrorProfile_StartA2dpAudio(void)
{
    mirrorProfile_SendAvStatusMessage(TRUE);
}

void MirrorProfile_StopA2dpAudio(void)
{
    mirrorProfile_SendAvStatusMessage(FALSE);
}

void MirrorProfile_StartA2dpAudioSynchronisation(void)
{
    a2dp_connect_parameters_t params;
    audio_source_t audio_source = MirrorProfile_GetA2dpState()->audio_source;
    connect_parameters_t connect_params =
        {.source = {.type = source_type_audio, .u = {.audio = audio_source}},
         .source_params = {sizeof(params), &params}};

    memset(&params, 0, sizeof(params));
    /* Use any source SEID, to trigger the kymera start forwarding function.
       Leave other fields as zero as they are not used in this mode */
    params.seid = AV_SEID_SBC_MONO_TWS_SRC;
    params.sink = MirrorProfile_GetAudioSyncL2capState()->link_sink;
    params.client_lock = MirrorProfile_GetA2dpStartLockAddr();
    params.client_lock_mask = MIRROR_PROFILE_AUDIO_START_LOCK;

    KymeraAdaptation_Connect(&connect_params);
}

void MirrorProfile_StopA2dpAudioSynchronisation(void)
{
    a2dp_disconnect_parameters_t params;
    audio_source_t audio_source = MirrorProfile_GetA2dpState()->audio_source;
    disconnect_parameters_t disconnect_params = {
        .source = {.type = source_type_audio, .u = {.audio = audio_source}},
        .source_params = {sizeof(params), &params}};

    memset(&params, 0, sizeof(params));
    /* Use any source SEID, to trigger the kymera start forwarding function.
       Leave other fields as zero as they are not used in this mode */
    params.seid = AV_SEID_SBC_MONO_TWS_SRC;
    params.source = MirrorProfile_GetAudioSyncL2capState()->link_source;

    KymeraAdaptation_Disconnect(&disconnect_params);
}

static unsigned mirrorProfile_GetContext(audio_source_t source)
{
    audio_source_provider_context_t context = context_audio_disconnected;

    if (mirrorProfile_IsAudioRouted(source))
    {
        context = context_audio_is_playing;
    }

    return (unsigned)context;
}

const audio_source_audio_interface_t * MirrorProfile_GetAudioInterface(void)
{
    return &mirror_audio_interface;
}

const media_control_interface_t * MirrorProfile_GetMediaControlInterface(void)
{
    return &mirror_media_control_interface;
}

#endif /* INCLUDE_MIRRORING */

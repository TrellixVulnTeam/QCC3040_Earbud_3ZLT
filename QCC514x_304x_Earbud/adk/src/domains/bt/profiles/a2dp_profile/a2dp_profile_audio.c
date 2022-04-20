/*!
\copyright  Copyright (c) 2019-2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief
*/

#include "a2dp_profile_audio.h"
#include "a2dp_profile_caps.h"
#include "a2dp_profile.h"
#include "audio_sources.h"
#include "av.h"
#include "av_instance.h"
#include "bt_device.h"
#include "kymera_adaptation.h"
#include "kymera_adaptation_audio_protected.h"
#include "source_param_types.h"
#include "volume_system.h"

#include <a2dp.h>
#include <device.h>
#include <device_list.h>
#include <device_properties.h>
#include <logging.h>
#include <panic.h>
#include <stdlib.h>
#include <stream.h>

#ifdef INCLUDE_MIRRORING
/* Not applicable to Mirroring */
#define MESSAGES_FROM_SYNC_IND_TO_A2DP_START_REQ 0
#else
/*! \brief The number of message sent from requesting the A2DP source instance
           resume, to the A2DP library flushing data to the A2DP signalling sink
           requesting a media start. Used to delay the startup of kymera until
           after the A2DP media start request is flushed, avoid that being blocked by
           the long kymera start functions.
@startuml
participant "Sink AV Inst" as sink
participant "Source AV Inst" as source
participant "A2DP library" as a2dp

sink-->source:AV_INTERNAL_A2DP_INST_SYNC_IND\n(reason=A2DP_INST_SYNC_REASON_MEDIA_STARTING)
source-->source:AV_INTERNAL_A2DP_RESUME_MEDIA_REQ
source-->source:AV_INTERNAL_A2DP_INST_SYNC_RES
source-->a2dp:A2dpMediaStartRequest()
a2dp-->a2dp:A2DP_INTERNAL_MEDIA_START_REQ
@enduml
*/
#define MESSAGES_FROM_SYNC_IND_TO_A2DP_START_REQ 4
#endif

static bool a2dpProfile_GetHandsetSourceAudioConnectParameters(audio_source_t source, source_defined_params_t * source_params);
static bool a2dpProfile_GetPeerSourceAudioConnectParameters(audio_source_t source, source_defined_params_t * source_params);
static void a2dpProfile_FreeAudioConnectParameters(audio_source_t source, source_defined_params_t * source_params);
static bool a2dpProfile_GetHandsetSourceAudioDisconnectParameters(audio_source_t source, source_defined_params_t * source_params);
static bool a2dpProfile_GetPeerSourceAudioDisconnectParameters(audio_source_t source, source_defined_params_t * source_params);
static void a2dpProfile_FreeAudioDisconnectParameters(audio_source_t source, source_defined_params_t * source_params);
static bool a2dpProfile_IsAudioRouted(audio_source_t source);
static source_status_t a2dpProfile_SetState(audio_source_t source, source_state_t state);

static const audio_source_audio_interface_t handset_source_audio_interface =
{
    .GetConnectParameters = a2dpProfile_GetHandsetSourceAudioConnectParameters,
    .ReleaseConnectParameters = a2dpProfile_FreeAudioConnectParameters,
    .GetDisconnectParameters = a2dpProfile_GetHandsetSourceAudioDisconnectParameters,
    .ReleaseDisconnectParameters = a2dpProfile_FreeAudioDisconnectParameters,
    .IsAudioRouted = a2dpProfile_IsAudioRouted,
    .SetState = a2dpProfile_SetState
};

static const audio_source_audio_interface_t peer_source_audio_interface =
{
    .GetConnectParameters = a2dpProfile_GetPeerSourceAudioConnectParameters,
    .ReleaseConnectParameters = a2dpProfile_FreeAudioConnectParameters,
    .GetDisconnectParameters = a2dpProfile_GetPeerSourceAudioDisconnectParameters,
    .ReleaseDisconnectParameters = a2dpProfile_FreeAudioDisconnectParameters,
    .IsAudioRouted = a2dpProfile_IsAudioRouted,
    .SetState = NULL
};


static a2dpTaskData * A2dpProfile_GetHandsetData(audio_source_t audio_source)
{
    avInstanceTaskData* av_instance = Av_GetInstanceForHandsetSource(audio_source);

    return av_instance ? &av_instance->a2dp : NULL;
}

static uint32 a2dpProfile_GetMaxBitrate(const a2dp_codec_settings *codec_settings)
{
    /* When bitrate is zero, the argument is ignored by the underlying code */
    uint32 max_bitrate = 0;

    if (codec_settings->seid == AV_SEID_AAC_SNK)
    {
        max_bitrate = AAC_BITRATE;
    }

    return max_bitrate;
}

static bool a2dpProfile_GetConnectParameters(audio_source_t source, a2dpTaskData * a2dp, source_defined_params_t * source_params)
{
    bool populate_success = FALSE;
    PanicNull(source_params);
    if (a2dp == NULL)
    {
        return FALSE;
    }

    DEBUG_LOG("a2dpProfile_GetConnectParameters source=%d a2dp=%p, device_id=%d, stream_id=%d",
                                source, (void *)a2dp, a2dp->device_id, a2dp->stream_id);

    a2dp_codec_settings *codec_settings = A2dpCodecGetSettings(a2dp->device_id, a2dp->stream_id);
    if(codec_settings)
    {
        a2dp_connect_parameters_t * audio_connect_params = (a2dp_connect_parameters_t *)PanicNull(malloc(sizeof(a2dp_connect_parameters_t)));
        memset(audio_connect_params, 0, sizeof(a2dp_connect_parameters_t));

        audio_connect_params->client_lock = &a2dp->lock;
        audio_connect_params->client_lock_mask = APP_A2DP_AUDIO_START_LOCK;
        audio_connect_params->volume = AudioSources_CalculateOutputVolume(source);
        audio_connect_params->master_pre_start_delay = (appA2dpIsSeidNonTwsSink(a2dp->current_seid) ? MESSAGES_FROM_SYNC_IND_TO_A2DP_START_REQ : 0);
        audio_connect_params->rate = codec_settings->rate;
        audio_connect_params->channel_mode = codec_settings->channel_mode;
        audio_connect_params->seid = codec_settings->seid;
        audio_connect_params->sink = codec_settings->sink;
        audio_connect_params->content_protection = codec_settings->codecData.content_protection;
        audio_connect_params->bitpool = codec_settings->codecData.bitpool;
        audio_connect_params->format = codec_settings->codecData.format;
        audio_connect_params->packet_size = codec_settings->codecData.packet_size;
        audio_connect_params->max_bitrate = a2dpProfile_GetMaxBitrate(codec_settings);
        audio_connect_params->q2q_mode = codec_settings->codecData.aptx_ad_params.q2q_enabled;
        audio_connect_params->nq2q_ttp = codec_settings->codecData.aptx_ad_params.nq2q_ttp;
        audio_connect_params->aptx_features = codec_settings->codecData.aptx_ad_params.features;
        free(codec_settings);

        a2dp->media_sink = A2dpMediaGetSink(a2dp->device_id, a2dp->stream_id);

        source_params->data = (void *)audio_connect_params;
        source_params->data_length = sizeof(a2dp_connect_parameters_t);
        populate_success = TRUE;
    }
    else
    {
        a2dp->lock &= ~APP_A2DP_AUDIO_START_LOCK;
    }
    return populate_success;
}

static void a2dpProfile_FreeConnectParameters(source_defined_params_t * source_params)
{
    PanicNull(source_params);
    PanicFalse(source_params->data_length == sizeof(a2dp_connect_parameters_t));
    if(source_params->data_length)
    {
        free(source_params->data);
        source_params->data = (void *)NULL;
        source_params->data_length = 0;
    }
}

static bool a2dpProfile_GetDisconnectParameters(a2dpTaskData * a2dp, source_defined_params_t * source_params)
{
    a2dp_disconnect_parameters_t * audio_disconnect_params;
    Sink media_sink;
    PanicNull(source_params);
    PanicNull(a2dp);

    audio_disconnect_params = (a2dp_disconnect_parameters_t *)PanicNull(malloc(sizeof(a2dp_disconnect_parameters_t)));
    media_sink = a2dp->media_sink;
    audio_disconnect_params->source = StreamSourceFromSink(media_sink);
    audio_disconnect_params->seid = a2dp->last_known_seid;

    DEBUG_LOG("a2dpProfile_GetDisconnectParameters audio_disconnect_params->seid=%d, source=%d",
            audio_disconnect_params->seid, audio_disconnect_params->source);
    source_params->data = (void *)audio_disconnect_params;
    source_params->data_length = sizeof(a2dp_disconnect_parameters_t);

    return TRUE;
}

static void a2dpProfile_FreeDisconnectParameters(source_defined_params_t * source_params)
{
    PanicNull(source_params);
    PanicFalse(source_params->data_length == sizeof(a2dp_disconnect_parameters_t));
    if(source_params->data_length)
    {
        free(source_params->data);
        source_params->data = (void *)NULL;
        source_params->data_length = 0;
    }
}

static bool a2dpProfile_GetHandsetSourceAudioConnectParameters(audio_source_t source, source_defined_params_t * source_params)
{
    a2dpTaskData * a2dp = A2dpProfile_GetHandsetData(source);

    DEBUG_LOG("a2dpProfile_GetHandsetSourceAudioConnectParameters source=%d, a2dp=%p", source, a2dp);

    return a2dpProfile_GetConnectParameters(source, a2dp, source_params);
}

static bool a2dpProfile_GetPeerSourceAudioConnectParameters(audio_source_t source, source_defined_params_t * source_params)
{
    a2dpTaskData * a2dp = A2dpProfile_GetPeerData();

    DEBUG_LOG("a2dpProfile_GetPeerSourceAudioConnectParameters source=%d, a2dp=%p", source, a2dp);

    return a2dpProfile_GetConnectParameters(source, a2dp, source_params);
}

static void a2dpProfile_FreeAudioConnectParameters(audio_source_t source, source_defined_params_t * source_params)
{
    DEBUG_LOG("a2dpProfile_FreeAudioConnectParameters source=%d", source);
    a2dpProfile_FreeConnectParameters(source_params);
    UNUSED(source);
}

static bool a2dpProfile_GetHandsetSourceAudioDisconnectParameters(audio_source_t source, source_defined_params_t * source_params)
{
    a2dpTaskData * a2dp = A2dpProfile_GetHandsetData(source);

    DEBUG_LOG("a2dpProfile_GetHandsetSourceAudioDisconnectParameters source=%d, a2dp=%p", source, a2dp);

    return a2dpProfile_GetDisconnectParameters(a2dp, source_params);
}

static bool a2dpProfile_GetPeerSourceAudioDisconnectParameters(audio_source_t source, source_defined_params_t * source_params)
{
    a2dpTaskData * a2dp = A2dpProfile_GetPeerData();

    DEBUG_LOG("a2dpProfile_GetPeerSourceAudioDisconnectParameters source=%d, a2dp=%p", source, a2dp);
    UNUSED(source);
    return a2dpProfile_GetDisconnectParameters(a2dp, source_params);
}

static void a2dpProfile_FreeAudioDisconnectParameters(audio_source_t source, source_defined_params_t * source_params)
{
    DEBUG_LOG("a2dpProfile_FreeAudioDisconnectParameters source=%d", source);
    a2dpTaskData * a2dp = A2dpProfile_GetHandsetData(source);
    if (a2dp)
    {
        DEBUG_LOG("a2dpProfile_FreeAudioDisconnectParameters %p %d", &a2dp->lock, a2dp->lock);
        a2dp->lock &= ~APP_A2DP_AUDIO_STOP_LOCK;
    }

    a2dpProfile_FreeDisconnectParameters(source_params);
}

static bool a2dpProfile_IsAudioRouted(audio_source_t source)
{
    bool is_audio_routed = FALSE;

    avInstanceTaskData *av_instance = Av_GetInstanceForHandsetSource(source);

    if(av_instance && (av_instance->a2dp.source_state == source_state_connected))
    {
        is_audio_routed = TRUE;
    }

    DEBUG_LOG("a2dpProfile_IsAudioRouted source enum:audio_source_t:%d, is_audio_routed=%d", source, is_audio_routed);

    return is_audio_routed;
}

const audio_source_audio_interface_t * A2dpProfile_GetHandsetSourceAudioInterface(void)
{
    return &handset_source_audio_interface;
}

const audio_source_audio_interface_t * A2dpProfile_GetPeerSourceAudioInterface(void)
{
    return &peer_source_audio_interface;
}

bool A2dpProfile_GetForwardingConnectParameters(source_defined_params_t * source_params)
{
    a2dpTaskData * a2dp = A2dpProfile_GetPeerData();

    DEBUG_LOG("A2dpProfile_GetForwardingConnectParameters a2dp=%p", a2dp);
    return a2dpProfile_GetConnectParameters(audio_source_a2dp_1, a2dp, source_params);
}

void A2dpProfile_FreeForwardingConnectParameters(source_defined_params_t * source_params)
{
    DEBUG_LOG("A2dpProfile_FreeForwardingConnectParameters");
    a2dpProfile_FreeConnectParameters(source_params);
}

bool A2dpProfile_GetForwardingDisconnectParameters(source_defined_params_t * source_params)
{
    a2dpTaskData * a2dp = A2dpProfile_GetPeerData();

    DEBUG_LOG("A2dpProfile_GetForwardingDisconnectParameters a2dp=%p", a2dp);
    return a2dpProfile_GetDisconnectParameters(a2dp, source_params);
}

void A2dpProfile_FreeForwardingDisconnectParameters(source_defined_params_t * source_params)
{
    DEBUG_LOG("A2dpProfile_FreeForwardingDisconnectParameters");
    a2dpProfile_FreeDisconnectParameters(source_params);
}

static source_status_t a2dpProfile_SetState(audio_source_t source, source_state_t state)
{
    avInstanceTaskData *av_instance = Av_GetInstanceForHandsetSource(source);

    DEBUG_LOG_FN_ENTRY("a2dpProfile_SetState source=%d, state=%d", source, state );

    if(av_instance)
    {
        return A2dpProfile_SetSourceState(av_instance, state);
    }
    return source_status_ready;
}

/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Kymera module to manage A2DP audio latency based on streaming performance
*/

#include "kymera_latency_manager.h"
#include "kymera_tones_prompts.h"
#include "kymera_dynamic_latency.h"
#include "kymera_a2dp.h"
#include "kymera_volume.h"
#include "kymera_common.h"
#include "kymera_data.h"
#include "kymera_state.h"
#include "kymera_internal_msg_ids.h"
#include "av.h"
#include "panic.h"

/*! Macro for creating messages */
#define MAKE_KYMERA_MESSAGE(TYPE) \
    TYPE##_T *message = PanicUnlessNew(TYPE##_T);

/*! \brief Convert a RTP codec type to a A2DP sink stream endpoint id.
    \param codec The RTP codec type.
    \return The A2DP sink SEID.
*/
static uint8 appKymeraCodecToSinkSeid(rtp_codec_type_t codec)
{
    switch (codec)
    {
        case rtp_codec_type_sbc:
            return AV_SEID_SBC_SNK;
        case rtp_codec_type_aac:
            return AV_SEID_AAC_SNK;
        case rtp_codec_type_aptx:
            return AV_SEID_APTX_SNK;
        case rtp_codec_type_aptx_hd:
            return AV_SEID_APTXHD_SNK;
        case rtp_codec_type_aptx_ad:
            return AV_SEID_APTX_ADAPTIVE_SNK;
        default:
            Panic();
            return 0xff;
    }
}

#ifdef INCLUDE_LATENCY_MANAGER
#include <message.h>
#include <panic.h>
#include <system_clock.h>
#include <audio_clock.h>
#include "power_manager.h"
#include "mirror_profile.h"
#include "kymera.h"
#include "av_typedef.h"

kymera_latency_manager_data_t latency_data;

static uint32 Kymera_LatencyManagerGetLatencyForSeid(uint8 seid);

static uint32 latencyManager_GetBaseLatency(void)
{
    KYMERA_INTERNAL_A2DP_START_T *params = KymeraGetLatencyData()->a2dp_start_params;
    uint32 latency = TWS_STANDARD_LATENCY_MS;
    if (params)
    {
        if (params->codec_settings.seid == AV_SEID_APTXHD_SNK)
        {
            latency = APTX_HD_LATENCY_MS;
        }
    }
    return latency;
}

static void kymera_LatencyManagerReconfigureComplete(void)
{
    DEBUG_LOG("kymera_LatencyManagerReconfigureComplete");
    Kymera_LatencyManagerClearAdjustingLatency();
}

static void kymera_LatencyManagerConfigureRtpStartup(uint8 seid)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    Operator op_rtp_decoder;

    if (GET_OP_FROM_CHAIN(op_rtp_decoder, theKymera->chain_input_handle, OPR_RTP_DECODER))
    {
        uint32 latency_ms = Kymera_LatencyManagerGetLatencyForSeid(seid);
        /* With a normal TWM sync startup, the RTP decoder startup period/correction
            has to be disabled to avoid a glitch on secondary if the primary makes
            a latency correction. When resuming from a muted latency adjustment,
            any glitches are masked and the startup correction may be applied.
        */
        Kymera_A2dpConfigureRtpDecoderStartupPeriod(op_rtp_decoder, latency_ms / 2);
    }
}

static void kymera_LatencyManagerMuteStream(void)
{
    kymera_latency_manager_data_t * data = KymeraGetLatencyData();
    KYMERA_INTERNAL_A2DP_START_T *params = KymeraGetLatencyData()->a2dp_start_params;

    PanicNull(params);

#ifdef INCLUDE_STEREO
    Kymera_A2dpCommonStop(KymeraGetTaskData()->media_source);
    PanicFalse(Kymera_A2dpStart(&params->codec_settings, params->max_bitrate, VOLUME_MUTE_IN_DB, params->nq2q_ttp));

    kymera_LatencyManagerConfigureRtpStartup(params->codec_settings.seid);
    data->current_latency = Kymera_LatencyManagerGetLatencyForSeid(params->codec_settings.seid);

#else 
    {
        kymeraTaskData *theKymera = KymeraGetTaskData();
        bool forwarding = (theKymera->state == KYMERA_STATE_A2DP_STREAMING_WITH_FORWARDING);

        if (forwarding)
        {
            Kymera_A2dpStopForwarding(MirrorProfile_GetA2dpAudioSyncTransportSource());
        }

        Kymera_A2dpCommonStop(KymeraGetTaskData()->media_source);
        PanicFalse(Kymera_A2dpStart(&params->codec_settings, params->max_bitrate, VOLUME_MUTE_IN_DB, params->nq2q_ttp));

        kymera_LatencyManagerConfigureRtpStartup(params->codec_settings.seid);
        data->current_latency = Kymera_LatencyManagerGetLatencyForSeid(params->codec_settings.seid);

        if (forwarding)
        {
            /* Mirroring forwarding only requires the forwarding sync */
            a2dp_codec_settings settings = {0};
            settings.sink = MirrorProfile_GetA2dpAudioSyncTransportSink();
            Kymera_A2dpStartForwarding(&settings);
        }
        appKymeraA2dpSetSyncStartTime(VmGetTimerTime());
        appKymeraA2dpHandleAudioSyncStreamInd(MESSAGE_SINK_AUDIO_SYNCHRONISED, NULL);
    }
#endif /* INCLUDE_STEREO */

}

void Kymera_LatencyManagerHandleToneEnd(void)
{
    uint16 seid = Kymera_GetCurrentSeid();
    KYMERA_INTERNAL_A2DP_START_T *params = KymeraGetLatencyData()->a2dp_start_params;

    if (!Kymera_LatencyManagerIsReconfigInProgress())
    {
        return;
    }

    DEBUG_LOG("Kymera_LatencyManagerHandleToneEnd");
    if ((seid == AV_SEID_INVALID) || !params || !SinkIsValid(params->codec_settings.sink))
    {
        /* Streaming is disconnected while we were trying to Mute Audio. */
        kymera_LatencyManagerReconfigureComplete();
    }
    else
    {        
        audio_dsp_clock_configuration cconfig =
        {
            .active_mode = AUDIO_DSP_TURBO_CLOCK,
            .low_power_mode =  AUDIO_DSP_SLOW_CLOCK,
            .trigger_mode = AUDIO_DSP_CLOCK_NO_CHANGE
        };

        /* Boost system clocks to reduce transition time */
        appPowerPerformanceProfileRequest();
        AudioDspClockConfigure(&cconfig);

        kymera_LatencyManagerMuteStream();

        MessageSendLater(KymeraGetTask(),
                        KYMERA_INTERNAL_LATENCY_MANAGER_MUTE_COMPLETE,
                        NULL,
                        Kymera_LatencyManagerConfigMuteDurationMs());

        appPowerPerformanceProfileRelinquish();
    }
}

void Kymera_LatencyManagerHandleMuteComplete(void)
{
    KYMERA_INTERNAL_A2DP_START_T *params = KymeraGetLatencyData()->a2dp_start_params;
    DEBUG_LOG("Kymera_LatencyManagerHandleMuteComplete");
    if(params)
    {
        Kymera_A2dpHandleInternalSetVolume(params->volume_in_db);
    }
    else
    {
        /* A2DP got disconnected while Mute was in progress. Hence the stored params
           have been deleted. Mark reconfiguration as complete. */
        DEBUG_LOG("Kymera_LatencyManagerHandleMuteComplete: a2dp_start_params are NULL!");
    }
    
    kymera_LatencyManagerReconfigureComplete();
}

/*! \brief If enabled, override the latency provided to the function with the
           override latency value */
static uint32 kymera_LatencyManagerOverrideLatency(uint32 latency_ms)
{
    kymera_latency_manager_data_t * data = KymeraGetLatencyData();
    return data->override_latency ? data->override_latency : latency_ms;
}

static void kymera_LatencyManagerApplyLatency(void)
{
    kymera_latency_manager_data_t * data = KymeraGetLatencyData();
    kymera_chain_handle_t chain_handle = KymeraGetTaskData()->chain_input_handle;

    Operator op = ChainGetOperatorByRole(chain_handle, OPR_RTP_DECODER);
    if(op != INVALID_OPERATOR)
    {
        uint32 latency = kymera_LatencyManagerOverrideLatency(data->current_latency);
        DEBUG_LOG("kymera_LatencyManagerApplyLatency %ums", latency);
        OperatorsStandardSetTimeToPlayLatency(op, US_PER_MS * latency);
    }
}

void Kymera_LatencyManagerAdjustLatency(uint16 latency_ms)
{
    Kymera_LatencyManagerSetLatency(latency_ms);
    kymera_LatencyManagerApplyLatency();
}

static void kymera_LatencyManagerStartDynamicAdjustment(void)
{
    kymera_latency_manager_data_t * data = KymeraGetLatencyData();
    if (!data->gaming_mode_enabled)
    {
        if (data->a2dp_start_params)
        {
            Kymera_DynamicLatencyStartDynamicAdjustment(Kymera_LatencyManagerGetLatency());
        }
    }
}

static uint32 Kymera_LatencyManagerGetLatencyForSeid(uint8 seid)
{
    kymera_latency_manager_data_t * data = KymeraGetLatencyData();
    uint32 latency;

    /* In gaming mode, the latency is fixed */
    if(data->gaming_mode_enabled)
    {
        switch(seid)
        {
            case AV_SEID_SBC_SNK:
                latency = GAMING_MODE_LATENCY_SBC_MS;
                break;
            case AV_SEID_AAC_SNK:
                latency = GAMING_MODE_LATENCY_AAC_MS;
                break;
            case AV_SEID_APTX_SNK:
                latency = GAMING_MODE_LATENCY_APTX_CLASSIC_MS;
                break;
            case AV_SEID_APTXHD_SNK:
                latency = GAMING_MODE_LATENCY_APTX_HD_MS;
                break;
            default:
                latency = TWS_STANDARD_LATENCY_MS;
                break;
        }
    }
    else
    {
        KYMERA_INTERNAL_A2DP_START_T *params = data->a2dp_start_params;
        if (params)
        {
            /* Current (dynamic) latency is only valid once a2dp params are known */
            latency = Kymera_LatencyManagerGetLatency();
        }
        else
        {
            switch (seid)
            {
                case AV_SEID_APTXHD_SNK:
                    latency = APTX_HD_LATENCY_MS;
                    break;
                default:
                    latency = TWS_STANDARD_LATENCY_MS;
                    break;
            }
        }
    }
    return kymera_LatencyManagerOverrideLatency(latency);
}

uint32 Kymera_LatencyManagerGetLatencyForSeidInUs(uint8 seid)
{
    uint32 latency = Kymera_LatencyManagerGetLatencyForSeid(seid) * US_PER_MS;
    DEBUG_LOG("Kymera_LatencyManagerGetLatencyForSeidInUs: Seid: %d, Latency: %dus", seid, latency);
    return latency;
}

uint32 Kymera_LatencyManagerGetLatencyForCodecInUs(rtp_codec_type_t codec)
{
    return Kymera_LatencyManagerGetLatencyForSeidInUs(appKymeraCodecToSinkSeid(codec));
}

static uint32 timestampToDelay(marshal_rtime_t timestamp)
{
    rtime_t now = SystemClockGetTimerTime();
    int32 timer_offset = rtime_sub(timestamp, now);
    uint32 delay_in_ms = rtime_gt(timer_offset, 0) ? timer_offset/US_PER_MS : D_IMMEDIATE;
    return delay_in_ms;
}

static void kymera_PlayLatencyChangeTone(const ringtone_note *tone, rtime_t tone_instant, uint32 rate)
{
    KYMERA_INTERNAL_TONE_PROMPT_PLAY_T tone_params = {0};
    PanicNull((void*)tone);
    tone_params.rate = rate;
    tone_params.time_to_play = tone_instant;
    tone_params.tone = tone;
    KymeraGetTaskData()->tone_count++;
    appKymeraHandleInternalTonePromptPlay(&tone_params);
}

void Kymera_LatencyManagerReconfigureLatency(Task clientTask, rtime_t mute_instant, const ringtone_note *tone)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOG("appKymeraLatencyReconfigure, lock %u, busy_lock %u", theKymera->lock, theKymera->busy_lock);

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_LATENCY_RECONFIGURE);
    message->clientTask = clientTask;
    message->mute_instant = mute_instant;
    message->tone = tone;
    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_LATENCY_RECONFIGURE, message, &theKymera->lock);
}

void Kymera_LatencyManagerHandleLatencyReconfigure(const KYMERA_INTERNAL_LATENCY_RECONFIGURE_T *msg)
{
    kymera_latency_manager_data_t *data = KymeraGetLatencyData();
    KYMERA_INTERNAL_A2DP_START_T *params = data->a2dp_start_params;
    rtime_t tone_instant;

    DEBUG_LOG("Kymera_LatencyManagerReconfigureLatency");

    if (NULL == params)
    {
        MessageSend(msg->clientTask, KYMERA_LATENCY_MANAGER_RECONFIG_FAILED_IND, NULL);
    }
    else
    {
        Kymera_LatencyManagerSetAdjustingLatency();

        MessageSendConditionally(msg->clientTask,
                    KYMERA_LATENCY_MANAGER_RECONFIG_COMPLETE_IND,
                    NULL, &data->adjusting_latency);

        /* Play the tone after the audio is muted */
        tone_instant = rtime_add(msg->mute_instant, Kymera_LatencyManagerConfigMuteTransitionPeriodMs());
        /* If the a2dp streaming rate is 96K, tone generation rate shall be 8K which brings in use of resampler to output at 96K rate
            Note: Currently it's seen that tone generator is not possible to output at 96K sample rate */
        uint32 tone_sample_rate = (params->codec_settings.rate == SAMPLE_RATE_96000)?KYMERA_TONE_GEN_RATE:params->codec_settings.rate;
        kymera_PlayLatencyChangeTone(msg->tone, tone_instant, tone_sample_rate);

        MessageSendLater(KymeraGetTask(), KYMERA_INTERNAL_LATENCY_MANAGER_MUTE,
                         NULL, timestampToDelay(msg->mute_instant));
    }
}

void Kymera_LatencyManagerHandleMute(void)
{
    kymera_latency_manager_data_t * data = KymeraGetLatencyData();
    /* Calling this function will overwrite our stored volume, so backup and
       restore the actual volume */
    int16 actual_volume =  data->a2dp_start_params->volume_in_db;
    Kymera_A2dpHandleInternalSetVolume(VOLUME_MUTE_IN_DB);
    data->a2dp_start_params->volume_in_db = actual_volume;
}


void Kymera_LatencyManagerInit(bool enable_gaming_mode, uint32 override_latency_ms)
{
    DEBUG_LOG("Kymera_LatencyManagerInit %d", enable_gaming_mode);
    kymera_latency_manager_data_t * data = KymeraGetLatencyData();
    memset(data, 0 , sizeof(kymera_latency_manager_data_t));
    /* current_latency is set when A2DP is started */
    data->gaming_mode_enabled = enable_gaming_mode;
    data->override_latency = override_latency_ms;
}

static void kymera_LatencyManagerFreeA2dpParams(void)
{
    kymera_latency_manager_data_t *data = KymeraGetLatencyData();
    if (data->a2dp_start_params)
    {
        free(data->a2dp_start_params);
        data->a2dp_start_params = NULL;
    }
}

/*! \brief Set the low latency stream state and notify to registered clients
 *
 * NOTE: Low Latency Stream is set to active in following use cases:
 * 1) A2DP streaming(Any sink codec) + Gaming mode enabled.
 * 2) A2DP LL streaming(Q2Q mode)
 *
 * \param new_state New low latency stream state to be set
 */
static void kymera_LatencyManagerSetLLStreamState(ll_stream_state_t new_state)
{
    kymera_latency_manager_data_t *data = KymeraGetLatencyData();

    /* check if there is change in LL stream state and store */
    if (data->ll_stream_state != new_state)
    {
        DEBUG_LOG("kymera_LatencyManagerSetLLStreamState: Transitioned from enum:ll_stream_state_t:old_state[%d] to enum:ll_stream_state_t:new_state[%d]", data->ll_stream_state, new_state);
        data->ll_stream_state = new_state;
        /* Notify to registered clients */
        if (KymeraGetTaskData()->client_tasks)
        {
            MESSAGE_MAKE(msg, KYMERA_LOW_LATENCY_STATE_CHANGED_IND_T);
            msg->state = data->ll_stream_state;
            TaskList_MessageSendWithSize(KymeraGetTaskData()->client_tasks, KYMERA_LOW_LATENCY_STATE_CHANGED_IND, msg, sizeof(KYMERA_LOW_LATENCY_STATE_CHANGED_IND_T));
        }
    }
}

/*! \brief Set the high bandwidth stream state and notify to registered clients
 *
 * NOTE: High Bandwidth Stream is set to active when A2DP streaming sample rate is 96K
 *
 * \param new_state New high bandwidth stream state to be set
 */
static void kymera_LatencyManagerSetHQStreamState(hbw_stream_state_t new_state)
{
    kymera_latency_manager_data_t *data = KymeraGetLatencyData();

    /* check if there is change in HQ stream state and store */
    if (data->hbw_stream_state != new_state)
    {
        DEBUG_LOG("kymera_LatencyManagerSetHQStreamState: Transitioned from enum:hbw_stream_state_t:old_state[%d] to enum:hbw_stream_state_t:new_state[%d]", data->hbw_stream_state, new_state);
        data->hbw_stream_state = new_state;
        /* Notify to registered clients */
        if (KymeraGetTaskData()->client_tasks)
        {
            MESSAGE_MAKE(msg, KYMERA_HIGH_BANDWIDTH_STATE_CHANGED_IND_T);
            msg->state = data->hbw_stream_state;
            TaskList_MessageSendWithSize(KymeraGetTaskData()->client_tasks, KYMERA_HIGH_BANDWIDTH_STATE_CHANGED_IND, msg, sizeof(KYMERA_HIGH_BANDWIDTH_STATE_CHANGED_IND_T));
        }
    }
}

/*! \brief Start periodic check for identifying low latency stream by reading
 *          stream id from SSRC field of RTP packet.
 *  NOTE: this check shall be triggered when Q2Q mode enabled
*/
static void kymera_LatencyManagerStartLLStreamCheck(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    Transform packetiser = NULL;
#ifdef INCLUDE_MIRRORING
    packetiser = theKymera->hashu.packetiser;
#elif INCLUDE_STEREO
    packetiser = theKymera->packetiser;
#endif

    if (packetiser)
    {
        uint32 ssrc_value;
        /* Read SSRC field value of RTP packet seen during a2dp streaming */
        if (TransformQuery(packetiser, VM_TRANSFORM_QUERY_PACKETISE_SSRC, &ssrc_value))
        {
            /* Set low latency stream active/inactive based on stream id of low latency
             *  encoded by Aptx Adaptive 2.1 */
            if (((ssrc_value & 0xFFFF) == aptxAdaptiveLowLatencyStreamId_SSRC_Q2Q()) ||
                ((ssrc_value & 0xFFFF) == aptxAdaptiveLowLatencyStreamId_SSRC_AOSP_LL_0()) ||
                ((ssrc_value & 0xFFFF) == aptxAdaptiveLowLatencyStreamId_SSRC_AOSP_LL_1()))
            {
                kymera_LatencyManagerSetLLStreamState(LOW_LATENCY_STREAM_ACTIVE);
            }
            /* Set inactive, only if gaming mode is disabled */
            else if (!Kymera_LatencyManagerIsGamingModeEnabled())
            {
                kymera_LatencyManagerSetLLStreamState(LOW_LATENCY_STREAM_INACTIVE);
            }
        }
        MessageCancelFirst(KymeraGetTask(), KYMERA_INTERNAL_LOW_LATENCY_STREAM_CHECK);
        MessageSendLater(KymeraGetTask(), KYMERA_INTERNAL_LOW_LATENCY_STREAM_CHECK, NULL, Kymera_LatencyManagerConfigLLStreamCheckIntervalMs());

    }
}

/*! \brief Stop periodic check for identifying low latency stream */
static void kymera_LatencyManagerStopLLStreamCheck(void)
{
    MessageCancelFirst(KymeraGetTask(), KYMERA_INTERNAL_LOW_LATENCY_STREAM_CHECK);
    kymera_LatencyManagerSetLLStreamState(LOW_LATENCY_STREAM_INACTIVE);
}

void Kymera_LatencyManagerHandleLLStreamCheck(void)
{
    kymera_LatencyManagerStartLLStreamCheck();
}

void Kymera_LatencyManagerA2dpStart(const KYMERA_INTERNAL_A2DP_START_T* a2dp_start_params)
{
    /* The start params are stored for when the chain is reconfigured when
       entering/exiting gaming mode */
    kymera_latency_manager_data_t * data = KymeraGetLatencyData();
    kymera_LatencyManagerFreeA2dpParams();
    /* Initialise current_latency before setting the start params - this returns
       the initial static per-codec latency */
    data->current_latency = Kymera_LatencyManagerGetLatencyForSeid(a2dp_start_params->codec_settings.seid);
    data->a2dp_start_params = PanicUnlessMalloc(sizeof(*a2dp_start_params));
    *(data->a2dp_start_params) = *a2dp_start_params;
    kymera_LatencyManagerStartDynamicAdjustment();
    /* Start identifying LL Stream when a2dp streaming aptX adaptive */
    if (a2dp_start_params->codec_settings.seid == AV_SEID_APTX_ADAPTIVE_SNK)
    {
        kymera_LatencyManagerStartLLStreamCheck();
    }
    if (a2dp_start_params->codec_settings.rate == SAMPLE_RATE_96000)
    {
        kymera_LatencyManagerSetHQStreamState(HIGH_BANDWIDTH_STREAM_ACTIVE);
    }
    else
    {
        kymera_LatencyManagerSetHQStreamState(HIGH_BANDWIDTH_STREAM_INACTIVE);
    }
}

/*! \brief Called when A2DP media stops in kymera */
void Kymera_LatencyManagerA2dpStop(void)
{
    kymera_latency_manager_data_t *data = KymeraGetLatencyData();
    kymera_LatencyManagerFreeA2dpParams();
    /* Reset state, but retain the gaming mode and dynamic adjustment states*/
    Kymera_LatencyManagerInit(data->gaming_mode_enabled, data->override_latency);
    Kymera_DynamicLatencyStopDynamicAdjustment();
    kymera_LatencyManagerStopLLStreamCheck();
    kymera_LatencyManagerSetHQStreamState(HIGH_BANDWIDTH_STREAM_INACTIVE);
}

void Kymera_LatencyManagerHandleA2dpVolumeChange(int16 volume_in_db)
{
    kymera_latency_manager_data_t *data = KymeraGetLatencyData();
    if (data->a2dp_start_params)
    {
        data->a2dp_start_params->volume_in_db = volume_in_db;
    }
}

void Kymera_LatencyManagerHandleMirrorA2dpStreamActive(void)
{
    kymera_LatencyManagerStartDynamicAdjustment();
}

void Kymera_LatencyManagerHandleMirrorA2dpStreamInactive(void)
{
    Kymera_DynamicLatencyStopDynamicAdjustment();
}

bool Kymera_LatencyManagerSetOverrideLatency(uint32 latency_ms)
{
    kymera_latency_manager_data_t *data = KymeraGetLatencyData();
    DEBUG_LOG("Kymera_LatencyManagerSetOverrideLatency %dms", latency_ms);
    data->override_latency = latency_ms;
    return TRUE;
}

void Kymera_LatencyManagerEnableGamingMode(void)
{
    KymeraGetLatencyData()->gaming_mode_enabled = 1;
    Kymera_DynamicLatencyStopDynamicAdjustment();
    /* Set LL Stream active if a2dp streaming with any codec and gaming mode enabled */
    if (appKymeraIsBusyStreaming())
    {
        kymera_LatencyManagerSetLLStreamState(LOW_LATENCY_STREAM_ACTIVE);
    }
}

void Kymera_LatencyManagerDisableGamingMode(void)
{
    kymera_latency_manager_data_t * data = KymeraGetLatencyData();
    data->gaming_mode_enabled = 0;
    if (data->a2dp_start_params)
    {
        data->current_latency = latencyManager_GetBaseLatency();
        Kymera_DynamicLatencyStartDynamicAdjustment(data->current_latency);
    }

    /* Set LL Stream inactive only if there is no periodic Low Latency check is not running in Q2Q mode.
     * If periodic Low Latency check is active, let LL stream state gets updated in next check cycle */
    if (Kymera_LatencyMangerIsLLStreamActive() && !MessagePendingFirst(KymeraGetTask(), KYMERA_INTERNAL_LOW_LATENCY_STREAM_CHECK, NULL))
    {
        kymera_LatencyManagerSetLLStreamState(LOW_LATENCY_STREAM_INACTIVE);
    }
}

uint16 Kymera_LatencyManagerMarshal(uint8 *buf, uint16 length)
{
    uint16 bytes_written = 0;
    uint16 bytes_to_write = sizeof(KymeraGetLatencyData()->current_latency);
    if (length >= bytes_to_write)
    {
        memcpy(buf, &(KymeraGetLatencyData()->current_latency), bytes_to_write);
        bytes_written = bytes_to_write;
    }
    return bytes_written;
}

uint16 Kymera_LatencyManagerUnmarshal(const uint8 *buf, uint16 length)
{
    uint16 bytes_read = 0;
    uint16 bytes_to_read = sizeof(KymeraGetLatencyData()->current_latency);
    if (length >= bytes_to_read)
    {
        memcpy(&(KymeraGetLatencyData()->current_latency), buf, bytes_to_read);
        bytes_read = bytes_to_read;
    }
    return bytes_read;
}

void Kymera_LatencyManagerHandoverCommit(bool is_primary)
{
    kymera_latency_manager_data_t *data = KymeraGetLatencyData();
    if (is_primary && data->a2dp_start_params)
    {
        kymera_LatencyManagerApplyLatency();
    }
}

#else /* INCLUDE_LATENCY_MANAGER */

uint32 Kymera_LatencyManagerGetLatencyForSeidInUs(uint8 seid)
{
    switch (seid)
    {
        case AV_SEID_APTXHD_SNK:
            return APTX_HD_LATENCY_US;
        default:
            return TWS_STANDARD_LATENCY_US;
    }
}

uint32 Kymera_LatencyManagerGetLatencyForCodecInUs(rtp_codec_type_t codec)
{
    return Kymera_LatencyManagerGetLatencyForSeidInUs(appKymeraCodecToSinkSeid(codec));
}

#endif /* INCLUDE_LATENCY_MANAGER */

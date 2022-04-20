/*!
\copyright  Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Kymera A2DP for stereo
*/

#ifdef INCLUDE_STEREO

#include "kymera_a2dp.h"
#include "kymera_a2dp_private.h"
#include "kymera_common.h"
#include "kymera_state.h"

#include "kymera_output_if.h"
#include "kymera_source_sync.h"
#include "kymera_latency_manager.h"
#include "kymera_music_processing.h"
#include "kymera_leakthrough.h"
#include "kymera_config.h"
#include "kymera_data.h"
#include "kymera_setup.h"
#include "av.h"
#include "a2dp_profile_config.h"
#include <operators.h>
#include <logging.h>

static bool appKymeraA2dpGetPreferredChainOutput(kymera_output_chain_config *config);
static const output_callbacks_t appKymeraA2dpStereoCallbacks =
{
   .OutputGetPreferredChainConfig = appKymeraA2dpGetPreferredChainOutput,
};

static const output_registry_entry_t output_info =
{
    .user = output_user_a2dp,
    .connection = output_connection_stereo,
    .callbacks = &appKymeraA2dpStereoCallbacks,
};

static void appKymeraA2dpPopulateOutputChainConfig(a2dp_params_getter_t a2dp_params, kymera_output_chain_config *config)
{
    unsigned kick_period = KICK_PERIOD_FAST;
    unsigned block_size = DEFAULT_CODEC_BLOCK_SIZE;
    unsigned kp_multiplier = 5;
    unsigned kp_divider = 2;
    unsigned input_terminal_delta_buffer_size = 0;

    DEBUG_LOG("appKymeraA2dpPopulateOutputChainConfig");

    switch (a2dp_params.seid)
    {
        case AV_SEID_SBC_SNK:
            kick_period = KICK_PERIOD_MASTER_SBC;
            block_size = SBC_CODEC_BLOCK_SIZE;
            break;

        case AV_SEID_AAC_SNK:
            kick_period = KICK_PERIOD_MASTER_AAC;
            block_size = AAC_CODEC_BLOCK_SIZE;
           /* if AEC Ref is included in the audio graph, then there are possibilities that
               AAC audio graph could have MIPS issue when graph is running @ 32MHz.
               So, input terminal buffer for source_sync operator should have some extra delta
               to offset this issue */
            if(Kymera_OutputIsAecAlwaysUsed())
            {
                /* the delta increase in buffer size should be calculated such that the overall
                    terminal buffer size should be smaller than 2*decoder_block_size */
                input_terminal_delta_buffer_size = 500;

                /* on the similar grounds, also increase the output buffer of source_sync by 4times kp */
                kp_multiplier = 4;
                kp_divider = 0;
            }
            break;

        case AV_SEID_APTX_SNK:
        case AV_SEID_APTXHD_SNK:
            kick_period = KICK_PERIOD_MASTER_APTX;
            block_size = APTX_CODEC_BLOCK_SIZE;
        break;

#ifdef INCLUDE_APTX_ADAPTIVE
        case AV_SEID_APTX_ADAPTIVE_SNK:
            kick_period = KICK_PERIOD_MASTER_APTX_ADAPTIVE;
            block_size = APTX_CODEC_BLOCK_SIZE;
        break;
#endif

        default :
            Panic();
            break;
    }

    if (Kymera_FastKickPeriodInGamingMode() && Kymera_LatencyManagerIsGamingModeEnabled())
    {
        kick_period = KICK_PERIOD_FAST;
    }

    config->rate = a2dp_params.rate;
    config->kick_period = kick_period;
    config->source_sync_kick_back_threshold = block_size;
    if (kick_period == KICK_PERIOD_SLOW)
    {
        config->source_sync_max_period = appKymeraGetSlowKickSourceSyncPeriod(TRUE);
        config->source_sync_min_period = appKymeraGetSlowKickSourceSyncPeriod(FALSE);
    }
    else if (kick_period == KICK_PERIOD_FAST)
    {
        config->source_sync_max_period = appKymeraGetFastKickSourceSyncPeriod(TRUE);
        config->source_sync_min_period = appKymeraGetFastKickSourceSyncPeriod(FALSE);
    }
    config->set_source_sync_min_period = TRUE;
    config->set_source_sync_max_period = TRUE;
    config->set_source_sync_kick_back_threshold = TRUE;

    /* Output buffer is 2.5*KP or 4*KP (if AEC Ref is in the audio chain) */
    appKymeraSetSourceSyncConfigOutputBufferSize(config, kp_multiplier, kp_divider);
    appKymeraSetSourceSyncConfigInputBufferSize(config, (block_size + input_terminal_delta_buffer_size));
    config->chain_type = output_chain_stereo;
}

static bool appKymeraA2dpGetA2dpParametersPrediction(uint32 *rate, uint8 *seid)
{
    const kymera_callback_configs_t *config = Kymera_GetCallbackConfigs();
    DEBUG_LOG("appKymeraA2dpGetA2dpParametersPrediction");
    if ((config) && (config->GetA2dpParametersPrediction))
    {
        return config->GetA2dpParametersPrediction(rate, seid);
    }
    return FALSE;
}

static bool appKymeraA2dpGetPreferredChainOutput(kymera_output_chain_config *config)
{
    uint32 rate;
    uint8 seid;
    bool a2dp_params_are_valid = appKymeraA2dpGetA2dpParametersPrediction(&rate, &seid);
    if (a2dp_params_are_valid)
    {
        a2dp_params_getter_t a2dp_params;
        a2dp_params.rate = rate;
        a2dp_params.seid = seid;

        appKymeraA2dpPopulateOutputChainConfig(a2dp_params, config);
    }
    return a2dp_params_are_valid;
}

static void appKymeraCreateInputChain(kymeraTaskData *theKymera, uint8 seid)
{
    const chain_config_t *config = NULL;
    DEBUG_LOG("appKymeraCreateInputChain");

    switch (seid)
    {
        case AV_SEID_SBC_SNK:
            DEBUG_LOG("Create SBC input chain");
            config = Kymera_GetChainConfigs()->chain_input_sbc_stereo_config;
        break;

        case AV_SEID_AAC_SNK:
            DEBUG_LOG("Create AAC input chain");
            config = Kymera_GetChainConfigs()->chain_input_aac_stereo_config;
        break;

        case AV_SEID_APTX_SNK:
            DEBUG_LOG("Create aptX Classic input chain");
            config = Kymera_GetChainConfigs()->chain_input_aptx_stereo_config;
        break;

        case AV_SEID_APTXHD_SNK:
            DEBUG_LOG("Create aptX HD input chain");
            config = Kymera_GetChainConfigs()->chain_input_aptxhd_stereo_config;

        break;

#ifdef INCLUDE_APTX_ADAPTIVE
        case AV_SEID_APTX_ADAPTIVE_SNK:
             DEBUG_LOG("Create aptX Adaptive input chain");
             if (theKymera->q2q_mode)
                 config =  Kymera_GetChainConfigs()->chain_input_aptx_adaptive_stereo_q2q_config;
             else
                 config =  Kymera_GetChainConfigs()->chain_input_aptx_adaptive_stereo_config;
        break;
#endif
        default:
            Panic();
        break;
    }

    /* Create input chain */
    theKymera->chain_input_handle = PanicNull(ChainCreate(config));
}

static void appKymeraConfigureInputChain(kymeraTaskData *theKymera,
                                         uint8 seid, uint32 rate,
                                         bool cp_header_enabled,
                                         aptx_adaptive_ttp_latencies_t nq2q_ttp)
{
    kymera_chain_handle_t chain_handle = theKymera->chain_input_handle;
    rtp_codec_type_t rtp_codec = -1;
    rtp_working_mode_t mode = rtp_decode;
    Operator op_aac_decoder;
#ifdef INCLUDE_APTX_ADAPTIVE
    Operator op;
#endif
    Operator op_rtp_decoder = ChainGetOperatorByRole(chain_handle, OPR_RTP_DECODER);
    uint32_t rtp_buffer_size = PRE_DECODER_BUFFER_SIZE;
    uint32_t max_aptx_bitrate = 0;
    DEBUG_LOG("appKymeraConfigureInputChain");

    switch (seid)
    {
        case AV_SEID_SBC_SNK:
            DEBUG_LOG("configure SBC input chain");
            rtp_codec = rtp_codec_type_sbc;
        break;

        case AV_SEID_AAC_SNK:
            DEBUG_LOG("configure AAC input chain");
            rtp_codec = rtp_codec_type_aac;
            op_aac_decoder = PanicZero(ChainGetOperatorByRole(chain_handle, OPR_AAC_DECODER));
            OperatorsRtpSetAacCodec(op_rtp_decoder, op_aac_decoder);
        break;
		
        case AV_SEID_APTX_SNK:
            DEBUG_LOG("configure aptX Classic input chain");
            rtp_codec = rtp_codec_type_aptx;
            if (!cp_header_enabled)
            {
                mode = rtp_ttp_only;
            }
        break;

        case AV_SEID_APTXHD_SNK:
            DEBUG_LOG("configure aptX HD input chain");
            rtp_codec = rtp_codec_type_aptx_hd;
        break;

#ifdef INCLUDE_APTX_ADAPTIVE
        case AV_SEID_APTX_ADAPTIVE_SNK:
            DEBUG_LOG("configure aptX adaptive input chain");
            aptx_adaptive_ttp_in_ms_t aptx_ad_ttp;

            uint32_t max_aptx_latency = APTX_ADAPTIVE_HQ_LATENCY_MS;

            if (theKymera->q2q_mode)
            {
                max_aptx_bitrate = (rate == SAMPLE_RATE_96000) ? APTX_AD_CODEC_RATE_HS_QHS_96K_KBPS * 1000: APTX_AD_CODEC_RATE_QHS_48K_KBPS * 1000;
                rtp_buffer_size = appKymeraGetAudioBufferSize(max_aptx_bitrate, max_aptx_latency);

                op = PanicZero(ChainGetOperatorByRole(chain_handle, OPR_SWITCHED_PASSTHROUGH_CONSUMER));
                OperatorsSetSwitchedPassthruEncoding(op, spc_op_format_encoded);
                OperatorsStandardSetBufferSizeWithFormat(op, rtp_buffer_size, operator_data_format_encoded);
                OperatorsSetSwitchedPassthruMode(op, spc_op_mode_passthrough);
            }
            else
            {
                convertAptxAdaptiveTtpToOperatorsFormat(nq2q_ttp, &aptx_ad_ttp);
                getAdjustedAptxAdaptiveTtpLatencies(&aptx_ad_ttp);
                OperatorsRtpSetAptxAdaptiveTTPLatency(op_rtp_decoder, aptx_ad_ttp);
                rtp_codec = rtp_codec_type_aptx_ad;

                max_aptx_bitrate = (rate == SAMPLE_RATE_96000) ? APTX_AD_CODEC_RATE_HS_NQHS_96K_KBPS * 1000 : APTX_AD_CODEC_RATE_NQHS_48K_KBPS *1000;
                max_aptx_latency = aptx_ad_ttp.high_quality ;
                rtp_buffer_size = appKymeraGetAudioBufferSize(max_aptx_bitrate, max_aptx_latency);
            }

            op = PanicZero(ChainGetOperatorByRole(chain_handle, OPR_APTX_ADAPTIVE_DECODER));
            OperatorsStandardSetSampleRate(op, rate);

        break;
#endif

        default:
            Panic();
        break;
    }

    if (!theKymera->q2q_mode) /* We don't use rtp decoder for Q2Q mode */
        appKymeraConfigureRtpDecoder(op_rtp_decoder, rtp_codec, mode, rate, cp_header_enabled, rtp_buffer_size);

    if(theKymera->chain_config_callbacks && theKymera->chain_config_callbacks->ConfigureA2dpInputChain)
    {
        kymera_a2dp_config_params_t params = {0};
        params.seid = seid;
        params.sample_rate = rate;
        params.max_bitrate = max_aptx_bitrate;
        params.nq2q_ttp = nq2q_ttp;
        theKymera->chain_config_callbacks->ConfigureA2dpInputChain(chain_handle, &params);
    }

    ChainConnect(chain_handle);
}

static void appKymeraCreateAndConfigureOutputChain(uint8 seid, uint32 rate,
                                                   int16 volume_in_db)
{
    kymera_output_chain_config config = {0};
    a2dp_params_getter_t a2dp_params;
    a2dp_params.seid = seid;
    a2dp_params.rate = rate;

    appKymeraA2dpPopulateOutputChainConfig(a2dp_params, &config);
    DEBUG_LOG("appKymeraCreateAndConfigureOutputChain, creating output chain, completing startup");
    PanicFalse(Kymera_OutputPrepare(output_user_a2dp, &config));
    KymeraOutput_SetMainVolume(volume_in_db);
}

static void appKymeraStartChains(kymeraTaskData *theKymera)
{
    bool connected;

    DEBUG_LOG("appKymeraStartChains");
    /* Start the output chain regardless of whether the source was connected
    to the input chain. Failing to do so would mean audio would be unable
    to play a tone. This would cause kymera to lock, since it would never
    receive a KYMERA_OP_MSG_ID_TONE_END and the kymera lock would never
    be cleared. */
    KymeraOutput_ChainStart();
    Kymera_StartMusicProcessingChain();
    /* In Q2Q mode the media source has already been connected to the input
    chain by the TransformPacketise so the chain can be started immediately */
    if (theKymera->q2q_mode)
    {
        ChainStart(theKymera->chain_input_handle);
    }
    else
    {
        /* The media source may fail to connect to the input chain if the source
        disconnects between the time A2DP asks Kymera to start and this
        function being called. A2DP will subsequently ask Kymera to stop. */
        connected = ChainConnectInput(theKymera->chain_input_handle, theKymera->media_source, EPR_SINK_MEDIA);
        if (connected)
        {
            ChainStart(theKymera->chain_input_handle);
        }
    }
}

static void appKymeraJoinChains(kymeraTaskData *theKymera)
{
    output_source_t output = {0};
    output.stereo.left = ChainGetOutput(theKymera->chain_input_handle, EPR_SOURCE_DECODED_PCM);
    output.stereo.right = ChainGetOutput(theKymera->chain_input_handle, EPR_SOURCE_DECODED_PCM_RIGHT);

    if(Kymera_IsMusicProcessingPresent())
    {
        PanicFalse(ChainConnectInput(theKymera->chain_music_processing_handle, output.stereo.left, EPR_MUSIC_PROCESSING_IN_L));
        PanicFalse(ChainConnectInput(theKymera->chain_music_processing_handle, output.stereo.right, EPR_MUSIC_PROCESSING_IN_R));
        output.stereo.left = ChainGetOutput(theKymera->chain_music_processing_handle, EPR_MUSIC_PROCESSING_OUT_L);
        output.stereo.right = ChainGetOutput(theKymera->chain_music_processing_handle, EPR_MUSIC_PROCESSING_OUT_R);
    }

    PanicFalse(Kymera_OutputConnect(output_user_a2dp, &output));
}

bool Kymera_A2dpStart(const a2dp_codec_settings *codec_settings, uint32 max_bitrate, int16 volume_in_db,
                                     aptx_adaptive_ttp_latencies_t nq2q_ttp)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    UNUSED(max_bitrate);
    bool cp_header_enabled;
    uint32 rate;
    uint8 seid;
    Source media_source;
    uint16 mtu;

    DEBUG_LOG("Kymera_A2dpStart");
    appKymeraGetA2dpCodecSettingsCore(codec_settings, &seid, &media_source, &rate, &cp_header_enabled, &mtu, NULL);

    PanicZero(media_source); /* Force panic at this point as source should never be zero */

    /* If the DSP is already running, set turbo clock to reduce startup time.
    If the DSP is not running this call will fail. That is ignored since
    the DSP will subsequently be started when the first chain is created
    and it starts by default at turbo clock */
    appKymeraSetActiveDspClock(AUDIO_DSP_TURBO_CLOCK);
    theKymera->cp_header_enabled = cp_header_enabled;

    appKymeraCreateAndConfigureOutputChain(seid, rate, volume_in_db);
    
    appKymeraCreateInputChain(theKymera, seid);
    appKymeraConfigureInputChain(theKymera, seid,
                                 rate, cp_header_enabled,
                                 nq2q_ttp);
    Kymera_CreateMusicProcessingChain();
    Kymera_ConfigureMusicProcessing(rate);
    appKymeraJoinChains(theKymera);
    appKymeraConfigureDspPowerMode();
    /* Connect media source to chain */
    StreamDisconnect(media_source, 0);

    if (theKymera->q2q_mode)
    {
        Sink sink = ChainGetInput(theKymera->chain_input_handle, EPR_SINK_MEDIA);
        Transform packetiser = PanicNull(TransformPacketise(media_source, sink));
		int16 hq_latency_adjust = Kymera_LatencyManagerIsGamingModeEnabled() ?
                                      (rate == SAMPLE_RATE_96000)? aptxAdaptiveTTPLatencyAdjustHQStandard() : aptxAdaptiveTTPLatencyAdjustHQGaming() :
                                      aptxAdaptiveTTPLatencyAdjustHQStandard();

        if (packetiser)
        {
            PanicFalse(TransformConfigure(packetiser, VM_TRANSFORM_PACKETISE_CODEC, VM_TRANSFORM_PACKETISE_CODEC_APTX));
            PanicFalse(TransformConfigure(packetiser, VM_TRANSFORM_PACKETISE_MODE, VM_TRANSFORM_PACKETISE_MODE_TWSPLUS));
            PanicFalse(TransformConfigure(packetiser, VM_TRANSFORM_PACKETISE_SAMPLE_RATE, (uint16) rate));
            PanicFalse(TransformConfigure(packetiser, VM_TRANSFORM_PACKETISE_CPENABLE, (uint16) cp_header_enabled));
            PanicFalse(TransformConfigure(packetiser, VM_TRANSFORM_PACKETISE_TTP_DELAY_SSRC_TRIGGER_1, aptxAdaptiveLowLatencyStreamId_SSRC_Q2Q()));
            PanicFalse(TransformConfigure(packetiser, VM_TRANSFORM_PACKETISE_TTP_DELAY_SSRC_1, aptxAdaptiveTTPLatencyAdjustLL()));
            PanicFalse(TransformConfigure(packetiser, VM_TRANSFORM_PACKETISE_TTP_DELAY_SSRC_TRIGGER_2, aptxAdaptiveHQStreamId_SSRC()));
            PanicFalse(TransformConfigure(packetiser, VM_TRANSFORM_PACKETISE_TTP_DELAY_SSRC_2, hq_latency_adjust));
            PanicFalse(TransformStart(packetiser));
            theKymera->packetiser = packetiser;
         }
    }
    theKymera->media_source = media_source;
    appKymeraStartChains(theKymera);
    Kymera_LeakthroughSetAecUseCase(aec_usecase_create_leakthrough_chain);
    return TRUE;

}

void Kymera_A2dpCommonStop(Source source)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    DEBUG_LOG("Kymera_A2dpCommonStop, source(%p)", source);

    PanicNull(theKymera->chain_input_handle);

    Kymera_LeakthroughSetAecUseCase(aec_usecase_default);

    /* Stop chains before disconnecting */
    ChainStop(theKymera->chain_input_handle);

    /* Disconnect A2DP source from the RTP operator then dispose */
    StreamDisconnect(source, 0);
    StreamConnectDispose(source);

    Kymera_StopMusicProcessingChain();

    Kymera_OutputDisconnect(output_user_a2dp);

    Kymera_DestroyMusicProcessingChain();

    /* Destroy chains now that input has been disconnected */
    ChainDestroy(theKymera->chain_input_handle);
    theKymera->chain_input_handle = NULL;

}

bool Kymera_A2dpHandleInternalStart(const KYMERA_INTERNAL_A2DP_START_T *msg)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    uint8 seid = msg->codec_settings.seid;
    uint32 rate = msg->codec_settings.rate;
    uint8 q2q = msg->q2q_mode;

    DEBUG_LOG("Kymera_A2dpHandleInternalStart, state %u, seid %u, rate %u", appKymeraGetState(), seid, rate);

    if (appA2dpIsSeidNonTwsSink(seid))
    {
        /* Only stop Leakthrough chain with non-TWS message. appKymeraA2dpStartMaster will recreate Leakthrough chain. */
        Kymera_LeakthroughStopChainIfRunning();

        switch (appKymeraGetState())
        {
            default:
            case KYMERA_STATE_IDLE:
            {
                theKymera->a2dp_seid = seid;
                theKymera->q2q_mode = q2q;
                appKymeraSetState(KYMERA_STATE_A2DP_STARTING_A);
            }
            // fall-through
            case KYMERA_STATE_A2DP_STARTING_A:
            {
                if (!Kymera_A2dpStart(&msg->codec_settings, msg->max_bitrate, msg->volume_in_db,
                                              msg->nq2q_ttp))
                {
                    DEBUG_LOG("Kymera_A2dpHandleInternalStart, state %u, seid %u, rate %u", appKymeraGetState(), seid, rate);
                    Panic();
                }
                /* Startup is complete, now streaming */
                appKymeraSetState(KYMERA_STATE_A2DP_STREAMING);
                Kymera_LatencyManagerA2dpStart(msg);
            }
            break;
        }
    }
    else
    {
        /* Unsupported SEID, control should never reach here */
        Panic();
    }
    return TRUE;
}

void Kymera_A2dpHandleInternalStop(const KYMERA_INTERNAL_A2DP_STOP_T *msg)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    uint8 seid = msg->seid;

    DEBUG_LOG("Kymera_A2dpHandleInternalStop, state %u, seid %u", appKymeraGetState(), seid);

    if (appA2dpIsSeidNonTwsSink(seid))
    {
        switch (appKymeraGetState())
        {
            case KYMERA_STATE_A2DP_STREAMING:
                /* Keep framework enabled until after DSP clock update */
                OperatorsFrameworkEnable();
                
                Kymera_A2dpCommonStop(msg->source);
                theKymera->a2dp_seid = AV_SEID_INVALID;
                appKymeraSetState(KYMERA_STATE_IDLE);
                
                /* Return to low power mode (if applicable) */
                appKymeraConfigureDspPowerMode();
                OperatorsFrameworkDisable();
                
                Kymera_LatencyManagerA2dpStop();
                Kymera_LeakthroughResumeChainIfSuspended();
            break;

            case KYMERA_STATE_IDLE:
            break;

            default:
                // Report, but ignore attempts to stop in invalid states
                DEBUG_LOG("Kymera_A2dpHandleInternalStop, invalid state %u", appKymeraGetState());
            break;
        }
    }
    else
    {
        /* Unsupported SEID, control should never reach here */
        Panic();
    }
}

void Kymera_A2dpHandleInternalSetVolume(int16 volume_in_db)
{
    DEBUG_LOG("Kymera_A2dpHandleInternalSetVolume, vol %d", volume_in_db);

    switch (appKymeraGetState())
    {
        case KYMERA_STATE_A2DP_STREAMING:
            KymeraOutput_SetMainVolume(volume_in_db);
            Kymera_LatencyManagerHandleA2dpVolumeChange(volume_in_db);
            break;

        default:
            break;
    }
}

void Kymera_A2dpInit(void)
{
    Kymera_OutputRegister(&output_info);
}

#endif /* INCLUDE_STEREO */

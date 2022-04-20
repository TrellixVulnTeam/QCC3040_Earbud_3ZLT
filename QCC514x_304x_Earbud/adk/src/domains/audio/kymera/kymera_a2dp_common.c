/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera_a2d_common.c
\brief      Kymera A2DP common functions.
*/

#include "kymera_a2dp.h"
#include "kymera_a2dp_private.h"
#include "kymera_output_if.h"
#include "kymera_common.h"
#include "kymera_chain_roles.h"
#include "kymera_latency_manager.h"
#include "kymera_config.h"
#include "kymera_data.h"
#include "sbc.h"
#include <av.h>
#include <operator.h>
#include <operators.h>
#include <panic.h>
#include <opmsg_prim.h>
#include <multidevice.h>
#include <logging.h>

#define CONVERSION_FACTOR_2MS_TO_1MS (2U)

/*! Default values for aptX adaptive NQ2Q TTP latency (in ms) */
#ifdef INCLUDE_STEREO
#define APTX_AD_TTP_LL0_MIN  55
#define APTX_AD_TTP_LL0_MAX 200
#define APTX_AD_TTP_LL1_MIN  75
#define APTX_AD_TTP_LL1_MAX 230
#define APTX_AD_TTP_HQ_MIN  200
#define APTX_AD_TTP_HQ_MAX  500
#define APTX_AD_TTP_TWS_MIN 200
#define APTX_AD_TTP_TWS_MAX 500
#else
/*! Earbud minimum latency is higher than max negotiated latency value  */
#define APTX_AD_TTP_LL0_MIN  90
#define APTX_AD_TTP_LL0_MAX 200
#define APTX_AD_TTP_LL1_MIN  90
#define APTX_AD_TTP_LL1_MAX 230
#define APTX_AD_TTP_HQ_MIN  300
#define APTX_AD_TTP_HQ_MAX  500
#define APTX_AD_TTP_TWS_MIN 300
#define APTX_AD_TTP_TWS_MAX 500
#endif

static uint32 divideAndRoundUp(uint32 dividend, uint32 divisor)
{
    if (dividend == 0)
        return 0;
    else
        return ((dividend - 1) / divisor) + 1;
}

void convertAptxAdaptiveTtpToOperatorsFormat(const aptx_adaptive_ttp_latencies_t ttp_in_non_q2q_mode,
                                             aptx_adaptive_ttp_in_ms_t *aptx_ad_ttp)
{
    aptx_ad_ttp->low_latency_0 = ttp_in_non_q2q_mode.low_latency_0_in_ms;
    aptx_ad_ttp->low_latency_1 = ttp_in_non_q2q_mode.low_latency_1_in_ms;
    aptx_ad_ttp->high_quality  = (uint16) (CONVERSION_FACTOR_2MS_TO_1MS * ttp_in_non_q2q_mode.high_quality_in_2ms);
    aptx_ad_ttp->tws_legacy    = (uint16) (CONVERSION_FACTOR_2MS_TO_1MS * ttp_in_non_q2q_mode.tws_legacy_in_2ms);
}

/* Adjust requested latency figures against defined minimum and maximum values for TWM */
void getAdjustedAptxAdaptiveTtpLatencies(aptx_adaptive_ttp_in_ms_t *aptx_ad_ttp)
{
    if (aptx_ad_ttp->low_latency_0 < APTX_AD_TTP_LL0_MIN || aptx_ad_ttp->low_latency_0 > APTX_AD_TTP_LL0_MAX)
        aptx_ad_ttp->low_latency_0 = APTX_AD_TTP_LL0_MIN;
    if (aptx_ad_ttp->low_latency_1 < APTX_AD_TTP_LL1_MIN || aptx_ad_ttp->low_latency_1 > APTX_AD_TTP_LL1_MAX)
        aptx_ad_ttp->low_latency_1 = APTX_AD_TTP_LL1_MIN;
    if (aptx_ad_ttp->high_quality < APTX_AD_TTP_HQ_MIN || aptx_ad_ttp->high_quality > APTX_AD_TTP_HQ_MAX)
        aptx_ad_ttp->high_quality = APTX_AD_TTP_HQ_MIN;
    if (aptx_ad_ttp->tws_legacy < APTX_AD_TTP_TWS_MIN || aptx_ad_ttp->tws_legacy > APTX_AD_TTP_TWS_MAX)
        aptx_ad_ttp->tws_legacy = APTX_AD_TTP_TWS_MIN;
}

void appKymeraGetA2dpCodecSettingsCore(const a2dp_codec_settings *codec_settings,
                                       uint8 *seid, Source *source, uint32 *rate,
                                       bool *cp_enabled, uint16 *mtu, bool *split_tx)
{
    if (seid)
    {
        *seid = codec_settings->seid;
    }
    if (source)
    {
        *source = StreamSourceFromSink(codec_settings->sink);
    }
    if (rate)
    {
        *rate = codec_settings->rate;
    }
    if (cp_enabled)
    {
        *cp_enabled = !!(codec_settings->codecData.content_protection);
    }
    if (mtu)
    {
        *mtu = codec_settings->codecData.packet_size;
    }
    if (split_tx)
    {
        *split_tx = !!(codec_settings->codecData.aptx_ad_params.features & aptx_ad_split_streaming );
    }
}

void Kymera_A2dpConfigureRtpDecoderStartupPeriod(Operator op, uint16 startup_period)
{
    /* These are the default parameters. */
    const uint32 filter_gain = FRACTIONAL(0.997);
    const uint32 err_scale = FRACTIONAL(-0.00000001);
    const OPMSG_COMMON_MSG_SET_TTP_PARAMS ttp_params_msg = {
    OPMSG_COMMON_MSG_SET_TTP_PARAMS_CREATE(OPMSG_COMMON_SET_TTP_PARAMS,
        UINT32_MSW(filter_gain), UINT32_LSW(filter_gain),
        UINT32_MSW(err_scale), UINT32_LSW(err_scale),
        startup_period)
    };
    PanicFalse(OperatorMessage(op, ttp_params_msg._data, OPMSG_COMMON_MSG_SET_TTP_PARAMS_WORD_SIZE, NULL, 0));
}

void appKymeraConfigureRtpDecoder(Operator op, rtp_codec_type_t codec_type, rtp_working_mode_t mode, uint32 rate, bool cp_header_enabled, unsigned buffer_size)
{
    uint32 latency;

    OperatorsRtpSetCodecType(op, codec_type);
    OperatorsRtpSetWorkingMode(op, mode);
    latency = Kymera_LatencyManagerGetLatencyForCodecInUs(codec_type);
    OperatorsStandardSetTimeToPlayLatency(op, latency);

    /* The RTP decoder controls the audio latency by assigning timestamps
    to the incoming audio stream. If the latency falls outside the limits (e.g.
    because the source delivers too much/little audio in a given time) the
    RTP decoder will reset its timestamp generator, returning to the target
    latency immediately. This will cause an audio glitch, but the AV sync will
    be correct and the system will operate correctly.

    Since audio is forwarded to the slave earbud, the minimum latency is the
    time at which the packetiser transmits packets to the slave device.
    If the latency were lower than this value, the packetiser would discard the audio
    frames and not transmit any audio to the slave, resulting in silence.
    */

    OperatorsStandardSetLatencyLimits(op, 0, US_PER_MS*TWS_STANDARD_LATENCY_MAX_MS);

    if (buffer_size)
    {
        OperatorsStandardSetBufferSizeWithFormat(op, buffer_size, operator_data_format_encoded);
    }

    OperatorsRtpSetContentProtection(op, cp_header_enabled);

    Kymera_A2dpConfigureRtpDecoderStartupPeriod(op, 0);
    OperatorsStandardSetSampleRate(op, rate);
}

static void appKymeraGetLeftRightMixerGains(bool stereo_lr_mix, bool is_left, int *gain_l, int *gain_r)
{
    int gl, gr;

    if (stereo_lr_mix)
    {
        gl = gr = GAIN_HALF;
    }
    else
    {
        gl = is_left ? GAIN_FULL : GAIN_MIN;
        gr = is_left ? GAIN_MIN : GAIN_FULL;
    }
    *gain_l = gl;
    *gain_r = gr;
}


void appKymeraConfigureLeftRightMixer(kymera_chain_handle_t chain, uint32 rate, bool stereo_lr_mix, bool is_left)
{
    Operator mixer;

    /* The aptX adaptive decoder uses it's own internal downmix */
    if (GET_OP_FROM_CHAIN(mixer, chain, OPR_APTX_ADAPTIVE_DECODER))
    {
        appKymeraSetAptxADMixerModes(mixer, is_left, stereo_lr_mix);
    }
    else if (GET_OP_FROM_CHAIN(mixer, chain, OPR_LEFT_RIGHT_MIXER))
    {
        int gain_l, gain_r;

        appKymeraGetLeftRightMixerGains(stereo_lr_mix, is_left, &gain_l, &gain_r);

        OperatorsConfigureMixer(mixer, rate, 1, gain_l, gain_r, GAIN_MIN, 1, 1, 0);
        OperatorsMixerSetNumberOfSamplesToRamp(mixer, MIXER_GAIN_RAMP_SAMPLES);
    }
}

void appKymeraSetLeftRightMixerMode(kymera_chain_handle_t chain, bool stereo_lr_mix, bool is_left)
{
    Operator mixer;

    /* The aptX adaptive decoder uses it's own internal downmix */
    if (GET_OP_FROM_CHAIN(mixer, chain, OPR_APTX_ADAPTIVE_DECODER))
    {
        appKymeraSetAptxADMixerModes(mixer, is_left, stereo_lr_mix);
    }
    else if (GET_OP_FROM_CHAIN(mixer, chain, OPR_APTX_CLASSIC_MONO_DECODER_NO_AUTOSYNC))
    {/* Check for one instance of the classic decoder. This means we are aptX classic
       and we need to reconfigure the chain. */
        if (appConfigEnableAptxStereoMix())
        {
            appKymeraReConfigureClassicChain(chain, stereo_lr_mix, is_left);
        }
    }
    else if (GET_OP_FROM_CHAIN(mixer, chain, OPR_LEFT_RIGHT_MIXER))
    {
        int gain_l, gain_r;

        appKymeraGetLeftRightMixerGains(stereo_lr_mix, is_left, &gain_l, &gain_r);

        OperatorsMixerSetGains(mixer, gain_l, gain_r, GAIN_MIN);
    }
}


void appKymeraReConfigureClassicChain(kymera_chain_handle_t chain, bool stereo_lr_mix, bool is_left)
{
    Operator mixer;

    DEBUG_LOG("appKymeraReConfigureClassicChain, %d, %d", stereo_lr_mix, is_left);

    if (GET_OP_FROM_CHAIN(mixer, chain, OPR_LEFT_RIGHT_MIXER))
    {
        int gain_l, gain_r;
        Operator op;
        /* Initialise vales to the right ear bud */
        chain_operator_role_t role = OPR_APTX_CLASSIC_MONO_DECODER_NO_AUTOSYNC;
        uint16_t mixer_port = 0;

        if (stereo_lr_mix) /* To dual passthrough mode */
        {
            if (is_left)
            { /* we we are left, we need to restart right channel */
                role = OPR_APTX_CLASSIC_MONO_DECODER_NO_AUTOSYNC_SECONDARY;
                mixer_port = 1;
            }
            op = PanicZero(ChainGetOperatorByRole(chain, role));
            Source aptx_mono = StreamSourceFromOperatorTerminal(op, 0);
            Sink mixer_in = StreamSinkFromOperatorTerminal(mixer, mixer_port);
            StreamConnect(aptx_mono, mixer_in);

            Operator op_list[] = {op};
            PanicFalse(OperatorStartMultiple(1, op_list, NULL));

            op = PanicZero(ChainGetOperatorByRole(chain, OPR_SWITCHED_PASSTHROUGH_CONSUMER));
            OperatorsSetSwitchedPassthruMode(op, spc_op_mode_tagsync_dual);
        }
        else /* To mono-mode */
        {
            spc_mode_t spc_mode = spc_op_mode_tagsync_1;
            if (is_left)
            {
                role = OPR_APTX_CLASSIC_MONO_DECODER_NO_AUTOSYNC_SECONDARY;
                spc_mode = spc_op_mode_tagsync_0;
                mixer_port = 1;
            }

            op = PanicZero(ChainGetOperatorByRole(chain, OPR_SWITCHED_PASSTHROUGH_CONSUMER));
            DEBUG_LOG("appKymeraReConfigureClassicChain [to mono] mode=%d, spc=%x", spc_mode, op);
            OperatorsSetSwitchedPassthruMode(op, spc_mode);

            op = PanicZero(ChainGetOperatorByRole(chain, role));
            Operator op_list[] = {op};
            PanicFalse(OperatorStopMultiple(1, op_list, NULL));


            Source aptx_mono_out = StreamSourceFromOperatorTerminal(op, 0);
            Sink mixer_in = StreamSinkFromOperatorTerminal(mixer, mixer_port);
            StreamDisconnect(aptx_mono_out, mixer_in);
        }

        appKymeraGetLeftRightMixerGains(stereo_lr_mix, is_left, &gain_l, &gain_r);
        DEBUG_LOG("appKymeraReConfigureClassicChain gainl=%d, gainr=%d", gain_l, gain_r);
        OperatorsMixerSetGains(mixer, gain_l, gain_r, GAIN_MIN);
        OperatorsMixerSetNumberOfSamplesToRamp(mixer, MIXER_GAIN_RAMP_SAMPLES);
    }
}

unsigned appKymeraGetSbcEncodedDataBufferSize(const sbc_encoder_params_t *sbc_params, uint32 latency_in_ms)
{
    uint32 frame_length = Sbc_GetFrameLength(sbc_params);
    uint32 bitrate = Sbc_GetBitrate(sbc_params);
    uint32 size_in_bits = divideAndRoundUp(latency_in_ms * bitrate, 1000);
    /* Round up this number if not perfectly divided by frame length to make sure we can buffer the latency required in SBC frames */
    uint32 num_frames = divideAndRoundUp(size_in_bits, frame_length * 8);
    size_in_bits = num_frames * frame_length * 8;
    unsigned size_in_words = divideAndRoundUp(size_in_bits, CODEC_BITS_PER_MEMORY_WORD);

    DEBUG_LOG("appKymeraGetSbcEncodedDataBufferSize: frame_length %u, bitrate %u, num_frames %u, buffer_size %u", frame_length, bitrate, num_frames, size_in_words);

    return size_in_words;
}

unsigned appKymeraGetAudioBufferSize(uint32 max_bitrate, uint32 latency_in_ms)
{
    uint32 size_in_bits = divideAndRoundUp(latency_in_ms * max_bitrate, 1000);
    unsigned size_in_words = divideAndRoundUp(size_in_bits, CODEC_BITS_PER_MEMORY_WORD);
    return size_in_words;
}

#ifndef INCLUDE_MIRRORING
unsigned appKymeraGetCurrentLatency(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    return theKymera->source_latency_adjust;
}

void appKymeraSetTargetLatency(uint16_t target_latency)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    theKymera->source_latency_adjust = target_latency;

    if (theKymera->packetiser)
    {
        TransformConfigure(theKymera->packetiser, VM_TRANSFORM_PACKETISE_TTP_DELAY, theKymera->source_latency_adjust);
    }
}
#endif

bool appKymeraSetAptxADMixerModes(Operator decoder, bool is_left, bool stereo_lr_mix)
{
#ifdef APTX_ADAPTIVE_SUPPORT_96K
    kymeraTaskData *theKymera = KymeraGetTaskData();
    uint32 mode = VM_TRANSFORM_PACKETISE_RTP_SPLIT_MODE_DISABLE;
    bool is_96K = (KymeraOutput_GetMainSampleRate() == SAMPLE_RATE_96000);

#ifdef INCLUDE_MIRRORING
    Transform packetiser = theKymera->hashu.packetiser;
#else
    Transform packetiser = theKymera->packetiser;
#endif

    /* Force disable stereo mix for 96K if necessary) */
    if ((appConfigEnableAptxAdaptiveStereoMix96K() == FALSE) && is_96K)
        stereo_lr_mix = FALSE;

    /* If we are in split TX mode AND 96K is enabled, left and right selection is done
     * via the packetiser */
    if ((theKymera->split_tx_mode) && is_96K)
    {        
        if (stereo_lr_mix)
            mode = VM_TRANSFORM_PACKETISE_RTP_SPLIT_MODE_PLAY_BOTH;
        else if (is_left)
            mode = VM_TRANSFORM_PACKETISE_RTP_SPLIT_MODE_PLAY_LEFT;
        else
            mode = VM_TRANSFORM_PACKETISE_RTP_SPLIT_MODE_PLAY_RIGHT;
    }
    else
    {
        /* Split TX is not enabled, so we must use the channel selection to select channel */
        if (decoder == INVALID_OPERATOR)
        {
            DEBUG_LOG("appKymeraSetAptxADMixerModes: decoder invalid, cannot configure");
            return FALSE;
        }
        OperatorsStandardSetAptXADChannelSelection(decoder, stereo_lr_mix, is_left);
        /* It will be necessary to ensure that the split mode is set to zero for this
           code path. This is taken care of by the initialisation of the variable */
    }

    if (packetiser != NULL)
    {
        DEBUG_LOG("appKymeraSetAptxADMixerModes: set 0x%x, %d, %d", mode, is_left, stereo_lr_mix);
        TransformConfigure(packetiser,VM_TRANSFORM_PACKETISE_RTP_SPLIT_MODE_CHANNELS , mode);
    }
    else
    {
        DEBUG_LOG("appKymeraSetAptxADMixerModes: packetiser is NULL, not configuring");
    }

#else
    /* 96K support is not enabled, so we must use the channel selection to select channel */
    if (decoder == INVALID_OPERATOR)
    {
        DEBUG_LOG("appKymeraSetAptxADMixerModes: decoder invalid, cannot configure");
        return FALSE;
    }
    OperatorsStandardSetAptXADChannelSelection(decoder, stereo_lr_mix, is_left);
#endif
    return TRUE;
}

#define L2CAP_FILTER_MASK         0xE0 // 1110 0000 - configure for TWM bit and either left or right
#define L2CAP_FILTER_REJECT_LEFT  0xA0 // 1010 0000 - reject left channel
#define L2CAP_FILTER_REJECT_RIGHT 0xC0 // 1100 0000 - reject right channel

#define L2CAP_FILTER_MASK_DISABLED  0x0
#define L2CAP_FILTER_DISABLED       0x0

#define L2CAP_FILTER_ENABLE         TRUE
#define L2CAP_FILTER_DISABLE        FALSE

#define RTP_TWM_BYTE_OFFSET 12

bool appKymeraA2dpSetL2capFilter(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    bool result1 = FALSE;
    bool result2 = FALSE;
    Sink media_sink = theKymera->sink;

    uint8 filter_val_local = Multidevice_IsLeft() ? L2CAP_FILTER_REJECT_RIGHT : L2CAP_FILTER_REJECT_LEFT;
    uint8 filter_val_remote = Multidevice_IsLeft() ? L2CAP_FILTER_REJECT_LEFT : L2CAP_FILTER_REJECT_RIGHT;

    result1 = SinkL2capFilterPackets(media_sink, L2CAP_FILTER_ENABLE, SINK_FILTER_PATH_LOCAL, RTP_TWM_BYTE_OFFSET, L2CAP_FILTER_MASK, filter_val_local);
    result2 = SinkL2capFilterPackets(media_sink, L2CAP_FILTER_ENABLE, SINK_FILTER_PATH_RELAY, RTP_TWM_BYTE_OFFSET, L2CAP_FILTER_MASK, filter_val_remote);

    DEBUG_LOG_VERBOSE("appKymeraA2dpSetL2capFilter: snk 0x%x, local 0x%x, remote 0x%x, results %d, %d", media_sink, filter_val_local, filter_val_remote, result1, result2);

    return (result1 && result2);
}


bool appKymeraA2dpDisableL2capFilter(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    Sink media_sink = theKymera->sink;

    bool result1 = SinkL2capFilterPackets(media_sink, L2CAP_FILTER_DISABLE, SINK_FILTER_PATH_LOCAL, 0, L2CAP_FILTER_DISABLED, L2CAP_FILTER_MASK_DISABLED);
    bool result2 = SinkL2capFilterPackets(media_sink, L2CAP_FILTER_DISABLE, SINK_FILTER_PATH_RELAY, 0, L2CAP_FILTER_DISABLED, L2CAP_FILTER_MASK_DISABLED);

    DEBUG_LOG_VERBOSE("appKymeraA2dpDisableL2capFilter: snk 0x%x, results %d, %d", media_sink, result1, result2);

    return (result1 && result2);
}

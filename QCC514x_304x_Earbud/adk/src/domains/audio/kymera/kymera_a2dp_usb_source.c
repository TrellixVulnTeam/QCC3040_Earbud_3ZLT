/*!
\copyright  Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Kymera A2DP Source for USB Wired Audio.
*/

#if defined(INCLUDE_A2DP_USB_SOURCE)

#include "kymera.h"
#include "kymera_a2dp.h"
#include "kymera_chain_roles.h"
#include "kymera_usb_audio.h"
#include "kymera_common.h"
#include "kymera_state.h"
#include "kymera_data.h"
#include "kymera_setup.h"
#include "kymera_config.h"
#include "av.h"
#include "a2dp_profile_caps.h"

#define BUFFER_SIZE_FACTOR 4

/* these values are currently fixed */
#define A2DP_SBC_SUPPORTED_SUBBANDS 8
#define A2DP_SBC_SUPPORTED_BLOCK_LENGTH 16


static void KymeraA2dpUsbSource_CreateInputChain(kymeraTaskData *theKymera, uint8 seid)
{
    const chain_config_t *config = NULL;
    DEBUG_LOG_FN_ENTRY("KymeraA2dpUsbSource_CreateInputChain");

    switch (seid)
    {
        case AV_SEID_SBC_SRC:
        {
            DEBUG_LOG_DEBUG("Encoder Config: AV_SEID_SBC_SRC");
            config = Kymera_GetChainConfigs()->chain_input_usb_sbc_encode_config;
            break;
        }
        case AV_SEID_APTX_CLASSIC_SRC:
        {
            DEBUG_LOG_DEBUG("Encoder Config: AV_SEID_APTX_CLASSIC_SRC");
            config = Kymera_GetChainConfigs()->chain_input_usb_aptx_classic_encode_config;
            break;
        }
        case AV_SEID_APTX_ADAPTIVE_SRC:
        {
            DEBUG_LOG_DEBUG("Encoder Config: AV_SEID_APTX_ADAPTIVE_SRC");
            config = Kymera_GetChainConfigs()->chain_input_usb_aptx_adaptive_encode_config;
            break;
        }
        default:
            Panic();
        break;
    }

    /* Create input chain */
    theKymera->chain_input_handle = PanicNull(ChainCreate(config));
}

static unsigned  KymeraA2dpUsbSource_CalculateBufferSize(unsigned output_rate)
{
    unsigned scaled_rate = output_rate / 1000;
    return (KICK_PERIOD_SLOW * scaled_rate * BUFFER_SIZE_FACTOR) / 1000;
}

static void KymeraA2dpUsbSource_ConfigureInputChain(const KYMERA_INTERNAL_USB_AUDIO_START_T *usb_audio,kymera_chain_handle_t chain_handle,
                                         const a2dp_codec_settings *codec_settings)
{
    DEBUG_LOG_FN_ENTRY("KymeraA2dpUsbSource_ConfigureInputChain");

    usb_config_t config;
    kymeraTaskData *theKymera = KymeraGetTaskData();

    Operator usb_audio_rx_op = ChainGetOperatorByRole(theKymera->chain_input_handle, OPR_USB_AUDIO_RX);
    Operator resampler_op = ChainGetOperatorByRole(theKymera->chain_input_handle, OPR_SPEAKER_RESAMPLER);

    DEBUG_LOG_INFO("KymeraA2dpUsbSource: Resampling %u -> %u", usb_audio->sample_freq, codec_settings->rate);

    OperatorsResamplerSetConversionRate(resampler_op, usb_audio->sample_freq, codec_settings->rate);

    DEBUG_LOG_V_VERBOSE("usb_audio->usb_params->sample_freq: %u",usb_audio->sample_freq);
    DEBUG_LOG_V_VERBOSE("usb_audio->usb_params->frame_size: %u",usb_audio->frame_size);
    DEBUG_LOG_V_VERBOSE("usb_audio->usb_params->channels: %u",usb_audio->channels);
    DEBUG_LOG_V_VERBOSE("usb_audio->usb_params->min_latency_ms: %u",usb_audio->min_latency_ms);
    DEBUG_LOG_V_VERBOSE("usb_audio->usb_params->max_latency_ms: %u",usb_audio->max_latency_ms);
    DEBUG_LOG_V_VERBOSE("usb_audio->usb_params->target_latency_ms: %u",usb_audio->target_latency_ms);

    config.sample_rate = usb_audio->sample_freq;
    config.sample_size = usb_audio->frame_size;
    config.number_of_channels = usb_audio->channels;

    OperatorsConfigureUsbAudio(usb_audio_rx_op, config);

    OperatorsStandardSetLatencyLimits(usb_audio_rx_op,
                                      MS_TO_US(usb_audio->min_latency_ms),
                                      MS_TO_US(usb_audio->max_latency_ms));
    OperatorsStandardSetTimeToPlayLatency(usb_audio_rx_op,MS_TO_US(usb_audio->target_latency_ms));

    OperatorsStandardSetBufferSizeWithFormat(usb_audio_rx_op, TTP_BUFFER_SIZE,
                                                     operator_data_format_pcm);

    switch (codec_settings->seid)
    {
        case AV_SEID_SBC_SRC:
        {
            Operator sbc_encoder = PanicZero(ChainGetOperatorByRole(chain_handle, OPR_SBC_ENCODER));

            /* Configure sbc_encoder */
            sbc_encoder_params_t sbc_encoder_params;

            sbc_encoder_params.channel_mode = codec_settings->channel_mode;
            sbc_encoder_params.bitpool_size = codec_settings->codecData.bitpool;
            sbc_encoder_params.sample_rate = codec_settings->rate;
            sbc_encoder_params.number_of_subbands = A2DP_SBC_SUPPORTED_SUBBANDS;
            sbc_encoder_params.number_of_blocks = A2DP_SBC_SUPPORTED_BLOCK_LENGTH;
            sbc_encoder_params.allocation_method = sbc_encoder_allocation_method_loudness;

            OperatorsSbcEncoderSetEncodingParams(sbc_encoder, &sbc_encoder_params);

        }
        break;

        case AV_SEID_APTX_CLASSIC_SRC:
        {
            /* no parameters needed for aptX Classic */
        }
        break;

        case AV_SEID_APTX_ADAPTIVE_SRC:
        {
            Operator aptx_encoder = PanicZero(ChainGetOperatorByRole(chain_handle,OPR_APTX_ADAPTIVE_ENCODER));

            aptxad_encoder_params_t encoder_parameters;

            /* encoder parameters... */
            unsigned aptx_channel = 4; /* stereo by default */
            unsigned aptx_rate = codec_settings->rate;

            encoder_parameters.bitrate = 279;
            encoder_parameters.dh5_dh3 = 352;
            encoder_parameters.quality = 2;
            encoder_parameters.channel = aptx_channel;
            encoder_parameters.compatibility= APTX_AD_ENCODER_R2_1;
            encoder_parameters.sample_rate = aptx_rate;
            OperatorsAptxAdEncoderSetEncodingParams(aptx_encoder,&encoder_parameters);

        }
        break;

        default:
            Panic();
        break;
    }

    Operator basic_passthrough_buffer = PanicZero(ChainGetOperatorByRole(chain_handle,OPR_ENCODER_OUTPUT_BUFFER));


    /* Configure basic_passthrough_buffer */
    OperatorsSetPassthroughDataFormat(basic_passthrough_buffer, operator_data_format_encoded);


    unsigned buffer_size;

    buffer_size = KymeraA2dpUsbSource_CalculateBufferSize(codec_settings->rate);
    OperatorsStandardSetBufferSize(basic_passthrough_buffer, buffer_size);

    Operator op = PanicZero(ChainGetOperatorByRole(chain_handle, OPR_SWITCHED_PASSTHROUGH_CONSUMER));
    OperatorsSetSwitchedPassthruEncoding(op, spc_op_format_encoded);
    OperatorsSetSwitchedPassthruMode(op, spc_op_mode_passthrough);

    ChainConnect(chain_handle);
}

static void KymeraA2dpUsbSource_StartChains(kymeraTaskData *theKymera, Source media_source)
{
    DEBUG_LOG_FN_ENTRY("kymeraUSbAudio_StartChains");
    /* The media source may fail to connect to the input chain if the source
    disconnects between the time A2DP asks Kymera to start and this
    function being called. A2DP will subsequently ask Kymera to stop. */
    PanicFalse(ChainConnectInput(theKymera->chain_input_handle, media_source, EPR_USB_FROM_HOST));
    ChainStart(theKymera->chain_input_handle);
}

static bool KymeraA2dpUsbSource_ConfigurePacketiser(kymera_chain_handle_t chain_handle, const a2dp_codec_settings *codec_settings)
{
    DEBUG_LOG_FN_ENTRY("KymeraA2dpUsbSource_ConfigurePacketiser");

    kymeraTaskData *theKymera = KymeraGetTaskData();

    Source source = PanicNull(ChainGetOutput(chain_handle, EPR_SOURCE_ENCODE_OUT));

    if (codec_settings->sink==NULL)
    {
        return FALSE;
    }

    Transform packetiser = PanicNull(TransformPacketise(source, codec_settings->sink));
    switch (codec_settings->seid)
    {
        case AV_SEID_APTX_CLASSIC_SRC:
        {
            PanicFalse(TransformConfigure(packetiser, VM_TRANSFORM_PACKETISE_CODEC, VM_TRANSFORM_PACKETISE_CODEC_APTX));
            PanicFalse(TransformConfigure(packetiser, VM_TRANSFORM_PACKETISE_MODE, VM_TRANSFORM_PACKETISE_MODE_RTP));
        }
        break;

        case AV_SEID_APTX_ADAPTIVE_SRC:
        {
            PanicFalse(TransformConfigure(packetiser, VM_TRANSFORM_PACKETISE_CODEC, VM_TRANSFORM_PACKETISE_CODEC_APTX));
            PanicFalse(TransformConfigure(packetiser, VM_TRANSFORM_PACKETISE_MODE, VM_TRANSFORM_PACKETISE_MODE_TWSPLUS));
            PanicFalse(TransformConfigure(packetiser, VM_TRANSFORM_PACKETISE_RTP_SSRC, aptxAdaptiveLowLatencyStreamId_SSRC_Q2Q()));
            PanicFalse(TransformConfigure(packetiser, VM_TRANSFORM_PACKETISE_RTP_SSRC_HI, 0));
        }
        break;

        case AV_SEID_SBC_SRC:
        {
            PanicFalse(TransformConfigure(packetiser, VM_TRANSFORM_PACKETISE_CODEC, VM_TRANSFORM_PACKETISE_CODEC_SBC));
            PanicFalse(TransformConfigure(packetiser, VM_TRANSFORM_PACKETISE_MODE, VM_TRANSFORM_PACKETISE_MODE_RTP));
        }
        break;

        default:
        break;
    }

    /* 
      Do not check for PanicFalse on the MTU because this feature is licensed on some platforms. e.g. QCC3056
      If the license check fails, it will return False, but play silence. Therefore it shouldn't stop here. 
    */ 
    TransformConfigure(packetiser, VM_TRANSFORM_PACKETISE_MTU, codec_settings->codecData.packet_size);

    PanicFalse(TransformConfigure(packetiser, VM_TRANSFORM_PACKETISE_SAMPLE_RATE, (uint16) codec_settings->rate));
    PanicFalse(TransformConfigure(packetiser, VM_TRANSFORM_PACKETISE_CPENABLE, (uint16) codec_settings->codecData.content_protection));
    PanicFalse(TransformStart(packetiser));

    theKymera->packetiser = packetiser;

    return TRUE;
}

static void KymeraA2dpUsbSource_DestroyChain(Source usb_source)
{
    DEBUG_LOG_FN_ENTRY("KymeraA2dpUsbSource_DestroyChain");
    kymeraTaskData *theKymera = KymeraGetTaskData();

    PanicNull(theKymera->chain_input_handle);

    Sink usb_from_host = ChainGetInput(theKymera->chain_input_handle, EPR_USB_FROM_HOST);

    Source from_encode_out = ChainGetOutput(theKymera->chain_input_handle, EPR_SOURCE_ENCODE_OUT);

    /* Stop chains before disconnecting */
    ChainStop(theKymera->chain_input_handle);

    StreamDisconnect(usb_source, 0);
    StreamConnectDispose(usb_source);
    SourceClose(usb_source);

    /* Disconnect the chain output */
    StreamDisconnect(from_encode_out, NULL);

    /* Disconnect codec source from chain */
    StreamDisconnect(NULL, usb_from_host);

    /* Destroy chains now that input has been disconnected */
    ChainDestroy(theKymera->chain_input_handle);

    theKymera->chain_input_handle = NULL;
    theKymera->output_rate = 0;
    theKymera->usb_rx = 0;
    /* Destroy packetiser */
    if (theKymera->packetiser)
    {
        TransformStop(theKymera->packetiser);
        theKymera->packetiser = NULL;
    }
}

void KymeraUsbAudio_Start(KYMERA_INTERNAL_USB_AUDIO_START_T *msg)
{
    DEBUG_LOG_FN_ENTRY("KymeraUsbAudio_Start");

    kymeraTaskData *theKymera = KymeraGetTaskData();
    if (theKymera->a2dp_output_params == NULL)
    {
        DEBUG_LOG_ERROR("KymeraUsbAudio_Start: A2DP output params not set");
        return;
    }
    const a2dp_codec_settings * codec_settings = theKymera->a2dp_output_params;
    Source usb_audio_source = msg->spkr_src;
    /* We have to disconnect the previous source stream. This may be hiding
       an underlying issue */
    StreamDisconnect(usb_audio_source, NULL);

    KymeraA2dpUsbSource_CreateInputChain(theKymera, codec_settings->seid);

    KymeraA2dpUsbSource_ConfigureInputChain( msg,theKymera->chain_input_handle, codec_settings);

    PanicFalse(KymeraA2dpUsbSource_ConfigurePacketiser(theKymera->chain_input_handle, codec_settings));

    appKymeraSetState(KYMERA_STATE_USB_AUDIO_ACTIVE);

    appKymeraConfigureDspPowerMode();

    switch (codec_settings->seid)
    {
        case AV_SEID_APTX_CLASSIC_SRC:
        DEBUG_LOG_INFO("Starting USB audio aptX Classic, Latencies: target %u, min %u, max %u",
                       msg->target_latency_ms,
                       msg->min_latency_ms,
                       msg->max_latency_ms);
        break;
        case AV_SEID_APTX_ADAPTIVE_SRC:
        DEBUG_LOG_INFO("Starting USB audio aptX Adaptive, Latencies: target %u, min %u, max %u",
                       msg->target_latency_ms,
                       msg->min_latency_ms,
                       msg->max_latency_ms);
        break;
        case AV_SEID_SBC_SRC:
        DEBUG_LOG_INFO("Starting USB audio SBC, Latencies: target %u, min %u, max %u",
                       msg->target_latency_ms,
                       msg->min_latency_ms,
                       msg->max_latency_ms);
        break;
        default:
        break;
    }
    KymeraA2dpUsbSource_StartChains(theKymera, usb_audio_source);
}

void KymeraUsbAudio_Stop(KYMERA_INTERNAL_USB_AUDIO_STOP_T *audio_params)
{
    DEBUG_LOG_FN_ENTRY("KymeraUsbAudio_Stop");
    switch (appKymeraGetState())
    {
        case KYMERA_STATE_USB_AUDIO_ACTIVE:
            KymeraA2dpUsbSource_DestroyChain(audio_params->source);
            appKymeraSetState(KYMERA_STATE_IDLE);
            PanicZero(audio_params->kymera_stopped_handler);
            audio_params->kymera_stopped_handler(audio_params->source);
        break;

        case KYMERA_STATE_IDLE:
        break;

        default:
            /* Report, but ignore attempts to stop in invalid states */
            DEBUG_LOG("KymeraUsbAudio_Stop, invalid state %u", appKymeraGetState());
        break;
    }
}

void KymeraUsbAudio_SetVolume(int16 volume_in_db)
{
    UNUSED(volume_in_db);
    DEBUG_LOG_V_VERBOSE("KymeraUsbAudio_SetVolume: Not Handled in kymera_a2dp_usb_source");
}

#endif

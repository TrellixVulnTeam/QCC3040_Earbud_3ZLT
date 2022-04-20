/*!
\copyright  Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Kymera A2DP Source for analog wired audio.
*/

#ifdef INCLUDE_A2DP_ANALOG_SOURCE
#include "kymera_wired_analog.h"
#include "kymera_a2dp.h"
#include "kymera_common.h"
#include "kymera_state.h"
#include "kymera_config.h"
#include "kymera_data.h"
#include "kymera_setup.h"
#include "av.h"
#include "kymera.h"
#include "kymera_chain_roles.h"
#include "a2dp_profile_caps.h"

#include <logging.h>

#define BUFFER_SIZE_FACTOR 4

/* these values are currently fixed */
#define A2DP_SBC_SUPPORTED_SUBBANDS 8
#define A2DP_SBC_SUPPORTED_BLOCK_LENGTH 16


static Source kymeraA2dpAnalogSource_GetSource(audio_channel channel, uint8 inst, uint32 rate)
{
#define SAMPLE_SIZE 16 /* only if 24 bit resolution is supported this can be 24 */
    Source source;
    analogue_input_params params = {
        .pre_amp = FALSE,
        .gain = 0x09, /* for line-in set to 0dB */
        .instance = 0, /* Place holder */
        .enable_24_bit_resolution = FALSE
        };

    DEBUG_LOG_VERBOSE("SourcekymeraWiredAnalog_GetSource, Get source for Channel: %u, Instance: %u and Sample Rate: %u", channel, inst, rate);
    params.instance = inst;
    source = AudioPluginAnalogueInputSetup(channel, params, rate);
    PanicFalse(SourceConfigure(source, STREAM_AUDIO_SAMPLE_SIZE, SAMPLE_SIZE));

    return source;
}


static void kymeraA2dpAnalogSource_CreateInputChain(kymeraTaskData *theKymera, uint8 seid)
{
    DEBUG_LOG_FN_ENTRY("kymeraA2dpAnalogSource_CreateInputChain");

    const chain_config_t *config = NULL;

    switch (seid)
    {
        case AV_SEID_SBC_SRC:
        {
            DEBUG_LOG_DEBUG("Encoder Config: AV_SEID_SBC_SRC");
            config = Kymera_GetChainConfigs()->chain_input_wired_sbc_encode_config;
            break;
        }
        case AV_SEID_APTX_CLASSIC_SRC:
        {
            DEBUG_LOG_DEBUG("Encoder Config: AV_SEID_APTX_CLASSIC_SRC");
            config = Kymera_GetChainConfigs()->chain_input_wired_aptx_classic_encode_config;
            break;
        }
        case AV_SEID_APTX_ADAPTIVE_SRC:
        {
            DEBUG_LOG_DEBUG("Encoder Config: AV_SEID_APTX_ADAPTIVE_SRC");
            config = Kymera_GetChainConfigs()->chain_input_wired_aptx_adaptive_encode_config;
            break;
        }
        default:
            Panic();
        break;
    }

    /* Create input chain */
    theKymera->chain_input_handle = PanicNull(ChainCreate(config));
}

static unsigned  kymeraA2dpAnalogSource_CalculateBufferSize(unsigned output_rate)
{
    unsigned scaled_rate = output_rate / 1000;
    return (KICK_PERIOD_SLOW * scaled_rate * BUFFER_SIZE_FACTOR) / 1000;
}

static void kymeraA2dpAnalogSource_ConfigureInputChain(kymera_chain_handle_t chain_handle,
                                         const a2dp_codec_settings *codec_settings, uint32 min_latency, uint32 max_latency, uint32 target_latency)
{
    DEBUG_LOG_FN_ENTRY("kymeraA2dpAnalogSource_ConfigureInputChain");

    Operator ttp_passthrough = PanicZero(ChainGetOperatorByRole(chain_handle, OPR_LATENCY_BUFFER));

    OperatorsStandardSetLatencyLimits(ttp_passthrough,
                                      MS_TO_US(min_latency),
                                      MS_TO_US(max_latency));

    OperatorsConfigureTtpPassthrough(ttp_passthrough, MS_TO_US(target_latency), codec_settings->rate, operator_data_format_pcm);

    OperatorsStandardSetBufferSizeWithFormat(ttp_passthrough, TTP_BUFFER_SIZE, operator_data_format_pcm);

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

    buffer_size = kymeraA2dpAnalogSource_CalculateBufferSize(codec_settings->rate);
    OperatorsStandardSetBufferSize(basic_passthrough_buffer, buffer_size);

    Operator op = PanicZero(ChainGetOperatorByRole(chain_handle, OPR_SWITCHED_PASSTHROUGH_CONSUMER));
    OperatorsSetSwitchedPassthruEncoding(op, spc_op_format_encoded);
    OperatorsSetSwitchedPassthruMode(op, spc_op_mode_passthrough);

    ChainConnect(chain_handle);
}

static bool kymeraA2dpAnalogSource_ConnectLineIn(kymera_chain_handle_t encoder_handle, uint32 rate)
{
    Source line_in_l = kymeraA2dpAnalogSource_GetSource(appConfigLeftAudioChannel(), appConfigLeftAudioInstance(), rate /* for now input/output rate are same */);
    Source line_in_r = kymeraA2dpAnalogSource_GetSource(appConfigRightAudioChannel(), appConfigRightAudioInstance(), rate /* for now input/output rate are same */);
    /* if stereo, then synchronize */
    if(line_in_r)
        SourceSynchronise(line_in_l, line_in_r);

    /* The media source may fail to connect to the input chain if the source
    disconnects between the time wired analog audio asks Kymera to start and this
    function being called. wired analog audio will subsequently ask Kymera to stop. */
    if (line_in_l != NULL && line_in_r !=NULL)
    {
        if (ChainConnectInput(encoder_handle, line_in_l, EPR_WIRED_STEREO_INPUT_L))
        {
            if (ChainConnectInput(encoder_handle, line_in_r, EPR_WIRED_STEREO_INPUT_R))
            {
                return TRUE;
            }
            else {
                DEBUG_LOG_ERROR("ChainConnectInput R Could not connect input");
            }
        }
        else {
            DEBUG_LOG_ERROR("ChainConnectInput L Could not connect input");
        }
    }
    else {
        DEBUG_LOG_ERROR("A Line in Source was NULL");
    }
    return FALSE;
}

static bool kymeraA2dpAnalogSource_ConfigurePacketiser(kymera_chain_handle_t chain_handle, const a2dp_codec_settings *codec_settings)
{
    DEBUG_LOG_FN_ENTRY("kymeraA2dpAnalogSource_ConfigurePacketiser");

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

static void kymeraA2dpAnalogSource_DestroyChain(void)
{
    DEBUG_LOG_FN_ENTRY("kymeraA2dpAnalogSource_DestroyChain");

    kymeraTaskData *theKymera = KymeraGetTaskData();

    PanicNull(theKymera->chain_input_handle);

    Sink to_ttp_l = ChainGetInput(theKymera->chain_input_handle, EPR_WIRED_STEREO_INPUT_L);
    Sink to_ttp_r = ChainGetInput(theKymera->chain_input_handle, EPR_WIRED_STEREO_INPUT_R);

    Source from_encode_out = ChainGetOutput(theKymera->chain_input_handle, EPR_SOURCE_ENCODE_OUT);


    DEBUG_LOG_V_VERBOSE("kymeraWiredAnalog_DestroyChain, from_encode_out source(%p)", from_encode_out);
    DEBUG_LOG_V_VERBOSE("kymeraWiredAnalog_DestroyChain, l-sink(%p), r-sink(%p)", to_ttp_l, to_ttp_r);

    /* Stop chains before disconnecting */
    ChainStop(theKymera->chain_input_handle);

    /* Disconnect codec source from chain */
    StreamDisconnect(NULL, to_ttp_l);
    StreamDisconnect(NULL, to_ttp_r);

    /* Disconnect the chain output */
    StreamDisconnect(from_encode_out, NULL);

    /* Destroy chains now that input has been disconnected */
    ChainDestroy(theKymera->chain_input_handle);

    theKymera->chain_input_handle = NULL;

    /* Destroy packetiser */
    if (theKymera->packetiser)
    {
        TransformStop(theKymera->packetiser);
        theKymera->packetiser = NULL;
    }
}

void KymeraWiredAnalog_StartPlayingAudio(const KYMERA_INTERNAL_WIRED_ANALOG_AUDIO_START_T *msg)
{
    DEBUG_LOG_FN_ENTRY("KymeraWiredAnalog_StartPlayingAudio");

    kymeraTaskData *theKymera = KymeraGetTaskData();

    const a2dp_codec_settings * codec_settings = theKymera->a2dp_output_params;
    kymeraA2dpAnalogSource_CreateInputChain(theKymera, codec_settings->seid);


    kymeraA2dpAnalogSource_ConfigureInputChain(theKymera->chain_input_handle, codec_settings, msg->min_latency, msg->max_latency, msg->target_latency);

    PanicFalse(kymeraA2dpAnalogSource_ConnectLineIn(theKymera->chain_input_handle, codec_settings->rate));

    PanicFalse(kymeraA2dpAnalogSource_ConfigurePacketiser(theKymera->chain_input_handle, codec_settings));

    appKymeraSetState(KYMERA_STATE_WIRED_AUDIO_PLAYING);
    switch (codec_settings->seid)
    {
        case AV_SEID_APTX_CLASSIC_SRC:
        DEBUG_LOG_INFO("Starting Analog audio aptX Classic, Latencies: target %u, min %u, max %u",
                       msg->target_latency,
                       msg->min_latency,
                       msg->max_latency);
        break;

        case AV_SEID_APTX_ADAPTIVE_SRC:
        DEBUG_LOG_INFO("Starting Analog audio aptX Adaptive, Latencies: target %u, min %u, max %u",
                       msg->target_latency,
                       msg->min_latency,
                       msg->max_latency);
        break;

        case AV_SEID_SBC_SRC:
        DEBUG_LOG_INFO("Starting Analog audio SBC, Latencies: target %u, min %u, max %u",
                       msg->target_latency,
                       msg->min_latency,
                       msg->max_latency);
        break;
        default:
        break;
    }
    ChainStart(theKymera->chain_input_handle);
}

void KymeraWiredAnalog_StopPlayingAudio(void)
{
    DEBUG_LOG_FN_ENTRY("KymeraWiredAnalog_StopPlayingAudio");

    switch (appKymeraGetState())
    {
        case KYMERA_STATE_WIRED_AUDIO_PLAYING:
            kymeraA2dpAnalogSource_DestroyChain();
            appKymeraSetState(KYMERA_STATE_IDLE);
        break;

        case KYMERA_STATE_IDLE:
        break;

        default:
            /* Report, but ignore attempts to stop in invalid states */
            DEBUG_LOG("KymeraWiredAnalog_StopPlayingAudio, invalid state %u", appKymeraGetState());
        break;
    }
}

#endif

/*!
\copyright  Copyright (c) 2019-2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Definition of all supported operator and endpoint roles.

*/

#ifndef KYMERA_CHAIN_ROLES_H_
#define KYMERA_CHAIN_ROLES_H_


/*@{*/
/*! These names may be used in chain operator definitions.
*/
typedef enum chain_operator_roles
{
    /*! Role identifier used for RTP decoder */
    OPR_RTP_DECODER = 0x1000,
    /*! Role identifier used for APTX demultiplexer (splitter) operator */
    OPR_APTX_DEMUX,
    /*! Role identifier used for switched passthrough operator. */
    OPR_SWITCHED_PASSTHROUGH_CONSUMER,
    /*! Role identifier used for APTX mono decoder operator */
    OPR_APTX_CLASSIC_MONO_DECODER_NO_AUTOSYNC,
    /*! Role identifier used for APTX mono decoder operator - secondary decoder */
    OPR_APTX_CLASSIC_MONO_DECODER_NO_AUTOSYNC_SECONDARY,
    /*! Role identifier used for APTX adaptive decoder operator */
    OPR_APTX_ADAPTIVE_DECODER,
    /*! Role identifier used for SBC decoder operator */
    OPR_SBC_DECODER,
    /*! Role identifier used for SBC decoder operator that re-decodes the locally rendered channel */
    OPR_LOCAL_CHANNEL_SBC_DECODER,
    /*! Role identifier used for SBC encoder operator */
    OPR_SBC_ENCODER,
    /*! Role identifier used for SBC encoder operator that re-encodes the locally rendered channel */
    OPR_LOCAL_CHANNEL_SBC_ENCODER,
    /*! Role identifier used for the AAC decoder operator */
    OPR_AAC_DECODER,
    /*! Role identifier used for the aptX classic decoder operator */
    OPR_APTX_DECODER,
    /*! Role identifier used for the aptX HD decoder operator */
    OPR_APTXHD_DECODER,
    /*! Role identifier used for the aptX adaptive TWS+ decoder operator */
    OPR_APTX_ADAPTIVE_TWS_PLUS_DECODER,
    /*! Role identifier used for the splitter */
    OPR_SPLITTER,
    /*! Role identifer used for a consumer operator */
    OPR_CONSUMER,

    /* SCO_roles - Operator roles used in the chains for SCO audio */
        /*! The operator processing incoming SCO */
    OPR_SCO_RECEIVE,
        /*! The operator processing outgoing SCO */
    OPR_SCO_SEND,
        /*! The clear voice capture operator for incoming SCO */
    OPR_CVC_RECEIVE,
        /*! The clear voice capture operator for outgoing SCO */
    OPR_CVC_SEND,

    /* Common_roles - Common operator roles used between chains */
        /*! Common synchronisation operator role */
    OPR_SOURCE_SYNC,
        /*! Common volume control operator role */
    OPR_VOLUME_CONTROL,

    /*! The buffering (passthrough) operator used in volume control chain */
    OPR_LATENCY_BUFFER,

    /*! Buffer to match buffer in a2dp/hfp chain. It it required to prevent samples being stalled and rendered with delay.
        It is only needed when a tone/vp aux source chain is joined with the main chain. */
    OPR_TONE_PROMPT_PCM_BUFFER,
    OPR_TONE_PROMPT_ENCODED_BUFFER,

    /*! Tone generator */
    OPR_TONE_GEN,

    /*! Mixes left and right stereo decode to a mono mix (100% L, 100% R, 50% L 50% R) */
    OPR_LEFT_RIGHT_MIXER,

    /*! Adaptive Echo Cancellation Reference operator */
    OPR_AEC,

    /*! Voice Activity Detection operator */
    OPR_VAD,

    /*! Role identifier used for MSBC encoder operator */
    OPR_MSBC_ENCODER,

    /*! Role identifier used for OPUS encoder operator */
    OPR_OPUS_ENCODER,

    /*! Splitter used in SCO recieve path for AANC */
    OPR_AANC_SPLT_SCO_RX,

    /*! Splitter used in SCO transmit path for AANC */
    OPR_AANC_SPLT_SCO_TX,

    /*!IIR Down sampler used for AANC use-case */
    OPR_AANC_DOWN_SAMPLE,

    /*!IIR UP sampler used for AANC use-case */
    OPR_AANC_UP_SAMPLE,

    /*!Consumer for AANC use-case */
    OPR_AANC_CONSUMER,

    /*!Basic Pass for AANC use-case */
    OPR_AANC_BASIC_PASS,

    /*!Core adaptive ANC capability */
    OPR_AANC,

    /*! Echo cancellor in AANC external mic path */
    OPR_AANC_FBC_FF_MIC_PATH,

    /*! Echo cancellor in AANC internal/error mic path */
    OPR_AANC_FBC_ERR_MIC_PATH,

    /*!Basic Pass for AANC MIC REF */
    OPR_AANC_MIC_REF_BASIC_PASS,

    /*! Splitter in mic ref path */
    OPR_AANC_SPLT_MIC_REF_PATH,

    /*! Wake-Up-Word engine operator */
    OPR_WUW,

    /*! VA graph manager operator */
    OPR_VA_GRAPH_MANAGER,

    /*! Alter gain for mic chain*/
    OPR_MIC_GAIN,

    /*! -12dB to provide headroom for music processing */
    OPR_ADD_HEADROOM,

    /*! +12dB to cancel headroom added for music processing */
    OPR_REMOVE_HEADROOM,

    /*! Speaker EQ */
    OPR_SPEAKER_EQ,
    /*! LC3 Decode SCO ISO operator */
    OPR_LC3_DECODE_SCO_ISO,

    /*! LC3 Encode SCO ISO operator */
    OPR_LC3_ENCODE_SCO_ISO,

    /*! Compander operator */
    OPR_COMPANDER,
    
    /*! PEQ operator */
    OPR_PEQ,
    
    /*! DBE operator */
    OPR_DBE,

    /*! Rate Adjust operator */
    OPR_RATE_ADJUST,

    /*! User EQ */
    OPR_USER_EQ,

    /*! USB Audio RX operator */
    OPR_USB_AUDIO_RX,

    /*! USB Audio TX operator */
    OPR_USB_AUDIO_TX,

    /*! Resampler for the speaker in usb voice receive path */
    OPR_SPEAKER_RESAMPLER,

    /*! Resampler for the mic chain in microphone concurrency */
    OPR_MIC_RESAMPLER,

    /*! Resampler for the aec chain in microphone concurrency */
    OPR_AEC_RESAMPLER,

    /*! Role identifier used for AptX Adaptive encoder */
    OPR_APTX_ADAPTIVE_ENCODER,

    /*! Buffer for A2DP Source Chain */
    OPR_ENCODER_OUTPUT_BUFFER,
    
    /*! Basic passthrough operator */
    OPR_BASIC_PASS,

    /*! Earbud Fit test operators */
    OPR_FIT_TEST,
    OPR_FIT_TEST_BASIC_PT,

    /*! Role identifier used for AptX Classic encoder */
    OPR_APTX_CLASSIC_ENCODER,
} chain_operator_role_t;

/*! These names may be used in chain endpoint definitions.
   If used, the chain definition should include this file using
   \verbatim <include_header name="../kymera_chain_roles.h"/>\endverbatim and
   be configured with the attribute \verbatim generate_endpoint_roles_enum="False"\endverbatim.
*/
typedef enum chain_endpoint_roles
{
    /*! The sink of AVDTP media, typically the RTP decoder */
    EPR_SINK_MEDIA = 0x2000,
    /*! The source of decoded PCM, typically the output of a codec decoder */
    EPR_SOURCE_DECODED_PCM,
    /*! The source of decoded PCM, typically the output of a codec decoder on right channel */
    EPR_SOURCE_DECODED_PCM_RIGHT,
    /*! The source of encoded media for forwarding, typically the output of
        a codec encoder or a aptx demultiplexer */
    EPR_SOURCE_FORWARDING_MEDIA,
    /*! The input to SCO receive portion of SCO chain */
    EPR_SCO_FROM_AIR,
    /*! The final output from SCO send portion of SCO chain */
    EPR_SCO_TO_AIR,
    /*! The first, or only, MIC input to echo cancellation of SCO chain */
    EPR_SCO_MIC1,
    /*! The second MIC input to echo cancellation of SCO chain */
    EPR_SCO_MIC2,
    /*! The speaker output from SCO chain */
    EPR_SCO_SPEAKER,

    /*! Output from the tone-generator/prompt chain */
    EPR_TONE_PROMPT_CHAIN_OUT,

    /*! Input to the prompt chain */
    EPR_PROMPT_IN,

    /*! Inputs to the va mic chain */
    EPR_VA_MIC_AEC_IN,
    EPR_VA_MIC_MIC1_IN,
    EPR_VA_MIC_MIC2_IN,

    /*! Outputs to the va mic chain */
    EPR_VA_MIC_WUW_OUT,
    EPR_VA_MIC_ENCODE_OUT,

    /*! Inputs to the va encode chain */
    EPR_VA_ENCODE_IN,

    /*! Output to the va encode chain */
    EPR_VA_ENCODE_OUT,

    /*! Inputs to the AEC reference chain */
    EPR_AEC_INPUT1,
    EPR_AEC_INPUT2,
    EPR_AEC_MIC1_IN,
    EPR_AEC_MIC2_IN,
    EPR_AEC_MIC3_IN,
    EPR_AEC_MIC4_IN,

    /*! Outputs to the AEC reference chain */
    EPR_AEC_REFERENCE_OUT,
    EPR_AEC_SPEAKER1_OUT,
    EPR_AEC_SPEAKER2_OUT,
    EPR_AEC_MIC1_OUT,
    EPR_AEC_MIC2_OUT,
    EPR_AEC_MIC3_OUT,
    EPR_AEC_MIC4_OUT,

    /*! Inputs & Outputs to the mic resampler chain */
    EPR_MIC_RESAMPLER_IN1,
    EPR_MIC_RESAMPLER_IN2,
    EPR_MIC_RESAMPLER_IN3,
    EPR_MIC_RESAMPLER_OUT1,
    EPR_MIC_RESAMPLER_OUT2,
    EPR_MIC_RESAMPLER_OUT3,

    /*! Inputs & Outputs to the aec resampler chain */
    EPR_AEC_RESAMPLER_IN_REF,
    EPR_AEC_RESAMPLER_OUT_REF,

    /*Duplicate outputs for AEC to be used in case of Concurrency with AANC*/
    EPR_AEC_MIC1_DUP_OUT,
    EPR_AEC_MIC2_DUP_OUT,
    EPR_AEC_MIC3_DUP_OUT,
    EPR_AEC_MIC4_DUP_OUT,

    /*! Inputs & outputs to SCO chain */
    EPR_SCO_TO_AANC,
    EPR_SCO_VOL_OUT,
    EPR_CVC_SEND_REF_IN,
    EPR_CVC_SEND_IN1,
    EPR_CVC_SEND_IN2,
    EPR_CVC_SEND_IN3,

    /*! Adaptive ANC Rx Splitter end points */
    EPR_SPLT_SCO_IN,
    EPR_SPLT_AEC_OUT,
    EPR_SPLT_OUT_ANC_VAD,

    /*! Adaptive ANC Tx Splitter end points. Not required with Mic concurrency framework*/
    EPR_AANC_BASIC_PASS_IN,
    EPR_SPLT_OUT_CVC_SEND,
    EPR_SPLT_OUT_ANC_FF,

    /*! Adaptive ANC Re-sampler end points TX path. Not required with Mic concurrency framework*/
    EPR_IIR_16K_IN1,
    EPR_IIR_16K_IN2,
    EPR_IIR_AEC_REF_16K_IN,
    EPR_IIR_8K_OUT1,
    EPR_IIR_8K_OUT2,
    EPR_IIR_AEC_REF_8K_OUT,

    /*! Adaptive ANC Re-sampler end points RX path */
    EPR_IIR_RX_8K_IN1,
    EPR_IIR_RX_16K_OUT1,

    /*! Core Adaptive ANC end points */
    EPR_AANC_VOICE_DECTECTION_IN,
    EPR_AANC_ERR_MIC_IN,
    EPR_AANC_FF_MIC_IN,
    EPR_AANC_CONSUMER,
    EPR_AANC_FBC_FF_MIC_REF_IN,
    EPR_AANC_FBC_FF_MIC_IN,
    EPR_AANC_FBC_FF_MIC_OUT,
    EPR_AANC_FBC_ERR_MIC_REF_IN,
    EPR_AANC_FBC_ERR_MIC_IN,
    EPR_AANC_FBC_ERR_MIC_OUT,
    EPR_SPLT_MIC_REF_IN,
    EPR_SPLT_MIC_REF_OUT1,
    EPR_SPLT_MIC_REF_OUT2,

    /*! Input to the va WuW chain */
    EPR_VA_WUW_IN,

    /*! Endpoints of output chain */
    EPR_SINK_STEREO_MIXER_L,
    EPR_SINK_STEREO_MIXER_R,
    EPR_VOLUME_AUX,
    EPR_SOURCE_STEREO_OUTPUT_L,
    EPR_SOURCE_STEREO_OUTPUT_R,

    /*! Inputs & outputs to music processing chain */
    EPR_MUSIC_PROCESSING_IN_L,
    EPR_MUSIC_PROCESSING_IN_R,
    EPR_MUSIC_PROCESSING_OUT_L,
    EPR_MUSIC_PROCESSING_OUT_R,
    /*! Input of Wired stereo chain */
    EPR_WIRED_STEREO_INPUT_L,
    EPR_WIRED_STEREO_INPUT_R,

    /*! Inputs & outputs to SCO ISO chain */
    EPR_SCO_ISO_FROM_AIR,
    EPR_SCO_ISO_TO_AIR,
    EPR_SCO_ISO_VOL_OUT,

    /*! Inputs & outputs to USB chain */
    EPR_USB_FROM_HOST,
    EPR_USB_TO_HOST,
    EPR_USB_RX_RESAMPLER_OUT,
    EPR_USB_CVC_RECEIVE_IN,

    /*! Inputs to MIC chain */
    EPR_MIC_MIC1_IN,

    /*! outputs to LC3 chain */
    EPR_LC3_ENCODE_ISO_TO_AIR,

    /*! Output to the A2DP Source Chain */
    EPR_SOURCE_ENCODE_OUT,

    /*! Fit test end points */
    EPR_FIT_TEST_INT_MIC_IN,
    EPR_FIT_TEST_PLAYBACK_IN

} chain_endpoint_role_t;

/*! These names are aliases for certain endpoint roles that can be repurposed
*/
typedef enum
{
    /*! Aliases for output chain endpoints */
    EPR_SINK_MIXER_MAIN_IN = EPR_SINK_STEREO_MIXER_L,
    EPR_SOURCE_MIXER_OUT = EPR_SOURCE_STEREO_OUTPUT_L,
} chain_endpoint_role_alias_t;

/*@}*/

#endif /* KYMERA_CHAIN_ROLES_H_ */

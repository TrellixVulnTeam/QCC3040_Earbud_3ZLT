/*!
\copyright  Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Pre and post init audio setup.

*/

#include "earbud_cap_ids.h"

#include "chains/chain_aptx_ad_tws_plus_decoder.h"

#include "chains/chain_input_aac_stereo_mix.h"
#include "chains/chain_input_sbc_stereo_mix.h"
#include "chains/chain_input_aptx_stereo_mix.h"
#include "chains/chain_forwarding_input_aptx_left.h"
#include "chains/chain_forwarding_input_aptx_right.h"

#include "chain_input_aptx_adaptive_stereo_mix.h"
#include "chain_input_aptx_adaptive_stereo_mix_q2q.h"

#include "chains/chain_aec.h"
#include "chains/chain_output_volume_mono.h"
#ifdef INCLUDE_KYMERA_COMPANDER
#include "chains/chain_output_volume_mono_compander.h"
#endif
#include "chains/chain_output_volume_common.h"

#include "chains/chain_tone_gen.h"
#include "chains/chain_prompt_sbc.h"
#include "chains/chain_prompt_pcm.h"

#include "chains/chain_sco_nb.h"
#include "chains/chain_sco_wb.h"
#include "chains/chain_sco_swb.h"

#include "chains/chain_sco_nb_2mic.h"
#include "chains/chain_sco_wb_2mic.h"
#include "chains/chain_sco_swb_2mic.h"
#include "chains/chain_sco_nb_3mic.h"
#include "chains/chain_sco_wb_3mic.h"
#include "chains/chain_sco_swb_3mic.h"

#include "chains/chain_va_encode_msbc.h"
#include "chains/chain_va_encode_opus.h"
#include "chains/chain_va_encode_sbc.h"
#include "chains/chain_va_mic_1mic_cvc.h"
#include "chains/chain_va_mic_1mic_cvc_wuw.h"
#include "chains/chain_va_mic_2mic_cvc.h"
#include "chains/chain_va_mic_2mic_cvc_wuw.h"
#include "chains/chain_va_wuw_qva.h"
#include "chains/chain_va_wuw_gva.h"
#include "chains/chain_va_wuw_apva.h"

#include "chains/chain_music_processing.h"
#ifdef INCLUDE_MUSIC_PROCESSING
        #include "chains/chain_music_processing_user_eq.h"
#endif
#include "chains/chain_aanc.h"
#include "chains/chain_aanc_fbc.h"
#include "chains/chain_aanc_splitter_mic_ref_path.h"

#include "chains/chain_mic_resampler.h"

#include "chains/chain_va_graph_manager.h"
#include "chains/chain_fit_test_mic_path.h"

#include "earbud_setup_audio.h"
#include "source_prediction.h"
#include "kymera.h"
#include <kymera_setup.h>

static const capability_bundle_t capability_bundle[] =
{
#ifdef DOWNLOAD_SWITCHED_PASSTHROUGH
    {
        "download_switched_passthrough_consumer.edkcs",
        capability_bundle_available_p0
    },
#endif
#ifdef DOWNLOAD_APTX_CLASSIC_DEMUX
    {
        "download_aptx_demux.edkcs",
        capability_bundle_available_p0
    },
#endif
#ifdef DOWNLOAD_AEC_REF
    {
#ifdef CORVUS_YD300
        "download_aec_reference.dkcs",
#else
        "download_aec_reference.edkcs",
#endif
        capability_bundle_available_p0
    },
#endif
#ifdef DOWNLOAD_ADAPTIVE_ANC
    {
        "download_aanc.edkcs",
        capability_bundle_available_p0
    },
#endif
#ifdef DOWNLOAD_APTX_ADAPTIVE_DECODE
    {
        "download_aptx_adaptive_decode.edkcs",
        capability_bundle_available_p0
    },
#endif
#if defined(DOWNLOAD_ASYNC_WBS_DEC) || defined(DOWNLOAD_ASYNC_WBS_ENC)
    /*  Chains for SCO forwarding.
        Likely to update to use the downloadable AEC regardless
        as offers better TTP support (synchronisation) and other
        extensions */
    {
        "download_async_wbs.edkcs",
        capability_bundle_available_p0
    },
#endif
#ifdef DOWNLOAD_VOLUME_CONTROL
    {
        "download_volume_control.edkcs",
        capability_bundle_available_p0
    },
#endif
#ifdef DOWNLOAD_OPUS_CELT_ENCODE
    {
        "download_opus_celt_encode.edkcs",
        capability_bundle_available_p0
    },
#endif
#ifdef DOWNLOAD_VA_GRAPH_MANAGER
    {
        "download_va_graph_manager.edkcs",
        capability_bundle_available_p0
    },
#endif
#ifdef DOWNLOAD_CVC_FBC
    {
        "download_cvc_fbc.edkcs",
#ifdef __QCC514X__
        capability_bundle_available_p0_and_p1
#else
        capability_bundle_available_p0
#endif
    },
#endif
#ifdef DOWNLOAD_GVA
    {
#ifdef __QCC305X__
        "download_gva.edkcs",
#else
        "download_gva.dkcs",
#endif
        capability_bundle_available_p0
    },
#endif
#ifdef DOWNLOAD_APVA
    {
#ifdef __QCC305X__
        "download_apva.edkcs",
#else
        "download_apva.dkcs",
#endif
        capability_bundle_available_p0
    },
#endif
#ifdef DOWNLOAD_CVC_3MIC
    {
        "download_cvc_send_internal_mic.dkcs",
        capability_bundle_available_p0
    },
#endif
#ifdef DOWNLOAD_OPUS_CELT_ENCODE
    {
        "download_opus_celt_encode.edkcs",
        capability_bundle_available_p0
    },
#endif
#ifdef DOWNLOAD_LC3_ENCODE_SCO_ISO
    {
        "download_lc3_encode_sco_iso.edkcs",
        capability_bundle_available_p0
    },
#endif
#ifdef DOWNLOAD_LC3_DECODE_SCO_ISO
    {
        "download_lc3_decode_sco_iso.edkcs",
        capability_bundle_available_p0
    },
#endif
#if defined(DOWNLOAD_SWBS_ENC_DEC) || defined(DOWNLOAD_SWBS_DEC) || defined(DOWNLOAD_SWBS_ENC)
    {
        "download_swbs.edkcs",
        capability_bundle_available_p0
    },
#endif
#ifdef DOWNLOAD_EARBUD_FIT_TEST
    {
        "download_earbud_fit_test.edkcs",
        capability_bundle_available_p0
    },
#endif
    {
        0, 0
    }
};

static const capability_bundle_config_t bundle_config = {capability_bundle, ARRAY_DIM(capability_bundle) - 1};

static const kymera_chain_configs_t chain_configs = {
        .chain_aptx_ad_tws_plus_decoder_config = &chain_aptx_ad_tws_plus_decoder_config,
#if defined (INCLUDE_DECODERS_ON_P1)
    .chain_input_aac_stereo_mix_config = &chain_input_aac_stereo_mix_config_p1,
    .chain_input_sbc_stereo_mix_config = &chain_input_sbc_stereo_mix_config_p1,
    .chain_input_aptx_stereo_mix_config = &chain_input_aptx_stereo_mix_config_p1,
    .chain_input_aptx_adaptive_stereo_mix_config = &chain_input_aptx_adaptive_stereo_mix_config_p1,
    .chain_input_aptx_adaptive_stereo_mix_q2q_config = &chain_input_aptx_adaptive_stereo_mix_q2q_config_p1,
    .chain_forwarding_input_aptx_left_config = &chain_forwarding_input_aptx_left_config_p1,
    .chain_forwarding_input_aptx_right_config = &chain_forwarding_input_aptx_right_config_p1,
    .chain_prompt_sbc_config = &chain_prompt_sbc_config_p1,
#else
#ifdef INCLUDE_WUW
#error Wake-up word requires decoders to be on P1
#endif
    .chain_input_aac_stereo_mix_config = &chain_input_aac_stereo_mix_config_p0,
    .chain_input_sbc_stereo_mix_config = &chain_input_sbc_stereo_mix_config_p0,
    .chain_input_aptx_stereo_mix_config = &chain_input_aptx_stereo_mix_config_p0,
    .chain_forwarding_input_aptx_left_config = &chain_forwarding_input_aptx_left_config_p0,
    .chain_forwarding_input_aptx_right_config = &chain_forwarding_input_aptx_right_config_p0,
    .chain_prompt_sbc_config = &chain_prompt_sbc_config_p0,
    .chain_input_aptx_adaptive_stereo_mix_config = &chain_input_aptx_adaptive_stereo_mix_config_p0,
    .chain_input_aptx_adaptive_stereo_mix_q2q_config = &chain_input_aptx_adaptive_stereo_mix_q2q_config_p0,

#endif
    .chain_aec_config = &chain_aec_config,
#ifdef INCLUDE_KYMERA_COMPANDER
    .chain_output_volume_mono_config = &chain_output_volume_mono_compander_config,
#else
    .chain_output_volume_mono_config = &chain_output_volume_mono_config,
#endif
    .chain_output_volume_common_config = &chain_output_volume_common_config,
    .chain_tone_gen_config = &chain_tone_gen_config,
    .chain_prompt_pcm_config = &chain_prompt_pcm_config,
    .chain_aanc_config = &chain_aanc_config,
    .chain_aanc_fbc_config = &chain_aanc_fbc_config,
    .chain_aanc_splitter_mic_ref_path_config = &chain_aanc_splitter_mic_ref_path_config,
#ifdef INCLUDE_SPEAKER_EQ
#ifdef INCLUDE_DECODERS_ON_P1
#ifdef INCLUDE_MUSIC_PROCESSING
    .chain_music_processing_config = &chain_music_processing_user_eq_config_p1,
#else
    .chain_music_processing_config = &chain_music_processing_config_p1,
#endif
#else
#ifdef INCLUDE_MUSIC_PROCESSING
    .chain_music_processing_config = &chain_music_processing_user_eq_config_p0,
#else
    .chain_music_processing_config = &chain_music_processing_config_p0,
#endif
#endif
#endif
    .chain_mic_resampler_config = &chain_mic_resampler_config,
    .chain_va_graph_manager_config = &chain_va_graph_manager_config,
	.chain_fit_test_mic_path_config = &chain_fit_test_mic_path_config,
};

static const kymera_callback_configs_t callback_configs = {
    .GetA2dpParametersPrediction = &SourcePrediction_GetA2dpParametersPrediction,
};

const appKymeraVaEncodeChainInfo va_encode_chain_info[] =
{
    {{va_audio_codec_sbc}, &chain_va_encode_sbc_config},
    {{va_audio_codec_msbc}, &chain_va_encode_msbc_config},
    {{va_audio_codec_opus}, &chain_va_encode_opus_config}
};

static const appKymeraVaEncodeChainTable va_encode_chain_table =
{
    .chain_table = va_encode_chain_info,
    .table_length = ARRAY_DIM(va_encode_chain_info)
};

static const appKymeraVaMicChainInfo va_mic_chain_info[] =
{
  /*{{  WuW,   CVC, mics}, chain_to_use}*/
#ifdef INCLUDE_WUW
    {{ TRUE,  TRUE,    1}, &chain_va_mic_1mic_cvc_wuw_config},
    {{ TRUE,  TRUE,    2}, &chain_va_mic_2mic_cvc_wuw_config},
#endif /* INCLUDE_WUW */
    {{FALSE,  TRUE,    1}, &chain_va_mic_1mic_cvc_config},
    {{FALSE,  TRUE,    2}, &chain_va_mic_2mic_cvc_config}
};

static const appKymeraVaMicChainTable va_mic_chain_table =
{
    .chain_table = va_mic_chain_info,
    .table_length = ARRAY_DIM(va_mic_chain_info)
};

#ifdef INCLUDE_WUW
static const appKymeraVaWuwChainInfo va_wuw_chain_info[] =
{
    {{va_wuw_engine_qva}, &chain_va_wuw_qva_config},
#ifdef INCLUDE_GAA
    {{va_wuw_engine_gva}, &chain_va_wuw_gva_config},
#endif /* INCLUDE_GAA */
#ifdef INCLUDE_AMA
    {{va_wuw_engine_apva}, &chain_va_wuw_apva_config}
#endif /* INCLUDE_AMA */
};

static const appKymeraVaWuwChainTable va_wuw_chain_table =
{
    .chain_table = va_wuw_chain_info,
    .table_length = ARRAY_DIM(va_wuw_chain_info)
};
#endif /* INCLUDE_WUW */

const appKymeraScoChainInfo kymera_sco_chain_table[] =
{
  /* sco_mode mic_cfg   chain                        rate */
  { SCO_NB,   1,        &chain_sco_nb_config,          8000 },
  { SCO_WB,   1,        &chain_sco_wb_config,         16000 },
  { SCO_SWB,  1,        &chain_sco_swb_config,        32000 },

  { SCO_NB,   2,        &chain_sco_nb_2mic_config,     8000 },
  { SCO_WB,   2,        &chain_sco_wb_2mic_config,    16000 },
  { SCO_SWB,  2,        &chain_sco_swb_2mic_config,   32000 },

  { SCO_NB,   3,        &chain_sco_nb_3mic_config,     8000 },
  { SCO_WB,   3,        &chain_sco_wb_3mic_config,    16000 },
  { SCO_SWB,  3,        &chain_sco_swb_3mic_config,   32000 },

  { NO_SCO }
};


const audio_output_config_t audio_hw_output_config =
{

#ifdef ENHANCED_ANC_USE_2ND_DAC_ENDPOINT
    .mapping = {
        {audio_output_type_dac, audio_output_hardware_instance_0, audio_output_channel_a },
        {audio_output_type_dac, audio_output_hardware_instance_0, audio_output_channel_b }
    },
#else
    .mapping = {
        {audio_output_type_dac, audio_output_hardware_instance_0, audio_output_channel_a }
    },
#endif
    .gain_type = {audio_output_gain_none, audio_output_gain_none},
    .output_resolution_mode = audio_output_24_bit,
    .fixed_hw_gain = 0
};

void Earbud_SetBundlesConfig(void)
{
    Kymera_SetBundleConfig(&bundle_config);
}

void Earbud_SetupAudio(void)
{
    Kymera_SetChainConfigs(&chain_configs);

    Kymera_SetScoChainTable(kymera_sco_chain_table);
    Kymera_SetVaMicChainTable(&va_mic_chain_table);
    Kymera_SetVaEncodeChainTable(&va_encode_chain_table);
#ifdef INCLUDE_WUW
    Kymera_SetVaWuwChainTable(&va_wuw_chain_table);
    Kymera_StoreLargestWuwEngine();
#endif /* INCLUDE_WUW */
    AudioOutputInit(&audio_hw_output_config);
    Kymera_SetCallbackConfigs(&callback_configs);
}

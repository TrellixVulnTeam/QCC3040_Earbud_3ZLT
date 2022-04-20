/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    kymera
\brief      Chain configure callbacks

Provided callbacks will be triggered between configuration and connection of the corresponding chain.

*/

#ifndef KYMERA_CHAIN_CONFIG_CALLBACKS_H_
#define KYMERA_CHAIN_CONFIG_CALLBACKS_H_

#include <kymera.h>

/*@{*/

typedef struct
{
    uint8 seid;
    uint32 sample_rate;
    uint32 max_bitrate;
    aptx_adaptive_ttp_latencies_t nq2q_ttp;
} kymera_a2dp_config_params_t;

typedef struct
{
    uint32 sample_rate;
    appKymeraScoMode mode;
    uint16 wesco;
} kymera_sco_config_params_t;

typedef struct
{
    uint32 sample_rate;
} kymera_common_config_params_t;

typedef kymera_common_config_params_t kymera_wired_config_params_t;

typedef struct
{
    uint32 sample_rate;
    uint8 sample_size;
    uint8 number_of_channels;
} kymera_usb_common_config_params_t;

typedef kymera_usb_common_config_params_t kymera_usb_audio_config_params_t;
typedef kymera_usb_common_config_params_t kymera_usb_voice_rx_config_params_t;
typedef kymera_usb_common_config_params_t kymera_usb_voice_tx_config_params_t;

typedef kymera_common_config_params_t kymera_output_config_params_t;

typedef struct
{
    uint32 sample_rate;
    bool is_tone;
    promptFormat prompt_format;
} kymera_tone_prompt_config_params_t;

typedef kymera_common_config_params_t kymera_music_processing_config_params_t;

typedef struct
{
    uint32 spk_sample_rate;
    uint32 mic_sample_rate;
} kymera_aec_config_params_t;

typedef struct
{
    uint32 input_sample_rate;
    uint32 output_sample_rate;
} kymera_mic_resmapler_config_params_t;

typedef kymera_common_config_params_t kymera_va_mic_config_params_t;

typedef struct
{
    void (*ConfigureA2dpInputChain)(kymera_chain_handle_t chain_handle, kymera_a2dp_config_params_t *params);
    void (*ConfigureScoInputChain)(kymera_chain_handle_t chain_handle, kymera_sco_config_params_t *params);
    void (*ConfigureWiredInputChain)(kymera_chain_handle_t chain_handle, kymera_wired_config_params_t *params);
    void (*ConfigureUsbAudioInputChain)(kymera_chain_handle_t chain_handle, kymera_usb_audio_config_params_t *params);
    void (*ConfigureUsbVoiceRxChain)(kymera_chain_handle_t chain_handle, kymera_usb_voice_rx_config_params_t *params);
    void (*ConfigureUsbVoiceTxChain)(kymera_chain_handle_t chain_handle, kymera_usb_voice_tx_config_params_t *params);
    void (*ConfigureOutputChain)(kymera_chain_handle_t chain_handle, kymera_output_config_params_t *params);
    void (*ConfigureTonePromptChain)(kymera_chain_handle_t chain_handle, kymera_tone_prompt_config_params_t *params);
    void (*ConfigureMusicProcessingChain)(kymera_chain_handle_t chain_handle, kymera_music_processing_config_params_t *params);
    void (*ConfigureAdaptiveAncChain)(kymera_chain_handle_t chain_handle);
    void (*ConfigureAdaptiveAncTuningChain)(kymera_chain_handle_t chain_handle);
    void (*ConfigureAecChain)(kymera_chain_handle_t chain_handle, kymera_aec_config_params_t *params);
    void (*ConfigureMicResamplerChain)(kymera_chain_handle_t chain_handle, kymera_mic_resmapler_config_params_t *params);
    void (*ConfigureVaMicChain)(kymera_chain_handle_t chain_handle, kymera_va_mic_config_params_t *params);
    void (*ConfigureWuwChain)(kymera_chain_handle_t chain_handle);
    void (*ConfigureGraphManagerChain)(kymera_chain_handle_t chain_handle);
} kymera_chain_config_callbacks_t;

/*! \brief Register structure of chain configure callbacks

    Individual function pointers should be set to NULL if not defined.

    \param callbacks Pointer to the structure of callbacks.
*/
void Kymera_RegisterConfigCallbacks(kymera_chain_config_callbacks_t *callbacks);

/*@}*/

#endif /* KYMERA_CHAIN_CONFIG_CALLBACKS_H_ */

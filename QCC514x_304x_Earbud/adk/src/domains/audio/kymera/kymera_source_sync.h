/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\ingroup    kymera
\file
\brief      Private (internal) kymera header file for source sync capability.
*/

#ifndef KYMERA_SOURCE_SYNC_H
#define KYMERA_SOURCE_SYNC_H

#include "kymera_output_chain_config.h"
#include "chain.h"

/*! The maximum size block of PCM samples produced by the decoder */
#define DEFAULT_CODEC_BLOCK_SIZE      (256)
#define SBC_CODEC_BLOCK_SIZE          (384)
#define AAC_CODEC_BLOCK_SIZE          (1024)
#define APTX_CODEC_BLOCK_SIZE         (512)

/*! \brief Calculate and set in config the source sync input buffer size in samples.
    \param config The kick period and rate must be set.
    \param codec_block_size The maximum size block of PCM samples produced by the decoder per kick.
    \note This calculation is suitable for chains where any bulk latency is upstream
    of the decoder and the buffer between the decoder and the source sync is only
    required to hold sufficient samples to contain the codec processing block size.
*/
void appKymeraSetSourceSyncConfigInputBufferSize(kymera_output_chain_config *config, unsigned codec_block_size);

/*! \brief Calculate and set in config the source sync output buffer size in samples.
    \param config The kick period and rate must be set.
    \param kp_multiply The mulitple factor of the kick period.
    \param kp_divide The division factor of the kick period.
    \note The calculation is (kick_period * multiply) / divide microseconds
    converted to number of samples (at the defined rate).
*/
void appKymeraSetSourceSyncConfigOutputBufferSize(kymera_output_chain_config *config, unsigned kp_multiply, unsigned kp_divide);

/*! \brief Get the default source sync period corresponding to slow kick
    \param is_max_period flag to indicate if its for maximum source sync period or not.
*/
standard_param_value_t appKymeraGetSlowKickSourceSyncPeriod( bool is_max_period);

/*! \brief Get the default source sync period corresponding to fast kick
    \param is_max_period flag to indicate if its for maximum source sync period or not.
*/
standard_param_value_t appKymeraGetFastKickSourceSyncPeriod( bool is_max_period);

/*! \brief Configure the source sync operator.
    \param chain The audio chain containing the source sync operator.
    \param config The audio output chain configuration.
    \param set_input_buffer Select whether the source sync input terminal buffer will be set.
    \param is_stereo Select the configuration data based on mono or stereo.
*/
void appKymeraConfigureSourceSync(kymera_chain_handle_t chain, const kymera_output_chain_config *config, bool set_input_buffer, bool is_stereo);

/*! \brief Set the source sync mono route gain.
    \param chain The audio chain containing the source sync operator.
    \param sample_rate The chain's sample rate.
    \param transition_samples The number of samples over which the gain will be transitioned.
    \param gain_in_db The new gain to apply.
*/
void appKymeraSourceSyncSetMonoRouteGain(kymera_chain_handle_t chain, uint32 sample_rate, uint32 transition_samples, int16 gain_in_db);

#endif

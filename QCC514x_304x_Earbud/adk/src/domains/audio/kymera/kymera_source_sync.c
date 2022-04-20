/*!
\copyright  Copyright (c) 2017-2020  Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       kymera_source_sync.c
\brief      Kymera source sync configuration.
*/

#include <stdlib.h>
#include "kymera_source_sync.h"
#include "kymera_common.h"
#include "kymera_volume.h"
#include "kymera_kick_period_config.h"
#include "kymera_chain_roles.h"
#include <logging.h>

/*! Convert a channel ID to a bit mask */
#define CHANNEL_TO_MASK(channel) ((uint32)1 << channel)

/*!@{ \name Port numbers for the Source Sync operator */
#define KYMERA_SOURCE_SYNC_INPUT_PORT (0)
#define KYMERA_SOURCE_SYNC_OUTPUT_PORT (0)

#define KYMERA_SOURCE_SYNC_INPUT_PORT_1 (1)
#define KYMERA_SOURCE_SYNC_OUTPUT_PORT_1 (1)
/*!@} */

/* Minimum source sync version that supports setting input terminal buffer size */
#define SET_TERMINAL_BUFFER_SIZE_MIN_VERSION 0x00030004
/* Minimum source sync version that supports setting kick back threshold */
#define SET_KICK_BACK_THRESHOLD_MIN_VERSION 0x00030004

/*! Helper macro to get size of fixed arrays to populate structures */
#define DIMENSION_AND_ADDR_OF(ARRAY) ARRAY_DIM((ARRAY)), (ARRAY)

/*! The default source sync minimum and maximum periods for slow kicks */
static const standard_param_value_t sosy_min_period_kp_7_5_value = FRACTIONAL(1000.0/KICK_PERIOD_SLOW);
static const standard_param_value_t sosy_max_period_kp_7_5_value = MILLISECONDS_Q6_26((1000.0+KICK_PERIOD_SLOW)/KICK_PERIOD_SLOW);

/*! The default source sync minimum and maximum periods for fast kicks */
static const standard_param_value_t sosy_min_period_kp_2_0_value = FRACTIONAL(500.0/KICK_PERIOD_FAST);
static const standard_param_value_t sosy_max_period_kp_2_0_value = MILLISECONDS_Q6_26((500.0+KICK_PERIOD_FAST)/KICK_PERIOD_FAST);

/* Configuration of source sync groups and routes */
static const source_sync_sink_group_t mono_sink_groups[] =
{
    {
        .meta_data_required = TRUE,
        .rate_match = FALSE,
        .channel_mask = CHANNEL_TO_MASK(KYMERA_SOURCE_SYNC_INPUT_PORT)
    }
};

static const source_sync_source_group_t mono_source_groups[] =
{
    {
        .meta_data_required = TRUE,
        .ttp_required = TRUE,
        .channel_mask = CHANNEL_TO_MASK(KYMERA_SOURCE_SYNC_OUTPUT_PORT)
    }
};

static source_sync_route_t mono_route[] =
{
    {
        .input_terminal = KYMERA_SOURCE_SYNC_INPUT_PORT,
        .output_terminal = KYMERA_SOURCE_SYNC_OUTPUT_PORT,
        .transition_samples = 0,
        .sample_rate = 0, /* Overridden later */
        .gain = 0
    }
};

#if defined(INCLUDE_STEREO)
static const source_sync_sink_group_t stereo_sink_groups[] =
{
    {
        .meta_data_required = TRUE,
        .rate_match = FALSE,
        .channel_mask = CHANNEL_TO_MASK(KYMERA_SOURCE_SYNC_INPUT_PORT)  | CHANNEL_TO_MASK(KYMERA_SOURCE_SYNC_INPUT_PORT_1)
    }

};

static const source_sync_source_group_t stereo_source_groups[] =
{
    {
        .meta_data_required = TRUE,
        .ttp_required = TRUE,
        .channel_mask = CHANNEL_TO_MASK(KYMERA_SOURCE_SYNC_OUTPUT_PORT) |CHANNEL_TO_MASK(KYMERA_SOURCE_SYNC_OUTPUT_PORT_1)
    }
};

static source_sync_route_t stereo_route[] =
{
    {
        .input_terminal = KYMERA_SOURCE_SYNC_INPUT_PORT,
        .output_terminal = KYMERA_SOURCE_SYNC_OUTPUT_PORT,
        .transition_samples = 0,
        .sample_rate = 0, /* Overridden later */
        .gain = 0
    }
    ,
    {
        .input_terminal = KYMERA_SOURCE_SYNC_INPUT_PORT_1,
        .output_terminal = KYMERA_SOURCE_SYNC_OUTPUT_PORT_1,
        .transition_samples = 0,
        .sample_rate = 0, /* Overridden later */
        .gain = 0
    }
};
#endif

static const standard_param_id_t sosy_min_period_id = 0;
static const standard_param_id_t sosy_max_period_id = 1;

static void appKymeraSetSourceSyncParameter(Operator op, standard_param_id_t id, standard_param_value_t value)
{
    set_params_data_t* set_params_data = OperatorsCreateSetParamsData(1);

    set_params_data->number_of_params = 1;
    set_params_data->standard_params[0].id = id;
    set_params_data->standard_params[0].value = value;

    OperatorsStandardSetParameters(op, set_params_data);
    free(set_params_data);
}

void appKymeraSetSourceSyncConfigInputBufferSize(kymera_output_chain_config *config, unsigned codec_block_size)
{
    /* This is the buffer size for a single kick period time. */
    unsigned unit_buffer_size = US_TO_BUFFER_SIZE_MONO_PCM(config->kick_period, config->rate);
    /* Note the +1 is due to Source Sync input quirk */
    config->source_sync_input_buffer_size_samples = unit_buffer_size + codec_block_size + 1;
}

void appKymeraSetSourceSyncConfigOutputBufferSize(kymera_output_chain_config *config,
                                                  unsigned kp_multiply, unsigned kp_divide)
{
    unsigned output_buffer_size_us = config->kick_period * kp_multiply;
    if (kp_divide > 1)
    {
        output_buffer_size_us /= kp_divide;
    }
    config->source_sync_output_buffer_size_samples = US_TO_BUFFER_SIZE_MONO_PCM(output_buffer_size_us, config->rate);
}

standard_param_value_t appKymeraGetSlowKickSourceSyncPeriod( bool is_max_period)
{
    return (is_max_period ? sosy_max_period_kp_7_5_value : sosy_min_period_kp_7_5_value);
}

standard_param_value_t appKymeraGetFastKickSourceSyncPeriod( bool is_max_period)
{
    return (is_max_period ? sosy_max_period_kp_2_0_value : sosy_min_period_kp_2_0_value);
}

void appKymeraConfigureSourceSync(kymera_chain_handle_t chain,
                                  const kymera_output_chain_config *config,
                                  bool set_input_buffer,
                                  bool is_stereo)
{
    Operator op;

    if (GET_OP_FROM_CHAIN(op, chain, OPR_SOURCE_SYNC))
    {
        capablity_version_t version_bits;
        uint32 version;


        /* Send operator configuration messages */
        OperatorsStandardSetSampleRate(op, config->rate);

        if(!is_stereo)
        {
            /* Override sample rate in routes config */
            mono_route[0].sample_rate = config->rate;

            OperatorsSourceSyncSetSinkGroups(op, DIMENSION_AND_ADDR_OF(mono_sink_groups));
            OperatorsSourceSyncSetSourceGroups(op, DIMENSION_AND_ADDR_OF(mono_source_groups));
            OperatorsSourceSyncSetRoutes(op, DIMENSION_AND_ADDR_OF(mono_route));

        }

        else
        {
            #if defined(INCLUDE_STEREO)
            /* Override sample rate in routes config */
            stereo_route[0].sample_rate = config->rate;
            stereo_route[1].sample_rate = config->rate;

            OperatorsSourceSyncSetSinkGroups(op, DIMENSION_AND_ADDR_OF(stereo_sink_groups));
            OperatorsSourceSyncSetSourceGroups(op, DIMENSION_AND_ADDR_OF(stereo_source_groups));
            OperatorsSourceSyncSetRoutes(op, DIMENSION_AND_ADDR_OF(stereo_route));
            #else
             Panic();
            #endif
        }

        OperatorsStandardSetBufferSize(op, config->source_sync_output_buffer_size_samples);

        version_bits = OperatorGetCapabilityVersion(op);
        version = UINT32_BUILD(version_bits.version_msb, version_bits.version_lsb);

        if (set_input_buffer)
        {
            if (version >= SET_TERMINAL_BUFFER_SIZE_MIN_VERSION)
            {
                /* SourceSync can set its input buffer size as a latency buffer. */
                OperatorsStandardSetTerminalBufferSize(op,
                    config->source_sync_input_buffer_size_samples, 0xFFFF, 0);
            }
            else
            {
                DEBUG_LOG_ERROR("appKymeraConfigureSourceSync version 0x%x cannot set term buf size", version);
            }
        }
        if (config->set_source_sync_max_period)
        {
            appKymeraSetSourceSyncParameter(op, sosy_max_period_id, config->source_sync_max_period);
        }
        if (config->set_source_sync_min_period)
        {
            appKymeraSetSourceSyncParameter(op, sosy_min_period_id, config->source_sync_min_period);
        }
        if (config->set_source_sync_kick_back_threshold)
        {
            if (version >= SET_KICK_BACK_THRESHOLD_MIN_VERSION)
            {
                OperatorsStandardSetBackKickThreshold(op, -(int)config->source_sync_kick_back_threshold,
                                                      common_back_kick_mode_level,
                                                      (unsigned)-1);
            }
            else
            {
                DEBUG_LOG_ERROR("appKymeraConfigureSourceSync version 0x%x cannot set kick back threshold", version);
            }
        }
    }
}

void appKymeraSourceSyncSetMonoRouteGain(kymera_chain_handle_t chain, uint32 sample_rate, uint32 transition_samples, int16 gain_in_db)
{
    Operator op;

    if (GET_OP_FROM_CHAIN(op, chain, OPR_SOURCE_SYNC))
    {
        source_sync_route_t route = mono_route[0];
        route.sample_rate = sample_rate;
        route.transition_samples = transition_samples;
        route.gain = Kymera_VolDbToGain(gain_in_db);
        OperatorsSourceSyncSetRoutes(op, 1, &route);
    }
}

/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\ingroup    kymera
\file
\brief      Private (internal) kymera header file defining the output chain
            configuration parameters.
*/

#ifndef KYMERA_OUTPUT_CHAIN_CONFIG_H
#define KYMERA_OUTPUT_CHAIN_CONFIG_H

#include "csrtypes.h"

/*! \brief List of chain types supported as an output chain */
typedef enum
{
    output_chain_mono,
    output_chain_stereo,
    output_chain_common,
} output_chain_t;

/*! \brief Output chain configuration parameters */
typedef struct
{
    /*! The output sample rate */
    uint32 rate;
    /*! The source sync minimum period */
    uint32 source_sync_min_period;
    /*! The source sync maximum period */
    uint32 source_sync_max_period;
    /*! The source sync kick back threshold */
    uint32 source_sync_kick_back_threshold;
    /*! The system kick period */
    uint16 kick_period;
    /*! The size of the buffer at the input of the source sync in samples.

        If the source sync version is >3.3, the source sync capability supports
        setting of its input terminal buffer size, so a separate basic
        passthrough operator is not required before the source sync to set a
        buffer size.

        For source sync version <=3.3, if a buffer is required before the source
        sync, a basic passthrough operator may be added.

        If this parameter is non-zero, when configuring the output chain, if a
        basic passthrough operator is found, its buffer size will be set to the
        value of this parameter and the source sync input terminal buffer
        size will not be set. If no basic passthrough operator is found the
        buffer size will be in the source sync terminal buffer size. This will
        fail if the source sync version is <=3.3. */
    uint16 source_sync_input_buffer_size_samples;
    /*! The size of the buffer at the output of the source sync in samples */
    uint16 source_sync_output_buffer_size_samples;
    /*! Used to map to the respective output audio chain definition */
    output_chain_t chain_type;
    /*! Whether to set the source sync minimum period. */
    unsigned set_source_sync_min_period : 1;
    /*! Whether to set the source sync maximum period. */
    unsigned set_source_sync_max_period : 1;
    /*! Whether to set the source sync maximum period. */
    unsigned set_source_sync_kick_back_threshold : 1;
    /*! Indicate that AEC Reference should be included in output chain. */
    unsigned chain_include_aec : 1;
} kymera_output_chain_config;

#endif

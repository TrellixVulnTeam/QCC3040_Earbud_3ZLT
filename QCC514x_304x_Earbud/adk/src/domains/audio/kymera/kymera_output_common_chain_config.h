/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\brief      Configuration used for the fixed output chain
*/

#ifndef KYMERA_OUTPUT_COMMON_CHAIN_CONFIG_H
#define KYMERA_OUTPUT_COMMON_CHAIN_CONFIG_H

#include "kymera_output_chain_config.h"

/*! \brief Get the configuration for the common output chain.
    \return The config for the common output chain if the feature is enabled, NULL otherwise.
*/
const kymera_output_chain_config * Kymera_OutputCommonChainGetConfig(void);

#endif // KYMERA_OUTPUT_COMMON_CHAIN_CONFIG_H

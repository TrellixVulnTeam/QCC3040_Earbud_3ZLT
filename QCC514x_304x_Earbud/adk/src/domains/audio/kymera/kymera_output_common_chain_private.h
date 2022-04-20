/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\brief      Configuration used for the fixed output chain
*/

#ifndef KYMERA_OUTPUT_COMMON_CHAIN_PRIVATE_H
#define KYMERA_OUTPUT_COMMON_CHAIN_PRIVATE_H

/*! \brief Enable the output common chain feature.
*/
void Kymera_OutputCommonChainConfigEnable(void);

/*! \brief Disable the output common chain feature. If there are active users it will cause a Panic.
*/
void Kymera_OutputCommonChainConfigDisable(void);

#endif // KYMERA_OUTPUT_COMMON_CHAIN_PRIVATE_H

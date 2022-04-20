/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\brief      Configuration used for the fixed output chain
*/

#ifndef KYMERA_OUTPUT_COMMON_CHAIN_H
#define KYMERA_OUTPUT_COMMON_CHAIN_H

/*! \brief Prepare and create the common output chain.
*/
void Kymera_OutputCommonChainPrepare(void);

/*! \brief Undo preparation of the common output chain, if there are no users chain will be destroyed.
*/
void Kymera_OutputCommonChainUndoPrepare(void);

/*! \brief Enable the output common chain feature.
*/
void Kymera_OutputCommonChainEnable(void);

/*! \brief Disable the output common chain feature. If there are active users it will cause a Panic.
*/
void Kymera_OutputCommonChainDisable(void);

#endif // KYMERA_OUTPUT_COMMON_CHAIN_H

/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       fast_pair_config.h
\brief      Configuration for Fast Pair Module.
*/

#ifndef FAST_PAIR_CONFIG_H_
#define FAST_PAIR_CONFIG_H_

#define FAST_PAIR_CONFIG_ASPK_LEN 32
#define FAST_PAIR_CONFIG_MODEL_ID_LEN 4 /* Model ID read from PS should be of even length. */

/*! \brief Get Fast Pair Model ID
 */
uint32 fastPair_GetModelId(void);

/*! \brief Set Fast Pair Model ID from PS
 */
void fastPair_SetModelId(const uint8* model_id);

#endif /* FAST_PAIR_CONFIG_H_ */

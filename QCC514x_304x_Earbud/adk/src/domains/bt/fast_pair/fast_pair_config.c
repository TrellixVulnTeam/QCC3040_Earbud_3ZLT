/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       fast_pair_config.c
\brief      Configuration file for Fast Pair Model ID.

*/
#include <panic.h>

#include "fast_pair.h"
#include "fast_pair_config.h"

uint32 fast_pair_model_id;

/*! \brief Get the Fast Pair Model ID
 */
uint32 fastPair_GetModelId(void)
{
    DEBUG_LOG("fastPair_GetModelId : %04x", fast_pair_model_id);
    return fast_pair_model_id;
}

/*! \brief Set the Fast Pair Model ID from PS
 */
void fastPair_SetModelId(const uint8* model_id)
{
    PanicNull((uint8 *)model_id);
    fast_pair_model_id = (model_id[2] << 16 & 0xFF0000) | (model_id[1] << 8 & 0xFF00) | (model_id[0] & 0xFF);
}

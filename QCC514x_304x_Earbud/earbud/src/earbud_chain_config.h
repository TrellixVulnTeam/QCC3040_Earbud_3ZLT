/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Configuration of application chains
*/

#ifndef EARBUD_CHAIN_CONFIG_H_
#define EARBUD_CHAIN_CONFIG_H_

#include "earbud_cap_ids.h"

#if CAP_ID_VA_CVC_1MIC == EB_CAP_ID_CVC_FBC
#define CVC_1MIC_VA_OUTPUT_TERMINAL 0
#else
#define CVC_1MIC_VA_OUTPUT_TERMINAL 1
#endif

#endif // EARBUD_CHAIN_CONFIG_H_

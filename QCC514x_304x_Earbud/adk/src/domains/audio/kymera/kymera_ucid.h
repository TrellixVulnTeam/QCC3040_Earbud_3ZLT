/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    kymera
\brief      List of UCIDs.

*/

#ifndef KYMERA_UCID_H_
#define KYMERA_UCID_H_

#include "kymera_chain_roles.h"
#include <chain.h>

/*@{*/

/*! \brief UCIDs for operator configuration*/
typedef enum kymera_operator_ucids
{
    UCID_AEC_DEFAULT_LT_DISABLED = 5,
    UCID_AEC_DEFAULT_LT_ENABLED = 33,
    UCID_AEC_8_KHZ  = 0,
    UCID_AEC_16_KHZ = 1,
    UCID_AEC_32_KHZ = 4,
    UCID_AEC_44_1_KHZ = 35,
    UCID_AEC_48_KHZ = 3,
    UCID_AEC_8_KHZ_LT_MODE_1 = 6,
    UCID_AEC_8_KHZ_LT_MODE_2 = 7,
    UCID_AEC_8_KHZ_LT_MODE_3 = 8,
    UCID_AEC_16_KHZ_LT_MODE_1 = 12,
    UCID_AEC_16_KHZ_LT_MODE_2 = 13,
    UCID_AEC_16_KHZ_LT_MODE_3 = 14,
    UCID_AEC_32_KHZ_LT_MODE_1 = 18,
    UCID_AEC_32_KHZ_LT_MODE_2 = 19,
    UCID_AEC_32_KHZ_LT_MODE_3 = 20,
    UCID_AEC_44_1_KHZ_LT_MODE_1 = 21,
    UCID_AEC_44_1_KHZ_LT_MODE_2 = 22,
    UCID_AEC_44_1_KHZ_LT_MODE_3 = 23,
    UCID_AEC_48_KHZ_LT_MODE_1 = 24,
    UCID_AEC_48_KHZ_LT_MODE_2 = 25,
    UCID_AEC_48_KHZ_LT_MODE_3 = 26,
    UCID_CVC_SEND    = 0,
    UCID_CVC_SEND_VA = 1,
    UCID_CVC_RECEIVE = 0,
    UCID_CVC_RECEIVE_EQ = 1,
    UCID_VOLUME_CONTROL = 0,
    UCID_SOURCE_SYNC = 0,
    UCID_PASS_ADD_HEADROOM = 0,
    UCID_PASS_REMOVE_HEADROOM = 1,
    UCID_PASS_VA = 2,
    UCID_SPEAKER_EQ = 0,
    UCID_USER_EQ_USER = 1,
    UCID_ANC_TUNING = 0,
    UCID_ADAPTIVE_ANC = 0,
    UCID_ADAPTIVE_ANC_FBC = 0,
    UCID_EFT = 0,
    UCID_COMPANDER_LIMITER = 1
} kymera_operator_ucid_t;

/*! \brief Set the UCID for a single operator */
bool Kymera_SetOperatorUcid(kymera_chain_handle_t chain, chain_operator_role_t role, kymera_operator_ucid_t ucid);

/*! \brief Set UCIDs for the Voice specific operators */
void Kymera_SetVoiceUcids(kymera_chain_handle_t chain);

/*@}*/

#endif /* KYMERA_UCID_H_ */

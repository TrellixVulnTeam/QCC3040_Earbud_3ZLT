/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    kymera
\brief      Function to set UCID

*/

#include "kymera_ucid.h"
#include "kymera_common.h"

bool Kymera_SetOperatorUcid(kymera_chain_handle_t chain, chain_operator_role_t role, kymera_operator_ucid_t ucid)
{
    Operator op;
    if (GET_OP_FROM_CHAIN(op, chain, role))
    {
        OperatorsStandardSetUCID(op, ucid);
        return TRUE;
    }
    return FALSE;
}

void Kymera_SetVoiceUcids(kymera_chain_handle_t chain)
{
    /* SCO/MIC forwarding RX chains do not have CVC send or receive */
    Kymera_SetOperatorUcid(chain, OPR_CVC_SEND, UCID_CVC_SEND);
    #ifdef INCLUDE_SPEAKER_EQ
        Kymera_SetOperatorUcid(chain, OPR_CVC_RECEIVE, UCID_CVC_RECEIVE_EQ);
    #else
        Kymera_SetOperatorUcid(chain, OPR_CVC_RECEIVE, UCID_CVC_RECEIVE);
    #endif
}

/*!
    \copyright Copyright (c) 2022 Qualcomm Technologies International, Ltd.
        All Rights Reserved.
        Qualcomm Technologies International, Ltd. Confidential and Proprietary.
    \file chain_va_mic_2mic_cvc_wuw.c
    \brief The chain_va_mic_2mic_cvc_wuw chain.

    This file is generated by C:\Qualcomm_Prog\qcc514x-qcc304x-src-1-0_qtil_standard_oem_earbud-ADK-21.1-CS2-MR1\adk\tools\packages\chaingen\chaingen_mod\__init__.pyc.
*/

#include <chain_va_mic_2mic_cvc_wuw.h>
#include <cap_id_prim.h>
#include <opmsg_prim.h>
#include <hydra_macros.h>
#include <../earbud_chain_config.h>
#include <kymera_chain_roles.h>
static const operator_config_t operators[] =
{
    MAKE_OPERATOR_CONFIG(CAP_ID_VAD, OPR_VAD),
    MAKE_OPERATOR_CONFIG(CAP_ID_SPLITTER, OPR_SPLITTER),
    MAKE_OPERATOR_CONFIG(CAP_ID_BASIC_PASS, OPR_MIC_GAIN),
    MAKE_OPERATOR_CONFIG(CAP_ID_VA_CVC_2MIC, OPR_CVC_SEND),
} ;

static const operator_endpoint_t inputs[] =
{
    {OPR_CVC_SEND, EPR_VA_MIC_AEC_IN, 0},
    {OPR_MIC_GAIN, EPR_VA_MIC_MIC1_IN, 0},
    {OPR_MIC_GAIN, EPR_VA_MIC_MIC2_IN, 1},
} ;

static const operator_endpoint_t outputs[] =
{
    {OPR_SPLITTER, EPR_VA_MIC_WUW_OUT, 0},
    {OPR_SPLITTER, EPR_VA_MIC_ENCODE_OUT, 1},
} ;

static const operator_connection_t connections[] =
{
    {OPR_MIC_GAIN, 0, OPR_CVC_SEND, 1, 1},
    {OPR_MIC_GAIN, 1, OPR_CVC_SEND, 2, 1},
    {OPR_CVC_SEND, 1, OPR_VAD, 0, 1},
    {OPR_VAD, 0, OPR_SPLITTER, 0, 1},
} ;

const chain_config_t chain_va_mic_2mic_cvc_wuw_config = {0, 0, operators, 4, inputs, 3, outputs, 2, connections, 4};


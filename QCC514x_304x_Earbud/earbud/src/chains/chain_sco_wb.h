/*!
    \copyright Copyright (c) 2022 Qualcomm Technologies International, Ltd.
        All Rights Reserved.
        Qualcomm Technologies International, Ltd. Confidential and Proprietary.
    \file chain_sco_wb.h
    \brief The chain_sco_wb chain.

    This file is generated by C:\Qualcomm_Prog\qcc514x-qcc304x-src-1-0_qtil_standard_oem_earbud-ADK-21.1-CS2-MR1\adk\tools\packages\chaingen\chaingen_mod\__init__.pyc.
*/

#ifndef _CHAIN_SCO_WB_H__
#define _CHAIN_SCO_WB_H__

/*!
\page chain_sco_wb
    @startuml
        object OPR_SCO_RECEIVE
        OPR_SCO_RECEIVE : id = CAP_ID_WBS_DEC
        object OPR_SCO_SEND
        OPR_SCO_SEND : id = CAP_ID_WBS_ENC
        object OPR_CVC_RECEIVE
        OPR_CVC_RECEIVE : id = CAP_ID_CVC_RECEIVE_WB
        object OPR_CVC_SEND
        OPR_CVC_SEND : id = CAP_ID_CVCHS1MIC_SEND_WB
        OPR_CVC_RECEIVE "IN(0)"<-- "OUT(0)" OPR_SCO_RECEIVE
        OPR_SCO_SEND "IN(0)"<-- "OUT(0)" OPR_CVC_SEND
        object EPR_SCO_FROM_AIR #lightgreen
        OPR_SCO_RECEIVE "SCO_IN(0)" <-- EPR_SCO_FROM_AIR
        object EPR_CVC_SEND_REF_IN #lightgreen
        OPR_CVC_SEND "REFERENCE(0)" <-- EPR_CVC_SEND_REF_IN
        object EPR_CVC_SEND_IN1 #lightgreen
        OPR_CVC_SEND "IN1(1)" <-- EPR_CVC_SEND_IN1
        object EPR_SCO_SPEAKER #lightblue
        EPR_SCO_SPEAKER <-- "OUT(0)" OPR_CVC_RECEIVE
        object EPR_SCO_TO_AIR #lightblue
        EPR_SCO_TO_AIR <-- "SCO_OUT(0)" OPR_SCO_SEND
    @enduml
*/

#include <chain.h>

extern const chain_config_t chain_sco_wb_config;

#endif /* _CHAIN_SCO_WB_H__ */


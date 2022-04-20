/*!
    \copyright Copyright (c) 2022 Qualcomm Technologies International, Ltd.
        All Rights Reserved.
        Qualcomm Technologies International, Ltd. Confidential and Proprietary.
    \file chain_aanc_splitter_mic_ref_path.h
    \brief The chain_aanc_splitter_mic_ref_path chain.

    This file is generated by C:\Qualcomm_Prog\qcc514x-qcc304x-src-1-0_qtil_standard_oem_earbud-ADK-21.1-CS2-MR1\adk\tools\packages\chaingen\chaingen_mod\__init__.pyc.
*/

#ifndef _CHAIN_AANC_SPLITTER_MIC_REF_PATH_H__
#define _CHAIN_AANC_SPLITTER_MIC_REF_PATH_H__

/*!
\page chain_aanc_splitter_mic_ref_path
    @startuml
        object OPR_AANC_SPLT_MIC_REF_PATH
        OPR_AANC_SPLT_MIC_REF_PATH : id = CAP_ID_SPLITTER
        object EPR_SPLT_MIC_REF_IN #lightgreen
        OPR_AANC_SPLT_MIC_REF_PATH "IN(0)" <-- EPR_SPLT_MIC_REF_IN
        object EPR_SPLT_MIC_REF_OUT1 #lightblue
        EPR_SPLT_MIC_REF_OUT1 <-- "OUT1(0)" OPR_AANC_SPLT_MIC_REF_PATH
        object EPR_SPLT_MIC_REF_OUT2 #lightblue
        EPR_SPLT_MIC_REF_OUT2 <-- "OUT2(1)" OPR_AANC_SPLT_MIC_REF_PATH
    @enduml
*/

#include <chain.h>

extern const chain_config_t chain_aanc_splitter_mic_ref_path_config;

#endif /* _CHAIN_AANC_SPLITTER_MIC_REF_PATH_H__ */

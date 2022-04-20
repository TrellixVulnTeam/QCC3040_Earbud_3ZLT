/*!
    \copyright Copyright (c) 2022 Qualcomm Technologies International, Ltd.
        All Rights Reserved.
        Qualcomm Technologies International, Ltd. Confidential and Proprietary.
    \file chain_prompt_pcm.h
    \brief The chain_prompt_pcm chain.

    This file is generated by C:\Qualcomm_Prog\qcc514x-qcc304x-src-1-0_qtil_standard_oem_earbud-ADK-21.1-CS2-MR1\adk\tools\packages\chaingen\chaingen_mod\__init__.pyc.
*/

#ifndef _CHAIN_PROMPT_PCM_H__
#define _CHAIN_PROMPT_PCM_H__

/*!
\page chain_prompt_pcm
    @startuml
        object OPR_TONE_PROMPT_PCM_BUFFER
        OPR_TONE_PROMPT_PCM_BUFFER : id = CAP_ID_BASIC_PASS
        object EPR_PROMPT_IN #lightgreen
        OPR_TONE_PROMPT_PCM_BUFFER "IN(0)" <-- EPR_PROMPT_IN
        object EPR_TONE_PROMPT_CHAIN_OUT #lightblue
        EPR_TONE_PROMPT_CHAIN_OUT <-- "OUT(0)" OPR_TONE_PROMPT_PCM_BUFFER
    @enduml
*/

#include <chain.h>

extern const chain_config_t chain_prompt_pcm_config;

#endif /* _CHAIN_PROMPT_PCM_H__ */


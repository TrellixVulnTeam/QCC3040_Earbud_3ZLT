/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       adk_test_cvc_demo.c
\brief      Implementation of specifc application testing functions
*/

#ifndef DISABLE_TEST_API

#include <logging.h>
#include <ui.h>
#include <logical_input_switch.h>

#include "kymera.h"
#include "adk_test_cvc_demo.h"

#ifdef GC_SECTIONS
/* Move all functions in KEEP_PM section to ensure they are not removed during
 * garbage collection */
#pragma unitcodesection KEEP_PM
#endif

#ifdef INCLUDE_CVC_DEMO
void appTestSetCvcSendPassthrough(bool passthrough, uint8 passthrough_mic)
{
    kymera_cvc_mode_t mode = ((passthrough == TRUE) ? KYMERA_CVC_SEND_PASSTHROUGH : KYMERA_CVC_SEND_FULL_PROCESSING);
    Kymera_ScoSetCvcPassthroughMode(mode, passthrough_mic);
}

void appTestGetCvcSendPassthrough(void)
{
    kymera_cvc_mode_t mode;
    uint8 passthrough_mic;
    Kymera_ScoGetCvcPassthroughMode(&mode, &passthrough_mic);
    if((mode & KYMERA_CVC_SEND_FULL_PROCESSING) == KYMERA_CVC_SEND_FULL_PROCESSING)
    {
        DEBUG_LOG_ALWAYS("appTestGetCvcSendPassthrough: mode: Full processing");
    }
    else if((mode & KYMERA_CVC_SEND_PASSTHROUGH) == KYMERA_CVC_SEND_PASSTHROUGH)
    {
        DEBUG_LOG_ALWAYS("appTestGetCvcSendPassthrough: mode: Passthrough, mic %d", passthrough_mic);
    }
    else
    {
        DEBUG_LOG_ALWAYS("appTestGetCvcSendPassthrough: Not yet set");
    }
}

void appTestSetCvcSendMicConfig(uint8 mic_config)
{
    Kymera_ScoSetCvcSend3MicMicConfig(mic_config);
}

uint8 appTestGetCvcSendMicConfig(void)
{
    uint8 mic_config;
    Kymera_ScoGetCvcSend3MicMicConfig(&mic_config);
    DEBUG_LOG_ALWAYS("appTestGetCvcSend3MicMicConfig: mic_config %d", mic_config);
    return mic_config;
}

uint8 appTestGetCvcSend3MicMode(void)
{
    uint8 mic_mode;
    Kymera_ScoGetCvcSend3MicModeOfOperation(&mic_mode);
    DEBUG_LOG_ALWAYS("appTestGetCvcSend3MicMode: mode %d", mic_mode);
    return mic_mode;
}
#endif /* INCLUDE_CVC_DEMO */
#endif /* DISABLE_TEST_API */

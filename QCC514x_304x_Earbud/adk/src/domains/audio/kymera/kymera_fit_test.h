/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera_fit_test.h
\brief      Private header to connect/manage to Earbud fit test audio graph
*/

#ifndef KYMERA_FIT_TEST_H_
#define KYMERA_FIT_TEST_H_

#include <operators.h>

/*! \brief Registers AANC callbacks in the mic interface layer
*/
#if defined(ENABLE_EARBUD_FIT_TEST)
void KymeraFitTest_Init(void);
#else
#define KymeraFitTest_Init() ((void)(0))
#endif

#if defined(ENABLE_EARBUD_FIT_TEST)
void KymeraFitTest_Start(void);
#else
#define KymeraFitTest_Start() ((void)(0))
#endif

#if defined(ENABLE_EARBUD_FIT_TEST)
void KymeraFitTest_Stop(void);
#else
#define KymeraFitTest_Stop() ((void)(0))
#endif

#if defined(ENABLE_EARBUD_FIT_TEST)
void KymeraFitTest_CancelPrompt(void);
#else
#define KymeraFitTest_CancelPrompt() ((void)(0))
#endif

#if defined(ENABLE_EARBUD_FIT_TEST)
FILE_INDEX KymeraFitTest_GetPromptIndex(void);
#else
#define KymeraFitTest_GetPromptIndex() (0)
#endif

#if defined(ENABLE_EARBUD_FIT_TEST)
void KymeraFitTest_ResetDspPowerMode(void);
#else
#define KymeraFitTest_ResetDspPowerMode() ((void)0)
#endif

#if defined(ENABLE_EARBUD_FIT_TEST)
bool KymeraFitTest_PromptReplayRequired(void);
#else
#define KymeraFitTest_PromptReplayRequired() (FALSE)
#endif

#if defined(ENABLE_EARBUD_FIT_TEST)
void KymeraFitTest_ReplayPrompt(void);
#else
#define KymeraFitTest_ReplayPrompt() ((void)0)
#endif

#if defined(ENABLE_EARBUD_FIT_TEST)
bool KymeraFitTest_IsTuningModeActive(void);
#else
#define KymeraFitTest_IsTuningModeActive() (FALSE)
#endif

#endif /* KYMERA_FIT_TEST_H_ */

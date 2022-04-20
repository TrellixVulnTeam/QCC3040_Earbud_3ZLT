/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    adk_test_common
\brief      Interface for voice assistant application testing functions
*/

/*! @{ */

#ifndef ADK_TEST_VA_H
#define ADK_TEST_VA_H

void appTestVaTap(void);
void appTestVaDoubleTap(void);
void appTestVaPressAndHold(void);
void appTestVaRelease(void);
void appTestVaHeldRelease(void);
void appTestSetActiveVa2GAA(void);
void appTestSetActiveVa2AMA(void);
bool appTestIsVaAudioActive(void);
unsigned appTest_VaGetSelectedAssistant(void);
#ifdef INCLUDE_AMA
void appTestPrintAmaLocale(void);
void appTestSetAmaLocale(const char *locale);
#endif
bool appTestIsVaDeviceInSniff(void);

#endif /* ADK_TEST_VA_H */

/*! @} */

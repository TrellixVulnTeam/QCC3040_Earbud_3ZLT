/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       adk_test_cvc_demo.h
\brief      Interface for specifc application testing functions
*/

#ifndef ADK_TEST_CVC_DEMO_H
#define ADK_TEST_CVC_DEMO_H

#ifdef INCLUDE_CVC_DEMO
/*! \brief Set 3Mic cVc Send mode (passthrough / full processing) and select the passthrough microphone */
void appTestSetCvcSendPassthrough(bool passthrough, uint8 passthrough_mic);
/*! \brief Get the 3Mic cVc Send mode */
void appTestGetCvcSendPassthrough(void);
/*! \brief Set 3Mic cVc Send microphone configuration */
void appTestSetCvcSendMicConfig(uint8 mic_config);
/*! \brief Get the 3Mic cVc Send microphone configuration */
uint8 appTestGetCvcSendMicConfig(void);
/*! \brief Get the 3Mic cVc Send internal mode: 2mic = FALSE, 3mic = TRUE */
uint8 appTestGetCvcSend3MicMode(void);
#endif /* INCLUDE_CVC_DEMO */

#endif /* ADK_TEST_CVC_DEMO_H */


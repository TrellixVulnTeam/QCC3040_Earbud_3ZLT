/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       fit_test.h
\brief      Wear detect implementation header file.
*/

#ifndef FIT_TEST_H_
#define FIT_TEST_H_
#include <hydra_macros.h>
#include <message.h>
#include <logging.h>
#include "domain_message.h"

/*! \brief The Fit test module final result states. */
typedef enum
{
    /*! Bad fit and Good ambient */
    fit_test_result_bad,
    /*! Good fit and Good ambient */
    fit_test_result_good,
    /*! Bad ambient */
    fit_test_result_error,
} fit_test_result_t;

typedef struct
{
    /* The info variable is  interpreted according to message that it is delievered to clients.
        */
    uint8 left_earbud_result;
    uint8 right_earbud_result;
} fit_test_event_msg_t;

typedef fit_test_event_msg_t FIT_TEST_RESULT_IND_T;

typedef enum
{
    FIT_TEST_RESULT_IND = FIT_TEST_MESSAGE_BASE,
    FIT_TEST_RUNNING,
    FIT_TEST_ABORTED
}fit_test_msg_t;

/*! \brief Fit test Initialisation function.
 *  gets Called during the init phase.
    \param Fit-test task.
 */
#if defined(ENABLE_EARBUD_FIT_TEST)
bool FitTest_init(Task init_task);
#else
#define FitTest_init(x) (FALSE)
#endif

/*! \brief returns the local device fit test result.
 *  \returns fit test result - good/bad/error.
 */
#if defined(ENABLE_EARBUD_FIT_TEST)
fit_test_result_t FitTest_GetLocalDeviceTestResult(void);
#else
#define FitTest_GetLocalDeviceTestResult() (fit_test_result_bad)
#endif

/*! \brief returns the remote device fit test result.
 *  \returns fit test result - good/bad/error.
 */
#if defined(ENABLE_EARBUD_FIT_TEST)
fit_test_result_t FitTest_GetRemoteDeviceTestResult(void);
#else
#define FitTest_GetRemoteDeviceTestResult() (fit_test_result_bad)
#endif

/*! \brief This API is used by peer ui domain to store the value
 *  of remote device fit test result.
 *  \param fit test result - good/bad/error.(typecasted to fit_test_result_t)
 */
#if defined(ENABLE_EARBUD_FIT_TEST)
void FitTest_StoreRemotePeerResults(uint8 result);
#else
#define FitTest_StoreRemotePeerResults(x) ((void)(0 * (x)))
#endif

/*! \brief This API is used to prepare the fit test. The state machine transit to Ready state after successful
 * execution of this.
 *  \param None
 *  \return Status indicating success or failure of the API execution.
 */
#if defined(ENABLE_EARBUD_FIT_TEST)
bool FitTest_MakeTestReady(void);
#else
#define FitTest_MakeTestReady() (FALSE)
#endif

/*! \brief This API tries to start the fit test. The state machine must be in a ready state for successful execution.
 *  \param None
 *  \return Status indicating success or failure of the API. Most possible reason for this API failure could be due to
 *         calling this API in wrong state.
 */
#if defined(ENABLE_EARBUD_FIT_TEST)
bool FitTest_StartTest(void);
#else
#define FitTest_StartTest() (FALSE)
#endif

/*! \brief This API tries to cancel already running fit test.
 *  \param None
 *  \return Status indicating success or failure of API.
 */
#if defined(ENABLE_EARBUD_FIT_TEST)
bool FitTest_AbortTest(void);
#else
#define FitTest_AbortTest() (FALSE)
#endif

/*! \brief This API disables the testing of fit test and requires application to re-prepare for fit-test using FitTest_MakeTestReady()
 *         before running another instance of the fit test.
 *  \param None
 *  \return Status indicating the success or failure of API.
 */
#if defined(ENABLE_EARBUD_FIT_TEST)
bool FitTest_DisableTest(void);
#else
#define FitTest_DisableTest() (FALSE)
#endif

#if defined(ENABLE_EARBUD_FIT_TEST)
void FitTest_ClientRegister(Task client_task);
#else
#define FitTest_ClientRegister(x) ((void)(0*(x)))
#endif

#if defined(ENABLE_EARBUD_FIT_TEST)
void FitTest_ClientUnRegister(Task client_task);
#else
#define FitTest_ClientUnRegister(x) ((void)(0*(x)))
#endif

#if defined(ENABLE_EARBUD_FIT_TEST)
bool FitTest_IsReady(void);
#else
#define FitTest_IsReady() (FALSE)
#endif

#if defined(ENABLE_EARBUD_FIT_TEST)
bool FitTest_IsRunning(void);
#else
#define FitTest_IsRunning() (FALSE)
#endif

#if defined(ENABLE_EARBUD_FIT_TEST)
bool FitTest_InformClients(void);
#else
#define FitTest_InformClients() (FALSE)
#endif

/*!
    \brief To identifty if fit test prompt needs to be replayed.
 *  \return TRUE if replay is needed, else FALSE.
 */
#if defined(ENABLE_EARBUD_FIT_TEST)
bool FitTest_PromptReplayRequired(void);
#else
#define FitTest_PromptReplayRequired() (FALSE)
#endif

/*!
    \brief Enters Fit Test tuning mode.
 *  \return Status indicating the success or failure of API.
 */
#if defined(ENABLE_EARBUD_FIT_TEST)
bool FitTest_EnterFitTestTuningMode(void);
#else
#define FitTest_EnterFitTestTuningMode() (FALSE)
#endif

/*!
    \brief Exits Fit Test tuning mode.
 *  \return Status indicating the success or failure of API.
 */
#if defined(ENABLE_EARBUD_FIT_TEST)
bool FitTest_ExitFitTestTuningMode(void);
#else
#define FitTest_ExitFitTestTuningMode() (FALSE)
#endif

/*!
    \brief Checks whether Fit Test tuning mode is currently active.
    \return TRUE if it is active, else FALSE.
 */
#if defined(ENABLE_EARBUD_FIT_TEST)
bool FitTest_IsTuningModeActive(void);
#else
#define FitTest_IsTuningModeActive() (FALSE)
#endif


#endif /* FIT_TEST_H_ */

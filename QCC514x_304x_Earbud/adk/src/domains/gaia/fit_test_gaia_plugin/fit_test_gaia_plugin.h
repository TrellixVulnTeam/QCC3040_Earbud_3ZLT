/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for the gaia fit test framework plugin
*/

#ifndef FIT_TEST_GAIA_PLUGIN_H_
#define FIT_TEST_GAIA_PLUGIN_H_
#include <gaia_features.h>
#include <gaia_framework.h>
#include <fit_test.h>


#define FIT_TEST_GAIA_PLUGIN_VERSION 1

#define FIT_TEST_GAIA_START_STOP_COMMAND_PAYLOAD_LENGTH          0x01
#define FIT_TEST_GAIA_START_TEST                                 0x01
#define FIT_TEST_GAIA_STOP_TEST                                  0x00

#define FIT_TEST_GAIA_TEST_RESULT_NOTIFICATION_PAYLOAD_LENGTH    0x02
#define FIT_TEST_GAIA_TEST_RESULT_LEFT_OFFSET                    0x00
#define FIT_TEST_GAIA_TEST_RESULT_RIGHT_OFFSET                   0x01

typedef enum
{
    fit_test_gaia_start_stop_command=0,
    /*! Total number of commands.*/
    number_of_fit_test_commands
}fit_test_gaia_plugin_command_ids_t;

typedef enum
{
    fit_test_result_available_notification=0,
    /*! Total number of notifications.*/
    number_of_fit_test_notifications
}fit_test_gaia_plugin_notification_ids_t;

#if defined(ENABLE_EARBUD_FIT_TEST)
void FitTestGaiaPlugin_Init(void);
#else
#define FitTestGaiaPlugin_Init() ((void)(0))
#endif

#endif /* FIT_TEST_GAIA_PLUGIN_H_*/

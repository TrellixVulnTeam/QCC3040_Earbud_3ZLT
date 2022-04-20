/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       anc_gaia_plugin_private.h
\brief      Internal header for gaia fit test framework plugin
*/
#ifndef FIT_TEST_GAIA_PLUGIN_PRIVATE_H_
#define FIT_TEST_GAIA_PLUGIN_PRIVATE_H_
#include <gaia_framework.h>

/*! \brief Fit test GAIA plugin data. */
typedef struct
{
    /*! State Proxy task */
    TaskData task;

} fit_test_gaia_plugin_task_data_t;

fit_test_gaia_plugin_task_data_t fit_test_gaia_plugin_data;

#define fitTestGaiaPlugin_GetTaskData()                  (&fit_test_gaia_plugin_data)

#endif/*FIT_TEST_GAIA_PLUGIN_PRIVATE_H_*/

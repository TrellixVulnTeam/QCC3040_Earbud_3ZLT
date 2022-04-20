/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Provides the user-defined feature data (i.e. Application Feature List),
            which can be read with GAIA 'Get User Feature' (& 'Next') commands from
            the mobile-app.
*/

#ifdef INCLUDE_GAIA
#ifdef ENABLE_GAIA_USER_FEATURE_LIST_DATA
#include "earbud_gaia_user_feature_config.h"

#include <logging.h>
#include <gaia_core_plugin.h>



#if !defined(DEFAULT_APP_FEATURE_LIST)
#define DEFAULT_APP_FEATURE_LIST    0   /* Make sure that there's at least one initialiser. */
#endif

/*! \brief User-defined Application Feature List that can be read by the mobile
           app with GAIA 'Get User Feature' and 'Get User Feature Next' commands.
    \note  This list can be defined with the initialiser that comprises an array
           of text strings (e.g. "ANC", "Touch Sensor v2") or UTF-8 string such
           as "\x33\x2D\x4D\x69\x63" (= "3-Mic").
*/
static const char *application_feature_list_data[] =
{
    /*
     * Put your Application Feature List here.
     */

    DEFAULT_APP_FEATURE_LIST
};

static const gaia_user_defined_feature_data_t usr_feature_list =
{
    .type           = gaia_user_feature_type_app_feature_list,
    .num_of_strings = sizeof(application_feature_list_data) / sizeof(char*),
    .string_list    = application_feature_list_data,
    .next           = NULL,
};


bool EarbudGaiaUserFeature_RegisterUserFeatureData(Task task)
{
    UNUSED(task);

    DEBUG_LOG_INFO("EarbudGaiaUserFeature_RegisterUserFeatureData");

    /* Register the callback function that provides the Application Feature List data. */
    GaiaCorePlugin_RegisterGetUserFeatureData(&usr_feature_list);

    return TRUE;
}


#endif /* ENABLE_GAIA_USER_FEATURE_LIST_DATA */
#endif /* INCLUDE_GAIA */

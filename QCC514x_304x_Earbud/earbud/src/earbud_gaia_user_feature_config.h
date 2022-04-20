/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Provides the user-defined feature data (i.e. Application Feature List),
            which can be read with GAIA 'Get User Feature' (& 'Next') commands from
            the mobile-app.
*/


#ifndef EARBUD_GAIA_USER_FEATURE_H
#define EARBUD_GAIA_USER_FEATURE_H

#ifdef INCLUDE_GAIA
#ifdef ENABLE_GAIA_USER_FEATURE_LIST_DATA
#include <message.h>


/*! \brief Register the user-defined feature data for GAIA 'Get User Feature'
           and 'Get User Feature Next' commands.

    \param task The init task

    \return TRUE if feature initialisation was successful, otherwise FALSE.
*/
bool EarbudGaiaUserFeature_RegisterUserFeatureData(Task task);

#endif /* ENABLE_GAIA_USER_FEATURE_LIST_DATA */
#endif /* INCLUDE_GAIA */
#endif /* EARBUD_GAIA_USER_FEATURE_H */

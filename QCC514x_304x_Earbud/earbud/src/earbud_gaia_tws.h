/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Provides TWS support for the earbud gaia plugin
*/


#ifndef EARBUD_GAIA_TWS_H
#define EARBUD_GAIA_TWS_H

#ifdef INCLUDE_GAIA

/*! Reconnection delay set to 6s as the worse case scenario measured was just over 5s */
#define EarbudGaiaTws_MobileAppReconnectionDelay() (6)

/*! \brief Initialise tws functionality in the plugin

    \param task The init task

    \return TRUE if feature initialisation was successful, otherwise FALSE.
*/
bool EarbudGaiaTws_Init(Task task);

#endif /* INCLUDE_GAIA */

#endif /* EARBUD_GAIA_TWS_H */

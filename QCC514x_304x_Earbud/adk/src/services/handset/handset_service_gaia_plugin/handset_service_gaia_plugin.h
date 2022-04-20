/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for the handset service gaia framework plugin
*/

#ifndef HANDSET_SERVICE_GAIA_PLUGIN_H_
#define HANDSET_SERVICE_GAIA_PLUGIN_H_


#include <gaia_features.h>

#include <gaia_framework.h>


/*! \brief Handset service gaia plugin version
*/
#define HANDSET_SERVICE_GAIA_PLUGIN_VERSION 1


/*! \brief These are the handset service commands provided by the GAIA framework
*/
typedef enum
{
    /*! Command to enable or disable multipoint */
    enable_multipoint = 0,
    /*! Total number of commands */
    number_of_handset_service_commands,
} handset_service_gaia_plugin_pdu_ids_t;

/*! \brief These are the handset service notifications provided by the GAIA framework
*/
typedef enum
{
    /*! Gaia Client will be told if multipoint is enabled or not */
    multipoint_enabled_changed = 0,
    /*! Total number of notifications */
    number_of_handset_service_notifications,
} handset_service_gaia_plugin_notifications_t;


/*! \brief Handset service plugin init function
*/
bool HandsetServicegGaiaPlugin_Init(Task init_task);

/*! \brief Public notification API for enabling and disabling multipoint

    \param multipoint_enabled    Indicator of the multipoint state
*/
void HandsetServicegGaiaPlugin_MultipointEnabledChanged(bool multipoint_enabled);


#endif /* HANDSET_SERVICE_GAIA_PLUGIN_H_ */

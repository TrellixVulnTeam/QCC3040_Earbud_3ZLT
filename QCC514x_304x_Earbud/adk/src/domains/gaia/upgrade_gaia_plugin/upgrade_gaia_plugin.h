/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for the upgrade gaia framework plugin
*/

#ifndef UPGRADE_GAIA_PLUGIN_H_
#define UPGRADE_GAIA_PLUGIN_H_

#if defined(INCLUDE_GAIA) && defined(INCLUDE_DFU)

#include <gaia_features.h>

#include <gaia_framework.h>


/*! \brief Upgrade gaia plugin version
*/
#define UPGRADE_GAIA_PLUGIN_VERSION 2


/*! \brief These are the upgrade commands provided by the GAIA framework
*/
typedef enum
{
    /*! Connects a GAIA transport to the upgrade library (GAIA_COMMAND_VM_UPGRADE_CONNECT) */
    upgrade_connect = 0,
    /*! Disconnects a GAIA transport from the upgrade library (GAIA_COMMAND_VM_UPGRADE_DISCONNECT) */
    upgrade_disconnect,
    /*! Tunnels a VM Upgrade Protocol command to the upgrade library (GAIA_COMMAND_VM_UPGRADE_CONTROL) */
    upgrade_control,
    /*! Returns the data endpoint which is set */
    get_data_endpoint,
    /*! Sets the data endpoint to be used */
    set_data_endpoint,
    /*! Total number of commands */
    number_of_upgrade_commands,
} upgrade_gaia_plugin_pdu_ids_t;

/*! \brief These are the core notifications provided by the GAIA framework
*/
typedef enum
{
    /*! Data indication notification */
    upgrade_data_indication = 0,
    /*! Request to stop unpgrade */
    upgrade_stop_request = 1,
    /*! Reqeust to (re)start upgrade */
    upgrade_start_request = 2,
    /*! Total number of notifications */
    number_of_upgrade_notifications,
} upgrade_gaia_plugin_notifications_t;

enum
{
    /*! Stop or start DFU */
    upgrade_stop_start_action,

    /*! Pause or resume DFU */
    upgrade_pause_resume_action
};

/*! \brief Gaia core plugin init function
*/
void UpgradeGaiaPlugin_Init(void);

/*! \brief Check if handset transfer is active for given tp_bd_addr
*/
bool UpgradeGaiaPlugin_IsHandsetTransferActive(const tp_bdaddr *tp_bd_addr);

#endif /* defined(INCLUDE_GAIA) && defined(INCLUDE_DFU) */
#endif /* UPGRADE_GAIA_PLUGIN_H_ */

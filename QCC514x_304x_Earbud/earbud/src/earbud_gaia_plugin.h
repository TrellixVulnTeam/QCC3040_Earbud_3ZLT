/*!
\copyright  Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for the gaia framework earbud plugin
*/

#ifndef EARBUD_GAIA_PLUGIN_H_
#define EARBUD_GAIA_PLUGIN_H_

#ifdef INCLUDE_GAIA
#include <gaia_features.h>
#include <gaia_framework.h>
#include <tws_topology.h>


/*! \brief Gaia earbud plugin version
*/
#define EARBUD_GAIA_PLUGIN_VERSION (3)

/*! \brief These are the earbud commands provided by the GAIA framework
*/
typedef enum
{
    /*! Finds out if the primary earbud is left or right */
    is_primary_left_or_right = 0,
    /*! Gets the serial number of the secondary earbud */
    get_secondary_serial_number = 1,
    /*! Total number of commands */
    number_of_earbud_commands,
} earbud_plugin_pdu_ids_t;

/*! \brief These are the core notifications provided by the GAIA framework
*/
typedef enum
{
    /*! The device can generate a Notification when a handover happens */
    primary_earbud_about_to_change = 0,
    /*! The device has changed from secondary role to primary role */
    primary_earbud_changed = 1,
    /*! The device has connected to or disconnected from from secondary */
    secondary_earbud_connection_state = 2,
} earbud_plugin_notifications_t;


/*! \brief These are the handover types
*/
typedef enum
{
    /*! Static handover */
    static_handover = 0,
    /*! Dynamic handover */
    dynamic_handover
} earbud_plugin_handover_types_t;


/*! \brief The secondary connection status
*/
typedef enum
{
    /*! The secondary earbud has disconnected from the primary */
    gaia_earbud_secondary_disconnected = 0,
    /*! The secondary earbud has connected to the primary */
    gaia_earbud_secondary_connected = 1
} gaia_earbud_secondary_state_t;


/*! \brief Peer request codes
*/
typedef enum
{
    /*! Request the serial number of the secondary earbud */
    peer_req_get_serial_number
} earbud_plugin_peer_req_t;

/*! \brief Peer request status
*/
typedef enum
{
    /*! The request completed successfully */
    peer_req_success,
    /*! The request could not be completed */
    peer_req_failure,
    /*! The request completed but the returned data has been truncated */
    peer_req_truncated
} earbud_plugin_peer_req_status_t;

/*! \brief Gaia earbud plugin init function
*/
void EarbudGaiaPlugin_Init(void);

/*! \brief Gaia earbud primary about to change notification function
    \param delay            Delay in seconds
*/
void EarbudGaiaPlugin_PrimaryAboutToChange(uint8 delay);

void EarbudGaiaPlugin_RoleChanged(tws_topology_role role);
#endif /* INCLUDE_GAIA */

#endif /* EARBUD_GAIA_PLUGIN_H_ */

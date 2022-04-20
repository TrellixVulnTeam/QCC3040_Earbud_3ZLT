/*!
\copyright  Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for the gaia framework core plugin
*/

#ifndef GAIA_CORE_PLUGIN_H_
#define GAIA_CORE_PLUGIN_H_

#include <gaia_features.h>

#include "gaia_framework.h"


/*! \brief Gaia core plugin version
*/
#define GAIA_CORE_PLUGIN_VERSION (4)


/*! \brief These are the built-in commands provided by the GAIA framework
*/
typedef enum
{
    /*! Get the Gaia protocol version number */
    get_api_version = 0,
    /*! Get the list of features the device supports */
    get_supported_features = 1,
    /*! Get the list continuation of features the device supports */
    get_supported_features_next = 2,
    /*! Get the customer provided serial number for this device */
    get_serial_number = 3,
    /*! Get the customer provided variant name */
    get_variant = 4,
    /*! Get the customer provided application version number */
    get_application_version = 5,
    /*! The mobile app can cause a device to warm reset using this command */
    device_reset = 6,
    /*! The mobile application can register to receive all the notifications from a Feature */
    register_notification = 7,
    /*! The mobile application can unregister to stop receiving feature notifications */
    unregister_notification = 8,
    /*! Set up a data transfer over one of several transports.*/
    data_transfer_setup,
    /*! The mobile app can get data bytes from the device as a command response. */
    data_transfer_get,
    /*! The mobile app can send data bytes to the device on the command payloads. */
    data_transfer_set,
    /*! Get transport information */
    get_transport_info = 12,
    /*! Set transport parameter */
    set_transport_parameter = 13,
    /*! Get user-defined feature data (e.g. Application Feature List). */
    get_user_feature = 14,
    /*! Get user-defined feature data that does not fit into the response PDU of 'Get User Feature' command. */
    get_user_feature_next = 15,
    /*! Get the BR/EDR Bluetooth device address, if any.  In a TWS scenario this will be the primary address. */
    get_device_bluetooth_address = 16,
    /*! Total number of commands */
    number_of_core_commands,
} core_plugin_pdu_ids_t;

/*! \brief These are the core notifications provided by the GAIA framework
*/
typedef enum
{
    /*! The device can generate a Notification when the charger is plugged in or unplugged */
    charger_status_notification = 0,
    /*! Total number of notifications */
    number_of_core_notifications,
} core_plugin_notifications_t;

/*! \brief User-defined Feature-Type.
*/
typedef enum
{
    gaia_user_feature_type_start_from_zero   = 0,
    gaia_user_feature_type_app_feature_list  = 1,

    number_of_gaia_user_feature_type
} gaia_user_feature_type_t;

/*! \brief User-defined feature data.
*/
typedef struct __gaia_user_defined_feature_data_t
{
    /*! The Feature-Type of the user-defined feature list. */
    gaia_user_feature_type_t                            type;
    /*! The number of strings (i.e. feature-describing texts) in the list. */
    uint16                                              num_of_strings;
    /*! Pointer to a user-defined feature list. */
    const char                                        **string_list;
    /*! Pointer to another user-defined feature data descriptor.  */
    const struct __gaia_user_defined_feature_data_t    *next;

} gaia_user_defined_feature_data_t;

/*! \brief Reading status parameters for 'Get User Feature' and 'Get User Feature Next' responses.
*/
typedef struct
{
    bool                        more_data;
    gaia_user_feature_type_t    feature_type;
    uint16                      next_offset;
    uint16                      buf_used;

} gaia_get_user_feature_reading_status_t;


/*! \brief Gaia core plugin init function
*/
void GaiaCorePlugin_Init(void);

/*! \brief Register the user-defined feature data provided by the Application,
           which can be read from the mobile app with the GAIA Get User Feature
           (Next) commands.

    \param[IN] data_ptr Pointer to a descriptor struct that holds a pointer to
               the user-defined feature list with its type and size.
*/
void GaiaCorePlugin_RegisterGetUserFeatureData(const gaia_user_defined_feature_data_t *data_ptr);

#endif /* GAIA_CORE_PLUGIN_H_ */

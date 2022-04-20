/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for the gaia framework API
*/

#ifndef GAIA_FRAMEWORK_H_
#define GAIA_FRAMEWORK_H_

#include <gaia.h>
#include <gaia_features.h>

#include "gaia_framework_internal.h"

/*! GAIA major version number */
#define GAIA_V3_VERSION_MAJOR 3

/*! GAIA minor version number */
#define GAIA_V3_VERSION_MINOR 1

/*! \brief These are the default error codes provided by the GAIA framework
*/
typedef enum
{
    /*! An invalid Feature ID was specified */
    feature_not_supported = 0,
    /*! An invalid PDU Specific ID was specified */
    command_not_supported,
    /*! The host is not authenticated to use a Command ID or control */
    failed_not_authenticated,
    /*! The command was valid, but the device could not successfully carry out the command */
    failed_insufficient_resources,
    /*! The device is in the process of authenticating the host */
    authenticating,
    /*! An invalid parameter was used in the command */
    invalid_parameter,
    /*! The device is not in the correct state to process the command */
    incorrect_state,
    /*! The command is in progress */
    in_progress
} gaia_default_error_codes_t;


typedef enum
{
    /*! Command was handled successfully */
    command_handled = 0,

    /*! Command is pending */
    command_pending,

    /*! Command was not handled */
    command_not_handled,

    /*! Feature not handled */
    feature_not_handled
} gaia_framework_command_status_t;

extern const handover_interface gaia_handover_if;

typedef struct
{
    gaia_framework_command_status_t (*command_handler)(GAIA_TRANSPORT *t, uint8 pdu_id, uint16 payload_length, const uint8 *payload);
    void (*transport_connect)(GAIA_TRANSPORT *t);
    void (*transport_disconnect)(GAIA_TRANSPORT *t);
    void (*send_all_notifications)(GAIA_TRANSPORT *t);
    bool (*handover_veto)(GAIA_TRANSPORT *t);
    void (*handover_abort)(GAIA_TRANSPORT *t);
    void (*handover_complete)(GAIA_TRANSPORT *t, bool is_primary);

    void (*role_change_start)(GAIA_TRANSPORT *t);
    void (*role_change_completed)(GAIA_TRANSPORT *t, bool is_primary);
    void (*role_change_cancelled)(GAIA_TRANSPORT *t);
} gaia_framework_plugin_functions_t;


/*! \brief Function pointer definition for a vendor specific command handler

    \param transport    Transport packet arrived on
    \param vendor_id    Vendor ID of the command
    \param feature_id   Feature ID of the command
    \param pdu_type     PDU type
    \param length       PDU specific ID
    \param payload_size Payload size
    \param payload      Payload data
*/
typedef void (*gaia_framework_vendor_command_handler_fn_t)(GAIA_TRANSPORT *transport, uint16 vendor_id, gaia_features_t feature_id, uint8 pdu_type, uint8 pdu_specific_id, uint16 payload_size, const uint8 *payload);


/*! \brief Initialisation function for the framework

    \param  init_task Init task
    \return True if successful
*/
bool GaiaFramework_Init(Task init_task);

/*! \brief Registers a new feature to the framework

    \param  feature_id          Feature ID of the plugin to be registered
    \param  version_number      Version number of the plugin to be registered
    \param  functions           Function table of the plugin to be registered
*/
void GaiaFramework_RegisterFeature(gaia_features_t feature_id, uint8 version_number,
                                   const gaia_framework_plugin_functions_t *functions);

/*! \brief Registers a vendor specific command handler

    \param  command_handler     Vendor ID specific command handler
*/
void GaiaFramework_RegisterVendorCommandHandler(gaia_framework_vendor_command_handler_fn_t command_handler);

/*! \brief Creates a response for a vendor command to be sent to the mobile application

    \param transport   Transport to send response on
    \param vendor_id   Vendor id from the command
    \param feature_id  Feature id from the command
    \param pdu_id      PDU specific ID for the message
    \param length      Length of the payload
    \param payload     Payload data
*/
void GaiaFramework_SendVendorResponse(GAIA_TRANSPORT *t, uint16 vendor_id, uint8 feature_id, uint8 pdu_id, uint16 length, const uint8 *payload);

/*! \brief Creates a error reply for a command to be sent to the mobile application

    \param transport   Transport to send response on
    \param vendor_id   Vendor id from the command
    \param feature_id  Feature id from the command
    \param pdu_id      PDU specific ID for the message
    \param length      Length of the payload
    \param payload     Payload data
*/
void GaiaFramework_SendVendorError(GAIA_TRANSPORT *t, uint16 vendor_id, uint8 feature_id, uint8 pdu_id, uint8 status_code);

/*! \brief Creates a notification to be sent to the mobile application

    \param transport   Transport to send response on
    \param vendor_id   Vendor id from the command
    \param feature_id       Feature ID for the plugin
    \param notification_id  Notification ID for the message
    \param length           Length of the payload
    \param payload          Payload data
*/
void GaiaFramework_SendVendorNotification(GAIA_TRANSPORT *t, uint16 vendor_id, uint8 feature_id, uint8 notification_id, uint16 length, const uint8 *payload);

/*! \brief Creates a response for a command to be sent to the mobile application

    \param transport   Transport to send response on
    \param feature_id  Feature id for the plugin
    \param pdu_id      PDU specific ID for the message
    \param length      Length of the payload
    \param payload     Payload data
*/
void GaiaFramework_SendResponse(GAIA_TRANSPORT *t, gaia_features_t feature_id, uint8 pdu_id, uint16 length, const uint8 *payload);

/*! \brief Creates a error reply for a command to be sent to the mobile application

    \param transport   Transport to send response on
    \param feature_id  Feature ID for the plugin
    \param pdu_id      PDU specific ID for the message
    \param length      Length of the payload
    \param payload     Payload data
*/
void GaiaFramework_SendError(GAIA_TRANSPORT *t, gaia_features_t feature_id, uint8 pdu_id, uint8 status_code);

/*! \brief Get the available space size (in bytes) in the stream buffer

    \param t            Transport to send response on
 
    \return The size of available space in bytes.
 */
uint16 GaiaFramework_GetPacketSpace(GAIA_TRANSPORT *t);

/*! \brief Create a packet with the specified payload size in the stream buffer

    \param t            Transport to send response on
    \param feature_id   Feature ID for the plugin
    \param pdu_id       PDU specific ID for the message
    \param size_payload Length of the payload

    \return A pointer to the start of the payload in the stream buffer.
 */
uint8* GaiaFramework_CreatePacket(GAIA_TRANSPORT *t, const gaia_features_t feature_id, const uint8 pdu_id, const uint16 size_payload);

/*! \brief Flush a packet in the stream buffer

    \param t            Transport to send response on
    \param size_payload Length of the payload
    \param payload      Payload data
 */
void GaiaFramework_FlushPacket(GAIA_TRANSPORT *t, const uint16 size_payload, uint8 *payload);


/*! \brief Creates a notification to be sent to the mobile application

    \param feature_id       Feature ID for the plugin
    \param notification_id  Notification ID for the message
    \param length           Length of the payload
    \param payload          Payload data
*/
void GaiaFramework_SendNotification(gaia_features_t feature_id, uint8 notification_id, uint16 length, const uint8 *payload);
void GaiaFramework_SendDataNotification(gaia_features_t feature_id, uint8 notification_id, uint16 length, const uint8 *payload);
void GaiaFramework_SendNotificationWithTransport(GAIA_TRANSPORT *t, gaia_features_t feature_id, uint8 notification_id, uint16 length, const uint8 *payload);

#endif /* GAIA_FRAMEWORK_H_ */

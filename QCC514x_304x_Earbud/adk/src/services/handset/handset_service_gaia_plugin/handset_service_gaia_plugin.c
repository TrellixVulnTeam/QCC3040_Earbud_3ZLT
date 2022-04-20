/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Handset service gaia plugin component
*/
#define DEBUG_LOG_MODULE_NAME handset_service_gaia_plugin
#include <logging.h>
DEBUG_LOG_DEFINE_LEVEL_VAR

#include "handset_service_gaia_plugin.h"
#include "handset_service.h"


static gaia_framework_command_status_t handsetServicegGaiaPlugin_MainHandler(GAIA_TRANSPORT *t, uint8 pdu_id, uint16 payload_length, const uint8 *payload);
static void handsetServicegGaiaPlugin_EnableMultipoint(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload);
static void handsetServicegGaiaPlugin_SendAllNotifications(GAIA_TRANSPORT *t);
static void handsetServicegGaiaPlugin_MultipointEnableChanged(void);

static uint8 multipoint_enabled;


bool HandsetServicegGaiaPlugin_Init(Task init_task)
{
    UNUSED(init_task);

    static const gaia_framework_plugin_functions_t functions =
    {
        .command_handler = handsetServicegGaiaPlugin_MainHandler,
        .send_all_notifications = handsetServicegGaiaPlugin_SendAllNotifications,
        .transport_connect = NULL,
        .transport_disconnect = NULL,
    };

    DEBUG_LOG_VERBOSE("HandsetServicegGaiaPlugin_Init");

#ifdef ENABLE_MULTIPOINT
    multipoint_enabled = TRUE;
#else
    multipoint_enabled = FALSE;
#endif

    GaiaFramework_RegisterFeature(GAIA_HANDSET_SERVICE_FEATURE_ID, HANDSET_SERVICE_GAIA_PLUGIN_VERSION, &functions);

    return TRUE;
}

void HandsetServicegGaiaPlugin_MultipointEnabledChanged(bool enable)
{
    DEBUG_LOG("HandsetServicegGaiaPlugin_MultipointEnabledChanged");

    multipoint_enabled = enable;

    handsetServicegGaiaPlugin_MultipointEnableChanged();
}

/*! \brief Function pointer definition for the command handler

    \param transport    Transport type

    \param pdu_id       PDU specific ID for the message

    \param length       Length of the payload

    \param payload      Payload data

    \return Gaia framework command status code
*/
static gaia_framework_command_status_t handsetServicegGaiaPlugin_MainHandler(GAIA_TRANSPORT *t, uint8 pdu_id, uint16 payload_length, const uint8 *payload)
{
    DEBUG_LOG("handsetServicegGaiaPlugin_MainHandler, transport %p, pdu_id %u", t, pdu_id);

    switch (pdu_id)
    {
        case enable_multipoint:
            handsetServicegGaiaPlugin_EnableMultipoint(t, payload_length, payload);
            break;

        default:
            DEBUG_LOG("handsetServicegGaiaPlugin_MainHandler, unhandled call for %u", pdu_id);
            return command_not_handled;
    }

    return command_handled;
}

/*! \brief Command that enables or disables multipoint

    \param transport    Transport type
*/
static void handsetServicegGaiaPlugin_EnableMultipoint(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    bool error = TRUE;
    handset_service_config_t handset_service_config = handset_service_singlepoint_config;

    DEBUG_LOG("handsetServicegGaiaPlugin_EnableMultipoint");

    if (payload_length > 0)
    {
        const bool enable = payload[0];

        if (enable)
        {
            handset_service_config = handset_service_multipoint_config;
        }

        if (HandsetService_Configure(handset_service_config))
        {
            error = FALSE;

            multipoint_enabled = enable;

            GaiaFramework_SendResponse(t, GAIA_HANDSET_SERVICE_FEATURE_ID, enable_multipoint, 0, NULL);
        }
        else
        {
            DEBUG_LOG_ERROR("handsetServicegGaiaPlugin_EnableMultipoint, invalid handset service configuration");
        }
    }
    else
    {
        DEBUG_LOG_ERROR("handsetServicegGaiaPlugin_EnableMultipoint, no feature in packet");
    }

    if (error)
    {
        GaiaFramework_SendError(t, GAIA_HANDSET_SERVICE_FEATURE_ID, enable_multipoint, invalid_parameter);
    }

}

/*! \brief Function that sends all available notifications

    \param transport    Transport type
*/
static void handsetServicegGaiaPlugin_SendAllNotifications(GAIA_TRANSPORT *t)
{
    UNUSED(t);

    DEBUG_LOG("handsetServicegGaiaPlugin_SendAllNotifications");

    handsetServicegGaiaPlugin_MultipointEnableChanged();
}

/*! \brief Gaia Client will be told if the User EQ is not present

    \param transport    Transport type
*/
static void handsetServicegGaiaPlugin_MultipointEnableChanged(void)
{
    DEBUG_LOG("handsetServicegGaiaPlugin_EqStateChange");

    GaiaFramework_SendNotification(GAIA_HANDSET_SERVICE_FEATURE_ID, multipoint_enabled_changed, 1, &multipoint_enabled);
}


/*!
\copyright  Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for the  upgrade gaia framework plugin
*/

#if defined(INCLUDE_GAIA) && defined(INCLUDE_DFU)

#define DEBUG_LOG_MODULE_NAME upgrade_gaia_plugin
#include <logging.h>
DEBUG_LOG_DEFINE_LEVEL_VAR

//#define UPGRADE_THROTTLE_ACTION upgrade_stop_start_action

/* upgrade_pause_resume_action support is added from v1.0.75.beta of GAIA client
 * app. So if earlier version of app is used, there may be issues like handover
 * geting veto'ed during DFU.
 */
#define UPGRADE_THROTTLE_ACTION upgrade_pause_resume_action

#include <upgrade_gaia_plugin.h>
#include <link_policy.h>
#include <bandwidth_manager.h>

#include <gaia.h>
#include <panic.h>
#include "bt_device.h"

typedef enum
{
    UPGRADE_STATE_DISCONNECTED,
    UPGRADE_STATE_CONNECTING,
    UPGRADE_STATE_CONNECTED,
    UPGRADE_STATE_DISCONNECTING,
} upgrade_gaia_plugin_state_t;

enum
{
    INTERNAL_CONNECT_REQ,
    INTERNAL_DISCONNECT_REQ,
};

typedef struct
{
    TaskData                    task;
    Task                        serverTask;
    GAIA_TRANSPORT             *transport;
    upgrade_gaia_plugin_state_t state;
    bool_t                      throttled;
    uint16                      lock;
    GAIA_TRANSPORT             *role_change_transport;
} upgrade_gaia_plugin_data_t;

upgrade_gaia_plugin_data_t upgradeGaiaPlugin_task;

static void upgradeGaiaPlugin_MessageHandler(Task task, MessageId id, Message message);

static void upgradeGaiaPlugin_SetState(upgrade_gaia_plugin_state_t state)
{
    upgradeGaiaPlugin_task.state = state;
    switch (state)
    {
        case UPGRADE_STATE_CONNECTING:
        case UPGRADE_STATE_DISCONNECTING:
            upgradeGaiaPlugin_task.lock = 1;
        break;

        default:
            upgradeGaiaPlugin_task.lock = 0;
        break;
    }
}

static void upgradeGaiaPlugin_TransportDisconnect(GAIA_TRANSPORT *t)
{
    DEBUG_LOG_INFO("upgradeGaiaPlugin_TransportDisconnect, transport %p", t);
    if (upgradeGaiaPlugin_task.transport == t)
    {
        DEBUG_LOG_INFO("upgradeGaiaPlugin_TransportDisconnect, disonnecting");
        UpgradeTransportDisconnectRequest();
        upgradeGaiaPlugin_task.transport = NULL;
        upgradeGaiaPlugin_SetState(UPGRADE_STATE_DISCONNECTED);
        BandwidthManager_FeatureStop(BANDWIDTH_MGR_FEATURE_DFU);
    }
    else if (upgradeGaiaPlugin_task.transport)
        DEBUG_LOG_ERROR("upgradeGaiaPlugin_TransportDisconnect, wrong transport");
}


static bool upgradeGaiaPlugin_HandoverVeto(GAIA_TRANSPORT *t)
{
    UNUSED(t);

    switch (upgradeGaiaPlugin_task.state)
    {
        case UPGRADE_STATE_DISCONNECTED:
        {
            /* Upgrade protocol not connected, so handover can proceed */
            return FALSE;
        }

        default:
        {
            /* Can't handover at the moment */
            DEBUG_LOG_DEBUG("upgradeGaiaPlugin_HandoverVeto, veto as upgrade not disconnected");
            return TRUE;
        }
    }
}


static void upgradeGaiaPlugin_UpgradeConnect(GAIA_TRANSPORT *t)
{
    /* Only allow connecting upgrade if not already connected */
    switch (upgradeGaiaPlugin_task.state)
    {
        case UPGRADE_STATE_DISCONNECTED:
        {
            DEBUG_LOG_INFO("upgradeGaiaPlugin_UpgradeConnect");

            upgradeGaiaPlugin_task.transport = t;
            upgradeGaiaPlugin_SetState(UPGRADE_STATE_CONNECTING);

            /* Connect transport task and request UPGRADE_TRANSPORT_DATA_CFM
             * messages and request several blocks at a time */
            UpgradeTransportConnectRequest(&upgradeGaiaPlugin_task.task,
                                           UPGRADE_DATA_CFM_ALL,
                                           UPGRADE_MAX_REQUEST_SIZE_NO_LIMIT);
        }
        break;


        default:
        {
            DEBUG_LOG_ERROR("upgradeGaiaPlugin_UpgradeConnect, already connected");
            GaiaFramework_SendError(t, GAIA_DFU_FEATURE_ID, upgrade_connect, incorrect_state);
        }
        break;
    }
}

static void upgradeGaiaPlugin_UpgradeConnectCfm(UPGRADE_TRANSPORT_CONNECT_CFM_T *cfm)
{
    DEBUG_LOG_INFO("upgradeGaiaPlugin_UpgradeConnectCfm, status %u", cfm->status);

    switch (upgradeGaiaPlugin_task.state)
    {
        case UPGRADE_STATE_CONNECTING:
        {
            if (cfm->status == upgrade_status_success)
            {
                upgradeGaiaPlugin_SetState(UPGRADE_STATE_CONNECTED);

                /* Inform bandwidth manager we've started if not already started */
                if (!BandwidthManager_IsFeatureRunning(BANDWIDTH_MGR_FEATURE_DFU))
                    BandwidthManager_FeatureStart(BANDWIDTH_MGR_FEATURE_DFU);

                TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(GaiaGetClientList()), APP_GAIA_UPGRADE_CONNECTED);
                appLinkPolicyUpdatePowerTable(&upgradeGaiaPlugin_task.transport->tp_bd_addr.taddr.addr);
                /* Set the flag for AG that Upgrade Transport is connected. */
                BtDevice_SetUpgradeTransportConnected(BtDevice_GetDeviceForTpbdaddr(&upgradeGaiaPlugin_task.transport->tp_bd_addr), TRUE);
                GaiaFramework_SendResponse(upgradeGaiaPlugin_task.transport, GAIA_DFU_FEATURE_ID,
                                           upgrade_connect, 0, NULL);
            }
            else
            {
                upgradeGaiaPlugin_SetState(UPGRADE_STATE_DISCONNECTED);
                GaiaFramework_SendError(upgradeGaiaPlugin_task.transport, GAIA_DFU_FEATURE_ID, upgrade_connect, incorrect_state);
                upgradeGaiaPlugin_task.transport = NULL;
            }
        }
        break;

        default:
        {
            DEBUG_LOG_ERROR("upgradeGaiaPlugin_UpgradeConnectCfm, in wrong state %u", upgradeGaiaPlugin_task.state);
            if (cfm->status == upgrade_status_success)
                UpgradeTransportDisconnectRequest();
        }
        break;
    }
}

static void upgradeGaiaPlugin_UpgradeDisconnect(GAIA_TRANSPORT *transport)
{
    switch (upgradeGaiaPlugin_task.state)
    {
        case UPGRADE_STATE_CONNECTED:
        case UPGRADE_STATE_CONNECTING:
        {
            /* Disconnect upgrade if command was on correct transport */
            if (upgradeGaiaPlugin_task.transport == transport)
            {
                DEBUG_LOG_INFO("upgradeGaiaPlugin_UpgradeDisconnect");

                UpgradeTransportDisconnectRequest();
                upgradeGaiaPlugin_SetState(UPGRADE_STATE_DISCONNECTING);
                TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(GaiaGetClientList()), APP_GAIA_UPGRADE_DISCONNECTED);
            }
            else
            {
                DEBUG_LOG_ERROR("upgradeGaiaPlugin_UpgradeDisconnect, from different transport");
                GaiaFramework_SendError(transport, GAIA_DFU_FEATURE_ID, upgrade_disconnect, incorrect_state);
            }
        }
        break;

        default:
        {
            DEBUG_LOG_ERROR("upgradeGaiaPlugin_UpgradeDisconnect, not connected");
            GaiaFramework_SendError(transport, GAIA_DFU_FEATURE_ID, upgrade_disconnect, incorrect_state);
        }
        break;
    }
}

static void upgradeGaiaPlugin_UpgradeDisconnectCfm(UPGRADE_TRANSPORT_DISCONNECT_CFM_T *cfm)
{
    DEBUG_LOG_INFO("upgradeGaiaPlugin_UpgradeDisconnectCfm, status %u", cfm->status);

    switch (upgradeGaiaPlugin_task.state)
    {
        case UPGRADE_STATE_DISCONNECTING:
        {
            PanicNull(upgradeGaiaPlugin_task.transport);
            if (cfm->status == upgrade_status_success)
            {
                GaiaFramework_SendResponse(upgradeGaiaPlugin_task.transport, GAIA_DFU_FEATURE_ID,
                                           upgrade_disconnect, 0, NULL);

                /* Reset the flag for AG as Upgrade Transport is disconnected. */
                BtDevice_SetUpgradeTransportConnected(BtDevice_GetDeviceForTpbdaddr(&upgradeGaiaPlugin_task.transport->tp_bd_addr), FALSE);

                appLinkPolicyUpdatePowerTable(&upgradeGaiaPlugin_task.transport->tp_bd_addr.taddr.addr);
                upgradeGaiaPlugin_SetState(UPGRADE_STATE_DISCONNECTED);

                /* Inform bandwidth manager we've stopped if disconnect wasn't due to being throttled */
                if (!upgradeGaiaPlugin_task.throttled)
                {
                    upgradeGaiaPlugin_task.transport = NULL;
                    BandwidthManager_FeatureStop(BANDWIDTH_MGR_FEATURE_DFU);
                }
            }
            else
                GaiaFramework_SendError(upgradeGaiaPlugin_task.transport, GAIA_DFU_FEATURE_ID,
                                        upgrade_disconnect, incorrect_state);
        }
        break;

        default:
        {
            if (upgradeGaiaPlugin_task.transport)
            {
                DEBUG_LOG_ERROR("upgradeGaiaPlugin_UpgradeDisconnect, incorrect state %u", upgradeGaiaPlugin_task.state);
                GaiaFramework_SendError(upgradeGaiaPlugin_task.transport, GAIA_DFU_FEATURE_ID,
                                        upgrade_disconnect, incorrect_state);
            }
        }
        break;
    }
}

static gaia_framework_command_status_t upgradeGaiaPlugin_UpgradeControl(GAIA_TRANSPORT *transport, uint16 payload_length, const uint8 *payload)
{
    /* Disconnect upgrade if command was on correct transport */
    if (upgradeGaiaPlugin_task.transport == transport)
    {
        DEBUG_LOG_VERBOSE("upgradeGaiaPlugin_UpgradeControl");
        UpgradeProcessDataRequest(payload_length, (uint8 *)payload);
        return command_pending;
    }
    else
    {
        DEBUG_LOG_ERROR("upgradeGaiaPlugin_UpgradeControl, from different transport");
        return command_not_handled;
    }
}

static void upgradeGaiaPlugin_UpgradeDataInd(UPGRADE_TRANSPORT_DATA_IND_T *ind)
{
    DEBUG_LOG_VERBOSE("upgradeGaiaPlugin_UpgradeDataInd");
    DEBUG_LOG_DATA_V_VERBOSE(ind->data, ind->size_data);

    GaiaFramework_SendNotificationWithTransport(upgradeGaiaPlugin_task.transport, GAIA_DFU_FEATURE_ID,
                                                upgrade_data_indication, ind->size_data, ind->data);
}

static void upgradeGaiaPlugin_UpgradeDataCfm(UPGRADE_TRANSPORT_DATA_CFM_T *cfm)
{
    uint8 status = cfm->status;

    if (upgradeGaiaPlugin_task.transport)
    {
        DEBUG_LOG_VERBOSE("upgradeGaiaPlugin_UpgradeDataCfm, status %u", status);

        /* Only send response if packet wasn't received over data endpoint */
        const gaia_data_endpoint_mode_t mode = Gaia_GetPayloadDataEndpointMode(upgradeGaiaPlugin_task.transport, cfm->size_data, cfm->data);
        if (mode == GAIA_DATA_ENDPOINT_MODE_NONE)
        {
            GaiaFramework_SendResponse(upgradeGaiaPlugin_task.transport, GAIA_DFU_FEATURE_ID,
                                       upgrade_control, sizeof(status), &status);
        }

        Gaia_CommandResponse(upgradeGaiaPlugin_task.transport, cfm->size_data, cfm->data);
    }
    else
        DEBUG_LOG_INFO("upgradeGaiaPlugin_UpgradeDataCfm, no transport");
}


static void upgradeGaiaPlugin_GetDataEndpoint(GAIA_TRANSPORT *t)
{
    DEBUG_LOG("upgradeGaiaPlugin_GetDataEndpoint");

    uint8 data_endpoint_mode = Gaia_GetDataEndpointMode(t);
    GaiaFramework_SendResponse(upgradeGaiaPlugin_task.transport, GAIA_DFU_FEATURE_ID,
                               get_data_endpoint, 1, &data_endpoint_mode);
}

static void upgradeGaiaPlugin_SetDataEndpoint(GAIA_TRANSPORT *t,  uint16 payload_length, const uint8 *payload)
{
    if (payload_length >= 1)
    {
        DEBUG_LOG("upgradeGaiaPlugin_SetDataEndpoint, mode %u", payload[0]);

        if (Gaia_SetDataEndpointMode(t, payload[0]))
        {
            GaiaFramework_SendResponse(t, GAIA_DFU_FEATURE_ID,
                                       set_data_endpoint, 0, NULL);
        }
        else
        {
            DEBUG_LOG("upgradeGaiaPlugin_SetDataEndpoint, failed to set mode");
            GaiaFramework_SendError(t, GAIA_DFU_FEATURE_ID,
                                    set_data_endpoint, invalid_parameter);
        }
    }
    else
    {
        DEBUG_LOG("upgradeGaiaPlugin_SetDataEndpoint, no payload");
        GaiaFramework_SendError(t, GAIA_DFU_FEATURE_ID,
                                set_data_endpoint, invalid_parameter);
    }
}

static void upgradeGaiaPlugin_SendAllNotifications(GAIA_TRANSPORT *t)
{
    UNUSED(t);
    DEBUG_LOG("upgradeGaiaPlugin_SendAllNotifications");
}

static gaia_framework_command_status_t upgradeGaiaPlugin_MainHandler(GAIA_TRANSPORT *t, uint8 pdu_id, uint16 payload_length, const uint8 *payload)
{
    DEBUG_LOG("upgradeGaiaPlugin_MainHandler, called for enum:upgrade_gaia_plugin_pdu_ids_t:%d", pdu_id);

    switch (pdu_id)
    {
        case upgrade_connect:
            upgradeGaiaPlugin_UpgradeConnect(t);
            return command_handled;

        case upgrade_disconnect:
            upgradeGaiaPlugin_UpgradeDisconnect(t);
            return command_handled;

        case upgrade_control:
            return upgradeGaiaPlugin_UpgradeControl(t, payload_length, payload);

        case get_data_endpoint:
            upgradeGaiaPlugin_GetDataEndpoint(t);
            return command_handled;

        case set_data_endpoint:
            upgradeGaiaPlugin_SetDataEndpoint(t, payload_length, payload);
            return command_handled;

        default:
            DEBUG_LOG_ERROR("upgradeGaiaPlugin_MainHandler, unhandled call for %d", pdu_id);
            return command_not_handled;
    }
}


static void upgradeGaiaPlugin_HandleInternalConnect(void)
{
    if (upgradeGaiaPlugin_task.state == UPGRADE_STATE_DISCONNECTED)
    {
        uint8 reason = upgrade_stop_start_action;

        /* If role_change_transport is set then send command on that transport, otherwise
         * send command on all transports */
        if (upgradeGaiaPlugin_task.role_change_transport)
        {
            GaiaFramework_SendNotificationWithTransport(upgradeGaiaPlugin_task.role_change_transport,
                                                        GAIA_DFU_FEATURE_ID,
                                                        upgrade_start_request, sizeof(reason), &reason);
        }
        else
        {
            GaiaFramework_SendNotification(GAIA_DFU_FEATURE_ID,
                                           upgrade_start_request, sizeof(reason), &reason);
        }
    }
    upgradeGaiaPlugin_task.role_change_transport = 0;
}

static void upgradeGaiaPlugin_HandleInternalDisconnect(void)
{
    if (upgradeGaiaPlugin_task.state == UPGRADE_STATE_CONNECTED)
    {
        /* Upgrade protocol is connected, so send a notification to the handset to stop upgrade */
        uint8 reason = upgrade_stop_start_action;
        GaiaFramework_SendNotificationWithTransport(upgradeGaiaPlugin_task.transport, GAIA_DFU_FEATURE_ID,
                                                    upgrade_stop_request, sizeof(reason), &reason);

        /* Remember transport role-change is effecting */
        upgradeGaiaPlugin_task.role_change_transport = upgradeGaiaPlugin_task.transport;
    }
}




static void upgradeGaiaPlugin_MessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);
    switch(id)
    {
        /* Response from call to UpgradeTransportConnectRequest() */
        case UPGRADE_TRANSPORT_CONNECT_CFM:
            upgradeGaiaPlugin_UpgradeConnectCfm((UPGRADE_TRANSPORT_CONNECT_CFM_T *)message);
            break;

        /* Response from call to UpgradeTransportDisconnectRequest() */
        case UPGRADE_TRANSPORT_DISCONNECT_CFM:
            upgradeGaiaPlugin_UpgradeDisconnectCfm((UPGRADE_TRANSPORT_DISCONNECT_CFM_T *)message);
            break;

        /* Request from upgrade library to send a data packet to the host */
        case UPGRADE_TRANSPORT_DATA_IND:
            upgradeGaiaPlugin_UpgradeDataInd((UPGRADE_TRANSPORT_DATA_IND_T *)message);
            break;

        case UPGRADE_TRANSPORT_DATA_CFM:
            upgradeGaiaPlugin_UpgradeDataCfm((UPGRADE_TRANSPORT_DATA_CFM_T *)message);
            break;

        case INTERNAL_CONNECT_REQ:
            upgradeGaiaPlugin_HandleInternalConnect();
        break;

        case INTERNAL_DISCONNECT_REQ:
            upgradeGaiaPlugin_HandleInternalDisconnect();
        break;

        default:
            DEBUG_LOG_ERROR("upgradeGaiaPlugin_MessageHandler, unhandled message MESSAGE:0x%04x", id);
            break;
    }
}

static void upgradeGaiaPlugin_RoleChangeStart(GAIA_TRANSPORT *t)
{
    DEBUG_LOG("upgradeGaiaPlugin_RoleChangeStart, transport %p", t);
    if (upgradeGaiaPlugin_task.transport == t)
    {
        /* Send internal message to request disconnecting upgrade */
        MessageSendConditionally(&upgradeGaiaPlugin_task.task, INTERNAL_DISCONNECT_REQ, NULL, &upgradeGaiaPlugin_task.lock);
    }
}

static void upgradeGaiaPlugin_RoleChangeCompleted(GAIA_TRANSPORT *t, bool is_primary)
{
    DEBUG_LOG("upgradeGaiaPlugin_RoleChangeCompleted, transport %p, is_primary %u", t, is_primary);
    if (is_primary)
    {
        /* Send internal message to request reconnecting upgrade */
        MessageSendConditionally(&upgradeGaiaPlugin_task.task, INTERNAL_CONNECT_REQ, NULL, &upgradeGaiaPlugin_task.lock);
    }
    else if (t == upgradeGaiaPlugin_task.role_change_transport)
    {
        upgradeGaiaPlugin_task.role_change_transport = 0;
    }
}

static void upgradeGaiaPlugin_RoleChangeCancelled(GAIA_TRANSPORT *t)
{
    DEBUG_LOG("upgradeGaiaPlugin_RoleChangeCancelled, transport %p", t);
    if (t == upgradeGaiaPlugin_task.role_change_transport)
    {
        /* Send internal message to request reconnecting upgrade */
        MessageSendConditionally(&upgradeGaiaPlugin_task.task, INTERNAL_CONNECT_REQ, NULL, &upgradeGaiaPlugin_task.lock);
    }
}

static void upgrade_BandwithThrottle(bool throttle_required)
{
    DEBUG_LOG("upgrade_BandwithThrottle, throttle_required %u", throttle_required);
    upgradeGaiaPlugin_task.throttled = throttle_required;

    /* Check if upgrade transport is connected */
    if (upgradeGaiaPlugin_task.transport)
    {
        /* Send notification to request upgrade to pause or resume */
        uint8 reason = UPGRADE_THROTTLE_ACTION;
        if (throttle_required)
            DEBUG_LOG("upgrade_BandwithThrottle, sending stop");
        else
            DEBUG_LOG("upgrade_BandwithThrottle, sending start");
        GaiaFramework_SendNotificationWithTransport(upgradeGaiaPlugin_task.transport, GAIA_DFU_FEATURE_ID,
                                                    throttle_required ? upgrade_stop_request : upgrade_start_request, sizeof(reason), &reason);
    }
}

void UpgradeGaiaPlugin_Init(void)
{
    static const gaia_framework_plugin_functions_t functions =
    {
        .command_handler = upgradeGaiaPlugin_MainHandler,
        .send_all_notifications = upgradeGaiaPlugin_SendAllNotifications,
        .transport_connect = NULL,
        .transport_disconnect = upgradeGaiaPlugin_TransportDisconnect,
        .handover_veto = upgradeGaiaPlugin_HandoverVeto,
        .role_change_start = upgradeGaiaPlugin_RoleChangeStart,
        .role_change_completed = upgradeGaiaPlugin_RoleChangeCompleted,
        .role_change_cancelled = upgradeGaiaPlugin_RoleChangeCancelled,
    };

    DEBUG_LOG("UpgradeGaiaPlugin_Init");

    upgradeGaiaPlugin_task.task.handler = upgradeGaiaPlugin_MessageHandler;
    upgradeGaiaPlugin_SetState(UPGRADE_STATE_DISCONNECTED);

    GaiaFramework_RegisterFeature(GAIA_DFU_FEATURE_ID, UPGRADE_GAIA_PLUGIN_VERSION, &functions);
    PanicFalse(BandwidthManager_RegisterFeature(BANDWIDTH_MGR_FEATURE_DFU, low_bandwidth_manager_priority, upgrade_BandwithThrottle));
}

bool UpgradeGaiaPlugin_IsHandsetTransferActive(const tp_bdaddr *tp_bd_addr)
{
    /* Return true if GAIA transport is not NULL (upgrade is connected) and
     * tp_bd_addr of GAIA transport and requested handset is same.
     */
    return (upgradeGaiaPlugin_task.transport != NULL &&
        BdaddrTpIsSame(&upgradeGaiaPlugin_task.transport->tp_bd_addr, tp_bd_addr));
}

#endif /* defined(INCLUDE_GAIA) && defined(INCLUDE_DFU) */

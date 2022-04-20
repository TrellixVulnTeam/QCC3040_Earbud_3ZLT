/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Handset service config
*/

#include "handset_service_config.h"
#include "handset_service.h"

#include <connection_manager.h>
#include <device_properties.h>
#include <device_db_serialiser.h>

#if defined(INCLUDE_GAIA)
#include "handset_service_gaia_plugin.h"
#endif

#include <device_list.h>

#include <panic.h>

const handset_service_config_t handset_service_multipoint_config =
{
    /* Two connections supported */
    .max_bredr_connections = 2,
    /* Only one LE connection supported */
    .max_le_connections = 1,
    /* Two ACL reconnection attempts per supported connection */
    .acl_connect_attempt_limit = 2,
    /* Page timeout 5 seconds*/
    .page_timeout = MS_TO_BT_SLOTS(5 * MS_PER_SEC)
};

const handset_service_config_t handset_service_singlepoint_config =
{
    /* One connection supported */
    .max_bredr_connections = 1,
    /* Only one LE connection supported */
    .max_le_connections = 1,
    /* Three ACL reconnection attempts per supported connection */
    .acl_connect_attempt_limit = 3,
    /* Page timeout 10 seconds*/
    .page_timeout = MS_TO_BT_SLOTS(10 * MS_PER_SEC)
};

static handset_service_config_t *handsetService_GetConfig(void)
{
    deviceType device_type = DEVICE_TYPE_SELF;

    device_t device = DeviceList_GetFirstDeviceWithPropertyValue(device_property_type, &device_type, sizeof(deviceType));
    if(device)
    {
        handset_service_config_t *config;
        size_t size;
        if(Device_GetProperty(device, device_property_headset_service_config, (void **)&config, &size))
        {
            PanicFalse(size == sizeof(handset_service_config_t));
            return config;
        }
    }

    return NULL;
}

uint8 handsetService_LeAclMaxConnections(void)
{
    handset_service_config_t *config = handsetService_GetConfig();
    if(config)
    {
        return config->max_le_connections;
    }
    return 1;
}

uint8 handsetService_BredrAclConnectAttemptLimit(void)
{
    handset_service_config_t *config = handsetService_GetConfig();
    if(config)
    {
        return config->acl_connect_attempt_limit;
    }

    return 1;
}

uint8 handsetService_BredrAclMaxConnections(void)
{
    handset_service_config_t *config = handsetService_GetConfig();
    if(config)
    {
        return config->max_bredr_connections;
    }

    return 1;
}

void HandsetServiceConfig_Init(void)
{
    deviceType device_type = DEVICE_TYPE_SELF;

    handsetService_HandleConfigUpdate();

    /* Handle situation when the SELF device is already created, but device_property_headset_service_config wasn't yet set.
       That is expected on first boot of non-earbud applications.
    */
    device_t device = DeviceList_GetFirstDeviceWithPropertyValue(device_property_type, &device_type, sizeof(deviceType));
    if(device)
    {
        if(!Device_IsPropertySet(device, device_property_headset_service_config))
        {
            handsetService_HandleSelfCreated();
        }

    }
}

bool HandsetService_Configure(handset_service_config_t config)
{
    deviceType device_type = DEVICE_TYPE_SELF;

    if(config.max_bredr_connections > HANDSET_SERVICE_MAX_PERMITTED_BREDR_CONNECTIONS)
    {
        return FALSE;
    }

    if(config.max_bredr_connections < 1)
    {
        return FALSE;
    }

    ConManager_SetPageTimeout(config.page_timeout);

    device_t device = DeviceList_GetFirstDeviceWithPropertyValue(device_property_type, &device_type, sizeof(deviceType));
    if(device)
    {
        Device_SetProperty(device, device_property_headset_service_config, &config, sizeof(config));
        DeviceDbSerialiser_SerialiseDevice(device);
    }

#if defined(INCLUDE_GAIA)
    if(config.max_bredr_connections > 1)
    {
        HandsetServicegGaiaPlugin_MultipointEnabledChanged(TRUE);
    }
    else
    {
        HandsetServicegGaiaPlugin_MultipointEnabledChanged(FALSE);
    }
#endif /* INCLUDE_GAIA */

    return TRUE;
}

void HandsetService_SetDefaultConfig(void *value, uint8 size)
{
    PanicFalse(size == sizeof(handset_service_config_t));
#ifdef ENABLE_MULTIPOINT
    memcpy(value, &handset_service_multipoint_config, size);
#else
    memcpy(value, &handset_service_singlepoint_config, size);
#endif
}

void handsetService_HandleSelfCreated(void)
{
    deviceType device_type = DEVICE_TYPE_SELF;

#ifdef ENABLE_MULTIPOINT
    HandsetService_Configure(handset_service_multipoint_config);
#else
    HandsetService_Configure(handset_service_singlepoint_config);
#endif

    device_t device = DeviceList_GetFirstDeviceWithPropertyValue(device_property_type, &device_type, sizeof(deviceType));
    if(device)
    {
        DeviceDbSerialiser_SerialiseDevice(device);
    }
}

void handsetService_HandleConfigUpdate(void)
{
    handset_service_config_t *config = handsetService_GetConfig();
    if(config)
    {
        ConManager_SetPageTimeout(config->page_timeout);
    }
}

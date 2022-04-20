/******************************************************************************
 Copyright (c) 2020 Qualcomm Technologies International, Ltd.
 All Rights Reserved.
 Qualcomm Technologies International, Ltd. Confidential and Proprietary.

 REVISION:      $Revision: #1 $
******************************************************************************/
#ifndef GATT_SERVICE_DISCOVERY_HANDLER_H__
#define GATT_SERVICE_DISCOVERY_HANDLER_H__

#include "gatt_service_discovery_private.h"

#ifdef __cplusplus
extern "C" {
#endif


/***************************************************************************
NAME
    gattSdMsgHandler

DESCRIPTION
    Handler for external messages sent to the library in the client role.
*/
void gattServiceDiscoveryMsgHandler(Task task, MessageId id, Message msg);

/***************************************************************************
NAME
    gattServiceDiscoveryRegisterSupportedServicesCfm

DESCRIPTION
    Send the GATT_SERVICE_DISCOVERY_REGISTER_SUPPORTED_SERVICES_CFM message.
    to application.
*/
void gattServiceDiscoveryRegisterSupportedServicesCfm(Task task,
                          GattSdResult result);

/***************************************************************************
NAME
    gattServiceDiscoveryStartCfm

DESCRIPTION
    Send the GATT_SERVICE_DISCOVERY_START_CFM message.to application.
*/
void gattServiceDiscoveryStartCfm(Task task,
                          GattSdResult result,
                          connection_id_t   cid);

/***************************************************************************
NAME
    gattServiceDiscoveryStopCfm

DESCRIPTION
    Send the GATT_SERVICE_DISCOVERY_STOP_CFM message.to application.
*/
void gattServiceDiscoveryStopCfm(Task task,
                          GattSdResult result,
                          connection_id_t   cid);

/***************************************************************************
NAME
    gattServiceDiscoveryGetDeviceConfigCfm

DESCRIPTION
    Send the GATT_SERVICE_DISCOVERY_GET_DEVICE_CONFIG_CFM message.to application.
*/
void gattServiceDiscoveryGetDeviceConfigCfm(Task task,
                          GattSdResult result,
                          connection_id_t cid,
                          uint16 srvcInfoCount,
                          GattSdSrvcInfo_t *srvcInfo);

/***************************************************************************
NAME
    gattServiceDiscoveryAddDeviceConfigCfm

DESCRIPTION
    Send the GATT_SERVICE_DISCOVERY_ADD_DEVICE_CONFIG_CFM message.to application.
*/
void gattServiceDiscoveryAddDeviceConfigCfm(Task task,
                          GattSdResult result);

/***************************************************************************
NAME
    gattServiceDiscoveryRemoveDeviceConfigCfm

DESCRIPTION
    Send the GATT_SERVICE_DISCOVERY_REMOVE_DEVICE_CONFIG_CFM message.to application.
*/
void gattServiceDiscoveryRemoveDeviceConfigCfm(Task task,
                          GattSdResult result,
                          connection_id_t cid);

/***************************************************************************
NAME
    gattServiceDiscoveryFindServiceRangeCfm

DESCRIPTION
    Send the GATT_SERVICE_DISCOVERY_FIND_SERVICE_RANGE_CFM message.to application.
*/
void gattServiceDiscoveryFindServiceRangeCfm(Task task,
                          GattSdResult result,
                          connection_id_t cid,
                          uint16 srvcInfoCount,
                          GattSdSrvcInfo_t *srvcInfo);


/***************************************************************************
NAME
    gattSdDlAddDevice

DESCRIPTION
    Add a new device element to the device list and return the pointer of the
    the device element.
*/
GattSdDeviceElement* gattSdDlAddDevice(GattSdDeviceElement** list);

/***************************************************************************
NAME
    gattSdDlRemoveDevice

DESCRIPTION
    Remove the device element from the device list.
*/
void gattSdDlRemoveDevice(GattSdDeviceElement** list,
                                GattSdDeviceElement* elem);

/***************************************************************************
NAME
    gattSdDlCleanup

DESCRIPTION
    Remove all the device element from the list and free the resources.
*/
void gattSdDlCleanup(GattSdDeviceElement* list);

/***************************************************************************
NAME
    gattSdDlFindByConnid

DESCRIPTION
    Find the device element based on the connection id
*/
GattSdDeviceElement* gattSdDlFindByConnid(GattSdDeviceElement* list, 
                                connection_id_t cid);

/***************************************************************************
NAME
    gattSdSlAddService

DESCRIPTION
    Add new service element to the service list and retur the pointer of the 
    service element.
*/
GattSdServiceElement* gattSdSlAddService(GattSdServiceElement** list);

/***************************************************************************
NAME
    gattSdSlCleanup

DESCRIPTION
    Remove all the service element from the service list.
*/
void gattSdSlCleanup(GattSdServiceElement* list);

/***************************************************************************
NAME
    gattSdAddDevice

DESCRIPTION
    Add device with the connection id to the device list.
*/
void gattSdAddDevice(GGSD *gatt_sd,
                                   connection_id_t cid);

/***************************************************************************
NAME
    gattSdAddService

DESCRIPTION
    Add service with the service id, start handle, end handle and service type 
    to the service list of the device element.
*/
void gattSdAddService(GattSdDeviceElement *devElem,
                                   GattSdSrvcId srvcId,
                                   uint16 startHandle,
                                   uint16 endHandle,
                                   GattSdServiceType serviceType);

/***************************************************************************
NAME
    gattSdGetSrvcIdFromUuid128

DESCRIPTION
    Get the GATT SD Service Id based on the 128 bit uuid from the 
    GattSdSrvcUuidInfo mainatined by GATT SD library.
*/
GattSdSrvcId gattSdGetSrvcIdFromUuid128(const uint32* uuid);


/***************************************************************************
NAME
    gattSdGetSrvcIdFromUuid32

DESCRIPTION
    Get the GATT SD Service Id based on the 32 bit uuid from the 
    GattSdSrvcUuidInfo mainatined by GATT SD library.
*/
GattSdSrvcId gattSdGetSrvcIdFromUuid32(uint32 uuid);


/***************************************************************************
NAME
    gattSdGetSrvcIdFromUuid16

DESCRIPTION
    Get the GATT SD Service Id based on the 16 bit uuid from the 
    GattSdSrvcUuidInfo mainatined by GATT SD library.
*/
GattSdSrvcId gattSdGetSrvcIdFromUuid16(uint16 uuid);


/***************************************************************************
NAME
    gattSdDiscoverPrimaryServiceByUuid

DESCRIPTION
    Returns true if GATT Service Discovery by UUID started successfully else
    FALSE.
*/
bool gattSdDiscoverPrimaryServiceByUuid(GGSD *gatt_sd);


#ifdef __cplusplus
}
#endif

#endif /* GATT_SERVICE_DISCOVERY_HANDLER_H__ */


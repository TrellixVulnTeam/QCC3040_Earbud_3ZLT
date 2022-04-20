/******************************************************************************
 Copyright (c) 2020 Qualcomm Technologies International, Ltd.
 All Rights Reserved.
 Qualcomm Technologies International, Ltd. Confidential and Proprietary.

 REVISION:      $Revision: #1 $
******************************************************************************/

#include "gatt_service_discovery_init.h"
#include "gatt_service_discovery_handler.h"
#include "gatt_service_discovery_debug.h"

#ifdef SYNERGY_GATT_SD_ENABLE
#include "csr_bt_gatt_lib.h"
#endif

/* GATT Service Discovery UUID Info List size */
#define GATT_SD_SRVC_UUID_INFO_LIST_SIZE  (0x06)

/* List of GATT Service UUIDs that could be discovered by Gatt Service module
 *
 * Based on gatt_uuid_type_t value, it will decides 16 bit or 128 bit UUID value 
 * used in UUID based Primary Service Discovery for the GATT Service
 *
 */
GattSdSrvcUuidInfo_t   GattSdSrvcUuidInfo[GATT_SD_SRVC_UUID_INFO_LIST_SIZE] = {
    {GATT_SD_GATT_SRVC, {gatt_uuid16, {0x00001801u, 0x00001000u, 0x80000080u, 0x5F9B34FBu}}},
    {GATT_SD_GAP_SRVC,  {gatt_uuid16, {0x00001800u, 0x00001000u, 0x80000080u, 0x5F9B34FBu}}},
    {GATT_SD_CSIS_SRVC, {gatt_uuid16, {0x00008FD8u, 0x00001000u, 0x80000080u, 0x5F9B34FBu}}},
    {GATT_SD_PACS_SRVC, {gatt_uuid16, {0x00008FD9u, 0x00001000u, 0x80000080u, 0x5F9B34FBu}}},
    {GATT_SD_ASCS_SRVC, {gatt_uuid16, {0x00008FDAu, 0x00001000u, 0x80000080u, 0x5F9B34FBu}}},
    {GATT_SD_VCS_SRVC,  {gatt_uuid16, {0x0000183Eu, 0x00001000u, 0x80000080u, 0x5F9B34FBu}}}
};

GattSdDeviceElement* gattSdDlAddDevice(GattSdDeviceElement** list)
{
    GattSdDeviceElement* elem = NULL;
    GattSdDeviceElement* temp = *list;
    if (temp == NULL) 
    {
        /* list is empty */
        elem = (GattSdDeviceElement*) GATT_SD_MALLOC(sizeof(GattSdDeviceElement));
        memset(elem, 0, sizeof(GattSdDeviceElement));
        elem->next = NULL;
        *list = elem;
    }
    else 
    {
        /* transverse list to the last element */
        while(temp->next != NULL)
            temp = temp->next;
        elem = (GattSdDeviceElement*) GATT_SD_MALLOC(sizeof(GattSdDeviceElement));
        memset(elem, 0, sizeof(GattSdDeviceElement));
        elem->next = NULL;
        temp->next = elem;
    }
    return elem;
}


void gattSdDlRemoveDevice(GattSdDeviceElement** list,
                                GattSdDeviceElement* elem)
{
    GattSdDeviceElement* temp1 = *list;
    GattSdDeviceElement* temp2 = *list;
    if (temp1 != NULL)
    {
        /* element is at the head of the list */
        if (temp1 == elem)
        {
            *list = temp1->next;
        }
        else
        {
            /* transverse the list to find the element */
            temp2 = temp1->next;
            while (temp2 != NULL)
            {
                if (temp2 == elem) {
                    temp1->next = temp2->next;
                    break;
                }
                else
                {
                    temp1 = temp1->next;
                    temp2 = temp2->next;
                }
            }
        }
    }
}

void gattSdDlCleanup(GattSdDeviceElement* list)
{
    while (list)
    {
        GattSdDeviceElement* devElem = list;
        /* Free Device service list */
        gattSdSlCleanup(devElem->serviceList);
        list = list->next;
        free(devElem);
    }
}


GattSdDeviceElement* gattSdDlFindByConnid(GattSdDeviceElement* list, 
                                connection_id_t cid)
{
    GattSdDeviceElement* temp = list;
    while (temp != NULL) {
        if (temp->cid == cid) {
            break;
        }
        temp = temp->next;
    }
    return temp;
}

GattSdServiceElement* gattSdSlAddService(GattSdServiceElement** list)
{
    GattSdServiceElement* elem = NULL;
    GattSdServiceElement* temp = *list;

    /* Check if list is empty */
    if (temp == NULL) 
    {
        /* Alloc new element */
        elem = (GattSdServiceElement*) GATT_SD_MALLOC(sizeof(GattSdServiceElement));
        memset(elem, 0, sizeof(GattSdServiceElement));
        elem->next = NULL;
        /* Set new element as head of the list */
        *list = elem;
    }
    else 
    {
        /* Transverse thorugh list to the last element */
        while(temp->next != NULL)
        {
            temp = temp->next;
        }
        /* Alloc memory for the new element */
        elem = (GattSdServiceElement*) GATT_SD_MALLOC(sizeof(GattSdServiceElement));
        memset(elem, 0, sizeof(GattSdServiceElement));
        elem->next = NULL;
        /* Append new element to the end of the list */
        temp->next = elem;
    }
    return elem;
}

void gattSdSlCleanup(GattSdServiceElement* list)
{
    while (list)
    {
        GattSdServiceElement* srvcElem = list;
        list = list->next;
        free(srvcElem);
    }
}


void gattServiceDiscoveryRegisterSupportedServicesCfm(Task task,
                                  GattSdResult result)
{
    MAKE_GATT_SD_MESSAGE(GATT_SERVICE_DISCOVERY_REGISTER_SUPPORTED_SERVICES_CFM);

    message->result = result;

    GATT_SD_MESSAGE_SEND(task, GATT_SERVICE_DISCOVERY_REGISTER_SUPPORTED_SERVICES_CFM, message);
}


void gattServiceDiscoveryStartCfm(Task task,
                          GattSdResult result,
                          connection_id_t cid)
{
    MAKE_GATT_SD_MESSAGE(GATT_SERVICE_DISCOVERY_START_CFM);

    message->result = result;
    message->cid = cid;

    GATT_SD_MESSAGE_SEND(task, GATT_SERVICE_DISCOVERY_START_CFM, message);
}

void gattServiceDiscoveryStopCfm(Task task,
                          GattSdResult result,
                          connection_id_t cid)
{
    MAKE_GATT_SD_MESSAGE(GATT_SERVICE_DISCOVERY_STOP_CFM);

    message->result = result;
    message->cid = cid;

    GATT_SD_MESSAGE_SEND(task, GATT_SERVICE_DISCOVERY_STOP_CFM, message);
}

void gattServiceDiscoveryGetDeviceConfigCfm(Task task,
                                  GattSdResult result,
                                  connection_id_t cid,
                                  uint16 srvcInfoCount,
                                  GattSdSrvcInfo_t *srvcInfo)
{
    MAKE_GATT_SD_MESSAGE(GATT_SERVICE_DISCOVERY_GET_DEVICE_CONFIG_CFM);

    message->result = result;
    message->cid = cid;
    message->srvcInfo = srvcInfo;
    message->srvcInfoCount = srvcInfoCount;

    GATT_SD_MESSAGE_SEND(task, GATT_SERVICE_DISCOVERY_GET_DEVICE_CONFIG_CFM, message);
}

void gattServiceDiscoveryAddDeviceConfigCfm(Task task,
                                  GattSdResult result)
{
    MAKE_GATT_SD_MESSAGE(GATT_SERVICE_DISCOVERY_ADD_DEVICE_CONFIG_CFM);

    message->result = result;

    GATT_SD_MESSAGE_SEND(task, GATT_SERVICE_DISCOVERY_ADD_DEVICE_CONFIG_CFM, message);
}

void gattServiceDiscoveryRemoveDeviceConfigCfm(Task task,
                                  GattSdResult result,
                                  connection_id_t cid)
{
    MAKE_GATT_SD_MESSAGE(GATT_SERVICE_DISCOVERY_REMOVE_DEVICE_CONFIG_CFM);

    message->result = result;
    message->cid = cid;

    GATT_SD_MESSAGE_SEND(task, GATT_SERVICE_DISCOVERY_REMOVE_DEVICE_CONFIG_CFM, message);
}

void gattServiceDiscoveryFindServiceRangeCfm(Task task,
                                  GattSdResult result,
                                  connection_id_t cid,
                                  uint16 srvcInfoCount,
                                  GattSdSrvcInfo_t *srvcInfo)
{
    MAKE_GATT_SD_MESSAGE(GATT_SERVICE_DISCOVERY_FIND_SERVICE_RANGE_CFM);

    message->result = result;
    message->cid = cid;
    message->srvcInfoCount = srvcInfoCount;
    message->srvcInfo = srvcInfo;

    GATT_SD_MESSAGE_SEND(task, GATT_SERVICE_DISCOVERY_FIND_SERVICE_RANGE_CFM, message);
}

void gattSdAddDevice(GGSD *gatt_sd,
                                   connection_id_t cid)
{
    GattSdDeviceElement *devElem = 
        GATT_SD_DL_FIND_BY_CONNID(gatt_sd->deviceList, cid);
    if (!devElem)
    {
        devElem = GATT_SD_DL_ADD_DEVICE(&gatt_sd->deviceList);
        devElem->cid = cid;
        devElem->serviceListCount = 0;
    }
}

void gattSdAddService(GattSdDeviceElement *devElem,
                                   GattSdSrvcId srvcId,
                                   uint16 startHandle,
                                   uint16 endHandle,
                                   GattSdServiceType serviceType)
{
    GattSdServiceElement* srvcElem = NULL;
    /* Look for duplicate srvc info */
    srvcElem = GATT_SD_SL_ADD_SRVC(&devElem->serviceList);
    srvcElem->srvcId = srvcId;
    srvcElem->startHandle = startHandle;
    srvcElem->endHandle = endHandle;
    srvcElem->serviceType = serviceType;
}

GattSdSrvcId gattSdGetSrvcIdFromUuid128(const uint32* uuid)
{
    GattSdSrvcId srvcId = GATT_SD_INVALID_SRVC;
    uint8 index;

    for (index = 0; index < GATT_SD_SRVC_UUID_INFO_LIST_SIZE; index++)
    {
        if ((uuid[0] == GattSdSrvcUuidInfo[index].srvcUuid.uuid[0]) &&
            (uuid[1] == GattSdSrvcUuidInfo[index].srvcUuid.uuid[1]) &&
            (uuid[2] == GattSdSrvcUuidInfo[index].srvcUuid.uuid[2]) &&
            (uuid[3] == GattSdSrvcUuidInfo[index].srvcUuid.uuid[3]))
        {
            return GattSdSrvcUuidInfo[index].srvcId;
        }
    }
    return srvcId;
}

GattSdSrvcId gattSdGetSrvcIdFromUuid32(uint32 uuid)
{
    GattSdSrvcId srvcId = GATT_SD_INVALID_SRVC;
    uint8 index;

    for (index = 0; index < GATT_SD_SRVC_UUID_INFO_LIST_SIZE; index++)
    {
        if (uuid == GattSdSrvcUuidInfo[index].srvcUuid.uuid[0])
        {
            return GattSdSrvcUuidInfo[index].srvcId;
        }
    }
    return srvcId;
}

GattSdSrvcId gattSdGetSrvcIdFromUuid16(uint16 uuid)
{
    GattSdSrvcId srvcId = GATT_SD_INVALID_SRVC;
    uint8 index;

    for (index = 0; index < GATT_SD_SRVC_UUID_INFO_LIST_SIZE; index++)
    {
        uint16 srvcUuid = (uint16) GattSdSrvcUuidInfo[index].srvcUuid.uuid[0];
        if (srvcUuid == uuid)
        {
            return GattSdSrvcUuidInfo[index].srvcId;
        }
    }
    return srvcId;
}

static uint8 gattSdGetSetBitIndex(GattSdSrvcId srvcId)
{
    uint8 i = 0;
    while(srvcId)
    {
        srvcId = srvcId >> 1;
        i++;
    }
    return i;
}

static GattSdSrvcId gattSdGetNextSrvcUuid(const GGSD *gatt_sd,
                                                    GattSdSrvcUuid *uuid)
{
    GattSdSrvcId srvcId = GATT_SD_INVALID_SRVC;
    /* Get the current set bit index */
    uint8 idx = gattSdGetSetBitIndex(gatt_sd->curSrvcId);

    /* Look for next service id to trigger the discovery */
    for(; idx < GATT_SD_SRVC_UUID_INFO_LIST_SIZE; idx++)
    {
        if (GattSdSrvcUuidInfo[idx].srvcId & gatt_sd->srvcIds)
        {
            break;
        }
    }

    /* Update the service id and service uuid */
    if (idx < GATT_SD_SRVC_UUID_INFO_LIST_SIZE)
    {
        srvcId = GattSdSrvcUuidInfo[idx].srvcId;
        uuid->type = GattSdSrvcUuidInfo[idx].srvcUuid.type;
        memcpy(uuid->uuid, GattSdSrvcUuidInfo[idx].srvcUuid.uuid, GATT_SD_UUID_SIZE);
    }

    return srvcId;
}

bool gattSdDiscoverPrimaryServiceByUuid(GGSD *gatt_sd)
{
    bool status = FALSE;
    GattSdSrvcUuid uuid;

    /* Get the Next Service uuid */
    gatt_sd->curSrvcId = gattSdGetNextSrvcUuid(gatt_sd, (GattSdSrvcUuid *)&uuid);

    if (gatt_sd->curSrvcId)
    {
#ifdef SYNERGY_GATT_SD_ENABLE
        if (uuid.type == gatt_uuid16)
        {
            uint16 uuid16 = GATT_SD_UUID_GET_16(uuid.uuid);
            CsrBtGattDiscoverPrimaryServicesBy16BitUuidReqSend(gatt_sd->gattId,
                                                           gatt_sd->curCid,
                                                           uuid16);
        }
        else if ((uuid.type == gatt_uuid32) || (uuid.type == gatt_uuid128))
        {
            uint32 uuidValue[4];
            /* Change the UUID formate as required by GATT layer */
            GATT_SD_UUID_LITTLE_ENDIAN_FORMAT(uuid.uuid, uuidValue);
            CsrBtGattDiscoverPrimaryServicesBy128BitUuidReqSend(gatt_sd->gattId,
                                                gatt_sd->curCid,
                                                (uint8 *)uuidValue);
        }
#else
        GattDiscoverPrimaryServiceRequest(&gatt_sd->lib_task,
                                          gatt_sd->curCid,
                                          uuid.type,
                                          uuid.uuid);
#endif
        /* Update Gatt Service discovery state */
        gatt_sd->state = GATT_SRVC_DISC_STATE_INPROGRESS;
        status = TRUE;
    }
    return status;
}

static void gattSdHandlePrimaryServiceDiscovery(GGSD* gatt_sd,
                                                connection_id_t cid,
                                                gatt_uuid_type_t uuid_type,
                                                const uint8 *uuid,
                                                uint16 start_handle,
                                                uint16 end_handle)
{
    GattSdDeviceElement *devElem = 
        GATT_SD_DL_FIND_BY_CONNID(gatt_sd->deviceList, cid);
    if (devElem == NULL)
    {
        /* If device element is not present, then create the device 
         * element and add to the GATT SD device list.
         * Also, set the service list count for the device to 0 */
        devElem = GATT_SD_DL_ADD_DEVICE(&gatt_sd->deviceList);
        devElem->cid = cid;
        devElem->serviceListCount = 0;
    }

    GATT_SD_DEBUG_INFO(("Handle Primary Service Discovery\n"));
    GATT_SD_DEBUG_INFO(("CID 0x%x, SH 0x%x EH 0x%x, UUID_TYPE 0x%x\n",
                        cid, start_handle, end_handle, uuid_type));

    if (devElem)
    {
        GattSdSrvcId srvcId = GATT_SD_INVALID_SRVC;
        if (uuid_type == gatt_uuid16)
        {
            uint16 uuid16 = GATT_SD_UUID_GET_16(uuid);
            srvcId = gattSdGetSrvcIdFromUuid16(uuid16);
        }
        else if (uuid_type == gatt_uuid128)
        {
            srvcId = gattSdGetSrvcIdFromUuid128((uint32 *)uuid);
        }
        else if (uuid_type == gatt_uuid32)
        {
            uint32 uuid32 = GATT_SD_UUID_GET_32(uuid);
            srvcId = gattSdGetSrvcIdFromUuid32(uuid32);
        }
 
        if (srvcId != GATT_SD_INVALID_SRVC)
        {
            gattSdAddService(devElem, srvcId,
                           start_handle, end_handle,
                           GATT_SD_SERVICE_TYPE_PRIMARY);
            devElem->serviceListCount++;
        }
    }
}


static void gattSdInternalMsgHandler(Task task, MessageId id, Message msg)
{
    GATT_SD_UNUSED(task);

    switch (id)
    {
        case GATT_SD_INTERNAL_MSG_DISCOVERY_START:
        {
            gattSdDiscoveryStartInternal((const GATT_SD_INTERNAL_MSG_DISCOVERY_START_T*) msg);
        }
        break;
        case GATT_SD_INTERNAL_MSG_DISCOVERY_STOP:
        {
            gattSdDiscoveryStopInternal((const GATT_SD_INTERNAL_MSG_DISCOVERY_START_T*) msg);
        }
        break;
        case GATT_SD_INTERNAL_MSG_GET_DEVICE_CONFIG:
        {
            gattSdGetDeviceConfigInternal((const GATT_SD_INTERNAL_MSG_GET_DEVICE_CONFIG_T*) msg);
        }
        break;
        case GATT_SD_INTERNAL_MSG_ADD_DEVICE_CONFIG:
        {
            gattSdAddDeviceConfigInternal((const GATT_SD_INTERNAL_MSG_ADD_DEVICE_CONFIG_T*) msg);
        }
        break;
        case GATT_SD_INTERNAL_MSG_REMOVE_DEVICE_CONFIG:
        {
            gattSdRemoveDeviceConfigInternal((const GATT_SD_INTERNAL_MSG_REMOVE_DEVICE_CONFIG_T*) msg);
        }
        break;
        case GATT_SD_INTERNAL_MSG_FIND_SERVICE_RANGE:
        {
            gattSdFindServiceRangeInternal((const GATT_SD_INTERNAL_MSG_FIND_SERVICE_RANGE_T*) msg);
        }
        break;
        default:
        {
            /* Internal unrecognised messages */
            GATT_SD_DEBUG_PANIC(("Gatt SD Internal Msg not handled [0x%x]\n", id));
            break;
        }
    }
}


#ifdef SYNERGY_GATT_SD_ENABLE

static void gattSdGetUuidFromBuffer(const uint8* buf, gatt_uuid_type_t uuidType, uint32 *uuid)
{
    if (uuidType == gatt_uuid16)
    {
        uuid[0] = GATT_SD_UUID_GET_16(buf);
    }
    else if (uuidType == gatt_uuid32)
    {
        uuid[0] = GATT_SD_UUID_GET_32(buf);
    }
    else if (uuidType == gatt_uuid128)
    {
        /* Reverse the Service UUID */
        uuid[0] = (uint32) (buf[12] | (buf[13] << 8) | (buf[14] << 16) | (buf[15] << 24));
        uuid[1] = (uint32) (buf[8] | (buf[9] << 8) | (buf[10] << 16) | (buf[11] << 24));
        uuid[2] = (uint32) (buf[4] | (buf[5] << 8) | (buf[6] << 16) | (buf[7] << 24));
        uuid[3] = (uint32) (buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24));
    }
}

static void gattSdGattMsgHandler(Task task, MessageId id, Message msg)
{
    GGSD *gatt_sd = gattServiceDiscoveryGetInstance();

    switch (id)
    {
        case CSR_BT_GATT_REGISTER_CFM:
        {
            /* GATT Register confirmation received with the gattId */
            CsrBtGattRegisterCfm* cfm = (CsrBtGattRegisterCfm *) msg;
            if(cfm->resultCode == CSR_BT_GATT_RESULT_SUCCESS)
            {
                gatt_sd->gattId = cfm->gattId;
            }
            break;
        }
        case CSR_BT_GATT_DISCOVER_SERVICES_IND:
        {
            /* Primary service discovery indication received */
            CsrBtGattDiscoverServicesInd* ind = 
                    (CsrBtGattDiscoverServicesInd *) msg;
            uint32 uuidValue[4];

            /* Get the service uuid type */
            gatt_uuid_type_t uuidType = GATT_SD_GET_UUID_TYPE(ind->uuid.length);
            /* Get the Service UUID from the buffer */
            gattSdGetUuidFromBuffer(ind->uuid.uuid, uuidType, uuidValue);

            gattSdHandlePrimaryServiceDiscovery(gatt_sd, ind->btConnId,
                    uuidType,(uint8 *)uuidValue, ind->startHandle, ind->endHandle);
            break;
        }
        case CSR_BT_GATT_DISCOVER_SERVICES_CFM:
        {
            /* Primary service discovery confirmation received */
            if (gatt_sd->discoverByUuid)
            {
                if (!gattSdDiscoverPrimaryServiceByUuid(gatt_sd))
                {
                    GATT_SD_DEBUG_INFO(("Primary Service Discovery : Complete\n"));
                    gatt_sd->state = GATT_SRVC_DISC_STATE_IDLE;
                    gattServiceDiscoveryStartCfm(gatt_sd->app_task,
                        GATT_SD_RESULT_SUCCESS, gatt_sd->curCid);
                }
            }
            else
            {
                GATT_SD_DEBUG_INFO(("All Primary Service Discovery : Complete\n"));
                gatt_sd->state = GATT_SRVC_DISC_STATE_IDLE;
                gattServiceDiscoveryStartCfm(gatt_sd->app_task,
                    GATT_SD_RESULT_SUCCESS, gatt_sd->curCid);
            }
            break;
        }
        default:
        {
            /* Internal unrecognised messages */
            GATT_SD_DEBUG_PANIC(("Gatt Msg not handled [0x%x]\n", id));
            break;
        }
    }
}

void GattServiceDiscoveryMsgHandler(void **gash)
{
    uint16 eventClass = 0;
    Task task;
    uint16 id;
    void* msg;

    GGSD *inst = (GGSD *) *gash;
    task = inst->app_task;

    if (CsrSchedMessageGet(&eventClass, &msg))
    {
        id = *(GattSdPrim *)msg;
        switch (eventClass)
        {
            case CSR_BT_GATT_PRIM:
            {
                gattSdGattMsgHandler(task, id, msg);
                break;
            }
            case GATT_SRVC_DISC_PRIM:
            {
                gattSdInternalMsgHandler(task, id, msg);
                break;
            }
            default:
            {
                break;
            }
        }
    }
    GATT_SD_FREE(msg);
}

#else

static void gattSdGattMsgHandler(Task task, MessageId id, Message msg)
{
    GGSD *gatt_sd = (GGSD*)task;

    switch (id)
    {
        case GATT_DISCOVER_ALL_PRIMARY_SERVICES_CFM:
        {
            GATT_DISCOVER_ALL_PRIMARY_SERVICES_CFM_T* cfm = 
                (GATT_DISCOVER_ALL_PRIMARY_SERVICES_CFM_T *) msg;

            if (cfm->status == gatt_status_success)
            {
                GATT_SD_DEBUG_INFO(("ALL Primary Services Discovery Cfm\n"));
                GATT_SD_DEBUG_INFO(("CID 0x%x, SH 0x%x EH 0x%x, UUID 0x%x, MORE 0x%x\n",
                        cfm->cid, cfm->handle, cfm->end, cfm->uuid[0], cfm->more_to_come));
                gattSdHandlePrimaryServiceDiscovery(gatt_sd, cfm->cid,
                    cfm->uuid_type, (uint8 *) cfm->uuid, cfm->handle, cfm->end);
            }
            if (!cfm->more_to_come)
            {
                GATT_SD_DEBUG_INFO(("ALL Primary Services Discovery : Complete\n"));
                gatt_sd->state = GATT_SRVC_DISC_STATE_IDLE;
                gattServiceDiscoveryStartCfm(gatt_sd->app_task,
                    GATT_SD_RESULT_SUCCESS, gatt_sd->curCid);
            }
        }
        break;
        case GATT_DISCOVER_PRIMARY_SERVICE_CFM:
        {
            GATT_DISCOVER_PRIMARY_SERVICE_CFM_T* cfm = 
                (GATT_DISCOVER_PRIMARY_SERVICE_CFM_T *) msg;
            if (cfm->status == gatt_status_success)
            {
                GATT_SD_DEBUG_INFO(("Primary Service Discovery Cfm\n"));
                GATT_SD_DEBUG_INFO(("CID 0x%x, SH 0x%x EH 0x%x, UUID 0x%x, MORE 0x%x\n",
                        cfm->cid, cfm->handle, cfm->end, cfm->uuid[0], cfm->more_to_come));
                gattSdHandlePrimaryServiceDiscovery(gatt_sd, cfm->cid,
                    cfm->uuid_type, (uint8 *) cfm->uuid, cfm->handle, cfm->end);
            }

            if (gatt_sd->discoverByUuid)
            {
                if (!gattSdDiscoverPrimaryServiceByUuid(gatt_sd))
                {
                    GATT_SD_DEBUG_INFO(("Primary Service Discovery : Complete\n"));
                    gatt_sd->state = GATT_SRVC_DISC_STATE_IDLE;
                    gattServiceDiscoveryStartCfm(gatt_sd->app_task,
                        GATT_SD_RESULT_SUCCESS, gatt_sd->curCid);
                }
            }
            break;
        }
        default:
        {
            /* Internal unrecognised messages */
            GATT_SD_DEBUG_PANIC(("Gatt Msg not handled [0x%x]\n", id));
            break;
        }
    }
}


void gattServiceDiscoveryMsgHandler(Task task, MessageId id, Message msg)
{
    if ((id >= GATT_MESSAGE_BASE) && (id < GATT_MESSAGE_TOP))
    {
        gattSdGattMsgHandler(task, id, msg);
    }
    /* Check message is internal Message */
    else if((id >= GATT_SD_INTERNAL_MSG_BASE) && (id < GATT_SD_INTERNAL_MSG_TOP))
    {
        gattSdInternalMsgHandler(task, id, msg);
    }
}

#endif


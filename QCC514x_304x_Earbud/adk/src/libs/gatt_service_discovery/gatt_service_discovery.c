/******************************************************************************
 Copyright (c) 2020 Qualcomm Technologies International, Ltd.
 All Rights Reserved.
 Qualcomm Technologies International, Ltd. Confidential and Proprietary.

 REVISION:      $Revision: #2 $
******************************************************************************/

#include "gatt_service_discovery.h"
#include "gatt_service_discovery_private.h"
#include "gatt_service_discovery_init.h"
#include "gatt_service_discovery_handler.h"
#include "gatt_service_discovery_debug.h"

#ifdef SYNERGY_GATT_SD_ENABLE
#include "csr_bt_gatt_lib.h"
#endif


void GattServiceDiscoveryRegisterSupportedServicesReq(Task appTask,
            GattSdSrvcId srvcIds, bool discoverByUuid)
{
    GGSD *gatt_sd = gattServiceDiscoveryGetInstance();

    /* Check for application phandle */
    if (gatt_sd->app_task == APP_TASK_INVALID)
    {
        GATT_SD_DEBUG_INFO(("Register Supported Services, updating appTask\n"));
        gatt_sd->app_task = appTask;
    }

    if (gatt_sd->app_task != appTask)
    {
        GATT_SD_DEBUG_INFO(("Register Supported Services Service not permitted from other task\n"));
        /* Send result : Register not permitted from other application task */
        gattServiceDiscoveryRegisterSupportedServicesCfm(appTask,
                        GATT_SD_RESULT_REGISTER_NOT_PERMITTED);
    }
    else
    {
        GATT_SD_DEBUG_INFO(("Register Supported Services Service Ids 0x%x, DiscoveryByUuid %d\n",
                            srvcIds, discoverByUuid));
        /* By Default GATT and GAP service will be discovered */
        gatt_sd->srvcIds = srvcIds | GATT_SD_GATT_SRVC | GATT_SD_GAP_SRVC;
        gatt_sd->curSrvcId = GATT_SD_INVALID_SRVC;
        gatt_sd->discoverByUuid = discoverByUuid;
        gattServiceDiscoveryRegisterSupportedServicesCfm(appTask,
            GATT_SD_RESULT_SUCCESS);
    }
}


void GattServiceDiscoveryStartReq(Task appTask, connection_id_t cid)
{
    GGSD *gatt_sd = gattServiceDiscoveryGetInstance();
    GattSdResult result = GATT_SD_RESULT_ERROR;

    if (gatt_sd->app_task != appTask)
    {
        GATT_SD_PANIC(("GATT Service discovery Not initialised!"));
    }
    else
    {
        GATT_SD_DEBUG_INFO(("Start Service Discovery CID 0x%x\n", cid));
        /* Check for Discover by UUIDs flag and List of Service IDs  */
        if (gatt_sd->discoverByUuid &&
            (gatt_sd->srvcIds == GATT_SD_INVALID_SRVC))
        {
            result = GATT_SD_RESULT_SRVC_LIST_EMPTY;
            /* Send operation not allowed */
            gattServiceDiscoveryStartCfm(appTask, result, cid);
        }
        else
        {
            MAKE_GATT_SD_MESSAGE(GATT_SD_INTERNAL_MSG_DISCOVERY_START);
            message->cid = cid;
            GATT_SD_MESSAGE_SEND_INTERNAL(&gatt_sd->lib_task,
                         GATT_SD_INTERNAL_MSG_DISCOVERY_START,
                         message);
        }
    }
}

void GattServiceDiscoveryStopReq(Task appTask, connection_id_t cid)
{
    GGSD *gatt_sd = gattServiceDiscoveryGetInstance();

    /* Check for application task */
    if (gatt_sd->app_task != appTask)
    {
        GATT_SD_PANIC(("GATT Service discovery Not initialised!"));
    }
    else
    {
        MAKE_GATT_SD_MESSAGE(GATT_SD_INTERNAL_MSG_DISCOVERY_STOP);
        message->cid = cid;
        GATT_SD_MESSAGE_SEND_INTERNAL(&gatt_sd->lib_task,
                     GATT_SD_INTERNAL_MSG_DISCOVERY_STOP,
                     message);
    }
}

void GattServiceDiscoveryGetDeviceConfigReq(Task appTask, connection_id_t cid)
{
    GGSD *gatt_sd = gattServiceDiscoveryGetInstance();

    /* Check for application task */
    if (gatt_sd->app_task != appTask)
    {
        GATT_SD_PANIC(("GATT Service discovery Not initialised!"));
    }
    else
    {
        MAKE_GATT_SD_MESSAGE(GATT_SD_INTERNAL_MSG_GET_DEVICE_CONFIG);
        message->cid = cid;
        GATT_SD_MESSAGE_SEND_INTERNAL(&gatt_sd->lib_task,
                     GATT_SD_INTERNAL_MSG_GET_DEVICE_CONFIG,
                     message);
    }
}

void GattServiceDiscoveryAddDeviceConfigReq(Task appTask,
                                    connection_id_t cid,
                                    uint16 srvcInfoCount,
                                    GattSdSrvcInfo_t *srvcInfo)
{
    GGSD *gatt_sd = gattServiceDiscoveryGetInstance();

    /* Check for application task */
    if (gatt_sd->app_task != appTask)
    {
        GATT_SD_PANIC(("GATT Service discovery Not initialised!"));
    }
    else
    {
        MAKE_GATT_SD_MESSAGE(GATT_SD_INTERNAL_MSG_ADD_DEVICE_CONFIG);
        message->srvcInfo = (GattSdSrvcInfo_t*) GATT_SD_MALLOC(sizeof(GattSdSrvcInfo_t)*srvcInfoCount);

        message->cid = cid;
        message->srvcInfoCount = srvcInfoCount;
        memcpy(message->srvcInfo, srvcInfo, sizeof(GattSdSrvcInfo_t) * srvcInfoCount);
        GATT_SD_MESSAGE_SEND_INTERNAL(&gatt_sd->lib_task,
                     GATT_SD_INTERNAL_MSG_ADD_DEVICE_CONFIG,
                     message);
    }
}

void GattServiceDiscoveryRemoveDeviceConfigReq(Task appTask,
                                                connection_id_t cid)
{
    GGSD *gatt_sd = gattServiceDiscoveryGetInstance();

    /* Check for application task */
    if (gatt_sd->app_task != appTask)
    {
        GATT_SD_PANIC(("GATT Service discovery Not initialised!"));
    }
    else
    {
        MAKE_GATT_SD_MESSAGE(GATT_SD_INTERNAL_MSG_REMOVE_DEVICE_CONFIG);
        message->cid = cid;
        GATT_SD_MESSAGE_SEND_INTERNAL(&gatt_sd->lib_task,
                     GATT_SD_INTERNAL_MSG_REMOVE_DEVICE_CONFIG,
                     message);
    }
}


void GattServiceDiscoveryFindServiceRangeReq(Task task,
                                            connection_id_t cid,
                                            GattSdSrvcId srvcIds)
{
    GGSD *gatt_sd = gattServiceDiscoveryGetInstance();

    MAKE_GATT_SD_MESSAGE(GATT_SD_INTERNAL_MSG_FIND_SERVICE_RANGE);
    message->task = task;
    message->cid = cid;
    message->srvcIds = srvcIds;
    GATT_SD_MESSAGE_SEND_INTERNAL(&gatt_sd->lib_task,
                 GATT_SD_INTERNAL_MSG_FIND_SERVICE_RANGE,
                 message);
}


/******************************************************************************
 *                      GATT Service Discovery Internal API                   *
 ******************************************************************************/
void gattSdDiscoveryStartInternal(const GATT_SD_INTERNAL_MSG_DISCOVERY_START_T* param)
{
    GGSD *gatt_sd = gattServiceDiscoveryGetInstance();
    GattSdResult result = GATT_SD_RESULT_INPROGRESS;

    /* Deinitialize the service info list for the device  */
    GattSdDeviceElement* devElem =
        GATT_SD_DL_FIND_BY_CONNID(gatt_sd->deviceList, param->cid);
    if (devElem)
    {
        while (devElem->serviceList) {
            GattSdServiceElement* elem = devElem->serviceList;
            devElem->serviceList = devElem->serviceList->next;
            GATT_SD_FREE(elem);
        }
        devElem->serviceListCount = 0;
    }
    GATT_SD_DEBUG_INFO(("Start Service Discovery CID 0x%x\n", param->cid));
    /* Update Search connection id */
    gatt_sd->curCid = param->cid;
    /* Start Primary Service Discovery by uuid or all */
    if (gatt_sd->discoverByUuid)
    {
        gatt_sd->curSrvcId = GATT_SD_INVALID_SRVC;
        /* Discover Primary service by UUID for the supported services */
        gattSdDiscoverPrimaryServiceByUuid(gatt_sd);
    }
    else
    {
        GATT_SD_DEBUG_INFO(("Discover by All Primary Service discovery\n"));
#ifdef SYNERGY_GATT_SD_ENABLE
        CsrBtGattDiscoverAllPrimaryServicesReqSend(gatt_sd->gattId, gatt_sd->curCid);
#else
        GattDiscoverAllPrimaryServicesRequest(&gatt_sd->lib_task, gatt_sd->curCid);
#endif
    }

    GATT_SD_DEBUG_INFO(("Start Service Discovery CID 0x%x, Result 0x%x\n",
                        param->cid, result));
    gattServiceDiscoveryStartCfm(gatt_sd->app_task, result, param->cid);
}


void gattSdDiscoveryStopInternal(const GATT_SD_INTERNAL_MSG_DISCOVERY_STOP_T* param)
{
    GGSD *gatt_sd = gattServiceDiscoveryGetInstance();
    GattSdResult result = GATT_SD_RESULT_SUCCESS;

    GattSdDeviceElement* devElem =
        GATT_SD_DL_FIND_BY_CONNID(gatt_sd->deviceList, param->cid);
    if (devElem)
    {
        /* Stop Service Discovery based on uuid or all */
        /* Set Gatt Service discovery state to Idle */
        gatt_sd->state = GATT_SRVC_DISC_STATE_IDLE;
    }
    else
    {
        /* Send device not found */
        result = GATT_SD_RESULT_DEVICE_NOT_FOUND;
    }

    GATT_SD_DEBUG_INFO(("Stop Service Discovery CID 0x%x, Result 0x%x\n",
                        param->cid, result));
    gattServiceDiscoveryStopCfm(gatt_sd->app_task, result, param->cid);
}

void gattSdGetDeviceConfigInternal(const GATT_SD_INTERNAL_MSG_GET_DEVICE_CONFIG_T* param)
{
    GGSD *gatt_sd = gattServiceDiscoveryGetInstance();
    GattSdResult result = GATT_SD_RESULT_DEVICE_NOT_FOUND;
    GattSdDeviceElement* devElem = NULL;
    GattSdSrvcInfo_t* srvcInfo = NULL;
    uint16 srvcInfoCount = 0;

    /* Find the device based on cid */
    devElem =  GATT_SD_DL_FIND_BY_CONNID(gatt_sd->deviceList, param->cid);
    if (devElem)
    {
        GattSdServiceElement* srvcElem = devElem->serviceList;
        srvcInfo = (GattSdSrvcInfo_t *) GATT_SD_MALLOC(sizeof(GattSdSrvcInfo_t)* devElem->serviceListCount);
        while(srvcElem != NULL)
        {
            srvcInfo[srvcInfoCount].startHandle = srvcElem->startHandle;
            srvcInfo[srvcInfoCount].endHandle = srvcElem->endHandle;
            srvcInfo[srvcInfoCount].srvcId = srvcElem->srvcId;
            srvcInfoCount++;
            srvcElem = srvcElem->next;
        }
        result = GATT_SD_RESULT_SUCCESS;
        GATT_SD_DEBUG_INFO(("Get Device Config CID 0x%x, SrvcInfoCount 0x%x\n",
                            param->cid, srvcInfoCount));
    }

    GATT_SD_DEBUG_INFO(("Get Device Config CID 0x%x, Result 0x%x\n",
                        param->cid, result));
    gattServiceDiscoveryGetDeviceConfigCfm(gatt_sd->app_task, result,
        param->cid, srvcInfoCount, srvcInfo);
}

void gattSdAddDeviceConfigInternal(const GATT_SD_INTERNAL_MSG_ADD_DEVICE_CONFIG_T* param)
{
    GGSD *gatt_sd = gattServiceDiscoveryGetInstance();
    GattSdResult result = GATT_SD_RESULT_DEVICE_CONFIG_PRESENT;
    GattSdDeviceElement* devElem = NULL;
    GattSdServiceElement *srvcElem = NULL;
    uint8 i;

    devElem = GATT_SD_DL_FIND_BY_CONNID(gatt_sd->deviceList, param->cid);
    if(devElem == NULL)
    {
        GATT_SD_DEBUG_INFO(("Add Device Config CID 0x%x, SrvcInfoListCount 0x%x\n",
                            param->cid, param->srvcInfoCount));
        devElem = GATT_SD_DL_ADD_DEVICE(&gatt_sd->deviceList);
        devElem->cid = param->cid;
        devElem->serviceListCount = 0;
        for (i = 0; i < param->srvcInfoCount; i++)
        {
            srvcElem = GATT_SD_SL_ADD_SRVC(&devElem->serviceList);
            srvcElem->srvcId = param->srvcInfo[i].srvcId;
            srvcElem->startHandle = param->srvcInfo[i].startHandle;
            srvcElem->endHandle = param->srvcInfo[i].endHandle;
            devElem->serviceListCount++;
        }
        result = GATT_SD_RESULT_SUCCESS;
    }

    /* Free the service info memory */
    GATT_SD_FREE(param->srvcInfo);

    GATT_SD_DEBUG_INFO(("Add Device Config CID 0x%x, Result 0x%x\n",
                            param->cid, result));
    gattServiceDiscoveryAddDeviceConfigCfm(gatt_sd->app_task, result);
}

void gattSdRemoveDeviceConfigInternal(const GATT_SD_INTERNAL_MSG_REMOVE_DEVICE_CONFIG_T* param)
{
    GGSD *gatt_sd = gattServiceDiscoveryGetInstance();
    GattSdResult result = GATT_SD_RESULT_DEVICE_NOT_FOUND;

    GattSdDeviceElement *devElem =
            GATT_SD_DL_FIND_BY_CONNID(gatt_sd->deviceList, param->cid);
    if(devElem != NULL)
    {
        GATT_SD_DEBUG_INFO(("Remove Device Config CID 0x%x\n", param->cid));
        /* Remove device element from the device list */
        GATT_SD_DL_REMOVE_DEVICE(&gatt_sd->deviceList, devElem);
        /* Remove the service list from the device element */
        GATT_SD_SL_CLEANUP(devElem->serviceList);
        GATT_SD_FREE(devElem);
        result = GATT_SD_RESULT_SUCCESS;
    }

    GATT_SD_DEBUG_INFO(("Remove Device Config CID 0x%x, Result 0x%x\n",
                                param->cid, result));
    gattServiceDiscoveryRemoveDeviceConfigCfm(gatt_sd->app_task, result, param->cid);
}

void gattSdFindServiceRangeInternal(const GATT_SD_INTERNAL_MSG_FIND_SERVICE_RANGE_T* param)
{
    GGSD *gatt_sd = gattServiceDiscoveryGetInstance();
    GattSdResult result = GATT_SD_RESULT_SUCCESS;
    GattSdDeviceElement* devElem = NULL;
    GattSdSrvcInfo_t* srvcInfo = NULL;
    uint16 srvcInfoCount = 0;

    /* Find the device based on cid */
    devElem = GATT_SD_DL_FIND_BY_CONNID(gatt_sd->deviceList, param->cid);
    if (devElem)
    {
        /* Transverse through the list to find the number of services
         * as per the service ids present in the device service list  */
        GattSdServiceElement *srvcElem = devElem->serviceList;
        while(srvcElem)
        {
            if (srvcElem->srvcId & param->srvcIds)
                srvcInfoCount++;

            srvcElem = srvcElem->next;
        }

        if (srvcInfoCount)
        {
            uint8 i = 0;
            srvcElem = devElem->serviceList;
            /* Allocate memory for the list of service info to be sent to
             * application */
            srvcInfo = (GattSdSrvcInfo_t *) malloc (sizeof(GattSdSrvcInfo_t) * srvcInfoCount);
            while(srvcElem)
            {
                if (srvcElem->srvcId & param->srvcIds)
                {
                    srvcInfo[i].startHandle = srvcElem->startHandle;
                    srvcInfo[i].endHandle = srvcElem->endHandle;
                    srvcInfo[i].srvcId = srvcElem->srvcId;
                    i++;
                }
                srvcElem = srvcElem->next;
            }
        }
        GATT_SD_DEBUG_INFO(("Find Service Range CID 0x%x, SrvcIds 0x%x, SrvcInfoCount 0x%x\n",
                            param->cid, param->srvcIds, srvcInfoCount));
    }
    else
    {
        /* Send device not found */
        result = GATT_SD_RESULT_DEVICE_NOT_FOUND;
    }

    GATT_SD_DEBUG_INFO(("Find Service Range CID 0x%x, SrvcIds 0x%x, Result 0x%x\n",
                        param->cid, param->srvcIds, result));
    gattServiceDiscoveryFindServiceRangeCfm(param->task, result, param->cid, srvcInfoCount, srvcInfo);
}



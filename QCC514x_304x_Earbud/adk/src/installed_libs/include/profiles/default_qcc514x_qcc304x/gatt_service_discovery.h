/******************************************************************************
 Copyright (c) 2020 Qualcomm Technologies International, Ltd.
 All Rights Reserved.
 Qualcomm Technologies International, Ltd. Confidential and Proprietary.

 REVISION:      $Revision: #2 $
******************************************************************************/
#ifndef GATT_SERVICE_DISCOVERY_H__
#define GATT_SERVICE_DISCOVERY_H__

#include "gatt.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint16 GattSdPrim;
typedef uint16 GattSdResult;

/* ---------- Defines the GATT Service Discovery (SD) GattSdResult ----------*/
#define GATT_SD_RESULT_SUCCESS                         ((GattSdResult) (0x0000))
#define GATT_SD_RESULT_INPROGRESS                      ((GattSdResult) (0x0001))
#define GATT_SD_RESULT_UNACCEPTABLE_PARAMETER          ((GattSdResult) (0x0002))
#define GATT_SD_RESULT_DEVICE_NOT_FOUND                ((GattSdResult) (0x0003))
#define GATT_SD_RESULT_SRVC_LIST_EMPTY                 ((GattSdResult) (0x0004))
#define GATT_SD_RESULT_DEVICE_CONFIG_PRESENT           ((GattSdResult) (0x0005))
#define GATT_SD_RESULT_REGISTER_NOT_PERMITTED          ((GattSdResult) (0x0006))
#define GATT_SD_RESULT_ERROR                           ((GattSdResult) (0x0007))


/*******************************************************************************
 * Primitive definitions
 *******************************************************************************/
#ifdef SYNERGY_GATT_SD_ENABLE
#define GATT_SERVICE_DISCOVERY_MESSAGE_BASE                               (0x6880)
#endif

#define GATT_SERVICE_DISCOVERY_REGISTER_SUPPORTED_SERVICES_CFM            ((GattSdPrim) (0x0000 + GATT_SERVICE_DISCOVERY_MESSAGE_BASE))
#define GATT_SERVICE_DISCOVERY_START_CFM                                  ((GattSdPrim) (0x0001 + GATT_SERVICE_DISCOVERY_MESSAGE_BASE))
#define GATT_SERVICE_DISCOVERY_STOP_CFM                                   ((GattSdPrim) (0x0002 + GATT_SERVICE_DISCOVERY_MESSAGE_BASE))
#define GATT_SERVICE_DISCOVERY_GET_DEVICE_CONFIG_CFM                      ((GattSdPrim) (0x0003 + GATT_SERVICE_DISCOVERY_MESSAGE_BASE))
#define GATT_SERVICE_DISCOVERY_ADD_DEVICE_CONFIG_CFM                      ((GattSdPrim) (0x0004 + GATT_SERVICE_DISCOVERY_MESSAGE_BASE))
#define GATT_SERVICE_DISCOVERY_REMOVE_DEVICE_CONFIG_CFM                   ((GattSdPrim) (0x0005 + GATT_SERVICE_DISCOVERY_MESSAGE_BASE))
#define GATT_SERVICE_DISCOVERY_FIND_SERVICE_RANGE_CFM                     ((GattSdPrim) (0x0006 + GATT_SERVICE_DISCOVERY_MESSAGE_BASE))
#define GATT_SERVICE_DISCOVERY_MESSAGE_TOP                                (0x0007 + GATT_SERVICE_DISCOVERY_MESSAGE_BASE)

/*******************************************************************************
 * End primitive definitions
 *******************************************************************************/
/* Gatt Service Ids for the supported GATT Services
 * It is a bitwise value where each bit represents a specfic GATT Service
 * Bit_7 to Bit_31 are RFU */
typedef uint32 GattSdSrvcId;
#define GATT_SD_INVALID_SRVC         (0x00000000u)
#define GATT_SD_GATT_SRVC            (0x00000001u)
#define GATT_SD_GAP_SRVC             (0x00000002u)
#define GATT_SD_CSIS_SRVC            (0x00000004u)
#define GATT_SD_PACS_SRVC            (0x00000008u)
#define GATT_SD_ASCS_SRVC            (0x00000010u)
#define GATT_SD_VCS_SRVC             (0x00000020u)


typedef struct  _GattSdSrvcInfo
{
    GattSdSrvcId                 srvcId;
    uint16                       startHandle;
    uint16                       endHandle;
} GattSdSrvcInfo_t;

/*!
    @brief GATT SD library message sent as a result of calling the
           GattServiceDiscoveryRegisterSupportedServices API.
*/
typedef struct
{
#ifdef SYNERGY_GATT_SD_ENABLE
    GattSdPrim                  type;
#endif
    GattSdResult                result;
} GATT_SERVICE_DISCOVERY_REGISTER_SUPPORTED_SERVICES_CFM_T;

/*!
    @brief GATT SD library message sent as a result of calling the
           GattServiceDiscoveryStart API.
*/
typedef struct
{
#ifdef SYNERGY_GATT_SD_ENABLE
    GattSdPrim                  type;
#endif
    GattSdResult                result;
    connection_id_t             cid;
} GATT_SERVICE_DISCOVERY_START_CFM_T;

/*!
    @brief GATT SD library message sent as a result of calling the
           GattServiceDiscoveryStop API.
*/
typedef struct
{
#ifdef SYNERGY_GATT_SD_ENABLE
    GattSdPrim                  type;
#endif
    GattSdResult                result;
    connection_id_t             cid;
} GATT_SERVICE_DISCOVERY_STOP_CFM_T;

/*!
    @brief GATT SD library message sent as a result of calling the
           GattServiceDiscoveryGetDeviceConfig API.

    Note:- The memory for srvcInfo should be free by the receving module.
*/
typedef struct
{
#ifdef SYNERGY_GATT_SD_ENABLE
    GattSdPrim                  type;
#endif
    GattSdResult                result;
    connection_id_t             cid;
    uint16                      srvcInfoCount;
    GattSdSrvcInfo_t           *srvcInfo;
} GATT_SERVICE_DISCOVERY_GET_DEVICE_CONFIG_CFM_T;

/*!
    @brief GATT SD library message sent as a result of calling the
           GattServiceDiscoveryAddDeviceConfig API.
*/
typedef struct
{
#ifdef SYNERGY_GATT_SD_ENABLE
    GattSdPrim                  type;
#endif
    GattSdResult                result;
    connection_id_t             cid;
} GATT_SERVICE_DISCOVERY_ADD_DEVICE_CONFIG_CFM_T;

/*!
    @brief GATT SD library message sent as a result of calling the
           GattServiceDiscoveryRemoveDeviceConfig API.
*/
typedef struct
{
#ifdef SYNERGY_GATT_SD_ENABLE
    GattSdPrim                  type;
#endif
    GattSdResult                result;
    connection_id_t             cid;
} GATT_SERVICE_DISCOVERY_REMOVE_DEVICE_CONFIG_CFM_T;

/*!
    @brief GATT SD library message sent as a result of calling the
           GattServiceDiscoveryFindServiceRange API.

    Note:- The memory for srvcInfo should be free by the receving module.
*/
typedef struct
{
#ifdef SYNERGY_GATT_SD_ENABLE
    GattSdPrim                  type;
#endif
    GattSdResult                result;
    connection_id_t             cid;
    uint16                      srvcInfoCount;
    GattSdSrvcInfo_t           *srvcInfo;
} GATT_SERVICE_DISCOVERY_FIND_SERVICE_RANGE_CFM_T;


#ifdef SYNERGY_GATT_SD_ENABLE
/*!
    @brief Initialises the GATT Service Discovery Library.

    @param gash Allocate the GATT Service Discovery.library instance and assign
           it to the gash.
           This instance will be passed as the message handler function
           parameter called by the scheduler to process the message send to
           GATT Service Discovery library.

    @return void
*/
void GattServiceDiscoveryInit(void **gash);


/*!
    @brief De-Initialises the GATT Service Discovery Library.

    @return void
*/
    void GattServiceDiscoveryDeinit(void **gash);

#else

/*!
    @brief Initialises the GATT Service Discovery Library.

           GattServiceDiscoveryInit() will cause a panic if malloc fails.

    @param appTask The task that initialised the GATT Service Discovery.library
           GATT Service Discovery messages will be sent to this task, must
           not be NULL.

    @return FALSE if task is NULL or already initialized, otherwise TRUE.
*/
bool GattServiceDiscoveryInit(Task appTask);

/*!
    @brief De-Initialises the GATT Service Discovery Library.

    @return void
*/
void GattServiceDiscoveryDeinit(void);

#endif

/*!
    @brief Register all the Supported GATT services which should be discovered
           GATT Service Discovery module.
           Based on the flag discoverByUuid, It discover services using
           Discover All Primary Service procedure or Discover Primary Service
           by UUID procedure.

    @param appTask Application task
    @param srvcIds Bitwise values of the supported services listed in
                   GattSdSrvcId
    @param discoverByUuid Flag to enable Service Discovery by UUID or discover
                          all primary services

    @return GATT_SERVICE_DISCOVERY_REGISTER_SUPPORTED_SERVICES_CFM_T confirmation
            message sent to application with the result.
*/
void GattServiceDiscoveryRegisterSupportedServicesReq(Task appTask,
                                   GattSdSrvcId srvcIds,
                                   bool discoverByUuid);

/*!
    @brief Start service discovery of the remote device

           Based on the flag discoverByUuid, It discover services using
           Discover All Primary Service procedure or Discover Primary Service
           by UUID procedure.

    @param appTask Application task
    @param cid Connection Id of the remote device

    @return GATT_SERVICE_DISCOVERY_START_CFM_T confirmation message sent to
            application with the result.
            On service discovery started, In Progress result will be sent
            to application.
            On service discovery completion, Success result will be sent
            to application.
*/
void GattServiceDiscoveryStartReq(Task appTask, connection_id_t cid);


/*!
    @brief Stop service discovery of the remote device

    @param appTask Application task
    @param cid Connection Id of the remote device

    @return GATT_SERVICE_DISCOVERY_STOP_CFM_T confirmation message sent to
            application with the result.
*/
void GattServiceDiscoveryStopReq(Task appTask, connection_id_t cid);


/*!
    @brief Get Remote Device configuration from Gatt Service Discovery
           module.

    @param appTask Application task
    @param cid Connection Id of the remote device

    @return GATT_SERVICE_DISCOVERY_GET_DEVICE_CONFIG_CFM_T confirmation
            message sent to application with the result.
*/
void GattServiceDiscoveryGetDeviceConfigReq(Task appTask, connection_id_t cid);


/*!
    @brief Add Remote Device Configuration to GATT Service Discovery
           module.

    @param appTask Application task
    @param cid Connection Id of the remote device
    @param srvcInfoListCount
    @param srvcInfoList

    @return GATT_SERVICE_DISCOVERY_ADD_DEVICE_CONFIG_CFM_T confirmation
            message sent to application with the result.
*/
void GattServiceDiscoveryAddDeviceConfigReq(Task appTask,
                            connection_id_t cid,
                            uint16 srvcInfoListCount,
                            GattSdSrvcInfo_t *srvcInfoList);


/*!
    @brief Remove Remote device Configuration from GATT Service Discovery
           module.

    @param appTask Application task
    @param cid Connection Id of the remote device

    @return GATT_SERVICE_DISCOVERY_REMOVE_DEVICE_CONFIG_CFM_T confirmation
            message sent to application with the result.
*/
void GattServiceDiscoveryRemoveDeviceConfigReq(Task appTask, connection_id_t cid);


/*!
    @brief Find the service range of the GATT Service on remote device

           This request can be sent by any task running on the device.

    @param task service task
    @param cid Connection Id of the remote device
    @param srvcIds Bitwise values of the supported services listed in
                   GattSdSrvcId

    @return GATT_SERVICE_DISCOVERY_FIND_SRVC_HNDL_RANGE_CFM_T confirmation
            message sent to application with the result.
*/
void GattServiceDiscoveryFindServiceRangeReq(Task task,
                                connection_id_t cid,
                                GattSdSrvcId srvcIds);

#ifdef __cplusplus
}
#endif

#endif /* GATT_SERVICE_DISCOVERY_H__ */


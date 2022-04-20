/******************************************************************************
 Copyright (c) 2020 Qualcomm Technologies International, Ltd.
 All Rights Reserved.
 Qualcomm Technologies International, Ltd. Confidential and Proprietary.

 REVISION:      $Revision: #1 $
******************************************************************************/
#ifndef GATT_SERVICE_DISCOVERY_PRIVATE_H__
#define GATT_SERVICE_DISCOVERY_PRIVATE_H__

#include "gatt_service_discovery.h"

#include <stdlib.h>
#ifndef SYNERGY_GATT_SD_ENABLE
#include <panic.h>
#include <message.h>
#endif


#ifdef __cplusplus
extern "C" {
#endif

typedef uint8 GattSdServiceType;
#define GATT_SD_SERVICE_TYPE_PRIMARY         ((GattSdServiceType) 0)
#define GATT_SD_SERVICE_TYPE_SECONDARY       ((GattSdServiceType) 1)

#define GATT_SD_UUID_SIZE                    (16)
#define GATT_SD_UUID_GET_16(uuid)     ((uint16)(uuid[0]|(uuid[1]<<8)))
#define GATT_SD_UUID_GET_32(uuid)     ((uint32)(uuid[0]|(uuid[1]<<8)|(uuid[2]<<16)|(uuid[3]<<24)))


#ifdef SYNERGY_GATT_SD_ENABLE
#define APP_TASK_INVALID  CSR_SCHED_QID_INVALID
#else
#define APP_TASK_INVALID  NULL
#endif

#ifdef SYNERGY_GATT_SD_ENABLE
/* GATT Service Discovery (SD) UUID type definitions for GATT.*/
typedef enum
{
    /*! UUID is not present */
    gatt_uuid_none,
    /*! UUID is a 16-bit Attribute UUID */
    gatt_uuid16,
    /*! UUID is a 128-bit UUID */
    gatt_uuid128,
    /*! UUID is a 32-bit Attribute UUID */
    gatt_uuid32
} gatt_uuid_type_t;

#define GATT_SD_GET_UUID_TYPE(UUID_LENGTH)        ((UUID_LENGTH == CSR_BT_UUID16_SIZE) ? \
                                                   gatt_uuid16 : \
                                                   ((UUID_LENGTH == CSR_BT_UUID128_SIZE) ? \
                                                   gatt_uuid128 : gatt_uuid32))


#define GATT_SD_UUID_LITTLE_ENDIAN_FORMAT(SRC_UUID, DEST_UUID)  {\
    DEST_UUID[0] = SRC_UUID[3];\
    DEST_UUID[1] = SRC_UUID[2];\
    DEST_UUID[2] = SRC_UUID[1];\
    DEST_UUID[3] = SRC_UUID[0];}

#endif

/* GATT Service Discovery (SD) Service UUID structure */
typedef struct
{
    gatt_uuid_type_t type;
    uint32 uuid[4];
} GattSdSrvcUuid;

/* GATT Service Discovery (SD) Service UUID Info structure */
typedef struct
{
    GattSdSrvcId    srvcId;
    GattSdSrvcUuid  srvcUuid;
} GattSdSrvcUuidInfo_t;


/* GATT Service Discovery (SD) Service element */
typedef struct GattSdServiceElementTag
{
    GattSdSrvcId                       srvcId;         /* Service ID */
    uint16                             startHandle;    /* Service start handle */
    uint16                             endHandle;      /* End handle of service */
    GattSdServiceType                  serviceType;    /* Service type - Primary/Secondary */
    struct GattSdServiceElementTag*    next;
} GattSdServiceElement;

/* GATT Service Discovery (SD) Device element */
typedef struct GattSdDeviceElementTag
{
    connection_id_t                    cid;
    GattSdServiceElement*              serviceList;         /* Service Discovery Info list */
    uint16                             serviceListCount;
    struct GattSdDeviceElementTag*     next;
} GattSdDeviceElement;


#ifdef SYNERGY_GATT_SD_ENABLE
#define GATT_SD_MESSAGE_SEND(TASK, ID, MSG)  { \
    MSG->type = ID;\
    CsrSchedMessagePut(TASK, GATT_SRVC_DISC_PRIM, (void*) MSG);}

#define GATT_SD_MESSAGE_SEND_INTERNAL(TASK, ID, MSG)  { \
    MSG->type = ID;\
    CsrSchedMessagePut(CSR_BT_GATT_SRVC_DISC_IFACEQUEUE, GATT_SRVC_DISC_PRIM, (void*) MSG);}

#define GATT_SD_UNUSED(X)  
#else
#define GATT_SD_MESSAGE_SEND(TASK, ID, MSG)  { \
    MessageSend(TASK, ID, (void*) MSG);}

#define GATT_SD_MESSAGE_SEND_INTERNAL(TASK, ID, MSG)  { \
    MessageSend(TASK, ID, (void*) MSG);}

#define GATT_SD_UNUSED(X)  UNUSED(X)
#endif



#define GATT_SD_INTERNAL_MSG_BASE       (0x0000)

#ifdef SYNERGY_GATT_SD_ENABLE

#define GATT_SD_MALLOC(TYPE_SIZE)       malloc(TYPE_SIZE)
#define GATT_SD_FREE(TYPE)              if (TYPE) free(TYPE)

#define MAKE_GATT_SD_MESSAGE(TYPE) \
            TYPE##_T * const message = (TYPE##_T *) malloc(sizeof(TYPE##_T))

#define MAKE_GATT_SD_MESSAGE_WITH_LEN(TYPE, LEN) MAKE_GATT_SD_MESSAGE(TYPE)

#else

#define GATT_SD_MALLOC(TYPE_SIZE)       PanicNull(calloc(1,TYPE_SIZE))
#define GATT_SD_FREE(TYPE)              if (TYPE) free(TYPE)

#define MAKE_GATT_SD_MESSAGE(TYPE) \
            TYPE##_T* message = (TYPE##_T *)PanicNull(calloc(1, sizeof(TYPE##_T)))

#define MAKE_GATT_SD_MESSAGE_WITH_LEN(TYPE, LEN) \
            TYPE##_T *message = (TYPE##_T *) PanicNull(calloc(1,sizeof(TYPE##_T) + ((LEN) - 1) * sizeof(uint8)))

#endif


/* Enums for GATT SD Librray internal structure */
typedef enum __gatt_service_discovery_internal_msg
{
    GATT_SD_INTERNAL_MSG_DISCOVERY_START = GATT_SD_INTERNAL_MSG_BASE,
    GATT_SD_INTERNAL_MSG_DISCOVERY_STOP,
    GATT_SD_INTERNAL_MSG_GET_DEVICE_CONFIG,
    GATT_SD_INTERNAL_MSG_ADD_DEVICE_CONFIG,
    GATT_SD_INTERNAL_MSG_REMOVE_DEVICE_CONFIG,
    GATT_SD_INTERNAL_MSG_FIND_SERVICE_RANGE,
    GATT_SD_INTERNAL_MSG_TOP

} gatt_sd_internal_msg_t;


/* Internal Message Structure to start service discovery */
typedef struct
{
#ifdef SYNERGY_GATT_SD_ENABLE
    gatt_sd_internal_msg_t      type;
#endif
    connection_id_t cid;

} GATT_SD_INTERNAL_MSG_DISCOVERY_START_T;

/* Internal Message Structure to stop service discovery */
typedef GATT_SD_INTERNAL_MSG_DISCOVERY_START_T GATT_SD_INTERNAL_MSG_DISCOVERY_STOP_T;

/* Internal Message Structure to get device config */
typedef GATT_SD_INTERNAL_MSG_DISCOVERY_START_T GATT_SD_INTERNAL_MSG_GET_DEVICE_CONFIG_T;

/* Internal Message Structure to add device config */
typedef struct
{
#ifdef SYNERGY_GATT_SD_ENABLE
    gatt_sd_internal_msg_t      type;
#endif
    connection_id_t cid;
    uint16 srvcInfoCount;
    GattSdSrvcInfo_t* srvcInfo;

} GATT_SD_INTERNAL_MSG_ADD_DEVICE_CONFIG_T;

/* Internal Message Structure to remove device config */
typedef GATT_SD_INTERNAL_MSG_DISCOVERY_START_T GATT_SD_INTERNAL_MSG_REMOVE_DEVICE_CONFIG_T;

/* Internal Message Structure to find service range */
typedef struct
{
#ifdef SYNERGY_GATT_SD_ENABLE
    GattSdPrim      type;
#endif
    Task task;
    connection_id_t cid;
    GattSdSrvcId srvcIds;

} GATT_SD_INTERNAL_MSG_FIND_SERVICE_RANGE_T;

/***************************************************************************
NAME
    gattSdDiscoveryStartInternal

DESCRIPTION
    GATT SD Internal API to start service discovery.
*/
void gattSdDiscoveryStartInternal(
        const GATT_SD_INTERNAL_MSG_DISCOVERY_START_T* param);

/***************************************************************************
NAME
    gattSdDiscoveryStopInternal

DESCRIPTION
    GATT SD Internal API to stop service discovery.
*/
void gattSdDiscoveryStopInternal(
        const GATT_SD_INTERNAL_MSG_DISCOVERY_STOP_T* param);

/***************************************************************************
NAME
    gattSdGetDeviceConfigInternal

DESCRIPTION
    GATT SD Internal API to get device config
*/
void gattSdGetDeviceConfigInternal(
        const GATT_SD_INTERNAL_MSG_GET_DEVICE_CONFIG_T* param);

/***************************************************************************
NAME
    gattSdAddDeviceConfigInternal

DESCRIPTION
    GATT SD Internal API to add device config
*/
void gattSdAddDeviceConfigInternal(
        const GATT_SD_INTERNAL_MSG_ADD_DEVICE_CONFIG_T* param);

/***************************************************************************
NAME
    gattSdRemoveDeviceConfigInternal

DESCRIPTION
    GATT SD Internal API to remove device config
*/
void gattSdRemoveDeviceConfigInternal(
        const GATT_SD_INTERNAL_MSG_REMOVE_DEVICE_CONFIG_T* param);


/***************************************************************************
NAME
    gattSdFindServiceRangeInternal

DESCRIPTION
    GATT SD Internal API to find the service range.
*/
void gattSdFindServiceRangeInternal(
        const GATT_SD_INTERNAL_MSG_FIND_SERVICE_RANGE_T* param);



#ifdef __cplusplus
}
#endif

#endif /* GATT_SERVICE_DISCOVERY_PRIVATE_H__ */

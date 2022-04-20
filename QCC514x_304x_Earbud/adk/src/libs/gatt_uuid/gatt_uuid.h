/*****************************************************************************
Copyright (c) 2020 Qualcomm Technologies International, Ltd.


FILE NAME
    gatt_uuid.h

DESCRIPTION
    Header file for the GATT UUID library
    
    This Library defines the UUIDs for use in GATT services as defined by
    the Bluetooth SIG.
*/

/*!
\defgroup gatt gatt
\ingroup vm_libs

\brief      Header file for the GATT UUID library

\section gatt_uuid_intro INTRODUCTION
    This file provides documentation for the GATT UUID library

@{

*/

#ifndef GATT_UUID_H_
#define GATT_UUID_H_


/*****************************************************************************
GATT Service UUIDs
*****************************************************************************/
#define GATT_TBS_UUID_TELEPHONE_BEARER_SERVICE               (0x8FD5)/*TBS*/
#define GATT_GTBS_UUID_TELEPHONE_BEARER_SERVICE              (0x8FDF)/*GTBS*/


/*****************************************************************************
GATT Characteristic UUIDs
*****************************************************************************/

/* Telephone Bearer Service (inc. Generic TBS) */
#define GATT_TBS_UUID_BEARER_PROVIDER_NAME                   (0x8FEA)
#define GATT_TBS_UUID_BEARER_UCI                             (0x8FEB)
#define GATT_TBS_UUID_BEARER_TECHNOLOGY                      (0x8FEC)
#define GATT_TBS_UUID_BEARER_URI_PREFIX_LIST                 (0x8FED)
#define GATT_TBS_UUID_SIGNAL_STRENGTH                        (0x8FEF)
#define GATT_TBS_UUID_SIGNAL_STRENGTH_REPORTING_INTERVAL     (0x8FF0)
#define GATT_TBS_UUID_LIST_CURRENT_CALLS                     (0x8FF1)
#define GATT_TBS_UUID_CONTENT_CONTROL_ID                     (0x8FF2)
#define GATT_TBS_UUID_STATUS_FLAGS                           (0x8FF3)
#define GATT_TBS_UUID_INCOMING_CALL_TARGET_BEARER_URI        (0x8FF4)
#define GATT_TBS_UUID_CALL_STATE                             (0x8FF5)
#define GATT_TBS_UUID_CALL_CONTROL_POINT                     (0x8FF6)
#define GATT_TBS_UUID_CALL_CONTROL_POINT_OPCODES             (0x8FF7)
#define GATT_TBS_UUID_TERMINATION_REASON                     (0x8FF8)
#define GATT_TBS_UUID_INCOMING_CALL                          (0x8FF9)
#define GATT_TBS_UUID_REMOTE_FRIENDLY_NAME                   (0x8FFA)


/*****************************************************************************
Characteristic Declaration UUIDs
*****************************************************************************/
#define GATT_CHARACTERISTIC_DECLARATION_UUID                                (0x2803)
#define GATT_INCLUDE_DECLARATION_UUID                                       (0x2802)
#define GATT_PRIMARY_SERVICE_DECLARATION_UUID                               (0x2800)
#define GATT_SECONDARY_SERVICE_DECLARATION_UUID                             (0x2801)


/*****************************************************************************
Characteristic Descriptor UUIDs
*****************************************************************************/
#define GATT_CHARACTERISTIC_EXTENDED_PROPERTIES_UUID                        (0x2900)
#define GATT_CHARACTERISTIC_USER_DESCRIPTION_UUID                           (0x2901)
#define GATT_CLIENT_CHARACTERISTIC_CONFIGURATION_UUID                       (0x2902)
#define GATT_CHARACTERISTIC_PRESENTATION_FORMAT_UUID                        (0x2904)
#define GATT_CHARACTERISTIC_AGGREGATE_FORMAT_UUID                           (0x2905)
#define GATT_VALID_RANGE_UUID                                               (0x2906)
#define GATT_EXTERNAL_REPORT_REFERENCE_UUID                                 (0x2907)
#define GATT_REPORT_REFERENCE_UUID                                          (0x2908)



#endif  /* GATT_UUID_H_ */

/** @} */

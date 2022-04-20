/*!
\copyright  Copyright (c) 2019-2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   le_scan_manager_periodic
\ingroup    bt_domain
\brief      LE periodic scanning component


*/

#ifndef LE_SCAN_MANAGER_PERIODIC_H_
#define LE_SCAN_MANAGER_PERIODIC_H_

#include "le_scan_manager.h"

#include "le_scan_manager_protected.h"


typedef enum
{
    LE_SCAN_MANAGER_PERIODIC_STOP_CFM,
    LE_SCAN_MANAGER_PERIODIC_DISABLE_CFM,
    LE_SCAN_MANAGER_PERIODIC_ENABLE_CFM
}periodicMessages;

typedef struct {
    le_scan_manager_status_t result;
    Task scan_task;
}LE_SCAN_MANAGER_PERIODIC_STOP_CFM_T;

typedef struct {
    le_scan_manager_status_t result;
}LE_SCAN_MANAGER_PERIODIC_DISABLE_CFM_T;

typedef struct {
    le_scan_manager_status_t result;
}LE_SCAN_MANAGER_PERIODIC_ENABLE_CFM_T;

#ifdef INCLUDE_ADVERTISING_EXTENSIONS

void leScanManager_PeriodicScanInit(void);

void leScanManager_StartPeriodicScanFindTrains(Task task, le_periodic_advertising_filter_t* filter);

bool leScanManager_PeriodicScanStop(Task req_task, Task scan_task);

bool LeScanManager_IsPeriodicTaskScanning(Task task);

bool leScanManager_HandlePeriodicClMessages(MessageId id, Message message);

bool leScanManager_PeriodicScanDisable(Task req_task);

bool leScanManager_PeriodicScanEnable(Task req_task);


#else
    
#define leScanManager_PeriodicScanInit() ((void)(0))

#define leScanManager_StartPeriodicScanFindTrains(x, y) ((void)(0))

#define leScanManager_PeriodicScanStop(x, y) (FALSE)

#define LeScanManager_IsPeriodicTaskScanning(x) (FALSE)

#define leScanManager_HandlePeriodicClMessages(x, y) (FALSE)

#define leScanManager_PeriodicScanDisable(x) (FALSE)

#define leScanManager_PeriodicScanEnable(x) (FALSE)

#endif /* INCLUDE_ADVERTISING_EXTENSIONS */


#endif /* LE_SCAN_MANAGER_PERIODIC_H_ */

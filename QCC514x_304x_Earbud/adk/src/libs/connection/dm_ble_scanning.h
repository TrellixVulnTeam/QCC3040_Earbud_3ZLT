/****************************************************************************
Copyright (c) 2019 - 2020 Qualcomm Technologies International, Ltd.


FILE NAME
    dm_ble_scanning.h

DESCRIPTION
    Deals with DM_HCI_ULP (BLE) prims from bluestack related to Scan
    functinality

NOTES

*/

/****************************************************************************
NAME
    connectionHandleBleSetScanCfm

DESCRIPTION
    Handles BlueStack cfm message for BLE Set Scan.

RETURNS
    void
*/

#ifndef DISABLE_BLE
#ifndef DM_BLE_SCANNING_H
#define DM_BLE_SCANNING_H


#include "connection.h"
#include "connection_private.h"
#include <app/bluestack/types.h>
#include <app/bluestack/dm_prim.h>

void connectionHandleDmBleSetScanEnableReq(
        connectionBleScanAdState *state,
        const CL_INTERNAL_DM_BLE_SET_SCAN_ENABLE_REQ_T *req
        );

void connectionHandleDmBleSetScanEnableCfm(
        connectionBleScanAdState *state,
        const DM_HCI_ULP_SET_SCAN_ENABLE_CFM_T* cfm
        );

/*****************************************************************************
 *                     Extended Scanning functions                           *
 *****************************************************************************/
#ifndef CL_EXCLUDE_ISOC

#define MAX_SCAN_HANDLES 5
#define MAX_TRAIN_SCAN_HANDLES 2
#define MAX_SYNC_HANDLES 4

/* Struct to hold task to scan_handle associations */
typedef struct
{
    Task    registering_task;
    uint8   scan_handle;
} task_scan_handles_pair;

/* Struct to hold task to sync_handle associations */
typedef struct
{
    Task    registering_task;
    uint16  sync_handle;
    Source  source;
} task_sync_handles_pair;

/****************************************************************************
NAME
    connectionHandleDmBleSetScanEnableReq

DESCRIPTION
    This function will initiate a Scan Enable request.

RETURNS
    void
*/
void connectionHandleDmBleExtScanEnableReq(
        connectionDmExtScanState *state,
        const CL_INTERNAL_DM_BLE_EXT_SCAN_ENABLE_REQ_T *req
        );

/****************************************************************************
NAME
    connectionHandleDmBleExtScanEnableCfm

DESCRIPTION
    Handle the DM_HCI_ULP_EXT_SCAN_ENABLE_CFM from Bluestack.

RETURNS
    void
*/
void connectionHandleDmBleExtScanEnableCfm(
        connectionDmExtScanState *state,
        const DM_ULP_EXT_SCAN_ENABLE_SCANNERS_CFM_T* cfm
        );

/****************************************************************************
NAME
    connectionHandleDmBleExtScanGetGlobalParamsReq

DESCRIPTION
    This function will initiate an Extended Scanning Get Global Parameters request.

RETURNS
    void
*/
void connectionHandleDmBleExtScanGetGlobalParamsReq(
        connectionDmExtScanState *state,
        const CL_INTERNAL_DM_BLE_EXT_SCAN_GET_GLOBAL_PARAMS_REQ_T *req
        );

/****************************************************************************
NAME
    connectionHandleDmBleExtScanGetGlobalParamsCfm

DESCRIPTION
    Handles status of Extended Scanning Get Global Parameters request

RETURNS
    void
*/
void connectionHandleDmBleExtScanGetGlobalParamsCfm(connectionDmExtScanState *state,
    const DM_ULP_EXT_SCAN_GET_GLOBAL_PARAMS_CFM_T *cfm);

/****************************************************************************
NAME
    connectionHandleDmBleExtScanSetParamsReq

DESCRIPTION
    This function will initiate an Extended Scanning Set Parameters request.

RETURNS
    void
*/
void connectionHandleDmBleExtScanSetParamsReq(
        connectionDmExtScanState *state,
        const CL_INTERNAL_DM_BLE_EXT_SCAN_SET_GLOBAL_PARAMS_REQ_T *req
        );

/****************************************************************************
NAME
    connectionHandleDmBleExtScanSetParamsCfm

DESCRIPTION
    Handles status of Extended Scanning Parameters request

RETURNS
    void
*/
void connectionHandleDmBleExtScanSetParamsCfm(connectionDmExtScanState *state,
    const DM_ULP_EXT_SCAN_SET_GLOBAL_PARAMS_CFM_T *cfm);

/****************************************************************************
NAME
    connectionHandleDmBleExtScanRegisterScannerReq

DESCRIPTION
    This function will initiate an Extended Scanning Register Scanner request.

RETURNS
    void
*/
void connectionHandleDmBleExtScanRegisterScannerReq(
        connectionDmExtScanState *state,
        const CL_INTERNAL_DM_BLE_EXT_SCAN_REGISTER_SCANNER_REQ_T *req
        );

/****************************************************************************
NAME
    connectionHandleDmBleExtScanRegisterScannerCfm

DESCRIPTION
    Handles status of Extended Scanning Register Scanner request

RETURNS
    void
*/
void connectionHandleDmBleExtScanRegisterScannerCfm(connectionDmExtScanState *state,
    const DM_ULP_EXT_SCAN_REGISTER_SCANNER_CFM_T *cfm);

/****************************************************************************
NAME
    connectionHandleDmBleExtScanUnregisterScannerReq

DESCRIPTION
    This function will initiate an Extended Scanning Unregister Scanner request.

RETURNS
    void
*/
void connectionHandleDmBleExtScanUnregisterScannerReq(
        connectionDmExtScanState *state,
        const CL_INTERNAL_DM_BLE_EXT_SCAN_UNREGISTER_SCANNER_REQ_T *req
        );

/****************************************************************************
NAME
    connectionHandleDmBleExtScanUnregisterScannerCfm

DESCRIPTION
    Handles status of Extended Scanning Unregister Scanner request

RETURNS
    void
*/
void connectionHandleDmBleExtScanUnregisterScannerCfm(connectionDmExtScanState *state,
    const DM_ULP_EXT_SCAN_UNREGISTER_SCANNER_CFM_T *cfm);

/****************************************************************************
NAME
    connectionHandleDmBleExtScanCtrlScanInfoInd

DESCRIPTION
    Handles the Extended Scanning Control Scan Info indication, sent any time
    the Contoller's LE Scanner config is changed or new scanners are enabled/disabled.

RETURNS
    void
*/
void connectionHandleDmBleExtScanCtrlScanInfoInd(const DM_ULP_EXT_SCAN_CTRL_SCAN_INFO_IND_T *ind);

/****************************************************************************
NAME
    connectionHandleDmBleExtScanFilteredAdvReportInd

DESCRIPTION
    Handles BLE Extended Scanning Filtered Advertising report indication

RETURNS
    void
*/
void connectionHandleDmBleExtScanFilteredAdvReportInd(const DM_ULP_EXT_SCAN_FILTERED_ADV_REPORT_IND_T *ind);

/****************************************************************************
NAME
    connectionHandleDmBleExtScanFilteredAdvReportDoneInd

DESCRIPTION
    Once all interested tasks have processed the BLE Extended Scanning Filtered
    Advertising report indication, clear the report from the incoming stream.

RETURNS
    void
*/
void connectionHandleDmBleExtScanFilteredAdvReportDoneInd(const CL_INTERNAL_DM_BLE_EXT_SCAN_ADV_REPORT_DONE_IND_T *ind);

/****************************************************************************
NAME
    connectionHandleDmBleExtScanGetCtrlScanInfoReq

DESCRIPTION
    This function will initiate an Extended Scanning Get Controller Scanner Info
    request.

RETURNS
    void
*/
void connectionHandleDmBleExtScanGetCtrlScanInfoReq(
        connectionDmExtScanState *state,
        const CL_INTERNAL_DM_BLE_EXT_SCAN_GET_CTRL_SCAN_INFO_REQ_T *req
        );

/****************************************************************************
NAME
    ConnectionDmBleExtScanGetCtrlScanInfoCfm

DESCRIPTION
    Handles status of an Extended Scanning Get Controller Scanner Info request

RETURNS
    void
*/
void ConnectionDmBleExtScanGetCtrlScanInfoCfm(connectionDmExtScanState *state,
    const DM_ULP_EXT_SCAN_GET_CTRL_SCAN_INFO_CFM_T *cfm);

/****************************************************************************
NAME
    connectionHandleDmBleExtScanDurationExpiredInd

DESCRIPTION
    Handles the Extended Scanning Duration Expired indication, sent any time a
    duration timer expires for a scanner. The scanner will no longer be scanning.

    If the scan handle has been unregistered, Connection library will also
    remove the association to the registering task from its tracking struct.
    Any advertising reports from this scanner in the stream that have not yet
    been processed will be silently consumed.

RETURNS
    void
*/
void connectionHandleDmBleExtScanDurationExpiredInd(const DM_ULP_EXT_SCAN_DURATION_EXPIRED_IND_T *ind);

/*****************************************************************************
 *                     Periodic Scanning functions                           *
 *****************************************************************************/
/****************************************************************************
NAME
    connectionHandleDmBlePeriodicScanSyncTrainReq

DESCRIPTION
    This function will initiate a Periodic Scanning Sync to Train request.

RETURNS
    void
*/
void connectionHandleDmBlePeriodicScanSyncTrainReq(
        connectionDmPerScanState *state,
        const CL_INTERNAL_DM_BLE_PERIODIC_SCAN_SYNC_TO_TRAIN_REQ_T *req
        );

/****************************************************************************
NAME
    connectionHandleDmBlePeriodicScanSyncTrainCfm

DESCRIPTION
    Handles status of Periodic Scanning Sync to Train request.

RETURNS
    void
*/
void connectionHandleDmBlePeriodicScanSyncTrainCfm(connectionDmPerScanState *state,
    const DM_ULP_PERIODIC_SCAN_SYNC_TO_TRAIN_CFM_T *cfm);

/****************************************************************************
NAME
    connectionHandleDmBlePeriodicScanSyncCancelCfm

DESCRIPTION
    Handles status of Periodic Scanning Sync Cancel request.

RETURNS
    void
*/
void connectionHandleDmBlePeriodicScanSyncCancelCfm(connectionDmPerScanState *state,
    const DM_ULP_PERIODIC_SCAN_SYNC_TO_TRAIN_CANCEL_CFM_T *cfm);

/****************************************************************************
NAME
    connectionHandleDmBlePeriodicScanSyncTerminateReq

DESCRIPTION
    This function will initiate a Periodic Scanning Sync Terminate request.

RETURNS
    void
*/
void connectionHandleDmBlePeriodicScanSyncTerminateReq(connectionDmPerScanState *state,
        const CL_INTERNAL_DM_BLE_PERIODIC_SCAN_SYNC_TERMINATE_REQ_T *req
        );

/****************************************************************************
NAME
    connectionHandleDmBlePeriodicScanSyncTerminateCfm

DESCRIPTION
    Handles status of Periodic Scanning Sync Terminate request.

RETURNS
    void
*/
void connectionHandleDmBlePeriodicScanSyncTerminateCfm(connectionDmPerScanState *state,
    const DM_ULP_PERIODIC_SCAN_SYNC_TERMINATE_CFM_T *cfm);

/****************************************************************************
NAME
    connectionHandleDmBlePeriodicScanSyncTransferReq

DESCRIPTION
    This function will initiate a Periodic Scanning Sync Transfer request.

RETURNS
    void
*/
void connectionHandleDmBlePeriodicScanSyncTransferReq(connectionDmPerScanState *state,
        const CL_INTERNAL_DM_BLE_PERIODIC_SCAN_SYNC_TRANSFER_REQ_T *req
        );

/****************************************************************************
NAME
    connectionHandleDmBlePeriodicScanSyncTransferCfm

DESCRIPTION
    Handles status of Periodic Scanning Sync Transfer request.

RETURNS
    void
*/
void connectionHandleDmBlePeriodicScanSyncTransferCfm(connectionDmPerScanState *state,
    const DM_ULP_PERIODIC_SCAN_SYNC_TRANSFER_CFM_T *cfm);

/****************************************************************************
NAME
    connectionHandleDmBlePeriodicScanSyncTransferParamsReq

DESCRIPTION
    This function will initiate a Periodic Scanning Sync Transfer Params request.

RETURNS
    void
*/
void connectionHandleDmBlePeriodicScanSyncTransferParamsReq(connectionDmPerScanState *state,
        const CL_INTERNAL_DM_BLE_PERIODIC_SCAN_SYNC_TRANSFER_PARAMS_REQ_T *req
        );

/****************************************************************************
NAME
    connectionHandleDmBlePeriodicScanSyncTransferParamsCfm

DESCRIPTION
    Handles status of Periodic Scanning Sync Transfer Params request.

RETURNS
    void
*/
void connectionHandleDmBlePeriodicScanSyncTransferParamsCfm(connectionDmPerScanState *state,
    const DM_ULP_PERIODIC_SCAN_SYNC_TRANSFER_PARAMS_CFM_T *cfm);

/****************************************************************************
NAME
    connectionHandleDmBlePeriodicScanStartFindTrainsReq

DESCRIPTION
    This function will initiate a Periodic Scanning Start Find Trains request.

RETURNS
    void
*/
void connectionHandleDmBlePeriodicScanStartFindTrainsReq(connectionDmPerScanState *state,
        const CL_INTERNAL_DM_BLE_PERIODIC_SCAN_START_FIND_TRAINS_REQ_T *req
        );

/****************************************************************************
NAME
    connectionHandleDmBlePeriodicScanStartFindTrainsCfm

DESCRIPTION
    Handles status of Periodic Scanning Start Find Trains request.

RETURNS
    void
*/
void connectionHandleDmBlePeriodicScanStartFindTrainsCfm(connectionDmPerScanState *state,
    const DM_ULP_PERIODIC_SCAN_START_FIND_TRAINS_CFM_T *cfm);

/****************************************************************************
NAME
    connectionHandleDmBlePeriodicScanStopFindTrainsReq

DESCRIPTION
    This function will initiate a Periodic Scanning Stop Find Trains request.

RETURNS
    void
*/
void connectionHandleDmBlePeriodicScanStopFindTrainsReq(connectionDmPerScanState *state,
        const CL_INTERNAL_DM_BLE_PERIODIC_SCAN_STOP_FIND_TRAINS_REQ_T *req
        );

/****************************************************************************
NAME
    connectionHandleDmBlePeriodicScanStopFindTrainsCfm

DESCRIPTION
    Handles status of Periodic Scanning Stop Find Trains request.

RETURNS
    void
*/
void connectionHandleDmBlePeriodicScanStopFindTrainsCfm(connectionDmPerScanState *state,
    const DM_ULP_PERIODIC_SCAN_STOP_FIND_TRAINS_CFM_T *cfm);

/****************************************************************************
NAME
    connectionHandleDmBlePeriodicScanSyncAdvReportEnableReq

DESCRIPTION
    This function will initiate a Periodic Scanning Sync Advertising Report Enable request.

RETURNS
    void
*/
void connectionHandleDmBlePeriodicScanSyncAdvReportEnableReq(connectionDmPerScanState *state,
        const CL_INTERNAL_DM_BLE_PERIODIC_SCAN_SYNC_ADV_REPORT_ENABLE_REQ_T *req
        );

/****************************************************************************
NAME
    connectionHandleDmBlePeriodicScanSyncAdvReportEnableCfm

DESCRIPTION
    Handles status of Periodic Scanning Sync Advertising Report Enable request.

RETURNS
    void
*/
void connectionHandleDmBlePeriodicScanSyncAdvReportEnableCfm(connectionDmPerScanState *state,
    const DM_ULP_PERIODIC_SCAN_SYNC_ADV_REPORT_ENABLE_CFM_T *cfm);

/****************************************************************************
NAME
    connectionHandleDmBlePeriodicScanSyncLostInd

DESCRIPTION
    Sync lost to periodic train.

RETURNS
    void
*/
void connectionHandleDmBlePeriodicScanSyncLostInd(
        const DM_ULP_PERIODIC_SCAN_SYNC_LOST_IND_T *ind
        );

/****************************************************************************
NAME
    connectionHandleDmBlePeriodicScanSyncTransferInd

DESCRIPTION
    An indication sent to the Profile/Application following an attempt by the
    local Controller to synchronize to a periodic advertising train.

RETURNS
    void
*/
void connectionHandleDmBlePeriodicScanSyncTransferInd(
        const DM_ULP_PERIODIC_SCAN_SYNC_TRANSFER_IND_T *ind
        );

/****************************************************************************
NAME
    connectionHandleDmBlePeriodicScanSyncAdvReportInd

DESCRIPTION
    Handles BLE Periodic Scanning Sync Advertising report indication

RETURNS
    void
*/
void connectionHandleDmBlePeriodicScanSyncAdvReportInd(const DM_ULP_PERIODIC_SCAN_SYNC_ADV_REPORT_IND_T *ind);

/****************************************************************************
NAME
    connectionHandleDmBleExtScanFilteredAdvReportDoneInd

DESCRIPTION
    Once all the associated task have processed the BLE Periodic Scanning Sync
    Advertising report indication, clear the report from the incoming stream.

RETURNS
    void
*/
void connectionHandleDmBlePerScanAdvReportDoneInd(const CL_INTERNAL_DM_BLE_PER_SCAN_ADV_REPORT_DONE_IND_T *ind);

/****************************************************************************
NAME
    connectionHandleMoreData

DESCRIPTION
    Handles MESSAGE_MORE_DATA from streams connected to the Connection library.
    Determines which stream (Periodic or Extended currently) the message is
    from, extracts the data from it and routes it to the relevant handler.

RETURNS
    void
*/
void connectionHandleMoreData(Source src, conn_lib_stream_types stream_type);
#endif /* CL_EXCLUDE_ISOC */

#endif // DM_BLE_SCANNING_H
#endif /* DISABLE_BLE */

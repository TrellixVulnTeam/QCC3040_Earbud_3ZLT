/****************************************************************************
Copyright (c) 2015 - 2020 Qualcomm Technologies International, Ltd.


FILE NAME
    dm_ble_advetising.h

DESCRIPTION
    This file contains the prototypes for BLE DM advertising msgs from Bluestack .

NOTES

*/

#ifndef DISABLE_BLE
#ifndef DM_BLE_ADVERTISING_H
#define DM_BLE_ADVERTISING_H

/****************************************************************************
NAME
    connectionHandleDmBleAdvParamUpdateInd

DESCRIPTION
    Handle the DM_ULP_ADV_PARAM_UPDATE_IND message from Bluestack and pass it
    on to the appliction that initialised the CL.

RETURNS
    void
*/
void connectionHandleDmBleAdvParamUpdateInd(
        const DM_ULP_ADV_PARAM_UPDATE_IND_T *ind
        );


/****************************************************************************
NAME
    connectionHandleDmBleSetAdvertiseEnableReq

DESCRIPTION
    This function will initiate an Advertising Enable request.

RETURNS
   void
*/
void connectionHandleDmBleSetAdvertiseEnableReq(
        connectionBleScanAdState *state,
        const CL_INTERNAL_DM_BLE_SET_ADVERTISE_ENABLE_REQ_T *req
        );


/****************************************************************************
NAME
    connectionHandleDmBleSetAdvertiseEnableCfm

DESCRIPTION
    Handle the DM_HCI_ULP_SET_ADVERTISE_ENABLE_CFM from Bluestack.

RETURNS
   void
*/
void connectionHandleDmBleSetAdvertiseEnableCfm(
        connectionBleScanAdState *state,
        const DM_HCI_ULP_SET_ADVERTISE_ENABLE_CFM_T *cfm
        );

/*****************************************************************************
 *                  Extended Advertising functions                           *
 *****************************************************************************/

/****************************************************************************
NAME
    connectionHandleDmBleeGetAdvScanCapabilitiesReq

DESCRIPTION
    This function will initiate an Get Advertising/Scanning Capabilities request.

RETURNS
   void
*/
void connectionHandleDmBleGetAdvScanCapabilitiesReq(
        connectionDmExtAdvState *state,
        const CL_INTERNAL_DM_BLE_GET_ADV_SCAN_CAPABILITIES_REQ_T *req
        );

/****************************************************************************
NAME
    connectionHandleDmBleExtAdvSetsInfoCfm

DESCRIPTION
    Handles status of an Get Advertising/Scanning Capabilities Request

RETURNS
    void
*/
void connectionHandleDmBleGetAdvScanCapabilitiesCfm(
        connectionDmExtAdvState *state,
        const DM_ULP_GET_ADV_SCAN_CAPABILITIES_CFM_T *cfm
        );

/****************************************************************************
NAME
    connectionHandleDmBleExtAdvSetsInfoReq

DESCRIPTION
    This function will initiate an Extended Advertising Sets Info request.

RETURNS
   void
*/
void connectionHandleDmBleExtAdvSetsInfoReq(
        connectionDmExtAdvState *state,
        const CL_INTERNAL_DM_BLE_EXT_ADV_SETS_INFO_REQ_T *req
        );

/****************************************************************************
NAME
    connectionHandleDmBleExtAdvSetsInfoCfm

DESCRIPTION
    Handles status of Extended Advertising Sets Info Request

RETURNS
    void
*/
void connectionHandleDmBleExtAdvSetsInfoCfm(
        connectionDmExtAdvState *state,
        const DM_ULP_EXT_ADV_SETS_INFO_CFM_T *cfm
        );

/****************************************************************************
NAME
    connectionHandleDmBleRegisterAppAdvSetReq

DESCRIPTION
    This function will initiate an Extended Advertising Register App Adv Set request.

RETURNS
   void
*/
void connectionHandleDmBleRegisterAppAdvSetReq(
        connectionDmExtAdvState *state,
        const CL_INTERNAL_DM_BLE_EXT_ADV_REGISTER_APP_ADV_SET_REQ_T *req
        );

/****************************************************************************
NAME
    connectionHandleDmBleExtAdvRegisterAppAdvSetCfm

DESCRIPTION
    Handle the DM_ULP_EXT_ADV_REGISTER_APP_ADV_SET_CFM from Bluestack.

RETURNS
   void
*/
void connectionHandleDmBleExtAdvRegisterAppAdvSetCfm(connectionDmExtAdvState *state,
        const DM_ULP_EXT_ADV_REGISTER_APP_ADV_SET_CFM_T *cfm
        );

/****************************************************************************
NAME
    connectionHandleDmBleExtAdvertiseEnableCfm

DESCRIPTION
    Handle the DM_ULP_EXT_ADV_ENABLE_CFM from Bluestack.

RETURNS
   void
*/
void connectionHandleDmBleExtAdvertiseEnableCfm(
        connectionDmExtAdvState *state,
        const DM_ULP_EXT_ADV_ENABLE_CFM_T *cfm
        );

/****************************************************************************
NAME
    connectionHandleDmBleUnregisterAppAdvSetReq

DESCRIPTION
    This function will initiate an Extended Advertising Unregister App Adv Set request.

RETURNS
   void
*/
void connectionHandleDmBleUnregisterAppAdvSetReq(
        connectionDmExtAdvState *state,
        const CL_INTERNAL_DM_BLE_EXT_ADV_UNREGISTER_APP_ADV_SET_REQ_T *req
        );

/****************************************************************************
NAME
    connectionHandleDmBleExtAdvUnregisterAppAdvSetCfm

DESCRIPTION
    Handle the DM_ULP_EXT_ADV_UNREGISTER_APP_ADV_SET_CFM from Bluestack.

RETURNS
   void
*/
void connectionHandleDmBleExtAdvUnregisterAppAdvSetCfm(connectionDmExtAdvState *state,
        const DM_ULP_EXT_ADV_UNREGISTER_APP_ADV_SET_CFM_T *cfm
        );

/****************************************************************************
NAME
    connectionHandleDmBleExtAdvertiseEnableReq

DESCRIPTION
    This function will initiate an Extended Advertising Enable request.

RETURNS
   void
*/
void connectionHandleDmBleExtAdvertiseEnableReq(connectionDmExtAdvState *state,
        const CL_INTERNAL_DM_BLE_EXT_ADVERTISE_ENABLE_REQ_T *req
        );

/****************************************************************************
NAME
    connectionHandleDmBleSetAdvMultiEnableReq

DESCRIPTION
    This function will initiate an Extended Advertising Multi Enable request.

RETURNS
   void
*/
void connectionHandleDmBleSetAdvMultiEnableReq(
        connectionDmExtAdvState *state,
        const CL_INTERNAL_DM_BLE_EXT_ADV_MULTI_ENABLE_REQ_T *req
        );

/****************************************************************************
NAME
    connectionHandleDmBleExtAdvMultiEnableCfm

DESCRIPTION
    Handle the DM_ULP_EXT_ADV_MULTI_ENABLE_CFM from Bluestack.

RETURNS
   void
*/
void connectionHandleDmBleExtAdvMultiEnableCfm(connectionDmExtAdvState *state,
        const DM_ULP_EXT_ADV_MULTI_ENABLE_CFM_T *cfm
        );

/****************************************************************************
NAME
    connectionHandleDmBleExtAdvSetParamsReq

DESCRIPTION
    This function will initiate an Extended Advertising Set Parameters request.

RETURNS
   void
*/
void connectionHandleDmBleExtAdvSetParamsReq(
        connectionDmExtAdvState *state,
        const CL_INTERNAL_DM_BLE_EXT_ADV_SET_PARAMS_REQ_T *req
        );

/****************************************************************************
NAME
    connectionHandleDmBleSetExtAdvertisingParamsCfm

DESCRIPTION
    Handle the DM_ULP_EXT_ADV_SET_PARAMS_CFM from Bluestack.

RETURNS
   void
*/
void connectionHandleDmBleExtAdvSetParamsCfm(
        connectionDmExtAdvState *state,
        const DM_ULP_EXT_ADV_SET_PARAMS_CFM_T *cfm
        );

/****************************************************************************
NAME
    connectionHandleDmBleExtAdvSetRandomAddressReq

DESCRIPTION
    This function will initiate an Extended Advertising Set Random Address request.

RETURNS
   void
*/
void connectionHandleDmBleExtAdvSetRandomAddressReq(
        connectionDmExtAdvState *state,
        const CL_INTERNAL_DM_BLE_EXT_ADV_SET_RANDOM_ADDRESS_REQ_T *req
        );

/****************************************************************************
NAME
    connectionHandleDmBleExtAdvSetRandomAddressCfm

DESCRIPTION
    Handles status of Extended Advertising Set Random Address request

RETURNS
    void
*/


void connectionHandleDmBleExtAdvSetRandomAddressCfm(
        connectionDmExtAdvState *state,
        const DM_ULP_EXT_ADV_SET_RANDOM_ADDR_CFM_T *cfm
        );

/****************************************************************************
NAME
    connectionHandleDmBleExtAdvSetDataReq

DESCRIPTION
    This function will initiate an Extended Advertising Set Data request.

RETURNS
   void
*/
void connectionHandleDmBleExtAdvSetDataReq(
        connectionDmExtAdvState *state,
        const CL_INTERNAL_DM_BLE_EXT_ADV_SET_DATA_REQ_T *req
        );

/****************************************************************************
NAME
    connectionHandleDmBleSetExtAdvertisingDataCfm

DESCRIPTION
    Handle the DM_HCI_ULP_EXT_ADV_SET_DATA_CFM from Bluestack.

RETURNS
   void
*/
void connectionHandleDmBleExtAdvSetDataCfm(
        connectionDmExtAdvState *state,
        const DM_HCI_ULP_EXT_ADV_SET_DATA_CFM_T *cfm
        );

/****************************************************************************
NAME
    connectionHandleDmBleExtAdvSetScanRespDataReq

DESCRIPTION
    This function will initiate an Extended Advertising Set Scan Response Data request.

RETURNS
   void
*/
void connectionHandleDmBleExtAdvSetScanRespDataReq(
        connectionDmExtAdvState *state,
        const CL_INTERNAL_DM_BLE_EXT_ADV_SET_SCAN_RESP_DATA_REQ_T *req
        );

/****************************************************************************
NAME
    connectionHandleDmBleSetExtAdvScanRespDataCfm

DESCRIPTION
    Handles status of Extended Advertising Scan Response Data Request

RETURNS
    void
*/


void connectionHandleDmBleExtAdvSetScanRespDataCfm(
        connectionDmExtAdvState *state,
        const DM_HCI_ULP_EXT_ADV_SET_SCAN_RESP_DATA_CFM_T *cfm
        );

/****************************************************************************
NAME
    connectionHandleDmBleExtAdvReadMaxAdvDataLenReq

DESCRIPTION
    This function will initiate an Extended Advertising Read Max Adv Data Len request.

RETURNS
   void
*/
void connectionHandleDmBleExtAdvReadMaxAdvDataLenReq(
        connectionDmExtAdvState *state,
        const CL_INTERNAL_DM_BLE_EXT_ADV_READ_MAX_ADV_DATA_LEN_REQ_T *req
        );

/****************************************************************************
NAME
    connectionHandleDmBleSetExtAdvReadMaxAdvDataLenCfm

DESCRIPTION
    Handles status of Extended Advertising Read Max Adv Data Len Request

RETURNS
    void
*/


void connectionHandleDmBleExtAdvReadMaxAdvDataLenCfm(
        connectionDmExtAdvState *state,
        const DM_ULP_EXT_ADV_READ_MAX_ADV_DATA_LEN_CFM_T *cfm
        );

/****************************************************************************
NAME
    connectionHandleDmBleExtAdvTerminatedInd

DESCRIPTION
    Handles the Extended Advertising terminated indication, sent any time
    advertising is stopped by the controller due to duration expiring or max
    extended advertising event limit reached or connection establishment.

RETURNS
    void
*/
void connectionHandleDmBleExtAdvTerminatedInd(const DM_ULP_EXT_ADV_TERMINATED_IND_T *ind);

/*****************************************************************************
 *                  Periodic Advertising functions                           *
 *****************************************************************************/

/****************************************************************************
NAME
    connectionHandleDmBlePerAdvSetParamsReq

DESCRIPTION
    This function will initiate an Periodic Advertising Set Parameters request.

RETURNS
   void
*/
void connectionHandleDmBlePerAdvSetParamsReq(
        connectionDmPerAdvState *state,
        const CL_INTERNAL_DM_BLE_PER_ADV_SET_PARAMS_REQ_T *req
        );

/****************************************************************************
NAME
    connectionHandleDmBlePerAdvSetParamsCfm

DESCRIPTION
    Handles status of Periodic Advertising Parameters Request

RETURNS
    void
*/

void connectionHandleDmBlePerAdvSetParamsCfm(connectionDmPerAdvState *state,
                            const DM_ULP_PERIODIC_ADV_SET_PARAMS_CFM_T *cfm);

/****************************************************************************
NAME
    connectionHandleDmBlePerAdvSetDataReq

DESCRIPTION
    This function will initiate an Periodic Advertising Set Data request.

RETURNS
   void
*/
void connectionHandleDmBlePerAdvSetDataReq(
        connectionDmPerAdvState *state,
        const CL_INTERNAL_DM_BLE_PER_ADV_SET_DATA_REQ_T *req
        );

/****************************************************************************
NAME
    connectionHandleDmBlePerAdvSetDataCfm

DESCRIPTION
    Handles status of Periodic Advertising Data Request

RETURNS
    void
*/
void connectionHandleDmBlePerAdvSetDataCfm(connectionDmPerAdvState *state,
        const DM_HCI_ULP_PERIODIC_ADV_SET_DATA_CFM_T *cfm
        );

/****************************************************************************
NAME
    connectionHandleDmBlePerAdvStartReq

DESCRIPTION
    This function will initiate an Periodic Advertising Start request.

RETURNS
   void
*/
void connectionHandleDmBlePerAdvStartReq(
        connectionDmPerAdvState *state,
        const CL_INTERNAL_DM_BLE_PER_ADV_START_REQ_T *req
        );

/****************************************************************************
NAME
    connectionHandleDmBlePerAdvStartCfm

DESCRIPTION
    Handles status of Periodic Advertising Start Request

RETURNS
    void
*/

void connectionHandleDmBlePerAdvStartCfm(connectionDmPerAdvState *state,
                            const DM_ULP_PERIODIC_ADV_START_CFM_T *cfm);

/****************************************************************************
NAME
    connectionHandleDmBlePerAdvStopReq

DESCRIPTION
    This function will initiate an Periodic Advertising Stop request.

RETURNS
   void
*/
void connectionHandleDmBlePerAdvStopReq(
        connectionDmPerAdvState *state,
        const CL_INTERNAL_DM_BLE_PER_ADV_STOP_REQ_T *req
        );

/****************************************************************************
NAME
    connectionHandleDmBlePerAdvStopCfm

DESCRIPTION
    Handles status of Periodic Advertising Stop Request

RETURNS
    void
*/

void connectionHandleDmBlePerAdvStopCfm(connectionDmPerAdvState *state,
                            const DM_ULP_PERIODIC_ADV_STOP_CFM_T *cfm);

/****************************************************************************
NAME
    connectionHandleDmBlePerAdvSetTransferReq

DESCRIPTION
    This function will initiate an Periodic Advertising Set Transfer request.

RETURNS
   void
*/
void connectionHandleDmBlePerAdvSetTransferReq(
        connectionDmPerAdvState *state,
        const CL_INTERNAL_DM_BLE_PER_ADV_SET_TRANSFER_REQ_T *req
        );

/****************************************************************************
NAME
    connectionHandleDmBlePerAdvSetTransferCfm

DESCRIPTION
    Handles status of Periodic Advertising Set Transfer Request

RETURNS
    void
*/

void connectionHandleDmBlePerAdvSetTransferCfm(connectionDmPerAdvState *state,
                            const DM_ULP_PERIODIC_ADV_SET_TRANSFER_CFM_T *cfm);

/****************************************************************************
NAME
    connectionHandleDmBlePerAdvReadMaxAdvDataLenReq

DESCRIPTION
    This function will initiate an Periodic Advertising Read Max Adv Data Len request.

RETURNS
   void
*/
void connectionHandleDmBlePerAdvReadMaxAdvDataLenReq(
        connectionDmPerAdvState *state,
        const CL_INTERNAL_DM_BLE_PER_ADV_READ_MAX_ADV_DATA_LEN_REQ_T *req
        );

/****************************************************************************
NAME
    connectionHandleDmBlePerAdvReadMaxAdvDataLenCfm

DESCRIPTION
    Handles status of Periodic Advertising Read Max Adv Data Len Request

RETURNS
    void
*/
void connectionHandleDmBlePerAdvReadMaxAdvDataLenCfm(
        connectionDmPerAdvState *state,
        const DM_ULP_PERIODIC_ADV_READ_MAX_ADV_DATA_LEN_CFM_T *cfm
        );

#endif /* DM_BLE_ADVERTISING_H */
#endif /* DISABLE_BLE */

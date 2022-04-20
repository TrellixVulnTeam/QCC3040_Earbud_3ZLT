/*!
\copyright  Copyright (c) 2018 - 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for management of Bluetooth Low Energy extended specific advertising

Provides control for Bluetooth Low Energy (BLE) advertisements.
*/


#ifndef LE_ADVERTSING_MANAGER_SELECT_EXTENDED_H_
#define LE_ADVERTSING_MANAGER_SELECT_EXTENDED_H_


#include "le_advertising_manager_private.h"

#include <connection.h>


#ifdef INCLUDE_ADVERTISING_EXTENSIONS

void leAdvertisingManager_SelectExtendedAdvertisingInit(void);

bool leAdvertisingManager_EnableExtendedAdvertising(bool enable);

void leAdvertisingManager_HandleExtendedSetAdvertisingDataCfm(const CL_DM_BLE_SET_EXT_ADV_DATA_CFM_T* cfm);

void leAdvertisingManager_HandleExtendedSetScanResponseDataCfm(const CL_DM_BLE_EXT_ADV_SET_SCAN_RESPONSE_DATA_CFM_T * cfm);

void leAdvertisingManager_HandleExtendedSetAdvertisingParamCfm(const CL_DM_BLE_SET_EXT_ADV_PARAMS_CFM_T *cfm);

void leAdvertisingManager_HandleExtendedSetAdvertisingEnableCfm(const CL_DM_BLE_EXT_ADVERTISE_ENABLE_CFM_T *cfm);

void leAdvertisingManager_HandleExtendedAdvertisingRegisterCfm(const CL_DM_BLE_EXT_ADV_REGISTER_APP_ADV_SET_CFM_T *cfm);

/*! \brief Check if extended advertising is currently active

    \return TRUE if extended advertising is active; FALSE otherwise.
*/
bool LeAdvertisingManager_IsExtendedAdvertisingActive(void);

#else
    
#define leAdvertisingManager_SelectExtendedAdvertisingInit() ((void)(0))

#define leAdvertisingManager_EnableExtendedAdvertising(x) (FALSE)

#define leAdvertisingManager_HandleExtendedSetAdvertisingDataCfm(x) ((void)(0))

#define leAdvertisingManager_HandleExtendedSetScanResponseDataCfm(x) ((void)(0))

#define leAdvertisingManager_HandleExtendedSetAdvertisingParamCfm(x) ((void)(0))

#define leAdvertisingManager_HandleExtendedSetAdvertisingEnableCfm(x) ((void)(0))

#define leAdvertisingManager_HandleExtendedAdvertisingRegisterCfm(x) ((void)(0))

#define LeAdvertisingManager_IsExtendedAdvertisingActive() (FALSE)
    
#endif /* INCLUDE_ADVERTISING_EXTENSIONS*/


#endif /* LE_ADVERTSING_MANAGER_SELECT_EXTENDED_H_ */

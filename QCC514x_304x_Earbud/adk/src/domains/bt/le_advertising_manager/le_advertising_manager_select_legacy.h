/*!
\copyright  Copyright (c) 2018 - 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for management of Bluetooth Low Energy legacy specific advertising

Provides control for Bluetooth Low Energy (BLE) advertisements.
*/


#ifndef LE_ADVERTSING_MANAGER_SELECT_LEGACY_H_
#define LE_ADVERTSING_MANAGER_SELECT_LEGACY_H_


#include "le_advertising_manager_private.h"

#include <connection.h>


void leAdvertisingManager_SelectLegacyAdvertisingInit(void);

void leAdvertisingManager_HandleSetLegacyAdvertisingDataCfm(uint16 status);

void leAdvertisingManager_HandleLegacySetScanResponseDataCfm(uint16 status);

void leAdvertisingManager_HandleLegacySetAdvertisingParamCfm(uint16 status);

void leAdvertisingManager_HandleInternalDataUpdateRequest(void);

void leAdvertisingManager_ScheduleInternalDataUpdate(void);

void leAdvertisingManager_SetupAdvertParams(void);

void leAdvertisingManager_SendMessageParameterSwitchover(void);

void leAdvertisingManager_CancelMessageParameterSwitchover(void);

void leAdvertisingManager_HandleInternalIntervalSwitchover(void);

#endif /* LE_ADVERTSING_MANAGER_SELECT_LEGACY_H_ */

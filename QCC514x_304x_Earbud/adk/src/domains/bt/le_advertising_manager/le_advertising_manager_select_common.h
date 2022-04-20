/*!
\copyright  Copyright (c) 2018 - 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for management of Bluetooth Low Energy legacy specific advertising

Provides control for Bluetooth Low Energy (BLE) advertisements.
*/


#ifndef LE_ADVERTSING_MANAGER_SELECT_COMMON_H_
#define LE_ADVERTSING_MANAGER_SELECT_COMMON_H_


#include "le_advertising_manager.h"
#include "le_advertising_manager_private.h"


bool leAdvertisingManager_IsLegacySet(const le_adv_data_set_t set);

le_adv_data_set_t leAdvertisingManager_SelectOnlyLegacySet(le_adv_data_set_t set);

bool leAdvertisingManager_IsExtendedSet(const le_adv_data_set_t set);

le_adv_data_set_t leAdvertisingManager_SelectOnlyExtendedSet(le_adv_data_set_t set);

void leAdvertisingManager_SetParamsUpdateFlag(bool params_update);

void leAdvertisingManager_SetDataUpdateRequired(const le_adv_data_set_t set, bool data_update);

/*! \brief Schedules the start advertising */
void LeAdvertisingManager_ScheduleAdvertisingStart(const le_adv_data_set_t set);

/*! \brief  Local Function to Send Select Data Set Confirmation Messages Following an Internal Advertising Start Request */
void LeAdvertisingManager_SendSelectConfirmMessage(void);

bool LeAdvertisingManager_CanAdvertisingBeStarted(void);

void leAdvertisingManager_ClearDataSetMessageStatusBitmask(void);

void leAdvertisingManager_ClearDataSetSelectBitmask(void);

void leAdvertisingManager_SetDataSetSelectMessageStatusBitmask(le_adv_data_set_t set, bool enable);

void leAdvertisingManager_SetDataSetSelectMessageStatusAfterRelease(le_adv_data_set_t set);

void leAdvertisingManager_SetDataSetSelectBitmask(le_adv_data_set_t set, bool enable);

bool leAdvertisingManager_IsDataSetSelected(void);

le_adv_data_set_t leAdvertisingManager_GetDataSetSelected(void);

void leAdvertisingManager_SetDataSetEventType(le_adv_event_type_t event_type);

le_adv_event_type_t leAdvertisingManager_GetDataSetEventType(void);

ble_adv_type leAdvertisingManager_GetAdvertType(le_adv_event_type_t event);

void leAdvertisingManager_GetDefaultAdvertisingIntervalParams(le_adv_common_parameters_t * param);

ble_adv_params_t leAdvertisingManager_GetAdvertisingIntervalParams(void);

#endif /* LE_ADVERTSING_MANAGER_SELECT_COMMON_H_ */

/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Manage execution of callbacks to construct adverts and scan response
*/

#ifndef LE_ADVERTSING_MANAGER_DATA_COMMON_H_
#define LE_ADVERTSING_MANAGER_DATA_COMMON_H_

#include "le_advertising_manager.h"
#include "le_advertising_manager_private.h"


typedef enum
{
    le_adv_manager_data_packet_advert,
    le_adv_manager_data_packet_scan_response,
    le_adv_manager_data_packet_max
} le_adv_manager_data_packet_type_t;


typedef bool(*CreateNewDataPacket)(le_adv_manager_data_packet_type_t type);
typedef bool(*DestroyDataPacket)(le_adv_manager_data_packet_type_t type);
typedef unsigned(*GetSizeDataPacket)(le_adv_manager_data_packet_type_t type);
typedef bool(*AddItemToDataPacket)(le_adv_manager_data_packet_type_t type, const le_adv_data_item_t* item);
typedef void(*SetupAdvertData)(void);
typedef void(*SetupScanResponseData)(void);

typedef struct
{
    CreateNewDataPacket     createNewDataPacket;
    DestroyDataPacket       destroyDataPacket;
    GetSizeDataPacket       getSizeDataPacket;
    AddItemToDataPacket     addItemToDataPacket;
    SetupAdvertData         setupAdvertData;
    SetupScanResponseData   setupScanResponseData;
} le_advertising_manager_data_packet_if_t;


void leAdvertisingManager_DataInit(void);
/*!
    Build advertising and scan response data packets
    
    Must be called before use of 
    - leAdvertisingManager_SetupScanResponseData
    - leAdvertisingManager_SetupAdvertData
    - leAdvertisingManager_ClearData
    
    \param set Mask of the data set(s) to build
    \returns TRUE if successful, FALSE if there is no data
 */
bool leAdvertisingManager_BuildData(le_adv_data_set_t set);

/*!
    Clear advertising and scan response data packets
    Must be called to clear data created with
    leAdvertisingManager_BuildData
    
    \param set Mask of the data set(s) to clear
 */
void leAdvertisingManager_ClearData(le_adv_data_set_t set);

void leAdvertisingManager_SetupAdvertData(le_adv_data_set_t set);

void leAdvertisingManager_SetupScanResponseData(le_adv_data_set_t set);

bool leAdvertisingManager_RegisterDataClient(le_adv_data_set_t set, const le_advertising_manager_data_packet_if_t* interface);

void leAdvertisingManager_DebugDataItems(const uint8 size, const uint8 * data);

const le_advertising_manager_data_packet_if_t* leAdvertisingManager_GetDataPacketFromSet(le_adv_data_set_t set);

#endif /* LE_ADVERTSING_MANAGER_DATA_COMMON_H_ */

/*!
\copyright  Copyright (c) 2018 - 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Manage execution of callbacks to construct adverts and scan response
*/

#include "le_advertising_manager_data_legacy.h"

#include "le_advertising_manager_data_common.h"
#include <stdlib.h>
#include <panic.h>


/*! Maximum data length of an advert if advertising length extensions are not used */
#define MAX_AD_DATA_SIZE_IN_OCTETS  (0x1F)

typedef struct 
{
    uint8  data[MAX_AD_DATA_SIZE_IN_OCTETS];
    uint8* head;
    uint8  space;
} le_adv_data_packet_t;

static le_adv_data_packet_t* le_adv_data_packet[le_adv_manager_data_packet_max];


static bool leAdvertisingManager_AddDataItemToLegacyPacket(le_adv_data_packet_t* packet, const le_adv_data_item_t* item)
{
    PanicNull(packet);
    PanicNull((le_adv_data_item_t*)item);
    
    if(item->size > packet->space)
    {
        return FALSE;
    }
    
    if(item->size)
    {
        memcpy(packet->head, item->data, item->size);
        packet->head  += item->size;
        packet->space -= item->size;
    }
    
    return TRUE;
}

static bool leAdvertisingManager_createNewLegacyDataPacket(le_adv_manager_data_packet_type_t type)
{
    le_adv_data_packet_t* new_packet = PanicUnlessMalloc(sizeof(le_adv_data_packet_t));
    
    new_packet->head = new_packet->data;
    new_packet->space = MAX_AD_DATA_SIZE_IN_OCTETS;
    
    le_adv_data_packet[type] = new_packet;
    
    return TRUE;
}

static bool leAdvertisingManager_destroyLegacyDataPacket(le_adv_manager_data_packet_type_t type)
{
    free(le_adv_data_packet[type]);
    le_adv_data_packet[type] = NULL;
    
    return TRUE;
}

static unsigned leAdvertisingManager_getSizeLegacyDataPacket(le_adv_manager_data_packet_type_t type)
{
    return (le_adv_data_packet[type]->head - le_adv_data_packet[type]->data);
}

static bool leAdvertisingManager_addItemToLegacyDataPacket(le_adv_manager_data_packet_type_t type, const le_adv_data_item_t* item)
{
    return leAdvertisingManager_AddDataItemToLegacyPacket(le_adv_data_packet[type], item);
}


static void leAdvertisingManager_setupLegacyAdvertData(void)
{
    uint8 size_advert = leAdvertisingManager_getSizeLegacyDataPacket(le_adv_manager_data_packet_advert);
    uint8* advert_start = size_advert ? le_adv_data_packet[le_adv_manager_data_packet_advert]->data : NULL;
    
    DEBUG_LOG_VERBOSE("leAdvertisingManager_setupLegacyAdvertData, Size is %d", size_advert);

    leAdvertisingManager_DebugDataItems(size_advert, advert_start);

    ConnectionDmBleSetAdvertisingDataReq(size_advert, advert_start);
}

static void leAdvertisingManager_setupLegacyScanResponseData(void)
{
    uint8 size_scan_rsp = leAdvertisingManager_getSizeLegacyDataPacket(le_adv_manager_data_packet_scan_response);
    uint8* scan_rsp_start = size_scan_rsp ? le_adv_data_packet[le_adv_manager_data_packet_scan_response]->data : NULL;

    DEBUG_LOG("leAdvertisingManager_setupLegacyScanResponseData, Size is %d", size_scan_rsp);

    leAdvertisingManager_DebugDataItems(size_scan_rsp, scan_rsp_start);

    ConnectionDmBleSetScanResponseDataReq(size_scan_rsp, scan_rsp_start);
}

static const le_advertising_manager_data_packet_if_t le_advertising_manager_legacy_data_fns = 
{
    .createNewDataPacket = leAdvertisingManager_createNewLegacyDataPacket,
    .destroyDataPacket = leAdvertisingManager_destroyLegacyDataPacket,
    .getSizeDataPacket = leAdvertisingManager_getSizeLegacyDataPacket,
    .addItemToDataPacket = leAdvertisingManager_addItemToLegacyDataPacket,
    .setupAdvertData = leAdvertisingManager_setupLegacyAdvertData,
    .setupScanResponseData = leAdvertisingManager_setupLegacyScanResponseData
};

void leAdvertisingManager_RegisterLegacyDataIf(void)
{
    leAdvertisingManager_RegisterDataClient(LE_ADV_MGR_ADVERTISING_SET_LEGACY,
                                            &le_advertising_manager_legacy_data_fns);
                                            
    for (unsigned index = 0; index < le_adv_manager_data_packet_max; index++)
    {
        le_adv_data_packet[index] = NULL;
    }
}


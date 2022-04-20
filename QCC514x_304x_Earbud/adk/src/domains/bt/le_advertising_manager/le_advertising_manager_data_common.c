/*!
\copyright  Copyright (c) 2018 - 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Manage execution of callbacks to construct adverts and scan response
*/

#include "le_advertising_manager_data_common.h"
#include "le_advertising_manager_clients.h"
#include "le_advertising_manager_uuid.h"
#include "le_advertising_manager_local_name.h"

#include <stdlib.h>
#include <panic.h>

#define for_all_data_sets(params) for((params)->data_set = le_adv_data_set_handset_identifiable; (params)->data_set <= le_adv_data_set_extended_handset; ((params)->data_set) <<= 1)

#define for_all_completeness(params) for((params)->completeness = le_adv_data_completeness_full; (params)->completeness <= le_adv_data_completeness_can_be_skipped; (params)->completeness++)

#define for_all_placements(params) for((params)->placement = le_adv_data_placement_advert; (params)->placement <= le_adv_data_placement_dont_care; (params)->placement++)

#define for_all_params_in_set(params, set) for_all_completeness(params) for_all_placements(params) for_all_data_sets(params) if(set & (params)->data_set)

#define LE_ADV_MGR_MAX_DATA_CLIENTS 2

typedef struct
{
    const le_advertising_manager_data_packet_if_t* interface;
    le_adv_data_set_t set;
} le_advertising_manager_data_if_t;

static le_advertising_manager_data_if_t le_adv_mgr_data_if[LE_ADV_MGR_MAX_DATA_CLIENTS];
static unsigned le_adv_mgr_size_data_if = 0;

const le_advertising_manager_data_packet_if_t* leAdvertisingManager_GetDataPacketFromSet(le_adv_data_set_t set)
{
    for (unsigned index = 0; index < le_adv_mgr_size_data_if; index++)
    {
        if (le_adv_mgr_data_if[index].set & set)
        {
            return le_adv_mgr_data_if[index].interface;
        }
    }
    
    return NULL;
}

static void leAdvertisingManager_AddDataItem(le_adv_data_set_t set, const le_adv_data_item_t* item, const le_adv_data_params_t* params)
{
    bool added = FALSE;
    const le_advertising_manager_data_packet_if_t* interface = leAdvertisingManager_GetDataPacketFromSet(set);
    
    if (interface != NULL)
    {
        switch(params->placement)
        {
            case le_adv_data_placement_advert:
            {
                added = interface->addItemToDataPacket(le_adv_manager_data_packet_advert, item);
                break;
            }
            
            case le_adv_data_placement_scan_response:
            {
                added = interface->addItemToDataPacket(le_adv_manager_data_packet_scan_response, item);
                break;
            }
            
            case le_adv_data_placement_dont_care:
            {
                added = interface->addItemToDataPacket(le_adv_manager_data_packet_advert, item);
                if(!added)
                {
                    added = interface->addItemToDataPacket(le_adv_manager_data_packet_scan_response, item);
                }
                break;
            }
            
            default:
            {
                added = FALSE;
                DEBUG_LOG_ERROR("leAdvertisingManager_AddDataItem, Unrecognised item placement attribute %d", params->placement);
                Panic();
                break;
            }
        }
    }
    
    if(params->completeness != le_adv_data_completeness_can_be_skipped)
    {
        DEBUG_LOG_VERBOSE("leAdvertisingManager_AddDataItem, Cannot skip the item, item ptr is 0x%x", item);

        if(item)
        {
            leAdvertisingManager_DebugDataItems(item->size, item->data);
        }

        PanicFalse(added);
    }
}

static void leAdvertisingManager_ProcessDataItem(const le_adv_data_item_t* item, const le_adv_data_params_t* params)
{
    if(item->data && item->size)
    {
        uint8 data_type = item->data[AD_DATA_TYPE_OFFSET];
        
        DEBUG_LOG_VERBOSE("leAdvertisingManager_ProcessDataItem %d", data_type);
        
        switch(data_type)
        {
            case ble_ad_type_complete_uuid16:
            {
                LeAdvertisingManager_Uuid16(item, params);
                break;
            }
            
            case ble_ad_type_complete_uuid32:
            {
                LeAdvertisingManager_Uuid32(item, params);
                break;
            }
            
            case ble_ad_type_complete_uuid128:
            {
                LeAdvertisingManager_Uuid128(item, params);
                break;
            }
            
            case ble_ad_type_complete_local_name:
            {
                LeAdvertisingManager_LocalNameRegister(item, params);
                break;
            }

            default:
            {
                break;
            }
        }
    }
}

static void leAdvertisingManager_BuildDataItem(le_adv_data_set_t set, const le_adv_data_item_t* item, const le_adv_data_params_t* params)
{
    if(item->data && item->size)
    {
        uint8 data_type = item->data[AD_DATA_TYPE_OFFSET];
        
        switch(data_type)
        {
            case ble_ad_type_complete_uuid16:
            case ble_ad_type_complete_uuid32:
            case ble_ad_type_complete_uuid128:
            case ble_ad_type_complete_local_name:
            {
                break;
            }

            default:
            {
                DEBUG_LOG("leAdvertisingManager_BuildDataItem %d", data_type);
                leAdvertisingManager_AddDataItem(set, item, params);
                break;
            }
        }
    }
}

static void leAdvertisingManager_ProcessClientData(le_adv_mgr_register_handle client_handle, const le_adv_data_params_t* params)
{
    size_t num_items = leAdvertisingManager_ClientNumItems(client_handle, params);
    
    if(num_items)
    {
        DEBUG_LOG_V_VERBOSE("leAdvertisingManager_ProcessClientData num_items %d", num_items);
        
        for(unsigned i = 0; i < num_items; i++)
        {
            le_adv_data_item_t item = client_handle->callback->GetItem(params, i);
            leAdvertisingManager_ProcessDataItem(&item, params);
        }
    }
}

static void leAdvertisingManager_BuildClientData(le_adv_data_set_t set, le_adv_mgr_register_handle client_handle, const le_adv_data_params_t* params)
{
    size_t num_items = leAdvertisingManager_ClientNumItems(client_handle, params);
    
    if(num_items)
    {
        DEBUG_LOG_V_VERBOSE("leAdvertisingManager_ProcessClientData num_items %d", num_items);
        
        for(unsigned i = 0; i < num_items; i++)
        {
            le_adv_data_item_t item = client_handle->callback->GetItem(params, i);
            leAdvertisingManager_BuildDataItem(set, &item, params);
        }
    }
}

static void leAdvertisingManager_ClearClientData(le_adv_mgr_register_handle client_handle, const le_adv_data_params_t* params)
{
    size_t num_items = leAdvertisingManager_ClientNumItems(client_handle, params);
    
    if(num_items)
    {
        client_handle->callback->ReleaseItems(params);
    }
}

static void leAdvertisingManager_ProcessAllClientsData(const le_adv_data_params_t* params)
{
    le_adv_mgr_client_iterator_t iterator;
    le_adv_mgr_register_handle client_handle = leAdvertisingManager_HeadClient(&iterator);
    
    while(client_handle)
    {
        leAdvertisingManager_ProcessClientData(client_handle, params);
        client_handle = leAdvertisingManager_NextClient(&iterator);
    }
}

static void leAdvertisingManager_BuildAllClientsData(le_adv_data_set_t set, const le_adv_data_params_t* params)
{
    le_adv_mgr_client_iterator_t iterator;
    le_adv_mgr_register_handle client_handle = leAdvertisingManager_HeadClient(&iterator);
    
    while(client_handle)
    {
        leAdvertisingManager_BuildClientData(set, client_handle, params);
        client_handle = leAdvertisingManager_NextClient(&iterator);
    }
}

static void leAdvertisingManager_ClearAllClientsData(const le_adv_data_params_t* params)
{
    le_adv_mgr_client_iterator_t iterator;
    le_adv_mgr_register_handle client_handle = leAdvertisingManager_HeadClient(&iterator);
    
    while(client_handle)
    {
        leAdvertisingManager_ClearClientData(client_handle, params);
        client_handle = leAdvertisingManager_NextClient(&iterator);
    }
}

static void leAdvertisingManager_BuildLocalNameData(le_adv_data_set_t set, const le_adv_data_params_t* params)
{
    le_adv_data_item_t item;
    
    if(LeAdvertisingManager_LocalNameGet(&item, params))
    {
        leAdvertisingManager_AddDataItem(set, &item, params);
    }
}

static void leAdvertisingManager_BuildUuidData(le_adv_data_set_t set, const le_adv_data_params_t* params)
{
    le_adv_data_item_t item;
    
    if(LeAdvertisingManager_Uuid16List(&item, params))
    {
        leAdvertisingManager_AddDataItem(set, &item, params);
    }
    if(LeAdvertisingManager_Uuid32List(&item, params))
    {
        leAdvertisingManager_AddDataItem(set, &item, params);
    }
    if(LeAdvertisingManager_Uuid128List(&item, params))
    {
        leAdvertisingManager_AddDataItem(set, &item, params);
    }
}

void leAdvertisingManager_DataInit(void)
{
    le_adv_mgr_size_data_if = 0;
    for (unsigned index = 0; index < LE_ADV_MGR_MAX_DATA_CLIENTS; index++)
    {
        le_adv_mgr_data_if[index].interface = NULL;
        le_adv_mgr_data_if[index].set = 0;
    }
}

bool leAdvertisingManager_BuildData(le_adv_data_set_t set)
{
    le_adv_data_params_t params;
    const le_advertising_manager_data_packet_if_t* interface = leAdvertisingManager_GetDataPacketFromSet(set);
    
    if (interface != NULL)
    {
        interface->createNewDataPacket(le_adv_manager_data_packet_advert);
        interface->createNewDataPacket(le_adv_manager_data_packet_scan_response);

        LeAdvertisingManager_UuidReset();
        LeAdvertisingManager_LocalNameReset();
        
        for_all_params_in_set(&params, set)
        {
            leAdvertisingManager_ProcessAllClientsData(&params);
        }
        
        for_all_params_in_set(&params, set)
        {
            leAdvertisingManager_BuildAllClientsData(set, &params);
            leAdvertisingManager_BuildLocalNameData(set, &params);
            leAdvertisingManager_BuildUuidData(set, &params);
        }
        
        if(interface->getSizeDataPacket(le_adv_manager_data_packet_advert) || 
            interface->getSizeDataPacket(le_adv_manager_data_packet_scan_response))
        {
            return TRUE;
        }
    }
    
    return FALSE;
}


void leAdvertisingManager_ClearData(le_adv_data_set_t set)
{
    le_adv_data_params_t params;
    const le_advertising_manager_data_packet_if_t* interface = leAdvertisingManager_GetDataPacketFromSet(set);
    
    if (interface != NULL)
    {
        interface->destroyDataPacket(le_adv_manager_data_packet_advert);
        interface->destroyDataPacket(le_adv_manager_data_packet_scan_response);
    }
    
    LeAdvertisingManager_UuidReset();
    
    for_all_params_in_set(&params, set)
    {
        leAdvertisingManager_ClearAllClientsData(&params);
    }
}

void leAdvertisingManager_SetupAdvertData(le_adv_data_set_t set)
{
    const le_advertising_manager_data_packet_if_t* interface = leAdvertisingManager_GetDataPacketFromSet(set);
    
    if (interface != NULL)
    {
        interface->setupAdvertData();
    }
}

void leAdvertisingManager_SetupScanResponseData(le_adv_data_set_t set)
{
    const le_advertising_manager_data_packet_if_t* interface = leAdvertisingManager_GetDataPacketFromSet(set);
    
    if (interface != NULL)
    {
        interface->setupScanResponseData();
    }
}

bool leAdvertisingManager_RegisterDataClient(le_adv_data_set_t set, const le_advertising_manager_data_packet_if_t* interface)
{
    if (le_adv_mgr_size_data_if < LE_ADV_MGR_MAX_DATA_CLIENTS)
    {
        for (unsigned index = 0; index < le_adv_mgr_size_data_if; index++)
        {
            if (le_adv_mgr_data_if[index].set & set)
            {
                /* Set should not already be registered as this will fail */
                return FALSE;
            }
        }
        
        PanicNull((void *)interface->createNewDataPacket);
        PanicNull((void *)interface->destroyDataPacket);
        PanicNull((void *)interface->getSizeDataPacket);
        PanicNull((void *)interface->addItemToDataPacket);
        PanicNull((void *)interface->setupAdvertData);
        PanicNull((void *)interface->setupScanResponseData);
        
        le_adv_mgr_data_if[le_adv_mgr_size_data_if].interface = interface;
        le_adv_mgr_data_if[le_adv_mgr_size_data_if].set = set;
        le_adv_mgr_size_data_if++;
        return TRUE;
    }
    
    return FALSE;
}

void leAdvertisingManager_DebugDataItems(const uint8 size, const uint8 * data)
{
    if(size && data)
    {
        for(int i=0;i<size;i++)
            DEBUG_LOG_V_VERBOSE("Data[%d] is 0x%x", i, data[i]);
    }
}

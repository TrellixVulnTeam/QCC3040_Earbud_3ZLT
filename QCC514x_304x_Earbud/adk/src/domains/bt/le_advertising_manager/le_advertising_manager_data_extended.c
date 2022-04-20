/*!
\copyright  Copyright (c) 2018 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Manage execution of callbacks to construct extended adverts and scan response
*/

#ifdef INCLUDE_ADVERTISING_EXTENSIONS

#include "le_advertising_manager_data_extended.h"

#include "le_advertising_manager_data_common.h"
#include "le_advertising_manager_clients.h"
#include "le_advertising_manager_uuid.h"
#include "le_advertising_manager_local_name.h"

#include <stdlib.h>
#include <panic.h>


/*! Maximum data length of an extended advert (as supported by connection library). */
#define MAX_EXT_AD_DATA_SIZE_IN_OCTETS  (251u)

/*! Maximum number of extended advert data buffers (as supported by connection library). */
#define MAX_EXT_AD_DATA_BUFFER_COUNT    (8u)

/*! Maximum data length of a single extended advert buffer (as supported by the connection library). */
#define MAX_EXT_AD_DATA_BUFFER_SIZE_IN_OCTETS   (32u)


typedef struct
{
    uint8 *data[MAX_EXT_AD_DATA_BUFFER_COUNT];
    uint8 data_size;
} le_ext_adv_data_packet_t;

static le_ext_adv_data_packet_t *le_ext_adv_data_packets[le_adv_manager_data_packet_max] = {0};


/* Note: this function assumes that MAX_EXT_AD_DATA_BUFFER_SIZE_IN_OCTETS is 32 */
static void leAdvertisingManager_DebugExtendedDataItems(le_ext_adv_data_packet_t *packet)
{
    if (packet && packet->data_size)
    {
        for (uint8 i = 0; i < ARRAY_DIM(packet->data); i++)
        {
            if (packet->data[i])
            {
                DEBUG_LOG_V_VERBOSE("[ 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x",
                                    packet->data[i][0], packet->data[i][1], packet->data[i][2], packet->data[i][3],
                                    packet->data[i][4], packet->data[i][5], packet->data[i][6], packet->data[i][7]);
                DEBUG_LOG_V_VERBOSE("  0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x",
                                    packet->data[i][8], packet->data[i][9], packet->data[i][10], packet->data[i][11],
                                    packet->data[i][12], packet->data[i][13], packet->data[i][14], packet->data[i][15]);
                DEBUG_LOG_V_VERBOSE("  0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x",
                                    packet->data[i][16], packet->data[i][17], packet->data[i][18], packet->data[i][19],
                                    packet->data[i][20], packet->data[i][21], packet->data[i][22], packet->data[i][23]);
                DEBUG_LOG_V_VERBOSE("  0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x ]",
                                    packet->data[i][24], packet->data[i][25], packet->data[i][26], packet->data[i][27],
                                    packet->data[i][28], packet->data[i][29], packet->data[i][30], packet->data[i][31]);
            }
        }
    }
}

static bool leAdvertisingManager_AddDataItemToExtendedPacket(le_ext_adv_data_packet_t* packet, const le_adv_data_item_t* item)
{
    PanicNull(packet);
    PanicNull((le_adv_data_item_t*)item);

    DEBUG_LOG("leAdvertisingManager_AddDataItemToExtendedPacket packet_size %u item_size %u",
              packet->data_size, item->size);

    if ((MAX_EXT_AD_DATA_SIZE_IN_OCTETS - packet->data_size) < item->size)
    {
        return FALSE;
    }
    
    if(item->size)
    {
        uint8 data_to_copy = item->size;
        const uint8 *data_ptr = item->data;

        while (data_to_copy)
        {
            int current_buffer_idx = (packet->data_size / MAX_EXT_AD_DATA_BUFFER_SIZE_IN_OCTETS);
            size_t current_buffer_pos = (packet->data_size % MAX_EXT_AD_DATA_BUFFER_SIZE_IN_OCTETS);
            size_t current_buffer_space = (MAX_EXT_AD_DATA_BUFFER_SIZE_IN_OCTETS - current_buffer_pos);

            if (!packet->data[current_buffer_idx])
            {
                packet->data[current_buffer_idx] = PanicUnlessMalloc(MAX_EXT_AD_DATA_BUFFER_SIZE_IN_OCTETS);
                memset(packet->data[current_buffer_idx], 0, MAX_EXT_AD_DATA_BUFFER_SIZE_IN_OCTETS);
            }

            uint8 *current_buffer_ptr = &packet->data[current_buffer_idx][current_buffer_pos];
            uint8 size = (data_to_copy > current_buffer_space) ? current_buffer_space : data_to_copy;

            memmove(current_buffer_ptr, data_ptr, size);
            packet->data_size += size;

            data_ptr += size;
            data_to_copy -= size;
        }
    }
    
    return TRUE;
}

static bool leAdvertisingManager_createNewExtendedDataPacket(le_adv_manager_data_packet_type_t type)
{
    le_ext_adv_data_packet_t* new_packet;

    if (le_ext_adv_data_packets[type])
    {
        new_packet = le_ext_adv_data_packets[type];
    }
    else
    {
        new_packet = PanicUnlessMalloc(sizeof(*new_packet));
        memset(new_packet, 0, sizeof(*new_packet));
        le_ext_adv_data_packets[type] = new_packet;
        DEBUG_LOG_VERBOSE("leAdvertisingManager_createNewExtendedDataPacket type: enum:le_adv_manager_data_packet_type_t:%d new_ptr: %p prev_ptr: %p",
                          type, new_packet, le_ext_adv_data_packets[type]);
    }
    
    return TRUE;
}

static bool leAdvertisingManager_destroyExtendedDataPacket(le_adv_manager_data_packet_type_t type)
{
    le_ext_adv_data_packet_t *packet = le_ext_adv_data_packets[type];

    DEBUG_LOG_VERBOSE("leAdvertisingManager_destroyExtendedDataPacket type: enum:le_adv_manager_data_packet_type_t:%d ptr: %p",
                      type, packet);

    if (le_ext_adv_data_packets[type])
    {
        for (uint8 i = 0; i < ARRAY_DIM(le_ext_adv_data_packets[type]->data); i++)
        {
            free(le_ext_adv_data_packets[type]->data[i]);
        }

        free(le_ext_adv_data_packets[type]);
        le_ext_adv_data_packets[type] = NULL;
    }

    return TRUE;
}

static unsigned leAdvertisingManager_getSizeExtendedDataPacket(le_adv_manager_data_packet_type_t type)
{
    return (le_ext_adv_data_packets[type] ? le_ext_adv_data_packets[type]->data_size : 0);
}

static bool leAdvertisingManager_addItemToExtendedDataPacket(le_adv_manager_data_packet_type_t type, const le_adv_data_item_t* item)
{
    return leAdvertisingManager_AddDataItemToExtendedPacket(le_ext_adv_data_packets[type], item);
}

static void leAdvertisingManager_setupExtendedAdvertData(void)
{
    uint8 size_advert = leAdvertisingManager_getSizeExtendedDataPacket(le_adv_manager_data_packet_advert);
    uint8* advert_start[MAX_EXT_AD_DATA_BUFFER_COUNT] = {0};
    le_ext_adv_data_packet_t *packet = le_ext_adv_data_packets[le_adv_manager_data_packet_advert];

    if (size_advert)
    {
        for (uint8 i = 0; i < ARRAY_DIM(advert_start); i++)
        {
            advert_start[i] = packet->data[i];
        }
    }

    DEBUG_LOG_VERBOSE("leAdvertisingManager_setupExtendedAdvertData, Size is %d data:%p", size_advert, advert_start[0]);

    leAdvertisingManager_DebugExtendedDataItems(packet);

    ConnectionDmBleExtAdvSetDataReq(AdvManagerGetTask(), ADV_HANDLE_APP_SET_1, complete_data, size_advert, advert_start);

    /* ConnectionDmBleExtAdvSetDataReq takes ownership of the buffers passed
       in via advert_start, so set the pointers to NULL in the packet data. */
    packet->data_size = 0;
    memset(packet->data, 0, sizeof(packet->data));
}

static void leAdvertisingManager_setupExtendedScanResponseData(void)
{
    uint8 size_scan_rsp = leAdvertisingManager_getSizeExtendedDataPacket(le_adv_manager_data_packet_scan_response);
    uint8* scan_rsp_start[MAX_EXT_AD_DATA_BUFFER_COUNT] = {0};
    le_ext_adv_data_packet_t *packet = le_ext_adv_data_packets[le_adv_manager_data_packet_scan_response];

    if (size_scan_rsp)
    {
        for (uint8 i = 0; i < ARRAY_DIM(scan_rsp_start); i++)
        {
            scan_rsp_start[i] = packet->data[i];
        }
    }
    
    DEBUG_LOG("leAdvertisingManager_setupExtendedScanResponseData, Size is %d", size_scan_rsp);

    leAdvertisingManager_DebugExtendedDataItems(packet);

    ConnectionDmBleExtAdvSetScanRespDataReq(AdvManagerGetTask(), ADV_HANDLE_APP_SET_1, complete_data, size_scan_rsp, scan_rsp_start);

    /* ConnectionDmBleExtAdvSetScanRespDataReq takes ownership of the buffers passed
       in via scan_rsp_start, so set the pointers to NULL in the packet data. */
    packet->data_size = 0;
    memset(packet->data, 0, sizeof(packet->data));
}

static const le_advertising_manager_data_packet_if_t le_advertising_manager_extended_data_fns = 
{
    .createNewDataPacket = leAdvertisingManager_createNewExtendedDataPacket,
    .destroyDataPacket = leAdvertisingManager_destroyExtendedDataPacket,
    .getSizeDataPacket = leAdvertisingManager_getSizeExtendedDataPacket,
    .addItemToDataPacket = leAdvertisingManager_addItemToExtendedDataPacket,
    .setupAdvertData = leAdvertisingManager_setupExtendedAdvertData,
    .setupScanResponseData = leAdvertisingManager_setupExtendedScanResponseData
};


void leAdvertisingManager_RegisterExtendedDataIf(void)
{
    leAdvertisingManager_RegisterDataClient(LE_ADV_MGR_ADVERTISING_SET_EXTENDED,
                                            &le_advertising_manager_extended_data_fns);
                                            
    for (unsigned index = 0; index < le_adv_manager_data_packet_max; index++)
    {
        le_ext_adv_data_packets[index] = NULL;
    }
}

#endif /* INCLUDE_ADVERTISING_EXTENSIONS*/

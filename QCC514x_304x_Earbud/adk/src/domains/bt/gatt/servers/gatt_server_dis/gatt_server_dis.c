/*!
\copyright  Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       gatt_server_dis.c
\brief      Implementation of the GATT Device Information Service Server module.
*/

#ifdef INCLUDE_GATT_DEVICE_INFO_SERVER

#include "le_advertising_manager.h"

#include "gatt_server_dis.h"

#include "gatt_handler.h"
#include "gatt_connect.h"
#include "gatt_handler_db_if.h"

#include "device_info.h"

#include <hydra_macros.h>
#include <logging.h>
#include <gatt.h>
#include <string.h>
#include <stdlib.h>
#include <panic.h>


#define NUMBER_OF_ADVERT_DATA_ITEMS         1
#define SIZE_DEVICE_INFO_ADVERT             4

/* Contains device information service supported characteristics data */
static gatt_dis_init_params_t device_info_server_params;

/* App passes reference of gatt_server_device_info to library via GattDeviceInfoServerInit.
library reset this struct so why device_info_server_params is required to pass data separately. */
gattServerDeviceInfoData gatt_server_device_info = {0};

static unsigned int gattServerDeviceInfo_NumberOfAdvItems(const le_adv_data_params_t * params);
static le_adv_data_item_t gattServerDeviceInfo_GetAdvDataItems(const le_adv_data_params_t * params, unsigned int id);
static void gattServerDeviceInfo_ReleaseAdvDataItems(const le_adv_data_params_t * params);

static const le_adv_data_callback_t gatt_device_info_le_advert_callback =
{
    .GetNumberOfItems = gattServerDeviceInfo_NumberOfAdvItems,
    .GetItem = gattServerDeviceInfo_GetAdvDataItems,
    .ReleaseItems = gattServerDeviceInfo_ReleaseAdvDataItems
};

static const uint8 gatt_device_info_advert_data[SIZE_DEVICE_INFO_ADVERT] = { \
    SIZE_DEVICE_INFO_ADVERT - 1, \
    ble_ad_type_complete_uuid16, \
    GATT_SERVICE_UUID_DEVICE_INFORMATION & 0xFF, \
    GATT_SERVICE_UUID_DEVICE_INFORMATION >> 8 \
};

static le_adv_data_item_t gatt_device_info_advert;

static void gattServerDeviceInfo_MessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(id);
    UNUSED(message);

    DEBUG_LOG_WARN("gattServerDeviceInfo_MessageHandler MESSAGE:0x%x", id);
}

static void gattServerDeviceInfo_SetupAdvertising(void)
{
    gatt_device_info_advert.size = SIZE_DEVICE_INFO_ADVERT;
    gatt_device_info_advert.data = gatt_device_info_advert_data;

    LeAdvertisingManager_Register(NULL, &gatt_device_info_le_advert_callback);
}

static bool gattServerDeviceInfo_InitialiseDisParams(void)
{
    bool initialised = FALSE;
    DEBUG_LOG_FN_ENTRY("gattServerDeviceInfo_InitialiseDisParams");

    device_info_server_params.dis_strings = calloc (1, sizeof(gatt_dis_strings_t));

    if (device_info_server_params.dis_strings != NULL)
    {
        device_info_server_params.dis_strings->manufacturer_name_string = DeviceInfo_GetManufacturer();
        device_info_server_params.dis_strings->model_num_string = DeviceInfo_GetModelId();
        device_info_server_params.dis_strings->serial_num_string = DeviceInfo_GetSerialNumber();
        device_info_server_params.dis_strings->hw_revision_string = DeviceInfo_GetHardwareVersion();
        device_info_server_params.dis_strings->fw_revision_string = DeviceInfo_GetFirmwareVersion();
        device_info_server_params.dis_strings->sw_revision_string = DeviceInfo_GetSoftwareVersion();

        initialised = TRUE;
    }
    device_info_server_params.ieee_data = NULL;
    device_info_server_params.pnp_id = NULL;
    device_info_server_params.system_id = NULL;

    return initialised;
}

static void gattServerDeviceInfo_init(void)
{
    DEBUG_LOG_FN_ENTRY("gattServerDeviceInfo_init Server Init");

    gatt_server_device_info.gatt_device_info_task.handler = gattServerDeviceInfo_MessageHandler;

    /* Read the Device Information Service server data to be initialised */
    if(gattServerDeviceInfo_InitialiseDisParams())
    {
        if (!GattDeviceInfoServerInit(GetGattServerDeviceInfoTask(),
                                      GetGattServerDeviceInfoGdis(),
                                      &device_info_server_params,
                                      HANDLE_DEVICE_INFORMATION_SERVICE,
                                      HANDLE_DEVICE_INFORMATION_SERVICE_END))
        {
            // Don't free any of the initialisation parameters in case they were related to the failure
            DEBUG_LOG_ERROR("gattServerDeviceInfo_init Server failed");
            Panic();
        }
        gattServerDeviceInfo_SetupAdvertising();
    }
}

static bool gattDeviceInfoServer_CanAdvertiseService(const le_adv_data_params_t * params)
{
    /* DIS will not worry if LE Advertising Manager(LEAM) can't fit advert data into the
    advertising/scan response space.
    DIS advertising data is skippable (optional data item) meaning if LEAM can ignore it
    if it can not fit into advert space.*/
    return ((params->data_set == le_adv_data_set_handset_identifiable) &&
            (params->placement == le_adv_data_placement_dont_care) &&
            (params->completeness == le_adv_data_completeness_can_be_skipped)
           );
}

static unsigned int gattServerDeviceInfo_NumberOfAdvItems(const le_adv_data_params_t * params)
{
    if(gattDeviceInfoServer_CanAdvertiseService(params))
    {
        return NUMBER_OF_ADVERT_DATA_ITEMS;
    }

    return 0;
}

static le_adv_data_item_t gattServerDeviceInfo_GetAdvDataItems(const le_adv_data_params_t * params, unsigned int id)
{
    UNUSED(params);

    /* Safety check to make sure advert data items 1 */
    COMPILE_TIME_ASSERT(NUMBER_OF_ADVERT_DATA_ITEMS == 1, AdvertDataItemExceeds);

    /* id is an index to the Number of Adv items i.e.gattServerDeviceInfo_NumberOfAdvItems() 
    id is 0 indexed, so if 1 item in AdvItems, LE advertising manager will pass id = 0
    if 2 items in AdvItems, LE advertising manager will pass id = 0 and 1
    i.e this function will be called twice, first called with id=0 and and then id= 1.
    As gattServerDeviceInfo_NumberOfAdvItems() can't be more than 1, expectation that 
    id should 0 and never be 1 or greater than 1.*/
    PanicFalse(id == 0);

    return gatt_device_info_advert;
}

static void gattServerDeviceInfo_ReleaseAdvDataItems(const le_adv_data_params_t * params)
{
    UNUSED(params);

    return;
}

/*****************************************************************************/
bool GattServerDeviceInfo_Init(Task init_task)
{
    UNUSED(init_task);

    gattServerDeviceInfo_init();

    DEBUG_LOG("GattServerDeviceInfo_Init. Device Info Service Server initialised");

    return TRUE;
}

#endif /* INCLUDE_GATT_DEVICE_INFO_SERVER */

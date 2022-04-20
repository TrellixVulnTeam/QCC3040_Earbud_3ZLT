/*!
\copyright  Copyright (c) 2008 - 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       swift_pair.c
\brief      Source file for Swift Pair
*/

#ifdef INCLUDE_SWIFT_PAIR
#include "swift_pair.h"

#define SWIFT_PAIR_ADV_PARAMS_REQUESTED(params) ((params->completeness == le_adv_data_completeness_full) && \
            (params->placement == le_adv_data_placement_advert) && (params->data_set == le_adv_data_set_handset_identifiable))

#define SWIFT_PAIR_ADV_PAYLOAD 0
#define SWIFT_PAIR_ADV_ITEMS 1

#define SWIFT_PAIR_ADV_LENGTH 10
#define SWIFT_PAIR_MICROSOFT_VENDOR_ID 0x0006
#define SWIFT_PAIR_MICROSOFT_BEACON_ID 0x03
#define SWIFT_PAIR_MICROSOFT_SUB_SCENARIO_ID 0X02
#define SWIFT_PAIR_RESERVED_RSSI_BYTE 0x80
/* CoD for an audio sink device is set to 0x200404 (3 bytes) considering the appropriate value for Major Service Class(Bit 21 set),
   Major Device Class(Bit 10 set) and Minor Device Class(Bit 2 set) according to Bluetooth specification.*/
#define SWIFT_PAIR_CLASS_OF_DEVICE 0x200404

static bool IsInPairingMode = FALSE;

static const uint8 sp_payload[SWIFT_PAIR_ADV_LENGTH] =
{
    SWIFT_PAIR_ADV_LENGTH - 1,
    ble_ad_type_manufacturer_specific_data,
    SWIFT_PAIR_MICROSOFT_VENDOR_ID & 0xFF, 
    (SWIFT_PAIR_MICROSOFT_VENDOR_ID >> 8) & 0xFF,
    SWIFT_PAIR_MICROSOFT_BEACON_ID,
    SWIFT_PAIR_MICROSOFT_SUB_SCENARIO_ID,
    SWIFT_PAIR_RESERVED_RSSI_BYTE,
    SWIFT_PAIR_CLASS_OF_DEVICE & 0xFF,
    (SWIFT_PAIR_CLASS_OF_DEVICE >> 8) & 0xFF,
    (SWIFT_PAIR_CLASS_OF_DEVICE >> 16) & 0xFF,
};

static const le_adv_data_item_t sp_advert_payload =
{
    SWIFT_PAIR_ADV_LENGTH,
    sp_payload
};

swiftPairTaskData swift_pair_task_data;

le_adv_mgr_register_handle swift_pair_adv_register_handle;


/*! Static functions for swiftpair advertising data */
static unsigned int swiftPair_AdvGetNumberOfItems(const le_adv_data_params_t * params);
static le_adv_data_item_t swiftPair_AdvGetDataItem(const le_adv_data_params_t * params, unsigned int id);
static void swiftPair_ReleaseItems(const le_adv_data_params_t * params);

/*! Callback registered with LE Advertising Manager*/
static const le_adv_data_callback_t swiftPair_advertising_callback = {
    .GetNumberOfItems = &swiftPair_AdvGetNumberOfItems,
    .GetItem = &swiftPair_AdvGetDataItem,
    .ReleaseItems = &swiftPair_ReleaseItems
};

/*! \brief Provide the number of items expected to go in adverts for a given mode

      Advertising Manager is expected to retrive the number of items first before the swiftPair_AdvGetDataItem() callback

      For swiftpair there wont be any adverts in case of le_adv_data_completeness_can_be_shortened/skipped
*/
static unsigned int swiftPair_AdvGetNumberOfItems(const le_adv_data_params_t * params)
{
    unsigned int number = 0;

    if (params->data_set != le_adv_data_set_peer)
    {
        if (IsInPairingMode && SWIFT_PAIR_ADV_PARAMS_REQUESTED(params))
        {
            number = SWIFT_PAIR_ADV_ITEMS;
        }
        else
        {
            DEBUG_LOG("swiftPair_AdvGetNumberOfItems: Non-connectable \n");
        }
    }

    return number;
}

/*! \brief Provide the advertisement data expected to go in adverts for a given mode

    Each data item in GetItems will be invoked separately by Adv Mgr, more precisely, one item per AD type.
*/
static le_adv_data_item_t swiftPair_AdvGetDataItem(const le_adv_data_params_t * params, unsigned int id)
{
    le_adv_data_item_t data_item = {0};

    if (params->data_set != le_adv_data_set_peer)
    {
        if (IsInPairingMode && SWIFT_PAIR_ADV_PARAMS_REQUESTED(params) && 
            (id == SWIFT_PAIR_ADV_PAYLOAD))
        {
            DEBUG_LOG("swiftPair_AdvGetDataItem: swift pair advert payload advertise ");
            return sp_advert_payload;
        }
        else
        {
            DEBUG_LOG("swiftPair_AdvGetDataItem: Not in pairing mode or Invalid data_set_identifier %d \n", id);
        }
    }

    return data_item;
}

/*! \brief Release any allocated swiftpair data

      Advertising Manager is expected to retrive the number of items first before the swiftPair_AdvGetDataItem() callback
*/
static void swiftPair_ReleaseItems(const le_adv_data_params_t * params)
{
    UNUSED(params);

    return;
}

/*! \brief Check if the device is in handset discoverable mode so that swift pair payload can be advertised
*/
void swiftPair_SetIdentifiable(const le_adv_data_set_t data_set)
{
    swiftPairTaskData *theSwiftPair = swiftPair_GetTaskData();

    IsInPairingMode = (data_set == le_adv_data_set_handset_identifiable)?(TRUE):(FALSE);
    DEBUG_LOG("swiftPair_SetIdentifiable %d", IsInPairingMode);

    if (swift_pair_adv_register_handle)
    {
        (void)LeAdvertisingManager_NotifyDataChange(&theSwiftPair->task, swift_pair_adv_register_handle);
    }
    else
    {
        DEBUG_LOG("swiftPair_SetIdentifiable: Invalid Handle ");
    }
}

/*! \brief Handle the Pairing activity messages from pairing module */
void swiftPair_PairingActivity(PAIRING_ACTIVITY_T *message)
{
    switch(message->status)
    {
        case pairingActivityInProgress:
        {
            DEBUG_LOG("swiftPair_PairingActivity: pairingActivityInProgress");
            swiftPair_SetIdentifiable(le_adv_data_set_handset_identifiable);
        }
        break;

        case pairingActivityNotInProgress:
        {
            DEBUG_LOG("swiftPair_PairingActivity: pairingActivityNotInProgress");
            swiftPair_SetIdentifiable(le_adv_data_set_handset_unidentifiable);
        }
        break;

        default:
            DEBUG_LOG("swiftPair_PairingActivity: Invalid message id ");
        break;
    }

}
/*! \brief Message Handler

    This function is the main message handler for the swift pair module.
*/
void swiftPair_HandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch (id)
    {
        case PAIRING_ACTIVITY:
            swiftPair_PairingActivity((PAIRING_ACTIVITY_T*)message);
        break;

        default:
        break;
    }
}

/*! Get pointer to Swift Pair data structure */
swiftPairTaskData* swiftPair_GetTaskData(void)
{
    return (&swift_pair_task_data);
}

/*! Register swift pair advertising callback with LE Advertising Manager */
void swiftPair_SetUpAdvertising(void)
{
    swiftPairTaskData *theSwiftPair = swiftPair_GetTaskData();

    swift_pair_adv_register_handle = LeAdvertisingManager_Register(&theSwiftPair->task, &swiftPair_advertising_callback);
}

/*! Initialization of swift pair module */
bool SwiftPair_Init(Task init_task)
{
    swiftPairTaskData *theSwiftPair = swiftPair_GetTaskData();

    UNUSED(init_task);

    DEBUG_LOG("SwiftPair_Init");

    memset(theSwiftPair, 0, sizeof(*theSwiftPair));

    /* Set up task handler */
    theSwiftPair->task.handler = swiftPair_HandleMessage;

    /* Initialize the Swift Pair Advertising Interface */
    swiftPair_SetUpAdvertising();

    /* Register with pairing module to know when device is Br/Edr discoverable */
    Pairing_ActivityClientRegister(&(theSwiftPair->task));

     return TRUE;
}
#endif /* INCLUDE_SWIFT_PAIR */
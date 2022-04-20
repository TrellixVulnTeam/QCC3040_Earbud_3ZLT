/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Manage the handset extended advertising set.
*/

#if defined(INCLUDE_ADVERTISING_EXTENSIONS)

#include "handset_service_extended_advertising.h"
#include "handset_service_protected.h"

#include "le_advertising_manager.h"

#include <logging.h>
#include <panic.h>


#define handsetServiceExtAdvGetTaskData() (&hs_ext_adv_data)
#define handsetServiceExtAdvGetTask()     (&hs_ext_adv_data.task)


typedef enum
{
    EXT_ADVERT_STATE_NOT_SELECTED = 0,
    EXT_ADVERT_STATE_SELECTED,
    EXT_ADVERT_STATE_SELECTING,
    EXT_ADVERT_STATE_RELEASING,
} handset_service_ext_advert_state_t;

typedef struct
{
    TaskData task;
    le_adv_data_set_handle le_advert_handle;
    handset_service_ext_advert_state_t le_advert_state;
} handset_service_ext_adv_task_data_t;

handset_service_ext_adv_task_data_t hs_ext_adv_data;


static void handsetServiceExtAdv_SetBleAdvertState(handset_service_ext_advert_state_t state);
static handset_service_ext_advert_state_t handsetServiceExtAdv_GetBleAdvertState(void);
static void handsetServiceExtAdv_SetBleAdvertHandle(le_adv_data_set_handle handle);
static le_adv_data_set_handle handsetServiceExtAdv_GetBleAdvertHandle(void);
static void handsetServiceExtAdv_EnableAdvertising(void);
static void handsetServiceExtAdv_DisableAdvertising(void);
static void handsetServiceExtAdv_HandleLamSelectDatasetCfm(const LE_ADV_MGR_SELECT_DATASET_CFM_T *cfm);
static void handsetServiceExtAdv_HandleLamReleaseDatasetCfm(const LE_ADV_MGR_RELEASE_DATASET_CFM_T *cfm);
static void handsetServiceExtAdv_HandleMessage(Task task, MessageId id, Message message);


static void handsetServiceExtAdv_SetBleAdvertState(handset_service_ext_advert_state_t state)
{
    hs_ext_adv_data.le_advert_state = state;
    UNUSED(HandsetServiceExtAdv_UpdateAdvertisingData());
}

static handset_service_ext_advert_state_t handsetServiceExtAdv_GetBleAdvertState(void)
{
    return hs_ext_adv_data.le_advert_state;
}

static void handsetServiceExtAdv_SetBleAdvertHandle(le_adv_data_set_handle handle)
{
    hs_ext_adv_data.le_advert_handle = handle;
}

static le_adv_data_set_handle handsetServiceExtAdv_GetBleAdvertHandle(void)
{
    return hs_ext_adv_data.le_advert_handle;
}

static void handsetServiceExtAdv_EnableAdvertising(void)
{
    le_adv_select_params_t adv_select_params;
    le_adv_data_set_handle adv_handle = NULL;

    DEBUG_LOG("handsetServiceExtAdv_EnableAdvertising, Le Advert State is %d", handsetServiceExtAdv_GetBleAdvertState());

    adv_select_params.set = le_adv_data_set_extended_handset;

    adv_handle = LeAdvertisingManager_SelectAdvertisingDataSet(handsetServiceExtAdvGetTask(), &adv_select_params);

    handsetServiceExtAdv_SetBleAdvertState(EXT_ADVERT_STATE_SELECTING);

    if (adv_handle != NULL)
    {
        handsetServiceExtAdv_SetBleAdvertHandle(adv_handle);

        DEBUG_LOG("handsetServiceExtAdv_EnableAdvertising. Selected set with handle=%p", handsetServiceExtAdv_GetBleAdvertHandle());
    }
}

static void handsetServiceExtAdv_DisableAdvertising(void)
{
    DEBUG_LOG("handsetServiceExtAdv_DisableAdvertising, release set with handle=%p", handsetServiceExtAdv_GetBleAdvertHandle());

    PanicFalse(LeAdvertisingManager_ReleaseAdvertisingDataSet(handsetServiceExtAdv_GetBleAdvertHandle()));

    handsetServiceExtAdv_SetBleAdvertHandle(NULL);
    handsetServiceExtAdv_SetBleAdvertState(EXT_ADVERT_STATE_RELEASING);

    DEBUG_LOG("handsetServiceExtAdv_DisableAdvertising, handle=%p", handsetServiceExtAdv_GetBleAdvertHandle());
}

static void  handsetServiceExtAdv_HandleLamSelectDatasetCfm(const LE_ADV_MGR_SELECT_DATASET_CFM_T *cfm)
{
    DEBUG_LOG("handsetServiceExtAdv_HandleLamSelectDatasetCfm, cfm status is %d", cfm->status);

    PanicFalse(cfm->status == le_adv_mgr_status_success);

    handsetServiceExtAdv_SetBleAdvertState(EXT_ADVERT_STATE_SELECTED);
}

static void handsetServiceExtAdv_HandleLamReleaseDatasetCfm(const LE_ADV_MGR_RELEASE_DATASET_CFM_T *cfm)
{
    DEBUG_LOG("handsetServiceExtAdv_HandleLamReleaseDatasetCfm, cfm status is %d", cfm->status);

    PanicFalse(cfm->status == le_adv_mgr_status_success);

    handsetServiceExtAdv_SetBleAdvertState(EXT_ADVERT_STATE_NOT_SELECTED);
}

static void handsetServiceExtAdv_HandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch (id)
    {
    /* LE Advertising messages */
    case LE_ADV_MGR_SELECT_DATASET_CFM:
        {
            handsetServiceExtAdv_HandleLamSelectDatasetCfm((const LE_ADV_MGR_SELECT_DATASET_CFM_T *)message);
        }
        break;
    case LE_ADV_MGR_RELEASE_DATASET_CFM:
        {
            handsetServiceExtAdv_HandleLamReleaseDatasetCfm((const LE_ADV_MGR_RELEASE_DATASET_CFM_T *)message);
        }
        break;
    default:
        {
            DEBUG_LOG("handsetServiceExtAdv_HandleMessage Unhandled %d", id);
            Panic();
        }
        break;
    }
}

void HandsetServiceExtAdv_Init(void)
{
    handset_service_ext_adv_task_data_t* handset = handsetServiceExtAdvGetTaskData();

    memset(handset, 0, sizeof(*handset));
    handset->task.handler = handsetServiceExtAdv_HandleMessage;
}

bool HandsetServiceExtAdv_UpdateAdvertisingData(void)
{
   handset_service_ext_advert_state_t le_advert_state = handsetServiceExtAdv_GetBleAdvertState();

    if ((le_advert_state == EXT_ADVERT_STATE_SELECTING) || (le_advert_state == EXT_ADVERT_STATE_RELEASING))
    {
        HS_LOG("HandsetServiceExtAdv_UpdateAdvertisingData. Le advertising data set select/release state is enum:handset_service_ext_advert_state_t:%d", 
                    le_advert_state);
        return TRUE;
    }

    unsigned le_connections = HandsetServiceSm_GetLeAclConnectionCount();
    bool have_spare_le_connections = le_connections < handsetService_LeAclMaxConnections();
    bool is_le_connectable = HandsetService_IsBleConnectable();
    bool pairing_possible = HandsetServiceSm_CouldDevicesPair();

    bool enable_advertising = is_le_connectable && have_spare_le_connections && !pairing_possible;

    HS_LOG("HandsetServiceExtAdv_UpdateAdvertisingData. State enum:handset_service_ext_advert_state_t:%d. Le Connectable Status is %x. Spare LE conns:%d.", 
            le_advert_state, is_le_connectable, have_spare_le_connections);

    if (handsetServiceExtAdv_GetBleAdvertHandle())
    {
        HS_LOG("HandsetServiceExtAdv_UpdateAdvertisingData. There is an active data set with handle=%p. Disable:%d", 
                  handsetServiceExtAdv_GetBleAdvertHandle(), !enable_advertising);

        if (!enable_advertising)
        {
            handsetServiceExtAdv_DisableAdvertising();
            return TRUE;
        }
    }
    else
    {
        HS_LOG("HandsetServiceExtAdv_UpdateAdvertisingData. There is no active data set. Enable:%d",
                  enable_advertising);

        if (enable_advertising)
        {
            handsetServiceExtAdv_EnableAdvertising();
            return TRUE;
        }
    }

    return FALSE;
}

#endif /* defined(INCLUDE_ADVERTISING_EXTENSIONS) */

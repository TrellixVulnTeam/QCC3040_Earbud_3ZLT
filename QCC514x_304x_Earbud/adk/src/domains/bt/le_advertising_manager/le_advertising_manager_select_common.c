/*!
\copyright  Copyright (c) 2018 - 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Management of Bluetooth Low Energy legacy specific advertising
*/


#include "le_advertising_manager_select_common.h"

#include "le_advertising_manager_clients.h"
#include "le_advertising_manager_private.h"
#include "le_advertising_manager_sm.h"
#include "le_advertising_manager_utils.h"

#include <panic.h>

static le_advert_start_params_t start_params;


/* Local function to check if select data set confirmation message is already scheduled and put in the message queue for the given data set */
static bool leAdvertisingManager_IsSelectDataSetCfmMessageScheduled(le_adv_data_set_t set)
{
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_IsSelectDataSetCfmMessageScheduled");
    
    return MessageCancelFirst(leAdvertisingManager_GetTaskForDataSet(set), LE_ADV_MGR_SELECT_DATASET_CFM);
    
}

/* Function to check if select data set confirmation message is pending but not yet put in the message queue for the given data set */
static bool leAdvertisingManager_IsSelectDataSetCfmMessagePending(le_adv_data_set_t set)
{
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_IsSelectDataSetCfmMessagePending");
    
    return start_params.set_awaiting_select_cfm_msg & set;
    
}

/*Local function to decide whether there is a need to send select data set confirmation message when scheduling internal start message  */
static bool leAdvertisingManager_IsSelectDataSetConfirmationToBeSent(le_adv_data_set_t set)
{
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_IsSelectDataSetConfirmationToBeSent");

    bool is_select_cfm_message_scheduled = leAdvertisingManager_IsSelectDataSetCfmMessageScheduled(set);
    bool is_select_cfm_message_pending =  leAdvertisingManager_IsSelectDataSetCfmMessagePending(set);

    return is_select_cfm_message_scheduled | is_select_cfm_message_pending;
    
}


bool leAdvertisingManager_IsLegacySet(const le_adv_data_set_t set)
{
    bool is_legacy = FALSE;
    
    if (set & LE_ADV_MGR_ADVERTISING_SET_LEGACY)
    {
        is_legacy = TRUE;
    }
    
    return is_legacy;
}

le_adv_data_set_t leAdvertisingManager_SelectOnlyLegacySet(le_adv_data_set_t set)
{
    return (set & LE_ADV_MGR_ADVERTISING_SET_LEGACY);
}

bool leAdvertisingManager_IsExtendedSet(const le_adv_data_set_t set)
{
    bool is_extended = FALSE;
    
    if (set & LE_ADV_MGR_ADVERTISING_SET_EXTENDED)
    {
        is_extended = TRUE;
    }
    
    return is_extended;
}

le_adv_data_set_t leAdvertisingManager_SelectOnlyExtendedSet(le_adv_data_set_t set)
{
    return (set & LE_ADV_MGR_ADVERTISING_SET_EXTENDED);
}

void leAdvertisingManager_SetParamsUpdateFlag(bool params_update)
{
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();

    adv_task_data->is_params_update_required = params_update;

}

void leAdvertisingManager_SetDataUpdateRequired(const le_adv_data_set_t set, bool data_update)
{
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
    
    if (leAdvertisingManager_IsLegacySet(set))
    {
        adv_task_data->is_legacy_data_update_required = data_update;
    }
    if (leAdvertisingManager_IsExtendedSet(set))
    {
        adv_task_data->is_extended_data_update_required = data_update;
    }
}

void LeAdvertisingManager_ScheduleAdvertisingStart(const le_adv_data_set_t set)
{
    Task message_task = NULL;
    
    DEBUG_LOG_LEVEL_1("LeAdvertisingManager_ScheduleAdvertisingStart");
    
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
    
    MAKE_MESSAGE(LE_ADV_MGR_INTERNAL_START);
    message->set = set;
    
    if (leAdvertisingManager_IsLegacySet(set))
    {
        message_task = AdvManagerGetLegacyTask();
    }
    else
    {
        message_task = AdvManagerGetExtendedTask();
    }
    
    MessageSendConditionally(message_task, LE_ADV_MGR_INTERNAL_START, message,  &adv_task_data->blockingCondition );
}


void LeAdvertisingManager_SendSelectConfirmMessage(void)
{
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
    
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_SendSelectConfirmMessage");
    
    if(start_params.set_awaiting_select_cfm_msg)
    {
        for(le_adv_data_set_t data_set_msg_index = le_adv_data_set_handset_identifiable; data_set_msg_index <=le_adv_data_set_extended_handset; data_set_msg_index <<= 1 )
        {
            if(!(start_params.set_awaiting_select_cfm_msg & data_set_msg_index))
                continue;                
            
            leAdvertisingManager_SetDataSetSelectMessageStatusBitmask(data_set_msg_index, FALSE);
            
            MAKE_MESSAGE(LE_ADV_MGR_SELECT_DATASET_CFM);
            message->status = le_adv_mgr_status_success;
            MessageSendConditionally(leAdvertisingManager_GetTaskForDataSet(data_set_msg_index), LE_ADV_MGR_SELECT_DATASET_CFM, message,  &adv_task_data->blockingCondition );
        }
    }        
}

bool LeAdvertisingManager_CanAdvertisingBeStarted(void)
{    
    bool can_be_started = TRUE;
    
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_Start");
    
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
            
    if(FALSE == LeAdvertisingManagerSm_IsInitialised())
    {
        DEBUG_LOG_LEVEL_1("leAdvertisingManager_Start Failure, State Machine is not Initialised");
        
        can_be_started = FALSE;
    }
    
    if(TRUE == LeAdvertisingManagerSm_IsAdvertisingStarting())
    {
        DEBUG_LOG_LEVEL_1("leAdvertisingManager_Start Failure, Advertising is already in a process of starting");
        
        can_be_started =  FALSE;
    }
    
    if( FALSE == leAdvertisingManager_IsAdvertisingAllowed())
    {
        
        DEBUG_LOG_LEVEL_1("leAdvertisingManager_Start Failure, Advertising is currently not allowed");
        
        can_be_started =  FALSE;
    }
    else
    {        
                
        if(0UL == (adv_task_data->mask_enabled_events & leAdvertisingManager_GetDataSetEventType()))
        {
            DEBUG_LOG_LEVEL_1("leAdvertisingManager_Start Failure, Advertising for the requested advertising event is currently not enabled");
            
            can_be_started =  FALSE;
        }
        
    }

    if(leAdvertisingManager_ClientListIsEmpty())
    {
        DEBUG_LOG_LEVEL_1("leAdvertisingManager_Start Failure, Database is empty");
        can_be_started =  FALSE;
    }

    return can_be_started;
}

/* Function to Clear Data Set Select Message Status Bitmask */
void leAdvertisingManager_ClearDataSetMessageStatusBitmask(void)
{
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_ClearMessageStatusBitmask");
    
    start_params.set_awaiting_select_cfm_msg = 0;    
}

/* Function to Clear Data Set Select Status Bitmask */
void leAdvertisingManager_ClearDataSetSelectBitmask(void)
{
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_ClearDataSetBitmask");
    
    start_params.set = 0;    
}

/* Function to Set Data Set Select Message Status */
void leAdvertisingManager_SetDataSetSelectMessageStatusBitmask(le_adv_data_set_t set, bool enable)
{
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_SetDataSetSelectMessageStatusBitmask");
    
    if(enable)
    {
        DEBUG_LOG_LEVEL_2("leAdvertisingManager_SetDataSetSelectMessageStatusBitmask Info, Enable bitmask, Message status is %x", set);
        
        start_params.set_awaiting_select_cfm_msg |= set;
        
    }
    else
    {
        DEBUG_LOG_LEVEL_2("leAdvertisingManager_SetDataSetSelectMessageStatusBitmask Info, Disable bitmask, Message status is %x", set);
        
        start_params.set_awaiting_select_cfm_msg &= ~set;
        
    }    
    
    DEBUG_LOG_LEVEL_2("leAdvertisingManager_SetDataSetSelectMessageStatusBitmask Info, Start Params Message Status is %x", start_params.set_awaiting_select_cfm_msg);
}

/* Function to Set Data Set Select Message Status */
void leAdvertisingManager_SetDataSetSelectMessageStatusAfterRelease(le_adv_data_set_t set)
{
    le_adv_data_set_t selected_set;
    
    if (leAdvertisingManager_IsLegacySet(set))
    {
        selected_set = leAdvertisingManager_SelectOnlyLegacySet(set);
        leAdvertisingManager_SetDataSetSelectMessageStatusBitmask(selected_set, leAdvertisingManager_IsSelectDataSetConfirmationToBeSent(selected_set));
    }
    if (leAdvertisingManager_IsExtendedSet(set))
    {
        selected_set = leAdvertisingManager_SelectOnlyExtendedSet(set);
        leAdvertisingManager_SetDataSetSelectMessageStatusBitmask(selected_set, leAdvertisingManager_IsSelectDataSetConfirmationToBeSent(selected_set));
    }
}

/* Function to Set Local Data Set Bitmask */
void leAdvertisingManager_SetDataSetSelectBitmask(le_adv_data_set_t set, bool enable)
{
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_SetDataSetSelectBitmask");
    
    if(enable)
    {
        DEBUG_LOG_LEVEL_2("leAdvertisingManager_SetDataSetSelectBitmask Info, Enable bitmask, Data set is %x", set);
        
        start_params.set |= set;
        
    }
    else
    {
        DEBUG_LOG_LEVEL_2("leAdvertisingManager_SetDataSetSelectBitmask Info, Disable bitmask, Data set is %x", set);
        
        start_params.set &= ~set;
        
    }    
    
    DEBUG_LOG_LEVEL_2("leAdvertisingManager_SetDataSetSelectBitmask Info, Start Params Data Set is %x", start_params.set);
}


/* Function to Check if one of the supported data sets is already selected */
bool leAdvertisingManager_IsDataSetSelected(void)
{
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_IsDataSetSelected");
        
    if(start_params.set)
    {
        DEBUG_LOG_LEVEL_2("leAdvertisingManager_IsDataSetSelected Info, Selected data set is %x", start_params.set);
        return TRUE;
    }
    
    return FALSE;
}

le_adv_data_set_t leAdvertisingManager_GetDataSetSelected(void)
{
    return start_params.set;
}

void leAdvertisingManager_SetDataSetEventType(le_adv_event_type_t event_type)
{
    start_params.event = event_type;
}

le_adv_event_type_t leAdvertisingManager_GetDataSetEventType(void)
{
    return start_params.event;
}

/* Function to Set Up Advertising Event Type */
ble_adv_type leAdvertisingManager_GetAdvertType(le_adv_event_type_t event)
{
    switch(event)
    {
        case le_adv_event_type_connectable_general:
            return ble_adv_ind;
            
        case le_adv_event_type_connectable_directed:
            return ble_adv_direct_ind;
            
        case le_adv_event_type_nonconnectable_discoverable:
            return ble_adv_scan_ind;
            
        case le_adv_event_type_nonconnectable_nondiscoverable:
            return ble_adv_nonconn_ind;
       
        default:
            Panic();
            return ble_adv_nonconn_ind;
    }
}

/* Populate the interval pair min/max input with the default advertising interval values */
void leAdvertisingManager_GetDefaultAdvertisingIntervalParams(le_adv_common_parameters_t * param)
{
    param->le_adv_interval_min = DEFAULT_ADVERTISING_INTERVAL_MIN_IN_SLOTS;
    param->le_adv_interval_max = DEFAULT_ADVERTISING_INTERVAL_MAX_IN_SLOTS;
    
}

/* Function to Set Advertising Interval */
ble_adv_params_t leAdvertisingManager_GetAdvertisingIntervalParams(void)
{
    ble_adv_params_t params;
    adv_mgr_task_data_t * adv_task_data = AdvManagerGetTaskData();
    le_adv_params_set_handle handle = adv_task_data->params_handle;
    
    memset(&params, 0, sizeof(ble_adv_params_t));
    
    if(NULL == handle)
    {
        le_adv_common_parameters_t interval_pair;
        leAdvertisingManager_GetDefaultAdvertisingIntervalParams(&interval_pair);
        params.undirect_adv.adv_interval_min = interval_pair.le_adv_interval_min;
        params.undirect_adv.adv_interval_max = interval_pair.le_adv_interval_max;
    }
    else
    {
        params.undirect_adv.adv_interval_min = handle->params_set->set_type[handle->active_params_set].le_adv_interval_min;
        params.undirect_adv.adv_interval_max = handle->params_set->set_type[handle->active_params_set].le_adv_interval_max;
    }
    
    params.undirect_adv.filter_policy = ble_filter_none;
    
    return params;
}

/*!
\copyright  Copyright (c) 2018 - 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Management of Bluetooth Low Energy legacy specific advertising
*/


#include "le_advertising_manager_select_legacy.h"

#include "le_advertising_manager_data_common.h"
#include "le_advertising_manager_data_legacy.h"
#include "le_advertising_manager_private.h"
#include "le_advertising_manager_select_common.h"
#include "le_advertising_manager_sm.h"
#include "le_advertising_manager_utils.h"

#include "local_addr.h"

#include <panic.h>


static bool leAdvertisingManager_GetLegacyDataUpdateRequired(void)
{
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
    
    return adv_task_data->is_legacy_data_update_required;
}


static void leAdvertisingManager_SetAdvertisingParamsReq(void)
{
    ble_adv_type type = leAdvertisingManager_GetAdvertType(leAdvertisingManager_GetDataSetEventType());
    
    ble_adv_params_t interval_params = leAdvertisingManager_GetAdvertisingIntervalParams();

    ConnectionDmBleSetAdvertisingParamsReq(type, LocalAddr_GetBleType(), BLE_ADV_CHANNEL_ALL, &interval_params);
}

/* Local function to update advertising and scan response data */
static bool leAdvertisingManager_UpdateData(void)
{
    leAdvertisingManager_SetDataUpdateRequired(LE_ADV_MGR_ADVERTISING_SET_LEGACY, FALSE);

    if(leAdvertisingManager_BuildData(leAdvertisingManager_SelectOnlyLegacySet((leAdvertisingManager_GetDataSetSelected()))))
    {
        leAdvertisingManager_SetupAdvertData(leAdvertisingManager_SelectOnlyLegacySet((leAdvertisingManager_GetDataSetSelected())));
        leAdvertisingManager_SetBlockingCondition(le_adv_blocking_condition_data_cfm);
        return TRUE;
    }
    else
    {
        DEBUG_LOG_LEVEL_2("leAdvertisingManager_Start Info, There is no data to advertise");
        leAdvertisingManager_ClearData(leAdvertisingManager_SelectOnlyLegacySet((leAdvertisingManager_GetDataSetSelected())));
        return FALSE;
    }

}

/* Local function to update advertising parameters in use */
void leAdvertisingManager_ScheduleInternalDataUpdate(void)
{
    DEBUG_LOG("leAdvertisingManager_ScheduleInternalDataUpdate");

    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();

    DEBUG_LOG("leAdvertisingManager_ScheduleInternalDataUpdate Info, Send Message LE_ADV_MGR_INTERNAL_DATA_UPDATE on blocking condition %d", adv_task_data->blockingCondition);

    MessageSendConditionally(AdvManagerGetTask(), LE_ADV_MGR_INTERNAL_DATA_UPDATE, NULL, &adv_task_data->blockingCondition );
}

/* Local function to handle internal data update request */
void leAdvertisingManager_HandleInternalDataUpdateRequest(void)
{
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleInternalDataUpdateRequest");
    
    if(LeAdvertisingManagerSm_IsAdvertisingStarted())
    {
        DEBUG_LOG_LEVEL_2("leAdvertisingManager_HandleInternalDataUpdateRequest Info, Advertising in progress, action data update");

        leAdvertisingManager_SetParamsUpdateFlag(FALSE);

        leAdvertisingManager_UpdateData();
    }
    else if(LeAdvertisingManagerSm_IsAdvertisingStarting())
    {
        DEBUG_LOG_LEVEL_2("leAdvertisingManager_HandleInternalDataUpdateRequest Info, Advertising not started, reschedule data update");

        leAdvertisingManager_ScheduleInternalDataUpdate();
    }
}

/* Local Function to Start Advertising */
static bool leAdvertisingManager_Start(void)
{    
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_Start");
            
    if (!LeAdvertisingManager_CanAdvertisingBeStarted())
    {
        DEBUG_LOG_LEVEL_1("leAdvertisingManager_Start Failure");
        return FALSE;
    }
        
    if(leAdvertisingManager_GetLegacyDataUpdateRequired())
    {
        DEBUG_LOG_LEVEL_2("leAdvertisingManager_Start Info, Data update is needed");        
        
        leAdvertisingManager_SetParamsUpdateFlag(TRUE);
        
        if(!leAdvertisingManager_UpdateData())
        {
        	DEBUG_LOG_LEVEL_1("leAdvertisingManager_Start Failure, Data update failed");

            return FALSE;
        }
    }
    else
    {
        DEBUG_LOG_LEVEL_2("leAdvertisingManager_Start Info, Data update is not needed, advertising parameters need to be configured");
        leAdvertisingManager_SetupAdvertParams();
    }
    
    LeAdvertisingManagerSm_SetState(le_adv_mgr_state_starting);
    return TRUE;
}

/* Local Function to Handle Internal Advertising Start Request */
static void leAdvertisingManager_HandleLegacyInternalStartRequest(const LE_ADV_MGR_INTERNAL_START_T * msg)
{
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleLegacyInternalStartRequest");
    
    if(LeAdvertisingManagerSm_IsAdvertisingStarted())
    {
        DEBUG_LOG_LEVEL_2("leAdvertisingManager_HandleLegacyInternalStartRequest Info, Advertising already started, suspending and rescheduling");
        
        LeAdvertisingManagerSm_SetState(le_adv_mgr_state_suspending);
        leAdvertisingManager_SetBlockingCondition(le_adv_blocking_condition_enable_cfm);

        leAdvertisingManager_SetDataUpdateRequired(LE_ADV_MGR_ADVERTISING_SET_LEGACY, TRUE);
        
        LeAdvertisingManager_ScheduleAdvertisingStart(msg->set);
        return;
    }
    
    LeAdvertisingManagerSm_SetState(le_adv_mgr_state_initialised);
        
    leAdvertisingManager_Start();
    
    LeAdvertisingManager_SendSelectConfirmMessage();
}


static void leAdvertisingManager_handleLegacyAdvertMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch (id)
    {
        case LE_ADV_MGR_INTERNAL_START:
            DEBUG_LOG_LEVEL_1("LE_ADV_MGR_INTERNAL_START. Legacy");
            leAdvertisingManager_HandleLegacyInternalStartRequest((const LE_ADV_MGR_INTERNAL_START_T *)message);
            break;
            
        default:
            break;
    }
    
}

void leAdvertisingManager_SelectLegacyAdvertisingInit(void)
{
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
    
    adv_task_data->legacy_task.handler = leAdvertisingManager_handleLegacyAdvertMessage;
    
    leAdvertisingManager_RegisterLegacyDataIf();
}

/* Function to handle CL_DM_BLE_SET_ADVERTISING_DATA_CFM message */
void leAdvertisingManager_HandleSetLegacyAdvertisingDataCfm(uint16 status)
{
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleSetLegacyAdvertisingDataCfm");
    
    if(leAdvertisingManager_CheckBlockingCondition(le_adv_blocking_condition_data_cfm))
    {
        if (success == status)
        {
            DEBUG_LOG_LEVEL_2("leAdvertisingManager_HandleSetLegacyAdvertisingDataCfm Info, CL_DM_BLE_SET_ADVERTISING_DATA_CFM received with success");
            
            le_adv_data_set_t set;
            set = leAdvertisingManager_SelectOnlyLegacySet((leAdvertisingManager_GetDataSetSelected()));
            if(set)
            {
                leAdvertisingManager_SetupScanResponseData(set);
                leAdvertisingManager_ClearData(set);
                leAdvertisingManager_SetBlockingCondition(le_adv_blocking_condition_scan_response_cfm);

            }
            else
            {
                leAdvertisingManager_SetBlockingCondition(le_adv_blocking_condition_none);
            }
        }
        else
        {
            DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleSetLegacyAdvertisingDataCfm Failure, CL_DM_BLE_SET_ADVERTISING_DATA_CFM received with failure");
            Panic();
        }
    }
    else
    {
        DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleSetLegacyAdvertisingDataCfm Failure, Message Received in Unexpected Blocking Condition %x", leAdvertisingManager_GetBlockingCondition());
        Panic();
    }
}

static bool leAdvertisingManager_IsParamsUpdateRequired(void)
{
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();

    return  adv_task_data->is_params_update_required;
}

/* Function to handle CL_DM_BLE_SET_SCAN_RESPONSE_DATA_CFM message */
void leAdvertisingManager_HandleLegacySetScanResponseDataCfm(uint16 status)
{    
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleLegacySetScanResponseDataCfm");
    
    if (leAdvertisingManager_CheckBlockingCondition(le_adv_blocking_condition_scan_response_cfm))
    {
        DEBUG_LOG_LEVEL_2("leAdvertisingManager_HandleLegacySetScanResponseDataCfm Info, adv_task_data->blockingCondition is %x status is %x", leAdvertisingManager_GetBlockingCondition(), status);

        if (success == status)
        {            
            if(leAdvertisingManager_IsParamsUpdateRequired())
            {
                DEBUG_LOG_LEVEL_2("leAdvertisingManager_HandleSetScanResponseDataCfm Info, Parameters update required");

                leAdvertisingManager_SetupAdvertParams();
            }
            else
            {
                DEBUG_LOG_LEVEL_2("leAdvertisingManager_HandleSetScanResponseDataCfm Info, Parameters update not required");

                leAdvertisingManager_SetBlockingCondition(le_adv_blocking_condition_none);
            }
        }
        else
        {
            DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleLegacySetScanResponseDataCfm Failure, CL_DM_BLE_SET_SCAN_RESPONSE_DATA_CFM received with failure");
            Panic();
        }
    }
    else
    {
        DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleLegacySetScanResponseDataCfm Failure, Message Received in Unexpected Blocking Condition %x", leAdvertisingManager_GetBlockingCondition());
        Panic();
    }
}

/* Function to handle CL_DM_BLE_SET_ADVERTISING_PARAMS_CFM message */
void leAdvertisingManager_HandleLegacySetAdvertisingParamCfm(uint16 status)
{
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleLegacySetAdvertisingParamCfm");
    
    if (leAdvertisingManager_CheckBlockingCondition(le_adv_blocking_condition_params_cfm))
    {            
        DEBUG_LOG_LEVEL_2("leAdvertisingManager_HandleLegacySetAdvertisingParamCfm Info, adv_task_data->blockingCondition is %x status is %x", leAdvertisingManager_GetBlockingCondition(), status);
            
        if (success == status)
        {            
            DEBUG_LOG_LEVEL_2("leAdvertisingManager_HandleLegacySetAdvertisingParamCfm Info, CL_DM_BLE_SET_ADVERTISING_PARAMS_CFM received with success");    
            
            if(LeAdvertisingManagerSm_IsAdvertisingStarting())
            {
                LeAdvertisingManagerSm_SetState(le_adv_mgr_state_starting);
                leAdvertisingManager_SetBlockingCondition(le_adv_blocking_condition_enable_cfm);
            }
            else
            {
                if(LeAdvertisingManagerSm_IsAdvertisingStarted() )
                {
                        leAdvertisingManager_SendMessageParameterSwitchover();
                }

                leAdvertisingManager_SetBlockingCondition(le_adv_blocking_condition_none);
            }
        }
        else
        {            
            DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleLegacySetAdvertisingParamCfm Failure, CL_DM_BLE_SET_ADVERTISING_PARAMS_CFM received with failure");

            Panic();
        }
    }
    else
    {
        if (   LeAdvertisingManagerSm_IsAdvertisingStarted()
            || LeAdvertisingManagerSm_IsAdvertisingStarting())
        {
            DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleLegacySetAdvertisingParamCfm Failure, Message Received in Unexpected Blocking Condition %x", leAdvertisingManager_GetBlockingCondition());

            Panic();
        }
        else
        {
            DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleLegacySetAdvertisingParamCfm Ignoring");
        }
    }
}

/* Function to Set Up Advertising Parameters */
void leAdvertisingManager_SetupAdvertParams(void)
{
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_SetupAdvertParams");

    leAdvertisingManager_SetBlockingCondition(le_adv_blocking_condition_params_cfm);

    DEBUG_LOG_LEVEL_2("leAdvertisingManager_SetupAdvertParams Info, Request advertising parameters set, blocking condition is %x", leAdvertisingManager_GetBlockingCondition());
    leAdvertisingManager_SetAdvertisingParamsReq();
}

/* Function to send LE_ADV_MGR_INTERNAL_MSG_NOTIFY_INTERVAL_SWITCHOVER message */
void leAdvertisingManager_SendMessageParameterSwitchover(void)
{
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_SendMessageParameterSwitchover");

    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();

    if((adv_task_data->params_handle) && (adv_task_data->params_handle->config_table))
    {
        uint16 index = adv_task_data->params_handle->index_active_config_table_entry;
        uint16 timeout = adv_task_data->params_handle->config_table->row[index].timeout_fallback_in_seconds;

        DEBUG_LOG_LEVEL_1("leAdvertisingManager_SendMessageParameterSwitchover, Selected Config Table Index is %d, Timeout is %d seconds", index, timeout );

        /* If timeout defined for the existing entry in the config table is zero, fall back to the next entry should not happen and LE advetising manager should not trigger switchover */
        if(timeout)
        {
            DEBUG_LOG_LEVEL_1("leAdvertisingManager_SendMessageParameterSwitchover, Fallback Timeout is %d seconds", timeout);

            MessageCancelFirst(AdvManagerGetTask(), LE_ADV_MGR_INTERNAL_MSG_NOTIFY_INTERVAL_SWITCHOVER);
            MessageSendLater(AdvManagerGetTask(), LE_ADV_MGR_INTERNAL_MSG_NOTIFY_INTERVAL_SWITCHOVER, NULL, D_SEC(timeout));
        }
    }
}

/* Function to cancel LE_ADV_MGR_INTERNAL_MSG_NOTIFY_INTERVAL_SWITCHOVER message */
void leAdvertisingManager_CancelMessageParameterSwitchover(void)
{
    MessageCancelAll(AdvManagerGetTask(), LE_ADV_MGR_INTERNAL_MSG_NOTIFY_INTERVAL_SWITCHOVER);

}

/* Function to Handle Internal Advetising Interval Switchover Message */
void leAdvertisingManager_HandleInternalIntervalSwitchover(void)
{
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleInternalIntervalSwitchover");

    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();

    uint8 old_index = adv_task_data->params_handle->index_active_config_table_entry;
    uint8 new_index = old_index + 1;

    if(new_index > le_adv_advertising_config_set_max)
    {

        DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleInternalIntervalSwitchover, Invalid Config Table Index");
        Panic();
    }

    LeAdvertisingManager_ParametersSelect(new_index);

}

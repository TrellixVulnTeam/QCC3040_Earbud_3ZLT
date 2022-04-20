/*!
\copyright  Copyright (c) 2018 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Management of Bluetooth Low Energy advertising
*/

#include <adc.h>
#include <panic.h>
#include <stdlib.h>

#include "connection_abstraction.h"
#include <task_list.h>
#include <connection_manager.h>

#include "local_addr.h"
#include "local_name.h"

#include "le_advertising_manager.h"
#include "le_advertising_manager_sm.h"
#include "le_advertising_manager_clients.h"
#include "le_advertising_manager_data_common.h"
#include "le_advertising_manager_private.h"
#include "le_advertising_manager_select_common.h"
#include "le_advertising_manager_select_extended.h"
#include "le_advertising_manager_select_legacy.h"
#include "le_advertising_manager_utils.h"

#include "logging.h"
#include "hydra_macros.h"

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_TYPE(le_adv_mgr_message_id_t)
LOGGING_PRESERVE_MESSAGE_ENUM(adv_mgr_internal_messages_t)

#ifndef HOSTED_TEST_ENVIRONMENT
/*! There is checking that the messages assigned by this module do
not overrun into the next module's message ID allocation */

ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(ADV_MANAGER, ADV_MANAGER_MESSAGE_END)

#endif

#define LE_EXT_ADV_SCAN_ENABLE_MASK        2

/*!< Task information for the advertising manager */
adv_mgr_task_data_t app_adv_manager;

static le_adv_mgr_state_machine_t * sm = NULL;
static Task task_allow_all;
static Task task_enable_connectable;

static void leAdvertisingManager_SendReleaseDataSetCfmMessageConditionally(Task task, le_adv_mgr_status_t status);
static void leAdvertisingManager_SetAllowAdvertising(bool allow);
static bool leAdvertisingManager_ScheduleAllowDisallowMessages(bool allow, le_adv_mgr_status_t status);
static void leAdvertisingManager_EnableAdvertising(bool enable);
static bool leAdvertisingManager_ScheduleEnableDisableConnectableMessages(bool enable, le_adv_mgr_status_t status);
static void leAdvertisingManager_SetAllowedAdvertisingBitmaskConnectable(bool action);
static void leAdvertisingManager_HandleInternalParametersUpdateRequest(void);
static void leAdvertisingManager_HandleInternalReleaseDatasetRequest(const LE_ADV_MGR_INTERNAL_RELEASE_DATASET_T * msg);
static void leAdvertisingManager_HandleInternalAllowAdvertisingRequest(const LE_ADV_MGR_INTERNAL_ALLOW_ADVERTISING_T * message);
static void leAdvertisingManager_HandleInternalEnableConnectableRequest(const LE_ADV_MGR_INTERNAL_ENABLE_CONNECTABLE_T * message);
static bool leAdvertisingManager_UpdateParameters(uint8);
static void leAdvertisingManager_HandleEnableAdvertising(const LE_ADV_INTERNAL_MSG_ENABLE_ADVERTISING_T * message);
static void leAdvertisingManager_HandleNotifyRpaAddressChange(void);

static void leAdvertisingManager_HandleConManagerTpConnectInd(const CON_MANAGER_TP_CONNECT_IND_T *ind)
{
    PanicNull((CON_MANAGER_TP_CONNECT_IND_T*)ind);

    DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleConManagerTpConnectInd enum:le_adv_mgr_state_t:%d %06x Incoming:%d",
        LeAdvertisingManagerSm_GetState(),
        ind->tpaddr.taddr.addr.lap,
        ind->incoming);

   if(ind->incoming == TRUE)
   {
       /* We already got Ext Adv terminate Indication, just clear the blocking condition */
      if (leAdvertisingManager_CheckBlockingCondition(le_adv_blocking_condition_enable_connect_ind))
      {
          leAdvertisingManager_SetBlockingCondition(le_adv_blocking_condition_none);
      }
      else
      {
          if(leAdvertisingManager_CheckBlockingCondition(le_adv_blocking_condition_none))
          {
              /* No blocking condition, set the blocking condition directly here */
              leAdvertisingManager_SetBlockingCondition(le_adv_blocking_condition_enable_terminate_ind);
          }
          else
          {
              MessageSendConditionally(AdvManagerGetTask(), LE_ADV_MGR_INTERNAL_GOT_TP_CONNECT_IND,
                                       NULL, &leAdvertisingManager_GetBlockingCondition());
          }
      }
   }
}

static void handleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    DEBUG_LOG_LEVEL_1("LEAM handle_message MESSAGE:adv_mgr_internal_messages_t:0x%x",id);
    
    switch (id)
    {
        case LE_ADV_INTERNAL_MSG_ENABLE_ADVERTISING:
            leAdvertisingManager_HandleEnableAdvertising((const LE_ADV_INTERNAL_MSG_ENABLE_ADVERTISING_T*)message);
            return;
            
        case LE_ADV_INTERNAL_MSG_NOTIFY_RPA_CHANGE:
            leAdvertisingManager_HandleNotifyRpaAddressChange();
            break;
        case CL_DM_BLE_SET_ADVERTISE_ENABLE_CFM:
            LeAdvertisingManager_HandleConnectionLibraryMessages(CL_DM_BLE_SET_ADVERTISE_ENABLE_CFM, (const CL_DM_BLE_SET_ADVERTISE_ENABLE_CFM_T*)message, FALSE);
            break;
            
        case CL_DM_BLE_EXT_ADVERTISE_ENABLE_CFM:
            LeAdvertisingManager_HandleConnectionLibraryMessages(CL_DM_BLE_EXT_ADVERTISE_ENABLE_CFM, (const CL_DM_BLE_EXT_ADVERTISE_ENABLE_CFM_T*)message, FALSE);
            break;
            
        case CL_DM_BLE_SET_EXT_ADV_DATA_CFM:
            LeAdvertisingManager_HandleConnectionLibraryMessages(CL_DM_BLE_SET_EXT_ADV_DATA_CFM, (const CL_DM_BLE_SET_EXT_ADV_DATA_CFM_T*)message, FALSE);
            break;

        case CL_DM_BLE_EXT_ADV_SET_SCAN_RESPONSE_DATA_CFM:
            LeAdvertisingManager_HandleConnectionLibraryMessages(CL_DM_BLE_EXT_ADV_SET_SCAN_RESPONSE_DATA_CFM, (const CL_DM_BLE_EXT_ADV_SET_SCAN_RESPONSE_DATA_CFM_T*)message, FALSE);
            break;
            
        case CL_DM_BLE_SET_EXT_ADV_PARAMS_CFM:
            LeAdvertisingManager_HandleConnectionLibraryMessages(CL_DM_BLE_SET_EXT_ADV_PARAMS_CFM, (const CL_DM_BLE_SET_EXT_ADV_PARAMS_CFM_T*)message, FALSE);
            break;
            
        case CL_DM_BLE_EXT_ADV_REGISTER_APP_ADV_SET_CFM:
            LeAdvertisingManager_HandleConnectionLibraryMessages(CL_DM_BLE_EXT_ADV_REGISTER_APP_ADV_SET_CFM, (const CL_DM_BLE_EXT_ADV_REGISTER_APP_ADV_SET_CFM_T*)message, FALSE);
            break;
        case CL_DM_BLE_GET_ADV_SCAN_CAPABILITIES_CFM:
            LeAdvertisingManager_HandleConnectionLibraryMessages(CL_DM_BLE_GET_ADV_SCAN_CAPABILITIES_CFM, (const CL_DM_BLE_GET_ADV_SCAN_CAPABILITIES_CFM_T*)message, FALSE);
            break;
        case CL_DM_BLE_EXT_ADV_TERMINATED_IND:
            LeAdvertisingManager_HandleConnectionLibraryMessages(CL_DM_BLE_EXT_ADV_TERMINATED_IND, (const CL_DM_BLE_EXT_ADV_TERMINATED_IND_T*)message, FALSE);
            break;
        case LE_ADV_MGR_INTERNAL_MSG_NOTIFY_INTERVAL_SWITCHOVER:
            leAdvertisingManager_HandleInternalIntervalSwitchover();
            break;

        case LE_ADV_MGR_INTERNAL_DATA_UPDATE:
            leAdvertisingManager_HandleInternalDataUpdateRequest();
            break;

        case LE_ADV_MGR_INTERNAL_ENABLE_CONNECTABLE:
            leAdvertisingManager_HandleInternalEnableConnectableRequest((const LE_ADV_MGR_INTERNAL_ENABLE_CONNECTABLE_T *) message);
            break;            
            
        case LE_ADV_MGR_INTERNAL_ALLOW_ADVERTISING:
            leAdvertisingManager_HandleInternalAllowAdvertisingRequest((const LE_ADV_MGR_INTERNAL_ALLOW_ADVERTISING_T *) message);
            break;
            
        case LE_ADV_MGR_INTERNAL_RELEASE_DATASET:
            if(leAdvertisingManager_CheckBlockingCondition(le_adv_blocking_condition_none))
            {
                /* No blocking condition, process the release dataset request */
                leAdvertisingManager_HandleInternalReleaseDatasetRequest((const LE_ADV_MGR_INTERNAL_RELEASE_DATASET_T *) message);
            }
            else
            {
                /* Blocking condition exists, repost the release dataset request */
                MessageSendConditionally(AdvManagerGetTask(), LE_ADV_MGR_INTERNAL_RELEASE_DATASET, message, &leAdvertisingManager_GetBlockingCondition());				
            }
            break;
        
        case LE_ADV_MGR_INTERNAL_PARAMS_UPDATE:
            leAdvertisingManager_HandleInternalParametersUpdateRequest();
            break;

        case CON_MANAGER_TP_CONNECT_IND:
            leAdvertisingManager_HandleConManagerTpConnectInd((const CON_MANAGER_TP_CONNECT_IND_T *) message);
            break;

        case CON_MANAGER_TP_DISCONNECT_IND:
        case CON_MANAGER_TP_DISCONNECT_REQUESTED_IND:
        case CON_MANAGER_BLE_PARAMS_UPDATE_IND:
            /* These messages are sent when registered with connection manager.
               They are not needed by the LE Advertising manager. */
            break;

        case LE_ADV_MGR_INTERNAL_GOT_TP_CONNECT_IND:
            leAdvertisingManager_SetBlockingCondition(le_adv_blocking_condition_enable_terminate_ind);
            break;

        default:
            Panic();
            break;
    }
    
}

/* Local function to enable/disable connectable advertising */
static void leAdvertisingManager_InternalEnableConnectableAdvertising(Task task, bool enable)
{
    task_enable_connectable = task;

    DEBUG_LOG_LEVEL_1("leAdvertisingManager_InternalEnableConnectableAdvertising enable %d", enable);

    leAdvertisingManager_SetAllowedAdvertisingBitmaskConnectable(enable);
       
    leAdvertisingManager_EnableAdvertising(enable);

    leAdvertisingManager_ScheduleEnableDisableConnectableMessages(enable, le_adv_mgr_status_success);    
}

/* Local function to send an internal message to schedule connectable advertising */
static void leAdvertisingManager_ScheduleInternalEnableConnectableAdvertising(Task task, bool enable)
{
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
        
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_ScheduleInternalEnableConnectableAdvertising, Send message LE_ADV_MGR_INTERNAL_ENABLE_CONNECTABLE on blocking condition %d", adv_task_data->blockingCondition);
        
    MAKE_MESSAGE(LE_ADV_MGR_INTERNAL_ENABLE_CONNECTABLE);
    message->enable = enable;
    message->task = task;
    MessageSendConditionally(AdvManagerGetTask(), LE_ADV_MGR_INTERNAL_ENABLE_CONNECTABLE, message, &adv_task_data->blockingCondition );
    
}

/* Local function to handle internal connectable advertising request */
static void leAdvertisingManager_HandleInternalEnableConnectableRequest(const LE_ADV_MGR_INTERNAL_ENABLE_CONNECTABLE_T * msg)
{
    leAdvertisingManager_InternalEnableConnectableAdvertising(msg->task, msg->enable);
    
}

/* Local function to allow/disallow advertising */
static void leAdvertisingManager_InternalAllowAdvertising(Task task, bool allow)
{
    task_allow_all = task;

    leAdvertisingManager_SetAllowAdvertising(allow);
    
    leAdvertisingManager_EnableAdvertising(allow);

    leAdvertisingManager_ScheduleAllowDisallowMessages(allow, le_adv_mgr_status_success);
    
}

/* Local function to send an internal message to schedule allow/disallow advertising */
static void leAdvertisingManager_ScheduleInternalAllowAdvertising(Task task, bool allow)
{
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
        
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_ScheduleInternalAllowAdvertising, Send message LE_ADV_MGR_INTERNAL_ALLOW_ADVERTISING on blocking condition %d", adv_task_data->blockingCondition);
        
    MAKE_MESSAGE(LE_ADV_MGR_INTERNAL_ALLOW_ADVERTISING);
    message->allow = allow;
    message->task = task;
    MessageSendConditionally(AdvManagerGetTask(), LE_ADV_MGR_INTERNAL_ALLOW_ADVERTISING, message, &adv_task_data->blockingCondition );
    
}

/* Local function to handle internal allow/disallow advertising request */
static void leAdvertisingManager_HandleInternalAllowAdvertisingRequest(const LE_ADV_MGR_INTERNAL_ALLOW_ADVERTISING_T * msg)
{
    leAdvertisingManager_InternalAllowAdvertising(msg->task, msg->allow);
    
}

/* Local function to release dataset */
static bool leAdvertisingManager_InternalReleaseDataset(le_adv_data_set_handle handle)
{    
    le_adv_data_set_t start_params_set;
    
    leAdvertisingManager_EnableAdvertising(FALSE);
    
    leAdvertisingManager_SetDataSetSelectBitmask(handle->set, FALSE);
    
    leAdvertisingManager_SendReleaseDataSetCfmMessageConditionally(leAdvertisingManager_GetTaskForDataSet(handle->set), le_adv_mgr_status_success);
    
    MessageCancelAll(AdvManagerGetLegacyTask(), LE_ADV_MGR_INTERNAL_START);    
    MessageCancelAll(AdvManagerGetExtendedTask(), LE_ADV_MGR_INTERNAL_START);
    
    leAdvertisingManager_FreeHandleForDataSet(handle->set);    
    
    start_params_set = leAdvertisingManager_GetDataSetSelected();
        
    if(start_params_set)
    {
        DEBUG_LOG_LEVEL_2("LeAdvertisingManager_ReleaseAdvertisingDataSet Info, Local start parameters contain a valid set, reschedule advertising start with the set %x", leAdvertisingManager_GetDataSetSelected());
        
        leAdvertisingManager_SetDataUpdateRequired(start_params_set, TRUE);
        
        leAdvertisingManager_SetDataSetSelectMessageStatusAfterRelease(start_params_set);
        
        LeAdvertisingManager_ScheduleAdvertisingStart(start_params_set);
    }
        
    return TRUE;
}

/* Local function to send an internal message to schedule release dataset */
static void leAdvertisingManager_ScheduleInternalReleaseDataset(le_adv_data_set_handle handle)
{
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
        
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_ScheduleInternalReleaseDataset, Send message LE_ADV_MGR_INTERNAL_RELEASE_DATASET on blocking condition %d", adv_task_data->blockingCondition);
        
    MAKE_MESSAGE(LE_ADV_MGR_INTERNAL_RELEASE_DATASET);
    message->handle = handle;
    MessageSendConditionally(AdvManagerGetTask(), LE_ADV_MGR_INTERNAL_RELEASE_DATASET, message, &adv_task_data->blockingCondition );
}

/* Local function to handle internal release dataset request */
static void leAdvertisingManager_HandleInternalReleaseDatasetRequest(const LE_ADV_MGR_INTERNAL_RELEASE_DATASET_T * msg)
{
    leAdvertisingManager_InternalReleaseDataset(msg->handle);
    
}

/* Local function to send an internal message to schedule parameters update */
static void leAdvertisingManager_ScheduleInternalParametersUpdate(void)
{
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
        
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_ScheduleInternalParametersUpdate, Send message LE_ADV_MGR_INTERNAL_PARAMS_UPDATE on blocking condition %d", adv_task_data->blockingCondition);
        
    MessageSendConditionally(AdvManagerGetTask(), LE_ADV_MGR_INTERNAL_PARAMS_UPDATE, NULL, &adv_task_data->blockingCondition );
}

/* Local function to handle internal parameters update request */
static void leAdvertisingManager_HandleInternalParametersUpdateRequest(void)
{
    leAdvertisingManager_SetupAdvertParams();
}
     
/* Local Function to Handle Internal Enable Advertising Message */
static void leAdvertisingManager_HandleEnableAdvertising(const LE_ADV_INTERNAL_MSG_ENABLE_ADVERTISING_T * message)
{   
    if(TRUE == message->action)
    {                               
        LeAdvertisingManagerSm_SetState(le_adv_mgr_state_starting);
    }
    else
    {                   
        LeAdvertisingManagerSm_SetState(le_adv_mgr_state_suspending);
    }

    leAdvertisingManager_SetBlockingCondition(le_adv_blocking_condition_enable_cfm);
    
}

/* Local Function to Handle Internal Notify Rpa Address Change Message */
static void leAdvertisingManager_HandleNotifyRpaAddressChange(void)
{
    le_adv_mgr_client_iterator_t iterator;
    le_adv_mgr_register_handle client_handle = leAdvertisingManager_HeadClient(&iterator);
    
    while(client_handle)
    {
        if(client_handle->task)
        {
            MessageSend(client_handle->task, LE_ADV_MGR_RPA_TIMEOUT_IND, NULL);
        }
        
        client_handle = leAdvertisingManager_NextClient(&iterator);
    }
    
    MessageCancelAll(AdvManagerGetTask(), LE_ADV_INTERNAL_MSG_NOTIFY_RPA_CHANGE);
    MessageSendLater(AdvManagerGetTask(), LE_ADV_INTERNAL_MSG_NOTIFY_RPA_CHANGE, NULL, D_SEC(BLE_RPA_TIMEOUT_DEFAULT));
}

/* Local Function to Cancel Scheduled Enable/Disable Connectable Confirmation Messages */
static bool leAdvertisingManager_CancelPendingEnableDisableConnectableMessages(void)
{
    MessageCancelAll(task_enable_connectable, LE_ADV_MGR_ENABLE_CONNECTABLE_CFM);
    
    return TRUE;
}

/* Local Function to Schedule Enable/Disable Connectable Confirmation Messages */
static bool leAdvertisingManager_ScheduleEnableDisableConnectableMessages(bool enable, le_adv_mgr_status_t status)
{    
    adv_mgr_task_data_t *advMan = AdvManagerGetTaskData();
        
    MAKE_MESSAGE(LE_ADV_MGR_ENABLE_CONNECTABLE_CFM);
    message->enable = enable;
    message->status = status;

    MessageSendConditionally(task_enable_connectable, LE_ADV_MGR_ENABLE_CONNECTABLE_CFM, message, &advMan->blockingCondition);
    
    return TRUE;
}       
            
/* Local Function to Cancel Scheduled Allow/Disallow Messages */
static bool leAdvertisingManager_CancelPendingAllowDisallowMessages(void)
{
    MessageCancelAll(task_allow_all, LE_ADV_MGR_ALLOW_ADVERTISING_CFM);
    
    return TRUE;
}

/* Local Function to Schedule Allow/Disallow Confirmation Messages */
static bool leAdvertisingManager_ScheduleAllowDisallowMessages(bool allow, le_adv_mgr_status_t status)
{   
    adv_mgr_task_data_t *advMan = AdvManagerGetTaskData();
    
    MAKE_MESSAGE(LE_ADV_MGR_ALLOW_ADVERTISING_CFM);
    message->allow = allow;
    message->status = status;
    
    MessageSendConditionally(task_allow_all, LE_ADV_MGR_ALLOW_ADVERTISING_CFM, message, &advMan->blockingCondition);
    
    return TRUE;
}  

/* Local Function to Schedule an Internal Advertising Enable/Disable Message */
static void leAdvertisingManager_ScheduleInternalEnableMessage(bool action)
{
    adv_mgr_task_data_t *advMan = AdvManagerGetTaskData();
        
    MAKE_MESSAGE(LE_ADV_INTERNAL_MSG_ENABLE_ADVERTISING);
    message->action = action;
    MessageSendConditionally(AdvManagerGetTask(), LE_ADV_INTERNAL_MSG_ENABLE_ADVERTISING, message, &advMan->blockingCondition );
}

/* Local Function to Return the State of Connectable LE Advertising Being Enabled/Disabled */
bool leAdvertisingManager_IsConnectableAdvertisingEnabled(void)
{
    adv_mgr_task_data_t *advMan = AdvManagerGetTaskData();
    
    const unsigned bitmask_connectable_events = le_adv_event_type_connectable_general | le_adv_event_type_connectable_directed;
        
    return (bitmask_connectable_events == advMan->mask_enabled_events);
}

/* Prevent advertising if any below conditions are not met */
static bool leAdvertisingManager_IsAdvertisingPossible(void)
{
    if(!leAdvertisingManager_IsConnectableAdvertisingEnabled())
    {
        return FALSE;
    }
    if(!leAdvertisingManager_IsAdvertisingAllowed())
    {
        return FALSE;
    }
    if(!leAdvertisingManager_IsDataSetSelected())
    {
        return FALSE;
    }
    return TRUE;
}

/* Cancel pending messages and restart enable/disable */
static void leAdvertisingManager_RestartEnableAdvertising(bool enable)
{
    leAdvertisingManager_CancelPendingAllowDisallowMessages();
    leAdvertisingManager_CancelPendingEnableDisableConnectableMessages();
    leAdvertisingManager_ScheduleInternalEnableMessage(enable);
}

/* Local Function to Decide whether to Suspend or Resume Advertising and act as decided */
static void leAdvertisingManager_EnableAdvertising(bool enable)
{
    if(enable)
    {
        if(leAdvertisingManager_IsAdvertisingPossible())
        {
            if(LeAdvertisingManagerSm_IsSuspended())
            {
                LeAdvertisingManagerSm_SetState(le_adv_mgr_state_starting);
                
                leAdvertisingManager_SetBlockingCondition(le_adv_blocking_condition_enable_cfm);
            }
            else if(LeAdvertisingManagerSm_IsSuspending())
            {
                leAdvertisingManager_RestartEnableAdvertising(enable);
            }
            else if (leAdvertisingManager_EnableExtendedAdvertising(enable))
            {
                /* Nothing more to do as extended advertising will be in progress */
            }
        }
    }
    else
    {
        if(LeAdvertisingManagerSm_IsAdvertisingStarting())
        {
            leAdvertisingManager_RestartEnableAdvertising(enable);
        }
        else if(LeAdvertisingManagerSm_IsAdvertisingStarted())
        {
            LeAdvertisingManagerSm_SetState(le_adv_mgr_state_suspending);
            leAdvertisingManager_SetBlockingCondition(le_adv_blocking_condition_enable_cfm);
        }
        else if (leAdvertisingManager_EnableExtendedAdvertising(enable))
        {
            /* Nothing more to do as extended advertising will be in progress */
        }
    }
}

/* Local Function to Set/Reset Bitmask for Connectable Advertising Event Types */
static void leAdvertisingManager_SetAllowedAdvertisingBitmaskConnectable(bool action)
{
    adv_mgr_task_data_t *advMan = AdvManagerGetTaskData();

    advMan->mask_enabled_events |= le_adv_event_type_connectable_general;
    advMan->mask_enabled_events |= le_adv_event_type_connectable_directed;
    
    if(FALSE == action)
    {        
        advMan->mask_enabled_events &= ~le_adv_event_type_connectable_general;
        advMan->mask_enabled_events &= ~le_adv_event_type_connectable_directed;;
    }
}

/* Local Function to Set the Allow/Disallow Flag for All Advertising Event Types */
static void leAdvertisingManager_SetAllowAdvertising(bool allow)
{
    adv_mgr_task_data_t *advMan = AdvManagerGetTaskData();
    advMan->is_advertising_allowed = allow;
}

/* Local Function to Handle Connection Library Advertise Enable Fail Response */
static void leAdvertisingManager_HandleLegacySetAdvertisingEnableCfmFailure(void)
{
    if(leAdvertisingManager_CancelPendingAllowDisallowMessages())
    {
        bool adv_allowed = leAdvertisingManager_IsAdvertisingAllowed();
        leAdvertisingManager_ScheduleAllowDisallowMessages(adv_allowed, le_adv_mgr_status_error_unknown);
    }
    
    if(leAdvertisingManager_CancelPendingEnableDisableConnectableMessages())
    {
        bool connectable = leAdvertisingManager_IsConnectableAdvertisingEnabled();
        leAdvertisingManager_ScheduleEnableDisableConnectableMessages(connectable, le_adv_mgr_status_error_unknown);
    }
    
    LeAdvertisingManagerSm_SetState(le_adv_mgr_state_suspended);
    MessageCancelAll(AdvManagerGetTask(), LE_ADV_INTERNAL_MSG_NOTIFY_RPA_CHANGE);
}

/* Local function to set the state machine to suspended state and cancel RPA notify messages */
static void leAdvertisingManager_SetSuspendedStateAndCancelRpaNotifyMessages(void)
{
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_SetSuspendedStateAndCancelRpaNotifyMessages");
    
    LeAdvertisingManagerSm_SetState(le_adv_mgr_state_suspended);
    
    MessageCancelAll(AdvManagerGetTask(), LE_ADV_INTERNAL_MSG_NOTIFY_RPA_CHANGE);    
    
}

/* Local function to handle CL_DM_BLE_SET_ADVERTISE_ENABLE_CFM message */
static void leAdvertisingManager_HandleLegacySetAdvertisingEnableCfm(uint16 status)
{
    bool enable_success = FALSE;
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleLegacySetAdvertisingEnableCfm");
    
    if (leAdvertisingManager_CheckBlockingCondition(le_adv_blocking_condition_enable_cfm))
    {
        if(hci_success == status)
        {
            if(LeAdvertisingManagerSm_IsSuspending())
            {
                DEBUG_LOG_LEVEL_2("leAdvertisingManager_HandleLegacySetAdvertisingEnableCfm Info, State machine is in suspending state");

                leAdvertisingManager_SetSuspendedStateAndCancelRpaNotifyMessages();
                leAdvertisingManager_CancelMessageParameterSwitchover();

                enable_success = TRUE;
            }
            else if(LeAdvertisingManagerSm_IsAdvertisingStarting())
            {
                DEBUG_LOG_LEVEL_2("leAdvertisingManager_HandleLegacySetAdvertisingEnableCfm Info, State machine is in starting state");

                LeAdvertisingManagerSm_SetState(le_adv_mgr_state_started);
                MessageSendLater(AdvManagerGetTask(), LE_ADV_INTERNAL_MSG_NOTIFY_RPA_CHANGE, NULL, D_SEC(BLE_RPA_TIMEOUT_DEFAULT));
                leAdvertisingManager_SendMessageParameterSwitchover();
                enable_success = TRUE;
            }
        }
        else if( (hci_error_command_disallowed == status) && LeAdvertisingManagerSm_IsSuspending() )
        {
            DEBUG_LOG_LEVEL_2("leAdvertisingManager_HandleLegacySetAdvertisingEnableCfm Info, State machine is in suspending state, encountered an expected command disallowed error, treated as success, HCI status is %x", status);

            leAdvertisingManager_SetSuspendedStateAndCancelRpaNotifyMessages();
            leAdvertisingManager_CancelMessageParameterSwitchover();
            enable_success = TRUE;
        }
        else
        {
            DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleLegacySetAdvertisingEnableCfm Failure, CL_DM_BLE_SET_ADVERTISE_ENABLE_CFM received with failure, HCI status is %x", status);
            
            leAdvertisingManager_HandleLegacySetAdvertisingEnableCfmFailure();
        }
        
        if (!enable_success || !leAdvertisingManager_EnableExtendedAdvertising(LeAdvertisingManagerSm_IsAdvertisingStarted()))
        {
            leAdvertisingManager_SetBlockingCondition(le_adv_blocking_condition_none);
        }
    }
    else
    {
        DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleLegacySetAdvertisingEnableCfm Failure, Message Received in Unexpected Blocking Condition %x",
                          leAdvertisingManager_GetBlockingCondition());
        
        Panic();
    }
}

/* Local function to prepare and send the conditional confirmation messages */
static void leAdvertisingManager_SendReleaseDataSetCfmMessageConditionally(Task task, le_adv_mgr_status_t status)
{
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_SendReleaseDataSetCfmMessageConditionally");
    
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
    
    MAKE_MESSAGE(LE_ADV_MGR_RELEASE_DATASET_CFM);
    message->status = status;         
        
    DEBUG_LOG_LEVEL_2("leAdvertisingManager_SendReleaseDataSetCfmMessageConditionally Info, Task is %x, status is %x, on blocking condition %d", task, status, adv_task_data->blockingCondition);
       
    MessageSendConditionally(task, LE_ADV_MGR_RELEASE_DATASET_CFM, message, &adv_task_data->blockingCondition );
}

/* Local function to check if parameter fallback mechanism needs to be activated */
static bool leAdvertisingManager_IsFallbackNeeded(void)
{
    DEBUG_LOG("leAdvertisingManager_IsFallbackNeeded");

    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();

    le_adv_parameters_config_table_t * table = adv_task_data->params_handle->config_table;
    uint8 index = adv_task_data->params_handle->index_active_config_table_entry;
    uint16 timeout = table->row[index].timeout_fallback_in_seconds;

    if(timeout)
    {
        DEBUG_LOG("leAdvertisingManager_IsFallbackNeeded, Fallback is needed with timeout %d seconds", timeout);

        return TRUE;
    }

    return FALSE;

}

/* Local function to update advertising parameters in use */
static bool leAdvertisingManager_UpdateParameters(uint8 index)
{
    DEBUG_LOG("leAdvertisingManager_UpdateParameters, Parameter Set Index is %d", index);

    if(FALSE == LeAdvertisingManagerSm_IsInitialised())
    {
        DEBUG_LOG("leAdvertisingManager_UpdateParameters, Not Initialised");
        return FALSE;
    }

    if((le_adv_preset_advertising_interval_slow != index) && (le_adv_preset_advertising_interval_fast != index))
    {
        DEBUG_LOG("leAdvertisingManager_UpdateParameters, Invalid Index");
        return FALSE;
    }

    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();

    if(NULL == adv_task_data->params_handle)
    {
        DEBUG_LOG("leAdvertisingManager_UpdateParameters, Invalid Parameter");
        return FALSE;
    }

    if((index != adv_task_data->params_handle->active_params_set))
    {
        DEBUG_LOG("leAdvertisingManager_UpdateParameters, Index Different, Change Parameter Set Immediately");

        adv_task_data->params_handle->active_params_set = index;

        leAdvertisingManager_ScheduleInternalParametersUpdate();
    }
    else if((leAdvertisingManager_IsFallbackNeeded()))
    {
        DEBUG_LOG("leAdvertisingManager_UpdateParameters, Fallback Needed, Change Parameter Set after Timeout");

        leAdvertisingManager_SendMessageParameterSwitchover();
    }

    return TRUE;
}

/* API Function to Register Callback Functions For LE Advertising Data */
le_adv_mgr_register_handle LeAdvertisingManager_Register(Task task, const le_adv_data_callback_t * const callback)
{       
    DEBUG_LOG_VERBOSE("LeAdvertisingManager_Register");
    
    if(FALSE == LeAdvertisingManagerSm_IsInitialised())
        return NULL;
    
    return LeAdvertisingManager_NewClient(task, callback);
}

static void leAdvertisingManager_GetAdvScanCapabilities(void)
{
    DEBUG_LOG("leAdvertisingManager_GetAdvScanCapabilities");
    ConnectionDmBleGetAdvScanCapabilitiesReq(AdvManagerGetTask());
}

static bool leAdvertisingManager_IfExtendedAdvertisementAndScanningEnabled(uint8 available_api)
{
    return (available_api & LE_EXT_ADV_SCAN_ENABLE_MASK);
}

static void leAdvertisingManager_HandleGetAdvScanCapabilitiesCfm(const CL_DM_BLE_GET_ADV_SCAN_CAPABILITIES_CFM_T* cfm)
{
    PanicNull((CL_DM_BLE_GET_ADV_SCAN_CAPABILITIES_CFM_T*)cfm);

    DEBUG_LOG("leAdvertisingManager_HandleGetAdvScanCapabilitiesCfm status=%d available_api=%x",
                                                                    cfm->status,cfm->available_api);

    if(cfm->status != success)
    {
        return;
    }

    if(leAdvertisingManager_IfExtendedAdvertisementAndScanningEnabled(cfm->available_api))
    {
        adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
        adv_task_data->is_extended_advertising_and_scanning_enabled = TRUE;
    }
}
/* API Function to Initialise LE Advertising Manager */
bool LeAdvertisingManager_Init(Task init_task)
{
    UNUSED(init_task);

    DEBUG_LOG("LeAdvertisingManager_Init");
    
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
    
    memset(adv_task_data, 0, sizeof(*adv_task_data));
    adv_task_data->task.handler = handleMessage;
    
    leAdvertisingManager_DataInit();
    leAdvertisingManager_SelectLegacyAdvertisingInit();
    leAdvertisingManager_SelectExtendedAdvertisingInit();
    
    leAdvertisingManager_ClearDataSetSelectBitmask();
    leAdvertisingManager_ClearDataSetMessageStatusBitmask();

    adv_task_data->dataset_handset_handle = NULL;
    adv_task_data->dataset_peer_handle = NULL;
    adv_task_data->dataset_extended_handset_handle = NULL;
    adv_task_data->params_handle = NULL;
    adv_task_data->is_params_update_required = TRUE;
    
    sm = LeAdvertisingManagerSm_Init();
    PanicNull(sm);
    LeAdvertisingManagerSm_SetState(le_adv_mgr_state_initialised);
    
    leAdvertisingManager_ClientsInit();
    
    leAdvertisingManager_SetDataSetEventType(le_adv_event_type_connectable_general);
    leAdvertisingManager_GetAdvScanCapabilities();

    task_allow_all = 0;
    task_enable_connectable = 0;
    
    return TRUE;
}

/* API Function to De-Initialise LE Advertising Manager */
bool LeAdvertisingManager_DeInit(void)
{
    DEBUG_LOG("LeAdvertisingManager_DeInit");
    
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();

    if(NULL != adv_task_data->params_handle)
    {
        free(adv_task_data->params_handle);
        adv_task_data->params_handle = NULL;
    }
    
    if(NULL != adv_task_data->dataset_handset_handle)
    {
        free(adv_task_data->dataset_handset_handle);
        adv_task_data->dataset_handset_handle = NULL;
    }
    
    if(NULL != adv_task_data->dataset_peer_handle)
    {
        free(adv_task_data->dataset_peer_handle);
        adv_task_data->dataset_peer_handle = NULL;
    }
    
    if(NULL != adv_task_data->dataset_extended_handset_handle)
    {
        free(adv_task_data->dataset_extended_handset_handle);
        adv_task_data->dataset_extended_handset_handle = NULL;
    }
    
    memset(adv_task_data, 0, sizeof(adv_mgr_task_data_t));

    if(NULL != sm)
    {
        LeAdvertisingManagerSm_SetState(le_adv_mgr_state_uninitialised);
        
    }
    
    return TRUE;        
}

/* API Function to enable/disable connectable LE advertising */
bool LeAdvertisingManager_EnableConnectableAdvertising(Task task, bool enable)
{
    DEBUG_LOG("LeAdvertisingManager_EnableConnectableAdvertising enable %d", enable);
        
    if(NULL == task)
    {
        DEBUG_LOG("LeAdvertisingManager_EnableConnectableAdvertising, Task Input is Null");
        return FALSE;
    }
    
    if(FALSE == LeAdvertisingManagerSm_IsInitialised())
    {
        DEBUG_LOG("LeAdvertisingManager_EnableConnectableAdvertising, LE adv. mgr. Uninitialised");
        return FALSE;
    }
    
    if((task_enable_connectable != task) && (task_enable_connectable))
    {
        DEBUG_LOG("LeAdvertisingManager_EnableConnectableAdvertising, Task Input is Invalid");
        return FALSE;
    }

    leAdvertisingManager_ScheduleInternalEnableConnectableAdvertising(task, enable);
			
    return TRUE;
}

/* API Function to enable/disable all LE advertising */
bool LeAdvertisingManager_AllowAdvertising(Task task, bool allow)
{
    bool response = FALSE;

    if (task && LeAdvertisingManagerSm_IsInitialised())
    {
        response = TRUE;
        leAdvertisingManager_ScheduleInternalAllowAdvertising(task, allow);
    }

    DEBUG_LOG("LeAdvertisingManager_AllowAdvertising. allow:%d response:%d",
                allow, response);

    return response;
}

static void leAdvertisingManager_HandleExtAdvTerminatedIndication(const CL_DM_BLE_EXT_ADV_TERMINATED_IND_T *ind)
{
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleExtAdvTerminatedIndication enum:le_adv_mgr_state_t:%d %06x Reason:%d 0x%x",
        LeAdvertisingManagerSm_GetState(),
        ind->taddr.addr.lap,
        ind->reason,
        ind->adv_bits);

    if (leAdvertisingManager_CheckBlockingCondition(le_adv_blocking_condition_enable_terminate_ind))
    {
        leAdvertisingManager_SetBlockingCondition(le_adv_blocking_condition_none);
    }
    else
    {
        bool cancelled = MessageCancelFirst(AdvManagerGetTask(), LE_ADV_MGR_INTERNAL_GOT_TP_CONNECT_IND);
        if (cancelled)
        {
            if (MessagePendingFirst(AdvManagerGetTask(), LE_ADV_MGR_INTERNAL_GOT_TP_CONNECT_IND, NULL))
            {
                DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleExtAdvTerminatedIndication In flight GOT_TP_CONNECT_IND cancelled. One still in flight");
            }
            else
            {
                DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleExtAdvTerminatedIndication In flight GOT_TP_CONNECT_IND cancelled");
            }
        }
        else
        {
            /* If we are suspending/suspended then there are no TP_CONNECT_IND messages.
               Otherwise as there is no in flight Message, we need to wait for
               the GOT_TP_CONNECT_IND.
               Set a blocking condition */
            if (   !LeAdvertisingManagerSm_IsSuspended()
                && !LeAdvertisingManagerSm_IsSuspending()
                && !leAdvertisingManager_CheckBlockingCondition(le_adv_blocking_condition_enable_cfm))
            {
                leAdvertisingManager_SetBlockingCondition(le_adv_blocking_condition_enable_connect_ind);
            }
        }
    }
}

/* API function to use as the handler for connection library messages */
bool LeAdvertisingManager_HandleConnectionLibraryMessages(MessageId id, Message message, bool already_handled)
{
    DEBUG_LOG_V_VERBOSE("LeAdvertisingManager_HandleConnectionLibraryMessages MESSAGE:0x%x", id);
    
    switch (id)
    {
        /* Legacy advertising */
        case CL_DM_BLE_SET_ADVERTISING_DATA_CFM:
            leAdvertisingManager_HandleSetLegacyAdvertisingDataCfm(((const CL_DM_BLE_SET_ADVERTISING_DATA_CFM_T *)message)->status);
            return TRUE;

        case CL_DM_BLE_SET_SCAN_RESPONSE_DATA_CFM:
            leAdvertisingManager_HandleLegacySetScanResponseDataCfm(((const CL_DM_BLE_SET_SCAN_RESPONSE_DATA_CFM_T *)message)->status);
            return TRUE;
            
        case CL_DM_BLE_SET_ADVERTISING_PARAMS_CFM:
            leAdvertisingManager_HandleLegacySetAdvertisingParamCfm(((const CL_DM_BLE_SET_ADVERTISING_PARAMS_CFM_T *)message)->status);
            return TRUE;

        case CL_DM_BLE_SET_ADVERTISE_ENABLE_CFM:
            leAdvertisingManager_HandleLegacySetAdvertisingEnableCfm(((const CL_DM_BLE_SET_ADVERTISE_ENABLE_CFM_T *)message)->status);
            return TRUE;
            
        /* Extended advertising */
        case CL_DM_BLE_SET_EXT_ADV_DATA_CFM:
            leAdvertisingManager_HandleExtendedSetAdvertisingDataCfm((const CL_DM_BLE_SET_EXT_ADV_DATA_CFM_T *)message);
            return TRUE;

        case CL_DM_BLE_EXT_ADV_SET_SCAN_RESPONSE_DATA_CFM:
            leAdvertisingManager_HandleExtendedSetScanResponseDataCfm((const CL_DM_BLE_EXT_ADV_SET_SCAN_RESPONSE_DATA_CFM_T *)message);
            return TRUE;
            
        case CL_DM_BLE_SET_EXT_ADV_PARAMS_CFM:
            leAdvertisingManager_HandleExtendedSetAdvertisingParamCfm((const CL_DM_BLE_SET_EXT_ADV_PARAMS_CFM_T *)message);
            return TRUE;
            
        case CL_DM_BLE_EXT_ADVERTISE_ENABLE_CFM:
            leAdvertisingManager_HandleExtendedSetAdvertisingEnableCfm((const CL_DM_BLE_EXT_ADVERTISE_ENABLE_CFM_T *)message);
            return TRUE;
            
        case CL_DM_BLE_EXT_ADV_REGISTER_APP_ADV_SET_CFM:
            leAdvertisingManager_HandleExtendedAdvertisingRegisterCfm((const CL_DM_BLE_EXT_ADV_REGISTER_APP_ADV_SET_CFM_T *)message);
            return TRUE;

        case CL_DM_BLE_GET_ADV_SCAN_CAPABILITIES_CFM:
            leAdvertisingManager_HandleGetAdvScanCapabilitiesCfm((const CL_DM_BLE_GET_ADV_SCAN_CAPABILITIES_CFM_T *)message);
            return TRUE;

        case CL_DM_BLE_EXT_ADV_TERMINATED_IND:
            leAdvertisingManager_HandleExtAdvTerminatedIndication((const CL_DM_BLE_EXT_ADV_TERMINATED_IND_T *)message);
            return TRUE;

        default:
            return already_handled;
    }
}

/* API function to select the data set for undirected advertising */
le_adv_data_set_handle LeAdvertisingManager_SelectAdvertisingDataSet(Task task, const le_adv_select_params_t * params)
{
    DEBUG_LOG("LeAdvertisingManager_SelectAdvertisingDataSet");
    
    if(FALSE == LeAdvertisingManagerSm_IsInitialised())
    {
        DEBUG_LOG("LeAdvertisingManager_SelectAdvertisingDataSet Failure, State Machine is not Initialised");
            
        return NULL;
    }
    
    if( (NULL == params) || (NULL == task) )
    {
        DEBUG_LOG("LeAdvertisingManager_SelectAdvertisingDataSet Failure, Invalid Input Arguments");
        
        return NULL;
    }
    else
    {
        DEBUG_LOG_LEVEL_2("LeAdvertisingManager_SelectAdvertisingDataSet Info, Task is %x Selected Data Set is %x" , task, params->set);
    }
    
    if(leAdvertisingManager_CheckIfHandleExists(params->set))
    {
        DEBUG_LOG("LeAdvertisingManager_SelectAdvertisingDataSet Failure, Dataset Handle Already Exists");
        
        return NULL;
        
    }
    else
    {
        leAdvertisingManager_SetDataUpdateRequired(params->set, TRUE);
    
        le_adv_data_set_handle handle = leAdvertisingManager_CreateNewDataSetHandle(params->set);
        
        handle->task = task;
        
        leAdvertisingManager_SetDataSetSelectBitmask(params->set, TRUE);
        
        leAdvertisingManager_SetDataSetSelectMessageStatusBitmask(params->set, TRUE);
        
        if(leAdvertisingManager_IsLegacySet(params->set))
        {
            MessageCancelAll(AdvManagerGetLegacyTask(), LE_ADV_MGR_INTERNAL_START);
        }
        else
        {
            MessageCancelAll(AdvManagerGetExtendedTask(), LE_ADV_MGR_INTERNAL_START);
        }
            
        LeAdvertisingManager_ScheduleAdvertisingStart(params->set);  
        
        DEBUG_LOG_LEVEL_2("LeAdvertisingManager_SelectAdvertisingDataSet Info, Handle does not exist, create new handle, handle->task is %x, handle->set is %x", handle->task, handle->set);
        
        return handle;        
    }
}

/* API function to release the data set for undirected advertising */
bool LeAdvertisingManager_ReleaseAdvertisingDataSet(le_adv_data_set_handle handle)
{    
    DEBUG_LOG("LeAdvertisingManager_ReleaseAdvertisingDataSet");
    
    if(NULL == handle)
    {
        DEBUG_LOG_LEVEL_1("LeAdvertisingManager_ReleaseAdvertisingDataSet Failure, Invalid data set handle");
        
        return FALSE;
    }
    else
    {
        DEBUG_LOG_LEVEL_2("LeAdvertisingManager_ReleaseAdvertisingDataSet Info, Data set handle is %x", handle);
    }
    
    leAdvertisingManager_ScheduleInternalReleaseDataset(handle);
			    
    return TRUE;
}

/* API function to notify a change in the data */
bool LeAdvertisingManager_NotifyDataChange(Task task, const le_adv_mgr_register_handle handle)
{    
    DEBUG_LOG("LeAdvertisingManager_NotifyDataChange");
    
    if(!leAdvertisingManager_ClientHandleIsValid(handle))
    {
        DEBUG_LOG_LEVEL_1("LeAdvertisingManager_NotifyDataChange Failure, Invalid Handle");
        return FALSE;
    }
    
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();

    leAdvertisingManager_SetDataUpdateRequired(LE_ADV_MGR_ADVERTISING_SET_LEGACY, TRUE);
    
    if(adv_task_data->keep_advertising_on_notify)
    {
        DEBUG_LOG_LEVEL_2("LeAdvertisingManager_NotifyDataChange Info, Keep advertising without restarting");
        
        if(LeAdvertisingManagerSm_IsAdvertisingStarting() || LeAdvertisingManagerSm_IsAdvertisingStarted())
        {
            DEBUG_LOG_LEVEL_2("LeAdvertisingManager_NotifyDataChange Info, Advertising in progress, schedule data update without suspending ongoing advertising");
            leAdvertisingManager_ScheduleInternalDataUpdate();
        }
    }
    else
    {
        DEBUG_LOG_LEVEL_2("LeAdvertisingManager_NotifyDataChange Info, Restart advertising is needed");
        if(LeAdvertisingManagerSm_IsAdvertisingStarting() || LeAdvertisingManagerSm_IsAdvertisingStarted())
        {
            DEBUG_LOG_LEVEL_2("LeAdvertisingManager_NotifyDataChange Info, Advertising in progress, suspend and reschedule advertising");
        
            LeAdvertisingManager_ScheduleAdvertisingStart(leAdvertisingManager_GetDataSetSelected());  
        }
    }
        
    MAKE_MESSAGE(LE_ADV_MGR_NOTIFY_DATA_CHANGE_CFM);
    message->status = le_adv_mgr_status_success;
    
    MessageSend(task, LE_ADV_MGR_NOTIFY_DATA_CHANGE_CFM, message);
    return TRUE;
}

/* API function to configure LE advertising behavior when LeAdvertisingManager_NotifyDataChange() API is called */
bool LeAdvertisingManager_ConfigureAdvertisingOnNotifyDataChange(const le_adv_config_notify_t config)
{    
    DEBUG_LOG("LeAdvertisingManager_ConfigureAdvertisingOnNotifyDataChange");

    if(FALSE == LeAdvertisingManagerSm_IsInitialised())
        return FALSE;
    
    if((config != le_adv_config_notify_keep_advertising) && (config != le_adv_config_notify_restart_advertising))
        return FALSE;
    
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
    
    if(config == le_adv_config_notify_keep_advertising)
    {
        adv_task_data->keep_advertising_on_notify = TRUE;
    }
    else if(config == le_adv_config_notify_restart_advertising)
    {
        adv_task_data->keep_advertising_on_notify = FALSE;
    }
        
    return TRUE;
}

/* API function to register LE advertising parameter sets */
bool LeAdvertisingManager_ParametersRegister(const le_adv_parameters_t *params)
{
    DEBUG_LOG("LeAdvertisingManager_ParametersRegister");
    
    if(NULL == params)
        return FALSE;
    
    if(FALSE == LeAdvertisingManagerSm_IsInitialised())
        return FALSE;
    
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
    
    adv_task_data->params_handle =  PanicUnlessMalloc(sizeof(struct _le_adv_params_set));
    memset(adv_task_data->params_handle, 0, sizeof(struct _le_adv_params_set));
    
    adv_task_data->params_handle->params_set = (le_adv_parameters_set_t *)params->sets;    
    adv_task_data->params_handle->config_table = (le_adv_parameters_config_table_t *)params->table;
    adv_task_data->params_handle->active_params_set = le_adv_preset_advertising_interval_invalid;
    adv_task_data->params_handle->index_active_config_table_entry = 0;
    
    return TRUE;
    
}

/* API function to select an LE advertising parameter set config table entry */
bool LeAdvertisingManager_ParametersSelect(uint8 index)
{
    DEBUG_LOG("LeAdvertisingManager_ParametersSelect, Config Table Index is %d", index);
    
    if(FALSE == LeAdvertisingManagerSm_IsInitialised())
    {
        DEBUG_LOG("LeAdvertisingManager_ParametersSelect, Uninitialised");
        return FALSE;
    }
    
    if(index > le_adv_advertising_config_set_max)
    {
        DEBUG_LOG("LeAdvertisingManager_ParametersSelect, Invalid Table Index");
        return FALSE;
    }

    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();

    if((adv_task_data->params_handle) && (adv_task_data->params_handle->config_table))
    {
        le_adv_parameters_set_t * set = adv_task_data->params_handle->params_set;
        le_adv_parameters_config_table_t * table = adv_task_data->params_handle->config_table;

        if((NULL == adv_task_data->params_handle) \
           || (NULL == set)
           || (NULL == table))
        {
            DEBUG_LOG("LeAdvertisingManager_ParametersSelect, Invalid Parameter");
            return FALSE;
        }

        leAdvertisingManager_CancelMessageParameterSwitchover();
        adv_task_data->params_handle->index_active_config_table_entry = index;

        return leAdvertisingManager_UpdateParameters(table->row[index].set_default);

    }

    return FALSE;

}

/* API function to retrieve LE advertising interval minimum/maximum value pair */
bool LeAdvertisingManager_GetAdvertisingInterval(le_adv_common_parameters_t * interval)
{
    DEBUG_LOG("LeAdvertisingManager_GetAdvertisingInterval");
    
    if(NULL == interval)
    {
        return FALSE;
    }

    if(FALSE == LeAdvertisingManagerSm_IsInitialised())
    {
        return FALSE;
    }
    
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
    le_adv_params_set_handle handle = adv_task_data->params_handle;
    
    if(NULL == adv_task_data->params_handle)
    {
        leAdvertisingManager_GetDefaultAdvertisingIntervalParams(interval);
        
        return TRUE;
    }
    
    if(le_adv_preset_advertising_interval_max >= adv_task_data->params_handle->active_params_set)
    {
        interval->le_adv_interval_min = handle->params_set->set_type[handle->active_params_set].le_adv_interval_min;
        interval->le_adv_interval_max = handle->params_set->set_type[handle->active_params_set].le_adv_interval_max;
    }
    
    return TRUE;
}


/* API function to retrieve LE advertising own address configuration */
bool LeAdvertisingManager_GetOwnAddressConfig(le_adv_own_addr_config_t * own_address_config)
{
    DEBUG_LOG("LeAdvertisingManager_GetOwnAddressConfig");
        
    if(FALSE == LeAdvertisingManagerSm_IsInitialised())
    {
        return FALSE;
    }
    
    own_address_config->own_address_type = LocalAddr_GetBleType();
    own_address_config->timeout = BLE_RPA_TIMEOUT_DEFAULT;
    
    return TRUE;
}

/*!
\copyright  Copyright (c) 2018 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Management of Bluetooth Low Energy extended specific advertising
*/

#ifdef INCLUDE_ADVERTISING_EXTENSIONS

#include "le_advertising_manager_select_extended.h"

#include "le_advertising_manager_data_common.h"
#include "le_advertising_manager_data_extended.h"
#include "le_advertising_manager_private.h"
#include "le_advertising_manager_select_common.h"
#include "le_advertising_manager_sm.h"
#include "le_advertising_manager_utils.h"

#include "local_addr.h"

#include <bdaddr.h>

#include <panic.h>


/* Bitfields applied to ConnectionDmBleExtAdvSetParamsReq event_properties */
#define ADV_EVENT_PROPERTIES_NONE          0
#define ADV_EVENT_PROPERTIES_CONNECTABLE   (1 << 0)
#define ADV_EVENT_PROPERTIES_SCANNABLE     (1 << 1)

#define ADV_FILTER_POLICY                  0
#define ADV_PRIMARY_PHY                    1
#define ADV_SECONDARY_PHY                  1
#define ADV_SECONDARY_MAX_SKIP             0
#define ADV_SID                            0


/* Data type for the state of extended advertising */
typedef enum{
    le_adv_mgr_extended_state_idle,
    le_adv_mgr_extended_state_starting,
    le_adv_mgr_extended_state_started,
    le_adv_mgr_extended_state_suspending,
    le_adv_mgr_extended_state_suspended
}le_adv_mgr_extended_state_t;

le_adv_mgr_extended_state_t le_adv_mgr_extended_state = le_adv_mgr_extended_state_idle;

static void leAdvertisingManager_HandleExtendedSetAdvertisingParamCfm_Locally(const CL_DM_BLE_SET_EXT_ADV_PARAMS_CFM_T *cfm);

static void leAdvertisingManager_ResumeExtendedAdvertising(void)
{
    ConnectionDmBleExtAdvertiseEnableReq(AdvManagerGetTask(), TRUE, ADV_HANDLE_APP_SET_1);
}

static void leAdvertisingManager_SuspendExtendedAdvertising(void)
{
    ConnectionDmBleExtAdvertiseEnableReq(AdvManagerGetTask(), FALSE, ADV_HANDLE_APP_SET_1);
}

static bool leAdvertisingManager_GetExtendedDataUpdateRequired(void)
{
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
    
    return adv_task_data->is_extended_data_update_required;
}

static void leAdvertisingManager_EnterExtendedIdleState(void)
{
    le_adv_mgr_extended_state = le_adv_mgr_extended_state_idle;
}

static void leAdvertisingManager_EnterExtendedSuspendingState(void)
{
    le_adv_mgr_extended_state = le_adv_mgr_extended_state_suspending;
    leAdvertisingManager_SuspendExtendedAdvertising();
}

static void leAdvertisingManager_EnterExtendedSuspendedState(void)
{
    le_adv_mgr_extended_state = le_adv_mgr_extended_state_suspended;
}

static void leAdvertisingManager_EnterExtendedStartingState(void)
{
    le_adv_mgr_extended_state = le_adv_mgr_extended_state_starting;
    
}

static void leAdvertisingManager_EnterExtendedStartedState(void)
{
    le_adv_mgr_extended_state = le_adv_mgr_extended_state_started;
}

static bool leAdvertisingManager_IsExtendedStateSuspending(void)
{
    return (le_adv_mgr_extended_state_suspending == le_adv_mgr_extended_state);
}

static bool leAdvertisingManager_IsExtendedStateSuspended(void)
{
    return (le_adv_mgr_extended_state_suspended == le_adv_mgr_extended_state);
}

static bool leAdvertisingManager_IsExtendedStateStarting(void)
{
    return (le_adv_mgr_extended_state_starting == le_adv_mgr_extended_state);
}

static bool leAdvertisingManager_IsExtendedStateStarted(void)
{
    return (le_adv_mgr_extended_state_started == le_adv_mgr_extended_state);
}


static void leAdvertisingManager_SetExtendedState(le_adv_mgr_extended_state_t state)
{
    le_adv_mgr_extended_state_t old_state = le_adv_mgr_extended_state;
    
    switch(state)
    {
        case le_adv_mgr_extended_state_idle:
        leAdvertisingManager_EnterExtendedIdleState();
        break;        
        
        case le_adv_mgr_extended_state_starting:
        if(le_adv_mgr_extended_state_idle != old_state)
            leAdvertisingManager_ResumeExtendedAdvertising();
        leAdvertisingManager_EnterExtendedStartingState();
        break;        
        
        case le_adv_mgr_extended_state_suspending:
        leAdvertisingManager_EnterExtendedSuspendingState();     
        break;
        
        case le_adv_mgr_extended_state_started:
        leAdvertisingManager_EnterExtendedStartedState();
        break;
                
        case le_adv_mgr_extended_state_suspended:
        leAdvertisingManager_EnterExtendedSuspendedState();
        break;
        
        default:
        break;
    }
}

static void leAdvertisingManager_SetAdvertisingParamsReq(void)
{
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
    uint8 local_address_type = LocalAddr_GetBleType();
    Task confirm_task = AdvManagerGetTask();
    typed_bdaddr taddr = {0};
    ble_adv_params_t interval_params = leAdvertisingManager_GetAdvertisingIntervalParams();

    DEBUG_LOG_LEVEL_1("leAdvertisingManager_SetAdvertisingParamsReq(EXT) Address type:%d",
            local_address_type);

    /* For extended advert parameters... if a random or resolvable address is used
       then the address details must be supplied separately. If this is the case
       request that the response to setting data is sent locally - so that the address
       can be set correctly before continuing. */
    switch(local_address_type)
    {
        case OWN_ADDRESS_PUBLIC:
        case OWN_ADDRESS_GENERATE_RPA_FBP:
            /* No need to program address */
            break;

        case OWN_ADDRESS_RANDOM:
        case OWN_ADDRESS_GENERATE_RPA_FBR:
            DEBUG_LOG_LEVEL_2("leAdvertisingManager_SetAdvertisingParamsReq(EXT) Setting address");

            confirm_task = &adv_task_data->extended_task;
            break;

        default:
            DEBUG_LOG_WARN("leAdvertisingManager_SetAdvertisingParamsReq(EXT) Unexpected local address:%d",
                           local_address_type);
            break;
    }

    BdaddrTypedSetEmpty(&taddr);

    ConnectionDmBleExtAdvSetParamsReq(confirm_task, 
                                      ADV_HANDLE_APP_SET_1, 
                                      ADV_EVENT_PROPERTIES_CONNECTABLE,
                                      interval_params.undirect_adv.adv_interval_min,
                                      interval_params.undirect_adv.adv_interval_max, 
                                      BLE_ADV_CHANNEL_ALL, 
                                      local_address_type,
                                      taddr, 
                                      ADV_FILTER_POLICY,
                                      ADV_PRIMARY_PHY,
                                      ADV_SECONDARY_MAX_SKIP,
                                      ADV_SECONDARY_PHY,
                                      ADV_SID);
}

/* Local Function to Set Up Advertising Parameters */
static void leAdvertisingManager_SetupAdvertParams(void)
{           
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_SetupAdvertParams");

    leAdvertisingManager_SetBlockingCondition(le_adv_blocking_condition_params_cfm);

    DEBUG_LOG_LEVEL_2("leAdvertisingManager_SetupAdvertParams Info, Request advertising parameters set, blocking condition is %x", leAdvertisingManager_GetBlockingCondition());
    leAdvertisingManager_SetAdvertisingParamsReq();
}

/* Local Function to Start Extended Advertising */
static bool leAdvertisingManager_StartExtendedAdvertising(void)
{    
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_StartExtendedAdvertising");
    
    if (!LeAdvertisingManager_CanAdvertisingBeStarted())
    {
        DEBUG_LOG_LEVEL_1("leAdvertisingManager_StartExtendedAdvertising Failure");
        return FALSE;
    }
    
    leAdvertisingManager_SetupAdvertParams();
    
    leAdvertisingManager_SetExtendedState(le_adv_mgr_extended_state_starting);
    return TRUE;
}

/* Local Function to Handle Internal Advertising Start Request */
static void leAdvertisingManager_HandleExtendedInternalStartRequest(const LE_ADV_MGR_INTERNAL_START_T * msg)
{
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleExtendedInternalStartRequest");
    
    if(leAdvertisingManager_IsExtendedStateStarted())
    {
        DEBUG_LOG_LEVEL_2("leAdvertisingManager_HandleExtendedInternalStartRequest Info, Advertising already started, suspending and rescheduling");
        
        leAdvertisingManager_SetExtendedState(le_adv_mgr_extended_state_suspending);
        leAdvertisingManager_SetBlockingCondition(le_adv_blocking_condition_enable_cfm);

        leAdvertisingManager_SetDataUpdateRequired(LE_ADV_MGR_ADVERTISING_SET_EXTENDED, TRUE);
        
        LeAdvertisingManager_ScheduleAdvertisingStart(msg->set);
        return;
    }
    
    leAdvertisingManager_SetExtendedState(le_adv_mgr_extended_state_idle);
    
    leAdvertisingManager_SetDataSetEventType(le_adv_event_type_connectable_general);
    
    leAdvertisingManager_StartExtendedAdvertising();
    
    LeAdvertisingManager_SendSelectConfirmMessage();
}

/* Forward the status from setting the random address in the extended advertising
   to the normal advertising manager handler */
static void leAdvertisingManager_handleSetRandomAddressCfm(const CL_DM_BLE_EXT_ADV_SET_RANDOM_ADDRESS_CFM_T *confirm)
{
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();

    DEBUG_LOG_VERBOSE("leAdvertisingManager_handleSetRandomAddressCfm enum:connection_lib_status:%d ADDR: %04x%02x%06lx", 
                        confirm->status,
                        confirm->random_addr.lap, 
                        confirm->random_addr.uap, 
                        confirm->random_addr.nap);

    if (   confirm->status == hci_error_controller_busy /* Can be busy behind the scenes */
        && adv_task_data->extended_advert_rpa_retries)
    {
        bdaddr not_used = {0};

        DEBUG_LOG_WARN("leAdvertisingManager_handleSetRandomAddressCfm. Busy, retrying request.");

        adv_task_data->extended_advert_rpa_retries--;

        ConnectionDmBleExtAdvSetRandomAddressReq(&adv_task_data->extended_task,
                                                 ADV_HANDLE_APP_SET_1,
                                                 ble_local_addr_use_global,
                                                 not_used);
    }
    else
    {
        MESSAGE_MAKE(cfm, CL_DM_BLE_SET_EXT_ADV_PARAMS_CFM_T);

        cfm->status = (confirm->status == hci_success) ? success : fail;
        cfm->adv_sid = 0xFF;    /* Select an unusual value. Not used at present. */

        leAdvertisingManager_HandleExtendedSetAdvertisingParamCfm(cfm);
    }
}


static void leAdvertisingManager_handleExtendedAdvertMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    DEBUG_LOG_LEVEL_1("leAdvertisingManager_handleExtendedAdvertMessage state enum:le_adv_mgr_extended_state_t:%d MSG:adv_mgr_internal_messages_t:0x%x",
                     le_adv_mgr_extended_state, id);

    switch (id)
    {
        case LE_ADV_MGR_INTERNAL_START:
            leAdvertisingManager_HandleExtendedInternalStartRequest((const LE_ADV_MGR_INTERNAL_START_T *)message);
            break;

        case CL_DM_BLE_EXT_ADV_SET_RANDOM_ADDRESS_CFM:
            leAdvertisingManager_handleSetRandomAddressCfm((const CL_DM_BLE_EXT_ADV_SET_RANDOM_ADDRESS_CFM_T *)message);
            break;

        case CL_DM_BLE_SET_EXT_ADV_PARAMS_CFM:
            leAdvertisingManager_HandleExtendedSetAdvertisingParamCfm_Locally((const CL_DM_BLE_SET_EXT_ADV_PARAMS_CFM_T *)message);
            break;

        default:
            break;
    }
    
}

static void leAdvertisingManager_RegisterExtendedApplicationSet(uint8 adv_handle)
{
    PanicFalse(leAdvertisingManager_CheckBlockingCondition(le_adv_blocking_condition_none));
    
    leAdvertisingManager_SetBlockingCondition(le_adv_blocking_condition_register_cfm);

    ConnectionDmBleExtAdvRegisterAppAdvSetReq(AdvManagerGetTask(), adv_handle);
}


void leAdvertisingManager_SelectExtendedAdvertisingInit(void)
{
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
    
    adv_task_data->extended_task.handler = leAdvertisingManager_handleExtendedAdvertMessage;
    
    le_adv_mgr_extended_state = le_adv_mgr_extended_state_idle;
    
    leAdvertisingManager_RegisterExtendedDataIf();
    
    leAdvertisingManager_RegisterExtendedApplicationSet(ADV_HANDLE_APP_SET_1);
}

bool leAdvertisingManager_EnableExtendedAdvertising(bool enable)
{
    bool extended_enabled = FALSE;
    
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_EnableExtendedAdvertising enable:%d dataset:%d state enum:le_adv_mgr_extended_state_t:%d",
                      enable, leAdvertisingManager_GetDataSetSelected(), le_adv_mgr_extended_state);

    if (leAdvertisingManager_SelectOnlyExtendedSet((leAdvertisingManager_GetDataSetSelected()))
        || leAdvertisingManager_IsExtendedStateSuspending())
    {
        if(enable)
        {
            if(leAdvertisingManager_IsExtendedStateSuspended())
            {
                leAdvertisingManager_SetExtendedState(le_adv_mgr_extended_state_starting);
                leAdvertisingManager_SetBlockingCondition(le_adv_blocking_condition_enable_cfm);
                extended_enabled = TRUE;
            }
            else if(leAdvertisingManager_IsExtendedStateStarting())
            {
                extended_enabled = TRUE;
            }
        }
        else
        {
            if(leAdvertisingManager_IsExtendedStateStarted())
            {
                leAdvertisingManager_SetExtendedState(le_adv_mgr_extended_state_suspending);
                leAdvertisingManager_SetBlockingCondition(le_adv_blocking_condition_enable_cfm);
                extended_enabled = TRUE;
            }
            else if(leAdvertisingManager_IsExtendedStateSuspending())
            {
                extended_enabled = TRUE;
            }
        }
    }
    
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_EnableExtendedAdvertising enabled:%d", extended_enabled);
    
    return extended_enabled;
}

/* Function to handle CL_DM_BLE_SET_EXT_ADV_DATA_CFM message */
void leAdvertisingManager_HandleExtendedSetAdvertisingDataCfm(const CL_DM_BLE_SET_EXT_ADV_DATA_CFM_T* cfm)
{
    le_adv_data_set_t set;

    DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleExtendedSetAdvertisingDataCfm");

    if(leAdvertisingManager_CheckBlockingCondition(le_adv_blocking_condition_data_cfm))
    {
        if (success == cfm->status)
        {
            DEBUG_LOG_LEVEL_2("leAdvertisingManager_HandleExtendedSetAdvertisingDataCfm Info, CL_DM_BLE_SET_ADVERTISING_DATA_CFM received with success");
            set = leAdvertisingManager_SelectOnlyExtendedSet((leAdvertisingManager_GetDataSetSelected()));
            leAdvertisingManager_SetupScanResponseData(set);
            leAdvertisingManager_ClearData(set);
            leAdvertisingManager_SetBlockingCondition(le_adv_blocking_condition_scan_response_cfm);
        }
        else
        {
            DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleExtendedSetAdvertisingDataCfm Failure, CL_DM_BLE_SET_ADVERTISING_DATA_CFM received with failure");
            Panic();
        }
    }
    else
    {
        DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleExtendedSetAdvertisingDataCfm Failure, Message Received in Unexpected Blocking Condition %x", leAdvertisingManager_GetBlockingCondition());
        Panic();
    }
}

/* Function to handle CL_DM_BLE_EXT_ADV_SET_SCAN_RESPONSE_DATA_CFM message */
void leAdvertisingManager_HandleExtendedSetScanResponseDataCfm(const CL_DM_BLE_EXT_ADV_SET_SCAN_RESPONSE_DATA_CFM_T * cfm)
{    
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleExtendedSetScanResponseDataCfm");
    
    if (leAdvertisingManager_CheckBlockingCondition(le_adv_blocking_condition_scan_response_cfm))
    {
        DEBUG_LOG_LEVEL_2("leAdvertisingManager_HandleExtendedSetScanResponseDataCfm Info, adv_task_data->blockingCondition is %x cfm->status is %x", leAdvertisingManager_GetBlockingCondition(), cfm->status);

        if (success == cfm->status)
        {            
            DEBUG_LOG_LEVEL_2("leAdvertisingManager_HandleExtendedSetScanResponseDataCfm Info, CL_DM_BLE_SET_SCAN_RESPONSE_DATA_CFM received with success");    
            
            leAdvertisingManager_SetExtendedState(le_adv_mgr_extended_state_starting);
            leAdvertisingManager_SetBlockingCondition(le_adv_blocking_condition_enable_cfm);
        }
        else
        {
            DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleExtendedSetScanResponseDataCfm Failure, CL_DM_BLE_SET_SCAN_RESPONSE_DATA_CFM received with failure");
            Panic();
        }
    }
    else
    {
        DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleExtendedSetScanResponseDataCfm Failure, Message Received in Unexpected Blocking Condition %x", leAdvertisingManager_GetBlockingCondition());
        Panic();
    }
}

static void leAdvertisingManager_HandleExtendedSetAdvertisingParamCfm_Locally(const CL_DM_BLE_SET_EXT_ADV_PARAMS_CFM_T *cfm)
{
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
    bdaddr not_used = {0};

    if (cfm->status == success)
    {
        ConnectionDmBleExtAdvSetRandomAddressReq(&adv_task_data->extended_task,
                                                 ADV_HANDLE_APP_SET_1,
                                                 ble_local_addr_use_global,
                                                 not_used);

        adv_task_data->extended_advert_rpa_retries = 2;
    }
    else
    {
        /* Error occurred. No need to set random address */
        leAdvertisingManager_HandleExtendedSetAdvertisingParamCfm(cfm);
    }
}

/* Function to handle CL_DM_BLE_SET_EXT_ADV_PARAMS_CFM message */
void leAdvertisingManager_HandleExtendedSetAdvertisingParamCfm(const CL_DM_BLE_SET_EXT_ADV_PARAMS_CFM_T *cfm)
{
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleExtendedSetAdvertisingParamCfm");
    
    if (leAdvertisingManager_CheckBlockingCondition(le_adv_blocking_condition_params_cfm))
    {            
        DEBUG_LOG_LEVEL_2("leAdvertisingManager_HandleExtendedSetAdvertisingParamCfm Info, adv_task_data->blockingCondition is %x cfm->status is %x", leAdvertisingManager_GetBlockingCondition(), cfm->status);
            
        if (success == cfm->status)
        {            
            DEBUG_LOG_LEVEL_2("leAdvertisingManager_HandleExtendedSetAdvertisingParamCfm Info, CL_DM_BLE_SET_ADVERTISING_PARAMS_CFM received with success");    
            
            if(leAdvertisingManager_GetExtendedDataUpdateRequired())
            {
                DEBUG_LOG_LEVEL_2("leAdvertisingManager_HandleExtendedSetAdvertisingParamCfm Info, Data update is needed");        
                
                leAdvertisingManager_SetDataUpdateRequired(LE_ADV_MGR_ADVERTISING_SET_EXTENDED, FALSE);
                
                if(leAdvertisingManager_BuildData(leAdvertisingManager_SelectOnlyExtendedSet((leAdvertisingManager_GetDataSetSelected()))))
                {
                    leAdvertisingManager_SetupAdvertData(leAdvertisingManager_SelectOnlyExtendedSet(leAdvertisingManager_GetDataSetSelected()));
                    leAdvertisingManager_SetBlockingCondition(le_adv_blocking_condition_data_cfm);
                }
                else
                {
                    DEBUG_LOG_LEVEL_2("leAdvertisingManager_HandleExtendedSetAdvertisingParamCfm Info, There is no data to advertise");
                    leAdvertisingManager_ClearData(leAdvertisingManager_SelectOnlyExtendedSet((leAdvertisingManager_GetDataSetSelected())));
                }
            }
            else
            {
                leAdvertisingManager_SetExtendedState(le_adv_mgr_extended_state_starting);
                leAdvertisingManager_SetBlockingCondition(le_adv_blocking_condition_enable_cfm);
            }
        }
        else
        {            
            DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleExtendedSetAdvertisingParamCfm Failure, CL_DM_BLE_SET_ADVERTISING_PARAMS_CFM received with failure");

            Panic();
        }
    }
    else
    {      
        DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleExtendedSetAdvertisingParamCfm Failure, Message Received in Unexpected Blocking Condition %x", leAdvertisingManager_GetBlockingCondition());
                                    
        Panic();
    }
}

void leAdvertisingManager_HandleExtendedSetAdvertisingEnableCfm(const CL_DM_BLE_EXT_ADVERTISE_ENABLE_CFM_T *cfm)
{
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleExtendedSetAdvertisingEnableCfm");
    
    if (leAdvertisingManager_CheckBlockingCondition(le_adv_blocking_condition_enable_cfm))
    {
        if(hci_success == cfm->status)
        {
            if(leAdvertisingManager_IsExtendedStateSuspending())
            {
                DEBUG_LOG_LEVEL_2("leAdvertisingManager_HandleExtendedSetAdvertisingEnableCfm Info, State machine is in suspending state");
                
                leAdvertisingManager_SetExtendedState(le_adv_mgr_extended_state_suspended);
            }
            else if(leAdvertisingManager_IsExtendedStateStarting())
            {
                DEBUG_LOG_LEVEL_2("leAdvertisingManager_HandleExtendedSetAdvertisingEnableCfm Info, State machine is in starting state");

                leAdvertisingManager_SetExtendedState(le_adv_mgr_extended_state_started);
            }
        }
        else if( (hci_error_command_disallowed == cfm->status) && leAdvertisingManager_IsExtendedStateSuspending() )
        {
            DEBUG_LOG_LEVEL_2("leAdvertisingManager_HandleExtendedSetAdvertisingEnableCfm Info, State machine is in suspending state, encountered an expected command disallowed error, treated as success, HCI status is %x", cfm->status);

            leAdvertisingManager_SetExtendedState(le_adv_mgr_extended_state_suspended);
        }
        else
        {
            DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleExtendedSetAdvertisingEnableCfm Failure, CL_DM_BLE_SET_ADVERTISE_ENABLE_CFM received with failure, HCI status is %x", cfm->status);

            leAdvertisingManager_SetExtendedState(le_adv_mgr_extended_state_suspended);
        }
        
        leAdvertisingManager_SetBlockingCondition(le_adv_blocking_condition_none);
    }
    else
    {
        DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleExtendedSetAdvertisingEnableCfm Failure, Message Received in Unexpected Blocking Condition %x", leAdvertisingManager_GetBlockingCondition());
        
        Panic();
    }
}

void leAdvertisingManager_HandleExtendedAdvertisingRegisterCfm(const CL_DM_BLE_EXT_ADV_REGISTER_APP_ADV_SET_CFM_T *cfm)
{
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleExtendedAdvertisingRegisterCfm");
    
    if (leAdvertisingManager_CheckBlockingCondition(le_adv_blocking_condition_register_cfm))
    {
        leAdvertisingManager_SetBlockingCondition(le_adv_blocking_condition_none);
        
        if(hci_success != cfm->status)
        {
            Panic();
        }
    }
}

bool LeAdvertisingManager_IsExtendedAdvertisingActive(void)
{
    return (le_adv_mgr_extended_state_started == le_adv_mgr_extended_state);
}

#endif /* INCLUDE_ADVERTISING_EXTENSIONS*/

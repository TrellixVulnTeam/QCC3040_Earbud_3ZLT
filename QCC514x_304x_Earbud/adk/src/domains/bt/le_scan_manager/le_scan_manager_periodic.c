/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of module managing le periodic scanning.
*/

#ifdef INCLUDE_ADVERTISING_EXTENSIONS


#include "le_scan_manager_periodic.h"

#include <logging.h>
#include <panic.h>
#include <vmtypes.h>
#include <stdlib.h>

static void leScanManager_HandlePeriodicClMessagesTask(Task, MessageId, Message);

const TaskData periodic_task = {.handler = leScanManager_HandlePeriodicClMessagesTask};

#define Lesmp_GetTask() ((Task)&periodic_task)

/*! LE Periodic States */
typedef enum le_scan_manger_periodic_states
{
    /*! Periodic module has not yet been initialised */
    LE_SCAN_MANAGER_PERIODIC_STATE_UNINITIALISED,
    /*! Periodic module is enabled */
    LE_SCAN_MANAGER_PERIODIC_STATE_ENABLED,
    /*! Periodic module is disabled */
    LE_SCAN_MANAGER_PERIODIC_STATE_DISABLED,
    /*! Periodic module is paused */
    LE_SCAN_MANAGER_PERIODIC_STATE_PAUSED,
    /*! Periodic module is scanning */
    LE_SCAN_MANAGER_PERIODIC_STATE_SCANNING
} periodicScanState;

/*! Current LE Periodic Command*/
typedef enum le_scan_manager_periodic_commands
{
    /*! Periodic module No Command */
    LE_SCAN_MANAGER_CMD_PERIODIC_NONE,
    /*! Periodic module Find Trains Command */
    LE_SCAN_MANAGER_CMD_PERIODIC_START_FIND_TRAIN,
    /*! Periodic module Stop Command */
    LE_SCAN_MANAGER_CMD_PERIODIC_STOP,
    /*! Periodic module Disable Command */
    LE_SCAN_MANAGER_CMD_PERIODIC_DISABLE,
    /*! Periodic module Enable Command */
    LE_SCAN_MANAGER_CMD_PERIODIC_ENABLE
} periodicScanCommand;

/* \brief LE Periodic Scan settings. */
typedef struct
{
    /*! Filter for Periodic Scanning*/
    le_periodic_advertising_filter_t filter;
    /*! Scan Handle for Periodic Scanning*/
    uint8 scan_handle;
    /*! Scan Procedure for Periodic Scanning*/
    periodicScanCommand scan_procedure;
    /*! Scan Task for Periodic Scanning*/
    Task scan_task;
    /*! Sync Handle for Periodic Scanning*/
    uint16 sync_handle;
} le_periodic_scan_settings_t;

/*! \brief LE scan manager task and state machine Strcuture. */
typedef struct
{
   /*! Task for Periodic Scanning message handling */
   TaskData task;
   /*! State for Periodic Scanning */
   periodicScanState state;
   /*! Current command for Periodic Scanning */
   periodicScanCommand command;
   /*! Settings used for the Busy lock */
   le_periodic_scan_settings_t* is_busy;
   /*! Busy lock */
   uint16 busy_lock;
   /*! Current Task Requester for receiving periodic messages*/
   Task  requester;
   /*! List Of tasks which to get response of filtered extended adverts*/
   task_list_t ext_scan_filtered_adv_report_client_list; 
   /*! List Of tasks which to get response of Periodic Adverts*/
   task_list_t find_trains_client_list; 
   /*! Active settings */
   le_periodic_scan_settings_t* active_settings[MAX_ACTIVE_SCANS];
}le_scan_manager_periodic_data_t;  

/*!< SM data structure */
le_scan_manager_periodic_data_t  le_scan_manager_periodic_data;

#define LeScanManagerGetPeriodicTaskData()          (&le_scan_manager_periodic_data)
#define LeScanManagerGetPeriodicTask()              (&le_scan_manager_periodic_data.task)
#define LeScanManagerGetPeriodicState()             (le_scan_manager_periodic_data.state)
#define LeScanManagerSetPeriodicCurrentCommand(s)   (le_scan_manager_periodic_data.command = s)
#define LeScanManagerGetPeriodicCurrentCommand()    (le_scan_manager_periodic_data.command)

static void leScanManager_handleConnectionDmBlePeriodicScanStartFindTrainsCfm(const CL_DM_BLE_PERIODIC_SCAN_START_FIND_TRAINS_CFM_T* cfm);
static void leScanManager_handleConnectionDmBlePeriodicScanStopFindTrainsCfm(const CL_DM_BLE_PERIODIC_SCAN_STOP_FIND_TRAINS_CFM_T* cfm);
static void leScanManager_HandlePeriodicMessages(Task task, MessageId id, Message message);


static void leScanManager_SetPeriodicState(periodicScanState state)
{
    DEBUG_LOG("leScanManager_SetPeriodicState %d->%d", le_scan_manager_periodic_data.state, state);
    le_scan_manager_periodic_data.state = state;
}

static void leScanManager_SendPeriodicStopCfm(Task req_task, Task scan_task, le_scan_manager_status_t scan_status)
{
    MAKE_MESSAGE(LE_SCAN_MANAGER_PERIODIC_STOP_CFM);
    message->result = scan_status;
    message->scan_task = scan_task;
    MessageSend(req_task,LE_SCAN_MANAGER_PERIODIC_STOP_CFM,message);
}

static void leScanManager_SendPeriodicDisableCfm(Task req_task, le_scan_manager_status_t scan_status)
{
    MAKE_MESSAGE(LE_SCAN_MANAGER_PERIODIC_DISABLE_CFM);
    message->result = scan_status;
    MessageSend(req_task,LE_SCAN_MANAGER_PERIODIC_DISABLE_CFM,message);
}

static void leScanManager_SendPeriodicEnableCfm(Task req_task, le_scan_manager_status_t scan_status)
{
    MAKE_MESSAGE(LE_SCAN_MANAGER_PERIODIC_ENABLE_CFM);
    message->result = scan_status;
    MessageSend(req_task,LE_SCAN_MANAGER_PERIODIC_ENABLE_CFM,message);
}

static uint8 leScanManager_GetPeriodicScanEmptySlotIndex(void)
{
    uint8 settings_index;
    le_scan_manager_periodic_data_t* periodic_data = LeScanManagerGetPeriodicTaskData();

    for (settings_index = 0; settings_index < MAX_ACTIVE_SCANS; settings_index++)
    {
        if (periodic_data->active_settings[settings_index] == NULL)
        {
            break;
        }
    }

    return settings_index;
}

static le_periodic_scan_settings_t* leScanManager_StorePeriodicFindTrainsScan(le_periodic_advertising_filter_t* filter, Task task)
{
    le_periodic_scan_settings_t *scan_settings;
    le_scan_manager_periodic_data_t* periodic_data = LeScanManagerGetPeriodicTaskData();
    uint8 settings_index = leScanManager_GetPeriodicScanEmptySlotIndex();
    
    PanicNull(filter);
    
    if (settings_index < MAX_ACTIVE_SCANS)
    {
        DEBUG_LOG("leScanManager_StorePeriodicFindTrainsScan scan settings available.");
        periodic_data->active_settings[settings_index] = PanicUnlessMalloc(sizeof(*scan_settings));
 
        periodic_data->active_settings[settings_index]->filter.size_ad_types = filter->size_ad_types;
        periodic_data->active_settings[settings_index]->filter.ad_types = PanicUnlessMalloc(filter->size_ad_types);
        memcpy(periodic_data->active_settings[settings_index]->filter.ad_types , filter->ad_types , sizeof(uint8)*(filter->size_ad_types));
        
        periodic_data->active_settings[settings_index]->scan_procedure = LE_SCAN_MANAGER_CMD_PERIODIC_START_FIND_TRAIN;
        periodic_data->active_settings[settings_index]->scan_task = task;

        scan_settings = periodic_data->active_settings[settings_index];
    }
    else
    {
        DEBUG_LOG("leScanManager_StorePeriodicFindTrainsScan scan settings unavailable.");
        scan_settings = NULL;
    }
    return scan_settings;
}

static uint8 leScanManager_GetPeriodicIndexFromTask(Task task)
{
    le_scan_manager_periodic_data_t* periodic_data = LeScanManagerGetPeriodicTaskData();
    uint8 settings_index;
    for (settings_index = 0; settings_index < MAX_ACTIVE_SCANS; settings_index++)
    {
        le_periodic_scan_settings_t *scan_settings = periodic_data->active_settings[settings_index];
        if (scan_settings && scan_settings->scan_task == task)
        {
            break;
        }
    }

    return settings_index;
}

static void leScanManager_FreePeriodicScanSettings(le_periodic_scan_settings_t *scan_settings)
{
    if (scan_settings->filter.ad_types)
    {
        free(scan_settings->filter.ad_types);
    }
    free(scan_settings);
}

static bool leScanManager_ClearPeriodicScanOnTask(Task requester)
{
    le_scan_manager_periodic_data_t* periodic_data = LeScanManagerGetPeriodicTaskData();
    int settings_index;

    for (settings_index = 0; settings_index < MAX_ACTIVE_SCANS; settings_index++)
    {
        if ((periodic_data->active_settings[settings_index]!= NULL)&&(periodic_data->active_settings[settings_index]->scan_task == requester))
        {
            leScanManager_FreePeriodicScanSettings(periodic_data->active_settings[settings_index]);
            periodic_data->active_settings[settings_index] = NULL;

            return TRUE;
        }
    }
    return FALSE;
}

static bool leScanManager_ReleasePeriodicScan(Task task)
{
    bool released = FALSE;
    le_scan_manager_periodic_data_t* periodic_data = LeScanManagerGetPeriodicTaskData();
    uint8 settings_index = leScanManager_GetPeriodicIndexFromTask(task);

    if (settings_index < MAX_ACTIVE_SCANS)
    {
        le_periodic_scan_settings_t *scan_settings = periodic_data->active_settings[settings_index];

        DEBUG_LOG("leScanManager_ReleaseScan scan settings released index %u", settings_index);
        leScanManager_FreePeriodicScanSettings(scan_settings);
        periodic_data->active_settings[settings_index] = NULL;
        released = TRUE;
    }

    return released;
}

static bool leScanManager_AnyActivePeriodicScan(void)
{
    int settings_index;
    bool scan_active = FALSE;
    le_scan_manager_periodic_data_t* periodic_data = LeScanManagerGetPeriodicTaskData();

    for (settings_index = 0; settings_index < MAX_ACTIVE_SCANS; settings_index++)
    {
        if ((periodic_data->active_settings[settings_index]!= NULL)&&
            (periodic_data->active_settings[settings_index]->scan_procedure == LE_SCAN_MANAGER_CMD_PERIODIC_START_FIND_TRAIN))
        {
            scan_active = TRUE;
        }
    }
    
    return scan_active;
}

static bool leScanManager_addExtScanFilteredAdvReportClient(Task client)
{
    le_scan_manager_periodic_data_t * periodic_data = LeScanManagerGetPeriodicTaskData();
    return TaskList_AddTask(&periodic_data->ext_scan_filtered_adv_report_client_list, client);
}

static bool leScanManager_addPeriodicFindTrainsClient(Task client)
{
    le_scan_manager_periodic_data_t * periodic_data = LeScanManagerGetPeriodicTaskData();
    return TaskList_AddTask(&periodic_data->find_trains_client_list, client);
}

static bool leScanManager_removePeriodicFindTrainsClient(Task client)
{
    le_scan_manager_periodic_data_t * periodic_data = LeScanManagerGetPeriodicTaskData();
    return TaskList_RemoveTask(&periodic_data->find_trains_client_list, client);
}


static void leScanManager_SendPeriodicScanFindTrainsStartCfm(Task task,le_scan_result_t scan_status, uint8 scan_handle)
{
    MAKE_MESSAGE(LE_SCAN_MANAGER_START_PERIODIC_SCAN_FIND_TRAINS_CFM);
    message->status = scan_status;
    message->scan_handle = scan_handle;
    MessageSend(task,LE_SCAN_MANAGER_START_PERIODIC_SCAN_FIND_TRAINS_CFM,message);
}

static void leScanManager_handlePeriodicScanFailure(periodicScanCommand cmd, Task req)
{
    DEBUG_LOG("leScanManager_handlePeriodicScanFailure for command %d and client %d",cmd,req);
    switch(cmd)
    {
        case LE_SCAN_MANAGER_CMD_PERIODIC_START_FIND_TRAIN:
        {
            leScanManager_SendPeriodicScanFindTrainsStartCfm(req, LE_SCAN_MANAGER_RESULT_FAILURE, 0);
        }
        break;
        default:
            Panic();
        break;
    }
    leScanManager_ClearPeriodicScanOnTask(req);
}

static void leScanManager_SetPeriodicBusy(le_periodic_scan_settings_t* scan_settings)
{
    le_scan_manager_periodic_data_t * periodic_data = LeScanManagerGetPeriodicTaskData();

    periodic_data->is_busy = scan_settings;
    periodic_data->busy_lock = 1;
}

static bool leScanManager_IsPeriodicBusy(void)
{
    le_scan_manager_periodic_data_t * periodic_data = LeScanManagerGetPeriodicTaskData();

    return periodic_data->busy_lock;
}

static le_periodic_scan_settings_t* leScanManager_GetPeriodicBusySettings(void)
{
    le_scan_manager_periodic_data_t * periodic_data = LeScanManagerGetPeriodicTaskData();

    return periodic_data->is_busy;
}

static void leScanManager_ClearPeriodicBusy(void)
{
    le_scan_manager_periodic_data_t * periodic_data = LeScanManagerGetPeriodicTaskData();

    periodic_data->is_busy = NULL;
    periodic_data->busy_lock = 0;
}

static void leScanManager_SendFindTrainReq(le_periodic_scan_settings_t* scan_settings)
{
    DEBUG_LOG("leScanManager_SendFindTrainReq handles scan:%d sync:%d procedure:enum:periodicScanCommand:%d",
                scan_settings->scan_handle, scan_settings->sync_handle,
                scan_settings->scan_procedure);

    leScanManager_SetPeriodicBusy(scan_settings);

    uint8 *ad_structure_info[CL_AD_STRUCT_INFO_BYTE_PTRS] = {0};

    ConnectionDmBlePeriodicScanStartFindTrainsReq(Lesmp_GetTask(),
                                                  0, /* XXXXX-0-00 Flags: Receive all, Report all */
                                                  0, /*scan_for_x_seconds*/
                                                  0, /*ad_structure_filter*/
                                                  0, /*ad_structure_filter_sub_field1*/
                                                  0, /*ad_structure_filter_sub_field2*/
                                                  0, /* ad_structure_info_len */
                                                    ad_structure_info);
}

static void leScanManager_SendStopFindTrainReq(le_periodic_scan_settings_t* scan_settings)
{
    leScanManager_SetPeriodicBusy(scan_settings);
    DEBUG_LOG("leScanManager_SendStopFindTrainReq");

    ConnectionDmBlePeriodicScanStopFindTrainsReq(Lesmp_GetTask(), scan_settings->scan_handle);
}

static bool leScanManager_isPeriodicDuplicate(Task requester)
{
    le_scan_manager_periodic_data_t * periodic_data = LeScanManagerGetPeriodicTaskData();
    int settings_index;

    for (settings_index = 0; settings_index < MAX_ACTIVE_SCANS; settings_index++)
    {
        if ((periodic_data->active_settings[settings_index]!= NULL) && (periodic_data->active_settings[settings_index]->scan_task == requester))
        {
            return TRUE;
        }
    }
    return FALSE;
}

static void leScanManager_HandlePeriodicScanFindTrainsStart(Task task, le_periodic_advertising_filter_t* filter)
{
    periodicScanState current_state = LeScanManagerGetPeriodicState();
    DEBUG_LOG("leScanManager_HandlePeriodicScanFindTrainsStart Current State is:: %d", current_state);
    bool respond = FALSE;
    le_periodic_scan_settings_t* scan_settings = NULL;
    le_scan_manager_status_t scan_result = {LE_SCAN_MANAGER_RESULT_FAILURE};

    if(leScanManager_isPeriodicDuplicate(task))
    {
        DEBUG_LOG("Found Duplicate for Task %d",task);
        scan_result.status = LE_SCAN_MANAGER_RESULT_FAILURE;
        respond = TRUE;
    }
    else if(leScanManager_IsPeriodicBusy())
    {
        DEBUG_LOG("CL is Busy!");
        scan_result.status = LE_SCAN_MANAGER_RESULT_BUSY;
        respond = TRUE;
    }
    else
    {
        switch (current_state)
        {
            case LE_SCAN_MANAGER_PERIODIC_STATE_DISABLED:
            case LE_SCAN_MANAGER_PERIODIC_STATE_PAUSED:
            {
                /* Save the scan parameters and respond. Scan shall start on resume/enable */
                DEBUG_LOG("leScanManager_HandlePeriodicScanFindTrainsStart Cannot start scanning in state %d!", current_state);
                scan_settings = leScanManager_StorePeriodicFindTrainsScan(filter,task);
                
                if(scan_settings)
                {
                    DEBUG_LOG("leScanManager_HandlePeriodicScanFindTrainsStart new scan settings created.");
                    scan_result.status = LE_SCAN_MANAGER_RESULT_SUCCESS;
                }
                else
                {
                    scan_result.status = LE_SCAN_MANAGER_RESULT_FAILURE;
                }
                respond = TRUE;
            }
            break;

            case LE_SCAN_MANAGER_PERIODIC_STATE_ENABLED:
            case LE_SCAN_MANAGER_PERIODIC_STATE_SCANNING:
            {
                /* Acquire the Scan and save the Filter Details in Local Structure */
                scan_settings = leScanManager_StorePeriodicFindTrainsScan(filter,task);

                if(scan_settings)
                {
                    LeScanManagerSetPeriodicCurrentCommand(LE_SCAN_MANAGER_CMD_PERIODIC_START_FIND_TRAIN);
                    DEBUG_LOG("leScanManager_HandlePeriodicScanFindTrainsStart new scan settings created.");
                    leScanManager_SendFindTrainReq(scan_settings);
                }
                else
                {
                    scan_result.status = LE_SCAN_MANAGER_RESULT_FAILURE;
                    respond = TRUE;
                }
            }
            break;

            default:
                break;
        }
    }
    if(respond)
    {
        leScanManager_SendPeriodicScanFindTrainsStartCfm(task,scan_result.status,0);
    }
}

static Task leScanManager_GetNextPeriodicScanTaskAfterSpecifiedTask(Task task)
{
    int settings_index;
    le_scan_manager_periodic_data_t* periodic_data = LeScanManagerGetPeriodicTaskData();
    
    for (settings_index = 0; settings_index < MAX_ACTIVE_SCANS; settings_index++)
    {
        if ((periodic_data->active_settings[settings_index]!= NULL) && (periodic_data->active_settings[settings_index]->scan_task==task))
        {
            settings_index++;
            for (; settings_index < MAX_ACTIVE_SCANS; settings_index++)
            {
                if (periodic_data->active_settings[settings_index]!= NULL)
                {
                    switch (periodic_data->active_settings[settings_index]->scan_procedure)
                    {
                        case LE_SCAN_MANAGER_CMD_PERIODIC_START_FIND_TRAIN:
                        {
                            return periodic_data->active_settings[settings_index]->scan_task;
                        }
                        default:
                        break;
                    }
                }
            }
        }
    }
    
    return NULL;
}

static bool leScanManager_StartPeriodicScanByTask(Task task)
{
    int settings_index;
    le_scan_manager_periodic_data_t* periodic_data = LeScanManagerGetPeriodicTaskData();
    
    for (settings_index = 0; settings_index < MAX_ACTIVE_SCANS; settings_index++)
    {
        if (periodic_data->active_settings[settings_index]!= NULL)
        {
            if(periodic_data->active_settings[settings_index]->scan_task == task)
            {
                switch (periodic_data->active_settings[settings_index]->scan_procedure)
                {
                    case LE_SCAN_MANAGER_CMD_PERIODIC_START_FIND_TRAIN:
                    {
                        leScanManager_SendFindTrainReq(periodic_data->active_settings[settings_index]);
                        return FALSE;
                    }
                    default:
                        Panic();
                    break;
                }
            }
        }
    }
    
    return TRUE;
}

static void leScanManager_HandlePeriodicEnable(Task current_task)
{
    Task periodic_scan_task = leScanManager_GetNextPeriodicScanTaskAfterSpecifiedTask(current_task);
    le_scan_manager_periodic_data_t * periodic_data = LeScanManagerGetPeriodicTaskData();
    le_scan_manager_status_t scan_result = {LE_SCAN_MANAGER_RESULT_SUCCESS};
    bool respond = FALSE;
    
    if (periodic_scan_task)
    {
        respond = leScanManager_StartPeriodicScanByTask(periodic_scan_task);
    }
    else
    {
        respond = TRUE;
    }
    
    if(respond)
    {
        leScanManager_ClearPeriodicBusy();
        leScanManager_SetPeriodicState(LE_SCAN_MANAGER_PERIODIC_STATE_SCANNING);
        leScanManager_SendPeriodicEnableCfm(periodic_data->requester, scan_result);
    }
}

static void leScanManager_handleConnectionDmBlePeriodicScanStartFindTrainsCfm(const CL_DM_BLE_PERIODIC_SCAN_START_FIND_TRAINS_CFM_T* cfm)
{
    le_periodic_scan_settings_t *scan_settings = leScanManager_GetPeriodicBusySettings();
    Task scan_task = scan_settings->scan_task;
    periodicScanCommand scan_command = LeScanManagerGetPeriodicCurrentCommand();
    
    if (cfm->status == fail)
    {
        leScanManager_handlePeriodicScanFailure(scan_command, scan_task);
    } 
    else if (cfm->status == success)
    {
        switch(LeScanManagerGetPeriodicState())
        {
            case LE_SCAN_MANAGER_PERIODIC_STATE_ENABLED:
            case LE_SCAN_MANAGER_PERIODIC_STATE_SCANNING:
            {
                if(scan_command == LE_SCAN_MANAGER_CMD_PERIODIC_START_FIND_TRAIN)
                {
                    scan_settings->scan_handle = cfm->scan_handle;
                    leScanManager_ClearPeriodicBusy();
                    leScanManager_SetPeriodicState(LE_SCAN_MANAGER_PERIODIC_STATE_SCANNING);
                    leScanManager_addPeriodicFindTrainsClient(scan_task);
                    leScanManager_SendPeriodicScanFindTrainsStartCfm(scan_task, LE_SCAN_MANAGER_RESULT_SUCCESS, cfm->scan_handle);
                }
            }
            break;
            case LE_SCAN_MANAGER_PERIODIC_STATE_DISABLED:
            {
                if(scan_command == LE_SCAN_MANAGER_CMD_PERIODIC_ENABLE)
                {
                    scan_settings->scan_handle = cfm->scan_handle;
                    leScanManager_addExtScanFilteredAdvReportClient(scan_task);
                    leScanManager_HandlePeriodicEnable(scan_task);
                }
            }
            break;
            default:
            break;
        }
    }
}

static void leScanManager_handleConnectionDmBleExtScanFilteredAdvReportInd(const CL_DM_BLE_EXT_SCAN_FILTERED_ADV_REPORT_IND_T* ind)
{
    le_scan_manager_periodic_data_t * periodic_data = LeScanManagerGetPeriodicTaskData();

    switch(LeScanManagerGetPeriodicState())
    {
        case LE_SCAN_MANAGER_PERIODIC_STATE_SCANNING:
        {
            /* The indication's adv_data member points to data stored in a
               stream buffer. Connection library schedules an internal message
               that will be delivered immediately after this message is
               delivered to drop the data from the stream's buffer. Therefore
               the normal TaskList_MessageSendWithSize function cannot be used
               to forward the message to clients, as the adv_data would already
               be freed when the message was delivered to the clients. Therefore
               each client's handler is called directly. */
            Task next_client = NULL;
            while (TaskList_Iterate(&periodic_data->ext_scan_filtered_adv_report_client_list, &next_client))
            {
                next_client->handler(next_client, LE_SCAN_MANAGER_EXT_SCAN_FILTERED_ADV_REPORT_IND, ind);
            }
            next_client = NULL;
            while (TaskList_Iterate(&periodic_data->find_trains_client_list, &next_client))
            {
                next_client->handler(next_client, LE_SCAN_MANAGER_PERIODIC_FIND_TRAINS_ADV_REPORT_IND, ind);
            }
        }
        break;
        default:
        break;
    }
}

static Task leScanManager_GetFirstPeriodicScanTask(void)
{
    int settings_index;
    le_scan_manager_periodic_data_t* periodic_data = LeScanManagerGetPeriodicTaskData();
    
    for (settings_index = 0; settings_index < MAX_ACTIVE_SCANS; settings_index++)
    {
        if (periodic_data->active_settings[settings_index]!= NULL)
        {
            switch (periodic_data->active_settings[settings_index]->scan_procedure)
            {
                case LE_SCAN_MANAGER_CMD_PERIODIC_START_FIND_TRAIN:
                {
                    return periodic_data->active_settings[settings_index]->scan_task;
                }
                default:
                break;
            }
        }
    }
    
    return NULL;
}

static bool leScanManager_StopPeriodicScanByTask(Task task)
{
    int settings_index;
    le_scan_manager_periodic_data_t* periodic_data = LeScanManagerGetPeriodicTaskData();
    
    for (settings_index = 0; settings_index < MAX_ACTIVE_SCANS; settings_index++)
    {
        if (periodic_data->active_settings[settings_index]!= NULL)
        {
            if(periodic_data->active_settings[settings_index]->scan_task == task)
            {
                switch (periodic_data->active_settings[settings_index]->scan_procedure)
                {
                    case LE_SCAN_MANAGER_CMD_PERIODIC_START_FIND_TRAIN:
                    {
                        leScanManager_SendStopFindTrainReq(periodic_data->active_settings[settings_index]);
                        return FALSE;
                    }
                    default:
                        Panic();
                    break;
                }
            }
        }
    }
    
    return TRUE;
}

static void leScanManager_HandlePeriodicDisable(void)
{
    Task periodic_scan_task = leScanManager_GetFirstPeriodicScanTask();
    le_scan_manager_periodic_data_t * periodic_data = LeScanManagerGetPeriodicTaskData();
    le_scan_manager_status_t scan_result = {LE_SCAN_MANAGER_RESULT_SUCCESS};
    bool respond = FALSE;
    
    if (periodic_scan_task)
    {
        respond = leScanManager_StopPeriodicScanByTask(periodic_scan_task);
    }
    else
    {
        respond = TRUE;
    }
    
    if(respond)
    {
        leScanManager_ClearPeriodicBusy();
        leScanManager_SetPeriodicState(LE_SCAN_MANAGER_PERIODIC_STATE_DISABLED);
        leScanManager_SendPeriodicDisableCfm(periodic_data->requester, scan_result);
    }
}

static void leScanManager_handleConnectionDmBlePeriodicScanStopFindTrainsCfm(const CL_DM_BLE_PERIODIC_SCAN_STOP_FIND_TRAINS_CFM_T* cfm)
{
    le_scan_manager_periodic_data_t * periodic_data = LeScanManagerGetPeriodicTaskData();
    le_periodic_scan_settings_t *scan_settings = leScanManager_GetPeriodicBusySettings();
    periodicScanCommand scan_command = LeScanManagerGetPeriodicCurrentCommand();
    le_scan_manager_status_t scan_result = {LE_SCAN_MANAGER_RESULT_FAILURE};
    Task scan_task;
    
    if (cfm->status == success)
    {
        switch(LeScanManagerGetPeriodicState())
        {
            case LE_SCAN_MANAGER_PERIODIC_STATE_ENABLED:
            case LE_SCAN_MANAGER_PERIODIC_STATE_SCANNING:
            {
                if(scan_command == LE_SCAN_MANAGER_CMD_PERIODIC_STOP)
                {
                    scan_task = scan_settings->scan_task;
                    leScanManager_ClearPeriodicBusy();
                    leScanManager_removePeriodicFindTrainsClient(scan_task);
                    leScanManager_ReleasePeriodicScan(scan_task);
                    scan_result.status = LE_SCAN_MANAGER_RESULT_SUCCESS;
                    leScanManager_SendPeriodicStopCfm(periodic_data->requester, scan_task, scan_result);
                    /* update state if nothing scanning */
                    if (!leScanManager_AnyActivePeriodicScan())
                    {
                        leScanManager_SetPeriodicState(LE_SCAN_MANAGER_PERIODIC_STATE_ENABLED);
                    }
                }
                else if(scan_command == LE_SCAN_MANAGER_CMD_PERIODIC_DISABLE)
                {
                    scan_task = scan_settings->scan_task;
                    leScanManager_removePeriodicFindTrainsClient(scan_task);
                    leScanManager_ReleasePeriodicScan(scan_task);
                    leScanManager_HandlePeriodicDisable();
                }
            }
            break;
            default:
            break;
        }
    }
    else
    {
        Panic();
    }
}

/************************** API *************************************************/

void leScanManager_PeriodicScanInit(void)
{
    le_scan_manager_periodic_data_t * scanTask = LeScanManagerGetPeriodicTaskData();
    memset(scanTask, 0, sizeof(*scanTask));
    scanTask->task.handler = leScanManager_HandlePeriodicMessages;
    TaskList_Initialise(&scanTask->ext_scan_filtered_adv_report_client_list);
    TaskList_Initialise(&scanTask->find_trains_client_list);
    leScanManager_SetPeriodicState(LE_SCAN_MANAGER_PERIODIC_STATE_ENABLED);
}

void leScanManager_StartPeriodicScanFindTrains(Task task, le_periodic_advertising_filter_t* filter)
{
    DEBUG_LOG("leScanManager_StartPeriodicScanFindTrains from Requester %d",task);
    PanicNull(task);
    PanicFalse(LeScanManagerGetPeriodicState() > LE_SCAN_MANAGER_PERIODIC_STATE_UNINITIALISED);

    leScanManager_HandlePeriodicScanFindTrainsStart(task,filter);
}

bool leScanManager_PeriodicScanStop(Task req_task, Task scan_task)
{
    le_scan_manager_periodic_data_t * periodic_data = LeScanManagerGetPeriodicTaskData();
    le_scan_manager_status_t scan_result = {LE_SCAN_MANAGER_RESULT_FAILURE};
    bool respond = FALSE;
    
    if (leScanManager_GetPeriodicIndexFromTask(scan_task) >= MAX_ACTIVE_SCANS)
    {
        return FALSE;
    }
    
    if(leScanManager_IsPeriodicBusy())
    {
        DEBUG_LOG("CL is Busy!");
        scan_result.status = LE_SCAN_MANAGER_RESULT_BUSY;
        respond = TRUE;
    }
    else
    {
        switch(LeScanManagerGetPeriodicState())
        {
            case LE_SCAN_MANAGER_PERIODIC_STATE_SCANNING:
            {
                uint8 settings_index = leScanManager_GetPeriodicIndexFromTask(scan_task);

                if (settings_index < MAX_ACTIVE_SCANS)
                {
                    periodic_data->requester = req_task;
                    LeScanManagerSetPeriodicCurrentCommand(LE_SCAN_MANAGER_CMD_PERIODIC_STOP);
                    respond = leScanManager_StopPeriodicScanByTask(scan_task);
                    scan_result.status = LE_SCAN_MANAGER_RESULT_SUCCESS;
                }
                else
                {
                    /* Settings not found */
                    DEBUG_LOG("leScanManager_PeriodicScanStop cannot release scan settings");
                    scan_result.status = LE_SCAN_MANAGER_RESULT_SUCCESS;
                    respond = TRUE;
                }
            }
            break;
            default:
                scan_result.status = LE_SCAN_MANAGER_RESULT_SUCCESS;
                respond = TRUE;
                break;
        }
    }
    
    if(respond)
    {
        leScanManager_SendPeriodicStopCfm(req_task, scan_task, scan_result);
    }
    
    return TRUE;
}

bool LeScanManager_IsPeriodicTaskScanning(Task task)
{
    return leScanManager_isPeriodicDuplicate(task);
}

bool leScanManager_PeriodicScanDisable(Task req_task)
{
    le_scan_manager_periodic_data_t * periodic_data = LeScanManagerGetPeriodicTaskData();
    le_scan_manager_status_t scan_result = {LE_SCAN_MANAGER_RESULT_FAILURE};
    bool respond = FALSE;
    Task periodic_scan_task = leScanManager_GetFirstPeriodicScanTask();
    
    if (periodic_scan_task == NULL)
    {
        if (LeScanManagerGetPeriodicState() == LE_SCAN_MANAGER_PERIODIC_STATE_ENABLED)
        {
            leScanManager_SetPeriodicState(LE_SCAN_MANAGER_PERIODIC_STATE_DISABLED);
        }
        return FALSE;
    }
    
    if(leScanManager_IsPeriodicBusy())
    {
        DEBUG_LOG("CL is Busy!");
        scan_result.status = LE_SCAN_MANAGER_RESULT_BUSY;
        respond = TRUE;
    }
    else
    {
        switch(LeScanManagerGetPeriodicState())
        {
            case LE_SCAN_MANAGER_PERIODIC_STATE_SCANNING:
            {
                periodic_data->requester = req_task;
                LeScanManagerSetPeriodicCurrentCommand(LE_SCAN_MANAGER_CMD_PERIODIC_DISABLE);
                respond = leScanManager_StopPeriodicScanByTask(periodic_scan_task);
                scan_result.status = LE_SCAN_MANAGER_RESULT_SUCCESS;
            }
            break;
            default:
            {
                leScanManager_SetPeriodicState(LE_SCAN_MANAGER_PERIODIC_STATE_DISABLED);
                scan_result.status = LE_SCAN_MANAGER_RESULT_SUCCESS;
                respond = TRUE;
            }
            break;
        }
    }
    
    if(respond)
    {
        leScanManager_SendPeriodicDisableCfm(req_task, scan_result);
    }
    
    return TRUE;
}

bool leScanManager_PeriodicScanEnable(Task req_task)
{
    le_scan_manager_periodic_data_t * periodic_data = LeScanManagerGetPeriodicTaskData();
    le_scan_manager_status_t scan_result = {LE_SCAN_MANAGER_RESULT_FAILURE};
    bool respond = FALSE;
    Task periodic_scan_task = leScanManager_GetFirstPeriodicScanTask();
    
    if (periodic_scan_task == NULL)
    {
        if (LeScanManagerGetPeriodicState() == LE_SCAN_MANAGER_PERIODIC_STATE_DISABLED)
        {
            leScanManager_SetPeriodicState(LE_SCAN_MANAGER_PERIODIC_STATE_ENABLED);
        }
        return FALSE;
    }
    
    if(leScanManager_IsPeriodicBusy())
    {
        DEBUG_LOG("CL is Busy!");
        scan_result.status = LE_SCAN_MANAGER_RESULT_BUSY;
        respond = TRUE;
    }
    else
    {
        switch(LeScanManagerGetPeriodicState())
        {
            case LE_SCAN_MANAGER_PERIODIC_STATE_DISABLED:
            {
                periodic_data->requester = req_task;
                LeScanManagerSetPeriodicCurrentCommand(LE_SCAN_MANAGER_CMD_PERIODIC_ENABLE);
                respond = leScanManager_StartPeriodicScanByTask(periodic_scan_task);
                scan_result.status = LE_SCAN_MANAGER_RESULT_SUCCESS;
                if (respond)
                {
                    leScanManager_SetPeriodicState(LE_SCAN_MANAGER_PERIODIC_STATE_ENABLED);
                }
            }
            break;
            case LE_SCAN_MANAGER_PERIODIC_STATE_ENABLED:
            case LE_SCAN_MANAGER_PERIODIC_STATE_SCANNING:
            {
                DEBUG_LOG("leScanManager_PeriodicScanEnable no action in (%d) state", LeScanManagerGetPeriodicState());
                return FALSE;
            }
            case LE_SCAN_MANAGER_PERIODIC_STATE_PAUSED:
            {
            }
            break;
            default:
            {
                Panic();
            }
            break;
        }
    }
    
    if(respond)
    {
        leScanManager_SendPeriodicEnableCfm(req_task, scan_result);
    }
    
    return TRUE;
}

static void leScanManager_HandlePeriodicClMessagesTask(Task task, MessageId id, Message message)
{
    UNUSED(task);
    leScanManager_HandlePeriodicClMessages(id, message);
}

bool leScanManager_HandlePeriodicClMessages(MessageId id, Message message)
{
    DEBUG_LOG("leScanManager_HandlePeriodicClMessages MESSAGE:0x%x", id);
    switch (id)
    {
        /* CL messages */
        case CL_DM_BLE_PERIODIC_SCAN_START_FIND_TRAINS_CFM:
            leScanManager_handleConnectionDmBlePeriodicScanStartFindTrainsCfm((const CL_DM_BLE_PERIODIC_SCAN_START_FIND_TRAINS_CFM_T*) message);
            break;
        case CL_DM_BLE_EXT_SCAN_FILTERED_ADV_REPORT_IND:
            leScanManager_handleConnectionDmBleExtScanFilteredAdvReportInd((const CL_DM_BLE_EXT_SCAN_FILTERED_ADV_REPORT_IND_T*) message);
            break;
        case CL_DM_BLE_PERIODIC_SCAN_STOP_FIND_TRAINS_CFM:
            leScanManager_handleConnectionDmBlePeriodicScanStopFindTrainsCfm((const CL_DM_BLE_PERIODIC_SCAN_STOP_FIND_TRAINS_CFM_T*) message);
            break;
        default:
            return FALSE;
    }
    return TRUE;
}

static void leScanManager_HandlePeriodicMessages(Task task, MessageId id, Message message)
{
    UNUSED(task);
    leScanManager_HandlePeriodicClMessages(id, message);
}

#endif /* INCLUDE_ADVERTISING_EXTENSIONS */

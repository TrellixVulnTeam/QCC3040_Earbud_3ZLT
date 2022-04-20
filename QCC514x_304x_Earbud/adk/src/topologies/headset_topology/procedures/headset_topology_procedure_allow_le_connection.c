/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Procedure to enable LE connections to the handset

            \note this allows the connection if one arrives, but does not 
            make any changes to allow a connection
*/

#include "headset_topology_procedure_allow_le_connection.h"

#include <handset_service.h>
#include <le_advertising_manager.h>
#include <connection_manager.h>

#include <logging.h>

#include <message.h>
#include <panic.h>


/*! Parameter definition for LE connectable enable */
const ALLOW_LE_CONNECTION_PARAMS_T hs_topology_procedure_allow_le_connection_enable = { .enable = TRUE };
/*! Parameter definition for LE connectable disable */
const ALLOW_LE_CONNECTION_PARAMS_T hs_topology_procedure_allow_le_connection_disable = { .enable = FALSE };

static void headsetTopology_ProcAllowLEConnectionHandleMessage(Task task, MessageId id, Message message);
static void HeadsetTopology_ProcAllowLEConnectionStart(Task result_task,
                                                                   procedure_start_cfm_func_t proc_start_cfm_fn,
                                                                   procedure_complete_func_t proc_complete_fn,
                                                                   Message goal_data);

static void HeadsetTopology_ProcAllowLEConnectionCancel(procedure_cancel_cfm_func_t proc_cancel_cfm_fn);


typedef struct
{
    TaskData task;
    procedure_complete_func_t complete_fn;    
    procedure_cancel_cfm_func_t cancel_fn;
    bool    le_connectable;
    bool    le_advertising;
} headsetTopProcAllowLEConnectionTaskData;

const procedure_fns_t hs_proc_allow_le_connection_fns =
{
    HeadsetTopology_ProcAllowLEConnectionStart,
    HeadsetTopology_ProcAllowLEConnectionCancel,
};

headsetTopProcAllowLEConnectionTaskData headsettop_proc_allow_le_connection = {headsetTopology_ProcAllowLEConnectionHandleMessage};


#define HeadsetTopProcAllowLEConnectionGetTaskData()     (&headsettop_proc_allow_le_connection)
#define HeadsetTopProcAllowLEConnectionGetTask()         (&headsettop_proc_allow_le_connection.task)

static void headsetTopology_ProcAllowLEConnectionResetProc(void)
{
    headsetTopProcAllowLEConnectionTaskData *td = HeadsetTopProcAllowLEConnectionGetTaskData();
    
    HandsetService_ClientUnregister(HeadsetTopProcAllowLEConnectionGetTask());
    
    td->complete_fn = NULL;  
    td->cancel_fn = NULL;
    td->le_advertising = FALSE;
    td->le_connectable = FALSE;
}


static void HeadsetTopology_ProcAllowLEConnectionStart(Task result_task,
                                                                   procedure_start_cfm_func_t proc_start_cfm_fn,
                                                                   procedure_complete_func_t proc_complete_fn,
                                                                   Message goal_data)
{
    ALLOW_LE_CONNECTION_PARAMS_T* params = (ALLOW_LE_CONNECTION_PARAMS_T*)goal_data;
    
    headsetTopProcAllowLEConnectionTaskData *td = HeadsetTopProcAllowLEConnectionGetTaskData();

    UNUSED(result_task);

    td->complete_fn = proc_complete_fn;
    td->le_advertising = TRUE;
    td->le_connectable = TRUE;

    /* Register to be able to receive HANDSET_SERVICE_BLE_CONNECTABLE_CFM */
    HandsetService_ClientRegister(HeadsetTopProcAllowLEConnectionGetTask());

    DEBUG_LOG_VERBOSE("HeadsetTopology_ProcedureAllowLEConnectionStart ENABLE: %d", params->enable);

    LeAdvertisingManager_AllowAdvertising(HeadsetTopProcAllowLEConnectionGetTask(), params->enable);

    ConManagerAllowConnection(cm_transport_ble, params->enable);

    HandsetService_SetBleConnectable(params->enable);

    /* procedure started synchronously so indicate success */
    proc_start_cfm_fn(hs_topology_procedure_allow_le_connection, procedure_result_success);
}

static void HeadsetTopology_ProcAllowLEConnectionCancel(procedure_cancel_cfm_func_t proc_cancel_cfm_fn)
{
    headsetTopProcAllowLEConnectionTaskData *td = HeadsetTopProcAllowLEConnectionGetTaskData();

     DEBUG_LOG_VERBOSE("HeadsetTopology_ProcAllowLEConnectionCancel");
    /* There is no way to cancel the previous call to HandsetService_SetBleConnectable */
    td->complete_fn = NULL;

    headsetTopology_ProcAllowLEConnectionResetProc();

    Procedures_DelayedCancelCfmCallback(proc_cancel_cfm_fn,
                                  hs_topology_procedure_allow_le_connection,
                                  procedure_result_success);
}

static void headsetTopology_ProcAllowLEConnectionCompleteCancel(void)
{
    /* Use handset service to get LE connectable status */
    headsetTopProcAllowLEConnectionTaskData* td = HeadsetTopProcAllowLEConnectionGetTaskData();

    if(!(td->le_connectable) || !(td->le_advertising))
    {
        if (td->complete_fn)
        {
            td->complete_fn(hs_topology_procedure_allow_le_connection, procedure_result_success);
        }
        headsetTopology_ProcAllowLEConnectionResetProc();
    }
}

static void headsetTopology_ProcAllowLEConnectionHandleAllowLEConnectionCfm(const HANDSET_SERVICE_LE_CONNECTABLE_IND_T *ind)
{
    /* Use handset service to get LE connectable status */
    headsetTopProcAllowLEConnectionTaskData* td = HeadsetTopProcAllowLEConnectionGetTaskData();

    DEBUG_LOG_VERBOSE("headsetTopology_ProcAllowLEConnectionHandleAllowLEConnectionCfm LE Connectable: %d status %d", ind->le_connectable, ind->status);

    td->le_connectable = FALSE;
    headsetTopology_ProcAllowLEConnectionCompleteCancel();

}

static void headsetTopology_ProcAllowLEConnectionHandleAllowLEAdvertisingCfm(const LE_ADV_MGR_ALLOW_ADVERTISING_CFM_T *ind)
{
    /* Use handset service to get LE advertising status */
    headsetTopProcAllowLEConnectionTaskData* td = HeadsetTopProcAllowLEConnectionGetTaskData();

    DEBUG_LOG_VERBOSE("headsetTopology_ProcAllowLEConnectionHandleAllowLEAdvertisingCfm LE Advertising: %d status %d", ind->allow, ind->status);

    if(ind->status == le_adv_mgr_status_error_unknown)
    {
        Panic();
    }

    td->le_advertising = FALSE;
    headsetTopology_ProcAllowLEConnectionCompleteCancel();

}

static void headsetTopology_ProcAllowLEConnectionHandleMessage(Task task, MessageId id, Message message)
{
    headsetTopProcAllowLEConnectionTaskData* td = HeadsetTopProcAllowLEConnectionGetTaskData();

    UNUSED(task);

    DEBUG_LOG_VERBOSE("headsetTopology_ProcAllowLEConnectionHandleMessage id MESSAGE:0x%x", id);

    if (!td->complete_fn && !td->cancel_fn)
    {
        DEBUG_LOG_VERBOSE("delayed response, procedure completed, ignore id MESSAGE:0x%x", id);
        return;
    }
    switch (id)
    {
        case HANDSET_SERVICE_LE_CONNECTABLE_IND:
        {
            headsetTopology_ProcAllowLEConnectionHandleAllowLEConnectionCfm((const HANDSET_SERVICE_LE_CONNECTABLE_IND_T *)message);
            break;
        }
        case LE_ADV_MGR_ALLOW_ADVERTISING_CFM:
        {
            headsetTopology_ProcAllowLEConnectionHandleAllowLEAdvertisingCfm((const LE_ADV_MGR_ALLOW_ADVERTISING_CFM_T *)message);
            break;
        }
        default:
        {
            DEBUG_LOG_VERBOSE("headsetTopology_ProcAllowLEConnectionHandleMessage unhandled id MESSAGE:0x%x", id);
            break;
        }
    }
}



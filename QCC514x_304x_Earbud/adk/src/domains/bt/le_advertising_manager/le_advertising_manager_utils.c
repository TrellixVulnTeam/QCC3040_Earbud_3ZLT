/*!
\copyright  Copyright (c) 2018 - 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of internal LE advertising manager common utilities.
*/

#include "le_advertising_manager_utils.h"

#include "le_advertising_manager_private.h"

#include <stdlib.h>
#include <panic.h>

/*! \brief Checks whether two sets of input parameters are matched
    \params params1, first set of parameters
            params2, second set of parameters
    
    \return TRUE, if parameters matched, FALSE if not matched.    
 */
bool LeAdvertisingManager_ParametersMatch(const le_adv_data_params_t * params1, const le_adv_data_params_t * params2)
{
    if(params1->data_set != params2->data_set)
        return FALSE;

    if(params1->placement != params2->placement)
        return FALSE;
    
    if(params1->completeness != params2->completeness)
        return FALSE;
    
    return TRUE;
}

/* Local function to retrieve the reference to the handle asssigned to the given data set */
le_adv_data_set_handle * leAdvertisingManager_GetReferenceToHandleForDataSet(le_adv_data_set_t set)
{
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_GetReferenceToHandleForDataSet");
        
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
        
    le_adv_data_set_handle * p_handle = NULL;
    
    switch(set)
    {
        case le_adv_data_set_handset_unidentifiable:
        case le_adv_data_set_handset_identifiable:

        p_handle = &adv_task_data->dataset_handset_handle;
        DEBUG_LOG_LEVEL_2("leAdvertisingManager_GetReferenceToHandleForDataSet Info, Pointer to handle assigned to handset data set is %x handle is %x", p_handle, adv_task_data->dataset_handset_handle);
                
        break;
        
        case le_adv_data_set_peer:

        p_handle = &adv_task_data->dataset_peer_handle;
        DEBUG_LOG_LEVEL_2("leAdvertisingManager_GetReferenceToHandleForDataSet Info, Pointer to handle assigned to peer data set is %x handle is %x", p_handle, adv_task_data->dataset_peer_handle);            

        break;
        
        case le_adv_data_set_extended_handset:

        p_handle = &adv_task_data->dataset_extended_handset_handle;
        DEBUG_LOG_LEVEL_2("leAdvertisingManager_GetReferenceToHandleForDataSet Info, Pointer to handle assigned to handset data set is %x handle is %x", p_handle, adv_task_data->dataset_extended_handset_handle);
                
        break;
        
        default:
        
        DEBUG_LOG_LEVEL_1("leAdvertisingManager_GetReferenceToHandleForDataSet Failure, Invalid data set %x", set);
        
        break;
    }
    
    return p_handle;
    
}

/* Local Function to Free the Handle Asssigned to the Given Data Set */
void leAdvertisingManager_FreeHandleForDataSet(le_adv_data_set_t set)
{
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_FreeHandleForDataSet");
        
    le_adv_data_set_handle * p_handle = NULL;
    
    p_handle = leAdvertisingManager_GetReferenceToHandleForDataSet(set);
    PanicNull(p_handle);
    
    DEBUG_LOG_LEVEL_2("leAdvertisingManager_FreeHandleForDataSet Info, Reference to handle is %x, handle is %x, data set is %x", p_handle, *p_handle, set);
    
    free(*p_handle);
    *p_handle = NULL;
    
}

/* Local Function to Retrieve the Task Asssigned to the Given Data Set */
Task leAdvertisingManager_GetTaskForDataSet(le_adv_data_set_t set)
{
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_GetTaskForDataSet");
        
    le_adv_data_set_handle * p_handle = NULL;
        
    Task task = NULL;
    
    p_handle = leAdvertisingManager_GetReferenceToHandleForDataSet(set);
    PanicNull(p_handle);
    
    if(*p_handle)
    {   
        task = (*p_handle)->task;
        DEBUG_LOG_LEVEL_2("leAdvertisingManager_GetTaskForDataSet Info, Task is %x Data set is %x", task, set);        
    }
    else
    {
        DEBUG_LOG_LEVEL_1("leAdvertisingManager_GetTaskForDataSet Failure, No valid handle exists for data set %x", set);        
    }
    
    return task;    
    
}


/* Local Function to Check If there is a handle already created for a given data set */
bool leAdvertisingManager_CheckIfHandleExists(le_adv_data_set_t set)
{
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_CheckIfHandleExists");
       
    le_adv_data_set_handle * p_handle = NULL;
    
    bool ret_val = FALSE;
    
    p_handle = leAdvertisingManager_GetReferenceToHandleForDataSet(set);
    PanicNull(p_handle);
    
    if(*p_handle)
    {   
        DEBUG_LOG_LEVEL_2("leAdvertisingManager_CheckIfHandleExists Info, Handle is %x Data set is %x", *p_handle, set);
        ret_val = TRUE;        
    }
    else
    {
        DEBUG_LOG_LEVEL_2("leAdvertisingManager_CheckIfHandleExists Info, No valid handle exists for data set %x", set);        
    }
    
    return ret_val;
    
}

/* Local Function to Create a New Data Set Handle for a given data set */
le_adv_data_set_handle leAdvertisingManager_CreateNewDataSetHandle(le_adv_data_set_t set)
{
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_CreateNewDataSetHandle");
        
    le_adv_data_set_handle * p_handle = NULL;
    
    p_handle = leAdvertisingManager_GetReferenceToHandleForDataSet(set);
    PanicNull(p_handle);
    
    *p_handle = PanicUnlessMalloc(sizeof(struct _le_adv_data_set));
    (*p_handle)->set = set;
    
    DEBUG_LOG_LEVEL_2("leAdvertisingManager_CreateNewDataSetHandle Info, Reference to handle is %x, handle is %x, data set is %x" , p_handle, *p_handle, set);
    
    return *p_handle;
    
}    

static inline le_adv_blocking_condition_t leAdvertisingManager_ConvertUintToBlockingCondition(uint16 value)
{
    return (le_adv_blocking_condition_t)value;
}

void leAdvertisingManager_SetBlockingCondition(uint16 condition)
{
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();

    DEBUG_LOG_ALWAYS("leAdvertisingManager_SetBlockingCondition enum:le_adv_blocking_condition_t:%u->enum:le_adv_blocking_condition_t:%u",
              leAdvertisingManager_ConvertUintToBlockingCondition(adv_task_data->blockingCondition),
              leAdvertisingManager_ConvertUintToBlockingCondition(condition));

    adv_task_data->blockingCondition = condition;
}

bool leAdvertisingManager_CheckBlockingCondition(uint16 condition)
{
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();

    DEBUG_LOG_ALWAYS("leAdvertisingManager_CheckBlockingCondition Is enum:le_adv_blocking_condition_t:%u Checking enum:le_adv_blocking_condition_t:%u",
              leAdvertisingManager_ConvertUintToBlockingCondition(adv_task_data->blockingCondition),
              leAdvertisingManager_ConvertUintToBlockingCondition(condition));

    return (condition == adv_task_data->blockingCondition);
}

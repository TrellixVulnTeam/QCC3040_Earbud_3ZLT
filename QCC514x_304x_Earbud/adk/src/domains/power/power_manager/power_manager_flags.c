/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Utility functions to manage client flags.

*/

#include "power_manager_flags.h"
#include "power_manager.h"

bool appPowerAllClientsHaveFlagCleared(uint32 flag)
{
    task_list_t *clients = appPowerGetClients();
    Task next_task = NULL;
    task_list_data_t *data = NULL;

    if (TaskList_Size(clients))
    {
        while (TaskList_IterateWithDataRaw(clients, &next_task, &data))
        {
            if (data->u32 & flag)
            {
                return FALSE;
            }
        }
        return TRUE;
    }
    return FALSE;
}

bool appPowerAllClientsHaveFlagSet(uint32 flag)
{
    task_list_t *clients = appPowerGetClients();
    Task next_task = NULL;
    task_list_data_t *data = NULL;

    if (TaskList_Size(clients))
    {
        while (TaskList_IterateWithDataRaw(clients, &next_task, &data))
        {
            if (0 == (data->u32 & flag))
            {
                return FALSE;
            }
        }
    }

    return TRUE;
}

bool appPowerSetFlagInClient(Task task, uint32 flag)
{
    task_list_data_t *data = NULL;
    if (TaskList_GetDataForTaskRaw(appPowerGetClients(), task, &data))
    {
        data->u32 |= flag;
        return TRUE;
    }
    return FALSE;
}

bool appPowerClearFlagInClient(Task task, uint32 flag)
{
    task_list_data_t *data = NULL;
    if (TaskList_GetDataForTaskRaw(appPowerGetClients(), task, &data))
    {
        data->u32 &= ~flag;
        return TRUE;
    }
    return FALSE;
}

void appPowerSetFlagInAllClients(uint32 flag)
{
    task_list_t *clients = appPowerGetClients();
    Task next_task = NULL;
    task_list_data_t *data = NULL;
    while (TaskList_IterateWithDataRaw(clients, &next_task, &data))
    {
        data->u32 |= flag;
    }
}

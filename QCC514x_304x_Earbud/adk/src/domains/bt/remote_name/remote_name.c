/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   remote_name Remote name
\brief      Access to remote device friendly name.


*/

#include "remote_name.h"

#include <device_pskey.h>

#include <bt_device.h>
#include <device_properties.h>
#include <hfp_profile.h>
#include <task_list.h>

#include <vmtypes.h>
#include <message.h>
#include <connection_no_ble.h>
#include <device_list.h>

#include <logging.h>
#include <panic.h>

#include <stdlib.h>

typedef struct
{
    TaskData task_data;
    task_list_t *listeners;
} remote_name_t;

static remote_name_t remote_name;

static void remoteName_PrintName(uint8 *name, uint16 size)
{
    uint16 i;
    for(i = 0; i < size; ++i)
    {
        if(name[i] < 128)
        {
            DEBUG_LOG_VERBOSE("%c", name[i]);
        }
        else
        {
            DEBUG_LOG_VERBOSE("0x%x", name[i]);
        }
    }
}

static void remoteName_SendRequest(device_t device)
{
    bdaddr addr = DeviceProperties_GetBdAddr(device);
    ConnectionReadRemoteName((Task)&remote_name.task_data, &addr);
    DevicePsKey_SetFlag(device, device_pskey_remote_device_name, device_ps_key_flag_new_data_pending);
}

static void remoteName_Set(device_t device, const char *name, uint16 name_size)
{
    /* Maximum name length is 31 characters */
    uint8 num_of_bytes = name_size <= 31 ? name_size : 31;
    uint8 name_buffer[32] = {0};

    /* Make sure that there is 0 at the end of string */
    memcpy(name_buffer, name, num_of_bytes);
    /* Add space for 0 at the end of string */
    num_of_bytes += 1;
    DevicePsKey_Write(device, device_pskey_remote_device_name, name_buffer, num_of_bytes);
}

static void remoteName_MessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch (id)
    {
        case APP_HFP_CONNECTED_IND:
        {
            APP_HFP_CONNECTED_IND_T *msg = (APP_HFP_CONNECTED_IND_T *)message;
            remoteName_SendRequest(BtDevice_GetDeviceForBdAddr(&msg->bd_addr));
        }
        break;
        

        case CL_DM_REMOTE_NAME_COMPLETE:
        {
            CL_DM_REMOTE_NAME_COMPLETE_T *msg = (CL_DM_REMOTE_NAME_COMPLETE_T *)message;

            DEBUG_LOG_VERBOSE("CL_DM_REMOTE_NAME_COMPLETE status 0x%x, lap 0x%x, name len %d",
                    msg->status, msg->bd_addr.lap, msg->size_remote_name);

            device_t device = BtDevice_GetDeviceForBdAddr(&msg->bd_addr);
            DevicePsKey_ClearFlag(device, device_pskey_remote_device_name, device_ps_key_flag_new_data_pending);

            if(msg->status == rnr_success)
            {
                remoteName_PrintName(msg->remote_name, msg->size_remote_name);

                remoteName_Set(device, (char *)msg->remote_name, msg->size_remote_name);
            }
            else if(!DevicePsKey_IsFlagSet(device, device_pskey_remote_device_name, device_ps_key_flag_contains_data))
            {
                /* The was never set */
                DEBUG_LOG_VERBOSE("remoteName_MessageHandler setting empty name");
                const char empty_name[1] = {0};
                remoteName_Set(device, empty_name, 1);
            }

            if(DevicePsKey_IsFlagSet(device, device_pskey_remote_device_name, device_ps_key_flag_contains_data))
            {
                task_list_data_t data = {0};
                Task iter_task = NULL;

                while(TaskList_IterateWithData(remote_name.listeners, &iter_task, &data))
                {
                    if(device == (device_t)data.u32)
                    {
                        REMOTE_NAME_AVAILABLE_IND_T *rn_msg = PanicUnlessMalloc(sizeof(REMOTE_NAME_AVAILABLE_IND_T));
                        rn_msg->device = device;
                        MessageSend(iter_task, REMOTE_NAME_AVAILABLE_IND, rn_msg);
                        TaskList_RemoveTask(remote_name.listeners, iter_task);
                        iter_task = NULL;
                    }
                }
            }
        }
        break;
        default:
            break;
    }
}

bool RemoteName_Init(Task init_task)
{
    UNUSED(init_task);

    remote_name.task_data.handler = remoteName_MessageHandler;
    remote_name.listeners = TaskList_WithDataCreate();

    HfpProfile_RegisterStatusClient((Task)&remote_name.task_data);

    /* Clear run time flags which may have been written to persistent store */
    DevicePsKey_ClearFlagInAllDevices(device_pskey_remote_device_name, device_ps_key_flag_new_data_pending);


    return TRUE;
}

bool RemoteName_IsAvailable(device_t device)
{
    bool contains_data = DevicePsKey_IsFlagSet(device, device_pskey_remote_device_name, device_ps_key_flag_contains_data);
    bool request_in_progress = DevicePsKey_IsFlagSet(device, device_pskey_remote_device_name, device_ps_key_flag_new_data_pending);
    DEBUG_LOG_VERBOSE("RemoteName_IsAvailable %d, contains_data %d, request_in_progress %d ", contains_data && !request_in_progress, contains_data, request_in_progress);
    return contains_data && !request_in_progress;
}

void RemoteName_NotifyWhenAvailable(Task task, device_t device)
{
    task_list_data_t data;
    data.u32 = (uint32)device;
    TaskList_AddTaskWithData(remote_name.listeners, task, &data);
}

const char *RemoteName_Get(device_t device, uint16 *size)
{
    const char *name = (const char *)DevicePsKey_Read(device, device_pskey_remote_device_name, size);

    remoteName_PrintName((uint8 *)name, *size);

    return name;
}

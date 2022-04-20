/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_profile.c
\brief  Implementation of the profile interface for Amazon AVS
*/

#ifdef INCLUDE_AMA
#include <profile_manager.h>
#include <logging.h>
#include <csrtypes.h>
#include <stdlib.h>
#include <panic.h>
#include <task_list.h>
#include <device_properties.h>

#include "ama.h"
#include "ama_connect_state.h"
#include "ama_profile.h"
#include "ama_tws.h"
#include "ama_rfcomm.h"
#include "bt_device.h"

static void amaProfile_ProfileManagerMessageHandler(Task task, MessageId id, Message message);
static const TaskData profile_manager_task = { amaProfile_ProfileManagerMessageHandler };

/*! List of tasks requiring confirmation of AMA disconnect requests */
static task_list_with_data_t disconnect_request_clients;

static bool amaProfile_IsDisconnectRequired(void)
{
    return AmaTws_IsDisconnectRequired() || AmaRfcomm_IsConnected();
}

static void amaProfile_Disconnect(void)
{
    DEBUG_LOG("amaProfile_Disconnect");
    MAKE_AMA_RFCOMM_LOCAL_DISCONNECT_MESSAGE(AMA_RFCOMM_LOCAL_DISCONNECT_REQ);
    message->reason = ama_local_disconnect_reason_normal;
    MessageSend(AmaRfcomm_GetTask(), AMA_RFCOMM_LOCAL_DISCONNECT_REQ, message);
}

static void amaProfile_DisconnectHandler(bdaddr* bd_addr)
{
    DEBUG_LOG("amaProfile_DisconnectHandler");
    PanicNull((bdaddr *)bd_addr);
    device_t device = BtDevice_GetDeviceForBdAddr(bd_addr);
    if (device)
    {
        ProfileManager_AddToNotifyList(TaskList_GetBaseTaskList(&disconnect_request_clients), device);
        if (amaProfile_IsDisconnectRequired())
        {
            amaProfile_Disconnect();
            Ama_TransportDisconnected(AmaData_GetActiveTransport());
        }
        else
        {
            DEBUG_LOG("amaProfile_DisconnectHandler: Already disconnected, send cfm to profile_manager");
            ProfileManager_NotifyConfirmation(TaskList_GetBaseTaskList(&disconnect_request_clients),
                                              bd_addr, profile_manager_success,
                                              profile_manager_ama_profile,
                                              profile_manager_disconnect);
        }
    }
}

void AmaProfile_Init(void)
{
    DEBUG_LOG("AmaProfile_Init");
    TaskList_WithDataInitialise(&disconnect_request_clients);
    ProfileManager_RegisterProfile(profile_manager_ama_profile, NULL, amaProfile_DisconnectHandler);
    ProfileManager_ClientRegister((Task) &profile_manager_task);
}

void AmaProfile_SendConnectedInd(const bdaddr * bd_addr)
{
    DEBUG_LOG("AmaProfile_SendConnectedInd");
    ProfileManager_GenericConnectedInd(profile_manager_ama_profile, bd_addr);
}

void AmaProfile_SendDisconnectedInd(const bdaddr * bd_addr)
{
    DEBUG_LOG("AmaProfile_SendDisconnectedInd");
    if (TaskList_Size(TaskList_GetBaseTaskList(&disconnect_request_clients)) != 0)
    {
        ProfileManager_NotifyConfirmation(TaskList_GetBaseTaskList(&disconnect_request_clients),
                                          bd_addr,
                                          profile_manager_success,
                                          profile_manager_ama_profile,
                                          profile_manager_disconnect);
    }
    ProfileManager_GenericDisconnectedInd(profile_manager_ama_profile, bd_addr, 0);
}

/* On Android, the Alexa app doesn't send a disconnect request when the disconnection is triggered
   from the BT device menu so we piggy back off the A2DP profile disconnect.
   The AMA assistant is useless without an A2DP profile active. */
static void amaProfile_HandleDisconnectedProfileInd(DISCONNECTED_PROFILE_IND_T *ind)
{
    if (ind->profile == DEVICE_PROFILE_A2DP)
    {
        bdaddr addr = DeviceProperties_GetBdAddr(ind->device);

        if (appDeviceTypeIsHandset(&addr))
        {
            DEBUG_LOG("amaProfile_HandleDisconnectedProfileInd: a2dp with %04x %02x %06x", addr.nap, addr.uap, addr.lap);
            if (amaProfile_IsDisconnectRequired())
            {
                amaProfile_Disconnect();
            }
        }
    }
}

static void amaProfile_ProfileManagerMessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch (id)
    {
        case DISCONNECTED_PROFILE_IND:
            amaProfile_HandleDisconnectedProfileInd((DISCONNECTED_PROFILE_IND_T *) message);
            break;

        default:
            break;
    }
}

#endif /* INCLUDE_AMA */


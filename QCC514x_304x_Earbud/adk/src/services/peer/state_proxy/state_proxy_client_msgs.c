/*!
\copyright  Copyright (c) 2005 - 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       state_proxy_client_msgs.c
\brief
*/

/* local includes */
#include "state_proxy.h"
#include "state_proxy_private.h"
#include "state_proxy_marshal_defs.h"
#include "state_proxy_client_msgs.h"

/* framework includes */
#include <phy_state.h>
#include <av.h>
#include <hfp_profile.h>
#include <pairing.h>
#include <connection_manager.h>

/* system includes */
#include <panic.h>
#include <logging.h>
#include <stdlib.h>

static size_t stateProxy_GetEventSpecificSize(state_proxy_event_type type);

/*! \brief Convert state proxy event type to size of corresponding event message.
    \param[in] type Type of state proxy event.
    \return size_t Size of message.
*/
static size_t stateProxy_GetEventSpecificSize(state_proxy_event_type type)
{
    size_t size = 0;

    switch (type)
    {
        case state_proxy_event_type_phystate:
            size = sizeof(PHY_STATE_CHANGED_IND_T);
            break;
        case state_proxy_event_type_is_pairing:
            size = sizeof(PAIRING_ACTIVITY_T);
            break;
        case state_proxy_event_type_battery_state:
            size = sizeof(MESSAGE_BATTERY_REGION_UPDATE_STATE_T);
            break;
        case state_proxy_event_type_battery_voltage:
            size = sizeof(MESSAGE_BATTERY_LEVEL_UPDATE_VOLTAGE_T);
            break;
        case state_proxy_event_type_link_quality:
            size = sizeof(STATE_PROXY_LINK_QUALITY_T);
            break;
        case state_proxy_event_type_mic_quality:
            size = sizeof(STATE_PROXY_MIC_QUALITY_T);
            break;

        case state_proxy_event_type_anc:
            size = sizeof(STATE_PROXY_ANC_DATA_T);
            break;

        case state_proxy_event_type_leakthrough:
            size = sizeof(STATE_PROXY_LEAKTHROUGH_DATA_T);
            break;

        case state_proxy_event_type_aanc_logging:
            size = sizeof(STATE_PROXY_AANC_LOGGING_T);
            break;

        case state_proxy_event_type_aanc:
            size = sizeof(STATE_PROXY_AANC_DATA_T);
            break;

       default:
            DEBUG_LOG("stateProxy_GetEventSpecificSize unknown event type %u", type);
            Panic();
    }

    return size;
}

/*! \brief
    \param
*/
void stateProxy_MsgStateProxyEventClients(state_proxy_source source,
                                          state_proxy_event_type type,
                                          const void* event)
{
    state_proxy_task_data_t *proxy = stateProxy_GetTaskData();
    task_list_data_t data = {0};
    Task task = NULL;
    task_list_t notifiable_task_list;

    SP_LOG_VERBOSE("stateProxy_MsgStateProxyEventClients source %u type %u event %p", source, type, event);

    TaskList_Initialise(&notifiable_task_list);
    /* find registered clients for this event type and send them the message */
    while (TaskList_IterateWithData(proxy->event_tasks, &task, &data))
    {
        if ((data.u32 & type) == type)
        {
            TaskList_AddTask(&notifiable_task_list, task);
        }
    }
    if(TaskList_Size(&notifiable_task_list) > 0)
    {
        MAKE_STATE_PROXY_MESSAGE(STATE_PROXY_EVENT);
        message->source = source;
        message->type = type;
        message->timestamp = VmGetClock();
        /* event may be NULL for message without payload */
        if (event)
        {
            size_t event_msg_size_specific = stateProxy_GetEventSpecificSize(type);
            memcpy(&message->event, event, event_msg_size_specific);
        }
        TaskList_MessageSendWithSize(&notifiable_task_list, STATE_PROXY_EVENT, message, sizeof(*message));
        TaskList_RemoveAllTasks(&notifiable_task_list);
    }
}

bool stateProxy_AnyClientsRegisteredForEvent(state_proxy_event_type type)
{
    state_proxy_task_data_t *proxy = stateProxy_GetTaskData();
    task_list_data_t data = {0};
    Task task = NULL;
    bool registered = FALSE;

    /* find registered clients for this event type and send them the message */
    while (TaskList_IterateWithData(proxy->event_tasks, &task, &data))
    {
        if ((data.u32 & type) == type)
        {
            registered = TRUE;
            break;
        }
    }
    return registered;
}

void stateProxy_MsgStateProxyEventInitialStateSent(void)
{
    TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(stateProxy_GetEvents()), STATE_PROXY_EVENT_INITIAL_STATE_SENT);
}

void stateProxy_MsgStateProxyEventInitialStateReceived(void)
{
    TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(stateProxy_GetEvents()), STATE_PROXY_EVENT_INITIAL_STATE_RECEIVED);
}


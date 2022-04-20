/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       headset_phy_state.c
\brief      Manage physical state of a Headset. This is a skeleton module and not fully functional.
*/

#include "headset_phy_state.h"
#include "proximity.h"

#include <task_list.h>
#include <panic.h>
#include <logging.h>

headsetPhyStateTaskData app_headset_phy_state;

/*! Message creation macro for headset phyiscal state module. */
#define MAKE_HEADSET_PHYSTATE_MESSAGE(TYPE) TYPE##_T *message = PanicUnlessNew(TYPE##_T);

#ifndef HOSTED_TEST_ENVIRONMENT
/*! There is checking that the messages assigned by this module do
not overrun into the next module's message ID allocation */
ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(HEADSET_PHY_STATE, HEADSET_PHY_STATE_MESSAGE_END)

#endif

/*! \brief Send a PHY_STATE_CHANGED_IND message to all registered client tasks.
 */
static void appHeadsetPhyStateMsgSendStateChangedInd(headsetPhyState state, headset_phy_state_event event)
{
    MAKE_HEADSET_PHYSTATE_MESSAGE(HEADSET_PHY_STATE_CHANGED_IND);
    message->new_state = state;
    message->event = event;

    TaskList_MessageSend(TaskList_GetFlexibleBaseTaskList(HeadsetPhyStateGetClientTasks()),
        HEADSET_PHY_STATE_CHANGED_IND, message);
}

/*! \brief Perform actions on exiting HEADSET_PHY_STATE_UNKNOWN state. */
static void appHeadsetPhyStateExitUnknown(void)
{
    DEBUG_LOG_STATE("appHeadsetPhyStateExitUnknown");
}

/*! \brief Perform actions on exiting HEADSET_PHY_STATE_OFF_HEAD state. */
static void appHeadsetPhyStateExitOffHead(void)
{
    DEBUG_LOG_STATE("appHeadsetPhyStateExitOffHead");
}

/*! \brief Perform actions on exiting HEADSET_PHY_STATE_OFF_HEAD_AT_REST state. */
static void appHeadsetPhyStateExitOffHeadAtRest(void)
{
    DEBUG_LOG_STATE("appHeadsetPhyStateExitOffHeadAtRest");
}

/*! \brief Perform actions on exiting HEADSET_PHY_STATE_ON_HEAD state. */
static void appHeadsetPhyStateExitOnHead(void)
{
    DEBUG_LOG_STATE("appHeadsetPhyStateExitOnHead");
    appHeadsetPhyStateMsgSendStateChangedInd(HEADSET_PHY_STATE_OFF_HEAD, headset_phy_state_event_in_motion);
}

static void appHeadsetPhyStateHandleBadState(headsetPhyState phy_state)
{
    UNUSED(phy_state);
    DEBUG_LOG("appHeadsetPhyStateHandleBadState %d", phy_state);
    Panic();
}

/*! \brief Perform actions on entering HEADSET_PHY_STATE_OFF_HEAD state. */
static void appHeadsetPhyStateEnterOffHead(void)
{
    DEBUG_LOG("appHeadsetPhyStateEnterOffHead");
    appHeadsetPhyStateMsgSendStateChangedInd(HEADSET_PHY_STATE_OFF_HEAD, headset_phy_state_event_off_head);
}

/*! \brief Perform actions on entering HEADSET_PHY_STATE_OFF_HEAD_AT_REST state. */
static void appHeadsetPhyStateEnterOffHeadAtRest(void)
{
    DEBUG_LOG("appHeadsetPhyStateEnterOffHeadAtRest");
    appHeadsetPhyStateMsgSendStateChangedInd(HEADSET_PHY_STATE_OFF_HEAD_AT_REST, headset_phy_state_event_not_in_motion);
}

/*! \brief Perform actions on entering HEADSET_PHY_STATE_ON_HEAD state. */
static void appHeadsetPhyStateEnterOnHead(void)
{
    DEBUG_LOG("appHeadsetPhyStateEnterOnHead");
    appHeadsetPhyStateMsgSendStateChangedInd(HEADSET_PHY_STATE_ON_HEAD, headset_phy_state_event_on_head);
}

static void appHeadsetPhyStateSetState(headsetPhyStateTaskData* phy_state, headsetPhyState new_state)
{
    DEBUG_LOG_STATE("appHeadsetPhyStateSetState current %d reported %d new %d", phy_state->state, phy_state->reported_state, new_state);

    /* always update true state of the device */
    HeadsetPhyStateGetTaskData()->state = new_state;

    /* The state machine reflects what has been reported to clients, so transitions are
     * based on reported_state not state */
    switch (phy_state->reported_state)
    {
        case HEADSET_PHY_STATE_UNKNOWN:
            appHeadsetPhyStateExitUnknown();
            break;
        case HEADSET_PHY_STATE_OFF_HEAD:
            appHeadsetPhyStateExitOffHead();
            break;
        case HEADSET_PHY_STATE_OFF_HEAD_AT_REST:
            appHeadsetPhyStateExitOffHeadAtRest();
            break;
        case HEADSET_PHY_STATE_ON_HEAD:
            appHeadsetPhyStateExitOnHead();
            break;
        default:
            appHeadsetPhyStateHandleBadState(phy_state->reported_state);
            break;
    }

    if ((phy_state->reported_state == HEADSET_PHY_STATE_UNKNOWN) || (phy_state->reported_state == HEADSET_PHY_STATE_OFF_HEAD))
    {
        phy_state->reported_state = new_state;
    }
    else
    {
        phy_state->reported_state = HEADSET_PHY_STATE_OFF_HEAD;
    }

    switch (phy_state->reported_state)
    {
        case HEADSET_PHY_STATE_OFF_HEAD:
            appHeadsetPhyStateEnterOffHead();
            break;
        case HEADSET_PHY_STATE_OFF_HEAD_AT_REST:
            appHeadsetPhyStateEnterOffHeadAtRest();
            break;
        case HEADSET_PHY_STATE_ON_HEAD:
            appHeadsetPhyStateEnterOnHead();
            break;
        default:
            appHeadsetPhyStateHandleBadState(phy_state->reported_state);
            break;
    }

}

static void appHeadsetPhyStateHandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);

    switch (id)
    {
        default:
            DEBUG_LOG("Unknown message received 0x%x", id);
            break;
    }
}

void appHeadsetPhyStateRegisterClient(Task client_task)
{
    DEBUG_LOG("appHeadsetPhyStateRegisterClient %p", client_task);

    TaskList_AddTask(TaskList_GetFlexibleBaseTaskList(HeadsetPhyStateGetClientTasks()), client_task);
}

void appHeadsetPhyStateUnregisterClient(Task client_task)
{
    DEBUG_LOG("appHeadsetPhyStateUnregisterClient %p", client_task);

    TaskList_RemoveTask(TaskList_GetFlexibleBaseTaskList(HeadsetPhyStateGetClientTasks()), client_task);
}

headsetPhyState appHeadsetPhyStateGetState(void)
{
    headsetPhyStateTaskData* phy_state = HeadsetPhyStateGetTaskData();
    DEBUG_LOG("appHeadsetPhyStateGetState: %d", phy_state->state);
    return phy_state->state;
}

bool appHeadsetPhyStateIsOnHeadDetectionSupported(void)
{
    bool supported = appProximityIsClientRegistered(&(HeadsetPhyStateGetTaskData()->task));
    DEBUG_LOG("appHeadsetPhyStateIsOnHeadDetectionSupported: %d", supported);
    return supported;
}

void appHeadsetPhyStateOnHeadEvent(void)
{
    DEBUG_LOG("appHeadsetPhyStateOnHeadEvent");
    headsetPhyStateTaskData *phy_state = HeadsetPhyStateGetTaskData();
    if (phy_state->state != HEADSET_PHY_STATE_ON_HEAD)
    {
        appHeadsetPhyStateSetState(HeadsetPhyStateGetTaskData(), HEADSET_PHY_STATE_ON_HEAD);
    }
}

void appHeadsetPhyStateOffHeadEvent(void)
{
    DEBUG_LOG("appHeadsetPhyStateOffHeadEvent");
    headsetPhyStateTaskData *phy_state = HeadsetPhyStateGetTaskData();
    if (phy_state->state != HEADSET_PHY_STATE_OFF_HEAD)
    {
        appHeadsetPhyStateSetState(HeadsetPhyStateGetTaskData(), HEADSET_PHY_STATE_OFF_HEAD);
    }
}

void appHeadsetPhyStateMotionEvent(void)
{
    DEBUG_LOG("appHeadsetPhyStateMotionEvent");
    headsetPhyStateTaskData* phy_state = HeadsetPhyStateGetTaskData();
    phy_state->in_motion = TRUE;
}

void appHeadsetPhyStateNotInMotionEvent(void)
{
    DEBUG_LOG("appHeadsetPhyStateNotInMotionEvent");
    headsetPhyStateTaskData* phy_state = HeadsetPhyStateGetTaskData();
    phy_state->in_motion = FALSE;
}

void appHeadsetPhyStatePrepareToEnterDormant(void)
{
    DEBUG_LOG("appHeadsetPhyStatePrepareToEnterDormant");
}

bool appHeadsetPhyStateInit(Task init_task)
{
    DEBUG_LOG("appHeadsetPhyStateInit(%p)", init_task);
    headsetPhyStateTaskData* phy_state = HeadsetPhyStateGetTaskData();
    phy_state->task.handler = appHeadsetPhyStateHandleMessage;
    phy_state->reported_state = HEADSET_PHY_STATE_UNKNOWN;
    TaskList_InitialiseWithCapacity(HeadsetPhyStateGetClientTasks(), HEADSET_PHY_STATE_CLIENT_TASK_LIST_INIT_CAPACITY);
    phy_state->in_motion = FALSE;
    phy_state->in_proximity = FALSE;
    MessageSend(init_task, HEADSET_PHY_STATE_INIT_CFM, NULL);
    return TRUE;
}


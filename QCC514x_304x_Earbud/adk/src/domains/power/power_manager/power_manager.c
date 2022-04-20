/*!
\copyright  Copyright (c) 2008 - 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Power Management
*/

#include "power_manager.h"
#include "power_manager_conditions.h"
#include "power_manager_flags.h"
#include "power_manager_sm.h"
#include "power_manager_monitors.h"
#include "power_manager_performance.h"
#include "power_manager_ui.h"

#include <task_list.h>
#include <logging.h>

#include <message.h>
#include <panic.h>

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_ENUM(powerClientMessages)

#ifndef HOSTED_TEST_ENVIRONMENT

/*! There is checking that the messages assigned by this module do
not overrun into the next module's message ID allocation */
ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(POWER_APP, POWER_APP_MESSAGE_END)

#endif

/*!< Information for power_control */
powerTaskData   app_power;

bool appPowerInit(Task init_task)
{
    powerTaskData *thePower = PowerGetTaskData();

    UNUSED(init_task);

    memset(thePower, 0, sizeof(*thePower));

    thePower->task.handler = appPowerHandleMessage;
    thePower->allow_dormant = TRUE;
    thePower->user_initiated_shutdown = FALSE;
    thePower->performance_req_count = 0;
    thePower->state = POWER_STATE_INIT;

    TaskList_WithDataInitialise(&thePower->clients);

    appPowerPerformanceInit();
    appPowerRegisterMonitors();
    appPowerRegisterWithUi();

    return TRUE;
}

void appPowerInitComplete(void)
{
    PowerGetTaskData()->init_complete = TRUE;

    /* kick event handler to see if sleep is necessary now */
    appPowerHandlePowerEvent();
}

void appPowerOn(void)
{
    DEBUG_LOG("appPowerOn");

    TaskList_MessageSendId(appPowerGetClients(), POWER_ON);
}

bool appPowerOffRequest(void)
{
    DEBUG_LOG("appPowerOffRequest");
    if (appPowerCanPowerOff())
    {
        switch (PowerGetTaskData()->state)
        {
            case POWER_STATE_INIT:
                /* Cannot power off during initialisation */
                break;
            case POWER_STATE_OK:
                PowerGetTaskData()->user_initiated_shutdown = TRUE;
                appPowerSetState(POWER_STATE_TERMINATING_CLIENTS_NOTIFIED);
                return TRUE;
            case POWER_STATE_TERMINATING_CLIENTS_NOTIFIED:
            case POWER_STATE_TERMINATING_CLIENTS_RESPONDED:
                /* Already shutting down, accept the request, but do nothing. */
                return TRUE;
            case POWER_STATE_SOPORIFIC_CLIENTS_NOTIFIED:
            case POWER_STATE_SOPORIFIC_CLIENTS_RESPONDED:
                /* Cannot power off when already entering sleep */
                break;
        }
    }
    return FALSE;
}

void appPowerClientRegister(Task task)
{
    task_list_data_t data = {.u32 = 0};

    if (TaskList_AddTaskWithData(appPowerGetClients(), task, &data))
    {
        switch (PowerGetTaskData()->state)
        {
            case POWER_STATE_INIT:
                /* Cannot register during initialisation */
                Panic();
                break;
            case POWER_STATE_OK:
                break;
            case POWER_STATE_TERMINATING_CLIENTS_NOTIFIED:
                /* Notify the new client  */
                appPowerSetFlagInClient(task, APP_POWER_SHUTDOWN_PREPARE_RESPONSE_PENDING);
                MessageSend(task, APP_POWER_SHUTDOWN_PREPARE_IND, NULL);
                break;
            case POWER_STATE_TERMINATING_CLIENTS_RESPONDED:
                /* Already shutting down, the new client will not be informed of the
                shutdown, unless it is cancelled */
                break;
            case POWER_STATE_SOPORIFIC_CLIENTS_NOTIFIED:
                /* Notify the new client  */
                appPowerSetFlagInClient(task, APP_POWER_SLEEP_PREPARE_RESPONSE_PENDING);
                MessageSend(task, APP_POWER_SLEEP_PREPARE_IND, NULL);
                break;
            case POWER_STATE_SOPORIFIC_CLIENTS_RESPONDED:
                /* This should never happen, since entering this state results in
                the chip going to sleep. */
                break;
        }
        DEBUG_LOG("appPowerClientRegister %p registered", task);
    }
    else
    {
        DEBUG_LOG("appPowerClientRegister %p already registered (ignoring)", task);
    }
}

void appPowerClientUnregister(Task task)
{
    DEBUG_LOG("appPowerClientUnregister %p", task);

    /* Tidy up any outstanding actions the client may have. */
    appPowerClientAllowSleep(task);
    appPowerShutdownPrepareResponse(task);
    appPowerSleepPrepareResponse(task);

    MessageCancelAll(task, APP_POWER_SLEEP_PREPARE_IND);
    MessageCancelAll(task, APP_POWER_SLEEP_CANCELLED_IND);
    MessageCancelAll(task, APP_POWER_SHUTDOWN_PREPARE_IND);
    MessageCancelAll(task, APP_POWER_SHUTDOWN_CANCELLED_IND);

    TaskList_RemoveTask(appPowerGetClients(), task);
}

void appPowerClientAllowSleep(Task task)
{
    DEBUG_LOG("appPowerClientAllowSleep %p", task);

    appPowerSetFlagInClient(task, APP_POWER_ALLOW_SLEEP);

    if (POWER_STATE_OK == PowerGetTaskData()->state)
    {
        if (appPowerAllClientsHaveFlagSet(APP_POWER_ALLOW_SLEEP))
        {
            if (appPowerCanSleep())
            {
                appPowerSetState(POWER_STATE_SOPORIFIC_CLIENTS_NOTIFIED);
            }
        }
    }
}

void appPowerClientProhibitSleep(Task task)
{
    DEBUG_LOG("appPowerClientProhibitSleep %p", task);

    appPowerClearFlagInClient(task, APP_POWER_ALLOW_SLEEP);
}

void appPowerShutdownPrepareResponse(Task task)
{
    DEBUG_LOG("appPowerShutdownPrepareResponse 0x%x %p", PowerGetTaskData()->state, task);

    if (POWER_STATE_TERMINATING_CLIENTS_NOTIFIED == PowerGetTaskData()->state)
    {
        if (appPowerClearFlagInClient(task, APP_POWER_SHUTDOWN_PREPARE_RESPONSE_PENDING))
        {
            if (appPowerAllClientsHaveFlagCleared(APP_POWER_SHUTDOWN_PREPARE_RESPONSE_PENDING))
            {
                appPowerSetState(POWER_STATE_TERMINATING_CLIENTS_RESPONDED);
            }
        }
    }
    // Ignore response in wrong state.
}

void appPowerSleepPrepareResponse(Task task)
{
    DEBUG_LOG("appPowerSleepPrepareResponse 0x%x %p", PowerGetTaskData()->state, task);

    if (POWER_STATE_SOPORIFIC_CLIENTS_NOTIFIED == PowerGetTaskData()->state)
    {
        if (appPowerClearFlagInClient(task, APP_POWER_SLEEP_PREPARE_RESPONSE_PENDING))
        {
            if (appPowerAllClientsHaveFlagCleared(APP_POWER_SLEEP_PREPARE_RESPONSE_PENDING))
            {
                appPowerSetState(POWER_STATE_SOPORIFIC_CLIENTS_RESPONDED);
            }
        }
    }
    // Ignore response in wrong state.
}

bool appPowerIsShuttingDown(void)
{
    powerState state = PowerGetTaskData()->state;
    return (state == POWER_STATE_TERMINATING_CLIENTS_NOTIFIED ||
            state == POWER_STATE_TERMINATING_CLIENTS_RESPONDED);
}

/*! \brief returns power task pointer to requesting component

    \return power task pointer.
*/
Task PowerGetTask(void)
{
    return &app_power.task;
}


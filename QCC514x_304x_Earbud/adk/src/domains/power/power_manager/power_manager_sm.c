/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Power manager state machine

*/

#include "power_manager_sm.h"
#include "power_manager.h"
#include "power_manager_conditions.h"
#include "power_manager_flags.h"

#include "system_state.h"

#include <logging.h>
#include <task_list.h>
#include <message.h>
#include <panic.h>

static void appPowerExitInit(void)
{
    DEBUG_LOG("appPowerExitPowerStateInit");
    MessageSend(SystemState_GetTransitionTask(), APP_POWER_INIT_CFM, NULL);
}

static void appPowerEnterPowerStateOk(void)
{
    DEBUG_LOG("appPowerEnterPowerStateOk");
    PowerGetTaskData()->user_initiated_shutdown = FALSE;
}

static void appPowerExitPowerStateOk(void)
{
    DEBUG_LOG("appPowerExitPowerStateOk");
}

static void appPowerEnterPowerStateTerminatingClientsNotified(void)
{
    DEBUG_LOG("appPowerEnterPowerStateTerminatingClientsNotified");
    if (TaskList_Size(appPowerGetClients()))
    {
        TaskList_MessageSendId(appPowerGetClients(), APP_POWER_SHUTDOWN_PREPARE_IND);
        appPowerSetFlagInAllClients(APP_POWER_SHUTDOWN_PREPARE_RESPONSE_PENDING);

        TaskList_MessageSendId(appPowerGetClients(), POWER_OFF);
    }
    else
    {
        appPowerSetState(POWER_STATE_TERMINATING_CLIENTS_RESPONDED);
    }
}

static void appPowerExitPowerStateTerminatingClientsNotified(void)
{
    DEBUG_LOG("appPowerExitPowerStateTerminatingClientsNotified");
}

static void appPowerEnterPowerStateTerminatingClientsResponded(void)
{
    DEBUG_LOG("appPowerEnterPowerStateTerminatingClientsResponded");

    if (appPowerNeedsToPowerOff())
    {
        if(PowerGetTaskData()->user_initiated_shutdown)
        {
            SystemState_Shutdown();
        }
        else
        {
            SystemState_EmergencyShutdown();
        }
        // Usually does not return
    }
    appPowerSetState(POWER_STATE_OK);
}

/*! \brief Exiting means shutdown was aborted, tell clients */
static void appPowerExitPowerStateTerminatingClientsResponded(void)
{
    DEBUG_LOG("appPowerExitPowerStateTerminatingClientsResponded");
    TaskList_MessageSendId(appPowerGetClients(), APP_POWER_SHUTDOWN_CANCELLED_IND);
}

static void appPowerEnterPowerStateSoporificClientsNotified(void)
{
    DEBUG_LOG("appPowerEnterPowerStateSoporificClientsNotified");

    if (TaskList_Size(appPowerGetClients()))
    {
        TaskList_MessageSendId(appPowerGetClients(), APP_POWER_SLEEP_PREPARE_IND);
        appPowerSetFlagInAllClients(APP_POWER_SLEEP_PREPARE_RESPONSE_PENDING);
    }
    else
    {
        appPowerSetState(POWER_STATE_SOPORIFIC_CLIENTS_RESPONDED);
    }
}

static void appPowerExitPowerStateSoporificClientsNotified(void)
{
    DEBUG_LOG("appPowerExitPowerStateSoporificClientsNotified");
}

/*! At this point, power has sent #POWER_SLEEP_PREPARE_IND to all clients.
    All clients have responsed. Double check sleep is possible. If so sleep, if not
    return to ok. */
static void appPowerEnterPowerStateSoporificClientsResponded(void)
{
    DEBUG_LOG("appPowerEnterPowerStateSoporificClientsResponded");
    if (appPowerCanSleep())
    {
        SystemState_Sleep();
    }
    else
    {
        appPowerSetState(POWER_STATE_OK);
    }
}

/*! \brief Exiting means sleep was aborted, tell clients */
static void appPowerExitPowerStateSoporificClientsResponded(void)
{
    DEBUG_LOG("appPowerExitPowerStateSoporificClientsResponded");
    TaskList_MessageSendId(appPowerGetClients(), APP_POWER_SLEEP_CANCELLED_IND);
}

void appPowerSetState(powerState new_state)
{
    powerState current_state = PowerGetTaskData()->state;

    // Handle exiting states
    switch (current_state)
    {
        case POWER_STATE_INIT:
            appPowerExitInit();
            break;
        case POWER_STATE_OK:
            appPowerExitPowerStateOk();
            break;
        case POWER_STATE_TERMINATING_CLIENTS_NOTIFIED:
            appPowerExitPowerStateTerminatingClientsNotified();
            break;
        case POWER_STATE_TERMINATING_CLIENTS_RESPONDED:
            appPowerExitPowerStateTerminatingClientsResponded();
            break;
        case POWER_STATE_SOPORIFIC_CLIENTS_NOTIFIED:
            appPowerExitPowerStateSoporificClientsNotified();
            break;
        case POWER_STATE_SOPORIFIC_CLIENTS_RESPONDED:
            appPowerExitPowerStateSoporificClientsResponded();
            break;
        default:
            Panic();
            break;
    }

    PowerGetTaskData()->state = new_state;

    // Handle entering states
    switch (new_state)
    {
        case POWER_STATE_OK:
            appPowerEnterPowerStateOk();
            break;
        case POWER_STATE_TERMINATING_CLIENTS_NOTIFIED:
            appPowerEnterPowerStateTerminatingClientsNotified();
            break;
        case POWER_STATE_TERMINATING_CLIENTS_RESPONDED:
            appPowerEnterPowerStateTerminatingClientsResponded();
            break;
        case POWER_STATE_SOPORIFIC_CLIENTS_NOTIFIED:
            appPowerEnterPowerStateSoporificClientsNotified();
            break;
        case POWER_STATE_SOPORIFIC_CLIENTS_RESPONDED:
            appPowerEnterPowerStateSoporificClientsResponded();
            break;
        default:
            Panic();
            break;
    }
}

void appPowerHandlePowerEvent(void)
{
    switch (PowerGetTaskData()->state)
    {
        case POWER_STATE_INIT:
            if (appPowerNeedsToPowerOff())
            {
                SystemState_Shutdown();
                // Does not return
            }
            appPowerSetState(POWER_STATE_OK);
        break;

        case POWER_STATE_OK:
            if (appPowerNeedsToPowerOff())
            {
                DEBUG_LOG("appPowerHandlePowerEvent need to shutdown");
                appPowerSetState(POWER_STATE_TERMINATING_CLIENTS_NOTIFIED);
            }
            else if (appPowerAllClientsHaveFlagSet(APP_POWER_ALLOW_SLEEP))
            {
                if (appPowerCanSleep())
                {
                    DEBUG_LOG("appPowerHandlePowerEvent can sleep");
                    appPowerSetState(POWER_STATE_SOPORIFIC_CLIENTS_NOTIFIED);
                }
                else
                {
                    DEBUG_LOG("appPowerHandlePowerEvent insomniac");
                }
            }
        break;

        default:
        break;
    }
}

/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Transitions between power states.

*/

#include "system_state.h"

#include <task_list.h>
#include <logging.h>
#include <vmtypes.h>
#include <panic.h>

#include <string.h>

LOGGING_PRESERVE_MESSAGE_TYPE(system_state_messages_t)

typedef struct
{
    const system_state_step_t *transition_table;
    unsigned transition_table_count;
} transition_table_t;

typedef struct
{
    transition_table_t *transition;
    unsigned index;
    TaskData task;
    system_state_state_t state_on_completition;
} system_state_current_transition_t;

typedef struct
{
    system_state_state_t state;
    TaskData task;
    task_list_t *state_change_listeners;
    bool is_limbo_removed;

    system_state_current_transition_t current_transition;

    transition_table_t init;
    transition_table_t startup;
    transition_table_t power_on;
    transition_table_t power_off;
    transition_table_t sleep;
    transition_table_t shutdown;
    transition_table_t emergency_shutdown;
} system_state_ctx_t;

system_state_ctx_t system_state_ctx;

void SystemState_Init(void)
{
    DEBUG_LOG_VERBOSE("SystemState_Init");

    memset(&system_state_ctx, 0, sizeof(system_state_ctx));

    system_state_ctx.state_change_listeners = TaskList_CreateWithCapacity(2);

    system_state_ctx.state = system_state_powered_off;
}

void SystemState_RemoveLimboState(void)
{
    system_state_ctx.is_limbo_removed = TRUE;
}

static void systemState_RegisterTable(transition_table_t *table, const system_state_step_t *transition_table, unsigned transition_table_count)
{
    table->transition_table = transition_table;
    table->transition_table_count = transition_table_count;
}

void SystemState_RegisterTableForInitialise(const system_state_step_t *transition_table, unsigned transition_table_count)
{
    DEBUG_LOG_VERBOSE("SystemState_RegisterTableForInitialise, table size %d", transition_table_count);

    systemState_RegisterTable(&system_state_ctx.init, transition_table, transition_table_count);
}

void SystemState_RegisterTableForStartUp(const system_state_step_t *transition_table, unsigned transition_table_count)
{
    DEBUG_LOG_VERBOSE("SystemState_RegisterTableForStartUp, table size %d", transition_table_count);

    systemState_RegisterTable(&system_state_ctx.startup, transition_table, transition_table_count);
}

void SystemState_RegisterTableForPowerOn(const system_state_step_t *transition_table, unsigned transition_table_count)
{
    DEBUG_LOG_VERBOSE("SystemState_RegisterTableForPowerOn, table size %d", transition_table_count);

    systemState_RegisterTable(&system_state_ctx.power_on, transition_table, transition_table_count);
}

void SystemState_RegisterTableForPowerOff(const system_state_step_t *transition_table, unsigned transition_table_count)
{
    DEBUG_LOG_VERBOSE("SystemState_RegisterTableForPowerOff, table size %d", transition_table_count);

    systemState_RegisterTable(&system_state_ctx.power_off, transition_table, transition_table_count);
}

void SystemState_RegisterTableForSleep(const system_state_step_t *transition_table, unsigned transition_table_count)
{
    DEBUG_LOG_VERBOSE("SystemState_RegisterTableForSleep, table size %d", transition_table_count);

    systemState_RegisterTable(&system_state_ctx.sleep, transition_table, transition_table_count);
}

void SystemState_RegisterTableForShutdown(const system_state_step_t *transition_table, unsigned transition_table_count)
{
    DEBUG_LOG_VERBOSE("SystemState_RegisterTableForShutdown, table size %d", transition_table_count);

    systemState_RegisterTable(&system_state_ctx.shutdown, transition_table, transition_table_count);
}

void SystemState_RegisterTableForEmergencyShutdown(const system_state_step_t *transition_table, unsigned transition_table_count)
{
    DEBUG_LOG_VERBOSE("SystemState_RegisterTableForEmergencyShutdown, table size %d", transition_table_count);

    systemState_RegisterTable(&system_state_ctx.emergency_shutdown, transition_table, transition_table_count);
}


static void systemState_SetState(system_state_state_t new_state)
{
    MESSAGE_MAKE(msg, SYSTEM_STATE_STATE_CHANGE_T);
    msg->old_state = system_state_ctx.state;
    msg->new_state = new_state;

    DEBUG_LOG_STATE("systemState_SetState, old 0x%x, new 0x%x", msg->old_state, msg->new_state);

    system_state_ctx.state = new_state;

    TaskList_MessageSendWithSize(system_state_ctx.state_change_listeners, SYSTEM_STATE_STATE_CHANGE, msg, sizeof(SYSTEM_STATE_STATE_CHANGE_T));
}

static void systemState_CompleteTransition(transition_table_t *table, bool success)
{
    UNUSED(success);
    UNUSED(table);
    systemState_SetState(system_state_ctx.current_transition.state_on_completition);
}

static void systemState_DoTransition(transition_table_t *table)
{
    ++system_state_ctx.current_transition.index;

    while(system_state_ctx.current_transition.index < table->transition_table_count)
    {
        table->transition_table[system_state_ctx.current_transition.index].step((Task)&system_state_ctx.current_transition.task);

        if(table->transition_table[system_state_ctx.current_transition.index].async_message_id)
        {
            return;
        }

        ++system_state_ctx.current_transition.index;
    }

    systemState_CompleteTransition(table, TRUE);
}

static void systemState_MessageHandler(Task task, MessageId id, Message msg)
{
    transition_table_t *transition = system_state_ctx.current_transition.transition;

    UNUSED(task);

    if (transition->transition_table == NULL)
    {
        DEBUG_LOG_ERROR("systemState_MessageHandler ERROR: received MESSAGE:0x%x, transition 0x%x table is NULL", id, transition);
        return;
    }

    unsigned index = system_state_ctx.current_transition.index;
    MessageId expected_id = transition->transition_table[index].async_message_id;
    step_handler handler = transition->transition_table[index].async_handler;

    if(id != expected_id)
    {
        DEBUG_LOG_ERROR("systemState_MessageHandler ERROR: received message MESSAGE:0x%x at transition index 0x%x, but expected 0x%x message",
                id, index, expected_id);
        return;
    }

    if(handler)
    {
        handler(msg);
    }

    systemState_DoTransition(transition);
}

void SystemState_RegisterForStateChanges(Task listener)
{
    DEBUG_LOG_VERBOSE("SystemState_RegisterForStateChanges task: 0x%x", listener);

    TaskList_AddTask(system_state_ctx.state_change_listeners, listener);
}

system_state_state_t SystemState_GetState(void)
{
    DEBUG_LOG_VERBOSE("SystemState_GetState state: 0x%x", system_state_ctx.state);

    return system_state_ctx.state;
}

Task SystemState_GetTransitionTask(void)
{
    return (Task)&system_state_ctx.current_transition.task;
}

static void systemState_SetupCurrentTransition(transition_table_t *table, system_state_state_t state_on_completition)
{
    system_state_ctx.current_transition.transition = table;
    system_state_ctx.current_transition.index = -1;
    system_state_ctx.current_transition.task.handler = systemState_MessageHandler;
    system_state_ctx.current_transition.state_on_completition = state_on_completition;
}

static bool systemState_InitiateTransiton(system_state_state_t start_state,
                                    system_state_state_t transitional_state,
                                    system_state_state_t end_state,
                                    transition_table_t *table)
{
    if(!(SystemState_GetState() & start_state))
    {
        DEBUG_LOG_WARN("systemState_InitiateTransiton, transition rejected because system state 0x%x is not in 0x%x",
                SystemState_GetState(), start_state);
        return FALSE;
    }

    systemState_SetState(transitional_state);

    systemState_SetupCurrentTransition(table, end_state);

    systemState_DoTransition(table);

    return TRUE;
}

bool SystemState_Initialise(void)
{
    DEBUG_LOG_STATE("SystemState_Initialise");

    return systemState_InitiateTransiton(system_state_powered_off,
            system_state_initialisation, system_state_initialised, &system_state_ctx.init);
}

bool SystemState_StartUp(void)
{
    DEBUG_LOG_STATE("SystemState_StartUp");

    system_state_state_t dest_state = system_state_ctx.is_limbo_removed ? system_state_active : system_state_limbo;

    return systemState_InitiateTransiton(system_state_initialised,
            system_state_starting_up, dest_state, &system_state_ctx.startup);
}

bool SystemState_PowerOn(void)
{
    DEBUG_LOG_STATE("SystemState_PowerOn");

    if(system_state_ctx.is_limbo_removed)
    {
       return FALSE;
    }

    return systemState_InitiateTransiton(system_state_powering_off | system_state_limbo,
            system_state_powering_on, system_state_active, &system_state_ctx.power_on);
}

bool SystemState_PowerOff(void)
{
    DEBUG_LOG_STATE("SystemState_PowerOff");

    if(system_state_ctx.is_limbo_removed)
    {
        return FALSE;
    }

    return systemState_InitiateTransiton(system_state_powering_on | system_state_active,
            system_state_powering_off, system_state_limbo, &system_state_ctx.power_off);
}

bool SystemState_Sleep(void)
{
    DEBUG_LOG_STATE("SystemState_Sleep");

    system_state_state_t start_state = system_state_ctx.is_limbo_removed ? system_state_active : system_state_limbo;

    return systemState_InitiateTransiton(start_state,
                system_state_going_to_sleep, system_state_sleeping, &system_state_ctx.sleep);
}

bool SystemState_Shutdown(void)
{
    DEBUG_LOG_STATE("SystemState_Shutdown");

    system_state_state_t start_state = system_state_ctx.is_limbo_removed ? system_state_active : system_state_limbo;

    return systemState_InitiateTransiton(start_state,
            system_state_shutting_down, system_state_powered_off, &system_state_ctx.shutdown);
}

bool SystemState_EmergencyShutdown(void)
{
    DEBUG_LOG_STATE("SystemState_EmergencyShutdown");

    systemState_SetState(system_state_shutting_down_promptly);

    systemState_SetupCurrentTransition(&system_state_ctx.emergency_shutdown, system_state_powered_off);

    systemState_DoTransition(&system_state_ctx.emergency_shutdown);

    return TRUE;
}

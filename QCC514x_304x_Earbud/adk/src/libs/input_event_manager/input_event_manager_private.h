/*
Copyright (c) 2018-2021  Qualcomm Technologies International, Ltd.
*/

#ifndef INPUT_EVENT_MANAGER_PRIVATE_H_
#define INPUT_EVENT_MANAGER_PRIVATE_H_

#include <task_list.h>

#define MULTI_CLICK_TIMEOUT 500

/* Multi click states */
typedef enum input_event_multiclick_states_t
{
    IEM_STATE_MULTICLICK_IDLE,
    IEM_STATE_MULTICLICK_COUNTING
} input_event_multiclick_states_t;

typedef struct InputMultiClickState
{
    input_event_bits_t              input_event_mask;
    input_event_multiclick_states_t state;
    uint16                          n_clicks;
} InputMultiClickState_t;

typedef struct
{
    TaskData                    task;
    task_list_t *               client_tasks;

    const InputActionMessage_t *action_table;
    uint32                      num_action_messages;
    InputMultiClickState_t      multi_click_state;

    /* The input event bits as last read or indicated */
    input_event_bits_t          input_event_bits;

    /* PAM state stored for timers */
    const InputActionMessage_t *repeat;
    const InputActionMessage_t *held_release;

    const InputEventConfig_t   *input_config;

    uint32                      pio_state[NUMBER_OF_PIO_BANKS];

    /* Used in processing of PIO_MONITOR_ENABLE_CFM */
    uint16                      numActivePios;
    uint16                      maxActivePios;
} InputEventState_t;

/* Task messages */
enum input_event_message_internal_msg
{
    IEM_INTERNAL_HELD_TIMER,
    IEM_INTERNAL_REPEAT_TIMER,
    IEM_INTERNAL_HELD_RELEASE_TIMER,
    IEM_INTERNAL_MULTI_CLICK_TIMER,
    IEM_NUM_OF_INTERNAL_MESSAGES
};

#endif    /* INPUT_EVENT_MANAGER_PRIVATE_H_ */

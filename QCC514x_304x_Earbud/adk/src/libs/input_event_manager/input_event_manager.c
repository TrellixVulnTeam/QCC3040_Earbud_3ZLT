/*
Copyright (c) 2018-2021  Qualcomm Technologies International, Ltd.
*/

#define DEBUG_LOG_MODULE_NAME iem
#include <logging.h>

#include <csrtypes.h>
#include <vmtypes.h>
#include <panic.h>
#include <stdlib.h>
#include <string.h>
#include <task_list.h>
#include <pio.h>

#include "pio_monitor.h"
#include "input_event_manager.h"
#include "input_event_manager_private.h"

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_ENUM(input_event_message_internal_msg)
LOGGING_PRESERVE_MESSAGE_ENUM(input_event_multiclick_states_t)
DEBUG_LOG_DEFINE_LEVEL_VAR

static InputEventState_t input_event_manager_state = { {0} };

static bool getPreviousBitState(InputEventState_t *state, const InputActionMessage_t *input_action)
{
    return (input_action->bits == (state->input_event_bits & input_action->mask));
}

static bool actionIsPress(input_event_bits_t input_event_bits, const InputActionMessage_t *input_action)
{
    return (input_action->bits == (input_event_bits & input_action->mask));
}

static bool actionIsRelease(InputEventState_t *state,
                            const InputActionMessage_t *input_action,
                            input_event_bits_t input_event_bits)
{
    return getPreviousBitState(state, input_action) && !(actionIsPress(input_event_bits, input_action));
}

static void enterAction(InputEventState_t *state,
                        const InputActionMessage_t *input_action,
                        input_event_bits_t input_event_bits)
{
    /* If all the bits, for the msg, are 'on', and at least one of those bits,
     * was just turned on, then ...
     */
    if (actionIsPress(input_event_bits, input_action))
    {
        /* A new enter action cancels any existing repeat timer */
        (void) MessageCancelAll(&state->task,
                                IEM_INTERNAL_REPEAT_TIMER);
        DEBUG_LOG_VERBOSE("IEM: triggering enter action %p", input_action);
        TaskList_MessageSendId(state->client_tasks, input_action->message);

        /* if there is a repeat on this action, start the repeat timer */
        if (input_action->repeat)
        {
            state->repeat = input_action;
            MessageSendLater(&state->task, IEM_INTERNAL_REPEAT_TIMER, 0, input_action->repeat);
        }
        else
            state->repeat = 0;

    }
    /* if any of the bits are turned off and there is a repeat timer,
     * cancel it and clear the stored input_action
     */
    else if (input_action->repeat &&
             state->repeat == input_action &&
             actionIsRelease(state, input_action, input_event_bits))
    {
        (void) MessageCancelAll(&state->task, IEM_INTERNAL_REPEAT_TIMER);
        state->repeat = 0;
    }
}

static void releaseAction(InputEventState_t *state,
                        const InputActionMessage_t *input_action,
                        input_event_bits_t input_event_bits)
{
    if (actionIsRelease(state, input_action, input_event_bits))
    {
        DEBUG_LOG_VERBOSE("IEM: triggering release action %p", input_action);
        TaskList_MessageSendId(state->client_tasks, input_action->message);
    }
}

static void heldActionButtonDownAction(InputEventState_t *state, const InputActionMessage_t *input_action)
{
    /* Send a pointer to this input_action as part of the timer message so that it
     * can be handled when the timeout expired
     */
    const InputActionMessage_t **m = PanicUnlessNew(const InputActionMessage_t *);
    *m = input_action;

    DEBUG_LOG_V_VERBOSE("IEM: starting held timer %p", input_action);
    MessageSendLater(&state->task, IEM_INTERNAL_HELD_TIMER, m, input_action->timeout);
}

static void heldActionButtonReleaseAction(InputEventState_t *state)
{
    /* Cancel any active held or repeat timers. */
    if (!MessageCancelAll(&state->task, IEM_INTERNAL_HELD_TIMER))
    {
        (void)MessageCancelAll(&state->task, IEM_INTERNAL_REPEAT_TIMER);
    }
}

/* There can be 1+ held action/messages on the same PIO */
static void heldAction(InputEventState_t *state,
                       const InputActionMessage_t *input_action,
                       input_event_bits_t input_event_bits)
{
    /* If all the PIO, for the msg, are 'on'... */
    if (actionIsPress(input_event_bits, input_action))
    {
        heldActionButtonDownAction(state, input_action);
    }
    else if (actionIsRelease(state, input_action, input_event_bits))
    {
        heldActionButtonReleaseAction(state);
    }
}

static void heldReleaseButtonDownAction(InputEventState_t *state, const InputActionMessage_t *input_action)
{
    const InputActionMessage_t **m = PanicUnlessNew(const InputActionMessage_t *);
    *m = input_action;

    MessageSendLater(&state->task, IEM_INTERNAL_HELD_RELEASE_TIMER, (void*)m, input_action->timeout);
    state->held_release = 0;
}

static void heldReleaseButtonReleaseAction(InputEventState_t *state, const InputActionMessage_t *input_action)
{
    (void) MessageCancelAll(&state->task, IEM_INTERNAL_HELD_RELEASE_TIMER);

    if (state->held_release == input_action)
        /* If an action message was registered, it means that the
        * held_release timer has expired and hence send the
        * message */
    {
        DEBUG_LOG_VERBOSE("IEM: triggering held release action %p", input_action);
        state->held_release = 0;
        TaskList_MessageSendId(state->client_tasks, input_action->message);
    }
}

static void heldReleaseAction(InputEventState_t *state, const InputActionMessage_t *input_action, input_event_bits_t input_event_bits)
{
    /* If all the bits, for the msg, are 'on' then ...
     */
    if (actionIsPress(input_event_bits, input_action))
    {
        heldReleaseButtonDownAction(state, input_action);
    }
    else if (actionIsRelease(state, input_action, input_event_bits))
    {
        heldReleaseButtonReleaseAction(state, input_action);
    }
}

static void multiClickEnd(InputEventState_t *state)
{
    DEBUG_LOG_V_VERBOSE("IEM: multiClickEnd");

    MessageCancelAll(&state->task, IEM_INTERNAL_MULTI_CLICK_TIMER);
    state->multi_click_state.n_clicks = 0;
    state->multi_click_state.state = IEM_STATE_MULTICLICK_IDLE;
    state->multi_click_state.input_event_mask = 0;
}

static void multiClickReStartTimer(InputEventState_t *state,
                                 const InputActionMessage_t *input_action,
                                 input_event_bits_t input_event_bits)
{
    UNUSED(input_event_bits);

    MessageCancelAll(&state->task, IEM_INTERNAL_MULTI_CLICK_TIMER);

    /* Start the timer to finish the counting (unless another click is detected) */
    const InputActionMessage_t **m = PanicUnlessNew(const InputActionMessage_t *);
    *m = input_action;
    DEBUG_LOG_V_VERBOSE("IEM: multiClickReStartTimer %p, starting timer", input_action);
    MessageSendLater(&state->task, IEM_INTERNAL_MULTI_CLICK_TIMER, (void*)m, MULTI_CLICK_TIMEOUT);
}

static void multiClickCount(InputEventState_t *state,
                            const InputActionMessage_t *input_action,
                            input_event_bits_t input_event_bits)
{
    UNUSED(input_event_bits);

    /* Increment the click counter */
    state->multi_click_state.n_clicks++;
    DEBUG_LOG_VERBOSE("IEM: multiClickCount %p, n_clicks %d", input_action, state->multi_click_state.n_clicks);
}

static void multiClickStart(InputEventState_t *state,
                            const InputActionMessage_t *input_action,
                            input_event_bits_t input_event_bits)
{
    DEBUG_LOG_V_VERBOSE("IEM: multiClickStart %p, clicks %d, PIO bitmask 0x%x", input_action, input_action->repeat, input_event_bits);

    /* Cancel any current timers */
    MessageCancelAll(&state->task, IEM_INTERNAL_MULTI_CLICK_TIMER);

    /* Store the bits associated to this multi click counting */
    state->multi_click_state.input_event_mask = input_action->mask;
    state->multi_click_state.n_clicks = 0;
    state->multi_click_state.state = IEM_STATE_MULTICLICK_COUNTING;

    /* Start the timer in case a release is not detected in time */
    multiClickReStartTimer(state, input_action, input_event_bits);
}

static void multiClickAction(InputEventState_t *state,
                             const InputActionMessage_t *input_action,
                             input_event_bits_t input_event_bits)
{
    DEBUG_LOG_V_VERBOSE("IEM: multiClickAction %p, state enum:input_event_multiclick_states_t:%d", input_action, state->multi_click_state.state);

    switch (state->multi_click_state.state)
    {
        case IEM_STATE_MULTICLICK_IDLE:
        {
            /* Only action on the set of the PIO bits (button press down) */
            if (actionIsPress(input_event_bits, input_action))
            {
                multiClickStart(state, input_action, input_event_bits);
            }
            else if (actionIsRelease(state, input_action, input_event_bits))
            {
                DEBUG_LOG_V_VERBOSE("IEM: multiClickAction, idle and release, ignore");
            }
        }
        break;

        case IEM_STATE_MULTICLICK_COUNTING:
        {
            /* If button was pressed, restart the timer. */
            if (actionIsPress(input_event_bits, input_action))
            {
                /* However, if a multi click detection was ongoing but for a different set of PIOs... */
                if (state->multi_click_state.input_event_mask != input_action->mask)
                {
                    /* ...end the former and start a new multiclick detection */
                    DEBUG_LOG_VERBOSE("IEM: multiClickAction, button pressed ...");
                    DEBUG_LOG_VERBOSE("... PIO bitmask changed to 0x%x, was 0x%x. Reset multiclick.", input_action->mask, state->multi_click_state.input_event_mask);
                    multiClickStart(state, input_action, input_event_bits);
                }
                else
                {
                    multiClickReStartTimer(state, input_action, input_event_bits);
                }
            }
            else if (actionIsRelease(state, input_action, input_event_bits))
            {
                /* However, only count if the PIO bitmask matches */
                /* This is to account for a button that had started multi click but was then held down,
                 * whilst another button re-started a multiclick count. When the former is released,
                 * we shall do nothing since its (the former's) multi click count was ended by the latter's. */
                if (state->multi_click_state.input_event_mask == input_action->mask)
                {
                    multiClickCount(state, input_action, input_event_bits);
                }
                else
                {
                    DEBUG_LOG_V_VERBOSE("IEM: multiClickAction, button released ...");
                    DEBUG_LOG_V_VERBOSE("... PIO bitmask mismatch: got 0x%x, expected 0x%x, ignore.", input_action->mask, state->multi_click_state.input_event_mask);
                }
            }
        }
        break;
    }
}

static void inputEventsChanged(InputEventState_t *state, input_event_bits_t input_event_bits)
{
    input_event_bits_t changed_bits = state->input_event_bits ^ input_event_bits;
    const InputActionMessage_t *input_action = state->action_table;
    const uint32 size = state->num_action_messages;
    bool flag_multiclick_processed = FALSE;

    DEBUG_LOG_V_VERBOSE("IEM: inputEventsChanged, updated input events %08x", input_event_bits);

    /* Go through the action table to determine what action to do and
       what message may need to be sent. */
    for (;input_action != &(state->action_table[size]); input_action++)
    {
        if (changed_bits & input_action->mask)
        {
            switch (input_action->action)
            {
                case ENTER:
                    enterAction(state, input_action, input_event_bits);
                    break;

                case RELEASE:
                    releaseAction(state, input_action, input_event_bits);
                    break;

                case MULTI_CLICK:
                    /* Only process one multiclick entry in the entire list */
                    if (!flag_multiclick_processed)
                    {
                        multiClickAction(state, input_action, input_event_bits);
                        flag_multiclick_processed = TRUE;
                    }
                    break;

                case HELD:
                    heldAction(state, input_action, input_event_bits);
                    break;

                case HELD_RELEASE:
                    heldReleaseAction(state, input_action, input_event_bits);
                    break;

                default:
                    break;
            }
        }
    }

    /* Store the bits previously reported */
    state->input_event_bits = input_event_bits;
}

static uint32 calculateInputEvents(InputEventState_t *state)
{
    int pio, bank;
    uint32 input_event_bits = 0;
    for (bank = 0; bank < NUMBER_OF_PIO_BANKS; bank++)
    {
        const int pio_base = bank * 32;
        const uint32 pio_state = state->pio_state[bank];
        for (pio = 0; pio < 32; pio++)
        {
            const uint32 pio_mask = 1UL << pio;
            if (pio_state & pio_mask)
                input_event_bits |= (1UL << state->input_config->pio_to_iem_id[pio_base + pio]);
        }
    }
    return input_event_bits;
}

static void handleMessagePioChangedEvents(InputEventState_t *state, const MessagePioChanged *mpc)
{
    /* Mask out PIOs we're not interested in */
    const uint32 pio_state = (mpc->state) + ((uint32)mpc->state16to31 << 16);
    const uint32 pio_state_masked = pio_state & state->input_config->pio_input_mask[mpc->bank];

    if (state->pio_state[mpc->bank] != pio_state_masked)
    {
        /* Update our copy of the PIO state */
        state->pio_state[mpc->bank] = pio_state_masked;

        /* Calculate input events from PIO state and handle them */
        input_event_bits_t input_event_bits = calculateInputEvents(state);
        inputEventsChanged(state, input_event_bits);
    }
}

static void waitForEnableConfirmation(InputEventState_t *state)
{
    DEBUG_LOG_V_VERBOSE("IEM: Received event: PIO_MONITOR_ENABLE_CFM");
    if (++(state->numActivePios) == state->maxActivePios)
    {
        /* Send initial PIO messages */
        for (uint8 bank = 0; bank < NUMBER_OF_PIO_BANKS; bank++)
        {
            MessagePioChanged mpc_message;
            uint32 pio_state = PioGet32Bank(bank);
            mpc_message.state       = (pio_state >>  0) & 0xFFFF;
            mpc_message.state16to31 = (pio_state >> 16) & 0xFFFF;
            mpc_message.time = 0;
            mpc_message.bank = bank;
            handleMessagePioChangedEvents(state, &mpc_message);
        }
    }
}

static void iemHandler(Task task, MessageId id, Message message)
{
    InputEventState_t *state = (InputEventState_t *)task;

    if (id < IEM_NUM_OF_INTERNAL_MESSAGES)
    {
        DEBUG_LOG_V_VERBOSE("IEM: iemHandler enum:input_event_message_internal_msg:%d", id);
    }

    switch (id)
    {
        case MESSAGE_PIO_CHANGED:
        {
            const MessagePioChanged *mpc = (const MessagePioChanged *)message;
            DEBUG_LOG_V_VERBOSE("IEM: MESSAGE_PIO_CHANGED: bank=%hu, mask=%04x%04x",
                       mpc->bank,mpc->state16to31,mpc->state);
            handleMessagePioChangedEvents(state,mpc);
        }
        break;

        case PIO_MONITOR_ENABLE_CFM:
        {
            waitForEnableConfirmation(state);
        }
        break;

        /* If a pio has been HELD for the timeout required, then send the message stored */
        case IEM_INTERNAL_HELD_TIMER:
        {
            const InputActionMessage_t **m = (const InputActionMessage_t **)message;
            const InputActionMessage_t *input_action = *m;

            multiClickEnd(state);

            DEBUG_LOG_VERBOSE("IEM: triggering held action %p", input_action);
            TaskList_MessageSendId(state->client_tasks, input_action->message);

            /* Cancel any existing repeat timer that may be running */
            (void)MessageCancelAll(&state->task, IEM_INTERNAL_REPEAT_TIMER);

            /* If there is a repeat action start the repeat on this message
               and store the input_action */
            if (input_action->repeat)
            {
                MessageSendLater(&state->task,
                                 IEM_INTERNAL_REPEAT_TIMER, 0,
                                 input_action->repeat);

                state->repeat = input_action;
            }
        }
        break;

        case IEM_INTERNAL_REPEAT_TIMER:
        {
            if (state->repeat)
            {
                DEBUG_LOG_VERBOSE("IEM: triggering repeat action");
                TaskList_MessageSendId(state->client_tasks, (state->repeat)->message);

                /* Start the repeat timer again */
                MessageSendLater(&state->task, IEM_INTERNAL_REPEAT_TIMER,
                                 NULL, (state->repeat)->repeat);
            }
        }
        break;

        /* Store the input_action so that when the PIO for the message are released
           it can be validated and the message sent */
        case IEM_INTERNAL_HELD_RELEASE_TIMER:
        {
            const InputActionMessage_t **m = (const InputActionMessage_t **)message;
            state->held_release = *m;

            multiClickEnd(state);
        }
        break;

        case IEM_INTERNAL_MULTI_CLICK_TIMER:
        {
            const InputActionMessage_t **m = (const InputActionMessage_t **)message;
            const InputActionMessage_t *input_action = *m;

            const InputActionMessage_t *ia_loop = state->action_table;
            const uint32 size = state->num_action_messages;

            DEBUG_LOG_V_VERBOSE("IEM: multiclick timer %p, n_clicks %d", input_action, state->multi_click_state.n_clicks);

            /* Ignore if button is still pressed (i.e. PIO bits are high) */
            if (state->input_event_bits == input_action->mask)
            {
                DEBUG_LOG_VERBOSE("IEM: multiclick timer %p, button is still pressed, ignore.", input_action);
                break;
            }

            /* Go through the action table to look for a matching number of clicks and PIOs */
            for (;ia_loop != &(state->action_table[size]); ia_loop++)
            {
                if ((ia_loop->action == MULTI_CLICK) && (ia_loop->count == state->multi_click_state.n_clicks)
                        && (ia_loop->bits == input_action->bits) && (ia_loop->mask == input_action->mask))
                {
                    DEBUG_LOG_VERBOSE("IEM: triggering multiclick action %p, num_of_clicks %u, message %u", ia_loop, ia_loop->count, ia_loop->message);
                    TaskList_MessageSendId(state->client_tasks, ia_loop->message);
                }
            }
            /* Timeout happened, end and reset any multiclick counting */
            multiClickEnd(state);
        }
        break;

        default:
            break;
    }
}

static void configurePioHardware(void)
{
    /* Configure PIOs:
       1.  Map as PIOs
       2.  Allow deep sleep on either level
       3.  Set as inputs */
    for ( uint16 bank = 0 ; bank < NUMBER_OF_PIO_BANKS ; bank++ )
    {
        const uint32 pio_bank_mask = input_event_manager_state.input_config->pio_input_mask[bank];
        DEBUG_LOG_V_VERBOSE("IEM: Configuring bank %d, mask %08x", bank, pio_bank_mask);
        uint32 result;
        result = PioSetMapPins32Bank(bank, pio_bank_mask, pio_bank_mask);
        if (result != 0)
        {
            DEBUG_LOG_ERROR("IEM: PioSetMapPins32Bank error: bank %d, mask %08x, result=%08x", bank, pio_bank_mask, result);
            Panic();
        }
        PioSetDeepSleepEitherLevelBank(bank, pio_bank_mask, pio_bank_mask);
        result = PioSetDir32Bank(bank, pio_bank_mask, 0);
        if (result != 0)
        {
            DEBUG_LOG_ERROR("IEM: PioSetDir32Bank error: bank %d, mask %08x, result=%08x", bank, pio_bank_mask, result);
            Panic();
        }
    }
}

static void registerForPioEvents(void)
{
    for (uint8 pio = 0 ; pio < IEM_NUM_PIOS ; pio++)
    {
        if (input_event_manager_state.input_config->pio_to_iem_id[pio] != -1)
        {
            PioMonitorRegisterTask(&input_event_manager_state.task,pio);
        }
    }
    input_event_manager_state.maxActivePios = 1;
}

void InputEventManager_RegisterClient(Task client)
{
    TaskList_AddTask(input_event_manager_state.client_tasks, client);
}

Task InputEventManagerInit(Task client,
                           const InputActionMessage_t *action_table,
                           uint32 action_table_dim,
                           const InputEventConfig_t *input_config)
{
    DEBUG_LOG_INFO("IEM: InputEventManagerInit");
    memset(&input_event_manager_state, 0, sizeof(input_event_manager_state));

    input_event_manager_state.task.handler = iemHandler;
    input_event_manager_state.client_tasks = TaskList_Create();
    TaskList_AddTask(input_event_manager_state.client_tasks, client);
    input_event_manager_state.action_table = action_table;
    input_event_manager_state.num_action_messages = action_table_dim;
    input_event_manager_state.input_config = input_config;
    input_event_manager_state.multi_click_state.input_event_mask = 0;
    input_event_manager_state.multi_click_state.n_clicks = 0;
    input_event_manager_state.multi_click_state.state = IEM_STATE_MULTICLICK_IDLE;

    configurePioHardware();
    registerForPioEvents();
    PioMonitorSetDebounceParameters(input_config->debounce_period,input_config->debounce_reads);

    return &input_event_manager_state.task;
}

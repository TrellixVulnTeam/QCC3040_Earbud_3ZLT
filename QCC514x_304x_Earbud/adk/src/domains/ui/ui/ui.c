/*!
\copyright  Copyright (c) 2019 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      This is ui module which provides mechanism to find and send mapped ui
            input from ui configuration table for any incoming logical input.

    These apis are used by application as well as different ui providers, ui input
    consumers as well as ui provider context consumers to register/unregister themselves
    with the ui module as well as handling incoming logical input.This module sends mapped
    ui input for the logical input to interested ui input consumers.
*/

#include "ui_indicator_log_level.h"
#include "ui.h"
#include "ui_indicator_private.h"
#include "ui_inputs.h"
#include "ui_indicator_prompts.h"
#include "ui_indicator_tones.h"
#include "ui_indicator_leds.h"
#include "adk_log.h"

#include <stdlib.h>
#include <stdio.h>
#include <logging.h>
#include <panic.h>
#include <task_list.h>
#include <message_broker.h>

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_TYPE(ui_message_t)
LOGGING_PRESERVE_MESSAGE_ENUM(ui_internal_led_messages)
LOGGING_PRESERVE_MESSAGE_TYPE(ui_input_t)


#if UI_USE_LOG_LEVELS
DEBUG_LOG_DEFINE_LEVEL_VAR
#endif

#define NUM_OF_UI_INPUT_CONSUMERS                      5
#define ERROR_UI_PROVIDER_NOT_PRESENT                  0xFF


/*! Macro used to count the size of the UI_INPUTS group of messages.
    The macro is based on those in domain_message.h and allows for
    single individual message groups to be larger.

    Without this, there is a risk that the ui_input_consumers_task_list
    array will be too small, and overwritten.

    This is done by defining two enum values. The first matches the name,
    the second is based on the size and can cause subsequent enums to
    skip a value.
*/
#define GENERATE_GROUP_MULTIPLES_IMPL(component_name,size,...)  \
                        component_name, \
                        component_name##_UI_END = component_name + (size-1), 
#define GENERATE_GROUP_MULTIPLES(...) GENERATE_GROUP_MULTIPLES_IMPL(__VA_ARGS__,1,_unused)


/*! Enumerate the UI message group, so the total number is known */
enum UI_MESSAGE_GROUPS_COUNT
{
    FOREACH_UI_INPUTS_MESSAGE_GROUP(GENERATE_GROUP_MULTIPLES)
    NUMBER_OF_UI_INPUTS_MESSAGE_GROUPS,
};

/*! \brief UI task structure */
typedef struct
{
    /*! The UI task. */
    TaskData task;

} uiTaskData;

/*!< UI data structure */
uiTaskData  app_ui;

/*! \brief Ui provider struct*/
typedef struct{

    ui_providers_t ui_provider_id;
    ui_provider_context_callback_t ui_provider_context_callback;
}registered_ui_provider_t;

registered_ui_provider_t *registered_ui_providers = NULL;

/*! \brief Ui provider context consumer struct*/
typedef struct{

    Task consumer_task;
    ui_providers_t ui_provider_id;

}ui_provider_context_consumer_t;

ui_provider_context_consumer_t* ui_provider_context_consumers = NULL;

/* One task-list per ui input group */
static task_list_t ui_input_consumers_task_list[NUMBER_OF_UI_INPUTS_MESSAGE_GROUPS];

static uint8 num_of_ui_providers = 0;
static uint8 num_of_ctxt_consumers = 0;

static const ui_config_table_content_t* ui_config_table;
static unsigned ui_config_size = 0;

/*! The default UI interceptor function */
inject_ui_input inject_ui_input_funcptr = NULL;

/*! The default function for decision making about when to screen out logical inputs,
    by default they are never screened. */
li_screening_decider_t logical_input_screening_decider_funcptr = NULL;

/*! The default UI Event sniffer function */
sniff_ui_event sniff_ui_event_funcptr = NULL;

/*! Lock for sharing of kymera resource for tone/prompt playing */
uint16 ui_kymera_lock;


/******************************************************************************
 * Internal functions
 ******************************************************************************/
static uint8 ui_GetUiProviderIndexInRegisteredList(ui_providers_t ui_provider_id)
{
    for(uint8 index=0; index<num_of_ui_providers; index++)
    {
        if(registered_ui_providers[index].ui_provider_id == ui_provider_id)
            return index;
    }
    return ERROR_UI_PROVIDER_NOT_PRESENT;
}

static ui_input_t ui_GetUiInput(unsigned logical_input)
{
    uint8 ui_provider_index_in_list = 0;
    unsigned ui_provider_ctxt = 0;

    for(uint8 index = 0; index < ui_config_size; index++)
    {
        if(ui_config_table[index].logical_input == logical_input)
        {
            /* find index of the ui provider registered for the logical input in the list*/
            ui_provider_index_in_list = ui_GetUiProviderIndexInRegisteredList(ui_config_table[index].ui_provider_id );

            if(ui_provider_index_in_list != ERROR_UI_PROVIDER_NOT_PRESENT)
            {
                /* get current context of this ui provider*/
                ui_provider_ctxt = registered_ui_providers[ui_provider_index_in_list].ui_provider_context_callback();

                /* if context is same then return corresponding ui input*/
                if(ui_provider_ctxt == ui_config_table[index].ui_provider_context)
                    return ui_config_table[index].ui_input;

            }
        }
    }
    return ui_input_invalid;
}

/*! \brief Convert message group to 0-based index and send input to the indexed
    task list */
static void ui_SendUiInputToConsumerGroupTaskList(ui_input_t ui_input, uint32 delay)
{
    message_group_t group = ID_TO_MSG_GRP(ui_input);
    PanicFalse(group >= UI_INPUTS_MESSAGE_GROUP_START);
    group -= UI_INPUTS_MESSAGE_GROUP_START;
    PanicFalse(group < NUMBER_OF_UI_INPUTS_MESSAGE_GROUPS);

    TaskList_MessageSendLaterWithSize(&ui_input_consumers_task_list[group], ui_input, NULL, 0, delay);
}

static void ui_HandleLogicalInput(unsigned logical_input)
{
    /* Check whether the Application is screening Logical Inputs, if so dispose of it. */
    if (logical_input_screening_decider_funcptr &&
        logical_input_screening_decider_funcptr(logical_input))
    {
        DEBUG_LOG_VERBOSE("ui_HandleLogicalInput logical_input=%d screened", logical_input);
    }
    else
    {
        /* Look-up the highest priority UI Input for this Logical Input from the
           Application's configuration table. */
        ui_input_t ui_input = ui_GetUiInput(logical_input);
        uint32 delay = D_IMMEDIATE;

        if(ui_input != ui_input_invalid)
        {
           DEBUG_LOG("ui_HandleLogicalInput enum:ui_input_t:%d", ui_input);

            PanicNull((void*)inject_ui_input_funcptr);
            inject_ui_input_funcptr(ui_input, delay);
        }
    }
}

static void ui_HandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);

    ui_HandleLogicalInput(id);
}

/******************************************************************************
 * External API functions
 ******************************************************************************/
Task Ui_GetUiTask(void)
{
    return &app_ui.task;
}

void Ui_RegisterUiProvider(ui_providers_t ui_provider, ui_provider_context_callback_t ui_provider_context_callback)
{
    size_t new_size;
    registered_ui_provider_t *new_provider;

    DEBUG_LOG_VERBOSE("Ui_RegisterUiProvider enum:ui_providers_t:%d", ui_provider);

    PanicFalse((!num_of_ui_providers && !registered_ui_providers) ||
               ( num_of_ui_providers &&  registered_ui_providers));

    new_size = sizeof(registered_ui_provider_t) * (num_of_ui_providers + 1);
    registered_ui_providers = PanicNull(realloc(registered_ui_providers, new_size));

    new_provider = registered_ui_providers + num_of_ui_providers;
    new_provider->ui_provider_context_callback = ui_provider_context_callback;
    new_provider->ui_provider_id = ui_provider;

    num_of_ui_providers++;
}

void Ui_UnregisterUiProviders(void)
{
    free(registered_ui_providers);
    registered_ui_providers = NULL;
    num_of_ui_providers = 0;
}

void Ui_RegisterUiInputConsumer(Task ui_input_consumer_task,
                                const message_group_t * msg_groups_of_interest,
                                unsigned num_msg_groups)
{
    MessageBroker_RegisterInterestInMsgGroups(ui_input_consumer_task, msg_groups_of_interest, num_msg_groups);
}

static bool ui_taskIsAlreadyRegisteredForProviderContextChanges(
        ui_providers_t provider,
        Task client_task)
{
    bool is_already_registered = FALSE;
    for (int i=0; i < num_of_ctxt_consumers; i++)
    {
        if (provider == ui_provider_context_consumers[i].ui_provider_id &&
            client_task == ui_provider_context_consumers[i].consumer_task)
        {
            is_already_registered = TRUE;
        }
    }
    return is_already_registered;
}

void Ui_RegisterContextConsumers(
        ui_providers_t ui_provider,
        Task ui_provider_ctxt_consumer_task)
{
    if (!ui_taskIsAlreadyRegisteredForProviderContextChanges(ui_provider, ui_provider_ctxt_consumer_task))
    {
        size_t new_size;
        ui_provider_context_consumer_t *new_consumer;

        PanicFalse((!num_of_ctxt_consumers && !ui_provider_context_consumers) ||
                   ( num_of_ctxt_consumers &&  ui_provider_context_consumers));

        new_size = sizeof(ui_provider_context_consumer_t) * (num_of_ctxt_consumers + 1);
        ui_provider_context_consumers = PanicNull(realloc(ui_provider_context_consumers, new_size));

        new_consumer = ui_provider_context_consumers + num_of_ctxt_consumers;
        new_consumer->consumer_task = ui_provider_ctxt_consumer_task;
        new_consumer->ui_provider_id = ui_provider;

        num_of_ctxt_consumers++;
    }
}

void Ui_UnregisterContextConsumers(void)
{
    free(ui_provider_context_consumers);
    ui_provider_context_consumers = NULL;
    num_of_ctxt_consumers = 0;
}


void Ui_InformContextChange(ui_providers_t ui_provider,unsigned latest_ctxt)
{
    for(uint8 index=0; index<num_of_ctxt_consumers; index++)
    {
        if (ui_provider_context_consumers[index].ui_provider_id == ui_provider &&
            ui_provider_context_consumers[index].consumer_task != NULL)
        {

            MESSAGE_MAKE(message,UI_PROVIDER_CONTEXT_UPDATED_T);
            message->provider = ui_provider;
            message->context = latest_ctxt;
            MessageSend(ui_provider_context_consumers[index].consumer_task,
                        UI_PROVIDER_CONTEXT_UPDATED,
                        message);
        }
    }
}

void Ui_InjectLogicalInput(unsigned logical_input, bool is_right_device)
{
    UNUSED(is_right_device);

    ui_HandleLogicalInput(logical_input);
}

void Ui_InjectUiInput(ui_input_t ui_input)
{
    PanicNull((void*)inject_ui_input_funcptr);
    inject_ui_input_funcptr(ui_input, D_IMMEDIATE);
}

void Ui_InjectUiInputWithDelay(ui_input_t ui_input, uint32 delay)
{
    ui_SendUiInputToConsumerGroupTaskList(ui_input, delay);
}

/*! brief Initialise UI module */
bool Ui_Init(Task init_task)
{
    message_group_t group;
    uiTaskData *theUi = &app_ui;
    /* Set up task handler */
    theUi->task.handler = ui_HandleMessage;

    /* Set up the default UI interceptor function */
    inject_ui_input_funcptr = ui_SendUiInputToConsumerGroupTaskList;
    sniff_ui_event_funcptr = NULL;

    for (group = 0; group < NUMBER_OF_UI_INPUTS_MESSAGE_GROUPS; group++)
    {
        TaskList_Initialise(&ui_input_consumers_task_list[group]);
    }

    ui_ClearKymeraResourceLock();

    UNUSED(init_task);
    return TRUE;
}

void Ui_SetConfigurationTable(const ui_config_table_content_t* config_table,
                              unsigned config_size)
{
    ui_config_table = config_table;
    ui_config_size = config_size;
}

void Ui_RegisterUiInputsMessageGroup(Task task, message_group_t group)
{
    PanicFalse(group >= UI_INPUTS_MESSAGE_GROUP_START);
    group -= UI_INPUTS_MESSAGE_GROUP_START;
    PanicFalse(group < NUMBER_OF_UI_INPUTS_MESSAGE_GROUPS);

    TaskList_AddTask(&ui_input_consumers_task_list[group], task);
}

void Ui_RegisterUiEventSniffer(sniff_ui_event ui_sniff_func)
{
    sniff_ui_event_funcptr = ui_sniff_func;
}

/*! brief Function to register the new UI interceptor function */
inject_ui_input Ui_RegisterUiInputsInterceptor(inject_ui_input ui_intercept_func)
{
    inject_ui_input old_ui_interceptor;

    /* Return NULL for invalid interceptor function */
    if(ui_intercept_func == NULL)
        return NULL;

    /* Preserve the active UI interceptor function */
    old_ui_interceptor = inject_ui_input_funcptr;

    /* Update the active UI interceptor function pointer with the supplied function */
    inject_ui_input_funcptr = ui_intercept_func;

    /* Return the old UI interceptor function */
    return old_ui_interceptor;
}

/*! brief Function to register the new UI interceptor function */
void Ui_RegisterLogicalInputScreeningDecider(li_screening_decider_t screening_decider_func)
{
    PanicNull((void *)screening_decider_func);

    /* Update the active UI interceptor function pointer with the supplied function */
    logical_input_screening_decider_funcptr = screening_decider_func;
}

rtime_t Ui_RaiseUiEvent(ui_indication_type_t type, uint16 indication_index, rtime_t time_to_play)
{
    rtime_t initial_ttp = time_to_play;

    if (sniff_ui_event_funcptr != NULL)
    {
        time_to_play = sniff_ui_event_funcptr(type, indication_index, time_to_play);
    }

    DEBUG_LOG("Ui_RaiseUiEvent enum:ui_indication_type_t:%d index=%d initial=%d final ttp=%d",
              type, indication_index, initial_ttp, time_to_play );

    return time_to_play;
}

void Ui_NotifyUiEvent(ui_indication_type_t ind_type, uint16 ind_index, rtime_t timestamp)
{
    switch(ind_type)
    {
    case ui_indication_type_audio_prompt:
        {
            DEBUG_LOG("Ui_NotifyUiEvent send prompt_index %d at %d us", ind_index, timestamp);
            UiPrompts_NotifyUiIndication(ind_index, timestamp);
        }
        break;
    case ui_indication_type_audio_tone:
        {
            DEBUG_LOG("Ui_NotifyUiEvent send tone_index %d at %d us", ind_index, timestamp);
            UiTones_NotifyUiIndication(ind_index, timestamp);
        }
        break;
    case ui_indication_type_led:
        {
            DEBUG_LOG("Ui_NotifyUiEvent send led_index %d", ind_index);
            UiLeds_NotifyUiIndication(ind_index);
        }
        break;
    case ui_indication_type_prepare_for_prompt:
        {
            DEBUG_LOG("Ui_NotifyUiEvent send prepare prompt index %d", ind_index);
            UiPrompts_NotifyUiPrepareIndication(ind_index);
        }
        break;
    default:
        /* Can be extended in the future for other indication types */
        Panic();
        break;
    }
}

unsigned Ui_GetUiProviderContext(ui_providers_t ui_provider)
{
    uint8 provider_index = ui_GetUiProviderIndexInRegisteredList(ui_provider);

    PanicFalse(registered_ui_providers!=NULL);

    return registered_ui_providers[provider_index].ui_provider_context_callback();
}

#define DECLARE_MESSAGE_GROUP_REGISTRATION(group_name) \
MESSAGE_BROKER_GROUP_REGISTRATION_MAKE(group_name, Ui_RegisterUiInputsMessageGroup, NULL);

FOREACH_UI_INPUTS_MESSAGE_GROUP(DECLARE_MESSAGE_GROUP_REGISTRATION)

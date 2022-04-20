/*!
\copyright  Copyright (c) 2019 - 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       audio_curation.c
\brief      A component responsible for controlling audio curation services.
*/

#include "audio_curation.h"
#include "logging.h"
#include "ui.h"
#include "anc_state_manager.h"
#include "anc.h"
#include "power_manager.h"
#include "aec_leakthrough.h"
#include "phy_state.h"
#include "kymera.h"
#include "fit_test.h"


/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_ENUM(audio_curation_messages)
ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(AUDIO_CURATION_SERVICE, AUDIO_CURATION_MESSAGE_END)

#define audioCuration_SendEvent(msg_id) TaskList_MessageSendId(audioCuration_GetMessageClients(), msg_id)
static task_list_t * client_list;
static bool diagnostic;

static void audioCuration_HandleMessage(Task task, MessageId id, Message message);
static const TaskData ui_task = {audioCuration_HandleMessage};

static const message_group_t ui_inputs[] =
{
    UI_INPUTS_AUDIO_CURATION_MESSAGE_GROUP
};

static bool audioCuration_IsStereo(void)
{
#ifdef INCLUDE_STEREO
    return TRUE;
#else
    return FALSE;
#endif
}

static Task audioCuration_UiTask(void)
{
  return (Task)&ui_task;
}

static task_list_t * audioCuration_GetMessageClients(void)
{
    return client_list;
}

static void audioCuration_InitMessages(void)
{
    client_list = TaskList_Create();
}

static void handleAncClientEvent(MessageId id)
{
    switch (id)
    {
        case ANC_UPDATE_STATE_DISABLE_IND:
            DEBUG_LOG("handleAncClientEvent ANC_UPDATE_STATE_DISABLE_IND");
            audioCuration_SendEvent(AUDIO_CURATION_ANC_OFF);
            break;

        case ANC_UPDATE_STATE_ENABLE_IND:
            DEBUG_LOG("handleAncClientEvent ANC_UPDATE_STATE_ENABLE_IND");
            audioCuration_SendEvent(AUDIO_CURATION_ANC_ON);
            break;
        case ANC_UPDATE_MODE_CHANGED_IND:
            DEBUG_LOG("handleAncClientEvent ANC_UPDATE_MODE_CHANGED_IND");
            audioCuration_SendEvent(AUDIO_CURATION_ANC_MODE_CHANGED);
            break;
        
        case ANC_UPDATE_QUIETMODE_ON_IND :
            DEBUG_LOG("handleAncClientEvent ANC_UPDATE_QUIETMODE_ON_IND");
            audioCuration_SendEvent(AUDIO_CURATION_AANC_QUIET_MODE_ON);
            break;

        case ANC_UPDATE_QUIETMODE_OFF_IND :
            DEBUG_LOG("handleAncClientEvent AUDIO_CURATION_AANC_QUIET_MODE_OFF");
            audioCuration_SendEvent(AUDIO_CURATION_AANC_QUIET_MODE_OFF);  
            break;

        default:
            break;
    }
            
}

static void handleKymeraClientEvent(MessageId id, Message message)
{
    kymera_aanc_event_msg_t *ind = (kymera_aanc_event_msg_t*)message;
   
    DEBUG_LOG("handleKymeraClientEvent, MESSAGE:0x%x, Info: %d", id, ind->info);

    if ( diagnostic == FALSE )
        return;

    switch ( id )
    {
        case KYMERA_AANC_ED_INACTIVE_TRIGGER_IND :
            audioCuration_SendEvent(AUDIO_CURATION_AANC_ED_INACTIVE);
            break;

        case KYMERA_AANC_ED_ACTIVE_TRIGGER_IND :
            audioCuration_SendEvent(AUDIO_CURATION_AANC_ED_ACTIVE);
            break;

        case KYMERA_AANC_ED_ACTIVE_CLEAR_IND : /* fall through */
        case KYMERA_AANC_ED_INACTIVE_CLEAR_IND :
            audioCuration_SendEvent(AUDIO_CURATION_AANC_CLEAR_EVENT);
            break;

        default:
            break;

    }
    UNUSED(message);
}

static anc_mode_t getAncModeFromUiInput(MessageId ui_input)
{
    anc_mode_t mode;

    switch(ui_input)
    {
        case ui_input_anc_set_mode_1:
            mode = anc_mode_1;
            break;
        case ui_input_anc_set_mode_2:
            mode = anc_mode_2;
            break;
        case ui_input_anc_set_mode_3:
            mode = anc_mode_3;
            break;
        case ui_input_anc_set_mode_4:
            mode = anc_mode_4;
            break;
        case ui_input_anc_set_mode_5:
            mode = anc_mode_5;
            break;
        case ui_input_anc_set_mode_6:
            mode = anc_mode_6;
            break;
        case ui_input_anc_set_mode_7:
            mode = anc_mode_7;
            break;
        case ui_input_anc_set_mode_8:
            mode = anc_mode_8;
            break;
        case ui_input_anc_set_mode_9:
            mode = anc_mode_9;
            break;
        case ui_input_anc_set_mode_10:
            mode = anc_mode_10;
        break;
        default:
            mode = anc_mode_1;
            break;
    }
    return mode;
}

#ifdef ENABLE_AEC_LEAKTHROUGH
static leakthrough_mode_t getLeakthroughModeFromUiInput(MessageId ui_input)
{
    leakthrough_mode_t leakthrough_mode;

    switch(ui_input)
    {
        case ui_input_leakthrough_set_mode_1:
            leakthrough_mode = LEAKTHROUGH_MODE_1;
            break;
        case ui_input_leakthrough_set_mode_2:
            leakthrough_mode = LEAKTHROUGH_MODE_2;
            break;
        case ui_input_leakthrough_set_mode_3:
            leakthrough_mode = LEAKTHROUGH_MODE_3;
            break;
        default:
            leakthrough_mode = LEAKTHROUGH_MODE_1;
            break;
    }
    return leakthrough_mode;
}
#endif

/*! \brief provides ANC state machine context to the User Interface module.

    \param[in]  void

    \return     context - current context of ANC state machine.
*/
#ifdef ENABLE_ANC
static unsigned getAncCurrentContext(void)
{
    audio_curation_provider_context_t context = context_anc_disabled;
    
    if(AncStateManager_IsTuningModeActive())
    {
        context = context_anc_tuning_mode_active;
    }
    else if(AncStateManager_IsEnabled())
    {
        context = context_anc_enabled;
    }

    return (unsigned)context;
}
#endif

/*! Provides the leakthrough context to the User Interface module. */
#ifdef ENABLE_AEC_LEAKTHROUGH
static unsigned getLeakthroughCurrentContext(void)
{
    audio_curation_provider_context_t context = BAD_CONTEXT;
    if(AecLeakthrough_IsLeakthroughEnabled())
    {
        context = context_leakthrough_enabled;
    }
    else
    {
        context = context_leakthrough_disabled;
    }
    return (unsigned)context;
}
#endif

static void handlePowerClientEvents(MessageId id)
{
    switch(id)
    {
        /* Power indication */
        case APP_POWER_SHUTDOWN_PREPARE_IND:
            AncStateManager_PowerOff();
            AecLeakthrough_PowerOff();
            appPowerShutdownPrepareResponse(audioCuration_UiTask());
            break;

        case APP_POWER_SLEEP_PREPARE_IND:
            AncStateManager_PowerOff();
            AecLeakthrough_PowerOff();
            appPowerSleepPrepareResponse(audioCuration_UiTask());
            break;

        /*In case of Sleep/SHUTDOWN cancelled*/
        case APP_POWER_SHUTDOWN_CANCELLED_IND:
        case APP_POWER_SLEEP_CANCELLED_IND:
            AncStateManager_PowerOn();
            AecLeakthrough_PowerOn();
            break;

        default:
            break;
    }
}

static void handleAncUiDomainInput(MessageId ui_input)
{
    /* Do not inject ANC events if Fit Test in progress */
    if(FitTest_IsRunning())
        return;


    switch(ui_input)
    {
        case ui_input_anc_on:
            DEBUG_LOG("handleAncUiDomainInput, anc on input");
            AncStateManager_Enable();
            break;

        case ui_input_anc_off:
            DEBUG_LOG("handleAncUiDomainInput, anc off input");
            AncStateManager_Disable();
            break;

        case ui_input_anc_toggle_on_off:
            DEBUG_LOG("handleAncUiDomainInput, anc toggle on/off input");
            if(AncStateManager_IsEnabled())
            {
                AncStateManager_Disable();
            }
            else
            {
                AncStateManager_Enable();
            }
            break;
            
        case ui_input_anc_toggle_way:
            DEBUG_LOG("handleUiDomainInput, ui_input_anc_toggle_way");
            AncStateManager_HandleToggleWay();
            break;

        case ui_input_anc_set_mode_1:
        case ui_input_anc_set_mode_2:
        case ui_input_anc_set_mode_3:
        case ui_input_anc_set_mode_4:
        case ui_input_anc_set_mode_5:
        case ui_input_anc_set_mode_6:
        case ui_input_anc_set_mode_7:
        case ui_input_anc_set_mode_8:
        case ui_input_anc_set_mode_9:
        case ui_input_anc_set_mode_10:
            DEBUG_LOG("handleAncUiDomainInput, anc set mode input");
            AncStateManager_SetMode(getAncModeFromUiInput(ui_input));
            break;

        case ui_input_anc_set_next_mode:
            DEBUG_LOG("handleAncUiDomainInput, anc next mode input");
            AncStateManager_SetNextMode();
            break;

        case ui_input_anc_enter_tuning_mode:
            DEBUG_LOG("handleAncUiDomainInput, enter anc tuning input");
            AncStateManager_EnterAncTuningMode();
            break;

        case ui_input_anc_exit_tuning_mode:
            DEBUG_LOG("handleAncUiDomainInput, exit anc tuning input");
            AncStateManager_ExitAncTuningMode();
            break;

        case ui_input_anc_enter_adaptive_anc_tuning_mode:
            DEBUG_LOG("handleAncUiDomainInput, enter adaptive anc tuning input");
            AncStateManager_EnterAdaptiveAncTuningMode();
            break;

        case ui_input_anc_exit_adaptive_anc_tuning_mode:
            DEBUG_LOG("handleAncUiDomainInput, exit adaptive anc tuning input");
            AncStateManager_ExitAdaptiveAncTuningMode();
            break;

        case ui_input_anc_set_leakthrough_gain:
            DEBUG_LOG("handleUiDomainInput, set anc leakthrough gain input");
            AncStateManager_UpdateAncLeakthroughGain();
            break;

        case ui_input_anc_adaptivity_toggle_on_off:
            DEBUG_LOG("handleAncUiDomainInput, anc adaptivity toggle on/off input");
            if(AncStateManager_GetAdaptiveAncAdaptivity() == TRUE)
            {
                AncStateManager_DisableAdaptiveAncAdaptivity();
            }
            else
            {
                AncStateManager_EnableAdaptiveAncAdaptivity();
            }
            break;

        case ui_input_anc_toggle_diagnostic:
            DEBUG_LOG("handleAncUiDomainInput, toggle diagnostic");
            if ( diagnostic )
            {
                Kymera_ClientUnregister(audioCuration_UiTask());
            }
            else
            {
                Kymera_ClientRegister(audioCuration_UiTask());
            }
            diagnostic = !diagnostic;
            break;

        default:
            break;
    }
}

static void handleAecLeakthroughUiDomainInput(MessageId ui_input)
{
    switch(ui_input)
    {
        /* Leak-through UI Input events */
        case ui_input_leakthrough_on:
            DEBUG_LOG("handleAecLeakthroughUiDomainInput, leakthrough on input");
            AecLeakthrough_Enable();
            break;

        case ui_input_leakthrough_off:
            DEBUG_LOG("handleAecLeakthroughUiDomainInput, leakthrough off input");
            AecLeakthrough_Disable();
            break;

        case ui_input_leakthrough_toggle_on_off:
            DEBUG_LOG("handleAecLeakthroughUiDomainInput, leakthrough toggle on/off input");
            if(AecLeakthrough_IsLeakthroughEnabled())
            {
                AecLeakthrough_Disable();
            }
            else
            {
                AecLeakthrough_Enable();
            }
            break;

        case ui_input_leakthrough_set_mode_1:
        case ui_input_leakthrough_set_mode_2:
        case ui_input_leakthrough_set_mode_3:
            DEBUG_LOG("handleAecLeakthroughUiDomainInput, leakthrough set mode input");
            AecLeakthrough_SetMode(getLeakthroughModeFromUiInput(ui_input));
            break;

        case ui_input_leakthrough_set_next_mode:
            DEBUG_LOG("handleAecLeakthroughUiDomainInput, leakthrough set next mode input");
            AecLeakthrough_SetNextMode();
            break;

        default:
            break;
    }
}

static void handleFitTestUiDomainInput(MessageId ui_input)
{
    switch(ui_input)
    {
        case ui_input_fit_test_prepare_test:
            if(!FitTest_MakeTestReady())
                DEBUG_LOG("handleFitTestUiDomainInput, either fit test build is not included or event is ignored");
        break;

        case ui_input_fit_test_start:
            if(!FitTest_StartTest())
                DEBUG_LOG("handleFitTestUiDomainInput, either fit test build is not included or event is ignored");
        break;

        case ui_input_fit_test_abort:
            if(!FitTest_AbortTest())
                DEBUG_LOG("handleFitTestUiDomainInput, either fit test build is not included or event is ignored");
        break;

        case ui_input_fit_test_disable:
            if(!FitTest_DisableTest())
                DEBUG_LOG("handleFitTestUiDomainInput, either fit test build is not included or event is ignored");
        break;

        case ui_input_fit_test_remote_result_ready:
            if(!FitTest_InformClients())
                DEBUG_LOG("handleFitTestUiDomainInput, either fit test build is not included or event is ignored");
        break;

        case ui_input_fit_test_enter_tuning_mode:
            if(!FitTest_EnterFitTestTuningMode())
                DEBUG_LOG("handleFitTestUiDomainInput, either fit test build is not included or event is ignored");
        break;

        case ui_input_fit_test_exit_tuning_mode:
            if(!FitTest_ExitFitTestTuningMode())
                DEBUG_LOG("handleFitTestUiDomainInput, either fit test build is not included or event is ignored");
        break;

        default:
            DEBUG_LOG_ALWAYS("handleFitTestUiInput Unhandled ui input");
        break;
    }
}

static void handleLeAudioUiDomainInput(MessageId ui_input)
{
    switch(ui_input)
    {
        case ui_input_le_audio_disable_anc:
            DEBUG_LOG("handleLeAudioUiDomainInput, ui_input_le_audio_disable_anc");
            AncStateManager_Disable();
        break;

        case ui_input_le_audio_enable_anc:
            DEBUG_LOG("handleLeAudioUiDomainInput, ui_input_le_audio_enable_anc");
            AncStateManager_Enable();
        break;

        default:
            DEBUG_LOG_ALWAYS("handleLeAudioUiDomainInput Unhandled ui input");
        break;
    }
}

static void handleUiDomainInput(MessageId ui_input)
{
    if((ui_input >= ui_input_anc_on) && (ui_input <= ui_input_anc_toggle_diagnostic))
    {
        handleAncUiDomainInput(ui_input);
    }
    else if((ui_input >= ui_input_leakthrough_on) && (ui_input <= ui_input_leakthrough_set_next_mode))
    {
        handleAecLeakthroughUiDomainInput(ui_input);
    }
    else if((ui_input >= ui_input_fit_test_prepare_test) && (ui_input <= ui_input_fit_test_exit_tuning_mode))
    {
        handleFitTestUiDomainInput(ui_input);
    }
    else if((ui_input == ui_input_le_audio_disable_anc) || (ui_input == ui_input_le_audio_enable_anc))
    {
        handleLeAudioUiDomainInput(ui_input);
    }
    else
    {
        DEBUG_LOG("handleUiDomainInput, unhandled input");
    }
}

static unsigned getCurrentContext(void)
{
#ifdef ENABLE_AEC_LEAKTHROUGH
    return getLeakthroughCurrentContext();
#elif defined ENABLE_ANC
    return getAncCurrentContext();
#else
    return (unsigned)BAD_CONTEXT;
#endif
}


static void audioCuration_HandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);

    if (ID_IN_MSG_GRP(POWER_APP, id))
    {
        handlePowerClientEvents(id);
    }
    else if (ID_IN_MSG_GRP(ANC, id))
    {
        handleAncClientEvent(id);
    }
    else if (ID_IN_MSG_GRP(KYMERA, id))
    {
        handleKymeraClientEvent(id, message);
    }
    else if (ID_IN_MSG_GRP(UI_INPUTS_AUDIO_CURATION, id))
    {
        handleUiDomainInput(id);
    }
}


/*! \brief Initialise the audio curation service

    \param[in]  init_task - added to inline with app_init

    \return     bool - returns initalization status
*/
bool AudioCuration_Init(Task init_task)
{
    Ui_RegisterUiProvider(ui_provider_audio_curation, getCurrentContext);

    Ui_RegisterUiInputConsumer(audioCuration_UiTask(), ui_inputs, ARRAY_DIM(ui_inputs));

    DEBUG_LOG("AudioCuration_Init, called");
#ifdef CORVUS_YD300
    diagnostic = TRUE;
    Kymera_ClientRegister(audioCuration_UiTask());
#else
    diagnostic = FALSE;
#endif
    /* register with power to receive shutdown messages. */
    appPowerClientRegister(audioCuration_UiTask());
    appPowerClientAllowSleep(audioCuration_UiTask());
    audioCuration_InitMessages();
    AncStateManager_ClientRegister(audioCuration_UiTask());
    if(!audioCuration_IsStereo())
    {
        /* AEC-Leakthrough Post Init setup for peer sync */
        AecLeakthrough_PostInitSetup();
        /* Post Init setup for ANC peer sync */
        AncStateManager_PostInitSetup();
    }
    
    UNUSED(init_task);
    
    return TRUE;
}

static void audioCuration_RegisterMessageGroup(Task task, message_group_t group)
{
    PanicFalse(group == AUDIO_CURATION_SERVICE_MESSAGE_GROUP);
    TaskList_AddTask(audioCuration_GetMessageClients(), task);
}

MESSAGE_BROKER_GROUP_REGISTRATION_MAKE(AUDIO_CURATION_SERVICE, audioCuration_RegisterMessageGroup, NULL);



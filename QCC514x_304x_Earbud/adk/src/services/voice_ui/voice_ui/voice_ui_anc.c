/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       voice_ui_anc.c
\brief      Implementation of the voice UI ANC handling.
*/

#ifdef ENABLE_ANC

#include <logging.h>
#include "voice_ui_anc.h"
#include "anc_state_manager.h"
#include "voice_ui_container.h"

/*! Macro for creating messages */
#define MAKE_MESSAGE(TYPE) \
    TYPE##_T *message = PanicUnlessNew(TYPE##_T);

#define INVALID_MODE 0xFF

static void voiceUi_AncMessageHandler(Task task, MessageId id, Message message);
static void voiceUi_InternalAncMessageHandler(Task task, MessageId id, Message message);
static void voiceUi_NotifyEnableChange(void);
static void voiceUi_NotifyGainChange(void);
static bool voiceUi_IsModeLeakthrough(anc_mode_t mode);
static bool voiceUi_IsModeStaticAnc(anc_mode_t mode);
static void voiceUi_SetAncMode(anc_mode_t anc_mode);
static void voiceUi_EnableAncMode(anc_mode_t mode);
static uint8 voiceUi_ConvertPercentageToGain(uint8 percentage, uint8 max_gain);
static bool voiceUi_IsLeakthroughModeInitialised(void);
static void voiceUi_InitialiseLeakthroughMaxGain(void);
static void voiceUi_SetLeakthroughGain(uint8 gain_as_percentage);

static TaskData voice_ui_anc_task_data = { .handler = voiceUi_AncMessageHandler };
static TaskData voice_ui_anc_internal_task_data = { .handler = voiceUi_InternalAncMessageHandler };

static uint8 leakthrough_max_gain = 0;
static anc_mode_t leakthrough_mode = (anc_mode_t)INVALID_MODE;

static anc_mode_t static_anc_mode = (anc_mode_t)INVALID_MODE;

static void voiceUi_AncMessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);

    DEBUG_LOG("voiceUi_AncMessageHandler enum:anc_msg_t:%d", id);
    switch(id)
    {
        case ANC_UPDATE_STATE_ENABLE_IND:
        {
            if(!voiceUi_IsLeakthroughModeInitialised() && (AncStateManager_GetCurrentMode() == leakthrough_mode))
            {
                voiceUi_InitialiseLeakthroughMaxGain();
            }

            voiceUi_NotifyEnableChange();
        }
        break;

        case ANC_UPDATE_STATE_DISABLE_IND:
            voiceUi_NotifyEnableChange();
            break;

        case ANC_UPDATE_MODE_CHANGED_IND:
        {
            if(!voiceUi_IsLeakthroughModeInitialised() && (AncStateManager_GetCurrentMode() == leakthrough_mode))
            {
                voiceUi_InitialiseLeakthroughMaxGain();
            }

            voiceUi_NotifyEnableChange();
        }
        break;

        case ANC_UPDATE_GAIN_IND:
            voiceUi_NotifyGainChange();
            break;

        default:
            break;
    }
}

static void voiceUi_InternalAncMessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);

    DEBUG_LOG("voiceUi_InternalAncMessageHandler enum:voice_ui_anc_internal_message_t:%d", id);
    switch(id)
    {
        case VOICE_UI_ENABLE_STATIC_ANC:
            voiceUi_EnableAncMode(static_anc_mode);
            break;

        case VOICE_UI_ENABLE_LEAKTHROUGH:
            voiceUi_EnableAncMode(leakthrough_mode);
            break;

        case VOICE_UI_DISABLE_ANC_AND_LEAKTHROUGH:
            Ui_InjectUiInput(ui_input_anc_off);
            voiceUi_NotifyEnableChange();
            break;

        case VOICE_UI_SET_LEAKTHROUGH_GAIN:
        {
            const VOICE_UI_SET_LEAKTHROUGH_GAIN_T * msg = (const VOICE_UI_SET_LEAKTHROUGH_GAIN_T *)message;

            if(voiceUi_IsLeakthroughModeInitialised() && msg)
            {
                if(!VoiceUi_IsLeakthroughEnabled())
                {
                    voiceUi_EnableAncMode(leakthrough_mode);
                }

                voiceUi_SetLeakthroughGain(msg->gain_as_percentage);
            }
        }
        break;

        default:
            break;
    }
}

static bool voiceUi_IsModeLeakthrough(anc_mode_t mode)
{
    return AncConfig_IsAncModeStatic(mode) && AncConfig_IsAncModeLeakThrough(mode);
}

static bool voiceUi_IsModeStaticAnc(anc_mode_t mode)
{
    return AncConfig_IsAncModeStatic(mode) && !AncConfig_IsAncModeLeakThrough(mode);
}

static uint8 voiceUi_ConvertGainToPercentage(uint8 gain, uint8 max_gain)
{
    uint16 percentage = (gain * 100) / max_gain;

    if (percentage > 100)
    {
        DEBUG_LOG_WARN("voiceUi_ConvertPercentageToGain invalid percentage %u%%", percentage);
        percentage = 100;
    }

    return (uint8) percentage;
}

static uint8 voiceUi_ConvertPercentageToGain(uint8 percentage, uint8 max_gain)
{
    uint16 gain;

    if (percentage > 100)
    {
        DEBUG_LOG_WARN("voiceUi_ConvertPercentageToGain invalid percentage %u%%", percentage);
        gain = max_gain;
    }
    else
    {
        gain = (percentage * max_gain) / 100;
    }

    return (uint8) gain;
}

static void voiceUi_SetLeakthroughGain(uint8 gain_as_percentage)
{
    uint8 gain = voiceUi_ConvertPercentageToGain(gain_as_percentage, leakthrough_max_gain);
    AncStateManager_StoreAncLeakthroughGain(gain);
    Ui_InjectUiInput(ui_input_anc_set_leakthrough_gain);
    DEBUG_LOG_VERBOSE("voiceUi_SetLeakthroughGain %d = %d%%", gain, gain_as_percentage);
}

static void voiceUi_NotifyAncEnableChange(void)
{
    voice_ui_handle_t* handle = VoiceUi_GetActiveVa();

    if (handle && handle->voice_assistant && handle->voice_assistant->AncEnableUpdate)
    {
        if(VoiceUi_IsStaticAncEnabled())
            handle->voice_assistant->AncEnableUpdate(TRUE);
        else
            handle->voice_assistant->AncEnableUpdate(FALSE);
    }
}

static void voiceUi_NotifyLeakthroughEnableChange(void)
{
    voice_ui_handle_t* handle = VoiceUi_GetActiveVa();

    if (handle && handle->voice_assistant && handle->voice_assistant->LeakthroughEnableUpdate)
    {
        if(VoiceUi_IsLeakthroughEnabled())
            handle->voice_assistant->LeakthroughEnableUpdate(TRUE);
        else
            handle->voice_assistant->LeakthroughEnableUpdate(FALSE);
    }
}

static void voiceUi_NotifyEnableChange(void)
{
    voiceUi_NotifyAncEnableChange();
    voiceUi_NotifyLeakthroughEnableChange();
}

static void voiceUi_NotifyGainChange(void)
{
    voice_ui_handle_t* handle = VoiceUi_GetActiveVa();

    if(voiceUi_IsModeLeakthrough(AncStateManager_GetCurrentMode()) && handle && handle->voice_assistant && handle->voice_assistant->LeakthroughGainUpdate)
    {
        handle->voice_assistant->LeakthroughGainUpdate(VoiceUi_GetLeakthroughLevelAsPercentage());
    }
}

static void voiceUi_SetAncMode(anc_mode_t anc_mode)
{
    DEBUG_LOG("voiceUi_SetAncMode");

    switch(anc_mode)
    {
        case anc_mode_1:
            Ui_InjectUiInput(ui_input_anc_set_mode_1);
            break;
        case anc_mode_2:
            Ui_InjectUiInput(ui_input_anc_set_mode_2);
            break;
        case anc_mode_3:
            Ui_InjectUiInput(ui_input_anc_set_mode_3);
            break;
        case anc_mode_4:
            Ui_InjectUiInput(ui_input_anc_set_mode_4);
            break;
        case anc_mode_5:
            Ui_InjectUiInput(ui_input_anc_set_mode_5);
            break;
        case anc_mode_6:
            Ui_InjectUiInput(ui_input_anc_set_mode_6);
            break;
        case anc_mode_7:
            Ui_InjectUiInput(ui_input_anc_set_mode_7);
            break;
        case anc_mode_8:
            Ui_InjectUiInput(ui_input_anc_set_mode_8);
            break;
        case anc_mode_9:
            Ui_InjectUiInput(ui_input_anc_set_mode_9);
            break;
        case anc_mode_10:
            Ui_InjectUiInput(ui_input_anc_set_mode_10);
            break;
        default:
            Ui_InjectUiInput(ui_input_anc_set_mode_1);
            break;
    }
}

static anc_mode_t voiceUi_GetSpecificAncModeType(bool (*isAncModeType)(anc_mode_t))
{
    DEBUG_LOG_FN_ENTRY("voiceUi_GetSpecificAncModeType");

    PanicNull((void *) isAncModeType);

    anc_mode_t requested_mode = INVALID_MODE;
    anc_mode_t intial_mode = AncStateManager_GetCurrentMode();

    if(isAncModeType(intial_mode))
    {
         requested_mode = intial_mode;
    }
    else
    {
        for(anc_mode_t mode = anc_mode_1; mode <= anc_mode_10; mode++)
        {
            if(isAncModeType(mode))
            {
                DEBUG_LOG("voiceUi_GetSpecificAncModeType enum:anc_mode_t:%d", mode);
                requested_mode = mode;
                break;
            }
        }

        if(!requested_mode == INVALID_MODE)
        {
            DEBUG_LOG_WARN("voiceUi_GetSpecificAncModeType requested mode not found");
        }
    }

    return requested_mode;
}

static void voiceUi_EnableAncMode(anc_mode_t mode)
{
    if(mode == INVALID_MODE)
    {
        DEBUG_LOG_WARN("voiceUi_EnableAncMode invalid mode");
    }
    else
    {
        voiceUi_SetAncMode(mode);

        if(!AncStateManager_IsEnabled())
        {
            Ui_InjectUiInput(ui_input_anc_on);
        }

        voiceUi_NotifyEnableChange();
    }
}

static bool voiceUi_IsLeakthroughModeInitialised(void)
{
    return (leakthrough_max_gain != 0) && (leakthrough_mode != INVALID_MODE);
}

static void voiceUi_InitialiseAncModeType(anc_mode_t * mode_to_populate, bool (*isAncModeType)(anc_mode_t))
{
    PanicNull((void *) isAncModeType);
    PanicNull(mode_to_populate);

    anc_mode_t first_mode_of_type = voiceUi_GetSpecificAncModeType(isAncModeType);

    if(first_mode_of_type != INVALID_MODE)
    {
        *mode_to_populate =  first_mode_of_type;
        DEBUG_LOG("voiceUi_InitialiseAncModeType enum:anc_mode_t:%d", *mode_to_populate);
    }
}

static void voiceUi_InitialiseLeakthroughMaxGain(void)
{
    leakthrough_max_gain = AncStateManager_GetAncGain();
    DEBUG_LOG("voiceUi_InitialiseLeakthroughMaxGain enum:anc_mode_t:%d %u", leakthrough_mode, leakthrough_max_gain);
}

void VoiceUi_AncInit(void)
{
    DEBUG_LOG("VoiceUi_AncInit");
    AncStateManager_ClientRegister((Task)&voice_ui_anc_task_data);

    /* Find and store static ANC mode */
    voiceUi_InitialiseAncModeType(&static_anc_mode, voiceUi_IsModeStaticAnc);

    /* Find and store leakthrough mode, store default gain as maximum gain */
    voiceUi_InitialiseAncModeType(&leakthrough_mode, voiceUi_IsModeLeakthrough);
}

bool VoiceUi_IsStaticAncEnabled(void)
{
    anc_mode_t current_mode = AncStateManager_GetCurrentMode();
    bool enabled = AncStateManager_IsEnabled() && (current_mode == static_anc_mode);
    DEBUG_LOG("VoiceUi_IsStaticAncEnabled %d", enabled);
    return enabled;
}

void VoiceUi_EnableStaticAnc(void)
{
    DEBUG_LOG("VoiceUi_AncSetEnabled");
    MessageSend((Task)&voice_ui_anc_internal_task_data, VOICE_UI_ENABLE_STATIC_ANC, NULL);
}

void VoiceUi_DisableStaticAnc(void)
{
    DEBUG_LOG("VoiceUi_DisableStaticAnc");
    if(VoiceUi_IsStaticAncEnabled())
    {
        MessageSend((Task)&voice_ui_anc_internal_task_data, VOICE_UI_DISABLE_ANC_AND_LEAKTHROUGH, NULL);
    }
}

bool VoiceUi_IsLeakthroughEnabled(void)
{
    anc_mode_t current_mode = AncStateManager_GetCurrentMode();
    bool enabled = AncStateManager_IsEnabled() && (current_mode == leakthrough_mode);
    DEBUG_LOG("VoiceUi_IsLeakthroughEnabled %d", enabled);
    return enabled; 
}

void VoiceUi_EnableLeakthrough(void)
{
    DEBUG_LOG("VoiceUi_EnableLeakthrough");
    MessageSend((Task)&voice_ui_anc_internal_task_data, VOICE_UI_ENABLE_LEAKTHROUGH, NULL);
}

void VoiceUi_DisableLeakthrough(void)
{
    DEBUG_LOG("VoiceUi_DisableLeakthrough");
    if(VoiceUi_IsLeakthroughEnabled())
    {
        MessageSend((Task)&voice_ui_anc_internal_task_data, VOICE_UI_DISABLE_ANC_AND_LEAKTHROUGH, NULL);
    }
}

uint8 VoiceUi_GetLeakthroughLevelAsPercentage(void)
{
    uint8 percentage = 0;

    if(voiceUi_IsLeakthroughModeInitialised() && VoiceUi_IsLeakthroughEnabled())
    {
        uint8 gain = AncStateManager_GetAncGain();
        percentage = voiceUi_ConvertGainToPercentage(gain, leakthrough_max_gain);
        DEBUG_LOG_VERBOSE("VoiceUi_GetLeakthroughLevelAsPercentage %d = %d%%", gain, percentage);
    }

    return percentage;
}

void VoiceUi_SetLeakthroughLevelFromPercentage(uint8 level_as_percentage)
{
    DEBUG_LOG("VoiceUi_SetLeakthroughLevelFromPercentage %d%%", level_as_percentage);
    MAKE_MESSAGE(VOICE_UI_SET_LEAKTHROUGH_GAIN);
    message->gain_as_percentage = level_as_percentage;
    MessageSend((Task)&voice_ui_anc_internal_task_data, VOICE_UI_SET_LEAKTHROUGH_GAIN, message);
}

#endif /* ENABLE_ANC */

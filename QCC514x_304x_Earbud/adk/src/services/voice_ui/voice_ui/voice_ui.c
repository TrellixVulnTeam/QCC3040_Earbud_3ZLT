/*!
\copyright  Copyright (c) 2019 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       voice_ui.c
\brief      Implementation of voice ui service.
*/

#include "logging.h"
#include <task_list.h>
#include <csrtypes.h>
#include <panic.h>
#include <power_manager.h>
#include <vmtypes.h>
#include <feature.h>
#include <va_profile.h>

#include "ui.h"

#include "voice_ui.h"
#include "voice_ui_config.h"
#include "voice_ui_container.h"
#include "voice_ui_audio.h"
#include "voice_ui_battery.h"
#include "voice_ui_anc.h"
#include "voice_ui_eq.h"
#include "voice_ui_session.h"
#ifdef INCLUDE_GAIA
#include "voice_ui_gaia_plugin.h"
#endif /* INCLUDE_GAIA */
#include "voice_ui_peer_sig.h"
#include "feature_manager.h"
#include <device_properties.h>
#include <device_db_serialiser.h>
#include <system_reboot.h>

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_TYPE(voice_ui_msg_id_t)
ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(VOICE_UI_SERVICE, VOICE_UI_SERVICE_MESSAGE_END)

#define VOICE_UI_REBOOT_DELAY_MILLISECONDS ((Delay) 250)

static feature_state_t voiceUi_GetFeatureState(void);
static void voiceUi_HandleMessage(Task task, MessageId id, Message message);
static const TaskData msg_handler = {voiceUi_HandleMessage};

static const feature_interface_t feature_manager_if =
{
    .GetState = voiceUi_GetFeatureState,
    .Suspend = VoiceUi_SuspendAudio,
    .Resume = VoiceUi_ResumeAudio
};

/* Ui Inputs in which voice ui service is interested*/
static const uint16 voice_ui_inputs[] =
{
    ID_TO_MSG_GRP(UI_INPUTS_VOICE_UI_MESSAGE_BASE),
};

static task_list_t * voice_ui_client_list;

static Task voiceUi_GetTask(void)
{
    return (Task)&msg_handler;
}

static void voiceUi_HandleUiInput(MessageId ui_input)
{
    voice_ui_handle_t* handle = VoiceUi_GetActiveVa();

    if(handle)
    {
        VoiceUi_EventHandler(handle, ui_input);
    }
}

static unsigned voiceUi_GetUiContext(void)
{
    return (unsigned)context_voice_ui_default;
}

static uint8_t voiceUi_GetDefaultFlags(void)
{
    return device_va_flag_wuw_enabled;
}

static void voiceUi_HandleBtDeviceSelfCreated(BT_DEVICE_SELF_CREATED_IND_T *ind)
{
    DEBUG_LOG_DEBUG("voiceUi_HandleBtDeviceSelfCreated");
    device_t device = PanicNull(ind->device);

    uint8_t flags = voiceUi_GetDefaultFlags();
    Device_SetPropertyU8(device, device_property_va_flags, flags);

    uint8 va_locale[DEVICE_SIZEOF_VA_LOCALE] = {0};

    Device_SetProperty(device, device_property_va_locale, va_locale, DEVICE_SIZEOF_VA_LOCALE);

    Device_SetPropertyU8(device, device_property_voice_assistant, VOICE_UI_PROVIDER_DEFAULT);

    DEBUG_LOG("voiceUi_HandleBtDeviceSelfCreated, setting defaults: "
              "device_property_va_flags: 0x%x, device_property_va_locale: \"\", device_property_voice_assistant: enum:voice_ui_provider_t:%d",
              flags, VOICE_UI_PROVIDER_DEFAULT);

    DeviceDbSerialiser_Serialise();
}

static void voiceUi_HandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);

    if (isMessageUiInput(id))
    {
        voiceUi_HandleUiInput(id);
        return;
    }

    switch (id)
    {
    case VOICE_UI_INTERNAL_REBOOT:
        SystemReboot_RebootWithAction(reboot_action_active_state);
        break;

    case BT_DEVICE_SELF_CREATED_IND:
        voiceUi_HandleBtDeviceSelfCreated((BT_DEVICE_SELF_CREATED_IND_T*) message);

    default:
        DEBUG_LOG_DEBUG("voiceUi_HandleMessage: unhandled MESSAGE:0x%04X", id);
        break;
    }
}

static void voiceUi_licenseCheck(void)
{
    if (VoiceUi_IsTwsFeatureIncluded())
    {   /* Earbuds */
        if (FeatureVerifyLicense(TDFBC_MONO))
        {
            DEBUG_LOG_VERBOSE("voiceUi_licenseCheck: TDFBC_MONO is licensed");
        }
        else
        {
            DEBUG_LOG_WARN("voiceUi_licenseCheck: TDFBC_MONO not licensed");
        }
    }
    else
    {   /* Headset */
        if (FeatureVerifyLicense(TDFBC))
        {
            DEBUG_LOG_VERBOSE("voiceUi_licenseCheck: TDFBC is licensed");
        }
        else
        {
            DEBUG_LOG_WARN("voiceUi_licenseCheck: TDFBC not licensed");
        }
    }

    if(VoiceUi_IsWakeUpWordFeatureIncluded())
    {
        if (VoiceUi_IsTwsFeatureIncluded())
        {   /* Earbuds */
            if (FeatureVerifyLicense(VAD_MONO))
            {
                DEBUG_LOG_VERBOSE("voiceUi_licenseCheck: VAD_MONO is licensed");
            }
            else
            {
                DEBUG_LOG_WARN("voiceUi_licenseCheck: VAD_MONO not licensed");
                if (LOG_LEVEL_CURRENT_SYMBOL >= DEBUG_LOG_LEVEL_VERBOSE)
                {
                    Panic();
                }
            }
        }
        else
        {   /* Headset */
            if (FeatureVerifyLicense(VAD))
            {
                DEBUG_LOG_VERBOSE("voiceUi_licenseCheck: VAD is licensed");
            }
            else
            {
                DEBUG_LOG_WARN("voiceUi_licenseCheck: VAD not licensed");
                if (LOG_LEVEL_CURRENT_SYMBOL >= DEBUG_LOG_LEVEL_VERBOSE)
                {
                    Panic();
                }
            }
        }
    }
}

static const bdaddr * voiceUi_GetVaBtAddress(void)
{
    voice_ui_handle_t* handle = VoiceUi_GetActiveVa();

    if (handle && handle->voice_assistant->GetBtAddress)
    {
        return handle->voice_assistant->GetBtAddress();
    }

    return NULL;
}

static bool voiceUi_IsVaActiveAtBdaddr(const bdaddr * bd_addr)
{
    bool is_active = FALSE;
    const bdaddr * va_addr = voiceUi_GetVaBtAddress();

    if (va_addr)
    {
        is_active = BdaddrIsSame(va_addr, bd_addr) && VoiceUi_IsVaActive();
    }

    DEBUG_LOG("voiceUi_IsVaActiveAtBdaddr %u", is_active);
    return is_active;
}

static feature_state_t voiceUi_GetFeatureState(void)
{
    feature_state_t state = feature_state_idle;

    if(VoiceUi_IsVaActive())
    {
        state = feature_state_running;
    }

    if(VoiceUi_IsAudioSuspended(VoiceUi_GetActiveVa()))
    {
        state = feature_state_suspended;
    }

    return state;
}

bool VoiceUi_Init(Task init_task)
{
    DEBUG_LOG("VoiceUi_Init()");

    UNUSED(init_task);

    voice_ui_client_list = TaskList_Create();
#ifdef INCLUDE_GAIA
    VoiceUiGaiaPlugin_Init();
#endif
    VoiceUi_PeerSignallingInit();

    VoiceUi_SetFeatureManagerHandle(FeatureManager_Register(feature_id_va, &feature_manager_if));
    VaProfile_RegisterClient(&voiceUi_IsVaActiveAtBdaddr);

    /* Register av task call back as ui provider*/
    Ui_RegisterUiProvider(ui_provider_voice_ui, voiceUi_GetUiContext);

    Ui_RegisterUiInputConsumer(voiceUi_GetTask(), (uint16*)voice_ui_inputs, sizeof(voice_ui_inputs)/sizeof(uint16));

    voiceUi_licenseCheck();

    VoiceUi_AudioInit();
    VoiceUi_BatteryInit();
#ifdef ENABLE_ANC
    VoiceUi_AncInit();
#endif
#ifdef INCLUDE_MUSIC_PROCESSING
    VoiceUi_EqInit();
#endif

    VoiceUi_VaSessionInit();

    BtDevice_RegisterListener(voiceUi_GetTask());

    return TRUE;
}

void VoiceUi_Notify(voice_ui_msg_id_t msg)
{
    TaskList_MessageSendId(voice_ui_client_list, msg);
}

static void voiceAssistant_RegisterMessageGroup(Task task, message_group_t group)
{
    PanicFalse(group == VOICE_UI_SERVICE_MESSAGE_GROUP);
    TaskList_AddTask(voice_ui_client_list, task);
}

void VoiceUi_RebootLater(void)
{
    MessageSendLater(voiceUi_GetTask(), VOICE_UI_INTERNAL_REBOOT, NULL, VOICE_UI_REBOOT_DELAY_MILLISECONDS);
}

void VoiceUi_AssistantConnected(void)
{
    VoiceUi_UpdateHfpState();
}

MESSAGE_BROKER_GROUP_REGISTRATION_MAKE(VOICE_UI_SERVICE, voiceAssistant_RegisterMessageGroup, NULL);

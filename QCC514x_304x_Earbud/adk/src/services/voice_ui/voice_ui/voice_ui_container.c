/*!
\copyright  Copyright (c) 2019 - 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of the voice assistants.
*/
#include <panic.h>
#include <stdlib.h>
#include <logging.h>
#include "voice_ui_config.h"
#include "voice_ui_container.h"
#ifdef INCLUDE_GAIA
#include "voice_ui_gaia_plugin.h"
#endif /* INCLUDE_GAIA */
#include "voice_ui_peer_sig.h"
#include "voice_ui_audio.h"
#include "voice_ui.h"
#include "voice_ui_session.h"

#include <bt_device.h>
#include <device_db_serialiser.h>
#include <device_properties.h>
#include <logging.h>

static voice_ui_handle_t *active_va = NULL;
static feature_manager_handle_t feature_manager_handle = NULL;

/*! \brief Container that holds the handle to created voice assistants */
static voice_ui_handle_t* voice_assistant_list[MAX_NO_VA_SUPPORTED ? MAX_NO_VA_SUPPORTED : 1] = {0};

static voice_ui_handle_t* voiceUi_FindHandleForProvider(voice_ui_provider_t provider_name);
static voice_ui_handle_t* voiceUi_GetHandleFromProvider(voice_ui_provider_t provider_name);
static bool voiceUi_ProviderIsValid(voice_ui_provider_t provider_name);

static void voiceUi_SetActiveVa(voice_ui_handle_t *va)
{
    if (active_va != va)
    {
        VoiceUi_UnrouteAudio();
        VoiceUi_VaSessionReset();
    }
    active_va = va;
}

static voice_ui_handle_t* voiceUi_GetActiveVa(void)
{
    return active_va;
}

static device_t voiceUi_GetMyDevice(void)
{
    bdaddr bd_addr;

    appDeviceGetMyBdAddr(&bd_addr);
    return BtDevice_GetDeviceForBdAddr(&bd_addr);
}

static voice_ui_handle_t* voiceUi_FindHandleForProvider(voice_ui_provider_t provider_name)
{
    /* Find corresponding handle in the container */
    voice_ui_handle_t *va_provider_handle = NULL;
    for (uint8 va_index = 0; va_index < MAX_NO_VA_SUPPORTED; va_index++)
    {
        voice_ui_handle_t* va_handle = voice_assistant_list[va_index];
        if (va_handle && va_handle->voice_assistant)
        {
            if (va_handle->voice_assistant->va_provider == provider_name)
            {
                va_provider_handle = va_handle;
                break;
            }
        }
    }

    return va_provider_handle;
}

static voice_ui_handle_t* voiceUi_GetHandleFromProvider(voice_ui_provider_t provider_name)
{
    return PanicNull(voiceUi_FindHandleForProvider(provider_name));
}

static bool voiceUi_ProviderIsValid(voice_ui_provider_t provider_name)
{
    return voiceUi_FindHandleForProvider(provider_name) != NULL;
}

void VoiceUi_SetSelectedVoiceAssistantInterface(voice_ui_provider_t va_provider)
{
    PanicFalse(va_provider != voice_ui_provider_none);
    voiceUi_SetActiveVa(voiceUi_GetHandleFromProvider(va_provider));
    voiceUi_GetActiveVa()->voice_assistant->SelectVoiceAssistant();
}

static void voiceui_MarshallSelectedAssistant(bool reboot)
{
    if(VoiceUi_IsTwsFeatureIncluded())
    {
        if(BtDevice_IsMyAddressPrimary())
        {
            VoiceUi_UpdateSelectedPeerVaProvider(reboot);
        }
    }
}

void VoiceUi_SetSelectedAssistant(uint8 voice_ui_provider, bool reboot)
{
    device_t device = voiceUi_GetMyDevice();

    DEBUG_LOG_DEBUG("VoiceUi_SetSelectedAssistant(enum:voice_ui_provider_t:%d)", voice_ui_provider);

    if (device)
    {
        Device_SetPropertyU8(device, device_property_voice_assistant, voice_ui_provider);
        DeviceDbSerialiser_Serialise();
        voiceui_MarshallSelectedAssistant(reboot);
        DEBUG_LOG_DEBUG("VoiceUi_SetSelectedAssistant: set property enum:voice_ui_provider_t:%d", voice_ui_provider);
    }
#ifdef INCLUDE_GAIA
    VoiceUiGaiaPlugin_NotifyAssistantChanged(voice_ui_provider);
#endif /* INCLUDE_GAIA */
}

static void voiceUi_DeselectCurrentAssistant(void)
{
    if (voiceUi_GetActiveVa())
    {
        DEBUG_LOG_DEBUG("voiceUi_DeselectCurrentAssistant");
        voiceUi_GetActiveVa()->voice_assistant->DeselectVoiceAssistant();
        voiceUi_SetActiveVa(NULL);
    }
}

voice_ui_handle_t * VoiceUi_Register(voice_ui_if_t *va_table)
{
    int va_index;
    voice_ui_handle_t* va_handle = NULL;

    /* Find a free slot in the container */
    for(va_index=0;va_index<MAX_NO_VA_SUPPORTED;va_index++)
    {
        if(!voice_assistant_list[va_index])
            break;
    }
    PanicFalse(va_index < MAX_NO_VA_SUPPORTED);

    va_handle = (voice_ui_handle_t*)PanicUnlessMalloc(sizeof(voice_ui_handle_t));
    va_handle->voice_assistant = va_table;
    voice_assistant_list[va_index] = va_handle;

    voice_ui_provider_t registering_va_provider = va_handle->voice_assistant->va_provider;
    DEBUG_LOG_DEBUG("VoiceUi_Register: enum:voice_ui_provider_t:%d", registering_va_provider);
    if (VoiceUi_GetSelectedAssistant() == registering_va_provider)
    {
        voiceUi_SetActiveVa(va_handle);
    }

    return va_handle;
}

voice_ui_handle_t* VoiceUi_GetActiveVa(void)
{
    return voiceUi_GetActiveVa();
}

feature_manager_handle_t VoiceUi_GetFeatureManagerHandle(void)
{
    return feature_manager_handle;
}

void VoiceUi_SetFeatureManagerHandle(feature_manager_handle_t handle)
{
    feature_manager_handle = handle;
}

voice_ui_provider_t VoiceUi_GetSelectedAssistant(void)
{
    uint8 selected_va = VOICE_UI_PROVIDER_DEFAULT;
    device_t device = voiceUi_GetMyDevice();

    if (device)
    {
        Device_GetPropertyU8(device, device_property_voice_assistant, &selected_va);
        DEBUG_LOG_DEBUG("VoiceUi_GetSelectedAssistant: got property enum:voice_ui_provider_t:%d", selected_va);
    }

    DEBUG_LOG_DEBUG("VoiceUi_GetSelectedAssistant: selected enum:voice_ui_provider_t:%d", selected_va);
    return selected_va;
}

uint16 VoiceUi_GetSupportedAssistants(uint8 *assistants)
{
    uint16 count = 0;
    uint16 va_index;

    PanicNull(assistants);

    /* Explicit support for 'none' */
    assistants[count++] = voice_ui_provider_none;

    for (va_index = 0; va_index < MAX_NO_VA_SUPPORTED; ++va_index)
    {
        if (voice_assistant_list[va_index] && voice_assistant_list[va_index]->voice_assistant)
        {
            DEBUG_LOG_DEBUG("VoiceUi_GetSupportedAssistants: voice assistant enum:voice_ui_provider_t:%d",
                voice_assistant_list[va_index]->voice_assistant->va_provider);
            assistants[count++] = voice_assistant_list[va_index]->voice_assistant->va_provider;
        }
    }

    DEBUG_LOG_DEBUG("VoiceUi_GetSupportedAssistants: count %d", count);
    return count;
}

bool VoiceUi_SelectVoiceAssistant(voice_ui_provider_t va_provider, voice_ui_reboot_permission_t reboot_permission)
{
    bool status = FALSE;
    bool reboot = FALSE;

    DEBUG_LOG_DEBUG("VoiceUi_SelectVoiceAssistant(va_provider enum:voice_ui_provider_t:%d, reboot_permission enum:voice_ui_reboot_permission_t:%d)",
                    va_provider, reboot_permission);

    if (voiceUi_GetActiveVa() && (va_provider == VoiceUi_GetSelectedAssistant()))
    {
    /*  Nothing to do  */
        status = TRUE;
    }
    else
    {
        if (va_provider == voice_ui_provider_none)
        {
            voiceUi_DeselectCurrentAssistant();
            status = TRUE;
        }
        else if (voiceUi_ProviderIsValid(va_provider))
        {
            voiceUi_DeselectCurrentAssistant();
            VoiceUi_SetSelectedVoiceAssistantInterface(va_provider);
            status = TRUE;
            if (reboot_permission == voice_ui_reboot_allowed)
            {
                reboot = voiceUi_GetActiveVa()->voice_assistant->reboot_required_on_provider_switch;
            }
        }
        else
        {
            DEBUG_LOG_ERROR("VoiceUi_SelectVoiceAssistant:va_provider enum:voice_ui_provider_t:%d not valid", va_provider);
        }
    }

    DEBUG_LOG_DEBUG("VoiceUi_SelectVoiceAssistant:va_provider enum:voice_ui_provider_t:%d, status %d, reboot enum:voice_ui_reboot_permission_t:%d",
                    va_provider, status, reboot);
    if (status)
    {
        VoiceUi_SetSelectedAssistant(va_provider, reboot);
        if (!VoiceUi_IsTwsFeatureIncluded() && reboot)
        {
            VoiceUi_RebootLater();
        }
    }
    return status;
}

void VoiceUi_EventHandler(voice_ui_handle_t* va_handle, ui_input_t event_id)
{
   if (va_handle)
   {
        if(va_handle->voice_assistant->EventHandler)
        {
            va_handle->voice_assistant->EventHandler(event_id);
        }
   }
}

inline static device_va_flag_t VoiceUi_GetDeviceFlags(void)
{
    device_t device = voiceUi_GetMyDevice();
    uint8 flags = 0;

    if (device)
    {
        Device_GetPropertyU8(device, device_property_va_flags, &flags);
        DEBUG_LOG_DEBUG("VoiceUi_GetDeviceFlags: flags=0x%02X", flags);
    }
    else
    {
        DEBUG_LOG_ERROR("VoiceUi_GetDeviceFlags: no device");
    }

    return (device_va_flag_t) flags;
}

bool VoiceUi_GetDeviceFlag(device_va_flag_t flag)
{
    device_va_flag_t flags = VoiceUi_GetDeviceFlags();
    bool value = (flags & flag) == flag;

    DEBUG_LOG_DEBUG("VoiceUi_GetDeviceFlag: flag=enum:device_va_flag_t:%02X value=%u", flag, value);
    return value;
}
void VoiceUi_GetPackedLocale(uint8 *packed_locale)
{
    void *value = NULL;
    device_t device = voiceUi_GetMyDevice();
    size_t size = 0;

    packed_locale[0] = '\0';

    if (device)
    {
        if (Device_GetProperty(device, device_property_va_locale, &value, &size) && size == DEVICE_SIZEOF_VA_LOCALE)
        {
            memcpy(packed_locale, value, DEVICE_SIZEOF_VA_LOCALE);
        }
    }
    else
    {
        DEBUG_LOG_ERROR("VoiceUi_GetPackedLocale: no device");
    }
}


void VoiceUi_SetPackedLocale(uint8 *packed_locale)
{
    device_t device = voiceUi_GetMyDevice();

    if (device)
    {
        Device_SetProperty(device, device_property_va_locale, packed_locale, DEVICE_SIZEOF_VA_LOCALE);
        DeviceDbSerialiser_Serialise();
    }
    else
    {
        DEBUG_LOG_ERROR("VoiceUi_SetPackedLocale: no device");
    }
}


void VoiceUi_SetDeviceFlag(device_va_flag_t flag, bool value)
{
    device_t device = voiceUi_GetMyDevice();
    DEBUG_LOG_DEBUG("VoiceUi_SetDeviceFlag: flag=enum:device_va_flag_t:%02X value=%u", flag, value);

    if (device)
    {
        uint8 flags;

        Device_GetPropertyU8(device, device_property_va_flags, &flags);

        if (value)
        {
            flags |= flag;
        }
        else
        {
            flags &= ~flag;
        }

        Device_SetPropertyU8(device, device_property_va_flags, flags);
        DeviceDbSerialiser_Serialise();
    }
    else
    {
        DEBUG_LOG_WARN("VoiceUi_SetDeviceFlag: no device");
    }
}

static void voiceUi_SetWuwEnable(bool enable)
{
    VoiceUi_SetDeviceFlag(device_va_flag_wuw_enabled, enable);

    if (voiceUi_GetActiveVa() && voiceUi_GetActiveVa()->voice_assistant && voiceUi_GetActiveVa()->voice_assistant->SetWakeWordDetectionEnable)
    {
        voiceUi_GetActiveVa()->voice_assistant->SetWakeWordDetectionEnable(enable);
    }
}


void VoiceUi_EnableWakeWordDetection(void)
{
    voiceUi_SetWuwEnable(TRUE);
}

void VoiceUi_DisableWakeWordDetection(void)
{
    voiceUi_SetWuwEnable(FALSE);
}

bool VoiceUi_WakeWordDetectionEnabled(void)
{
    return VoiceUi_GetDeviceFlag(device_va_flag_wuw_enabled);
}

#ifdef HOSTED_TEST_ENVIRONMENT
void VoiceUi_UnRegister(voice_ui_handle_t *va_handle)
{
    if (va_handle)
    {
        int i;
        for(i=0; i < MAX_NO_VA_SUPPORTED; i++)
        {
            if(voice_assistant_list[i] && (va_handle == voice_assistant_list[i]))
            {
                free(voice_assistant_list[i]);
                voice_assistant_list[i] = NULL;
                break;
            }
        }
    }
}
#endif

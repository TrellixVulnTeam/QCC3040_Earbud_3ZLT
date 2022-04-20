/*!
\copyright  Copyright (c) 2019 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama.c
\brief  Implementation of the service interface for Amazon AVS
*/

#ifdef INCLUDE_AMA
#include "ama.h"
#include "ama_actions.h"
#include "ama_anc.h"
#include "ama_audio.h"
#include "ama_battery.h"
#include "ama_ble.h"
#include "ama_config.h"
#include "ama_connect_state.h"
#include "ama_data.h"
#include "ama_eq.h"
#include "ama_voice_ui_handle.h"
#include "ama_profile.h"
#include "ama_protocol.h"
#include "ama_rfcomm.h"
#include "ama_send_command.h"
#include "ama_speech.h"

#include "bt_device.h"
#include <feature.h>
#include "gatt_server_gap.h"
#include "local_name.h"
#include <ps.h>
#include <stdlib.h>
#include <voice_ui.h>

#ifdef INCLUDE_ACCESSORY
#include "ama_accessory.h"
#endif

extern ama_config_t *ama_config;

static voice_ui_handle_t *voice_ui_handle = NULL;

/* AMA_TODO bring back our callback array! */
static ama_tx_callback_t ama_SendIap2Data;

/* Forward declaration */
static void ama_MessageHandler(Task task, MessageId id, Message message);
static void ama_EventHandler(ui_input_t   event_id);

static const TaskData ama_task = {ama_MessageHandler};
static void ama_DeselectVoiceAssistant(void);
static void ama_SelectVoiceAssistant(void);
static void ama_SessionCancelled(bool capture_suspended);
static void ama_SetWakeWordDetectionEnable(bool enable);

/* Return the task associated with Voice Assistant */
#define Ama_GetTask() ((Task)&ama_task)

static voice_ui_if_t ama_interface =
{
    .va_provider = voice_ui_provider_ama,
    .reboot_required_on_provider_switch = FALSE,
    .EventHandler = ama_EventHandler,
    .DeselectVoiceAssistant = ama_DeselectVoiceAssistant,
    .SelectVoiceAssistant = ama_SelectVoiceAssistant,
    .GetBtAddress = Ama_GetBtAddress,
    .SetWakeWordDetectionEnable = ama_SetWakeWordDetectionEnable,
    .BatteryUpdate = AmaBattery_Update,
#ifdef ENABLE_ANC
    .AncEnableUpdate = AmaAnc_EnabledUpdate,
    .LeakthroughEnableUpdate = NULL,
    .LeakthroughGainUpdate = NULL,
#endif
    .SessionCancelled = ama_SessionCancelled,
    .EqUpdate = Ama_EqUpdate,
    .audio_if =
    {
        .CaptureDataReceived = AmaAudio_HandleVoiceData,
#ifdef INCLUDE_WUW
        .WakeUpWordDetected = AmaAudio_WakeWordDetected,
#else
        .WakeUpWordDetected = NULL,
#endif /* INCLUDE_WUW */
    },
};

static void ama_SetWakeWordDetectionEnable(bool enable)
{
    DEBUG_LOG_DEBUG("ama_SetWakeWordDetectionEnable: %u", enable);

    if (enable)
    {
        AmaAudio_StartWakeWordDetection();
    }
    else
    {
        AmaAudio_StopWakeWordDetection();
    }
}

static void ama_DeselectVoiceAssistant(void)
{
    DEBUG_LOG("ama_DeselectVoiceAssistant");
    AmaSendCommand_NotifyDeviceConfig(ASSISTANT_OVERRIDE_REQUIRED);
}

static void ama_SelectVoiceAssistant(void)
{
    DEBUG_LOG("ama_SelectVoiceAssistant");
    AmaSendCommand_NotifyDeviceConfig(ASSISTANT_OVERRIDEN);
    if(VoiceUi_IsWakeUpWordFeatureIncluded())
    {
        if (Ama_IsConnected() && VoiceUi_WakeWordDetectionEnabled())
        {
            AmaAudio_StartWakeWordDetection();
        }
    }
}

static void ama_SessionCancelled(bool capture_suspended)
{
    UNUSED(capture_suspended);
    AmaSpeech_Stop();
    AmaData_SetState(ama_state_idle);
}

/****************************************************************************/
static void ama_GetDeviceInfo(ama_device_config_t *device_info)
{
    device_info->serial_number = NULL;
    uint16 name_len = 0;
    device_info->name = (char*)LocalName_GetName(&name_len);
    device_info->device_type = AMA_CONFIG_DEVICE_TYPE;
}

/*****************************************************************************/
static void ama_HandleSendPacket(AMA_SEND_PKT_IND_T* ind)
{
    Ama_SendData(ind->packet, ind->pkt_size);
}

/************************************************************************/
static void ama_MessageHandler(Task task, MessageId id, Message message)
{

    UNUSED(task);

    switch(id)
    {
        case AMA_SWITCH_TRANSPORT_IND:
            Ama_TransportSwitched(((AMA_SWITCH_TRANSPORT_IND_T*)message)->transport);
            break;
            
        case AMA_SEND_TRANSPORT_VERSION_ID:
            {
                ama_HandleSendPacket((AMA_SEND_PKT_IND_T*)message);
                /* Now we are ready for accepting any AVS commands */
                AmaData_SetState(ama_state_idle);
            }
            break;

         case AMA_SPEECH_PROVIDE_IND:
            {
                if(AmaAudio_Provide(message))
                {
                    AmaData_SetState(ama_state_sending);
                }
            }
            break;

         case AMA_SPEECH_STOP_IND:
            {
                AmaAudio_Stop();
                AmaData_SetState(ama_state_idle);
            }
            break;

        case AMA_SEND_PKT_IND:
            {
                ama_HandleSendPacket((AMA_SEND_PKT_IND_T*)message);
            }
            break;

        case AMA_OVERRIDE_ASSISTANT_IND:
            {
                VoiceUi_SelectVoiceAssistant(voice_ui_provider_ama, voice_ui_reboot_allowed);
                if(VoiceUi_IsWakeUpWordFeatureIncluded() && VoiceUi_WakeWordDetectionEnabled())
                {
                    AmaAudio_StartWakeWordDetection();
                }
            }
            break;
            
        case AMA_SYNCHRONIZE_SETTING_IND:
            {
                if(VoiceUi_IsWakeUpWordFeatureIncluded() && VoiceUi_WakeWordDetectionEnabled())
                {
                    AmaAudio_StartWakeWordDetection();
                }

                if(!Ama_IsRegistered() && Ama_IsConnected())
                {
                    VoiceUi_SetDeviceFlag(device_va_flag_ama_setup_done, TRUE);
                }
            }
            break;

        case AMA_UPGRADE_TRANSPORT_IND:
        case AMA_ENABLE_CLASSIC_PAIRING_IND:
        case AMA_START_ADVERTISING_AMA_IND:
        case AMA_STOP_ADVERTISING_AMA_IND:
        case AMA_SEND_AT_COMMAND_IND:
            /* AMA_TODO: Yet to handle */
            break;

        case AMA_SPEECH_STATE_IND:
            if (((AMA_SPEECH_STATE_IND_T*)message)->speech_state == ama_speech_state_idle)
            {
                VoiceUi_VaSessionEnded(Ama_GetVoiceUiHandle());
            }
            else
            {
                VoiceUi_VaSessionStarted(Ama_GetVoiceUiHandle());
            }
            break;

        default:
            DEBUG_LOG("ama_MessageHandler: unhandled MESSAGE:0x%04X", id);
            break;
    }

}

/************************************************************************/
static void ama_EventHandler(ui_input_t event_id)
{
    DEBUG_LOG("ama_EventHandler: event_id enum:ui_input_t:%d", event_id);

    if (!AmaActions_HandleVaEvent(event_id))
    {
        DEBUG_LOG("ama_EventHandler: unhandled");
    }

}

/************************************************************************/
void Ama_ConfigureCodec(ama_codec_t codec)
{
    ama_audio_data_t audio_config;

    audio_config.codec = codec;

    if(audio_config.codec == ama_codec_opus)
    {
        audio_config.u.opus_req_kbps = AMA_DEFAULT_OPUS_CODEC_BIT_RATE;
    }
    else if(audio_config.codec == ama_codec_msbc)
    {
        audio_config.u.msbc_bitpool_size = MSBC_ENCODER_BITPOOL_VALUE;
    }

    AmaData_SetAudioData(&audio_config);
}

static void ama_LicenseCheck(void)
{
    if (VoiceUi_IsTwsFeatureIncluded())
    {   /* Earbuds */
        if(VoiceUi_IsWakeUpWordFeatureIncluded())
        {
            if (FeatureVerifyLicense(AVA_MONO))
            {
                DEBUG_LOG_VERBOSE("ama_LicenseCheck: APVA MONO is licensed");
            }
            else
            {
                DEBUG_LOG_WARN("ama_LicenseCheck: APVA MONO not licensed");
                if (LOG_LEVEL_CURRENT_SYMBOL >= DEBUG_LOG_LEVEL_VERBOSE)
                    Panic();
            }
        }
    }
    else
    {   /* Headset */
        if(VoiceUi_IsWakeUpWordFeatureIncluded())
        {
            if (FeatureVerifyLicense(AVA))
            {
                DEBUG_LOG_VERBOSE("ama_LicenseCheck: APVA is licensed");
            }
            else
            {
                DEBUG_LOG_WARN("ama_LicenseCheck: APVA not licensed");
                if (LOG_LEVEL_CURRENT_SYMBOL >= DEBUG_LOG_LEVEL_VERBOSE)
                    Panic();
            }
        }
    }
}

/************************************************************************/
bool Ama_Init(Task init_task)
{
    ama_config_t ama_info;
    bool status = TRUE;
    
    UNUSED(init_task);

    DEBUG_LOG("Ama_Init");

    memset(&ama_info.device_config.local_addr, 0, sizeof(bdaddr));
    ama_GetDeviceInfo(&ama_info.device_config);
    ama_info.num_transports_supported = 2;

    voice_ui_handle = VoiceUi_Register(&ama_interface);
    AmaProtocol_Init(Ama_GetTask(), &ama_info);
    AmaActions_Init();

    /* LE advertising is used even when transport is not LE */
    AmaBle_RegisterAdvertising();
    GattServerGap_UseCompleteLocalName(TRUE);

    Ama_ConfigureCodec(ama_codec_msbc);
    AmaData_SetActiveTransport(ama_transport_rfcomm);
    
    AmaRfcomm_Init();
    AmaData_SetState(ama_state_initialized);
    AmaSpeech_SetToDefault();
    AmaProfile_Init();
    AmaAudio_Init();
    AmaBattery_Init();
    AmaAnc_Init();
    Ama_EqInit();

#ifdef INCLUDE_ACCESSORY
    status = AmaAccessory_Init();
#endif

    ama_LicenseCheck();

#ifndef HAVE_RDP_UI
    AmaAudio_RegisterLocalePrompts();
#endif

    return status;
}

/************************************************************************/
/* AMA_TODO: bring back our callback array! */
void Ama_SetTxCallback(ama_tx_callback_t callback, ama_transport_t transport)
{
    if (transport == ama_transport_iap)
    {
        ama_SendIap2Data = callback;
        ama_config->num_transports_supported = 3;
    }
}

/************************************************************************/
bool Ama_SendData(uint8 *data, uint16 size_data)
{
    bool status = FALSE;
    
    switch (AmaData_GetActiveTransport())
    {
    case ama_transport_rfcomm:
        status = AmaRfcomm_SendData(data, size_data);
        break;
    
    case ama_transport_iap:
        status = ama_SendIap2Data(data, size_data);
        break;

    default:
        break;
    }
    
    return status;
}

/************************************************************************/
bool Ama_ParseData(uint8 *data, uint16 size_data)
{
    return AmaParse_ParseData(data, size_data);
}

/************************************************************************/
voice_ui_handle_t *Ama_GetVoiceUiHandle(void)
{
    return voice_ui_handle;
}

#endif /* INCLUDE_AMA */


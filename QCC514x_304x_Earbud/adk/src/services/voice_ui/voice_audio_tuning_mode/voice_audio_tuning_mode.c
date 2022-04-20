/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Voice Audio Tuning VA client implementation
*/

#ifdef ENABLE_AUDIO_TUNING_MODE

#include "voice_audio_tuning_mode.h"
#include "voice_ui_va_client_if.h"
#include "ui.h"

static void voiceAudioTuningMode_EventHandler(ui_input_t event_id);
static void voiceAudioTuningMode_DeselectVoiceAssistant(void){}
static void voiceAudioTuningMode_SelectVoiceAssistant(void){}
static unsigned voiceAudioTuningMode_AudioInputDataReceived(Source source);
static void voiceAudioTuningMode_SessionCancelled(bool capture_suspended);

static voice_ui_if_t va_interface =
{
    .va_provider = voice_ui_provider_audio_tuning,
    .reboot_required_on_provider_switch = FALSE,
    .EventHandler = voiceAudioTuningMode_EventHandler,
    .DeselectVoiceAssistant = voiceAudioTuningMode_DeselectVoiceAssistant,
    .SelectVoiceAssistant = voiceAudioTuningMode_SelectVoiceAssistant,
    .BatteryUpdate = NULL,
    .SessionCancelled = voiceAudioTuningMode_SessionCancelled,
    .audio_if =
    {
        .CaptureDataReceived = voiceAudioTuningMode_AudioInputDataReceived,
        .WakeUpWordDetected = NULL,
    }
};

static const va_audio_voice_capture_params_t capture_config =
{
    .encode_config =
    {
        .encoder = va_audio_codec_sbc,
        .encoder_params.sbc =
        {
            .bitpool_size = 24,
            .block_length = 16,
            .number_of_subbands = 8,
            .allocation_method = sbc_encoder_allocation_method_loudness,
        },
    },
    .mic_config =
    {
        .sample_rate = 16000,
        .max_number_of_mics = 2,
        .min_number_of_mics = 1,
    },
};

static voice_ui_handle_t *va_handle = NULL;
static bool capture_active = FALSE;

static unsigned voiceAudioTuningMode_AudioInputDataReceived(Source source)
{
    SourceDrop(source, SourceSize(source));
    return 0;
}

static void voiceAudioTuningMode_SessionCancelled(bool capture_suspended)
{
    UNUSED(capture_suspended);
    capture_active = FALSE;
}

static void voiceAudioTuningMode_Toggle(void)
{
    if(capture_active)
    {
        VoiceUi_StopAudioCapture(va_handle);
        capture_active = FALSE;
    }
    else
    {
        if (VoiceUi_StartAudioCapture(va_handle, &capture_config) == voice_ui_audio_success)
            capture_active = TRUE;
    }
}

static void voiceAudioTuningMode_EventHandler(ui_input_t event_id)
{
    switch(event_id)
    {
        case ui_input_va_3:
            voiceAudioTuningMode_Toggle();
            break;
        default:
            break;
    }
}

bool VoiceAudioTuningMode_Init(Task init_task)
{
    UNUSED(init_task);
    va_handle = VoiceUi_Register(&va_interface);
    return TRUE;
}

#endif /*ENABLE_AUDIO_TUNING_MODE */

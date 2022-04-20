/*!
\copyright  Copyright (c) 2019 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_audio.c
\brief  Implementation of audio functionality for Amazon Voice Service
*/

#ifdef INCLUDE_AMA
#include "ama.h"
#include "ama_config.h"
#include "ama_connect_state.h"
#include "ama_audio.h"
#include "ama_speech.h"
#include "ama_rfcomm.h"
#include "ama_voice_ui_handle.h"
#include <voice_ui_container.h>
#include <voice_ui_va_client_if.h>
#include <voice_ui_peer_sig.h>
#include "ama_ble.h"
#include "ama_speech.h"
#include <kymera.h>
#include <source.h>
#include <stdlib.h>
#include <system_clock.h>
#include <voice_ui.h>
#include <voice_ui_audio.h>
#include <ui_indicator_prompts.h>

#define PRE_ROLL_US 500000UL
#define AMA_AUDIO_SS_DELAY_US (0UL)
#define AmaAudio_GetOpusFrameSize(config)  ((config->u.opus_req_kbps == AMA_OPUS_16KBPS) ? 40 : 80);
#define AmaAudio_SetAudioFormat(config) ((config->u.opus_req_kbps == AMA_OPUS_16KBPS) ?\
                                        AmaSpeech_SetAudioFormat(ama_audio_format_opus_16khz_16kbps_cbr_0_20ms) :\
                                        AmaSpeech_SetAudioFormat(ama_audio_format_opus_16khz_32kbps_cbr_0_20ms))
#define AMA_LOCALE_FILENAME_STR_SIZE    32

typedef enum
{
    ama_audio_prompt_unregistered,
    ama_audio_prompt_not_connected
} ama_audio_prompt_t;

typedef struct _ama_current_locale
{
    char name[AMA_LOCALE_STR_SIZE];
    FILE_INDEX file_index;
}ama_current_locale_t;

typedef struct{
    char* locale;   /* Name of locale */
    char* model;    /* Name of model that supports locale */
}locale_to_model_t;

/* Function pointer to send the encoded voice data */
static bool (*amaAudio_SendVoiceData)(Source src);

static ama_current_locale_t current_locale = {.file_index = FILE_NONE, .name = AMA_DEFAULT_LOCALE};

static const char *locale_ids[] = {AMA_AVAILABLE_LOCALES};
static const locale_to_model_t locale_to_model[] =
{
    AMA_LOCALE_TO_MODEL_OVERRIDES
};

static char locale_filename[AMA_LOCALE_FILENAME_STR_SIZE];
static const ui_event_indicator_table_t locale_prompt_data[] =
{
    {.sys_event=VOICE_UI_AMA_UNREGISTERED,
        { .prompt.filename = locale_filename,
          .prompt.rate = 48000,
          .prompt.format = PROMPT_FORMAT_SBC,
          .prompt.interruptible = TRUE,
          .prompt.queueable = FALSE,
          .prompt.requires_repeat_delay = FALSE }
    },
    {.sys_event=VOICE_UI_AMA_NOT_CONNECTED,
        { .prompt.filename = locale_filename,
          .prompt.rate = 48000,
          .prompt.format = PROMPT_FORMAT_SBC,
          .prompt.interruptible = FALSE,
          .prompt.queueable = TRUE,
          .prompt.requires_repeat_delay = TRUE }
    }
};

static const char *amaAudio_GetPromptFileSuffix(ama_audio_prompt_t prompt)
{
    const char *result = NULL;
    switch (prompt)
    {
    case ama_audio_prompt_unregistered:
        result = "_ama_unregistered.sbc";
        break;

    case ama_audio_prompt_not_connected:
        result = "_ama_not_connected.sbc";
        break;
    }
    return result;
}

static void amaAudio_CreatePromptFilename(const char *locale, const char *suffix,
                                          char *filename, size_t size)
{
    size_t needed = strlen(locale) + strlen(suffix) + 1;
    PanicFalse(size >= needed);
    strcpy(filename, locale);
    strcat(filename, suffix);
}

static FILE_INDEX amaAudio_CheckFileExists(const char *locale, const char *suffix,
                                           char *filebuf, size_t bufsize)
{
    char filename[AMA_LOCALE_FILENAME_STR_SIZE];
    amaAudio_CreatePromptFilename(locale, suffix, filename, sizeof(filename));
    FILE_INDEX file_index = FileFind(FILE_ROOT, filename, strlen(filename));
    if ( file_index != FILE_NONE && filebuf != NULL)
    {
        PanicFalse(bufsize > strlen(filename));
        strcpy(filebuf,filename);
    }
    return file_index;
}

static FILE_INDEX amaAudio_GetLocalePromptFilenameAndIndex(const char *locale, ama_audio_prompt_t prompt,
                                                           char *filebuf, size_t bufsize)
{
    FILE_INDEX file_index = FILE_NONE;
    const char *prompt_name = amaAudio_GetPromptFileSuffix(prompt);

    if( prompt_name == NULL)
        return FILE_NONE;

    file_index = amaAudio_CheckFileExists(locale, prompt_name, filebuf, bufsize);

    if (file_index == FILE_NONE)
    {
        const char *model = AmaAudio_GetModelFromLocale(locale);
        if (strcmp(model, locale) != 0)
        {
            /* The model for the locale is different to the locale */
            file_index = amaAudio_CheckFileExists(model, prompt_name, filebuf, bufsize);
        }
    }
    return file_index;
}

static FILE_INDEX amaAudio_FindLocalePromptFileIndex(const char *locale, ama_audio_prompt_t prompt)
{
    return amaAudio_GetLocalePromptFilenameAndIndex(locale, prompt, NULL, 0);
}

static FILE_INDEX amaAudio_ResolveLocaleFilename(ama_audio_prompt_t prompt, char *filebuf, size_t bufsize)
{
    FILE_INDEX file_index = FILE_NONE;
    char locale[AMA_LOCALE_STR_SIZE];
    if (AmaAudio_GetDeviceLocale(locale, AMA_LOCALE_STR_SIZE))
    {
        file_index = amaAudio_GetLocalePromptFilenameAndIndex(locale, prompt, filebuf, bufsize);
    }

    if (file_index == FILE_NONE)
    {
        DEBUG_LOG_WARN("amaAudio_ResolveLocaleFilename: localised file not found, trying default locale");
        file_index = amaAudio_GetLocalePromptFilenameAndIndex(AMA_DEFAULT_LOCALE, prompt, filebuf, bufsize);
    }
    return file_index;
}

static const ui_event_indicator_table_t *amaAudio_GetPromptData(MessageId id)
{
    FILE_INDEX file_index;
    const ui_event_indicator_table_t *result = NULL;
    switch (id)
    {
        case VOICE_UI_AMA_UNREGISTERED:
            file_index = amaAudio_ResolveLocaleFilename(ama_audio_prompt_unregistered,
                                                        locale_filename, sizeof(locale_filename));
            PanicFalse(file_index != FILE_NONE);
            PanicFalse(locale_prompt_data[0].sys_event == id);
            result = &locale_prompt_data[0];
        break;
        case VOICE_UI_AMA_NOT_CONNECTED:
            file_index = amaAudio_ResolveLocaleFilename(ama_audio_prompt_not_connected,
                                                        locale_filename, sizeof(locale_filename));
            PanicFalse(file_index != FILE_NONE);
            PanicFalse(locale_prompt_data[1].sys_event == id);
            result = &locale_prompt_data[1];
        break;
    }
    PanicFalse(result != NULL);
    return result;
}

void AmaAudio_RegisterLocalePrompts(void)
{
    DEBUG_LOG("AmaAudio_RegisterLocalePrompts for each handled event");
    for (size_t i = 0 ; i < ARRAY_DIM(locale_prompt_data); i++)
    {
        UiPrompts_SetUserPromptDataFunction(amaAudio_GetPromptData, locale_prompt_data[i].sys_event);
    }
}

void AmaAudio_DeregisterLocalePrompts(void)
{
    DEBUG_LOG("AmaAudio_DeregisterLocalePrompts for each handled event");
    for (size_t i = 0 ; i < ARRAY_DIM(locale_prompt_data); i++)
    {
        UiPrompts_ClearUserPromptDataFunction(locale_prompt_data[i].sys_event);
    }
}

bool AmaAudio_ValidateLocale(const char *locale)
{
    FILE_INDEX file_index = amaAudio_FindLocalePromptFileIndex(locale, ama_audio_prompt_unregistered);

    if (file_index == FILE_NONE)
    {
        /* There is no "unregistered" prompt */
#ifndef AMA_LOCALES_NEED_UNREGISTERED_PROMPT
        /* The default locale must have an "unregistered" prompt */
        if (strcmp(locale, AMA_DEFAULT_LOCALE) == 0)
#else
        /* All locales must have an "unregistered" prompt */
#endif
        {
            return FALSE;
        }
    }

    file_index = amaAudio_FindLocalePromptFileIndex(locale, ama_audio_prompt_not_connected);

    if (file_index != FILE_NONE)
    {
        /* The locale has a "not connected" prompt */
        if(VoiceUi_IsWakeUpWordFeatureIncluded())
        {
            /* For wake-up-word, the local must have a locale file */
            file_index = FileFind(FILE_ROOT, locale, strlen(locale));
        }
    }

    return (file_index != FILE_NONE);
}

static void ama_GetLocalesInFileSystem(ama_supported_locales_t* supported_locales)
{
    for(uint8 i = 0; i < ARRAY_DIM(locale_ids); i++)
    {
        const char *model = AmaAudio_GetModelFromLocale(locale_ids[i]);
        if (AmaAudio_ValidateLocale(model))
        {
            supported_locales->name[supported_locales->num_locales] = locale_ids[i];
            supported_locales->num_locales++;
        }
    }
}

#define ValidateLocaleSize()   PanicFalse(MAX_AMA_LOCALES >= ARRAY_DIM(locale_ids))

static void amaAudio_SetDeviceLocale(const char *locale);

inline static void amaAudio_SetCurrentLocaleFileIndex(void)
{
    const char* model = AmaAudio_GetModelFromLocale(current_locale.name);
    current_locale.file_index = FileFind(FILE_ROOT, model, strlen(model));
    PanicFalse(current_locale.file_index != FILE_NONE);
}

static bool amaAudio_SendMsbcVoiceData(Source source)
{
    #define AMA_HEADER_LEN 4
    #define MSBC_ENC_PKT_LEN 60
    #define MSBC_FRAME_LEN 57
    #define MSBC_FRAME_COUNT 5

    uint8 frames_to_send;
    uint16 payload_posn;
    uint16 lengthSourceThreshold;
    uint8 *buffer = NULL;
    uint8 no_of_transport_pkt = 0;
    uint8 initial_position = 0;

    bool sent_if_necessary = FALSE;

    frames_to_send = MSBC_FRAME_COUNT;
    initial_position = AMA_HEADER_LEN;

    lengthSourceThreshold = MSBC_ENC_PKT_LEN * frames_to_send;

    while ((SourceSize(source) >= (lengthSourceThreshold + 2)) && no_of_transport_pkt < 3)
    {
        const uint8 *source_ptr = SourceMap(source);
        uint32 copied = 0;
        uint32 frame;
        uint16 length;

        if(!buffer)
            buffer = PanicUnlessMalloc((MSBC_FRAME_LEN * frames_to_send) + AMA_HEADER_LEN);

        payload_posn = initial_position;

        for (frame = 0; frame < frames_to_send; frame++)
        {
            memmove(&buffer[payload_posn], &source_ptr[(frame * MSBC_ENC_PKT_LEN) + 2], MSBC_FRAME_LEN);
            payload_posn += MSBC_FRAME_LEN;
            copied += MSBC_FRAME_LEN;
        }

        length = AmaProtocol_PrepareVoicePacket(buffer, copied);

        sent_if_necessary = Ama_SendData(buffer, length);

        if(sent_if_necessary)
        {
            SourceDrop(source, lengthSourceThreshold);
        }
        else
        {
            break;
        }

        no_of_transport_pkt++;
    }

    free(buffer);

    DEBUG_LOG_V_VERBOSE("amaAudio_SendMsbcVoiceData: %d bytes remaining", SourceSize(source));

    return sent_if_necessary;
}

static bool amaAudio_SendOpusVoiceData(Source source)
{
    /* Parameters used by Opus codec*/
    #define AMA_OPUS_HEADER_LEN         3
    #define OPUS_16KBPS_ENC_PKT_LEN     40
    #define OPUS_32KBPS_ENC_PKT_LEN     80
    #define OPUS_16KBPS_LE_FRAME_COUNT      4
    #define OPUS_16KBPS_RFCOMM_FRAME_COUNT  5
    #define OPUS_32KBPS_RFCOMM_FRAME_COUNT  3
    #define OPUS_32KBPS_LE_FRAME_COUNT      2

    uint16 payload_posn;
    uint16 lengthSourceThreshold;
    uint8 *buffer = NULL;
    bool sent_if_necessary = FALSE;
    uint8 no_of_transport_pkt = 0;
    ama_transport_t transport;
    uint16 opus_enc_pkt_len = OPUS_16KBPS_ENC_PKT_LEN; /* Make complier happy. */
    uint16 opus_frame_count = OPUS_16KBPS_RFCOMM_FRAME_COUNT;

    transport = AmaData_GetActiveTransport();

    switch(AmaSpeech_GetAudioFormat())
    {
        case AUDIO_FORMAT__OPUS_16KHZ_16KBPS_CBR_0_20MS :

            if(transport == ama_transport_rfcomm)
            {
                opus_enc_pkt_len = OPUS_16KBPS_ENC_PKT_LEN;
                opus_frame_count = OPUS_16KBPS_RFCOMM_FRAME_COUNT;
            }
            else
            {
                opus_enc_pkt_len = OPUS_16KBPS_ENC_PKT_LEN;
                opus_frame_count = OPUS_16KBPS_LE_FRAME_COUNT;
            }
            break;

        case AUDIO_FORMAT__OPUS_16KHZ_32KBPS_CBR_0_20MS :

            if(transport == ama_transport_rfcomm)
            {
                opus_enc_pkt_len = OPUS_32KBPS_ENC_PKT_LEN;
                opus_frame_count = OPUS_32KBPS_RFCOMM_FRAME_COUNT;
            }
            else
            {
                opus_enc_pkt_len = OPUS_32KBPS_ENC_PKT_LEN;
                opus_frame_count = OPUS_32KBPS_LE_FRAME_COUNT;
            }
            break;

        case AUDIO_FORMAT__PCM_L16_16KHZ_MONO :
        case AUDIO_FORMAT__MSBC:
        default:
            DEBUG_LOG_ERROR("amaAudio_SendOpusVoiceData: Unexpected audio format");
            Panic();
            break;
    }

    lengthSourceThreshold = (opus_frame_count * opus_enc_pkt_len);

    while (SourceSize(source) && (SourceSize(source) >= lengthSourceThreshold) && (no_of_transport_pkt < 3))
    {
        const uint8 *opus_ptr = SourceMap(source);
        uint16 frame;
        uint16 copied = 0;
        uint16 length;

        if(!buffer)
            buffer = PanicUnlessMalloc((lengthSourceThreshold) + 3);

        payload_posn = AMA_OPUS_HEADER_LEN;

        for (frame = 0; frame < opus_frame_count; frame++)
        {
            memmove(&buffer[payload_posn], &opus_ptr[(frame*opus_enc_pkt_len)], opus_enc_pkt_len);
            payload_posn += opus_enc_pkt_len;
            copied += opus_enc_pkt_len;
        }

        length = AmaProtocol_PrepareVoicePacket(buffer, copied);

        sent_if_necessary = Ama_SendData(buffer, length);

        if(sent_if_necessary)
        {
            SourceDrop(source, lengthSourceThreshold);
        }
        else
        {
            break;
        }

        no_of_transport_pkt++;
    }

    free(buffer);

    DEBUG_LOG_V_VERBOSE("amaAudio_SendOpusVoiceData: %d bytes remaining", SourceSize(source));

    return sent_if_necessary;
}

static va_audio_codec_t amaAudio_ConvertCodecType(ama_codec_t codec_type)
{
    switch(codec_type)
    {
        case ama_codec_sbc:
            return va_audio_codec_sbc;
        case ama_codec_msbc:
            return va_audio_codec_msbc;
        case ama_codec_opus:
            return va_audio_codec_opus;
        default:
            DEBUG_LOG_ERROR("amaAudio_ConvertCodecType: Unknown codec");
            Panic();
            return va_audio_codec_last;
    }
}

unsigned AmaAudio_HandleVoiceData(Source src)
{
    unsigned timeout_in_ms = 0;

    if(AmaData_IsSendingVoiceData())
    {
        if (amaAudio_SendVoiceData(src) == FALSE)
        {
            /* Making sure we attempt to retransmit even if the source is full */
            timeout_in_ms = 50;
        }
    }
    else
    {
        SourceDrop(src, SourceSize(src));
    }

    return timeout_in_ms;
}

static va_audio_encode_config_t amaAudio_GetEncodeConfiguration(void)
{
    va_audio_encode_config_t config = {0};

    ama_audio_data_t *ama_audio_cfg = AmaData_GetAudioData();
    config.encoder = amaAudio_ConvertCodecType(ama_audio_cfg->codec);

    switch(config.encoder)
    {
        case va_audio_codec_msbc:
            amaAudio_SendVoiceData = amaAudio_SendMsbcVoiceData;
            config.encoder_params.msbc.bitpool_size = ama_audio_cfg->u.msbc_bitpool_size;
            break;

        case va_audio_codec_opus:
            amaAudio_SendVoiceData = amaAudio_SendOpusVoiceData;
            config.encoder_params.opus.frame_size = AmaAudio_GetOpusFrameSize(ama_audio_cfg);
            break;

        default:
            DEBUG_LOG_ERROR("amaAudio_GetEncodeConfiguration: Unsupported codec");
            Panic();
            break;
    }

    return config;
}

static void amaAudio_SetAudioFormat(void)
{
    ama_audio_data_t *ama_audio_cfg = AmaData_GetAudioData();

    switch(ama_audio_cfg->codec)
    {
        case ama_codec_msbc:
            AmaSpeech_SetAudioFormat(ama_audio_format_msbc);
            break;
        case ama_codec_opus:
            AmaAudio_SetAudioFormat(ama_audio_cfg);
            break;
        default:
            DEBUG_LOG_ERROR("amaAudio_SetAudioFormat: Unsupported codec");
            Panic();
            break;
    }
}

static uint32 amaAudio_GetStartCaptureTimestamp(const va_audio_wuw_detection_info_t *wuw_info)
{
    return (wuw_info->start_timestamp - (uint32) PRE_ROLL_US);
}

bool AmaAudio_WakeWordDetected(va_audio_wuw_capture_params_t *capture_params, const va_audio_wuw_detection_info_t *wuw_info)
{
    bool start_capture = FALSE;

    capture_params->start_timestamp = amaAudio_GetStartCaptureTimestamp(wuw_info);

    DEBUG_LOG("amaAudio_WakeUpWordDetected");

    amaAudio_SetAudioFormat();

    if (AmaData_IsReadyToSendStartSpeech() && AmaSpeech_StartWakeWord(PRE_ROLL_US, wuw_info->start_timestamp, wuw_info->end_timestamp))
    {
        start_capture = TRUE;
        capture_params->encode_config = amaAudio_GetEncodeConfiguration();
        AmaData_SetState(ama_state_sending);
    }

    return start_capture;
}

static bool amaAudio_StartVoiceCapture(void)
{
    va_audio_voice_capture_params_t audio_cfg = {0};

    audio_cfg.mic_config.sample_rate = 16000;
    audio_cfg.mic_config.max_number_of_mics = AMA_MAX_NUMBER_OF_MICS;
    audio_cfg.mic_config.min_number_of_mics = AMA_MIN_NUMBER_OF_MICS;
    audio_cfg.encode_config = amaAudio_GetEncodeConfiguration();

    voice_ui_audio_status_t status = VoiceUi_StartAudioCapture(Ama_GetVoiceUiHandle(), &audio_cfg);
    if (voice_ui_audio_failed == status)
    {
        DEBUG_LOG_ERROR("amaAudio_StartVoiceCapture: Failed to start capture");
        Panic();
    }

    return (voice_ui_audio_success == status) ;
}

static void amaAudio_StopVoiceCapture(void)
{
    VoiceUi_StopAudioCapture(Ama_GetVoiceUiHandle());
}

static DataFileID ama_LoadWakeUpWordModel(wuw_model_id_t model)
{
    DEBUG_LOG("ama_LoadWakeUpWordModel %d", model);
    return OperatorDataLoadEx(model, DATAFILE_BIN, STORAGE_INTERNAL, FALSE);
}

static void amaAudio_StartWuwDetection(void)
{
    DEBUG_LOG_DEBUG("amaAudio_StartWuwDetection");

    if (current_locale.file_index == FILE_NONE)
    {
        amaAudio_SetCurrentLocaleFileIndex();
    }

    va_audio_wuw_detection_params_t detection =
    {
        .max_pre_roll_in_ms = 2000,
        .wuw_config =
        {
            .engine = va_wuw_engine_apva,
            .model = current_locale.file_index,
            .LoadWakeUpWordModel = ama_LoadWakeUpWordModel,
            .engine_init_preroll_ms = 500,
        },
        .mic_config =
        {
            .sample_rate = 16000,
            .max_number_of_mics = AMA_MAX_NUMBER_OF_MICS,
            .min_number_of_mics = AMA_MIN_NUMBER_OF_MICS
        }
    };

    if (detection.wuw_config.model == FILE_NONE)
    {
        DEBUG_LOG_ERROR("amaAudio_StartWuWDetection: Failed to find model");
        Panic();
    }
    voice_ui_audio_status_t status = VoiceUi_StartWakeUpWordDetection(Ama_GetVoiceUiHandle(), &detection);
    if (voice_ui_audio_failed == status)
    {
        DEBUG_LOG_ERROR("amaAudio_StartWuWDetection: Failed to start detection");
        Panic();
    }
}

static void amaAudio_StopWuwDetection(void)
{
    VoiceUi_StopWakeUpWordDetection(Ama_GetVoiceUiHandle());
}

static bool amaAudio_Trigger(ama_audio_trigger_t trigger_type)
{
    bool return_val = FALSE;

    amaAudio_SetAudioFormat();

    if (AmaData_IsReadyToSendStartSpeech() &&
       (VoiceUi_IsAudioSuspended(Ama_GetVoiceUiHandle()) == FALSE))
    {
        switch(trigger_type)
        {
            case ama_audio_trigger_tap:
                return_val = AmaSpeech_StartTapToTalk();
                break;
            case ama_audio_trigger_press:
                return_val = AmaSpeech_StartPushToTalk();
                break;
            default:
                DEBUG_LOG_ERROR("AmaAudio_Start: Unsupported trigger");
                Panic();
                break;
        }
    }

    if(return_val)
    {
        if(!amaAudio_StartVoiceCapture())
        {
            AmaSpeech_Stop();
            return_val = FALSE;
        }
    }

    return return_val;
}

static void amaAudio_SetLocaleName(const char* name)
{
    amaAudio_SetDeviceLocale(name);
    if (AmaAudio_GetDeviceLocale(current_locale.name, ARRAY_DIM(current_locale.name)) == FALSE)
    {
        strncpy(current_locale.name, name, ARRAY_DIM(current_locale.name));
        current_locale.name[ARRAY_DIM(current_locale.name)-1] = '\0';
    }
}

static void amaAudio_PlayPrompt(ama_audio_prompt_t prompt)
{
    DEBUG_LOG_DEBUG("amaAudio_PlayPrompt: prompt=enum:ama_audio_prompt_t:%d", prompt);

#ifndef HAVE_RDP_UI
    FILE_INDEX file_index = amaAudio_ResolveLocaleFilename(prompt, NULL, 0);

    if (file_index == FILE_NONE)
    {
        DEBUG_LOG_ERROR("amaAudio_PlayPrompt: file not found for default locale");
        Panic();
    }
#endif
    switch (prompt)
    {
    case ama_audio_prompt_unregistered:
        VoiceUi_Notify(VOICE_UI_AMA_UNREGISTERED);
        break;

    case ama_audio_prompt_not_connected:
        VoiceUi_Notify(VOICE_UI_AMA_NOT_CONNECTED);
        break;
    }
}

bool AmaAudio_Start(ama_audio_trigger_t type)
{
    bool started = FALSE;

    if (Ama_IsRegistered())
    {
        if (Ama_IsConnected())
        {
            started = amaAudio_Trigger(type);
        }
        else
        {
            amaAudio_PlayPrompt(ama_audio_prompt_not_connected);
        }
    }
    else
    {
        amaAudio_PlayPrompt(ama_audio_prompt_unregistered);
    }

    return started;
}

void AmaAudio_Stop(void)
{
    DEBUG_LOG("AmaAudio_Stop");
    amaAudio_StopVoiceCapture();
}

bool AmaAudio_Provide(const AMA_SPEECH_PROVIDE_IND_T* ind)
{
    bool return_val = FALSE;
    if (AmaData_IsReadyToSendStartSpeech())
    {
        return_val = amaAudio_StartVoiceCapture();
    }
    AmaProtocol_ProvideSpeechRsp(return_val, ind);
    return return_val;
}

void AmaAudio_End(void)
{
    DEBUG_LOG("AmaAudio_End");
    AmaSpeech_End();
    amaAudio_StopVoiceCapture();
}

void AmaAudio_StartWakeWordDetection(void)
{
    DEBUG_LOG("AmaAudio_StartWakeWordDetection");
    amaAudio_StartWuwDetection();
}

void AmaAudio_StopWakeWordDetection(void)
{
    DEBUG_LOG("AmaAudio_StopWakeWordDetection");
    amaAudio_StopWuwDetection();
}

const char *AmaAudio_GetModelFromLocale(const char* locale)
{
    const char *model = locale;
    for(uint8 i=0; i<sizeof(locale_to_model)/sizeof(locale_to_model[0]); i++)
    {
        if(strcmp(locale, locale_to_model[i].locale) == 0)
        {
            model = locale_to_model[i].model;
            break;
        }
    }
    return model;
}

void AmaAudio_GetSupportedLocales(ama_supported_locales_t* supported_locales)
{
    memset(supported_locales, 0, sizeof(ama_supported_locales_t));
    ama_GetLocalesInFileSystem(supported_locales);
}

const char* AmaAudio_GetCurrentLocale(void)
{
    return current_locale.name;
}

void AmaAudio_SetLocale(const char* locale)
{
    amaAudio_SetLocaleName(locale);

    if(VoiceUi_IsWakeUpWordFeatureIncluded())
    {
        amaAudio_SetCurrentLocaleFileIndex();

        if (VoiceUi_WakeWordDetectionEnabled())
        {
            AmaAudio_StartWakeWordDetection();
        }
    }
}

bool AmaAudio_GetDeviceLocale(char *locale, uint8 locale_size)
{
    uint8 packed_locale[DEVICE_SIZEOF_VA_LOCALE];
    /* The unpacked form has a hyphen and a \0 terminator */
    PanicFalse(locale_size >= AMA_LOCALE_STR_SIZE);
    VoiceUi_GetPackedLocale(packed_locale);

    if (packed_locale[0] != '\0')
    {
        locale[0] = packed_locale[0];
        locale[1] = packed_locale[1];
        locale[2] = '-';
        locale[3] = packed_locale[2];
        locale[4] = packed_locale[3];
        locale[5] = '\0';

        DEBUG_LOG_DEBUG("AmaAudio_GetDeviceLocale: locale=\"%c%c%c%c%c\"",
                        locale[0], locale[1], locale[2], locale[3], locale[4]);

        return TRUE;
    }

    DEBUG_LOG_WARN("AmaAudio_GetDeviceLocale: no locale");
    return FALSE;
}

static void amaAudio_SetDeviceLocale(const char *locale)
{
    /* The unpacked form has a hyphen and a \0 terminator */
    if (strlen(locale) == AMA_LOCALE_STR_LEN && locale[2] == '-')
    {
        uint8 packed_locale[DEVICE_SIZEOF_VA_LOCALE];

        packed_locale[0] = locale[0];
        packed_locale[1] = locale[1];
        packed_locale[2] = locale[3];
        packed_locale[3] = locale[4];

        VoiceUi_SetPackedLocale(packed_locale);

        DEBUG_LOG_DEBUG("amaAudio_SetDeviceLocale: locale=\"%c%c%c%c%c\"",
                        locale[0], locale[1], locale[2], locale[3], locale[4]);
    }
    else
    {
        DEBUG_LOG_ERROR("amaAudio_SetDeviceLocale: bad ISO language-country");
        Panic();
    }
}

void AmaAudio_Init(void)
{
    /* Check that the internal and external locale representation sizes are compatible */
    PanicFalse((DEVICE_SIZEOF_VA_LOCALE+2)==AMA_LOCALE_STR_SIZE);
    if(VoiceUi_IsWakeUpWordFeatureIncluded())
    {
        ValidateLocaleSize();
    }
}

#endif /* INCLUDE_AMA */

/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of voice assistant audio testing functions.
*/

#ifndef DISABLE_TEST_API

#include "va_audio_test.h"
#include "voice_audio_manager.h"
#include "ama_config.h"
#include <logging.h>
#include <operator.h>

#ifdef GC_SECTIONS
/* Move all functions in KEEP_PM section to ensure they are not removed during
 * garbage collection */
#pragma unitcodesection KEEP_PM
#endif

static const va_audio_encode_config_t va_encode_config_table[] =
{
    {.encoder = va_audio_codec_sbc, .encoder_params.sbc =
        {
            .bitpool_size = 24,
            .block_length = 16,
            .number_of_subbands = 8,
            .allocation_method = sbc_encoder_allocation_method_loudness,
        }
    },
    {.encoder = va_audio_codec_msbc, .encoder_params.msbc = {.bitpool_size = 24}},
    {.encoder = va_audio_codec_opus, .encoder_params.opus = {.frame_size = 40}},
};

static bool vaTestPopulateVaEncodeConfig(va_audio_codec_t encoder, va_audio_encode_config_t *config)
{
    bool status = FALSE;
    unsigned i;

    for(i = 0; i < ARRAY_DIM(va_encode_config_table); i++)
    {
        if (va_encode_config_table[i].encoder == encoder)
        {
            status = TRUE;
            if (config)
            {
                *config = va_encode_config_table[i];
            }
            break;
        }
    }

    return status;
}

static bool vaTestPopulateVaMicConfig(unsigned num_of_mics, va_audio_mic_config_t *config)
{
    config->sample_rate = 16000;
    config->min_number_of_mics = 1;
    /* Use it as max in order to attempt to use this many mics */
    config->max_number_of_mics = num_of_mics;

    return (config->max_number_of_mics >= 1);
}

static unsigned vaTestDropDataInSource(Source source)
{
    DEBUG_LOG_V_VERBOSE("vaTestDropDataInSource");
    SourceDrop(source, SourceSize(source));
    return 0;
}

static bool vaTestPopulateVoiceCaptureParams(va_audio_codec_t encoder, unsigned num_of_mics, va_audio_voice_capture_params_t *params)
{
    return vaTestPopulateVaMicConfig(num_of_mics, &params->mic_config) && vaTestPopulateVaEncodeConfig(encoder, &params->encode_config);
}

bool appTestStartVaCapture(va_audio_codec_t encoder, unsigned num_of_mics)
{
    bool status = FALSE;
    va_audio_voice_capture_params_t params = {0};

    if (vaTestPopulateVoiceCaptureParams(encoder, num_of_mics, &params))
    {
        status = VoiceAudioManager_StartCapture(vaTestDropDataInSource, &params);
    }

    return status;
}

bool appTestStopVaCapture(void)
{
    return VoiceAudioManager_StopCapture();
}

#ifdef INCLUDE_WUW

static const struct
{
    va_wuw_engine_t engine;
    unsigned        capture_ts_based_on_wuw_start_ts:1;
    uint16          max_pre_roll_in_ms;
    uint16          pre_roll_on_capture_in_ms;
    const char     *model;
} wuw_detection_start_table[] =
{
    {va_wuw_engine_qva, TRUE, 2000, 500, "tfd_0.bin"},
    {va_wuw_engine_gva, TRUE, 2000, 500, "gaa_model.bin"},
    {va_wuw_engine_apva, FALSE, 2000, 500, AMA_DEFAULT_LOCALE}
};

static struct
{
    unsigned         start_capture_on_detection:1;
    va_audio_codec_t encoder_for_capture_on_detection;
    va_wuw_engine_t  wuw_engine;
} va_config = {0};

static bool vaTestPopulateStartCaptureTimeStamp(const va_audio_wuw_detection_info_t *wuw_info, uint32 *timestamp)
{
    bool status = FALSE;
    unsigned i;

    for(i = 0; i < ARRAY_DIM(wuw_detection_start_table); i++)
    {
        if (wuw_detection_start_table[i].engine == va_config.wuw_engine)
        {
            uint32 pre_roll = wuw_detection_start_table[i].pre_roll_on_capture_in_ms * 1000;
            status = TRUE;
            if (timestamp)
            {
                if (wuw_detection_start_table[i].capture_ts_based_on_wuw_start_ts)
                {
                    *timestamp = wuw_info->start_timestamp - pre_roll;
                }
                else
                {
                    *timestamp = wuw_info->end_timestamp - pre_roll;
                }
            }
            break;
        }
    }

    return status;
}

static va_audio_wuw_detected_response_t vaTestWuwDetected(const va_audio_wuw_detection_info_t *wuw_info)
{
    va_audio_wuw_detected_response_t response = {0};

    if (va_config.start_capture_on_detection &&
        vaTestPopulateVaEncodeConfig(va_config.encoder_for_capture_on_detection, &response.capture_params.encode_config) &&
        vaTestPopulateStartCaptureTimeStamp(wuw_info, &response.capture_params.start_timestamp))
    {
        response.start_capture = TRUE;
        response.capture_callback = vaTestDropDataInSource;
    }

    return response;
}

static DataFileID vaTestLoadWuwModel(wuw_model_id_t model)
{
    return OperatorDataLoadEx(model, DATAFILE_BIN, STORAGE_INTERNAL, FALSE);
}

static bool vaTestPopulateWuwDetectionParams(va_wuw_engine_t engine, unsigned num_of_mics, va_audio_wuw_detection_params_t *params)
{
    bool status = FALSE;
    unsigned i;

    if (vaTestPopulateVaMicConfig(num_of_mics, &params->mic_config) == FALSE)
    {
        return FALSE;
    }

    for(i = 0; i < ARRAY_DIM(wuw_detection_start_table); i++)
    {
        if (wuw_detection_start_table[i].engine == engine)
        {
            FILE_INDEX model = FileFind(FILE_ROOT, wuw_detection_start_table[i].model, strlen(wuw_detection_start_table[i].model));
            if (model != FILE_NONE)
            {
                status = TRUE;
                if (params)
                {
                    params->wuw_config.engine = wuw_detection_start_table[i].engine;
                    params->wuw_config.model = model;
                    params->wuw_config.LoadWakeUpWordModel = vaTestLoadWuwModel;
                    params->max_pre_roll_in_ms = wuw_detection_start_table[i].max_pre_roll_in_ms;
                }
                break;
            }
        }
    }

    return status;
}

bool appTestStartVaWakeUpWordDetection(va_wuw_engine_t wuw_engine, unsigned num_of_mics, bool start_capture_on_detection, va_audio_codec_t encoder)
{
    va_audio_wuw_detection_params_t params = {0};

    if (vaTestPopulateWuwDetectionParams(wuw_engine, num_of_mics, &params) == FALSE)
    {
        return FALSE;
    }

    if (start_capture_on_detection && (vaTestPopulateVaEncodeConfig(encoder, NULL) == FALSE))
    {
        return FALSE;
    }

    va_config.start_capture_on_detection = start_capture_on_detection;
    va_config.encoder_for_capture_on_detection = encoder;
    va_config.wuw_engine = wuw_engine;

    return VoiceAudioManager_StartDetection(vaTestWuwDetected, &params);
}

bool appTestStopVaWakeUpWordDetection(void)
{
    return VoiceAudioManager_StopDetection();
}

#endif /* INCLUDE_WUW */

#endif /* DISABLE_TEST_API */

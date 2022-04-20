/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Data types used by APIs related to Voice Assistants audio

*/

#ifndef VA_AUDIO_TYPES_H_
#define VA_AUDIO_TYPES_H_

#include <file.h>
#include <source.h>
#include <audio_sbc_encoder_params.h>
#include <audio_opus_encoder_params.h>
#include <audio_msbc_encoder_params.h>

/*! \brief Defines the different codecs used for voice assistants audio */
typedef enum
{
    va_audio_codec_sbc,
    va_audio_codec_msbc,
    va_audio_codec_opus,
    va_audio_codec_last
} va_audio_codec_t;

/*! \brief parameters for SBC encoder */
typedef struct
{
    uint8 bitpool_size;
    uint8 block_length;
    uint8 number_of_subbands;
    sbc_encoder_allocation_method_t allocation_method;
} va_audio_encoder_sbc_params_t;

/*! \brief Union of the parameters of each encoder */
typedef union
{
    va_audio_encoder_sbc_params_t sbc;
    va_audio_encoder_msbc_params_t msbc;
    va_audio_encoder_opus_params_t opus;
} va_audio_encoder_params_t;

/*! \brief Encoder configuration */
typedef struct
{
    va_audio_codec_t          encoder;
    va_audio_encoder_params_t encoder_params;
} va_audio_encode_config_t;

/*! \brief MIC configuration */
typedef struct
{
    uint32 sample_rate;
    uint8 max_number_of_mics;
    uint8 min_number_of_mics;
} va_audio_mic_config_t;

/*! \brief Voice capture parameters */
typedef struct
{
    va_audio_encode_config_t encode_config;
    va_audio_mic_config_t    mic_config;
} va_audio_voice_capture_params_t;

/*! \brief Defines the different Wake-Up-Word engines used for voice assistants audio */
typedef enum
{
    va_wuw_engine_qva,
    va_wuw_engine_gva,
    va_wuw_engine_apva
} va_wuw_engine_t;

/*! \brief Generic type for the Wake-Up-Word models ID */
typedef FILE_INDEX wuw_model_id_t;

static inline bool VA_AUDIO_MIC_CONFIG_EQUAL(const va_audio_mic_config_t *a, const va_audio_mic_config_t *b)
{
    return ((a->sample_rate == b->sample_rate) &&
            (a->max_number_of_mics == b->max_number_of_mics) &&
            (a->min_number_of_mics == b->min_number_of_mics));
}

/*! \brief Wake-Up-Word engine configuration */
typedef struct
{
    va_wuw_engine_t engine;
    wuw_model_id_t  model;
    DataFileID (*LoadWakeUpWordModel)(wuw_model_id_t model);
    /*! Sets the time offset to send to the splitter at VAD trigger */
    uint16      engine_init_preroll_ms;
} va_audio_wuw_config_t;

static inline bool VA_AUDIO_WUW_CONFIG_EQUAL(const va_audio_wuw_config_t *a, const va_audio_wuw_config_t *b)
{
    return ((a->engine == b->engine) &&
            (a->model == b->model) &&
            (a->LoadWakeUpWordModel == b->LoadWakeUpWordModel) &&
            (a->engine_init_preroll_ms == b->engine_init_preroll_ms)
           );
}

/*! \brief Wake-Up-Word detection parameters */
typedef struct
{
    /*! Buffer for mic data should be able to contain this many milliseconds */
    uint16                max_pre_roll_in_ms;
    va_audio_wuw_config_t wuw_config;
    va_audio_mic_config_t mic_config;
} va_audio_wuw_detection_params_t;

static inline bool VA_AUDIO_DETECTION_PARAMS_EQUAL(const va_audio_wuw_detection_params_t *a, const va_audio_wuw_detection_params_t *b)
{
    return ((a->max_pre_roll_in_ms == b->max_pre_roll_in_ms) &&
            VA_AUDIO_WUW_CONFIG_EQUAL(&a->wuw_config, &b->wuw_config) &&
            VA_AUDIO_MIC_CONFIG_EQUAL(&a->mic_config, &b->mic_config));
}

/*! \brief Information given for a Wake-Up-Word being detected */
typedef struct
{
    uint16 detection_confidence;
    /* Start Timestamp of Wake-Up-Word */
    uint32 start_timestamp;
    /* End Timestamp of Wake-Up-Word */
    uint32 end_timestamp;
} va_audio_wuw_detection_info_t;

/*! \brief Wake-Up-Word capture parameters */
typedef struct
{
    va_audio_encode_config_t encode_config;
    /*! Timestamp from which to start sending the buffered mic data */
    uint32 start_timestamp;
} va_audio_wuw_capture_params_t;

/*! \brief WuW engine version number */
typedef struct
{
    /*! The most significant word of the version number */
    uint16 msw;
     /*! The least significant word of the version number */
    uint16 lsw;
} va_audio_wuw_engine_version_t;

#endif /* VA_AUDIO_TYPES_H_ */

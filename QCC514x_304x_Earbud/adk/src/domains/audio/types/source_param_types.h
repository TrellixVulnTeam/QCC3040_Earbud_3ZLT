/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Datatypes common between audio and voice sources
*/

#ifndef SOURCE_PARAM_TYPES_H_
#define SOURCE_PARAM_TYPES_H_

#include <audio_sources_list.h>
#include <voice_sources_list.h>

typedef enum
{
    source_type_invalid = 0,
    source_type_voice,
    source_type_audio,
    source_type_max
} source_type_t;

typedef struct
{
    source_type_t type;
    union
    {
        audio_source_t audio;
        voice_source_t voice;
    } u;
} generic_source_t;

#define GENERIC_VOICE_SOURCE_MAKE(src) generic_source_t generic_##src = {.type = source_type_voice, .u = {.voice = src}}
#define GENERIC_AUDIO_SOURCE_MAKE(src) generic_source_t generic_##src = {.type = source_type_audio, .u = {.audio = src}}

#define GenericSource_IsAudio(src) (src.type == source_type_audio)
#define GenericSource_IsVoice(src) (src.type == source_type_voice)

#define GenericSource_IsValid(src) (src.type > source_type_invalid && src.type < source_type_max)

#define GenericSource_IsSameAudioSource(src1, src2) (GenericSource_IsAudio(src1) && GenericSource_IsAudio(src2) && (src1.u.audio == src2.u.audio))
#define GenericSource_IsSameVoiceSource(src1, src2) (GenericSource_IsVoice(src1) && GenericSource_IsVoice(src2) && (src1.u.voice == src2.u.voice))
#define GenericSource_IsSame(src1, src2) (GenericSource_IsSameAudioSource(src1, src2) || GenericSource_IsSameVoiceSource(src1, src2))

typedef struct
{
    unsigned data_length;
    void * data;
} source_defined_params_t;


typedef enum
{
    source_state_disconnected,
    source_state_connecting,
    source_state_connected,
    source_state_disconnecting,
    source_state_invalid = 0xFF
} source_state_t;

typedef enum
{
    source_status_ready,
    source_status_preparing,
    source_status_error
}source_status_t;

#endif /* SOURCE_PARAM_TYPES_H_ */

/****************************************************************************
Copyright (c) 2020 Qualcomm Technologies International, Ltd.

DESCRIPTION
    Protected API to only be used within the AMA component
*/

#ifndef AMA_AUDIO_PROTECTED_H
#define AMA_AUDIO_PROTECTED_H

#include <source.h>
#include <va_audio_types.h>

unsigned AmaAudio_HandleVoiceData(Source src);

bool AmaAudio_WakeWordDetected(va_audio_wuw_capture_params_t *capture_params, const va_audio_wuw_detection_info_t *wuw_info);

#endif // AMA_AUDIO_PROTECTED_H

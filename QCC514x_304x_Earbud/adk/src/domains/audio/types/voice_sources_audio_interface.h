/*!
\copyright  Copyright (c) 2018-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      This header file defines the interface that shall be implemented by all
            voice sources in the framework to abstract the control/query of audio
            routing.

            Implementation of this interface is mandatory for a voice source
*/

#ifndef VOICE_SOURCES_AUDIO_INTERFACE_H_
#define VOICE_SOURCES_AUDIO_INTERFACE_H_

#include "source_param_types.h"
#include "voice_sources_list.h"

typedef struct
{
    bool (*GetConnectParameters)(voice_source_t source, source_defined_params_t * source_params);
    void (*ReleaseConnectParameters)(voice_source_t source, source_defined_params_t * source_params);
    bool (*GetDisconnectParameters)(voice_source_t source, source_defined_params_t * source_params);
    void (*ReleaseDisconnectParameters)(voice_source_t source, source_defined_params_t * source_params);
    bool (*IsAudioRouted)(voice_source_t source);
    bool (*IsVoiceChannelAvailable)(voice_source_t source);
    source_status_t (*SetState)(voice_source_t source, source_state_t state);
} voice_source_audio_interface_t;

#endif /* VOICE_SOURCES_AUDIO_INTERFACE_H_ */

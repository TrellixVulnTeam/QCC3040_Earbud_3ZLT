/*!
\copyright  Copyright (c) 2018-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Interface to volume for voice sources - provides a mechanism for accessing
            the volume associated with a voice source.

            This interface is optional, used to extend the default volume functionality
            of a voice source
*/

#ifndef VOICE_SOURCES_VOLUME_INTERFACE_H_
#define VOICE_SOURCES_VOLUME_INTERFACE_H_

#include "voice_sources_list.h"
#include "volume_types.h"

/*! \brief The voice source volume interface
*/
typedef struct
{
    volume_t (*GetVolume)(voice_source_t source);
    void (*SetVolume)(voice_source_t source, volume_t volume);
    mute_state_t (*GetMuteState)(voice_source_t source);
    void (*SetMuteState)(voice_source_t source, mute_state_t state);
} voice_source_volume_interface_t;

#endif /* VOICE_SOURCES_VOLUME_INTERFACE_H_ */

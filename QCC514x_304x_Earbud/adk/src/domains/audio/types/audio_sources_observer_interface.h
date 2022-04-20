/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Interface to audio_sources_observer - provides a mechanism for observing specific
            changes within an audio source.

            This interface is optional and partial implementations are valid with every call
            within it also being optional
*/

#ifndef AUDIO_SOURCES_OBSERVER_INTERFACE_H_
#define AUDIO_SOURCES_OBSERVER_INTERFACE_H_

#include "audio_sources_list.h"
#include "volume_types.h"

#define MAX_OBSERVER_INTERFACES (6)

typedef enum
{
    source_routed,
    source_unrouted
} audio_routing_change_t;

/*! \brief The audio source observer interface
*/
typedef struct
{
    void (*OnVolumeChange)(audio_source_t source, event_origin_t origin, volume_t volume);
    void (*OnAudioRoutingChange)(audio_source_t source, audio_routing_change_t change);
    void (*OnMuteChange)(audio_source_t source, event_origin_t origin, bool mute_state);
} audio_source_observer_interface_t;

#endif /* AUDIO_SOURCES_OBSERVER_INTERFACE_H_ */

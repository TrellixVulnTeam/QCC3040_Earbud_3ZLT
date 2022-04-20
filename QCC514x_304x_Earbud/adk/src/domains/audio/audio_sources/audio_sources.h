/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   audio_sources Audio Sources
\ingroup    audio_domain
\brief      The audio sources component provides generic API to control any audio source.

The audio sources component allows multiple components to register its implementation of one or more interfaces.
Then the component using audio sources can 'route' its call to the specific implementation using audio_source_t source parameter.
In effect component using audio sources and in fact audio sources itself doesn't depend on the code implementing paricular audio source.

Each of interfaces works independently from others.
As such any combination of interfaces can be implemented and registered by a component.

Typical example would be A2DP profile in BT domain implementing the audio and volume interfaces,
AVRCP profile implementing media control and observer (for absolute volume) interfaces,
and Media Player service using the audio sources component to control it.

The interfaces are:
 - Audio - getting parameters required to use an audio source in the audio subsystem
 - Media Control - controlling playback of audio, like pause, fast forward
 - Volume - controlling volume of typical audio sources
 - Volume Control - controlling volume of audio sources where the volume value is determined by a remote device
 - Observer - currently notify on volume change only, and it is used to implement AVRCP absolute volume

*/

#ifndef AUDIO_SOURCES_H_
#define AUDIO_SOURCES_H_

#include "audio_sources_list.h"
#include "audio_sources_audio_interface.h"
#include "audio_sources_media_control_interface.h"
#include "audio_sources_volume_interface.h"
#include "audio_sources_volume_control_interface.h"
#include "audio_sources_observer_interface.h"
#include "source_param_types.h"

#include <device.h>

/*! \brief   Audio Sources UI Provider contexts

    Each Audio Source implemented in the CAA framework must provide an implementation of
    the UI Provider interface. This interface allows modules such as the Media Player
    service to determine the state of the Audio Source in a manner abstracted from the
    implementation specifics of that particular source, be it USB, line-in, A2DP etc.

    This abstracted state information can then be used in the CAA UI domain, services and
    via the Focus interface to allow the Application high level control of Audio use cases.

    \warning The values assigned to the symbolic identifiers of this enumerated type
             must not be modified.

    \note    This enumeration is used by the Focus Select module to determine relative
             priorities between Audio Sources and Voice Sources. This prioritisation is
             then used to determince which source should be the focus of UI interactions
             or audio routing. Look-up tables in the that module depend on the values
             assigned here remaining constant. In the event that new contexts are
             required, these should be added after the existing assignments, and the
             look-up tables in Focus Select must be maintained accordingly. */
typedef enum
{
    context_audio_disconnected = 0,     /*! Disconnected */
    context_audio_connected = 1,        /*! Connected but not receiving an audio stream */
    context_audio_is_streaming = 2,     /*! Receiving an audio stream with undefined content (tones, music, video, etc) */
    context_audio_is_playing = 3,       /*! Receiving an audio stream with playing content (music or video) */
    context_audio_is_va_response = 4,   /*! Receiving an audio stream that is a Voice Assistant response */
    context_audio_is_paused = 5,        /*! Receiving an audio stream with paused context */
    max_audio_contexts
} audio_source_provider_context_t;

/*\{*/
/*! \brief Initialise the audio sources domain

    \param init_task Unused
 */
bool AudioSources_Init(Task init_task);

/*! \brief Get the current context of source

    \param source The source to get the context of.

    \return The context of source
 */
unsigned AudioSources_GetSourceContext(audio_source_t source);

/*! \brief Get the Source Device associated with the provided Audio Source

    \param source The source for which to get the Source Device.

    \return The device associated with the source
 */
device_t AudioSources_GetSourceDevice(audio_source_t source);

/* Audio Interface */

/*! \brief Registers an audio interface for an audio source.

    \param source The audio source
    \param interface The audio source audio interface to register
 */
void AudioSources_RegisterAudioInterface(audio_source_t source, const audio_source_audio_interface_t * interface);

/*! \brief Get the connect parameters for a source using its registered audio interface.

    This may involve allocating memory therefore the complimenting AudioSources_ReleaseConnectParameters()
    must be called once the connect parameters are no longer required

    \param source The audio source
    \param source_params Pointer to the structure the source is to populate

    \return TRUE if parameters were populated, else FALSE
 */
bool AudioSources_GetConnectParameters(audio_source_t source, source_defined_params_t * source_params);

/*! \brief Cleanup/free the connect parameters for a source using its registered audio interface.

    The complimentary function to AudioSources_GetConnectParameters()

    \param source The audio source
    \param source_params Pointer to the structure originally populated by the equivalent get function
 */
void AudioSources_ReleaseConnectParameters(audio_source_t source, source_defined_params_t * source_params);

/*! \brief Get the disconnect parameters for a source using its registered audio interface.

    This may involve allocating memory therefore the complimenting AudioSources_ReleaseDisconnectParameters()
    must be called once the connect parameters are no longer required

    \param source The audio source
    \param source_params Pointer to the structure the source is to populate

    \return TRUE if parameters were populated, else FALSE
 */
bool AudioSources_GetDisconnectParameters(audio_source_t source, source_defined_params_t * source_params);

/*! \brief Cleanup/free the disconnect parameters for a source using its registered audio interface.

    The complimentary function to AudioSources_GetDisconnectParameters()

    \param source The audio source
    \param source_params Pointer to the structure originally populated by the equivalent get function
 */
void AudioSources_ReleaseDisconnectParameters(audio_source_t source, source_defined_params_t * source_params);

/*! \brief Check to determine if a audio sources audio is available.

    \param source The audio source

    \return TRUE if audio sources audio is available, else FALSE
 */
bool AudioSources_IsAudioRouted(audio_source_t source);

/*! \brief Inform source of it's current routing state

    \param source The audio source
    \param state The sources routed state

    \return source_status_ready if audio router can continue,
     source_status_preparing if the audio router should wait.
 */
source_status_t AudioSources_SetState(audio_source_t source, source_state_t state);

/* Media control Interface */
void AudioSources_RegisterMediaControlInterface(audio_source_t source, const media_control_interface_t * interface);
void AudioSources_Play(audio_source_t source);
void AudioSources_Pause(audio_source_t source);
void AudioSources_PlayPause(audio_source_t source);
void AudioSources_Stop(audio_source_t source);
void AudioSources_Forward(audio_source_t source);
void AudioSources_Back(audio_source_t source);
void AudioSources_FastForward(audio_source_t source, bool state);
void AudioSources_FastRewind(audio_source_t source, bool state);
void AudioSources_NextGroup(audio_source_t source);
void AudioSources_PreviousGroup(audio_source_t source);
void AudioSources_Shuffle(audio_source_t source, shuffle_state_t state);
void AudioSources_Repeat(audio_source_t source, repeat_state_t state);


/* Volume Interface */

/*! \brief Registers a volume interface for an audio source.

    \param source The audio source
    \param interface The audio source volume interface to register
 */
void AudioSources_RegisterVolume(audio_source_t source, const audio_source_volume_interface_t * interface);

/*! \brief Get the current volume for a source using its registered volume interface.

    \param source The audio source

    \return The volume of the specified audio source
 */
volume_t AudioSources_GetVolume(audio_source_t source);

/*! \brief Set the current volume for a source using its registered volume interface.

    \param source The audio source
    \param volume The new volume to set
 */
void AudioSources_SetVolume(audio_source_t source, volume_t volume);

/*! \brief Get the current mute state for a source using its registered volume interface.

    \param source The audio source

    \return The mute state of the specified source
 */
mute_state_t AudioSources_GetMuteState(audio_source_t source);

/*! \brief Set the current mute state for a source using its registered volume interface.

    \param source The audio source
    \param volume The new mute state to set
 */
void AudioSources_SetMuteState(audio_source_t source, mute_state_t mute_state);

/* Volume Control Interface */

/*! \brief Registers a volume control interface for an audio source.

    \param source The audio source
    \param interface The audio source volume control interface to register
 */
void AudioSources_RegisterVolumeControl(audio_source_t source, const audio_source_volume_control_interface_t * interface);

/*! \brief Checks to see whether a volume control interface has been registered for a given audio source.

    \param source The audio source

    \return TRUE if a volume control has been registered for the specified source, else FALSE
 */
bool AudioSources_IsVolumeControlRegistered(audio_source_t source);

/*! \brief Calls the volume up function of a sources registered volume control interface.

    \param source The audio source
 */
void AudioSources_VolumeUp(audio_source_t source);

/*! \brief Calls the volume down function of a sources registered volume control interface.

    \param source The audio source
 */
void AudioSources_VolumeDown(audio_source_t source);

/*! \brief Sets a specific volume using a sources registered volume control interface.

    \param source The audio source
    \param volume The new volume
 */
void AudioSources_VolumeSetAbsolute(audio_source_t source, volume_t volume);

/*! \brief Calls the mute function of a sources registered volume control interface.

    \param source The audio source
    \param state The mute state
 */
void AudioSources_Mute(audio_source_t source, mute_state_t state);


/* Observer Interface */

/*! \brief Registers an observer interface for an audio source.

    \param source The audio source
    \param interface The audio source observer interface to register
 */
void AudioSources_RegisterObserver(audio_source_t source, const audio_source_observer_interface_t * interface);

/*! \brief Calls the volume observer function of a sources registered observer interface.

    \param source The audio source
    \param origin The origin of the volume change event
    \param volume The new volume
 */
void AudioSources_OnVolumeChange(audio_source_t source, event_origin_t origin, volume_t volume);

/*! \brief Calls the OnAudioRouted observer function of a sources registered observer interface.

    \param source The audio source
    \param change Indicates whether the audio source became routed or unrouted
 */
void AudioSources_OnAudioRoutingChange(audio_source_t source, audio_routing_change_t change);

/*! \brief Calls the mute observer function of a sources registered observer interface.

    \param source The audio source
    \param origin The origin of the volume change event
    \param mute_state The new mute state, TRUE if the source is muted, else FALSE
 */
void AudioSources_OnMuteChange(audio_source_t source, event_origin_t origin, bool mute_state);

/*! \brief Unregisters an observer interface for an audio source.

    \param source The audio source
    \param interface The audio source observer interface to unregister
 */
void AudioSources_UnregisterObserver(audio_source_t source, const audio_source_observer_interface_t * interface);

/*! \brief Requests all sources with a registered interface to pause
 */
void AudioSources_PauseAll(void);

/*\}*/


/* Misc Functions */

/*! \brief Gets the currently routed audio source.

    \return the currently routed audio source
 */
audio_source_t AudioSources_GetRoutedSource(void);


/*! \brief Calculate the current output volume for an audio source

    The calculation takes into account the current volume and mute settings
    of the source and the current system volume and mute settings.

    If the source does not implement the Get/SetMuteState interface this
    calculation will use the current system mute state instead.

    \param source The audio source to calculate the output volume for.

    \return The output volume.
*/
volume_t AudioSources_CalculateOutputVolume(audio_source_t source);

#endif /* AUDIO_SOURCES_H_ */

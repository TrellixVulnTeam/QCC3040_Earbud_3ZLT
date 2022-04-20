/*!
\copyright  Copyright (c) 2018-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   voice_sources Voice Sources
\ingroup    audio_domain
\brief      The voice sources component provides generic API to control any voice source (like HFP or USB).

The voice source are analogous to the \ref audio_source audio source.

The interfaces are:
 - Audio - getting parameters required to use an voice source in the audio subsystem
 - Volume - controlling volume of typical voice sources
 - Volume Control - controlling volume of voice sources where the volume value is determined by a remote device
 - Observer - currently notify on volume change only
 - Telephony Control - performing telephony related actions

*/

#ifndef VOICE_SOURCES_H_
#define VOICE_SOURCES_H_

#include "voice_sources_list.h"
#include "voice_sources_audio_interface.h"
#include "voice_sources_telephony_control_interface.h"
#include "voice_sources_volume_interface.h"
#include "voice_sources_volume_control_interface.h"
#include "voice_sources_observer_interface.h"
#include "source_param_types.h"

/*! \brief   Voice Sources UI Provider contexts

    Each Voice Source implemented in the CAA framework must provide an implementation of
    the UI Provider interface. This interface allows modules such as the Telephony service
    to determine the state of the Voice Source in a manner abstracted from the implementation
    specifics of that particular source, be it USB, HFP, LE Uni-cast etc.

    This abstracted state information can then be used in the CAA UI domain, services and
    via the Focus interface to allow the Application high level control of Voice use cases.

    \warning The values assigned to the symbolic identifiers of this enumerated type
             must not be modified.

    \note    This enumeration is used by the Focus Select module to determine relative
             priorities between Audio Sources and Voice Sources. This prioritisation is
             then used to determince which source should be the focus of UI interactions
             or audio routing. Look-up tables in the Focus Select module depend on the
             values assigned here remaining constant. In the event that new contexts are
             required, these should be added after the existing assignments, and the
             look-up tables in Focus Select must be maintained accordingly. */
typedef enum
{
    context_voice_disconnected = 0,
    context_voice_connected = 1,
    context_voice_ringing_outgoing = 2,
    context_voice_ringing_incoming = 3,
    context_voice_in_call = 4,
    context_voice_in_call_with_incoming = 5,
    context_voice_in_call_with_outgoing = 6,
    context_voice_in_call_with_held = 7,
    context_voice_call_held = 8,
    context_voice_in_multiparty_call = 9,
    max_voice_contexts

} voice_source_provider_context_t;

/*\{*/
/*! \brief Initialise the voice sources domain

    \param init_task Unused
 */
bool VoiceSources_Init(Task init_task);

/*! \brief Get the current context of source

    \param source The source to get the context of.

    \return The context of source
 */
unsigned VoiceSources_GetSourceContext(voice_source_t source);

/* Voice Source Audio Interface */

/*! \brief Initialises the voice source audio registry.
 */
void VoiceSources_AudioRegistryInit(void);

/*! \brief Registers an audio interface for an voice source.

    \param source The voice source
    \param interface The voice source audio interface to register
 */
void VoiceSources_RegisterAudioInterface(voice_source_t source, const voice_source_audio_interface_t * interface);

/*! \brief Get the connect parameters for a source using its registered audio interface.

    This may involve allocating memory therefore the complimenting VoiceSources_ReleaseConnectParameters()
    must be called once the connect parameters are no longer required

    \param source The voice source
    \param source_params Pointer to the structure the source is to populate

    \return TRUE if parameters were populated, else FALSE
 */
bool VoiceSources_GetConnectParameters(voice_source_t source, source_defined_params_t * source_params);

/*! \brief Cleanup/free the connect parameters for a source using its registered audio interface.

    The complimentary function to AudioSources_GetConnectParameters()

    \param source The voice source
    \param source_params Pointer to the structure originally populated by the equivalent get function
 */
void VoiceSources_ReleaseConnectParameters(voice_source_t source, source_defined_params_t * source_params);

/*! \brief Get the disconnect parameters for a source using its registered audio interface.

    This may involve allocating memory therefore the complimenting VoiceSources_ReleaseDisconnectParameters()
    must be called once the connect parameters are no longer required

    \param source The voice source
    \param source_params Pointer to the structure the source is to populate

    \return TRUE if parameters were populated, else FALSE
 */
bool VoiceSources_GetDisconnectParameters(voice_source_t source, source_defined_params_t * source_params);

/*! \brief Cleanup/free the disconnect parameters for a source using its registered audio interface.

    The complimentary function to AudioSources_GetConnectParameters()

    \param source The voice source
    \param source_params Pointer to the structure originally populated by the equivalent get function
 */
void VoiceSources_ReleaseDisconnectParameters(voice_source_t source, source_defined_params_t * source_params);

/*! \brief Check to determine if a voice source is currently routed.

    \param source The voice source

    \return TRUE if voice sources audio is routed, else FALSE
 */
bool VoiceSources_IsAudioRouted(voice_source_t source);

/*! \brief Check to determine if a voice sources audio is available.

    \param source The voice source

    \return TRUE if voice sources audio is available, else FALSE
 */
bool VoiceSources_IsVoiceChannelAvailable(voice_source_t source);

/* Volume Interface */

/*! \brief Initialises the voice source volume registry.
 */
void VoiceSources_VolumeRegistryInit(void);

/*! \brief Registers a volume interface for a voice source.

    \param source The voice source
    \param interface The voice source volume interface to register
 */
void VoiceSources_RegisterVolume(voice_source_t source, const voice_source_volume_interface_t * interface);

/*! \brief Get the current volume for a source using its registered volume interface.

    \param source The voice source

    \return The volume of the specified source
 */
volume_t VoiceSources_GetVolume(voice_source_t source);

/*! \brief Set the current volume for a source using its registered volume interface.

    \param source The voice source
    \param volume The new volume to set
 */
void VoiceSources_SetVolume(voice_source_t source, volume_t volume);

/*! \brief Get the current mute state for a source using its registered volume interface.

    \param source The voice source

    \return The mute state of the specified source
 */
mute_state_t VoiceSources_GetMuteState(voice_source_t source);

/*! \brief Set the current mute state for a source using its registered volume interface.

    \param source The voice source
    \param volume The new mute state to set
 */
void VoiceSources_SetMuteState(voice_source_t source, mute_state_t mute_state);

/* Volume Control Interface */

/*! \brief Initialises the voice source volume control registry.
 */
void VoiceSources_VolumeControlRegistryInit(void);

/*! \brief Registers a volume control interface for a voice source.

    \param source The voice source
    \param interface The voice source volume control interface to register
 */
void VoiceSources_RegisterVolumeControl(voice_source_t source, const voice_source_volume_control_interface_t * interface);

/*! \brief Checks to see whether a volume control interface has been registered for a given voice source.

    \param source The voice source

    \return TRUE if a volume control has been registered for the specified source, else FALSE
 */
bool VoiceSources_IsVolumeControlRegistered(voice_source_t source);

/*! \brief Calls the volume up function of a sources registered volume control interface.

    \param source The voice source
 */
void VoiceSources_VolumeUp(voice_source_t source);

/*! \brief Calls the volume down function of a sources registered volume control interface.

    \param source The voice source
 */
void VoiceSources_VolumeDown(voice_source_t source);

/*! \brief Sets a specific volume using a sources registered volume control interface.

    \param source The voice source
    \param volume The new volume
 */
void VoiceSources_VolumeSetAbsolute(voice_source_t source, volume_t volume);

/*! \brief Calls the mute function of a sources registered volume control interface.

    \param source The voice source
    \param state The mute state
 */
void VoiceSources_Mute(voice_source_t source, mute_state_t state);


/* Observer Interface */

/*! \brief Initialises the voice source observer registry.
 */
void VoiceSources_ObserverRegistryInit(void);

/*! \brief Registers an observer interface for a voice source.

    \param source The voice source
    \param interface The voice source observer interface to register
 */
void VoiceSources_RegisterObserver(voice_source_t source, const voice_source_observer_interface_t * interface);

/*! \brief Deregisters the observer interface for a voice source.

    \param source The voice source
 */
void VoiceSources_DeregisterObserver(voice_source_t source, const voice_source_observer_interface_t * interface);

/*! \brief Calls the volume observer function of a sources registered observer interface.

    \param source The voice source
    \param origin The origin of the volume change event
    \param volume The new volume
 */
void VoiceSources_OnVolumeChange(voice_source_t source, event_origin_t origin, volume_t volume);

/*! \brief Calls the mute state observer function of a sources registered observer interface.

    \param source The voice source
    \param origin The origin of the mute change event
    \param volume The new mute state, TRUE if muted else FALSE
 */
void VoiceSources_OnMuteChange(voice_source_t source, event_origin_t origin, bool mute_state);

/* Telephony Control Interface */

/*! \brief Initialises the voice source telephony control registry.
 */
void VoiceSources_TelephonyControlRegistryInit(void);

/*! \brief Registers an telephony control interface for a voice source.

    \param source The voice source
    \param interface The voice source telephony control interface to register
 */
void VoiceSources_RegisterTelephonyControlInterface(voice_source_t source, const voice_source_telephony_control_interface_t * interface);

/*! \brief Deregisters any previously registered telephony control interface for a voice source.

    \param source The voice source
 */
void VoiceSources_DeregisterTelephonyControlInterface(voice_source_t source);

/*! \brief Calls the accept incoming call function of a sources registered telephony control interface.

    \param source The voice source
 */
void VoiceSources_AcceptIncomingCall(voice_source_t source);

/*! \brief Calls the reject incoming call function of a sources registered telephony control interface.

    \param source The voice source
 */
void VoiceSources_RejectIncomingCall(voice_source_t source);

/*! \brief Calls the terminate ongoing call function of a sources registered telephony control interface.

    \param source The voice source
 */
void VoiceSources_TerminateOngoingCall(voice_source_t source);

/*! \brief Transfers the audio of an ongoing call. 

    \param source The voice source
    \param direction The direction to transfer the audio of an ongoing call.
    
     \note  voice_source_audio_transfer_toggle : ongoing call audio is transferred to the other end (AG or device)
            does not currently have a link.
 */
void VoiceSources_TransferOngoingCallAudio(voice_source_t source, voice_source_audio_transfer_direction_t direction);

/*! \brief Initiates a call using the given number via a sources registered telephony control interface.

    \param source The voice source
    \param number The phone number to dial
 */
void VoiceSources_InitiateCallUsingNumber(voice_source_t source, phone_number_t number);

/*! \brief Initiates a voice dial with the handsets native voice service via a sources registered telephony control interface.

    \param source The voice source
 */
void VoiceSources_InitiateVoiceDial(voice_source_t source);

/*! \brief Initiates an outgoing call to the last dialled number via a sources registered telephony control interface.

    \param source The voice source
 */
void VoiceSources_InitiateCallLastDialled(voice_source_t source);

/*! \brief Toggles the microphone mute state via a sources registered telephony control interface.

    \param source The voice source
 */
void VoiceSources_ToggleMicrophoneMute(voice_source_t source);

/*\}*/

/* Misc Functions */

/*! \brief Gets the currently routed voice source.

    \return the currently routed voice source
 */
voice_source_t VoiceSources_GetRoutedSource(void);

/*! \brief To determine whether voice is currently routed.

\return TRUE if voice routed , else FALSE
 */
bool VoiceSources_IsAnyVoiceSourceRouted(void);

/*! \brief Inform source of it's current routing state

    \param source The voice source
    \param state The sources routed state

    \return source_status_ready if audio router can continue,
     source_status_preparing if the audio router should wait.
 */
source_status_t VoiceSources_SetState(voice_source_t source, source_state_t state);

/*! \brief Determine if this source has an implementation registered for its
           voice_source_telephony_control_interface_t.

    \param source - The voice source

    \return True means an implementation is registered, otherwise False
 */
bool VoiceSources_IsSourceRegisteredForTelephonyControl(voice_source_t source);

/*! \brief Calculate the current output volume for a voice source

    The calculation takes into account the current volume and mute settings
    of the source and the current system volume and mute settings.

    If the source does not implement the Get/SetMuteState interface this
    calculation will use the current system mute state instead.

    \param source The voice source to calculate the output volume for.

    \return The output volume.
*/
volume_t VoiceSources_CalculateOutputVolume(voice_source_t source);

/*! \brief Perform an enhanced call control action

    \param source The voice source to perform the action on
    
    \param action The action to perform
*/
void VoiceSources_TwcControl(voice_source_t source, voice_source_twc_control_t action);

#endif /* VOICE_SOURCES_H_ */

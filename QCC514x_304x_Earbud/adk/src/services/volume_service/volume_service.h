/*!
\copyright  Copyright (c) 2018-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   volume_service Volume Service
\ingroup    services
\brief      The volume service utilises functionality in the domain layer to provide a high level,
            user-centric interface to the application for controlling volume.

The Volume service uses \ref audio_domain Audio domain.

*/

#ifndef VOLUME_SERVICE_H_
#define VOLUME_SERVICE_H_

#include "audio_sources_list.h"
#include "domain_message.h"
#include "voice_sources_list.h"
#include "volume_types.h"

/*! Messages sent by the volume service to interested clients. */
enum volume_service_messages
{
    VOLUME_SERVICE_MAX_VOLUME = VOLUME_SERVICE_MESSAGE_BASE,
    VOLUME_SERVICE_MIN_VOLUME,
    VOLUME_SERVICE_VOLUME_UPDATED,

    /*! This must be the final message */
    VOLUME_SERVICE_MESSAGE_END
};

/*\{*/

/*! \brief Initialises the volume service.

    \param init_task Not used

    \return TRUE
 */
bool VolumeService_Init(Task init_task);

/*! \brief Sets the volume of an audio source.

    \param source The audio source
    \param origin The origin of the call to change the volume
    \param new_volume The intended volume
 */
void VolumeService_SetAudioSourceVolume(audio_source_t source, event_origin_t origin, volume_t new_volume);

/*! \brief Increments the volume of an audio source.

    \param source The audio source
    \param origin The origin of the call to change the volume
 */
void VolumeService_IncrementAudioSourceVolume(audio_source_t source, event_origin_t origin);

/*! \brief Decrements the volume of an audio source.

    \param source The audio source
    \param origin The origin of the call to change the volume
 */
void VolumeService_DecrementAudioSourceVolume(audio_source_t source, event_origin_t origin);

/*! \brief Mute the local output.

    \param source The audio source requesting the mute
    \param origin The origin of the call to change the volume
    \param mute_state The mute state requested, TRUE for mute, FALSE for unmute
 */
void VolumeService_AudioSourceMute(audio_source_t source, event_origin_t origin, bool mute_state);

/*! \brief Sets the system volume.

    \param origin The origin of the call to change the volume
    \param new_volume The intended volume
 */
void VolumeService_SetSystemVolume(event_origin_t origin, volume_t new_volume);

/*! \brief Increments the system volume.

    \param origin The origin of the call to change the volume
 */
void VolumeService_IncrementSystemVolume(event_origin_t origin);

/*! \brief Decrements the system volume.

    \param origin The origin of the call to change the volume
 */
void VolumeService_DecrementSystemVolume(event_origin_t origin);

/*! \brief Sets the volume of a voice source.

    \param source The voice source
    \param origin The origin of the call to set the volume
    \param new_volume The intended volume
 */
void VolumeService_SetVoiceSourceVolume(voice_source_t source, event_origin_t origin, volume_t new_volume);

/*! \brief Increments the volume of a voice source.

    \param source The voice source
    \param origin The origin of the call to change the volume
 */
void VolumeService_IncrementVoiceSourceVolume(voice_source_t source, event_origin_t origin);

/*! \brief Decrements the volume of a voice source.

    \param source The voice source
    \param origin The origin of the call to change the volume
 */
void VolumeService_DecrementVoiceSourceVolume(voice_source_t source, event_origin_t origin);

/*! \brief Mute the local output.

    \param source The voice source requesting the mute
    \param origin The origin of the call to change the volume
    \param mute_state The mute state requested, TRUE for mute, FALSE for unmute
 */
void VolumeService_VoiceSourceMute(voice_source_t source, event_origin_t origin, bool mute_state);

/*! \brief Change the volume of an A2DP/AVRCP source

    \note A2DP/AVRCP volume range is 0-127.
    \note Increasing the volume by 100 from 100 will return TRUE as the
    volume changed but the level set will be the maximum - 127

    \param  source  The audio source to which the volume change should be applied
    \param  step    Relative amount to adjust the volume by.
                    This can be negative.
*/
void VolumeService_ChangeAudioSourceVolume(audio_source_t source, int16 step);

/*\}*/

#endif /* VOLUME_SERVICE_H_ */

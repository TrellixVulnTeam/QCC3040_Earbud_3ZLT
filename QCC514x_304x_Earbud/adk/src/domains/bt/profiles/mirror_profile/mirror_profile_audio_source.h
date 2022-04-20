/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Mirror profile audio source control.
*/

#ifndef MIRROR_PROFILE_AUDIO_SOURCE_H_
#define MIRROR_PROFILE_AUDIO_SOURCE_H_

#include "audio_sources.h"
#include "audio_sources_audio_interface.h"
#include "source_param_types.h"

/*! \brief Gets the mirror A2DP audio interface.

    \return The audio source audio interface for a mirror A2DP source
 */
const audio_source_audio_interface_t *MirrorProfile_GetAudioInterface(void);

/*! \brief Gets the mirror media control interface.

    \return The interface.
 */
const media_control_interface_t *MirrorProfile_GetMediaControlInterface(void);


/*! \brief Read the connect parameters from the source and store them in the
        mirror profile a2dp state.

    \param source The audio source.

    \return TRUE if connect parameters were valid, else FALSE.
 */
bool MirrorProfile_StoreAudioSourceParameters(audio_source_t source);

/*! \brief Start audio for A2DP. */
void MirrorProfile_StartA2dpAudio(void);

/*! \brief Stop audio for A2DP. */
void MirrorProfile_StopA2dpAudio(void);

/*! \brief Start A2DP audio synchronisation with the other earbud. */
void MirrorProfile_StartA2dpAudioSynchronisation(void);

/*! \brief Stop A2DP audio synchronisation with the other earbud. */
void MirrorProfile_StopA2dpAudioSynchronisation(void);


#endif

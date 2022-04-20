/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      The audio source observer interface implementation provided by Mirror Profile
*/

#ifndef MIRROR_PROFILE_VOLUME_OBSERVER_H_
#define MIRROR_PROFILE_VOLUME_OBSERVER_H_

#include "audio_sources_observer_interface.h"

/*! \brief Registers mirror profile as an observer to Audio Source Observer 
           interface for the mirrored source
 */
void mirrorProfile_RegisterForMirroredSourceVolume(void);

/*! \brief Unregisters mirror profile as an observer to Audio Source Observer 
           interface for the mirrored source.
 */
void mirrorProfile_UnregisterForMirroredSourceVolume(void);

#endif /* MIRROR_PROFILE_VOLUME_OBSERVER_H_ */

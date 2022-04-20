/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup  Wired Audio
\ingroup    wired audio
\brief  wired audio volume interface definitions
*/

#ifndef WIRED_AUDIO_VOLUME_INTERFACE_H_
#define WIRED_AUDIO_VOLUME_INTERFACE_H_

#include "audio_sources_volume_interface.h"

/*\{*/

/*! \brief Gets the Wired audio volume interface.

    \return The audio volume interface for an Wired audio source
 */
const audio_source_volume_interface_t * WiredAudioSource_GetWiredVolumeInterface(void);

#endif /* WIRED_AUDIO_VOLUME_INTERFACE_H_ */


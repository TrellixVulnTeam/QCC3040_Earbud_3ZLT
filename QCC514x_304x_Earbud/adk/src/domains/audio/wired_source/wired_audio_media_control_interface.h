/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   wired_audio_source Wired Audio
\ingroup    wired audio
\brief      The wired audio media control interface implementations.
*/

#ifndef WIRED_AUDIO_MEDIA_CONTROL_INTERFACE_H_
#define WIRED_AUDIO_MEDIA_CONTTOL_INTERFACE_H_

#include "audio_sources_media_control_interface.h"

/*\{*/

/*! \brief Gets the Wired audio media control interface.

    \return The audio source media control interface for an Wired audio source
 */
const media_control_interface_t * WiredAudioSource_GetMediaControlInterface(void);

#endif /* WIRED_AUDIO_MEDIA_CONTTOL_INTERFACE_H_ */


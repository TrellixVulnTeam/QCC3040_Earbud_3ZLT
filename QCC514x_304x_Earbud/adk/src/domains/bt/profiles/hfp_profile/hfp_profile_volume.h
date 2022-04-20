/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   hfp_profile_volume HFP Profile Volume
\ingroup    hfp_profile
\brief      The voice source volume interface implementation for HFP sources
*/

#ifndef HFP_PROFILE_VOLUME_H_
#define HFP_PROFILE_VOLUME_H_

#include "hfp_profile_typedef.h"

#include "voice_sources.h"
#include "voice_sources_volume_interface.h"

/*\{*/
/*! \brief Initialise HFP volume.
 *
    \param source The voice source to configure.
    \param init_volume The initial volume level.
*/
void HfpProfileVolume_Init(voice_source_t source, uint8 init_volume);

/*! \brief Gets the HFP volume interface.

    \return The voice source volume interface for an HFP source
 */
const voice_source_volume_interface_t * HfpProfile_GetVoiceSourceVolumeInterface(void);

/*! \brief Notify all registered clients of new HFP volume. */
void HfpProfileVolume_NotifyClients(voice_source_t source, uint8 new_volume);

/*\}*/

#endif /* HFP_PROFILE_VOLUME_H_ */

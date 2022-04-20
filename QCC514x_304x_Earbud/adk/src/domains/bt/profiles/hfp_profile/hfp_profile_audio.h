/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      The voice source audio interface implementation for hfp voice sources
*/

#ifndef HFP_PROFILE_AUDIO_H_
#define HFP_PROFILE_AUDIO_H_

#include "hfp_profile.h"
#include "voice_sources_audio_interface.h"

/*! \brief Gets the HFP audio interface.

    \return The voice source audio interface for an HFP source
 */
const voice_source_audio_interface_t * HfpProfile_GetAudioInterface(void);

void HfpProfile_StoreConnectParams(hfpInstanceTaskData * instance, uint8 codec, uint8 wesco, uint8 tesco, uint16 qce_codec_mode_id);

#endif /* HFP_PROFILE_AUDIO_H_ */

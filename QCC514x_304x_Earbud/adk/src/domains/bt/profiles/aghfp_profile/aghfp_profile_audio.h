/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      The voice source audio interface implementation for aghfp voice sources
*/


#ifndef AGHFP_PROFILE_AUDIO_H
#define AGHFP_PROFILE_AUDIO_H

#include "voice_sources_audio_interface.h"
#include <aghfp.h>

/*! \brief Gets the AGHFP audio interface.

    \return The voice source audio interface for an AGHFP source
 */
const voice_source_audio_interface_t * AghfpProfile_GetAudioInterface(void);

void AghfpProfile_StoreConnectParams(const AGHFP_AUDIO_CONNECT_CFM_T *cfm);

#endif // AGHFP_PROFILE_AUDIO_H

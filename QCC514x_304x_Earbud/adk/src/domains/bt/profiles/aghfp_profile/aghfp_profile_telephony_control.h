/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   aghfp_profile_telephony_control HFP Profile Telephony Control
\ingroup    aghfp_profile
\brief      The voice source telephony control interface implementation for AGHFP sources
*/

#ifndef AGHFP_PROFILE_TELEPHONY_CONTROL_H
#define AGHFP_PROFILE_TELEPHONY_CONTROL_H

#include "voice_sources_telephony_control_interface.h"

/*! \brief Gets the HFP telephony control interface.

    \return The voice source telephony control interface for an HFP source
 */
const voice_source_telephony_control_interface_t * AghfpProfile_GetTelephonyControlInterface(void);

#endif // AGHFP_PROFILE_TELEPHONY_CONTROL_H

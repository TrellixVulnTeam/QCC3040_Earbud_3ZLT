/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      AGHFP state machine component.
*/

#ifndef AGHFP_PROFILE_SM_H
#define AGHFP_PROFILE_SM_H

#include "aghfp_profile_typedef.h"

aghfpState AghfpProfile_GetState(aghfpInstanceTaskData* instance);
void AghfpProfile_SetState(aghfpInstanceTaskData* instance, aghfpState state);

#endif // AGHFP_PROFILE_SM_H

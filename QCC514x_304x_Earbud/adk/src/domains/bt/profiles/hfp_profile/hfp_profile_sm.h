/*!
\copyright  Copyright (c) 2008 - 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   hfp_profile HFP Profile
\ingroup    profiles
\brief      HFP Profile state machine.
*/

#ifndef HFP_PROFILE_SM_H_
#define HFP_PROFILE_SM_H_

#include <task_list.h>

#include "hfp_profile_typedef.h"

void appHfpSetState(hfpInstanceTaskData* instance, hfpState state);
hfpState appHfpGetState(hfpInstanceTaskData* instance);

#endif /* HFP_PROFILE_SM_H_ */

/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Kymera private header with state related definitions
*/

#ifndef KYMERA_STATE_H_
#define KYMERA_STATE_H_

#include "kymera_state_types.h"

void appKymeraSetState(appKymeraState state);
appKymeraState appKymeraGetState(void);
bool appKymeraIsBusy(void);
bool appKymeraInConcurrency(void);
bool appKymeraIsBusyStreaming(void);
bool appKymeraIsScoActive(void);

#endif /* KYMERA_STATE_H_ */

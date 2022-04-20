/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Kymera module for its internal state
*/

#include "kymera_state.h"
#include "kymera_data.h"
#include "kymera_anc.h"
#include "kymera_adaptive_anc.h"
#include <logging.h>

void appKymeraSetState(appKymeraState state)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOG_STATE("appKymeraSetState, state %u -> %u", theKymera->state, state);
    theKymera->state = state;
    KymeraAnc_PreStateTransition(state);
    /* Set busy lock if not in idle or tone state */
    theKymera->busy_lock = (state != KYMERA_STATE_IDLE) &&
                           (state != KYMERA_STATE_TONE_PLAYING) &&
                           (state != KYMERA_STATE_STANDALONE_LEAKTHROUGH) &&
                           (state != KYMERA_STATE_ADAPTIVE_ANC_STARTED);
}

appKymeraState appKymeraGetState(void)
{
    return KymeraGetTaskData()->state;
}

bool appKymeraIsBusy(void)
{
    return appKymeraGetState() != KYMERA_STATE_IDLE;
}

bool appKymeraInConcurrency(void)
{
    return (KymeraAdaptiveAnc_IsConcurrencyActive());
}

bool appKymeraIsBusyStreaming(void)
{
    return (appKymeraGetState() == KYMERA_STATE_A2DP_STREAMING) || (appKymeraGetState() == KYMERA_STATE_A2DP_STREAMING_WITH_FORWARDING);
}

bool appKymeraIsScoActive(void)
{
    return (appKymeraGetState() == KYMERA_STATE_SCO_ACTIVE) || (appKymeraGetState() == KYMERA_STATE_SCO_SLAVE_ACTIVE);
}

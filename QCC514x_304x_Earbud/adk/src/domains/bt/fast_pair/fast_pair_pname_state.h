/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       fast_pair_pname_state.h
\brief      Header file of Fast Pair personalized name state
*/


#ifndef FAST_PAIR_PNAME_STATE_H_
#define FAST_PAIR_PNAME_STATE_H_

#include "fast_pair.h"
#include "fast_pair_events.h"


/******************************************************************************
NAME
    fastPair_StatePNameHandleEvent

DESCRIPTION
    Event handler for the Fast Pair Personalized Name.

RETURNS
    Bool indicating if the event was successfully processed.
*/
bool fastPair_StatePNameHandleEvent(fast_pair_state_event_t event);

/*! \brief Get the Fast Pair Personalized Name.

    \param pname      Personalized Name buffer of size FAST_PAIR_PNAME_DATA_LEN to be read to.
    \param pname_len  Valid length of Personalized Name read.

    \return bool TRUE if Personalized Name is read else FALSE
 */
bool fastPair_GetPName(uint8 pname[FAST_PAIR_PNAME_DATA_LEN], uint8 *pname_len);

#endif

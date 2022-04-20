/*******************************************************************************
Copyright (c) 2008 - 2019 Qualcomm Technologies International, Ltd.


FILE NAME
    fast_pair_wait_aes_key_state.h

DESCRIPTION
    Event handling functions for the Fast Pair Wait for AES Key State.
*/


#ifndef FAST_PAIR_WAIT_AES_KEY_STATE_H_
#define FAST_PAIR_WAIT_AES_KEY_STATE_H_

#include "fast_pair.h"
#include "fast_pair_events.h"

/* KBP - Device Action: Data ID for Personalized Name */
#define FAST_PAIR_DEVICE_ACTION_REQ_DATA_ID_PNAME             (0x01)

/******************************************************************************
NAME
    fastPair_StateWaitAESKeyHandleEvent

DESCRIPTION
    Event handler for the Fast Pair Wait for AES Key State.

RETURNS
    Bool indicating if the event was successfully processed.
*/
bool fastPair_StateWaitAESKeyHandleEvent(fast_pair_state_event_t event);

/******************************************************************************
NAME
    fastPair_IsProviderPairingRequested

DESCRIPTION
    To identify either of seeker or provider has initiated pairing

RETURNS
    TRUE, if pairing is initiated by provider
    FALSE, otherwise
*/
bool fastPair_IsProviderPairingRequested(void);

#endif

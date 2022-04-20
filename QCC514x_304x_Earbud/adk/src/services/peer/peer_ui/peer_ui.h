/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       peer_ui.h
\brief	    Intercept UI input messages, add the delay and re-inject locally for
            synchronisation of UI input messages such as prompts.
*/

#ifndef PEER_UI_H_
#define PEER_UI_H_

#include "system_clock.h"

#define PEER_UI_PROMPT_DELAY_MS  (200U)
#define PEER_UI_PROMPT_DELAY_US  (US_PER_MS * PEER_UI_PROMPT_DELAY_MS)

#define PEER_ANC_UI_INPUT_DELAY_MS  (200U)
#define PEER_ANC_UI_INPUT_DELAY_US  (US_PER_MS * PEER_ANC_UI_INPUT_DELAY_MS)

#define PEER_LEAKTHROUGH_UI_INPUT_DELAY_MS  (300U)
#define PEER_LEAKTHROUGH_UI_INPUT_DELAY_US  (US_PER_MS * PEER_LEAKTHROUGH_UI_INPUT_DELAY_MS)

/*! brief Initialise Peer Ui  module */
bool PeerUi_Init(Task init_task);

#endif /* PEER_UI_H_ */

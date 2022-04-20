/*!
\copyright  Copyright (c) 2019 - 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       voice_ui.h
\defgroup   voice_ui 
\ingroup    services
\brief      A component responsible for controlling voice assistants.

Responsibilities:
- Voice Ui control (like A way to start/stop voice capture as a result of some user action) 
  Notifications to be sent to the Application raised by the VA.

The Voice Ui uses  \ref audio_domain Audio domain and \ref bt_domain BT domain.

*/

#ifndef VOICE_UI_H_
#define VOICE_UI_H_

#include "voice_ui_message_ids.h"

/*! \brief Voice UI Provider contexts */
typedef enum
{
    context_voice_ui_default = 0
} voice_ui_context_t;

/*\{*/

/*! \brief Initialise the voice ui service

    \param init_task Unused
 */
bool VoiceUi_Init(Task init_task);

/*! \brief Checks if a VA session is in progress
    \return TRUE if a session is ongoing, FALSE otherwise
 */
bool VoiceUi_IsSessionInProgress(void);

/*\}*/

#endif /* VOICE_UI_H_ */

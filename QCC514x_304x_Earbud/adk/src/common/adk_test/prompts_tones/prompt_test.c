/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of common prompts specific testing functions.
*/

#include "prompt_test.h"
#include "ui_indicator_prompts.h"

#ifdef GC_SECTIONS
/* Move all functions in KEEP_PM section to ensure they are not removed during
 * garbage collection */
#pragma unitcodesection KEEP_PM
#endif

void PromptTest_Play(MessageId sys_event)
{
    MessageSend(UiPrompts_GetUiPromptsTask(), sys_event, NULL);
}

/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    adk_test_common
\ingroup    common
\brief      Interface for common prompts specific testing functions.
*/

/*! @{ */

#ifndef PROMPT_TEST_H
#define PROMPT_TEST_H

/*! \brief Play prompt associated with a specific system event.
           The mapping between system events and prompts is the one provided from the application to ui_prompts.
    \param sys_event The system event associated with the prompt.
*/
void PromptTest_Play(MessageId sys_event);

#endif /* PROMPT_TEST_H */

/*! @} */


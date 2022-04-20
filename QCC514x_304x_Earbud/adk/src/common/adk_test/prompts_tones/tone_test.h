/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    adk_test_common
\brief      Interface for common tones specific testing functions.
*/

/*! @{ */

#ifndef TONE_TEST_H
#define TONE_TEST_H

/*! \brief Play tone associated with a specific system event.
           The mapping between system events and tones is the one provided from the application to ui_tones.
    \param sys_event The system event associated with the tone.
*/
void ToneTest_Play(MessageId sys_event);

#endif /* TONE_TEST_H */

/*! @} */

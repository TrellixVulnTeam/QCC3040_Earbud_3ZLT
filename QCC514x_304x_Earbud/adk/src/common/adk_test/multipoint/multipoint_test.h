/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    adk_test_common
\brief      Interface for common multipoint specifc testing functions.
*/

/*! @{ */

#ifndef MULTIPOINT_TEST_H
#define MULTIPOINT_TEST_H

/*! \brief Enable Multipoint so two BRERDR connections are allowed at a time. */
void MultipointTest_EnableMultipoint(void);

/*! \brief To check if Multipoint is enabled. */
bool MultipointTest_IsMultipointEnabled(void);

/*! \brief Find the number of connected handsets.

    \return The current number of connected handsets
*/
uint8 MultipointTest_NumberOfHandsetsConnected(void);

/*! \brief Enable or disable A2DP barge in behaviour 

    \param enable Enable or disable barge in
*/
void MultipointTest_EnableA2dpBargeIn(bool enable);

#endif /* MULTIPOINT_TEST_H */

/*! @} */


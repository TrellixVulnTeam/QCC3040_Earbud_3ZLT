/*!
\copyright  Copyright (c) 2008 - 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       phy_state_config.h
\brief      Configuration parameters for the Phy State component.
*/

/*! \brief Define the minimum time between generation of PHY_STATE_CHANGED_IND messages.

    \note If no PHY_STATE_CHANGED_IND has been sent for appPhyStateXXXNotificationsLimitMs
          the event will always be sent.

    \note If a PHY_STATE_CHANGED_IND was sent less than appPhyStateXXXNotificationsLimitMs
          ago, wait until that time has elapsed before sending the message.
          If after appPhyStateXXXNotificationsLimitMs has elapsed the sta0te is the same
          as last notified to clients, then DO NOT send the indication.

    \note Set to 0 to disable restrictions on PHY_STATE_CHANGED_IND messages and send
          PHY_STATE_CHANGED_IND message for event change.
 */
#define appPhyStateOutOfCaseNotificationsLimitMs() (3000)
#define appPhyStateInCaseNotificationsLimitMs()    (3000)
#define appPhyStateInEarNotificationsLimitMs()      (500)
#define appPhyStateOutOfEarNotificationsLimitMs()   (500)
#define appPhyStateInMotionNotificationsLimitMs()   (500)


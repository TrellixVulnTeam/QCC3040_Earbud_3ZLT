/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\brief	    Internal header file for the Power Management.

*/

#ifndef POWER_MANAGER_PRIVATE_H_
#define POWER_MANAGER_PRIVATE_H_

#include <domain_message.h>

typedef enum
{
    /*! Message to relinquish performance mode after a delay */
    POWER_MANAGER_INTERNAL_MESSAGE_PERFORMANCE_RELINIQUISH,

    /*! Always the last message */
    POWER_MANAGER_INTERNAL_MESSAGES_END,

} power_manager_internal_msgs_t;


ASSERT_INTERNAL_MESSAGES_NOT_OVERFLOWED(POWER_MANAGER_INTERNAL_MESSAGES_END)


#endif

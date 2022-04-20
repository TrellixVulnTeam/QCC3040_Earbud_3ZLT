/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Defines internal to the link policy manager
*/

#include "link_policy.h"
#include "link_policy_config.h"

#include <domain_message.h>

typedef enum 
{
    /*! Link policy discover role message which will be sent to
        link policy task after LINK_POLICY_DISCOVER_ROLE_TIMEOUT_MS
        if role change attempt fails. */
    LINK_POLICY_DISCOVER_ROLE,

    /*! Process a policy update requested earlier */
    LINK_POLICY_SCHEDULED_UPDATE,

    /*! This must be the final message */
    LINK_POLICY_INTERNAL_MESSAGE_END
} link_policy_internal_message_t;
ASSERT_INTERNAL_MESSAGES_NOT_OVERFLOWED(LINK_POLICY_INTERNAL_MESSAGE_END)

#define LINK_POLICY_DISCOVER_ROLE_TIMEOUT_MS  150


/*! Macro for creating link policy internal message based on the message name */
#define MAKE_LP_MESSAGE(TYPE) TYPE##_T *message = PanicUnlessNew(TYPE##_T);


/*! Link policy task structure */
typedef struct
{
    /*!< Link policy manager task */
    TaskData task;
} lpTaskData;

/*! Link policy discover role message data */
typedef struct
{
    /*!< Bluetooth address of remote device */
    bdaddr bd_addr;
}LINK_POLICY_DISCOVER_ROLE_T;

/*!< Link Policy Manager data structure */
extern lpTaskData  app_lp;

/*! Get pointer to Link Policy Manager data structure */
#define LinkPolicyGetTaskData()  (&app_lp)
#define LinkPolicyGetTask()      (&app_lp.task)


/*! @brief Update the link policy of connected handsets based on the system state.

    @param bd_addr The Bluetooth address of the handset whose state has changed.
    This may be set NULL if the state change is not related to any single handset.
    This address is not currently used, but may be used in the future.

    This API is similar to \ref appLinkPolicyUpdatePowerTable but the power table
    update is scheduled internally in the link policy code and will be applied
    after a short delay.
*/
void appLinkPolicyUpdatePowerTableDeferred(const bdaddr *bd_addr);

/*! @brief Discover role again if it has failed in previous attempt.
    @param bd_addr The Bluetooth address of the remote device.
*/
void appLinkPolicyHandleDiscoverRole(const LINK_POLICY_DISCOVER_ROLE_T * msg);


/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    device_test_service
\brief      Definition file for internal messages used by the device test service
*/

#include <domain_message.h>

/*! Internal messages for the device test service */
typedef enum
{
        /*! Internal message indicating that a reboot is now required */
    DEVICE_TEST_SERVICE_INTERNAL_REBOOT = INTERNAL_MESSAGE_BASE,
        /*! Internal message indicating that the test session 
            should be ended (without a reboot) */
    DEVICE_TEST_SERVICE_INTERNAL_END_TESTING,
        /*! Internal message indicating that we should clean any old
            entries */
    DEVICE_TEST_SERVICE_INTERNAL_CLEAN_UP,

    /*! This must be the final message */
    DEVICE_TEST_SERVICE_INTERNAL_MESSAGE_END
} device_test_service_internal_msg_t;
ASSERT_INTERNAL_MESSAGES_NOT_OVERFLOWED(DEVICE_TEST_SERVICE_INTERNAL_MESSAGE_END)


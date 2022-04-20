/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    device_test_service
\brief      Local functions related to authentication used in the Device Test Service.
*/
/*! \addtogroup device_test_service
 @{
*/
#include <stream.h>
#include <task_list.h>


/*! Check whether commands are allowed

    The function determines if AT commands are allowed at present.
    This checks whether the device test service is active and 
    authenticated.

    \return TRUE if commands are allowed, FALSE otherwise
 */
bool DeviceTestService_CommandsAllowed(void);

/*! @} End of group documentation */

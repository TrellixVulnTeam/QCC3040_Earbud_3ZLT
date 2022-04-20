/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Definition file for use by earbud test. Contains functions not of general
            use.
*/

#ifndef __DEVICE_TEST_SERVICE_TEST_H__
#define __DEVICE_TEST_SERVICE_TEST_H__

#include <device_test_service_data.h>
#include <device.h>

#ifdef INCLUDE_DEVICE_TEST_SERVICE

/*! Find out how many devices have been used by the device test
   service.

   The device test service tracks devices that it is responsible
   for creating or using. This function allows an application to
   discover how many, and if requested, which devices.

    \param[out] devices Address of a pointer than can be populated
                with an array of devices. NULL is permitted.
                If the pointer is populated after the call then 
                the application is responsible for freeing the 
                memory

    \return The number of devices found. 0 is possible.
 */
unsigned deviceTestService_GetDtsDevices(device_t **devices);

#else /* !INCLUDE_DEVICE_TEST_SERVICE */

#define deviceTestService_GetDtsDevices(_devs) (UNUSED(_devs),0)

#endif /* INCLUDE_DEVICE_TEST_SERVICE */

#endif /* __DEVICE_TEST_SERVICE_TEST_H__ */


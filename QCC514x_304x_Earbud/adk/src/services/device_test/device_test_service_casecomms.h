/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    device_test_service
\brief      Interface to device test service case comms client.
*/

#ifndef __DEVICE_TEST_SERVICE_CASECOMMS_H__
#define __DEVICE_TEST_SERVICE_CASECOMMS_H__

#ifdef INCLUDE_DEVICE_TEST_SERVICE
#ifdef INCLUDE_CASE_COMMS

/*! \brief Initialise DTS case comms client.
    
    Registers DTS with cc_protocol to use case comms.

    Any time after this call the registered DTS case comms client callbacks
    may be called for incoming case comms messages on the DTS channel or to
    report status of messages transmitted on the DTS channel by this client.
*/
void DeviceTestServiceCasecomms_Init(void);

#else

#define DeviceTestServiceCasecomms_Init()

#endif /* INCLUDE_CASE_COMMS */
#endif /* INCLUDE_DEVICE_TEST_SERVICE */
#endif /* __DEVICE_TEST_SERVICE_CASECOMMS_H__ */

/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       
\ingroup    adk_test_common
\brief      Interface to case comms testing functions

This component provides an API for test scripts to exercise the case comms protocol
functionality provided by the cc_protocol component in the case_comms domain.

Scripts must call CaseCommsTest_Init() before using any other API, this registers this
component as the user of the CASECOMMS_CID_TEST case comms channel.

After completion of tests using this API the DUT should be rebooted, no provision
is made to free allocated memory or unregister the test channel with case comms.

To transmit a message:
    - Call CaseCommsTest_TxMsg()
        - Check return value that message has been accepted for transmission
    - Poll CaseCommsTest_PollTxStatus() to check status of transmitted message
        - will initially return CASECOMMS_STATUS_UNKNOWN which should then change
          to a status indicating result of the transmission, for example
          CASECOMMS_TX_SUCCESS where message acknowledged. However, the test script
          could setup a failure scenario and test for an expected CASECOMMS_TX_FAIL
          status.

To receive a message...
    - Poll CaseCommsTest_PollRxMsgLen()
        - will initially return 0, but will return non-zero value when message is
          received
    - Allocate memory of size returned by CaseCommsTest_PollRxMsgLen()
    - Call CaseCommsTest_RxMsg() passing pointer to allocated memory to retrieve
      the received message.

Up to 8 queued messages can be handled by this test interface, this can be increased
by changing the NUM_QUEUED_TRANSACTIONS definition.

For example CaseCommsTest_TxMsg() can be called 8 times before starting to poll
for transmit status with CaseCommsTest_PollTxStatus() to support testing of queued
messaging.

Similarly, up to 8 messages can be received and buffered (within the limits of 
available heap memory) by a destination device.

Note that receipt of a message when all slots of the receive buffer are full will
result in the incoming message being discarded.
*/

/*! @{ */

#ifndef CASE_COMMS_TEST_H
#define CASE_COMMS_TEST_H

#ifdef INCLUDE_CASE_COMMS

#include <cc_protocol.h>

/*! \brief Initialise the case comms test API before use.
    \param max_msg_len Maximum size of message the test API should support.

    Registers case comms test APIs with case comms domain to use the test channel,
    and allocates memory for handlings test messages.

    \note Must be called before another other API in this interface can be used.
*/
void CaseCommsTest_Init(unsigned max_msg_len);

/*! \brief Transmit a case comms message.
    \param dest Destination device to transmit message to.
    \param mid Message ID of test message.
    \param msg Pointer to memory containing message to transmit.
    \param len Length of message
    \param expect_response TRUE if message expects a response, otherwise FALSE
    \return bool TRUE if message accepted for transmission to dest, otherwise FALSE

    \note expect_response is only valid on device in case mode. This will cause the
          case comms protocol to poll the destination for a response where it is still
          outstanding.

    \note When scripts call this API, the transmit status is reset to CASECOMMS_STATUS_UNKNOWN,
          scripts can then poll CaseCommsTest_GetTxStatus() to check for an expected
          CASECOMMS_TX_SUCCESS (or other) status, indicating the message was transmitted and
          acknowledged by the destination device.
*/
bool CaseCommsTest_TxMsg(cc_dev_t dest, unsigned mid, uint8* msg, uint16 len, bool expect_response);

/*! \brief Receive message transmitted to this device.
    \param msg Pointer to memory location in which the received message will be written.
    \return uint16 Size of message written to msg.

    \note After calling this function, the recorded length of the received message is reset to
          zero, so scripts can again call CaseCommsTest_GetMsgLen() to check for further
          received messages.
*/
uint16 CaseCommsTest_RxMsg(uint8* msg);

/*! \brief Get length of message received by this device.
    \return uint16 Size of received message in bytes.

    Scripts can poll this function for a non-zero value, which indicates a message
    has been received and the length of that message. The returned value indicates
    how much memory must be allocated in order to pass the pointer to that memory
    to CaseCommsTest_GetRxMsg() in order to read the received message.
*/
uint16 CaseCommsTest_PollRxMsgLen(void);

/*! \brief Get the transmit status of the last transmitted message.
    \return cc_tx_status_t CASECOMMS_TX_SUCCESS last message was transmitted and acknowledged
                           by the destination device.
                           CASECOMMS_TX_FAIL last message was transmitted but not acknowledged.
*/
cc_tx_status_t CaseCommsTest_PollTxStatus(void);

#endif /* INCLUDE_CASE_COMMS */
#endif /* CASE_COMMS_TEST_H */

/*! @} */


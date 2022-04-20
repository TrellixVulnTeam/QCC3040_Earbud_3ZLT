/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    case_domain
\brief      Case channel interface.
*/
/*! \addtogroup case_domain
@{
*/

#ifdef INCLUDE_CASE

/*! \brief Handler for incoming case channel message.
    \param mid Message ID.
    \param msg Message payload.
    \param length Length of the message payload.
*/
void Case_CommsCaseChannelHandleMessage(unsigned mid, uint8* msg, unsigned length);

/*! \brief Handle status for a transmitted case channel message.
    \param status CASECOMMS_TX_SUCCESS message successfully received by the case.
                  CASECOMMS_TX_FAIL message not successfully received by the case.
*/
void Case_CommsCaseChannelHandleTransmitStatus(case_comms_tx_status_t status);

#endif /* INCLUDE_CASE */
/*! @} End of group documentation */

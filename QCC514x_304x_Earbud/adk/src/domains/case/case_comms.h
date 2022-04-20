/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    case_domain
\brief      Interface to communication with the case using charger comms traps.
*/
/*! \addtogroup case_domain
@{
*/

#include "case.h"
#include "case_private.h"

#include <message.h>

#ifdef INCLUDE_CASE

/*! \brief Channel IDs used by components to communicate over Case Comms. 
    \note These values are used in the protocol with the case
          and must remain in sync with case software.
*/
typedef enum
{
    /*! Status information from the case. */
    CASECOMMS_CID_CASE = 0x0,

    CASECOMMS_CID_INVALID = 0xF
} case_comms_cid_t;

/*! Result of call to #Case_CommsTransmit().
*/
typedef enum
{
    /*! Message successfully received by destination. */
    CASECOMMS_TX_SUCCESS,

    /*! Message transmit failed, destination did not receive. */
    CASECOMMS_TX_FAIL
} case_comms_tx_status_t;

/*! \brief Devices participating in the Case Comms network.
    \note These values are used in the protocol with the case
          and must remain in sync with case software.
 */
typedef enum
{
    /*! Case. */
    CASECOMMS_DEVICE_CASE      = 0x0,

    /*! Right Earbud. */
    CASECOMMS_DEVICE_RIGHT_EB  = 0x1,

    /*! Left Earbud. */
    CASECOMMS_DEVICE_LEFT_EB   = 0x2,

    /*! Broadcast to both Left and Right Earbud. */
    CASECOMMS_DEVICE_BROADCAST = 0x3,
} case_comms_device_t;

/*! \brief Initialise comms with the case.
*/
void Case_CommsInit(void);

/*! \brief Handle incoming message from the case.
*/
void Case_CommsHandleMessageChargerCommsInd(const MessageChargerCommsInd* ind);

/*! \brief Handle status message regarding comms with the case.
*/
void Case_CommsHandleMessageChargerCommsStatus(const MessageChargerCommsStatus* status);

/*! \brief Transmit a message over case comms.
*/
bool Case_CommsTransmit(case_comms_device_t dest, case_comms_cid_t cid, unsigned mid, 
                        uint8* data, uint16 len);

#endif /* INCLUDE_CASE */
/*! @} End of group documentation */

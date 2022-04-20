/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of RSSI Pairing component.
            This component can be used in order to pair to a device using its RSSI value.
            The component will use the Inquiry Manager to start an inquiry scan and get
            returned results.
            If there are more than one result then the result with the highest RSSI value will be chosen
            so long as its RSSI is sufficiently higher then the next result and above the configured RSSI
            Threshold
            If there is only one returned device then that will be chosen if it's RSSI is above the threshhold

            For the chosen device RSSI Pairing will first create an ACL connection and then use the
            Pairing component in order to perform pairing. Once this is completed successfully
            a message will be sent to the client task.


            An set of Inquiry parameters must be defined in the application and the chosen index shall
            be passed to the RSSI pairing module in the call to RssiPairing_Start()
*/

#ifndef RSSI_PAIRING_H_
#define RSSI_PAIRING_H_

#include <inquiry_manager.h>
#include <domain_message.h>

/*! RSSI Pairing external messages. */
enum rssi_pairing_messages
{
    /*! Confirm pairing is complete */
    RSSI_PAIRING_PAIR_CFM = RSSI_PAIRING_MESSAGE_BASE,

    /*! This must be the final message */
    RSSI_PAIRING_MESSAGE_END
};

/*! \brief Definition of the #RSSI_PAIRING_PAIR_CFM_T message content */
typedef struct
{

    /*! The device address that was paired */
    bdaddr              bd_addr;

    /*! Status if the pairing was a success. */
    bool status;

} RSSI_PAIRING_PAIR_CFM_T;

/*! Rssi Pairing parameters*/
typedef struct
{
    /*! The minimum gap between the first and second candidate.
        i.e. There must be this much of a gap in the RSSI values in order for Pairing to happen*/
    uint16 rssi_gap;

    /*! The minimum threshold that a device must be over for it to be chosen as a candidate */
    int16 rssi_threshold;

    /*! The index of the inquiry set defined in the application that should be used */
    uint16 inquiry_filter;

    /*! Total number of inquiries. If 0, no inquiries will be performed */
    uint16 inquiry_count;

} rssi_pairing_parameters_t;

/*! \brief Initialise the RSSI Pairing Component.

    \param init_task Not used
    \return TRUE
*/
bool RssiPairing_Init(Task init_task);

/*! \brief Initialise the RSSI Pairing Module.

    \param client_task The task that shall receive the pairing confirmation.
    \param scan_parameters Pointer to the scan parameters to start the RSSI Pairing process with.

    \return TRUE if RSSI Pairing was successfully started.
*/
bool RssiPairing_Start(Task client_task, rssi_pairing_parameters_t *scan_parameters);

/*! \brief Stop RSSI Pairing Immediately.
           This will return a RSSI_PAIRING_PAIR_CFM message to the client task with status FALSE
*/
void RssiPairing_Stop(void);

/*! \brief Check if the RSSI Pairing module is active.
           i.e. if it is inquiry scanning or attempting to pair

    \return TRUE if pairing module is active.
*/
bool RssiPairing_IsActive(void);

#endif /* RSSI_PAIRING_H_ */

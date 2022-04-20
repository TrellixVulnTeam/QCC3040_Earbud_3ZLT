/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Component managing BT Inquiry
            An application must register a set of parameters when the application starts.
            An index of the desired parameters can then be selected when starting an inquiry scan.
            The inquiry manager will continue to scan with the defined timeout for
            the number of defined repeats until it finds atleast one candidate device.

            All clients (application or other components) can register to receive results using
            InquiryManager_ClientRegister()

            The Inquiry Manager does not keep a record of the devices it finds and it is down to the
            registered clients to keep track.

*/

#ifndef INQUIRY_MANAGER_H_
#define INQUIRY_MANAGER_H_

#include <domain_message.h>
#include <connection.h>

/*! Inquiry parameters structure */
typedef struct
{
    /*! Maximum number of responses in a single inquiry iteration */
    uint8 max_responses;

    /*! The inquiry timeout for a single iteration */
    uint8 timeout;

    /*! The class of device filter for the inquiry manager.
        if a device does not match this class of device then
        it will not be returned as a result */
    uint32 class_of_device;


} inquiry_manager_scan_parameters_t;


/*! Inquiry Manager external messages. */
enum inquiry_manager_messages
{
    INQUIRY_MANAGER_RESULT = INQUIRY_MANAGER_MESSAGE_BASE,

    INQUIRY_MANAGER_SCAN_STARTED,

    INQUIRY_MANAGER_SCAN_COMPLETE,

    /*! This must be the final message */
    INQUIRY_MANAGER_MESSAGE_END
};

/*! \brief Definition of the #INQUIRY_MANAGER_RESULT_T message content */
typedef struct
{
    /*! BT address of the discovered device. */
    bdaddr              bd_addr;
    /*! Class of device of the discovered device. */
    uint32              dev_class;
    /*! Clock offset of the discovered device. */
    uint16              clock_offset;
    /*! Page scan repetition mode of the discovered device. */
    page_scan_rep_mode  page_scan_rep_mode;
    /*! Page scan mode of the discovered device. */
    page_scan_mode      page_scan_mode;
    /*! RSSI of the discovered device.  Set to CL_RSSI_UNKNOWN if value not
     available. */
    int16               rssi;

} INQUIRY_MANAGER_RESULT_T;

/*! \brief Initialise the Inquiry Manager component.

    \param init_task Not used
    \return TRUE
*/
bool InquiryManager_Init(Task init_task);

/*! \brief Register a client task to receive results from the Inquiry Manager

    \param client_task task to receive messages.
    \return TRUE if registration was successful.
*/
bool InquiryManager_ClientRegister(Task client_task);

/*! \brief Unregister a client task to receive results from the Inquiry Manager

    \param client_task task to unregister.
    \return TRUE if registration was removed.
*/
bool InquiryManager_ClientUnregister(Task client_task);

/*! \brief Register a collection of parameters. These are usually defined in the application
           and passed in during initialisation.

    \param params pointer to parameter collection.
    \param params number of parameters in collection.
*/
void InquiryManager_RegisterParameters(const inquiry_manager_scan_parameters_t *params, uint16 set_length);

/*! \brief Begin inquiry scanning with a the selected parameters. The results will be sent to the
           registered clients.

    \param filter_id The index of the selected parameters.
    \return TRUE if the inquiry scanning could start.
*/
bool InquiryManager_Start(uint16 filter_id);

/*! \brief Immediately request that the inquiry scanning stops.
           An INQUIRY_MANAGER_SCAN_COMPLETE will still be sent
*/
void InquiryManager_Stop(void);

/*! \brief Returns if the Inquiry Manager is active
    \return TRUE if the Inquiry Manager is actively scanning.
*/
bool InquiryManager_IsInquiryActive(void);

#endif /* INQUIRY_MANAGER_H_ */

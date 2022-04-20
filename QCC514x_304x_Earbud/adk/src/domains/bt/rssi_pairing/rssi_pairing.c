/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Component managing pairing based on RSSI strength
*/

#include "rssi_pairing.h"

#include <logging.h>
#include <inquiry_manager.h>
#include <unexpected_message.h>
#include <pairing.h>
#include <connection_manager.h>
#include <panic.h>

#define NUMBER_OF_INQUIRY_RESULTS 2

/*! RSSI Pairing state */
typedef enum
{
    RSSI_PAIRING_STATE_IDLE,
    RSSI_PAIRING_STATE_INQUIRY,
    RSSI_PAIRING_STATE_ACL_CONNECTING,
    RSSI_PAIRING_STATE_PAIRING,
} rssi_pairing_state_t;

/*! Structure for a RSSI Pairing device candidate */
typedef struct
{
    /*! Bluetooth address of the candidate */
    bdaddr              bd_addr;

    /*! RSSI Value of the candidate */
    int16               rssi;
} rssi_paring_device_candidate_t;

/*! RSSI Pairing data */
typedef struct
{
    /*! Init's local task */
    TaskData task;

    /*! The selected minimum gap between the first and second candidate.
        i.e. There must be this much of a gap in the RSSI values in order for Pairing to happen*/
    uint16 scan_rssi_gap;

    /*! The selected minimum threshold
        that a device must be over for it to be chosen as a candidate */
    int16 scan_rssi_threshold;

    /*! The number of iterations left in the inquiry scan */
    uint16 inquiry_count;

    /*! The filter to use for the scan */
    uint16 inquiry_filter_index;

    /*! The List of candidates */
    rssi_paring_device_candidate_t inquiry_results[NUMBER_OF_INQUIRY_RESULTS];

    /*! RSSI Pairing state */
    rssi_pairing_state_t state;

    /*! The task to receive RSSI Pairing messages */
    Task client_task;

} rssi_pairing_data_t;

rssi_pairing_data_t rssi_pairing_data;

/*! Get pointer to RSSI Pairing task*/
#define RssiPairing_GetTask() (&rssi_pairing_data.task)

/*! Get pointer to RSSI Pairing data structure */
#define RssiPairing_GetTaskData() (&rssi_pairing_data)

/*! \brief Reset the candidate list*/
static void rssiPairing_ResetDevices(void)
{
    BdaddrSetZero(&rssi_pairing_data.inquiry_results[0].bd_addr);
    BdaddrSetZero(&rssi_pairing_data.inquiry_results[1].bd_addr);
    rssi_pairing_data.inquiry_results[0].rssi = 0;
    rssi_pairing_data.inquiry_results[1].rssi = 0;
}

/*! \brief Rest the RSSI Pairing manager*/
static void rssiPairing_ResetManager(void)
{
    rssi_pairing_data.client_task = NULL;
    rssi_pairing_data.scan_rssi_gap = 0;
    rssi_pairing_data.scan_rssi_threshold = 0;
    rssi_pairing_data.inquiry_count = 0;
    rssi_pairing_data.inquiry_filter_index = 0;
    rssi_pairing_data.state = RSSI_PAIRING_STATE_IDLE;
    rssiPairing_ResetDevices();
}
/*! \brief Handler for connection library INQUIRY_MANAGER_RESULT message.
           If the result RSSI is not above the threshold then it is discarded.
           If the candidate list is empty then it is added.
           If the RSSI value is not greater than the top 2 results it is discarded
           An incoming result will remove a previous candidate if its RSSI is greater.

    \param result the Inquiry Manager result*/
static void rssiPairing_HandleInquireManagerResult(const INQUIRY_MANAGER_RESULT_T *result)
{
    DEBUG_LOG_VERBOSE("rssiPairing_HandleInquireManagerResult");
    DEBUG_LOG_VERBOSE("RssiPairing: Inquiry Result:");
    DEBUG_LOG_VERBOSE("     bdaddr 0x%04x 0x%02x 0x%06lx", result->bd_addr.nap,
                                                           result->bd_addr.uap,
                                                           result->bd_addr.lap);
    DEBUG_LOG_VERBOSE("     rssi %d", result->rssi);

    /* if the rssi result is less than the set threshold discard the result. */
    if (result->rssi < rssi_pairing_data.scan_rssi_threshold)
    {
        return;
    }
    if (BdaddrIsZero(&rssi_pairing_data.inquiry_results[0].bd_addr) ||
        result->rssi > rssi_pairing_data.inquiry_results[0].rssi)
    {
        DEBUG_LOG_VERBOSE("RSSI Pairing: Highest RSSI:, bdaddr 0x%04x 0x%02x 0x%06lx rssi %d cod %lx",
                  result->bd_addr.nap,
                  result->bd_addr.uap,
                  result->bd_addr.lap,
                  result->rssi,
                  result->dev_class);

        /* Check if address is different from previous peak */
        if (!BdaddrIsSame(&result->bd_addr, &rssi_pairing_data.inquiry_results[0].bd_addr))
        {
            /* Store previous peak RSSI */
            rssi_pairing_data.inquiry_results[1] = rssi_pairing_data.inquiry_results[0];

            /* Store new address */
            rssi_pairing_data.inquiry_results[0].bd_addr = result->bd_addr;
            rssi_pairing_data.inquiry_results[0].rssi = result->rssi;

        }
    }
    else if (BdaddrIsZero(&rssi_pairing_data.inquiry_results[1].bd_addr)||
             result->rssi > rssi_pairing_data.inquiry_results[1].rssi)
    {
        /* Check if address is different from peak */
        if (!BdaddrIsSame(&result->bd_addr, &rssi_pairing_data.inquiry_results[0].bd_addr))
        {
            /* Store next highest RSSI */
            rssi_pairing_data.inquiry_results[1].bd_addr = result->bd_addr;
            rssi_pairing_data.inquiry_results[1].rssi = result->rssi;
        }
    }
}

/*! \brief Handler for the INQUIRY_MANAGER_SCAN_COMPLETE message
           If there is atleast one candidate in the list then RSSI pairing will first attempt to create
           an ACL with that device

           If there is more than one result then it will try to connect to the highest RSSI assuming that it is
           sufficiently higher than the next result (peak detection)

           If there are not candidates or the Scan was stopped using RssiPairing_Stop() then
           a RSSI_PAIRING_PAIR_CFM message will be sent with status FALSE */
static void rssiPairing_HandleInquireManagerScanComplete(void)
{
    DEBUG_LOG_FN_ENTRY("rssiPairing_HandleInquireManagerScanComplete");

    DEBUG_LOG_VERBOSE("RSSI Pairing: Inquiry Complete: bdaddr %x,%x,%lx rssi %d, next_rssi %d",
              rssi_pairing_data.inquiry_results[0].bd_addr.nap,
              rssi_pairing_data.inquiry_results[0].bd_addr.uap,
              rssi_pairing_data.inquiry_results[0].bd_addr.lap,
              rssi_pairing_data.inquiry_results[0].rssi,
              rssi_pairing_data.inquiry_results[1].rssi);

    /* RSSI Pairing will be set to idle if RSSI Pairing was Stopped using RssiPairing_Stop() */
    if (rssi_pairing_data.state != RSSI_PAIRING_STATE_IDLE)
    {
        /* Attempt to connect to device with highest RSSI */
        if (!BdaddrIsZero(&rssi_pairing_data.inquiry_results[0].bd_addr))
        {
            /* Check if RSSI peak is sufficently higher than next */
            if (BdaddrIsZero(&rssi_pairing_data.inquiry_results[1].bd_addr) ||
                (rssi_pairing_data.inquiry_results[0].rssi - rssi_pairing_data.inquiry_results[1].rssi) >= rssi_pairing_data.scan_rssi_gap)
            {
                DEBUG_LOG_VERBOSE("RSSI Pairing: Pairing with Highest RSSI: bdaddr 0x%04x 0x%02x 0x%06lx",
                               rssi_pairing_data.inquiry_results[0].bd_addr.nap,
                               rssi_pairing_data.inquiry_results[0].bd_addr.uap,
                               rssi_pairing_data.inquiry_results[0].bd_addr.lap);

                /* Create an ACL with the device before pairing */
                ConManagerCreateAcl(&rssi_pairing_data.inquiry_results[0].bd_addr);

                rssi_pairing_data.state = RSSI_PAIRING_STATE_ACL_CONNECTING;
                return;
            }
        }
        /* No viable RSSI candidate has been found. Start Inquiry manager again
         * and decrement repeat count if not zero */
        if (rssi_pairing_data.inquiry_count != 0)
        {
            rssi_pairing_data.inquiry_count--;
            DEBUG_LOG_DEBUG("rssiPairing_HandleInquireManagerScanComplete: No Candidate Found. Scanning again inquiry_count:%d", rssi_pairing_data.inquiry_count);

            rssiPairing_ResetDevices();
            InquiryManager_Start(rssi_pairing_data.inquiry_filter_index);
            return;
        }
    }

    /* No RSSI candidate found. Send Failure */
    RSSI_PAIRING_PAIR_CFM_T* confirm_message = PanicUnlessNew(RSSI_PAIRING_PAIR_CFM_T);
    BdaddrSetZero(&confirm_message->bd_addr);
    confirm_message->status = FALSE;
    MessageSend(rssi_pairing_data.client_task, RSSI_PAIRING_PAIR_CFM, confirm_message);

    rssiPairing_ResetManager();

}

/*! \brief Handler for the CON_MANAGER_CONNECTION_IND message
           Pair to a device using Pairing_PairAddress if an ACL connection was successful

    \param msg The Connection Manager Indication
*/
static void rssiPairing_HandleConManagerConnectionInd(const CON_MANAGER_CONNECTION_IND_T *msg)
{
    DEBUG_LOG_FN_ENTRY("rssiPairing_HandleConManagerConnectionInd");

    if (rssi_pairing_data.state == RSSI_PAIRING_STATE_ACL_CONNECTING
            && BdaddrIsSame(&msg->bd_addr, &rssi_pairing_data.inquiry_results[0].bd_addr))
    {
        /* If the ACL was successfully created to the candidate device then the pairing module will be used
           to pair with the device*/
        Pairing_PairAddress(RssiPairing_GetTask(), &rssi_pairing_data.inquiry_results[0].bd_addr);
        rssi_pairing_data.state = RSSI_PAIRING_STATE_PAIRING;
    }

}

/*! \brief Handler for the PAIRING_PAIR_CFM message
           If the pairing was successful, a RSSI_PAIRING_PAIR_CFM is sent to the client task.

    \param message the Pairing Manager Pair confirmation
*/
static void rssiPairing_HandlePairingConfirm(const PAIRING_PAIR_CFM_T * message)
{

    DEBUG_LOG_FN_ENTRY("rssiPairing_HandlePairingConfirm status: enum:pairingStatus:%d", message->status);

    switch (message->status){
        case pairingSuccess:
        {
            DEBUG_LOG_VERBOSE("RSSI Pairing: Pairing Successful, bdaddr 0x%04x 0x%02x 0x%06lx",
                           message->device_bd_addr.nap,
                           message->device_bd_addr.uap,
                           message->device_bd_addr.lap);

            RSSI_PAIRING_PAIR_CFM_T* confirm_message = PanicUnlessNew(RSSI_PAIRING_PAIR_CFM_T);
            confirm_message->bd_addr = message->device_bd_addr;
            confirm_message->status = TRUE;
            MessageSend(rssi_pairing_data.client_task, RSSI_PAIRING_PAIR_CFM, confirm_message);

        }
        break;
        default:
        {
            RSSI_PAIRING_PAIR_CFM_T* confirm_message = PanicUnlessNew(RSSI_PAIRING_PAIR_CFM_T);
            confirm_message->bd_addr = message->device_bd_addr;
            confirm_message->status = FALSE;
            MessageSend(rssi_pairing_data.client_task, RSSI_PAIRING_PAIR_CFM, confirm_message);
        }
        break;
    }

    /* Release the connection ACL ownership */
    ConManagerReleaseAcl(&message->device_bd_addr);

    rssiPairing_ResetManager();

}

/*! \brief Handler for PAIRING_STOP_CFM message */
static void rssiPairing_HandlePairingStopped(const PAIRING_STOP_CFM_T *message) {
    DEBUG_LOG_FN_ENTRY("rssiPairing_HandlePairingStopped: status: enum:pairingStatus:%d",message->status);
}

/*! \brief Handler for component messages */
static void rssiPairing_HandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);
    DEBUG_LOG_FN_ENTRY("rssiPairing_HandleMessage");

    switch (id)
    {
        case CON_MANAGER_CONNECTION_IND:
            rssiPairing_HandleConManagerConnectionInd((CON_MANAGER_CONNECTION_IND_T *)message);
        break;

        case INQUIRY_MANAGER_RESULT:
            rssiPairing_HandleInquireManagerResult((INQUIRY_MANAGER_RESULT_T *)message);
        break;

        case INQUIRY_MANAGER_SCAN_COMPLETE:
            rssiPairing_HandleInquireManagerScanComplete();
        break;

        case PAIRING_PAIR_CFM:
            rssiPairing_HandlePairingConfirm((PAIRING_PAIR_CFM_T *)message);
        break;

        case PAIRING_STOP_CFM:
            rssiPairing_HandlePairingStopped((PAIRING_STOP_CFM_T *)message);
        break;

        default:
            UnexpectedMessage_HandleMessage(id);
        break;
    }
}

bool RssiPairing_Init(Task init_task)
{
    UNUSED(init_task);
    DEBUG_LOG_FN_ENTRY("InquiryManager_Init");

    rssi_pairing_data.task.handler = rssiPairing_HandleMessage;
    rssi_pairing_data.scan_rssi_gap = 0;
    rssi_pairing_data.scan_rssi_threshold = 0;
    rssi_pairing_data.state = RSSI_PAIRING_STATE_IDLE;

    rssiPairing_ResetDevices();
    InquiryManager_ClientRegister(RssiPairing_GetTask());
    ConManagerRegisterConnectionsClient(RssiPairing_GetTask());

    return TRUE;
}

bool RssiPairing_Start(Task client_task, rssi_pairing_parameters_t *scan_parameters)
{
    DEBUG_LOG_FN_ENTRY("RssiPairing_Start");

    if (scan_parameters == NULL)
    {
        return FALSE;
    }
    if (rssi_pairing_data.state != RSSI_PAIRING_STATE_IDLE)
    {
        DEBUG_LOG_DEBUG("RssiPairing_Start: Cannot Start. Pairing already in progress");
        return FALSE;
    }
    rssiPairing_ResetDevices();

    rssi_pairing_data.client_task = client_task;
    rssi_pairing_data.scan_rssi_gap = scan_parameters->rssi_gap;
    rssi_pairing_data.scan_rssi_threshold = scan_parameters->rssi_threshold;
    rssi_pairing_data.inquiry_count = scan_parameters->inquiry_count;
    rssi_pairing_data.inquiry_filter_index = scan_parameters->inquiry_filter;

	/* start the first inquiry scan */
    if (rssi_pairing_data.inquiry_count > 0)
    {
        rssi_pairing_data.inquiry_count--;
        if (InquiryManager_Start(scan_parameters->inquiry_filter))
        {
            rssi_pairing_data.state = RSSI_PAIRING_STATE_INQUIRY;
            return TRUE;
        }
        else
        {
            return FALSE;
        }
    }
    else
    {
        DEBUG_LOG_ERROR("RssiPairing_Start: Cannot Start. inquiry_count=0");
        return FALSE;
    }

}

void RssiPairing_Stop(void)
{
    rssiPairing_ResetManager();
    InquiryManager_Stop();
}

bool RssiPairing_IsActive(void)
{
    return (rssi_pairing_data.state > RSSI_PAIRING_STATE_IDLE);
}

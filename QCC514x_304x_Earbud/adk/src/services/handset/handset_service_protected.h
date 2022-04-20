/*!
\copyright  Copyright (c) 2019 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Handset service types to be used within handset_service only
*/

#ifndef HANDSET_SERVICE_PROTECTED_H_
#define HANDSET_SERVICE_PROTECTED_H_

#include <bdaddr.h>
#include <logging.h>
#include <panic.h>
#include <task_list.h>
#include <le_advertising_manager.h>

#include "handset_service.h"
#include "handset_service_sm.h"
#include "handset_service_multipoint_sm.h"
#include "handset_service_config.h"

/*! \{
    Macros for diagnostic output that can be suppressed.
*/
#define HS_LOG         DEBUG_LOG
/*! \} */

/*! Code assertion that can be checked at run time. This will cause a panic. */
#define assert(x) PanicFalse(x)

/*! Client task list initial list */
#define HANDSET_SERVICE_CLIENT_LIST_INIT_CAPACITY 1

#define HANDSET_SERVICE_DISCONNECT_ALL_CLIENT_LIST_INIT_CAPACITY 1

#define HANDSET_SERVICE_MAX_SM      4

/*! \brief Data type to specify the state of LE advertising data set select/release operation */
typedef enum
{
    handset_service_le_adv_data_set_state_not_selected = 0,
    handset_service_le_adv_data_set_state_selected,
    handset_service_le_adv_data_set_state_selecting,
    handset_service_le_adv_data_set_state_releasing,

} handset_service_le_adv_data_set_state_t;

/*! \brief The global data for the handset_service */
typedef struct
{
    /*! Handset Service task */
    TaskData task_data;

    /* Handset Service state machine */
    handset_service_state_machine_t state_machine[HANDSET_SERVICE_MAX_SM];

    /* Handset Service Multipoint state machine */
    handset_service_multipoint_state_machine_t mp_state_machine;

    /*! Client lists for notifications */
    TASK_LIST_WITH_INITIAL_CAPACITY(HANDSET_SERVICE_CLIENT_LIST_INIT_CAPACITY) client_list;
    TASK_LIST_WITH_INITIAL_CAPACITY(HANDSET_SERVICE_DISCONNECT_ALL_CLIENT_LIST_INIT_CAPACITY) disconnect_all_list;

    /* Flag to store if handset can be paired */
    bool pairing;
    
    /* Flag to indicate whether the device is BLE connectable */
    bool ble_connectable;
    
    /* State of LE advertising data set select/release */
    handset_service_le_adv_data_set_state_t ble_adv_state;

    /* Handle for LE advertising data set */
    le_adv_data_set_handle le_advert_handle;
    
    /* Selected LE advertising data set */
    le_adv_data_set_t le_advert_data_set;

    bool disconnect_all_in_progress;

} handset_service_data_t;

/*! \brief Internal messages for the handset_service */
typedef enum
{
    /*! Request to connect to a handset */
    HANDSET_SERVICE_INTERNAL_CONNECT_REQ = INTERNAL_MESSAGE_BASE,

    /*! Request to disconnect a handset */
    HANDSET_SERVICE_INTERNAL_DISCONNECT_REQ,

    /*! Delivered when an ACL connect request has completed. */
    HANDSET_SERVICE_INTERNAL_CONNECT_ACL_COMPLETE,

    /*! Request to cancel any in-progress connect to handset. */
    HANDSET_SERVICE_INTERNAL_CONNECT_STOP_REQ,

    /*! Request to re-try the ACL connection after a failure. */
    HANDSET_SERVICE_INTERNAL_CONNECT_ACL_RETRY_REQ,

    /*! Timeout message to clear the possible pairing flag for an SM */
    HANDSET_SERVICE_INTERNAL_POSSIBLE_PAIRING_TIMEOUT,

    /*! Request to connect profiles */
    HANDSET_SERVICE_INTERNAL_CONNECT_PROFILES_REQ,

    /*! This must be the final message */
    HANDSET_SERVICE_INTERNAL_MESSAGE_END
} handset_service_internal_msg_t;
ASSERT_INTERNAL_MESSAGES_NOT_OVERFLOWED(HANDSET_SERVICE_INTERNAL_MESSAGE_END)

typedef struct
{
    /*! Handset device to connect. */
    device_t device;

    /*! Mask of profile(s) to connect. */
    uint32 profiles;
} HANDSET_SERVICE_INTERNAL_CONNECT_REQ_T;

typedef struct
{
    /*! Address of handset device to disconnect. */
    bdaddr addr;
    /*! Any profiles that are to be excluded from the disconnection. */
    uint32 exclude;
} HANDSET_SERVICE_INTERNAL_DISCONNECT_REQ_T;

typedef struct
{
    /*! Handset device to stop connect for */
    device_t device;
} HANDSET_SERVICE_INTERNAL_CONNECT_STOP_REQ_T;

/* \brief Message structure for HANDSET_SERVICE_INTERNAL_POSSIBLE_PAIRING_TIMEOUT 

    The message contains both the device address and the state machine
    affected. This is because the message cannot be cancelled so
    the state machine could have been cleared and used for another device.
*/
typedef struct
{
    /*! Address of device to clear */
    tp_bdaddr                       address;
    /*! The specific state machine */
    handset_service_state_machine_t *sm;
} HANDSET_SERVICE_INTERNAL_POSSIBLE_PAIRING_TIMEOUT_T;


/*! \brief Send a HANDSET_SERVICE_CONNECTED_IND to registered clients.

    \param device Device that represents the handset
    \param profiles_connected Profiles currently connected to this handset.
*/
void HandsetService_SendConnectedIndNotification(device_t device,
    uint32 profiles_connected);

/*! \brief Send a HANDSET_SERVICE_DISCONNECTED_IND to registered clients.

    \param addr Address of the handset
    \param status Status of the connection.
*/
void HandsetService_SendDisconnectedIndNotification(const bdaddr *addr,
    handset_service_status_t status);

/*! \brief Send a HANDSET_SERVICE_FRIST_PROFILE_CONNECTED_IND to registered clients.

    \param device Device that represents the handset
*/
void HandsetService_SendFirstProfileConnectedIndNotification(device_t device);

/*! Handset Service module data. */
extern handset_service_data_t handset_service;

/*! Get pointer to the Handset Service modules data structure */
#define HandsetService_Get() (&handset_service)

/*! Get the Task for the handset_service */
#define HandsetService_GetTask() (&HandsetService_Get()->task_data)

/*! Get the client list for the handset service. */
#define HandsetService_GetClientList() (task_list_flexible_t *)(&HandsetService_Get()->client_list)

/*! \brief Get if the handset service has a BLE connection. 

    \return TRUE if there is an active BLE connection. FALSE otherwise
 */
bool HandsetService_IsBleConnected(void);

/*! Get if the handset service is BLE connectable */
#define HandsetService_IsBleConnectable() (HandsetService_Get()->ble_connectable)

#define HandsetService_GetDisconnectAllClientList() (task_list_flexible_t *)(&HandsetService_Get()->disconnect_all_list)

/*! Get the multipoint state machine which connects multiple handsets. */
#define HandsetService_GetMultipointSm() (HandsetService_Get()->mp_state_machine)

/*! Update advertising data */
bool handsetService_UpdateAdvertisingData(void);

/*! Check if a new handset connection is allowed */
bool handsetService_CheckHandsetCanConnect(const bdaddr *addr);

/*! Retreive the existing or create new handset statemachine for the requested bluetooth transport address */
handset_service_state_machine_t *handsetService_FindOrCreateSm(const tp_bdaddr *tp_addr);

/*! Resolve *tpaddr if necessary and possible. Populates *resolved_tpaddr with either
    the resolved address or a copy of *tpaddr */
void HandsetService_ResolveTpaddr(const tp_bdaddr *tpaddr, tp_bdaddr *resolved_tpaddr);

/*! \brief Function to get an active handset state machine based on a BR/EDR address.

    \param addr Address to search for. Treated as a BR/EDR address.

    \return Pointer to the matching state machine, or NULL if no match.
*/
handset_service_state_machine_t *HandsetService_GetSmForBdAddr(const bdaddr *addr);

#endif /* HANDSET_SERVICE_PROTECTED_H_ */

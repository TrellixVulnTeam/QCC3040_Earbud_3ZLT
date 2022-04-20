/*!
\copyright  Copyright (c) 2019 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   handset_service Handset Service
\ingroup    services
\brief      A service that provides procedures for connecting and managing
            connections to handset(s).

# Overview

This service is mainly concerned with managing the connection to a handset.
That includes both remote (initiated by the handset) and local (initiated by a
call to one of the connect APIs) connections.

## Remote Connections

Remote connections will be detected by this handset service and notified to
any clients that have registered for notifications.

For the handset to initiate a connection the local device must first be
connectable - there are functions in this service to control whether the local
device is connectable or not.

## Local Connections

There are functions to request the connection or disconnection of a handset and
these also include the list of profiles to connect to the handset. This service
will manage the connect or disconnect procedure until it is completed. When it
is complete the client Task that made the request will be sent a matching CFM
message containing the result.

Both connect and disconnect procedures can be cancelled while they are still in
progress by making the opposite request. For example, connect will be cancelled
if a disconnect request is made while it is progress, and vice versa for
disconnect.

## Connection Status

If the connection status changes for any reason, e.g. link-loss or the handset
has initiated a disconnect, it will be notified to any clients that have
registered for notifications.

*/

#ifndef HANDSET_SERVICE_H_
#define HANDSET_SERVICE_H_

#include <message.h>

#include <bdaddr.h>
#include <bt_device.h>

#include "domain_message.h"

/*@{*/

/*! \brief Events sent by handset to other modules. */
typedef enum
{
    /*! Module initialisation complete */
    HANDSET_INIT_CFM = HANDSET_SERVICE_MESSAGE_BASE,

    /*! A handset has initiated a connection. */
    HANDSET_SERVICE_CONNECTED_IND,

    /*! A handset has disconnected. */
    HANDSET_SERVICE_DISCONNECTED_IND,

    /*! Confirmation of completion of a connect request. */
    HANDSET_SERVICE_CONNECT_CFM,

    /*! Confirmation of completion of a disconnect request. */
    HANDSET_SERVICE_DISCONNECT_CFM,

    /*! Confirmation of completion of a cancel connect request. */
    HANDSET_SERVICE_CONNECT_STOP_CFM,

    /*! Confirmation of completion of a ble connectable request. */
    HANDSET_SERVICE_LE_CONNECTABLE_IND,

    /*! Confirmation of completion of reconnect request. */
    HANDSET_SERVICE_MP_CONNECT_CFM,

    /*! Confirmation of completion of cancelling reconnect request. */
    HANDSET_SERVICE_MP_CONNECT_STOP_CFM,

    /*! Confirmation of completion of a disconnect all request. */
    HANDSET_SERVICE_MP_DISCONNECT_ALL_CFM,

    /* Confirmation that very first profile is connected. */
    HANDSET_SERVICE_FIRST_PROFILE_CONNECTED_IND,

    /*! This must be the final message */
    HANDSET_SERVICE_MESSAGE_END
} handset_service_msg_t;

/*! \brief Status codes for the handset service. */
typedef enum
{
    handset_service_status_success,
    handset_service_status_failed,
    handset_service_status_cancelled,
    handset_service_status_no_mru,
    handset_service_status_connected,
    handset_service_status_disconnected,
    handset_service_status_link_loss,
} handset_service_status_t;

/*! \brief Configuration of handset service. */
typedef struct
{
    unsigned max_bredr_connections : 2;
    unsigned acl_connect_attempt_limit : 3;
        /* Number of Bluetooth Low Energy connections allowed.
           Must be greater than 1. If Bluetooth Low Energy is not
           required, changes should be made in Topology or the 
           application. */
    unsigned max_le_connections : 4;
    unsigned unused : 7;
    unsigned page_timeout : 16;
} handset_service_config_t;

/*! Recommended configuration for singlepoint applications */
extern const handset_service_config_t handset_service_singlepoint_config;

/*! Recommended configuration for multipoint applications */
extern const handset_service_config_t handset_service_multipoint_config;

/*! \brief Confirmation for a handset ble connectable request.

    This message will get sent each time for a previously placed
    ble connectable request.
*/
typedef struct
{ 
    /*! Reason for a previously placed ble connectable request */
    handset_service_status_t status;

    /*! Flag to indicate whether the device is LE connectable */
    bool le_connectable;
} HANDSET_SERVICE_LE_CONNECTABLE_IND_T;


/*! \brief Notification that a handset has connected.

    This message will get sent each time one or more profiles connect from
    a handset.

    It is likely the client will receive this message multiple times - up to a
    maximum of one message per profile.
*/
typedef struct
{
    /*! Address of the handset. */
    bdaddr addr;

    /*! Mask of the profile(s) currently connected. */
    uint32 profiles_connected;
} HANDSET_SERVICE_CONNECTED_IND_T;

/*! \brief Notification that first profile with handset is connected.

    It is used to play the "CONNECTED" prompt so don't wait for all the profiles
    to be connected.

    The client will receive this message only when first profile is connected.
*/
typedef struct
{
    /*! Address of the handset. */
    bdaddr addr;
} HANDSET_SERVICE_FIRST_PROFILE_CONNECTED_IND_T;

/*! \brief Notification that a handset has disconnected.

    This message will get sent once a handset has no profiles connected.
*/
typedef struct
{
    /*! Address of the handset. */
    bdaddr addr;

    /*! Reason for the disconnect */
    handset_service_status_t status;
} HANDSET_SERVICE_DISCONNECTED_IND_T;

/*! \brief Confirmation of completed handset connect request

    This message is sent once handset service has completed the connect,
    or it has been cancelled.

    The request could have completed successfully or it could have failed,
    or have been cancelled by a later disconnect request.
*/
typedef struct
{
    /*! Address of the handset. */
    bdaddr addr;

    /*! Status of the request */
    handset_service_status_t status;
} HANDSET_SERVICE_CONNECT_CFM_T;

/*! \brief Confirmation of completion of reconnect request

    This message is sent once handset service multipoint has completed
    the reconnection.
*/
typedef struct
{
    /*! Status of the request */
    handset_service_status_t status;
} HANDSET_SERVICE_MP_CONNECT_CFM_T;

/*! \brief Confirmation of completion of reconnect request

    This message is sent once handset service multipoint has stopped
    the reconnection.
*/
typedef struct
{
    /*! Status of the request */
    handset_service_status_t status;
} HANDSET_SERVICE_MP_CONNECT_STOP_CFM_T;

/*! \brief Confirmation of completed handset disconnect request

    This message is sent once handset service has completed the
    disconnect, or it has been cancelled.

    The request could have completed successfully or it could have  been
    cancelled by a later connect request.
*/
typedef HANDSET_SERVICE_CONNECT_CFM_T HANDSET_SERVICE_DISCONNECT_CFM_T;

/*! \brief Confirmation of completed handset disconnect all request

    This message is sent once handset service has completed the
    disconnect all request, or it has been cancelled.

    The request could have completed successfully or it could have  been
    cancelled by a later connect request.
*/
typedef struct
{
    /*! Status of the request */
    handset_service_status_t status;
} HANDSET_SERVICE_MP_DISCONNECT_ALL_CFM_T;

/*! \brief Confirmation of completed handset cancel connect request

    This message is sent once handset service has cancelled the ACL
    connection to the handset, or otherwise when the connect has completed
    normally.
*/
typedef HANDSET_SERVICE_CONNECT_CFM_T HANDSET_SERVICE_CONNECT_STOP_CFM_T;

/*! \brief Initialise the handset_service module.

    \param task The init task to send HANDSET_SERVICE_INIT_CFM to.

    \return TRUE if initialisation is in progress; FALSE if it failed.
*/
bool HandsetService_Init(Task task);

/*! \brief Register a Task to receive notifications from handset_service.

    Once registered, #client_task will receive #handset_msg_t messages.

    \param client_task Task to register to receive handset_service notifications.
*/
void HandsetService_ClientRegister(Task client_task);

/*! \brief Un-register a Task that is receiving notifications from handset_service.

    If the task is not currently registered then nothing will be changed.

    \param client_task Task to un-register from handet_service notifications.
*/
void HandsetService_ClientUnregister(Task client_task);

/*! \brief Connect to a handset specified by a bdaddr.

    Start the connection procedure for a handset with the given address.

    The connection procedure will be cancelled if
    #HandsetService_DisconnectRequest is called while it is still in progress.

    When the request completes, for whatever reason, the result is sent to
    the client #task in a HANDSET_SERVICE_CONNECT_CFM.

    \param task Task the CFM will be sent to when the request is completed.
    \param addr Address of the handset to connect to.
    \param profiles Profiles to connect.
*/
void HandsetService_ConnectAddressRequest(Task task, const bdaddr *addr, uint32 profiles);

/*! \brief Disconnect a handset with the given bdaddr.

    Start the disconnection procedure for a handset with the given address.

    The disconenction procedure will be cancelled if one of the connect request
    functions are called for the same handset.

    When the request completes, for whatever reason, the result is sent to
    the client #task in a HANDSET_SERVICE_DISCONNECT_CFM.

    \param task Task the CFM will be sent to when the request is completed.
    \param addr Address of the handset to disconnect.
    \param exclude The profiles to be excluded from disconnection.
*/
void HandsetService_DisconnectRequest(Task task, const bdaddr *addr, uint32 exclude);

/*! \brief Disconnect a handset with the given tp_bdaddr, optionally excluding certain profiles.

    Start the disconnection procedure for a handset with the given typed bluetooth address.

    The disconenction procedure will be cancelled if one of the connect request
    functions are called for the same handset.

    When the request completes, for whatever reason, the result is sent to
    the client #task in a HANDSET_SERVICE_DISCONNECT_CFM.

    \param task Task the CFM will be sent to when the request is completed.
    \param tp_addr Typed Address of the handset to disconnect.
    \param exclude The profiles to be excluded from disconnection.
*/
void HandsetService_DisconnectTpAddrRequest(Task task, const tp_bdaddr *tp_addr, uint32 exclude);

/*! \brief Stop the ACL connection to a handset.

    Cancel any in-progress connect to a handset if, and only if, the ACL has
    not been connected yet.

    If the ACL has been connected or the profiles have started to be connected
    then wait until the connection has completed normally.

    When the connection has either been cancelled or has completed, send a 
    #HANDSET_SERVICE_CONNECT_STOP_CFM to the client task.

    If the original connect request completed successfully the status in 
    #HANDSET_SERVICE_CONNECT_STOP_CFM will be handset_service_status_connected;
    otherwise it will be handset_service_status_disconnected.

    If the same client that requested a handset connect calls this function,
    #HANDSET_SERVICE_CONNECT_STOP_CFM will be sent before
    the #HANDSET_SERVICE_CONNECT_CFM for the original connect request.

    Note: Currently only one stop request at a time is supported. If a second
          request is made while one is in progress this function will panic.

    \param task Client task requesting the stop
    \param addr Public address of the handset to stop the connection for.
*/
void HandsetService_StopConnect(Task task, const bdaddr *addr);

/*! \brief Make the local device connectable.

    \param task Task the CFM will be sent to when the request is completed.
*/
void HandsetService_ConnectableRequest(Task task);

/*! \brief Cancel all requests to make the local device connectable.

    \param task Task the CFM will be sent to when the request is completed.
*/
void HandsetService_CancelConnectableRequest(Task task);

/*! \brief Check if the device is connected

    This function is mostly intended for use in tests. Applications should
    be able to request connections or register as clients.

    \param device[in] The device to be checked

    \return TRUE if the device passed is a handset and is completely
        connected. FALSE is returned in all other cased, including connecting
        and disconnecting.
*/
bool HandsetService_Connected(device_t device);

/*! \brief Check if a handset is connected over BR/EDR.

    This function checks if any BR/EDR ACL or profile is connected to the
    given bdaddr.

    \param[in] addr Address of handset to check.

    \return TRUE if BR/EDR ACL or profile(s) are connected, FALSE otherwise.
*/
bool HandsetService_IsBredrConnected(const bdaddr *addr);

/*! \brief Check if handset service has any BR/EDR connections.

    \return TRUE if handset service has any BR/EDR connection, FALSE otherwise.
*/
bool HandsetService_IsAnyBredrConnected(void);

/*! \brief Get the BT address of a handset with an BREDR connection.

    \note If there are multiple handsets with an BREDR connection, only the first
        will be returned.

    \param tp_addr Set to the typed BT address of a handset with a BREDR connection,
    if the function return value is TRUE.

    \return TRUE if a handset with an BREDR connection is found; FALSE if no handset BREDR connection.
*/
bool HandsetService_GetConnectedBredrHandsetTpAddress(tp_bdaddr * tp_addr);

/*! \brief Get the BT address of a handset with an LE connection.

    \note If there are multiple handsets with an LE connection, only the first
        will be returned.

    \param tp_addr Set to the typed BT address of a handset with an LE connection, 
    if the function return value is TRUE.

    \return TRUE if a handset with an LE connection is found; FALSE if no handset LE connection.
*/
bool HandsetService_GetConnectedLeHandsetTpAddress(tp_bdaddr *tp_addr);

/*! Set if the handset is BLE connectable.

    \param connectable TRUE if the device is BLE connectable.
*/
void HandsetService_SetBleConnectable(bool connectable);

/*! Configure handset service

    \param config Configuration
*/
bool HandsetService_Configure(handset_service_config_t config);

/*! \brief Check how many Bredr handsets are currently connected

    \return Number of connected Bredr handsets.
*/
unsigned HandsetService_GetNumberOfConnectedBredrHandsets(void);

/*! \brief Wrapper function which calls Multipoint API to connect multiple handsets.

    \param task Task the CFM will be sent to when the request is completed.
    \param profiles Profiles to connect.
*/
void HandsetService_ReconnectRequest(Task task, uint32 profiles);

/*! \brief Disconnect the least recently used handset.

    \param task The CFM will be sent to this task when the request is completed.
*/
void HandsetService_DisconnectLruHandsetRequest(Task task);

/*! \brief Wrapper function which calls Multipoint API to stop connect to handset.

    \param task Client task requesting the stop.
*/
void HandsetService_StopReconnect(Task task);

/*! \brief Disconnect all currently connected handsets.

    \param task Client task requesting the stop.
*/
void HandsetService_DisconnectAll(Task task);

/*! \brief Set default config after DFU

    It is called to set default handset service configuration,
    after software upgrade from ADK that didn't have handset service configuration.
*/
void HandsetService_SetDefaultConfig(void *value, uint8 size);
/*@}*/

#endif /* HANDSET_SERVICE_H_ */

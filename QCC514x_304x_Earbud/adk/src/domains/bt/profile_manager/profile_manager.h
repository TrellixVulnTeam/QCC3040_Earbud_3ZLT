/*!
\copyright  Copyright (c) 2018-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       profile_manager.h
\brief      The Profile Manager supervises the connection and disconnection of profiles
            to a remote device. It also tracks the profiles supported and connected with
            remote devices.

    These APIs are used by Profile implementations to register profile connection/disconnection
    handlers with the Profile Manager. They are also used by the Handset and Sink Services when
    either the Application or Topology layer wants to connect or disconnect some set of the
    supported profiles for a device. The Profile Manager also handles connection crossovers and
    is configurable to connect/disconnect the profiles in any order required by the Application.
*/

#ifndef PROFILE_MANAGER_H_
#define PROFILE_MANAGER_H_

#include <bdaddr.h>
#include <csrtypes.h>
#include <device.h>
#include <domain_message.h>
#include <task_list.h>

#define PROFILE_MANAGER_CLIENT_LIST_INIT_CAPACITY 1

/*! \brief supported profiles list. */
typedef enum {
   profile_manager_hfp_profile,
   profile_manager_a2dp_profile,
   profile_manager_avrcp_profile,
   profile_manager_ama_profile,
   profile_manager_gaa_profile,
   profile_manager_gaia_profile,
   profile_manager_peer_profile,
   profile_manager_accessory_profile,
   profile_manager_max_number_of_profiles,
   profile_manager_bad_profile
} profile_t;

typedef enum
{
    profile_manager_connect,
    profile_manager_disconnect
} profile_manager_request_type_t;

typedef enum
{
    profile_manager_success,
    profile_manager_failed,
    profile_manager_cancelled
} profile_manager_request_cfm_result_t;

typedef enum
{
    profile_manager_disconnected_normal,
    profile_manager_disconnected_link_loss,
    profile_manager_disconnected_link_transfer,
    profile_manager_disconnected_error
} profile_manager_disconnected_ind_reason_t;

enum profile_manager_messages
{
    /*
        The first four messages are sent by the profile manager to its registered clients (those
        that have called the ProfileManager_ClientRegister function).
    */
    CONNECT_PROFILES_CFM = PROFILE_MANAGER_MESSAGE_BASE,
    DISCONNECT_PROFILES_CFM,

    CONNECTED_PROFILE_IND,
    DISCONNECTED_PROFILE_IND,

    /*! This must be the final message */
    PROFILE_MANAGER_MESSAGE_END
};

typedef struct
{
    device_t device;
    profile_manager_request_cfm_result_t result;
} CONNECT_PROFILES_CFM_T;

typedef struct
{
    device_t device;
    uint32 profile;
} CONNECTED_PROFILE_IND_T;

typedef struct
{
    device_t device;
    profile_manager_request_cfm_result_t result;
} DISCONNECT_PROFILES_CFM_T;

typedef struct
{
    device_t device;
    uint32 profile;
    profile_manager_disconnected_ind_reason_t reason;
} DISCONNECTED_PROFILE_IND_T;

/*! \brief Profile manager task data. */
typedef struct
{
    TaskData dummy_task;
    TASK_LIST_WITH_INITIAL_CAPACITY(PROFILE_MANAGER_CLIENT_LIST_INIT_CAPACITY)             client_tasks;              /*!< List of tasks interested in Profile Manager indications */
    task_list_with_data_t   pending_connect_reqs;      /*!< List of tasks that are pending connection requests */
    task_list_with_data_t   pending_disconnect_reqs;   /*!< List of tasks that are pending discconnection requests */

} profile_manager_task_data;

/*!< Profile manager task */
extern profile_manager_task_data profile_manager;

/*! Get pointer to Device Management data structure */
#define ProfileManager_GetTaskData()  (&profile_manager)

/*! Get pointer to Device Management client tasks */
#define ProfileManager_GetClientTasks()  (task_list_flexible_t *)(&profile_manager.client_tasks)

/*! \brief Function pointers for Profile modules to implement APIs for connecting/disconnecting
           devices through the Profile Manager.

    \param bd_addr - Bluetooth address of the device to connect/disconnect.

    \return void
*/
typedef void (*profile_manager_registered_connect_request_t)(bdaddr* bd_addr);
typedef void (*profile_manager_registered_disconnect_request_t)(bdaddr* bd_addr);

/*! \brief Request to connect profiles for the given device.

     This API connects the BlueTooth profiles specified in the device property
     device_property_profiles_connect_order for the specified device. The profiles
     will be conected in the order in which they occur in the property, i.e. index 0 will
     be connected first. Profiles are connected sequentially.

    \param client the client requesting the connection, whom shall recieve the confirmation
    \param device for which profiles are to be connected

    \return TRUE : if a request was created , FALSE : If the request was not accepted
*/
bool ProfileManager_ConnectProfilesRequest(Task client, device_t device);

/*! \brief Request to disconnect profiles for the given device.

     This API disconnects the BlueTooth profiles specified in the device property
     device_property_profiles_disconnect_order for the specified device. The profiles
     will be disconected in the order in which they occur in the property, i.e. index 0 will
     be disconnected first. Profiles are disconnected sequentially.

    \param client the client requesting the disconnection, whom shall recieve the confirmation
    \param device for whch profiles are to be disconnected

    \return TRUE : if a request was created , FALSE : If the request was not accepted
*/
bool ProfileManager_DisconnectProfilesRequest(Task client, device_t device);

/*! \brief Register profile connect handlers with the profile manager.

    This api is called to register connect functions of different profiles
    and then these registered functions will be invoked by profile manager
    when any connect request will arrive from client.

    \param name - profile whose interface is being registered
    \param profile_if - pointer to profile interface struct.

    \return void
*/
void ProfileManager_RegisterProfile(profile_t name,
                                    profile_manager_registered_connect_request_t connect_fp,
                                    profile_manager_registered_disconnect_request_t disconnect_fp);

/*! \brief Helper function to add the profile manager as client of a profile modules that should
           recieve a response to a pending profile connect/disconnect request.

    This function is intended to be called by profile implementations to place the Profile Manager
    on their pending requests task list.

    \param list - the profiles pending request list
    \param device - the device that the pending request is for

    \return void
*/
void ProfileManager_AddToNotifyList(task_list_t *list, device_t device);

/*! \brief Helper function to send confirmations in response to a profile connect/disconnect
           request using a task list with data.

    \param list - the profiles pending request list
    \param bd_addr - the device bluetooth address that the notification is for
    \param result - whether the request was successful or not
    \param profile - the profile providing the notification
    \param type - whetehr the request was for connection or disconnection

    \return bool - indicates if a confirmation was notified, TRUE if notified.
*/
bool ProfileManager_NotifyConfirmation(task_list_t *list,
                                       const bdaddr * bd_addr,
                                       profile_manager_request_cfm_result_t result,
                                       profile_t profile,
                                       profile_manager_request_type_t type);

/*! \brief Initialse the Profile Manager module. */
bool ProfileManager_Init(Task init_task);

/*! \brief Register a Task to receive notifications from profile_manager.

    Once registered, #client_task will receive IND type messages from #profile_manager_messages.

    \param client_task Task to register to receive profile_manager notifications.
*/
void ProfileManager_ClientRegister(Task client_task);

/*! \brief Un-register a Task that is receiving notifications from profile_manager.

    If the task is not currently registered then nothing will be changed.

    \param client_task Task to un-register from profile_manager notifications.
*/
void ProfileManager_ClientUnregister(Task client_task);

/*! \brief Tell the profile manager that a generic profile is indicating connection.

    If the task is not currently registered then nothing will be changed.

    \param profile_t profile The type of generic profile that is indicating connection.
    \param bdaddr *bd_addr The Bluetooth address associated.
*/
void ProfileManager_GenericConnectedInd(profile_t profile, const bdaddr *bd_addr);

/*! \brief Tell the profile manager that a generic profile is indicating disconnection.

    \param profile_t profile The type of generic profile that is indicating disconnection.
    \param bdaddr *bd_addr The Bluetooth address associated.
    \param uint8 reason The reason for the disconnection.
*/
void ProfileManager_GenericDisconnectedInd(profile_t profile,
                                           const bdaddr *bd_addr,
                                           profile_manager_disconnected_ind_reason_t reason);

/*! \brief Tell the profile manager that a generic profile is confirming connection.

    \param profile_t profile The type of generic profile that is confirming connection.
    \param device_t device The device associated.
    \param bool successful.
*/
void ProfileManager_GenericConnectCfm(profile_t profile, device_t device, bool successful);

/*! \brief Tell the profile manager that a generic profile is confirming disconnection.

    \param profile_t profile The type of generic profile that is confirming disconnection.
    \param device_t device The device associated.
    \param bool successful.
*/
void ProfileManager_GenericDisconnectCfm(profile_t profile, device_t device, bool successful);

#endif /* PROFILE_MANAGER_H_*/

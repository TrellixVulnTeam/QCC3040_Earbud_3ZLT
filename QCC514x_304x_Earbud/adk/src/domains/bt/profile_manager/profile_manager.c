/*!
\copyright  Copyright (c) 2018 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       profile_manager.c
\brief      The Profile Manager supervises the connection and disconnection of profiles
            to a remote device. It also tracks the profiles supported and connected with
            remote devices.

    These APIs are used by Profile implementations to register profile connection/disconnection
    handlers with the Profile Manager. They are also used by the Handset and Sink Services when
    either the Application or Topology layer wants to connect or disconnect some set of the
    supported profiles for a device. The Profile Manager also handles connection crossovers and
    is configurable to connect/disconnect the profiles in any order required by the Application.
*/

#include "profile_manager.h"

#include <logging.h>
#include <panic.h>

#include <av.h>
#include <device_list.h>
#include <device_properties.h>
#include <hfp_profile.h>
#include <task_list.h>
#include <timestamp_event.h>

#ifdef FAST_PAIR_TIME_PROFILER
#include "fast_pair_time_profiler.h"
#endif

/* Dummy task address used to register the Profile Manager on Profile implementation connect/disconnect
   request task lists. */
#define DUMMY_PROFILE_MANAGER_TASK ((Task) 0xDEAD)

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_ENUM(profile_manager_messages)

#ifndef HOSTED_TEST_ENVIRONMENT

/*! There is checking that the messages assigned by this module do
not overrun into the next module's message ID allocation */
ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(PROFILE_MANAGER, PROFILE_MANAGER_MESSAGE_END)

#endif

#define PROFILE_MANAGER_LOG(...)   DEBUG_LOG(__VA_ARGS__)

profile_manager_task_data profile_manager;

typedef enum {
   request_succeeded,
   request_failed
} profile_request_status_t;

typedef struct
{
    profile_manager_registered_connect_request_t connect_req_fp;
    profile_manager_registered_disconnect_request_t disconnect_req_fp;
} profile_manager_registered_profile_interface_callbacks_t;

typedef struct
{
    device_t device;
    profile_manager_request_cfm_result_t result;
    profile_t profile;
    task_list_t * list;
    profile_manager_request_type_t type;
    bool is_profile_notification;
} profile_manager_notify_cfm_params_t;

/*! \brief array of function pointers used for making profile connection/disconnection requests. */
profile_manager_registered_profile_interface_callbacks_t profile_manager_interface_callbacks[profile_manager_max_number_of_profiles];

/* Keep in the order of the elements in the profile_t enum, and fully populated */
static uint32 profileTranslationList[profile_manager_max_number_of_profiles] = {
    DEVICE_PROFILE_HFP,
    DEVICE_PROFILE_A2DP,
    DEVICE_PROFILE_AVRCP,
    DEVICE_PROFILE_AMA,
    DEVICE_PROFILE_GAA,
    DEVICE_PROFILE_GAIA,
    DEVICE_PROFILE_PEER,
    DEVICE_PROFILE_ACCESSORY
};

typedef struct  {
    timestamp_event_id_t connected;
    timestamp_event_id_t disconnected;
} profileTimestampTable_t;

/* Keep in the order of the elements in the profile_t enum, and fully populated */
static profileTimestampTable_t profileTimestampTable[] =
{
        { TIMESTAMP_EVENT_PROFILE_CONNECTED_HFP,        TIMESTAMP_EVENT_PROFILE_DISCONNECTED_HFP        },
        { TIMESTAMP_EVENT_PROFILE_CONNECTED_A2DP,       TIMESTAMP_EVENT_PROFILE_DISCONNECTED_A2DP       },
        { TIMESTAMP_EVENT_PROFILE_CONNECTED_AVRCP,      TIMESTAMP_EVENT_PROFILE_DISCONNECTED_AVRCP      },
        { TIMESTAMP_EVENT_PROFILE_CONNECTED_AMA,        TIMESTAMP_EVENT_PROFILE_DISCONNECTED_AMA        },
        { TIMESTAMP_EVENT_PROFILE_CONNECTED_GAA,        TIMESTAMP_EVENT_PROFILE_DISCONNECTED_GAA        },
        { TIMESTAMP_EVENT_PROFILE_CONNECTED_GAIA,       TIMESTAMP_EVENT_PROFILE_DISCONNECTED_GAIA       },
        { TIMESTAMP_EVENT_PROFILE_CONNECTED_PEER,       TIMESTAMP_EVENT_PROFILE_DISCONNECTED_PEER       },
        { TIMESTAMP_EVENT_PROFILE_CONNECTED_ACCESSORY,  TIMESTAMP_EVENT_PROFILE_DISCONNECTED_ACCESSORY  }
};

static void profileManager_SetTimestamp(uint32 bt_profile)
{
#ifdef FAST_PAIR_TIME_PROFILER
    switch(bt_profile)
    {
    case DEVICE_PROFILE_HFP:
        fast_pair_event_time[fast_pair_event_hfp_conn_ind] = VmGetClock();
        break;
    case DEVICE_PROFILE_A2DP:
        fast_pair_event_time[fast_pair_event_a2dp_conn_ind] = VmGetClock();
    default:
        break;
    }
#else
    UNUSED(bt_profile);
#endif
}

static uint32 profileManager_ConvertToBtProfile(profile_t profile)
{
    return profileTranslationList[profile];
}

static profile_t profileManager_ConvertFromBtProfile(uint32 bt_profile)
{
    profile_t profile;
    for (profile = profile_manager_hfp_profile; profile < profile_manager_max_number_of_profiles; profile++)
    {
        if (bt_profile == profileTranslationList[profile])
            break;
    }

    return profile;
}

typedef struct
{
    device_t device;
    uint32 profile;
    profile_manager_request_type_t type;
} profile_manager_connect_next_on_ind_params;

/*******************************************************************************************************/

static void profileManager_SendConnectedInd(device_t device, uint32 profile)
{
    MESSAGE_MAKE(msg, CONNECTED_PROFILE_IND_T);
    msg->device = device;
    msg->profile = profile;
    TaskList_MessageSendWithSize(TaskList_GetFlexibleBaseTaskList(ProfileManager_GetClientTasks()), CONNECTED_PROFILE_IND, msg, sizeof(*msg));
}

static void profileManager_SendDisconnectedInd(device_t device, uint32 profile, profile_manager_disconnected_ind_reason_t reason)
{
    MESSAGE_MAKE(msg, DISCONNECTED_PROFILE_IND_T);
    msg->device = device;
    msg->profile = profile;
    msg->reason = reason;
    TaskList_MessageSendWithSize(TaskList_GetFlexibleBaseTaskList(ProfileManager_GetClientTasks()), DISCONNECTED_PROFILE_IND, msg, sizeof(*msg));
}

static task_list_t* profileManager_GetRequestTaskList(profile_manager_request_type_t type)
{
    profile_manager_task_data * pm = ProfileManager_GetTaskData();
    task_list_with_data_t * request_client = (type == profile_manager_connect) ? &pm->pending_connect_reqs : &pm->pending_disconnect_reqs;
    return TaskList_GetBaseTaskList(request_client);
}

static uint8 profileManager_GetNextProfileRequestIndex(device_t device)
{
    uint8 profile_connect_index = 0;
    if (Device_GetPropertyU8(device, device_property_profile_request_index, &profile_connect_index))
    {
        profile_connect_index++;
    }
    Device_SetPropertyU8(device, device_property_profile_request_index, profile_connect_index);

    DEBUG_LOG("profileManager_GetNextProfileRequestIndex index=%d", profile_connect_index);

    return profile_connect_index;
}

static bool profileManager_GetProfileRequestOrder(device_t device,
                                                  profile_manager_request_type_t type,
                                                  profile_t** profiles_connecting_order,
                                                  uint8 *profiles_order_list_size)
{
    bool is_order_valid = FALSE;
    void * profiles_order = NULL;
    size_t property_size;
    device_property_t order_property = (type == profile_manager_connect) ?
                device_property_profiles_connect_order : device_property_profiles_disconnect_order;

    is_order_valid = Device_GetProperty(device, order_property, &profiles_order, &property_size);

    *profiles_order_list_size = property_size;
    *profiles_connecting_order = (profile_t*) profiles_order;
    return is_order_valid;
}

static bool profileManager_IsAlreadyConnected(uint32 connected_profiles, profile_t requested_profile)
{
    bool already_connected = FALSE;
    if (requested_profile < profile_manager_max_number_of_profiles)
    {
        uint32 bt_profile = profileManager_ConvertToBtProfile(requested_profile);
        if (bt_profile & connected_profiles)
            already_connected = TRUE;
    }
    return already_connected;
}

static profile_t profileManager_GetNextProfile(device_t device, profile_manager_request_type_t type)
{
    profile_t * profiles_order = NULL;
    uint8 profiles_order_list_size = 0;
    profile_t profile = profile_manager_bad_profile;

    if (profileManager_GetProfileRequestOrder(device, type, &profiles_order, &profiles_order_list_size))
    {
        uint8 profile_request_index = profileManager_GetNextProfileRequestIndex(device);
        PanicFalse(profile_request_index != profiles_order_list_size);

        profile = profiles_order[profile_request_index];
    }

    DEBUG_LOG("profileManager_GetNextProfile enum:profile_t:%d, enum:profile_manager_request_type_t:%d", profile, type);

    return profile;
}

static profile_t profileManager_GetCurrentProfile(device_t device, profile_manager_request_type_t type, uint8 profile_request_index)
{
    profile_t * profiles_order = NULL;
    uint8 profiles_order_list_size = 0;
    profile_t profile = profile_manager_bad_profile;

    if (profileManager_GetProfileRequestOrder(device, type, &profiles_order, &profiles_order_list_size))
    {
        PanicFalse(profile_request_index != profiles_order_list_size);

        profile = profiles_order[profile_request_index];
    }

    DEBUG_LOG("profileManager_GetCurrentProfile profile enum:profile_t:%d, enum:profile_manager_request_type_t:%d", profile, type);

    return profile;
}

static void profileManager_SendConfimation(Task task, profile_manager_notify_cfm_params_t * params)
{
    if (params->type == profile_manager_connect)
    {
        MESSAGE_MAKE(msg, CONNECT_PROFILES_CFM_T);
        msg->device = params->device;
        msg->result = params->result;
        MessageSend(task, CONNECT_PROFILES_CFM, msg);
    }
    else
    {
        MESSAGE_MAKE(msg, DISCONNECT_PROFILES_CFM_T);
        msg->device = params->device;
        msg->result = params->result;
        MessageSend(task, DISCONNECT_PROFILES_CFM, msg);
    }

    DEBUG_LOG("profileManager_SendConfimation device=%p enum:profile_manager_request_type_t:%d enum:profile_manager_request_cfm_result_t:%d",
              params->device, params->type, params->result);
}

static void profileManager_SignalProfileConfirmation(profile_manager_notify_cfm_params_t * params)
{
    bool succeeded = (params->result == profile_manager_success) ? TRUE : FALSE;

    DEBUG_LOG("profileManager_SignalProfileConfirmation device=%p enum:profile_t:%d success=%d",
              params->device, params->profile, succeeded);

    if (params->type == profile_manager_connect)
    {
        ProfileManager_GenericConnectCfm(params->profile, params->device, succeeded);
    }
    else
    {
        ProfileManager_GenericDisconnectCfm(params->profile, params->device, succeeded);
    }
}

static bool profileManager_FindClientSendCfm(Task task, task_list_data_t *data, void *arg)
{
    bool found_client_task  = FALSE;
    profile_manager_notify_cfm_params_t * params = (profile_manager_notify_cfm_params_t *)arg;

    if (data && params && data->ptr == params->device)
    {
        found_client_task = TRUE;
        if (params->is_profile_notification)
        {
            profileManager_SignalProfileConfirmation(params);
        }
        else
        {
            profileManager_SendConfimation(task, params);
        }

        /* Remove the requesting task from the pending list now we have sent confirmation. */
        TaskList_RemoveTask(params->list, task);
    }
    else
    {
        if (data == NULL)
        {
            DEBUG_LOG_ERROR("profileManager_FindClientSendCfm NULL data");
        }
        if (params == NULL)
        {
            DEBUG_LOG_ERROR("profileManager_FindClientSendCfm NULL params");
        }
        if (data && params)
        {
            DEBUG_LOG_ERROR("profileManager_FindClientSendCfm data->ptr %p != params->device %p",
                            data->ptr, params->device);
        }
    }
    return !found_client_task;
}

static void profileManager_SendConfirmationToRequestor(device_t device, profile_manager_request_type_t type, profile_manager_request_cfm_result_t result)
{
    task_list_t * req_task_list = profileManager_GetRequestTaskList(type);
    profile_manager_task_data * pm = ProfileManager_GetTaskData();

    profile_manager_notify_cfm_params_t params = {0};
    params.device = device;
    params.result = result;
    if (type == profile_manager_connect)
    {
        params.list = TaskList_GetBaseTaskList(&pm->pending_connect_reqs);
        params.type = profile_manager_connect;
    }
    else
    {
        params.list = TaskList_GetBaseTaskList(&pm->pending_disconnect_reqs);
        params.type = profile_manager_disconnect;
    }
    TaskList_IterateWithDataRawFunction(req_task_list, profileManager_FindClientSendCfm, &params);
}

static inline bool profileManager_IsRequestComplete(profile_t profile)
{
    return (profile >= profile_manager_max_number_of_profiles);
}

static bool profileManager_IssueNextProfileRequest(device_t device, profile_t profile, profile_manager_request_type_t type)
{
    bool called = FALSE;
    bdaddr bd_addr = DeviceProperties_GetBdAddr(device);

    DEBUG_LOG("profileManager_IssueNextProfileRequest bdaddr %04x,%02x,%06lx , enum:profile_t:%d, enum:profile_manager_request_type_t:%d",
              bd_addr.nap, bd_addr.uap, bd_addr.lap, profile, type );

    /* profile must always be a valid value otherwise the array de-reference
       below be past the end of the array */
    PanicFalse(profile < profile_manager_max_number_of_profiles);

    if (type == profile_manager_connect)
    {
        if (profile_manager_interface_callbacks[profile].connect_req_fp != NULL)
        {
            profile_manager_interface_callbacks[profile].connect_req_fp(&bd_addr);
            called = TRUE;
        }
    }
    else
    {
        if (profile_manager_interface_callbacks[profile].disconnect_req_fp != NULL)
        {
            profile_manager_interface_callbacks[profile].disconnect_req_fp(&bd_addr);
            called = TRUE;
        }
    }

    if (!called)
    {
        DEBUG_LOG("profileManager_IssueNextProfileRequest: no callback");
    }

    return called;
}

static profile_t profileManager_ConsumeRequestsForProfilesAlreadyInRequestedState( device_t device, profile_manager_request_type_t type)
{
    bool requested_profile_is_connected = (type == profile_manager_connect) ? TRUE : FALSE;
    uint32 connected_profiles = BtDevice_GetConnectedProfiles(device);
    profile_t profile = profileManager_GetNextProfile(device, type);

    while ((profileManager_IsAlreadyConnected(connected_profiles, profile) == requested_profile_is_connected) && (profile < profile_manager_max_number_of_profiles))
    {
        DEBUG_LOG("profileManager_ConsumeRequestsForProfilesAlreadyInRequestedState enum:profile_t:%d consumed state=%d",
                  profile, requested_profile_is_connected);
        profile = profileManager_GetNextProfile(device, type);
    }
    return profile;
}

static bool profileManager_NextProfile(device_t device, profile_manager_request_type_t type)
{
    bool is_success = FALSE;
    profile_t profile = profileManager_ConsumeRequestsForProfilesAlreadyInRequestedState(device, type);

    if (profile == profile_manager_bad_profile)
    {
        return is_success;
    }

    while (!profileManager_IsRequestComplete(profile))
    {
        if (profileManager_IssueNextProfileRequest(device, profile, type))
        {
            break;
        }
        else
        {
            profile = profileManager_ConsumeRequestsForProfilesAlreadyInRequestedState(device, type);
        }
    }

    if (profileManager_IsRequestComplete(profile))
    {
        profile_manager_request_cfm_result_t result = profile_manager_success;
        if (type == profile_manager_connect)
        {
            /* only send success if any of the profiles is connected. */
            if (BtDevice_GetConnectedProfiles(device))
            {
                result = profile_manager_success;
            }
            else /* when none of the requested profiles is connected. */
            {
                result = profile_manager_failed;
            }
        }
        else if (type == profile_manager_disconnect)
        {
            result = profile_manager_success;
        }

        profileManager_SendConfirmationToRequestor(device, type, result);

        /* Clear up the device properties. */
        device_property_t order_property_to_cancel = (type == profile_manager_connect) ?
                    device_property_profiles_connect_order : device_property_profiles_disconnect_order;
        Device_RemoveProperty(device, device_property_profile_request_index);
        Device_RemoveProperty(device, order_property_to_cancel);
    }
    is_success = TRUE;

    return is_success;
}

static bool profileManager_IndWasExpectedProfile(uint32 profile, profile_t last_requested_profile)
{
    profile_t ind_profile = profileManager_ConvertFromBtProfile(profile);
    if (ind_profile == profile_manager_max_number_of_profiles)
        ind_profile = profile_manager_bad_profile;

    DEBUG_LOG("profileManager_IndWasExpectedProfile ind_profile enum:profile_t:%d ,last_requested_profile enum:profile_t:%d", ind_profile, last_requested_profile);
    return (ind_profile == last_requested_profile);
}

static profile_t profileManager_GetPendingRequestProfile(device_t device, profile_manager_request_type_t type)
{
    profile_t * profiles_request_order = NULL;
    uint8 profiles_order_list_size = 0;
    profile_t profile = profile_manager_bad_profile;

    if (profileManager_GetProfileRequestOrder(device, type, &profiles_request_order, &profiles_order_list_size))
    {
        uint8 profile_request_index = 0;
        if (Device_GetPropertyU8(device, device_property_profile_request_index, &profile_request_index))
        {
            DEBUG_LOG("profileManager_GetPendingRequestProfile profile_request_index=%d", profile_request_index);
            PanicFalse(profile_request_index != profiles_order_list_size);
            profile = profiles_request_order[profile_request_index];
        }
    }
    return profile;
}

static bool profileManager_RequestNextProfileOnIndication(Task task, task_list_data_t *data, void *arg)
{
    bool found_a_client_req_pending = FALSE;
    profile_manager_connect_next_on_ind_params *params = (profile_manager_connect_next_on_ind_params *)arg;

    UNUSED(task);

    if (data->ptr == params->device)
    {
        profile_t last_profile_requested = profileManager_GetPendingRequestProfile(params->device, params->type);
        if (profileManager_IndWasExpectedProfile(params->profile, last_profile_requested))
        {
            found_a_client_req_pending = TRUE;
            profileManager_NextProfile(params->device, params->type);
        }
    }
    return !found_a_client_req_pending;
}

static void profileManager_UpdateConnectedProfilesProperty(device_t device, uint32 profile, profile_manager_request_type_t type)
{
    uint32 connected_mask = BtDevice_GetConnectedProfiles(device);
    (type == profile_manager_connect) ? (connected_mask |= profile) : (connected_mask &= ~profile);
    BtDevice_SetConnectedProfiles(device, connected_mask);
    profile_t pm_profile = profileManager_ConvertFromBtProfile(profile);

    if (pm_profile < profile_manager_max_number_of_profiles)
    {
        TimestampEvent(type == profile_manager_connect ? profileTimestampTable[pm_profile].connected :
                                                         profileTimestampTable[pm_profile].disconnected);
    }
    else
    {
        DEBUG_LOG("profileManager_UpdateConnectedProfilesProperty unrecorded profile %u timestmap", profile);
    }

    DEBUG_LOG("profileManager_UpdateConnectedProfilesProperty type enum:profile_manager_request_type_t:%d connected_mask=%x", type, connected_mask );
}

static void profileManager_UpdateSupportedProfilesProperty(device_t device, uint32 profile, profile_manager_request_type_t type)
{
    if (type == profile_manager_connect)
    {
        BtDevice_AddSupportedProfilesToDevice(device, profile);
    }
}

static void profileManager_UpdateProfileRequestStatus(device_t device, uint32 profile, profile_request_status_t status, profile_manager_request_type_t type)
{
    task_list_t * req_task_list = profileManager_GetRequestTaskList(type);

    PanicNull(device);

    if (status == request_succeeded)
    {
        DEBUG_LOG("profileManager_HandleProfileRequestStatus Ok");
        profile_manager_connect_next_on_ind_params params = {0};
        params.device = device;
        params.profile = profile;
        params.type = type;

        /* If there is a pending request for this device, then if this was the profile last requested, then continue to the next profile */
        TaskList_IterateWithDataRawFunction(req_task_list, profileManager_RequestNextProfileOnIndication, &params);
    }
    else
    {
        DEBUG_LOG("profileManager_HandleProfileRequestStatus failed");

        if (type == profile_manager_connect)
        {
            profile_manager_connect_next_on_ind_params params = {0};
            params.device = device;
            params.profile = profile;
            params.type = type;

            /* Check if there are more profiles to be connected, despite one of the requested profiles failing to connect */
            TaskList_IterateWithDataRawFunction(req_task_list, profileManager_RequestNextProfileOnIndication, &params);
        }
        else
        {
            profile_manager_notify_cfm_params_t params = {0};
            params.device = device;
            params.result = profile_manager_failed;
            params.list = TaskList_GetBaseTaskList(&ProfileManager_GetTaskData()->pending_disconnect_reqs);
            params.type = profile_manager_disconnect;
            TaskList_IterateWithDataRawFunction(req_task_list, profileManager_FindClientSendCfm, &params);
        }
    }
}

static void profileManager_HandleProfileStatusChange(device_t device, uint32 profile, profile_manager_request_type_t type, profile_request_status_t status)
{
    DEBUG_LOG("profileManager_HandleProfileStatusChange device %p profile %d type enum:profile_manager_request_type_t:%u status enum:profile_request_status_t:%d",
              device, profile, type, status);

    if (status == request_succeeded)
    {
        profileManager_UpdateConnectedProfilesProperty(device, profile, type);
        profileManager_UpdateSupportedProfilesProperty(device, profile, type);

        if (type == profile_manager_connect)
        {
            profileManager_SetTimestamp(profile);
        }
    }

    profileManager_UpdateProfileRequestStatus(device, profile, status, type);
}

static void profileManager_HandleConnectedProfileInd(const bdaddr* bd_addr, uint32 profile, profile_request_status_t status)
{
    device_t device = BtDevice_GetDeviceForBdAddr(bd_addr);

    DEBUG_LOG("profileManager_HandleConnectedProfileInd bdaddr %04x,%02x,%06lx, profile=%d, status enum:profile_request_status_t:%d",
              bd_addr->nap, bd_addr->uap, bd_addr->lap, profile, status);

    profileManager_HandleProfileStatusChange(device, profile, profile_manager_connect, status);

    /*
     * Send CONNECTED_PROFILE_IND only in case of successful connection else this
     * would put the handset service state machine in a bad state.
     */
    if(status == request_succeeded)
    {
        profileManager_SendConnectedInd(device, profile);
    }
}

static void profileManager_HandleDisconnectedProfileInd(const bdaddr* bd_addr, uint32 profile, profile_request_status_t status, profile_manager_disconnected_ind_reason_t reason)
{
    device_t device = BtDevice_GetDeviceForBdAddr(bd_addr);

    DEBUG_LOG("profileManager_HandleDisconnectedProfileInd bdaddr %04x %02x %06x, profile=%d",
              bd_addr->nap, bd_addr->uap, bd_addr->lap, profile );

    profileManager_HandleProfileStatusChange(device, profile, profile_manager_disconnect, status);
    profileManager_SendDisconnectedInd(device, profile, reason);
}

static void profileManager_checkForAndCancelPendingProfileRequest(device_t device, profile_manager_request_type_t type)
{
    uint8 request_index = 0;
    if (Device_GetPropertyU8(device, device_property_profile_request_index, &request_index))
    {
        /* We are in the middle of a pending profile request that needs to be cancelled */
        profile_manager_request_type_t request_to_cancel_type = (type == profile_manager_connect) ?
                    profile_manager_disconnect : profile_manager_connect;
        device_property_t order_property_to_cancel = (request_to_cancel_type == profile_manager_connect) ?
                    device_property_profiles_connect_order : device_property_profiles_disconnect_order;

        /* Cancel the last issued request by calling it's opposite request type API. */
        profile_t profile_to_cancel = profileManager_GetCurrentProfile(device, request_to_cancel_type, request_index);

        DEBUG_LOG("profileManager_checkForAndCancelPendingProfileRequest profile_to_cancel enum:profile_t:%d, enum:profile_manager_request_type_t:%d", profile_to_cancel, type);

        /* Only cancel if there is a valid profile to cancel */
        if (profile_to_cancel < profile_manager_max_number_of_profiles)
        {
            profileManager_IssueNextProfileRequest(device, profile_to_cancel, type);
        }

        /* Clear the previous request order at current index to prevent any further profiles being requested. */
        Device_RemoveProperty(device, device_property_profile_request_index);
        Device_RemoveProperty(device, order_property_to_cancel);

        /* Send request confirmation cancelled to client task */
        profileManager_SendConfirmationToRequestor(device, request_to_cancel_type, profile_manager_cancelled);
    }
}

static bool profileManager_HandleRequest(Task client, device_t device, profile_manager_request_type_t type)
{
    bool request_created = FALSE;
    task_list_t * req_task_list = profileManager_GetRequestTaskList(type);

    PanicNull(device);

    DEBUG_LOG("profileManager_HandleRequest type enum:profile_manager_request_type_t:%d, device=%x", type, device);

    if (TaskList_IsTaskOnList(req_task_list, client))
    {
        /* This client still has an outstanding request in progress - ignore the new request. */
        DEBUG_LOG_WARN("profileManager_HandleRequest Previous request from client %p still in progress", client);
    }
    else
    {
        profileManager_checkForAndCancelPendingProfileRequest(device, type);

        /* Store device and client for the pending request */
        task_list_data_t device_used = {0};
        device_used.ptr = device;
        TaskList_AddTaskWithData(req_task_list, client, &device_used);

        request_created = profileManager_NextProfile(device, type);
    }
    return request_created;
}

void ProfileManager_RegisterProfile(profile_t profile,
                                    profile_manager_registered_connect_request_t connect,
                                    profile_manager_registered_disconnect_request_t disconnect)
{
    DEBUG_LOG_DEBUG("ProfileManager_RegisterProfile: enum:profile_t:%d", profile);
    
    if(profile < profile_manager_max_number_of_profiles)
    {
        DEBUG_LOG_DEBUG("ProfileManager_RegisterProfile: con=%p dis=%p", connect, disconnect);
        
        profile_manager_interface_callbacks[profile].connect_req_fp = connect;
        profile_manager_interface_callbacks[profile].disconnect_req_fp = disconnect;
    }
}

bool ProfileManager_ConnectProfilesRequest(Task client, device_t device)
{
    DEBUG_LOG("ProfileManager_ConnectProfilesRequest(0x%p, 0x%p)", client, device);
    PanicNull(device);
    return profileManager_HandleRequest(client, device, profile_manager_connect);
}

bool ProfileManager_DisconnectProfilesRequest(Task client, device_t device)
{
    DEBUG_LOG("ProfileManager_DisconnectProfilesRequest(0x%p, 0x%p)", client, device);
    PanicNull(device);
    return profileManager_HandleRequest(client, device, profile_manager_disconnect);
}

void ProfileManager_AddToNotifyList(task_list_t *list, device_t device)
{
    /* Store the device and the Profile Manager as a client on the TaskList in the profile module for the pending request */
    task_list_data_t device_used = {0};
    device_used.ptr = device;
    TaskList_AddTaskWithData(list, DUMMY_PROFILE_MANAGER_TASK, &device_used);
}

bool ProfileManager_NotifyConfirmation(task_list_t *list,
                                       const bdaddr* bd_addr,
                                       profile_manager_request_cfm_result_t result,
                                       profile_t profile,
                                       profile_manager_request_type_t type)
{
    profile_manager_notify_cfm_params_t params = {0};

    bdaddr addr = *bd_addr;
    DeviceProperties_SanitiseBdAddr(&addr);
    device_t device = DeviceList_GetFirstDeviceWithPropertyValue(device_property_bdaddr, &addr, sizeof(bdaddr));
    PanicNull(device);

    params.device = device;
    params.result = result;
    params.profile = profile;
    params.list = list;
    params.type = type;
    params.is_profile_notification = TRUE;

    return !TaskList_IterateWithDataRawFunction(list, profileManager_FindClientSendCfm, &params);
}

static void profileManager_DummytaskHandler(Task task, MessageId id, Message msg)
{
    UNUSED(task);
    UNUSED(id);
    UNUSED(msg);
}

bool ProfileManager_Init(Task init_task)
{
    profile_manager_task_data * pm = ProfileManager_GetTaskData();

    UNUSED(init_task);

    DEBUG_LOG("ProfileManager_Init");

    pm->dummy_task.handler = profileManager_DummytaskHandler;
    TaskList_InitialiseWithCapacity(ProfileManager_GetClientTasks(), PROFILE_MANAGER_CLIENT_LIST_INIT_CAPACITY);
    TaskList_WithDataInitialise(&pm->pending_connect_reqs);
    TaskList_WithDataInitialise(&pm->pending_disconnect_reqs);

    return TRUE;
}

void ProfileManager_ClientRegister(Task client_task)
{
    TaskList_AddTask(TaskList_GetFlexibleBaseTaskList(ProfileManager_GetClientTasks()), client_task);
}

void ProfileManager_ClientUnregister(Task client_task)
{
    TaskList_RemoveTask(TaskList_GetFlexibleBaseTaskList(ProfileManager_GetClientTasks()), client_task);
}

void ProfileManager_GenericConnectedInd(profile_t profile, const bdaddr *bd_addr)
{
    DEBUG_LOG("ProfileManager_GenericConnectedInd(enum:profile_t:%d, [%lx, %x, %x])",
        profile, bd_addr->lap, bd_addr->uap, bd_addr->nap);

    if (profile < profile_manager_max_number_of_profiles)
    {
        profileManager_HandleConnectedProfileInd(bd_addr, profileManager_ConvertToBtProfile(profile), request_succeeded);
    }
    else
    {
        DEBUG_LOG_ERROR("ProfileManager_GenericConnectedInd: invalid profile");
    }
}

void ProfileManager_GenericDisconnectedInd(profile_t profile,
                                           const bdaddr *bd_addr,
                                           profile_manager_disconnected_ind_reason_t reason)
{
    DEBUG_LOG("ProfileManager_GenericDisconnectedInd(enum:profile_t:%d, [%lx, %x, %x], %d)",
        profile, bd_addr->lap, bd_addr->uap, bd_addr->nap, reason);

    UNUSED(reason);

    if (profile < profile_manager_max_number_of_profiles)
    {
        profileManager_HandleDisconnectedProfileInd(bd_addr, profileManager_ConvertToBtProfile(profile),
            request_succeeded, reason);
    }
    else
    {
         DEBUG_LOG_ERROR("ProfileManager_GenericDisconnectedInd: invalid profile");
    }
}

void ProfileManager_GenericConnectCfm(profile_t profile, device_t device, bool successful)
{
    DEBUG_LOG("ProfileManager_GenericConnectCfm(enum:profile_t:%d, %p, %d)", profile,  device, successful);
    if (profile < profile_manager_max_number_of_profiles)
    {
        bool disconnecting = FALSE;
        if (successful)
        {
            uint8 profile_request_index = 0;
            if (Device_GetPropertyU8(device, device_property_profile_request_index, &profile_request_index))
            {
                void * profiles_order = NULL;
                size_t property_size;
                DEBUG_LOG("ProfileManager_GenericConnectCfm: Connect/Disconnect in progress");
                if (Device_GetProperty(device, device_property_profiles_disconnect_order, &profiles_order, &property_size))
                {
                    DEBUG_LOG("ProfileManager_GenericConnectCfm: connecting while disconnecting");
                    if (profile_manager_interface_callbacks[profile].disconnect_req_fp != NULL)
                    {
                        disconnecting = TRUE;
                        bdaddr bd_addr = DeviceProperties_GetBdAddr(device);
                        DEBUG_LOG("ProfileManager_GenericConnectCfm: disconnecting profile");
                        profile_manager_interface_callbacks[profile].disconnect_req_fp(&bd_addr);
                    }
                    else
                    {
                        DEBUG_LOG_ERROR("ProfileManager_GenericConnectCfm: No disconnect callback");
                    }
                }
                else
                {
                    DEBUG_LOG("ProfileManager_GenericConnectCfm: NOT connecting while disconnecting");
                }
            }
            else
            {
                DEBUG_LOG("ProfileManager_GenericConnectCfm: Connect/Disconnect NOT in progress");
            }
        }

        if (disconnecting == FALSE)
        {
            unsigned bt_device_profile = profileManager_ConvertToBtProfile(profile);
            profileManager_HandleProfileStatusChange(device, bt_device_profile,
                profile_manager_connect, successful ? request_succeeded : request_failed );
            if (successful)
            {
                profileManager_SendConnectedInd(device, bt_device_profile);
            }
        }
    }
    else
    {
        DEBUG_LOG_ERROR("ProfileManager_GenericConnectCfm: invalid profile");
    }
}

void ProfileManager_GenericDisconnectCfm(profile_t profile, device_t device, bool successful)
{
    DEBUG_LOG("ProfileManager_GenericDisconnectCfm(enum:profile_t:%d, %p, %d)", profile,  device, successful);

    if (profile < profile_manager_max_number_of_profiles)
    {
        unsigned bt_device_profile = profileManager_ConvertToBtProfile(profile);
        profileManager_HandleProfileStatusChange(device, bt_device_profile,
            profile_manager_disconnect, successful ? request_succeeded : request_failed );
        if (successful)
        {
            profileManager_SendDisconnectedInd(device, bt_device_profile, profile_manager_disconnected_normal);
        }
    }
    else
    {
        DEBUG_LOG_ERROR("ProfileManager_GenericDisconnectCfm: invalid profile");
    }
}


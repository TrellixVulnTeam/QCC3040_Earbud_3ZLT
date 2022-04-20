/*!
\copyright  Copyright (c) 2019 - 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Miscellaneous functions for the PEER service to find peer using LE
            and select role
*/

#include <panic.h>

#include <logging.h>

#include <ps.h>

#include <bt_device.h>

#include "system_state.h"

#include "peer_find_role.h"
#include "peer_find_role_init.h"
#include "peer_find_role_config.h"
#include "peer_find_role_private.h"
#include "timestamp_event.h"
#include <ps_key_map.h>
#include "multidevice.h"

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_TYPE(peer_find_role_message_t)
LOGGING_PRESERVE_MESSAGE_TYPE(peer_find_role_internal_message_t)

#ifndef HOSTED_TEST_ENVIRONMENT

/*! There is checking that the messages assigned by this module do
not overrun into the next module's message ID allocation */
ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(PEER_FIND_ROLE, PEER_FIND_ROLE_MESSAGE_END)

#endif

bool peer_find_role_is_central(void)
{
    peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();
    uint16 flags;

    if (appDeviceGetFlags(&pfr->my_addr, &flags))
    {
        if (flags & DEVICE_FLAGS_MIRRORING_C_ROLE)
        {
            return TRUE;
        }
    }
    return FALSE;
}


void PeerFindRole_FindRole(int32 high_speed_time_ms)
{
    peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();

    PeerFindRole_RecordCall();

    DEBUG_LOG("PeerFindRole_FindRole. Current state:%d. Timeout:%d. Call#:%u", 
                    peer_find_role_get_state(), high_speed_time_ms,
                    PeerFindRole_NumCalls());

    if(peer_find_role_get_state() == PEER_FIND_ROLE_STATE_INITIALISED)
    {

        TimestampEvent(TIMESTAMP_EVENT_PEER_FIND_ROLE_STARTED);

        if (high_speed_time_ms < 0)
        {
            high_speed_time_ms = -high_speed_time_ms;
            pfr->timeout_means_timeout = TRUE;
        }

        pfr->role_timeout_ms = high_speed_time_ms;
        pfr->advertising_backoff_ms = PeerFindRoleConfigInitialAdvertisingBackoffMs();

        if(PeerFindRole_GetFixedRole() == peer_find_role_fixed_role_secondary)
        {
            DEBUG_LOG("PeerFindRole_FindRole. Don't time out as we're always secondary");
        }
        peer_find_role_set_state(PEER_FIND_ROLE_STATE_CHECKING_PEER);
    }
    else
    {
        /* It is not ok to start a new find role while a previous one is still
           active. If a client wants to re-start a find role it must cancel the
           previous one first with PeerFindRole_FindRoleCancel. */;
        /*To re-check the rules for no_rule_no_idle, which is causing this Function to Hit
         * twice. Removing Panic for now. */
    }
}


void PeerFindRole_FindRoleCancel(void)
{
    DEBUG_LOG("PeerFindRole_FindRoleCancel. Current state:%d", peer_find_role_get_state());

    peer_find_role_cancel_initial_timeout();

    MessageSend(PeerFindRoleGetTask(), PEER_FIND_ROLE_INTERNAL_CANCEL_FIND_ROLE, NULL);
}


void PeerFindRole_DisableScanning(bool disable)
{
    DEBUG_LOG("PeerFindRole_DisableScanning disable:%d",disable);

    peer_find_role_update_media_flag(disable, PEER_FIND_ROLE_SCANNING_DISABLED);
}


void PeerFindRole_RegisterTask(Task t)
{
    if (PEER_FIND_ROLE_STATE_UNINITIALISED == peer_find_role_get_state())
    {
        DEBUG_LOG("PeerFindRole_RegisterTask. Attempt to register task before initialised.");
        Panic();
        return;
    }

    TaskList_AddTask(TaskList_GetFlexibleBaseTaskList(PeerFindRoleGetTaskList()), t);
}


void PeerFindRole_UnregisterTask(Task t)
{
    TaskList_RemoveTask(TaskList_GetFlexibleBaseTaskList(PeerFindRoleGetTaskList()), t);
}


peer_find_role_score_t PeerFindRole_CurrentScore(void)
{
    peer_find_role_calculate_score();
    return peer_find_role_score();
}


void PeerFindRole_RegisterPrepareClient(Task task)
{
    peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();

    TaskList_AddTask(&pfr->prepare_tasks, task);
}


void PeerFindRole_UnregisterPrepareClient(Task task)
{
    peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();

    TaskList_RemoveTask(&pfr->prepare_tasks, task);

    
    if (   TaskList_Size(pfr->pending_prepare_tasks) 
        && TaskList_IsTaskOnList(pfr->pending_prepare_tasks, task))
    {
        DEBUG_LOG_WARN("PeerFindRole_UnregisterPrepareClient. "
                        "Task:%p unregistered while prepare in progress",
                        task);

        /* Complete any outstanding PREPARE request */
        PeerFindRole_PrepareResponse(task);
    }
}


void PeerFindRole_PrepareResponse(Task task)
{
    peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();
    uint16 remaining_tasks;

    if (!TaskList_Size(pfr->pending_prepare_tasks))
    {
        return;
    }

    TaskList_RemoveTask(pfr->pending_prepare_tasks, task);
    remaining_tasks = TaskList_Size(pfr->pending_prepare_tasks);
    if (remaining_tasks)
    {
        DEBUG_LOG_DEBUG("PeerFindRole_PrepareResponse. Task:%p responded. %d tasks remain",
                        task, remaining_tasks);
        return;
    }
    TaskList_Destroy(pfr->pending_prepare_tasks);
    pfr->pending_prepare_tasks = NULL;

    switch (peer_find_role_get_state())
    {
    case PEER_FIND_ROLE_STATE_SERVER_PREPARING:
    case PEER_FIND_ROLE_STATE_CLIENT_PREPARING:
        /* Only send the internal prepared message if we are in a state that is
           waiting for it. */
        MessageSend(PeerFindRoleGetTask(), PEER_FIND_ROLE_INTERNAL_PREPARED, NULL);
        break;

    default:
        /* No need to send the internal prepared message in other states. */
        break;
    }
}



/*! Check if link to our peer is now encrypted and send ourselves a message if so 

    \param[in]  ind The encryption indication from connection library

    \return TRUE if the indication is for the address we expected, FALSE otherwise
*/
static bool peer_find_role_handle_encryption_change(const CL_SM_ENCRYPTION_CHANGE_IND_T *ind)
{
    peerFindRoleTaskData *pfr = PeerFindRoleGetTaskData();

    if (BtDevice_LeDeviceIsPeer(&ind->tpaddr))
    {
        DEBUG_LOG("peer_find_role_handle_encryption_change. CL_SM_ENCRYPTION_CHANGE_IND %d state %d",
                    ind->encrypted, peer_find_role_get_state());

        pfr->gatt_encrypted = ind->encrypted;

        if (ind->encrypted)
        {
            if (PEER_FIND_ROLE_STATE_CLIENT_AWAITING_ENCRYPTION == peer_find_role_get_state())
            {
                peer_find_role_set_state(PEER_FIND_ROLE_STATE_CLIENT_PREPARING);
            }
        }
        return TRUE;
    }
    return FALSE;
}


bool PeerFindRole_HandleConnectionLibraryMessages(MessageId id, Message message,
                                                  bool already_handled)
{
    UNUSED(already_handled);
    
    /* Encryption is an indication. We don't care if already handled */
    if (CL_SM_ENCRYPTION_CHANGE_IND == id)
    {
        return peer_find_role_handle_encryption_change((const CL_SM_ENCRYPTION_CHANGE_IND_T *)message);
    }

    return FALSE;
}
#define FIXED_ROLE_PSKEY_LEN 1

/*! \brief Query if a fixed role has been assigned to the device. 

    \return The current role assigend to this device or peer_find_role_fixed_role_not_set if
            role switching is allowed
            The device should be reset after changing this setting
*/
peer_find_role_fixed_role_t PeerFindRole_GetFixedRole(void)
{
    if(peer_find_role.fixed_role == peer_find_role_fixed_role_invalid)
    {
        uint16 pskey_role;

        if(PsRetrieve(PS_KEY_FIXED_ROLE, &pskey_role, FIXED_ROLE_PSKEY_LEN))
        {
            peer_find_role_fixed_role_t role =(peer_find_role_fixed_role_t)pskey_role;

            if (role == peer_find_role_fixed_role_automatic)
            {
                switch (Multidevice_GetSide())
                {
                    case multidevice_side_left:
                        role = peer_find_role_fixed_role_secondary;
                        break;

                    case multidevice_side_right:
                        role = peer_find_role_fixed_role_primary;
                        break;

                    default:
                        role = peer_find_role_fixed_role_invalid;
                        break;
                }
            }

            peer_find_role.fixed_role = role;
        }

        if(peer_find_role.fixed_role >= peer_find_role_fixed_role_automatic)
        {
            peer_find_role.fixed_role = peer_find_role_fixed_role_not_set;
        }
    }

    DEBUG_LOG("PeerFindRole_GetFixedRole. role = %d", peer_find_role.fixed_role);

    return peer_find_role.fixed_role;
}

/*! \brief Assign a fixed role to the device or enable role switching

    \param role enum representing role behaviour the device is to adopt
*/
void PeerFindRole_SetFixedRole(peer_find_role_fixed_role_t role)
{
    if(role < peer_find_role_fixed_role_invalid)
    {
        uint16 buffer = (uint16)role;

        PanicFalse(PsStore(PS_KEY_FIXED_ROLE, &buffer, FIXED_ROLE_PSKEY_LEN));

        peer_find_role.fixed_role = role;
    }
    else
    {
        Panic();
    }
}

bool PeerFindRole_IsActive(void)
{
    switch (peer_find_role_get_state())
    {
        case PEER_FIND_ROLE_STATE_UNINITIALISED:
        case PEER_FIND_ROLE_STATE_INITIALISED:
            return FALSE;
        default:
            return TRUE;
    }
}

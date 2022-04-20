/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Procedure to connect profiles between Primary and Secondary Earbuds.
*/

#include "tws_topology_procedure_pri_connect_peer_profiles.h"
#include "tws_topology_procedures.h"
#include "tws_topology_common_primary_rule_functions.h"

#include <bt_device.h>
#include <peer_signalling.h>
#include <mirror_profile.h>
#include <av.h>
#include <handover_profile.h>
#include <connection_manager.h>

#include <logging.h>

#include <message.h>

static void twsTopology_ProcPriConnectPeerProfilesHandleMessage(Task task, MessageId id, Message message);
void TwsTopology_ProcedurePriConnectPeerProfilesStart(Task result_task,
                                        procedure_start_cfm_func_t proc_start_cfm_fn,
                                        procedure_complete_func_t proc_complete_fn,
                                        Message goal_data);
void TwsTopology_ProcedurePriConnectPeerProfilesCancel(procedure_cancel_cfm_func_t proc_cancel_fn);

const procedure_fns_t proc_pri_connect_peer_profiles_fns = {
    TwsTopology_ProcedurePriConnectPeerProfilesStart,
    TwsTopology_ProcedurePriConnectPeerProfilesCancel,
};

typedef struct
{
    TaskData task;
    procedure_complete_func_t complete_fn;
    uint32 profiles_status;
    bool active;
} twsTopProcPriConnectPeerProfilesTaskData;

twsTopProcPriConnectPeerProfilesTaskData twstop_proc_pri_connect_peer_profiles = {twsTopology_ProcPriConnectPeerProfilesHandleMessage};

#define TwsTopProcPriConnectPeerProfilesGetTaskData()     (&twstop_proc_pri_connect_peer_profiles)
#define TwsTopProcPriConnectPeerProfilesGetTask()         (&twstop_proc_pri_connect_peer_profiles.task)

static void twsTopology_ProcedurePriConnectPeerProfilesReset(void)
{
    twsTopProcPriConnectPeerProfilesTaskData* td = TwsTopProcPriConnectPeerProfilesGetTaskData();
    bdaddr secondary_addr;

    /* release the ACL, now held open by L2CAP */
    appDeviceGetSecondaryBdAddr(&secondary_addr);
    ConManagerReleaseAcl(&secondary_addr);
    ConManagerUnregisterTpConnectionsObserver(cm_transport_bredr, TwsTopProcPriConnectPeerProfilesGetTask());

    td->profiles_status = 0;
    td->active = FALSE;
}

static void twsTopology_ProcedurePriConnectPeerProfilesConnectProfile(void)
{
    twsTopProcPriConnectPeerProfilesTaskData* td = TwsTopProcPriConnectPeerProfilesGetTaskData();
    bdaddr secondary_addr;

    appDeviceGetSecondaryBdAddr(&secondary_addr);

    /* Connect the requested profiles in sequence. */
#ifdef INCLUDE_MIRRORING
    if(td->profiles_status & DEVICE_PROFILE_HANDOVER)
    {
        DEBUG_LOG("twsTopology_ProcedurePriConnectPeerProfilesConnectProfile HANDOVER");
        HandoverProfile_Connect(TwsTopProcPriConnectPeerProfilesGetTask(), &secondary_addr);
        return;
    }
    if (td->profiles_status & DEVICE_PROFILE_MIRROR)
    {
        DEBUG_LOG("twsTopology_ProcedurePriConnectPeerProfilesConnectProfile MIRROR");
        MirrorProfile_Connect(TwsTopProcPriConnectPeerProfilesGetTask(),&secondary_addr);
        return;
    }
#endif /* INCLUDE_MIRRORING */

    /* When peer signalling connects, many application messages are sent between
       buds (e.g. syncing state). These messages would delay the connection of other
       profiles if they were not already connected. So peer sig is connected last
       to allow this procedure to complete as quickly as possible */
    if (td->profiles_status & DEVICE_PROFILE_PEERSIG)
    {
        DEBUG_LOG("twsTopology_ProcedurePriConnectPeerProfilesConnectProfile PEERSIG");
        appPeerSigConnect(TwsTopProcPriConnectPeerProfilesGetTask(), &secondary_addr);
    }
}

void TwsTopology_ProcedurePriConnectPeerProfilesStart(Task result_task,
                                                   procedure_start_cfm_func_t proc_start_cfm_fn,
                                                   procedure_complete_func_t proc_complete_fn,
                                                   Message goal_data)
{
    twsTopProcPriConnectPeerProfilesTaskData* td = TwsTopProcPriConnectPeerProfilesGetTaskData();
    TWSTOP_PRIMARY_GOAL_CONNECT_PEER_PROFILES_T* cpp = (TWSTOP_PRIMARY_GOAL_CONNECT_PEER_PROFILES_T*)goal_data;

    UNUSED(result_task);

    DEBUG_LOG("TwsTopology_ProcedurePriConnectPeerProfilesStart");

    /* remember the profiles requested to track when complete */
    td->profiles_status = cpp->profiles;

    /* remember completion function */
    td->complete_fn = proc_complete_fn;
    /* mark procedure active so if cleanup() requested this procedure can ignore
     * any CFM messages that arrive afterwards */
    td->active = TRUE;

    ConManagerRegisterTpConnectionsObserver(cm_transport_bredr, TwsTopProcPriConnectPeerProfilesGetTask());

    twsTopology_ProcedurePriConnectPeerProfilesConnectProfile();

    /* start is synchronous, use the callback to confirm now */
    proc_start_cfm_fn(tws_topology_procedure_pri_connect_peer_profiles, procedure_result_success);
}

void TwsTopology_ProcedurePriConnectPeerProfilesCancel(procedure_cancel_cfm_func_t proc_cancel_cfm_fn)
{
    DEBUG_LOG("TwsTopology_ProcedurePriConnectPeerProfilesCancel");

    twsTopology_ProcedurePriConnectPeerProfilesReset();
    Procedures_DelayedCancelCfmCallback(proc_cancel_cfm_fn, tws_topology_procedure_pri_connect_peer_profiles, procedure_result_success);
}

static void twsTopology_ProcPriConnectPeerProfilesStatus(uint32 profile, bool succeeded)
{
    twsTopProcPriConnectPeerProfilesTaskData* td = TwsTopProcPriConnectPeerProfilesGetTaskData();

    /* remove the profile from the list being handled */
    td->profiles_status &= ~profile;

    /* if one of the profiles failed to connect, then reset this procedure and report
     * failure */
    if (!succeeded)
    {
        DEBUG_LOG("twsTopology_ProcPriConnectPeerProfilesStatus failed");
        twsTopology_ProcedurePriConnectPeerProfilesReset();
        td->complete_fn(tws_topology_procedure_pri_connect_peer_profiles, procedure_result_failed);
    }
    else if (!td->profiles_status)
    {
        /* reset procedure and report start complete if all profiles connected */
        twsTopology_ProcedurePriConnectPeerProfilesReset();
        td->complete_fn(tws_topology_procedure_pri_connect_peer_profiles, procedure_result_success);
    }
    else
    {
        twsTopology_ProcedurePriConnectPeerProfilesConnectProfile();
    }
}

static void twsTopology_ProcPriConnectPeerProfileHandleDisconnectInd(const CON_MANAGER_TP_DISCONNECT_IND_T* ind)
{
    twsTopProcPriConnectPeerProfilesTaskData* td = TwsTopProcPriConnectPeerProfilesGetTaskData();
    bdaddr secondary_addr;
    appDeviceGetSecondaryBdAddr(&secondary_addr);

    if (BdaddrIsSame(&ind->tpaddr.taddr.addr, &secondary_addr))
    {
        DEBUG_LOG("twsTopology_ProcPriConnectPeerProfileHandleDisconnectInd secondary disconnected");
        td->complete_fn(tws_topology_procedure_pri_connect_peer_profiles, procedure_result_failed);
        twsTopology_ProcedurePriConnectPeerProfilesReset();
    }
}

static void twsTopology_ProcPriConnectPeerProfilesHandleMessage(Task task, MessageId id, Message message)
{
    twsTopProcPriConnectPeerProfilesTaskData* td = TwsTopProcPriConnectPeerProfilesGetTaskData();

    UNUSED(task);

    /* if no longer active then ignore any CFM messages,
     * they'll be connect_cfm(cancelled) */
    if (!td->active)
    {
        return;
    }

    switch (id)
    {
        case CON_MANAGER_TP_DISCONNECT_IND:
            twsTopology_ProcPriConnectPeerProfileHandleDisconnectInd(message);
        break;

        case PEER_SIG_CONNECT_CFM:
        {
            PEER_SIG_CONNECT_CFM_T* cfm = (PEER_SIG_CONNECT_CFM_T*)message;
            DEBUG_LOG("twsTopology_ProcPriConnectPeerProfilesHandleMessage PEERSIG status %d", cfm->status);
            twsTopology_ProcPriConnectPeerProfilesStatus(DEVICE_PROFILE_PEERSIG, cfm->status == peerSigStatusSuccess);
        }
        break;

#ifdef INCLUDE_MIRRORING
        case HANDOVER_PROFILE_CONNECT_CFM:
        {
            HANDOVER_PROFILE_CONNECT_CFM_T *cfm = (HANDOVER_PROFILE_CONNECT_CFM_T *)message;
            DEBUG_LOG("twsTopology_ProcPriConnectPeerProfilesHandleMessage HANDOVER_PROFILE_CONNECT_CFM received, status %d", cfm->status);
            twsTopology_ProcPriConnectPeerProfilesStatus(DEVICE_PROFILE_HANDOVER, cfm->status == HANDOVER_PROFILE_STATUS_SUCCESS);
        }
        break;

        case MIRROR_PROFILE_CONNECT_CFM:
        {
            MIRROR_PROFILE_CONNECT_CFM_T* cfm = (MIRROR_PROFILE_CONNECT_CFM_T*)message;
            DEBUG_LOG("twsTopology_ProcPriConnectPeerProfilesHandleMessage MIRROR status %d", cfm->status);
            twsTopology_ProcPriConnectPeerProfilesStatus(DEVICE_PROFILE_MIRROR, cfm->status == mirror_profile_status_peer_connected);
        }
        break;
#endif /* INCLUDE_MIRRORING */

        /*! \todo handle AV connect CFM */
        default:
        break;
    }
}


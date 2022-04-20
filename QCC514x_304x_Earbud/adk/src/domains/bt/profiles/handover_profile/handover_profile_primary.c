/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\defgroup   handover_profile Handover Profile
\ingroup    profiles
\brief      Implementation of the handover of (initial) primary device.
*/
#ifdef INCLUDE_MIRRORING

#include "handover_profile_private.h"

/*! Primary handover states */
typedef enum handover_profile_primary_states
{
    HO_PRIMARY_STATE_SETUP,
    HO_PRIMARY_STATE_SELF_VETO,
    HO_PRIMARY_STATE_VETO1,
    HO_PRIMARY_STATE_SEND_START_REQ,
    HO_PRIMARY_STATE_WAIT_FOR_START_CFM,
    HO_PRIMARY_STATE_SEND_P1_MARSHAL_DATA,
    HO_PRIMARY_STATE_WAIT_FOR_MARSHAL_DATA_CFM,
    HO_PRIMARY_STATE_VETO2,
    HO_PRIMARY_STATE_PERFORMANCE_REQUEST,
    HO_PRIMARY_STATE_HALT_INACTIVE_BREDR_LINKS,
    HO_PRIMARY_STATE_VETO3,
    HO_PRIMARY_STATE_PREPARE_INACTIVE_BREDR_LINKS,
    HO_PRIMARY_STATE_VETO4,
    HO_PRIMARY_STATE_SEND_INACTIVE_BREDR_LINK_MARSHAL_DATA,
    HO_PRIMARY_STATE_WAIT_FOR_A2DP_PACKET,
    HO_PRIMARY_STATE_HALT_ACTIVE_BREDR_LINKS,
    HO_PRIMARY_STATE_VETO5,
    HO_PRIMARY_STATE_SET_PEER_LINK_ACTIVE_MODE,
    HO_PRIMARY_STATE_PREPARE_ACTIVE_BREDR_LINKS,
    HO_PRIMARY_STATE_WAIT_FOR_PEER_LINK_ACTIVE_MODE,
    HO_PRIMARY_STATE_VETO6,
    HO_PRIMARY_STATE_SEND_ACTIVE_BREDR_LINK_MARSHAL_DATA,
    HO_PRIMARY_STATE_CLEAR_PENDING_PEER_DATA,
    HO_PRIMARY_STATE_COMMIT_TO_SECONDARY_ROLE,
    HO_PRIMARY_STATE_PERFORMANCE_RELINQUISH,
    HO_PRIMARY_STATE_CLEANUP,
    HO_PRIMARY_STATE_COMPLETE

} handover_profile_primary_state_t;

static handover_profile_status_t handoverProfile_PrimarySetup(void);
static handover_profile_status_t handoverProfile_PrimarySelfVeto(void);
static handover_profile_status_t handoverProfile_PrimaryPerformanceRequest(void);
static handover_profile_status_t handoverProfile_HaltInactiveBredrLinks(void);
static handover_profile_status_t handoverProfile_PrepareInactiveBredrLinks(void);
static handover_profile_status_t handoverProfile_SendInactiveLinkMarshalData(void);
static handover_profile_status_t handoverProfile_WaitForA2dpPacket(void);
static handover_profile_status_t handoverProfile_HaltActiveBredrLinks(void);
static handover_profile_status_t handoverProfile_SetPeerLinkActiveMode(void);
static handover_profile_status_t handoverProfile_PrepareActiveBredrLinks(void);
static handover_profile_status_t handoverProfile_WaitForPeerLinkActiveMode(void);
static handover_profile_status_t handoverProfile_SendActiveLinkMarshalData(void);
static handover_profile_status_t handoverProfile_ClearPendingPeerData(void);
static handover_profile_status_t handoverProfile_CommitSecondaryRole(void);
static handover_profile_status_t handoverProfile_PrimaryPerformanceRelinquish(void);
static handover_profile_status_t handoverProfile_PrimaryCleanup(void);

/*! This table maps a function to perform the actions required of the associated state */
handover_profile_status_t (* const state_handler_map[])(void) =
{
    [HO_PRIMARY_STATE_SETUP] =                                  handoverProfile_PrimarySetup,
    [HO_PRIMARY_STATE_SELF_VETO] =                              handoverProfile_PrimarySelfVeto,
    [HO_PRIMARY_STATE_VETO1] =                                  handoverProfile_VetoP1Clients,
    [HO_PRIMARY_STATE_SEND_START_REQ] =                         handoverProtocol_SendStartReq,
    [HO_PRIMARY_STATE_WAIT_FOR_START_CFM] =                     handoverProtocol_WaitForStartCfm,
    [HO_PRIMARY_STATE_SEND_P1_MARSHAL_DATA] =                   handoverProtocol_SendP1MarshalData,
    [HO_PRIMARY_STATE_WAIT_FOR_MARSHAL_DATA_CFM] =              handoverProtocol_WaitForUnmarshalP1Cfm,
    [HO_PRIMARY_STATE_VETO2] =                                  handoverProfile_VetoP1Clients,
    [HO_PRIMARY_STATE_PERFORMANCE_REQUEST] =                    handoverProfile_PrimaryPerformanceRequest,
    [HO_PRIMARY_STATE_HALT_INACTIVE_BREDR_LINKS] =              handoverProfile_HaltInactiveBredrLinks,
    [HO_PRIMARY_STATE_VETO3] =                                  handoverProfile_VetoP1Clients,
    [HO_PRIMARY_STATE_PREPARE_INACTIVE_BREDR_LINKS] =           handoverProfile_PrepareInactiveBredrLinks,
    [HO_PRIMARY_STATE_VETO4] =                                  handoverProfile_VetoP1Clients,
    [HO_PRIMARY_STATE_SEND_INACTIVE_BREDR_LINK_MARSHAL_DATA] =  handoverProfile_SendInactiveLinkMarshalData,
    [HO_PRIMARY_STATE_WAIT_FOR_A2DP_PACKET] =                   handoverProfile_WaitForA2dpPacket,
    [HO_PRIMARY_STATE_HALT_ACTIVE_BREDR_LINKS] =                handoverProfile_HaltActiveBredrLinks,
    [HO_PRIMARY_STATE_VETO5] =                                  handoverProfile_VetoP1Clients,
    [HO_PRIMARY_STATE_SET_PEER_LINK_ACTIVE_MODE] =              handoverProfile_SetPeerLinkActiveMode,
    [HO_PRIMARY_STATE_PREPARE_ACTIVE_BREDR_LINKS] =             handoverProfile_PrepareActiveBredrLinks,
    [HO_PRIMARY_STATE_WAIT_FOR_PEER_LINK_ACTIVE_MODE] =         handoverProfile_WaitForPeerLinkActiveMode,
    [HO_PRIMARY_STATE_VETO6] =                                  handoverProfile_VetoP1Clients,
    [HO_PRIMARY_STATE_SEND_ACTIVE_BREDR_LINK_MARSHAL_DATA] =    handoverProfile_SendActiveLinkMarshalData,
    [HO_PRIMARY_STATE_CLEAR_PENDING_PEER_DATA] =                handoverProfile_ClearPendingPeerData,
    [HO_PRIMARY_STATE_COMMIT_TO_SECONDARY_ROLE] =               handoverProfile_CommitSecondaryRole,
    [HO_PRIMARY_STATE_PERFORMANCE_RELINQUISH] =                 handoverProfile_PrimaryPerformanceRelinquish,
    [HO_PRIMARY_STATE_CLEANUP] =                                handoverProfile_PrimaryCleanup,
    [HO_PRIMARY_STATE_COMPLETE] =                               NULL,
};

static handover_profile_status_t handoverProfile_CancelPrepareActiveBredrLinks(void);
static handover_profile_status_t handoverProfile_SetPeerLinkSniffMode(void);
static handover_profile_status_t handoverProfile_ResumeActiveBredrLinks(void);
static handover_profile_status_t handoverProfile_CancelPrepareInactiveBredrLinks(void);
static handover_profile_status_t handoverProfile_ResumeInactiveBredrLinks(void);
static handover_profile_status_t handoverProfile_PrimaryPerformanceRequest(void);
static handover_profile_status_t handoverProfile_Panic(void);

/*! This table maps a function to handle the failure of handover in the
    associated state. Each handler undoes the actions of the corresponding
    function in the state_handler_map table */
handover_profile_status_t (* const state_failure_map[])(void) =
{
    /* Handle failure by undoing each action performed durign the handover procedure. */
    [HO_PRIMARY_STATE_SETUP] =                                  handoverProfile_PrimaryCleanup,
    [HO_PRIMARY_STATE_SELF_VETO] =                              NULL,
    [HO_PRIMARY_STATE_VETO1] =                                  NULL,
    [HO_PRIMARY_STATE_SEND_START_REQ] =                         handoverProtocol_SendCancelInd,
    [HO_PRIMARY_STATE_WAIT_FOR_START_CFM] =                     NULL,
    [HO_PRIMARY_STATE_SEND_P1_MARSHAL_DATA] =                   handoverProfile_AbortP1Clients,
    [HO_PRIMARY_STATE_WAIT_FOR_MARSHAL_DATA_CFM] =              NULL,
    [HO_PRIMARY_STATE_VETO2] =                                  NULL,
    [HO_PRIMARY_STATE_PERFORMANCE_REQUEST] =                    handoverProfile_PrimaryPerformanceRelinquish,
    [HO_PRIMARY_STATE_HALT_INACTIVE_BREDR_LINKS] =              handoverProfile_ResumeInactiveBredrLinks,
    [HO_PRIMARY_STATE_VETO3] =                                  NULL,
    [HO_PRIMARY_STATE_PREPARE_INACTIVE_BREDR_LINKS] =           handoverProfile_CancelPrepareInactiveBredrLinks,
    [HO_PRIMARY_STATE_VETO4] =                                  NULL,
    [HO_PRIMARY_STATE_SEND_INACTIVE_BREDR_LINK_MARSHAL_DATA] =  NULL,
    [HO_PRIMARY_STATE_WAIT_FOR_A2DP_PACKET] =                   NULL,
    [HO_PRIMARY_STATE_HALT_ACTIVE_BREDR_LINKS] =                handoverProfile_ResumeActiveBredrLinks,
    [HO_PRIMARY_STATE_VETO5] =                                  NULL,
    [HO_PRIMARY_STATE_SET_PEER_LINK_ACTIVE_MODE] =              handoverProfile_SetPeerLinkSniffMode,
    [HO_PRIMARY_STATE_PREPARE_ACTIVE_BREDR_LINKS] =             handoverProfile_CancelPrepareActiveBredrLinks,
    [HO_PRIMARY_STATE_WAIT_FOR_PEER_LINK_ACTIVE_MODE] =         NULL,
    [HO_PRIMARY_STATE_VETO6] =                                  NULL,
    [HO_PRIMARY_STATE_SEND_ACTIVE_BREDR_LINK_MARSHAL_DATA] =    NULL,
    /* States not expected to fail under any circumstances */
    [HO_PRIMARY_STATE_CLEAR_PENDING_PEER_DATA] =                handoverProfile_Panic,
    [HO_PRIMARY_STATE_COMMIT_TO_SECONDARY_ROLE] =               handoverProfile_Panic,
    [HO_PRIMARY_STATE_PERFORMANCE_RELINQUISH] =                 handoverProfile_Panic,
    [HO_PRIMARY_STATE_CLEANUP] =                                handoverProfile_Panic,
    [HO_PRIMARY_STATE_COMPLETE] =                               handoverProfile_Panic,
};

static void handoverProfile_HandleFailureAsPrimary(handover_profile_primary_state_t last_completed_state);

handover_profile_status_t handoverProfile_HandoverAsPrimary(void)
{
    handover_profile_status_t status = HANDOVER_PROFILE_STATUS_SUCCESS;
    handover_profile_primary_state_t primary_state;

    for (primary_state = HO_PRIMARY_STATE_SETUP; primary_state != HO_PRIMARY_STATE_COMPLETE; primary_state++)
    {
        DEBUG_LOG_INFO("handoverProfile_HandoverAsPrimary enum:handover_profile_primary_state_t:%d", primary_state);

        if (state_handler_map[primary_state])
        {
            status = state_handler_map[primary_state]();

            /* Break out at the point a procedure is failed */
            if (status != HANDOVER_PROFILE_STATUS_SUCCESS)
            {
                break;
            }
        }
    }

    if (status != HANDOVER_PROFILE_STATUS_SUCCESS)
    {
        /* Pass in the last successful state */
        handoverProfile_HandleFailureAsPrimary(primary_state-1);
    }
    return status;
}

/*! \brief Iterate back through the completed states undoing actions to handle the failure */
static void handoverProfile_HandleFailureAsPrimary(handover_profile_primary_state_t last_completed_state)
{
    do
    {
        if (state_failure_map[last_completed_state])
        {
            DEBUG_LOG_INFO("handoverProfile_HandleFailureAsPrimary enum:handover_profile_primary_state_t:%d", last_completed_state);
            state_failure_map[last_completed_state]();
        }
    } while (last_completed_state-- != HO_PRIMARY_STATE_SETUP);
}

static handover_profile_status_t handoverProfile_PrimaryCleanup(void)
{
    handover_profile_task_data_t *ho_inst = Handover_GetTaskData();
    handover_device_t *next = ho_inst->device_list;
    while (next)
    {
        handover_device_t *current = next;
        Source source = current->u.p.btstack_source;
        next = current->next;
        current->next = NULL;
        SourceEmpty(source);
        SourceClose(source);
        free(current);
    }
    ho_inst->device_list = NULL;

    return HANDOVER_PROFILE_STATUS_SUCCESS;
}

static handover_profile_status_t handoverProfile_PrimarySetup(void)
{
    handover_profile_task_data_t *ho_inst = Handover_GetTaskData();
    cm_connection_iterator_t iterator;
    tp_bdaddr addr;

    /* Clean any stale state */
    handoverProfile_PrimaryCleanup();

    /* Note that the order of devices in the device_list is used to set the
       order of devices in the start request protocol message and the order in
       which devices are prepared and marshalled/unmarshalled during handover
       procedure.
    */

    if (ConManager_IterateFirstActiveConnection(&iterator, &addr))
    {
        do
        {
            if (!BdaddrIsSame(&addr.taddr.addr, &ho_inst->peer_addr))
            {

                handover_device_t *device = PanicUnlessMalloc(sizeof(*device));
                memset(device, 0, sizeof(*device));
                device->next = ho_inst->device_list;
                device->addr = addr;
                device->focused = BdaddrIsSame(MirrorProfile_GetMirroredDeviceAddress(), &addr.taddr.addr);
                ho_inst->device_list = device;
            }
        } while (ConManager_IterateNextActiveConnection(&iterator, &addr));
    }

    SourceEmpty(ho_inst->link_source);
    TimestampEvent(TIMESTAMP_EVENT_PRI_HANDOVER_STARTED);

    return HANDOVER_PROFILE_STATUS_SUCCESS;
}

static handover_profile_status_t handoverProfile_PrimarySelfVeto(void)
{
    handover_profile_task_data_t *ho_inst = Handover_GetTaskData();

    switch (ho_inst->secondary_firmware)
    {
        case HANDOVER_PROFILE_SECONDARY_FIRMWARE_UNKNOWN:
            DEBUG_LOG_INFO("handoverProfile_PrimarySelfVeto secondary firmware unknown - veto");
            return HANDOVER_PROFILE_STATUS_HANDOVER_VETOED;

        case HANDOVER_PROFILE_SECONDARY_FIRMWARE_MISMATCHED:
            DEBUG_LOG_INFO("handoverProfile_PrimarySelfVeto secondary firmware mismatched");
            return HANDOVER_PROFILE_STATUS_HANDOVER_FAILURE;

        default:
            break;
    }

    FOR_EACH_HANDOVER_DEVICE(device)
    {
        /*! Handover of LE devices is not supported, all LE links should be
            disconnect before attempting handover. */
        if (device->addr.transport == TRANSPORT_BLE_ACL)
        {
            DEBUG_LOG_INFO("handoverProfile_PrimarySelfVeto unexpected LE ACL");
            return HANDOVER_PROFILE_STATUS_HANDOVER_VETOED;
        }
    }
    return HANDOVER_PROFILE_STATUS_SUCCESS;
}

static handover_profile_status_t handoverProfile_PrimaryPerformanceRequest(void)
{
    appPowerPerformanceProfileRequest();
    return HANDOVER_PROFILE_STATUS_SUCCESS;
}

static handover_profile_status_t handoverProfile_PrimaryPerformanceRelinquish(void)
{
    appPowerPerformanceProfileRelinquish();
    return HANDOVER_PROFILE_STATUS_SUCCESS;
}

/* Check if all outbound data has been transmitted */
static bool handoverProfile_IsAclTransmitPending (const tp_bdaddr *addr, uint32 t)
{
    uint32 timeout = 0;
    bool timedout = TRUE;
    timeout = VmGetClock() + t;
    do
    {
        if(!AclTransmitDataPending(addr))
        {
            timedout = FALSE;
            break;
        }
    } while(VmGetClock() < timeout);

    return timedout;
}

static handover_profile_status_t handoverProfile_HaltLink(const tp_bdaddr *addr)
{
    if (AclReceiveEnable(addr, FALSE, HANDOVER_PROFILE_ACL_RECEIVE_ENABLE_TIMEOUT_USEC))
    {
        if (AclReceivedDataProcessed(addr, HANDOVER_PROFILE_ACL_RECEIVED_DATA_PROCESSED_TIMEOUT_USEC) == ACL_RECEIVE_DATA_PROCESSED_COMPLETE)
        {
            if (!handoverProfile_IsAclTransmitPending(addr, HANDOVER_PROFILE_ACL_TRANSMIT_DATA_PENDING_TIMEOUT_MSEC))
            {
                return HANDOVER_PROFILE_STATUS_SUCCESS;
            }
            else
            {
                DEBUG_LOG_INFO("handoverProfile_HaltDevice AclTransmitDataPending timeout/failed");
            }
        }
        else
        {
            DEBUG_LOG_INFO("handoverProfile_HaltDevice AclReceivedDataProcessed timeout/failed");
        }
    }
    else
    {
        DEBUG_LOG_INFO("handoverProfile_HaltDevice AclReceiveEnable(false) timeout/failed");
    }
    return HANDOVER_PROFILE_STATUS_HANDOVER_TIMEOUT;
}

static handover_profile_status_t handoverProfile_ResumeLink(const tp_bdaddr *addr)
{
    if (!AclReceiveEnable(addr, TRUE, HANDOVER_PROFILE_ACL_RECEIVE_ENABLE_TIMEOUT_USEC))
    {
        /* Ignore failure to resume as a link. */
        DEBUG_LOG_INFO("handoverProfile_ResumeLink AclReceiveEnable timeout/failed");
    }
    return HANDOVER_PROFILE_STATUS_SUCCESS;
}


static handover_profile_status_t handoverProfile_PrepareLink(handover_device_t *device)
{
    lp_power_mode mode;
    uint32 timeout = 0;
    bool timedout = TRUE;
    tp_bdaddr tp_peer_addr;
    handover_profile_task_data_t *ho_inst = Handover_GetTaskData();

    PanicFalse(ConManagerGetPowerMode(&device->addr, &mode));
    if (mode == lp_sniff)
    {
        uint16 sniff_slots;
        PanicFalse(ConManagerGetSniffInterval(&device->addr, &sniff_slots));
        timeout = VmGetClock() + (rtime_get_sniff_interval_in_ms(sniff_slots) * HANDOVER_PROFILE_NO_OF_TIMES_SNIFF_INTERVAL);
    }
    else
    {
        timeout = VmGetClock() + HANDOVER_PROFILE_ACL_HANDOVER_PREPARE_TIMEOUT_MSEC;
    }

    BdaddrTpFromBredrBdaddr(&tp_peer_addr, &ho_inst->peer_addr);
    do
    {
        if ((device->handle = AclHandoverPrepare(&device->addr, (const tp_bdaddr *)&tp_peer_addr)) != 0xFFFF)
        {
            acl_handover_prepare_status prepared_status;

             /* Wait until AclHandoverPrepared returns ACL_HANDOVER_PREPARE_COMPLETE or ACL_HANDOVER_PREPARE_FAILED */
            while ((prepared_status = AclHandoverPrepared(device->handle)) == ACL_HANDOVER_PREPARE_IN_PROGRESS)
            {
                /* Do nothing */
            }

            /* Exit loop if handover prepare completed */
            if (prepared_status == ACL_HANDOVER_PREPARE_COMPLETE)
            {
                device->u.p.btstack_source = StreamAclMarshalSource(&device->addr);
                /* Kick the source to marshal the upper stack data */
                SourceDrop(device->u.p.btstack_source, 0);
                device->u.p.btstack_data_len = SourceSize(device->u.p.btstack_source);
                timedout = FALSE;
                break;
            }
        }
    } while (VmGetClock() < timeout);

    /* AclHandoverPrepare didn't succeed equivalent to veto */
    if (device->handle == 0xFFFF)
    {
        DEBUG_LOG_INFO("handoverProfile_PrepareLink vetoed");
        return HANDOVER_PROFILE_STATUS_HANDOVER_VETOED;
    }

    if (timedout)
    {
        DEBUG_LOG_INFO("handoverProfile_PrepareLink timedout");
        return HANDOVER_PROFILE_STATUS_HANDOVER_TIMEOUT;
    }
    return HANDOVER_PROFILE_STATUS_SUCCESS;
}

static handover_profile_status_t handoverProfile_CancelPrepareLink(handover_device_t *device)
{
    if (device->handle != 0xffff)
    {
        if (!AclHandoverCancel(device->handle))
        {
            /* Ignore failure to cancel prepare */
            DEBUG_LOG_INFO("handoverProfile_CancelPrepareLink failed");
        }
    }
    return HANDOVER_PROFILE_STATUS_SUCCESS;
}

static handover_profile_status_t handoverProfile_HaltOrResumeBredrLinks(bool halt, bool focused)
{
    handover_profile_status_t result = HANDOVER_PROFILE_STATUS_SUCCESS;

    FOR_EACH_HANDOVER_DEVICE(device)
    {
        if (device->focused == focused)
        {
            DEBUG_LOG_INFO("handoverProfile_HaltBredrLinks lap:0x%x", device->addr.taddr.addr.lap);
            result = halt ? handoverProfile_HaltLink(&device->addr) : handoverProfile_ResumeLink(&device->addr);
            if (result != HANDOVER_PROFILE_STATUS_SUCCESS)
            {
                break;
            }
        }
    }
    return result;
}

static handover_profile_status_t handoverProfile_PrepareBredrLinks(bool prepare, bool focused)
{
    handover_profile_status_t result = HANDOVER_PROFILE_STATUS_SUCCESS;

    FOR_EACH_HANDOVER_DEVICE(device)
    {
        if (device->focused == focused)
        {
            DEBUG_LOG_INFO("handoverProfile_PrepareBredrLinks lap:0x%x", device->addr.taddr.addr.lap);
            if (prepare)
            {
                result = handoverProfile_PrepareLink(device);
            }
            else
            {
                result = handoverProfile_CancelPrepareLink(device);
            }
            if (result != HANDOVER_PROFILE_STATUS_SUCCESS)
            {
                break;
            }
        }
    }
    return result;
}

static handover_profile_status_t handoverProfile_HaltInactiveBredrLinks(void)
{
    return handoverProfile_HaltOrResumeBredrLinks(TRUE, FALSE);
}

static handover_profile_status_t handoverProfile_HaltActiveBredrLinks(void)
{
    return handoverProfile_HaltOrResumeBredrLinks(TRUE, TRUE);
}

static handover_profile_status_t handoverProfile_ResumeInactiveBredrLinks(void)
{
    return handoverProfile_HaltOrResumeBredrLinks(FALSE, FALSE);
}

static handover_profile_status_t handoverProfile_ResumeActiveBredrLinks(void)
{
    return handoverProfile_HaltOrResumeBredrLinks(FALSE, TRUE);
}

static handover_profile_status_t handoverProfile_PrepareInactiveBredrLinks(void)
{
    return handoverProfile_PrepareBredrLinks(TRUE, FALSE);
}

static handover_profile_status_t handoverProfile_PrepareActiveBredrLinks(void)
{
    return handoverProfile_PrepareBredrLinks(TRUE, TRUE);
}

static handover_profile_status_t handoverProfile_CancelPrepareInactiveBredrLinks(void)
{
    return handoverProfile_PrepareBredrLinks(FALSE, FALSE);
}

static handover_profile_status_t handoverProfile_CancelPrepareActiveBredrLinks(void)
{
    return handoverProfile_PrepareBredrLinks(FALSE, TRUE);
}

/*! \brief Wait for a packet to be processed by the transform connecting the
           A2DP media source to the audio subsystem

    \note If A2DP is streaming, the effective handover time can be reduced by
    starting handover immediately after a packet is received from the handset. A
    proportion of the handover time will then occur in the gap before the next
    packet. This increases the overall handover time from the perspective of the
    procedure that initiates the handover (since the software waits for a packet
    before even starting to handover), but reduces the chance of there being a
    audio glitch as the packet that is received can be decoded and rendered
    whilst the handover is performed.
*/
static handover_profile_status_t handoverProfile_WaitForA2dpPacket(void)
{
    Transform trans;

    HandoverPioSet();

    trans =  Kymera_GetA2dpMediaStreamTransform();
    if (trans && HANDOVER_PROFILE_A2DP_HANDOVER_WAIT_FOR_PACKET_TIMEOUT_MS)
    {
        uint32 timeout = VmGetClock() + HANDOVER_PROFILE_A2DP_HANDOVER_WAIT_FOR_PACKET_TIMEOUT_MS;

        /* Read once to clear flag */
        TransformPollTraffic(trans);

        while (VmGetClock() < timeout)
        {
            if (TransformPollTraffic(trans))
            {
                DEBUG_LOG("handoverProfile_WaitForPacket received packet");
                break;
            }
        }
    }
    HandoverPioClr();

    return HANDOVER_PROFILE_STATUS_SUCCESS;
}

static handover_profile_status_t handoverProfile_SetPeerLinkActiveMode(void)
{
    /* For A2DP handover the peer link policy may be changed to active mode at
       the same time as the controller prepares for handover. This reduces the
       handover time. For other handover types the link policy must be changed
       before the controller prepares for handover.
    */
    if (MirrorProfile_IsA2dpActive())
    {
        MirrorProfile_UpdatePeerLinkPolicy(lp_active);
    }
    else
    {
        if(!MirrorProfile_UpdatePeerLinkPolicyBlocking(lp_active, HANDOVER_PROFILE_EXIT_SNIFF_TIMEOUT_MSEC))
        {
            DEBUG_LOG_INFO("handoverProfile_SetPeerLinkActiveMode Could not exit sniff mode");
            return HANDOVER_PROFILE_STATUS_HANDOVER_TIMEOUT;
        }
    }
    return HANDOVER_PROFILE_STATUS_SUCCESS;
}

static handover_profile_status_t handoverProfile_SetPeerLinkSniffMode(void)
{
    if (MirrorProfile_IsA2dpActive())
    {
        /* For A2DP handover, first wait for the link to go active as it was previously
           set into sniff mode, but did not wait for the link mode to change */
        if(!MirrorProfile_WaitForPeerLinkMode(lp_active, HANDOVER_PROFILE_EXIT_SNIFF_TIMEOUT_MSEC))
        {
            /* Ignore failure */
            DEBUG_LOG_INFO("handoverProfile_SetPeerLinkSniffMode timeout waiting to enter active mode");
        }
    }

    if(!MirrorProfile_UpdatePeerLinkPolicyBlocking(lp_sniff, HANDOVER_PROFILE_REENTER_SNIFF_TIMEOUT_MSEC))
    {
        /* Ignore failure */
        DEBUG_LOG_INFO("handoverProfile_SetPeerLinkSniffMode timeout wait to re-enter sniff mode");
    }
    return HANDOVER_PROFILE_STATUS_SUCCESS;
}

static handover_profile_status_t handoverProfile_WaitForPeerLinkActiveMode(void)
{
    if(!MirrorProfile_WaitForPeerLinkMode(lp_active, HANDOVER_PROFILE_EXIT_SNIFF_TIMEOUT_MSEC))
    {
        DEBUG_LOG_INFO("HandoverProfile_PrepareForMarshal Could not exit sniff mode");
        return HANDOVER_PROFILE_STATUS_HANDOVER_TIMEOUT;
    }
    MirrorProfile_HandoverRefreshSubrate();
    return HANDOVER_PROFILE_STATUS_SUCCESS;
}

static handover_profile_status_t handoverProfile_SendInactiveLinkMarshalData(void)
{
    return handoverProtocol_SendBtStackMarshalData(FALSE);
}

static handover_profile_status_t handoverProfile_SendActiveLinkMarshalData(void)
{
    return handoverProtocol_SendBtStackMarshalData(TRUE);
}

static handover_profile_status_t handoverProfile_ClearPendingPeerData(void)
{
    handover_profile_task_data_t *ho_inst = Handover_GetTaskData();
    tp_bdaddr peer_tp_addr;

    BdaddrTpFromBredrBdaddr(&peer_tp_addr, &ho_inst->peer_addr);

    if (handoverProfile_IsAclTransmitPending(&peer_tp_addr, HANDOVER_PROFILE_P0_TRANSMIT_DATA_PENDING_TIMEOUT_MSEC))
    {
        DEBUG_LOG_INFO("handoverProfile_ClearPendingPeerData timeout");
        return HANDOVER_PROFILE_STATUS_HANDOVER_FAILURE;
    }
    else
    {
        return HANDOVER_PROFILE_STATUS_SUCCESS;
    }
}

static handover_profile_status_t handoverProfile_CommitSecondaryRole(void)
{
    handover_profile_task_data_t *ho_inst = Handover_GetTaskData();
    const bool secondary_role = FALSE;

    FOR_EACH_HANDOVER_DEVICE(device)
    {
        /* Commit P1 Clients */
        handoverProfile_CommitP1Clients(&device->addr, secondary_role);

        if (device->focused)
        {
            /* Commit BT stack only for focused/mirrored device */
            PanicFalse(AclHandoverCommit(device->handle));
        }
    }

    if (!MirrorProfile_WaitForPeerLinkMode(lp_sniff, HANDOVER_PROFILE_REENTER_SNIFF_TIMEOUT_MSEC))
    {
        DEBUG_LOG_INFO("handoverProfile_CommitSecondaryRole timeout waiting to re-enter sniff mode");
    }

    TimestampEvent(TIMESTAMP_EVENT_PRI_HANDOVER_COMPLETED);

    /* Call P1 complete() */
    handoverProfile_CompleteP1Clients(secondary_role);

    /* Update the new peer address */
    PanicFalse(appDeviceGetPeerBdAddr(&ho_inst->peer_addr));
    ho_inst->is_primary = secondary_role;

    DEBUG_LOG_INFO("handoverProfile_CommitSecondaryRole: I am new Secondary");

    return HANDOVER_PROFILE_STATUS_SUCCESS;
}

static handover_profile_status_t handoverProfile_Panic(void)
{
    Panic();
    return HANDOVER_PROFILE_STATUS_HANDOVER_FAILURE;
}

#endif

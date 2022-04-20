/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Peer earbud link policy control.
*/

#ifdef INCLUDE_MIRRORING

#include <app/bluestack/dm_prim.h>
#include <connection_manager.h>
#include <panic.h>
#include <message.h>
#include <qualcomm_connection_manager.h>

#include "mirror_profile_private.h"

/*! The link between the earbuds should remains in sniff mode at all times
    when any form of mirroring is active (with two exceptions). Sniff subrating
    is used to adjust the interval at which the earbuds communicate during
    A2DP/eSCO mirroring and when idle.
    There are two excpetional cases where the link between buds is active:
    1. When starting eSCO mirroring. Moving to active mode speeds up the creation
       of the eSCO mirror. The link is immediately put back into sniff mode once
       the eSCO mirror is created.
    2. When starting A2DP mirroring. Moving to active mode speeds up the creation
       of the A2DP mirror. The link is immediately put back into sniff mode once
       the A2DP mirror is created.
*/
#define SNIFF_INVERVAL 26
#define SNIFF_ATTEMPT 2
#define SNIFF_TIMEOUT 2

/*! When idle (not A2DP or eSCO mirroring), subrating is used to reduce power
    consumption when no data is being transferred. The subrating is asymmetric.
    The primary allows the secondary to subrate to 3x the sniff interval whereas
    the secondary allows the primary to subrate to 6x the sniff interval. This
    asymmetry results in a lower latency in re-establishing communication when
    becoming active - for example the A2DP restarts and primary needs to restart
    A2DP mirroring. This cost of the lower latency reconnection is a small power
    increase on the secondary. If power consumption is more important than
    reconnection latency, then the primary remote latency may be increased to
    6x.

    The role of the earbud ACL is not important during mirroring and during
    handover the role is not switched. However, when moving to the idle subrate,
    its systematically more power efficient and balanced for the primary to be
    master of the ACL.

    Therefore the ACL between the earbuds is role switch before moving to the
    idle subrate so the primary earbud is master of the ACL. The role switch
    may be disabled using the DISABLE_MIRROR_IDLE_ROLE_SWITCH.
*/
#define SNIFF_SUBRATE_IDLE_PRI_REMOTE_LATENCY 6
#define SNIFF_SUBRATE_IDLE_SEC_REMOTE_LATENCY 12
#define SNIFF_SUBRATE_TIMEOUT_IDLE 20

/*! Subrating is disabled completely when eSCO is active */
#define SNIFF_SUBRATE_ESCO_ACTIVE 2
#define SNIFF_SUBRATE_TIMEOUT_ESCO_ACTIVE 0


/*! Subrating is enabled during A2DP mirroring to minimise impact on mirroring
    whilst allowing reasonable data transfer latency (e.g. for audio sync messages). */
#define SNIFF_SUBRATE_A2DP_ACTIVE 4
/*! If the earbud connection supports fast sniff subrate exit the subrate can
    be increased further without impacting latency as the BT controllers uses the
    mirroring packets to indicate when either device has data to exit subrate */
#define SNIFF_SUBRATE_A2DP_ACTIVE_WITH_FAST_SUBRATE_EXIT 12
#define SNIFF_SUBRATE_TIMEOUT_A2DP_ACTIVE 0

/* Subrate parameters to use when transitioning between states or before entering
   idle. */
#define SNIFF_SUBRATE_TRANSITIONING 1
#define SNIFF_SUBRATE_TIMEOUT_TRANSITIONING 100

#define MAKE_PRIM_C(TYPE) MESSAGE_MAKE(prim,TYPE##_T); prim->common.op_code = TYPE; prim->common.length = sizeof(TYPE##_T);

#define SNIFF_MODE_REQ_COMMON { DM_HCI_SNIFF_MODE_REQ, sizeof(DM_HCI_SNIFF_MODE_REQ_T) }

/* The number of sniff instances to account for when computing expected peer link transmission time. */
#define NUM_SNIFF_INSTANCES 4

/*! \brief Subset of the DM_HCI_SNIFF_SUB_RATE_REQ_T parameters */
typedef struct
{
    uint16 max_remote_latency;
    uint16 min_remote_timeout;
    uint16 min_local_timeout;
} subrate_params_t;

static void mirrorProfile_PeerEnterSniff(const DM_HCI_SNIFF_MODE_REQ_T *params)
{
    MESSAGE_MAKE(prim, DM_HCI_SNIFF_MODE_REQ_T);
    /* Set subrate policy before entering sniff */
    MirrorProfile_PeerLinkPolicyInit();
    *prim = *params;
    BdaddrConvertVmToBluestack(&prim->bd_addr, &MirrorProfile_GetAudioSyncL2capState()->peer_addr);
    VmSendDmPrim(prim);
    MIRROR_LOG("mirrorProfile_PeerEnterSniff min:%d max:%d attempt:%d timeout:%d", params->min_interval,
                    params->min_interval, params->attempt, params->timeout);
}

static void mirrorProfile_PeerExitSniff(void)
{
    MAKE_PRIM_C(DM_HCI_EXIT_SNIFF_MODE_REQ);
    BdaddrConvertVmToBluestack(&prim->bd_addr, &MirrorProfile_GetAudioSyncL2capState()->peer_addr);
    VmSendDmPrim(prim);
    MIRROR_LOG("mirrorProfile_PeerExitSniff");
}

static void mirrorProfile_PeerSniffSubrate(const subrate_params_t *params)
{
    MESSAGE_MAKE(prim, DM_HCI_SNIFF_SUB_RATE_REQ_T);
    prim->common.op_code = DM_HCI_SNIFF_SUB_RATE_REQ;
    prim->common.length = sizeof(DM_HCI_SNIFF_SUB_RATE_REQ_T);
    prim->max_remote_latency = params->max_remote_latency;
    prim->min_remote_timeout = params->min_remote_timeout;
    prim->min_local_timeout = params->min_local_timeout;
    BdaddrConvertVmToBluestack(&prim->bd_addr, &MirrorProfile_GetAudioSyncL2capState()->peer_addr);
    VmSendDmPrim(prim);
    MIRROR_LOG("mirrorProfile_PeerSniffSubrate maxrl:%d minrt:%d minlt:%d",
                params->max_remote_latency, params->min_remote_timeout, params->min_local_timeout);
}

static const subrate_params_t sniff_sub_rate_req_idle_pri = {
    .max_remote_latency = SNIFF_SUBRATE_IDLE_PRI_REMOTE_LATENCY * SNIFF_INVERVAL,
    .min_remote_timeout = SNIFF_SUBRATE_TIMEOUT_IDLE * SNIFF_INVERVAL,
    .min_local_timeout = SNIFF_SUBRATE_TIMEOUT_IDLE * SNIFF_INVERVAL,
};

static const subrate_params_t sniff_sub_rate_req_idle_sec = {
    .max_remote_latency = SNIFF_SUBRATE_IDLE_SEC_REMOTE_LATENCY * SNIFF_INVERVAL,
    .min_remote_timeout = SNIFF_SUBRATE_TIMEOUT_IDLE * SNIFF_INVERVAL,
    .min_local_timeout = SNIFF_SUBRATE_TIMEOUT_IDLE * SNIFF_INVERVAL,
};

static const subrate_params_t sniff_sub_rate_req_transitioning = {
    .max_remote_latency = SNIFF_SUBRATE_TRANSITIONING * SNIFF_INVERVAL,
    .min_remote_timeout = SNIFF_SUBRATE_TIMEOUT_TRANSITIONING * SNIFF_INVERVAL,
    .min_local_timeout = SNIFF_SUBRATE_TIMEOUT_TRANSITIONING * SNIFF_INVERVAL,
};

static const subrate_params_t sniff_sub_rate_req_a2dp_active = {
    .max_remote_latency = SNIFF_SUBRATE_A2DP_ACTIVE * SNIFF_INVERVAL,
    .min_remote_timeout = SNIFF_SUBRATE_TIMEOUT_A2DP_ACTIVE * SNIFF_INVERVAL,
    .min_local_timeout = SNIFF_SUBRATE_TIMEOUT_A2DP_ACTIVE * SNIFF_INVERVAL,
};

static const subrate_params_t sniff_sub_rate_req_a2dp_active_fast_exit_subrate = {
    .max_remote_latency = SNIFF_SUBRATE_A2DP_ACTIVE_WITH_FAST_SUBRATE_EXIT * SNIFF_INVERVAL,
    .min_remote_timeout = SNIFF_SUBRATE_TIMEOUT_A2DP_ACTIVE * SNIFF_INVERVAL,
    .min_local_timeout = SNIFF_SUBRATE_TIMEOUT_A2DP_ACTIVE * SNIFF_INVERVAL,
};

static const subrate_params_t sniff_sub_rate_req_esco_active = {
    .max_remote_latency = SNIFF_SUBRATE_ESCO_ACTIVE * SNIFF_INVERVAL,
    .min_remote_timeout = SNIFF_SUBRATE_TIMEOUT_ESCO_ACTIVE * SNIFF_INVERVAL,
    .min_local_timeout = SNIFF_SUBRATE_TIMEOUT_ESCO_ACTIVE * SNIFF_INVERVAL,
};


static const DM_HCI_SNIFF_MODE_REQ_T sniff_mode_req = {
    .common       = SNIFF_MODE_REQ_COMMON,
    .max_interval = SNIFF_INVERVAL,
    .min_interval = SNIFF_INVERVAL,
    .attempt      = SNIFF_ATTEMPT,
    .timeout      = SNIFF_TIMEOUT,
};

void MirrorProfile_SendLinkPolicyTimeout(void)
{
    MessageSendLater(MirrorProfile_GetTask(), MIRROR_INTERNAL_PEER_LINK_POLICY_IDLE_TIMEOUT,
                     NULL, mirrorProfileConfig_LinkPolicyIdleTimeout());
}

void MirrorProfile_PeerLinkPolicyInit(void)
{
    if (!MirrorProfile_IsLinkPolicyInitialised())
    {
        MIRROR_LOG("MirrorProfile_PeerLinkPolicyInit");
        /* Start with lowest latency subrating parameters */
        mirrorProfile_PeerSniffSubrate(&sniff_sub_rate_req_transitioning);
        MirrorProfile_Get()->peer_lp_mode = PEER_LP_TRANSITIONING;
        MessageCancelFirst(MirrorProfile_GetTask(), MIRROR_INTERNAL_PEER_LINK_POLICY_IDLE_TIMEOUT);
        /* Start in active mode, then move to idle on timeout */
        MirrorProfile_SendLinkPolicyTimeout();
        MirrorProfile_SetLinkPolicyInitialised();
    }
}

void MirrorProfile_PeerLinkPolicyTransitioning(void)
{
    MessageCancelFirst(MirrorProfile_GetTask(), MIRROR_INTERNAL_PEER_LINK_POLICY_IDLE_TIMEOUT);

    if (MirrorProfile_IsAudioSyncL2capConnected() &&
        MirrorProfile_Get()->peer_lp_mode != PEER_LP_TRANSITIONING)
    {
        mirrorProfile_PeerSniffSubrate(&sniff_sub_rate_req_transitioning);
        MirrorProfile_Get()->peer_lp_mode = PEER_LP_TRANSITIONING;
    }
}

void MirrorProfile_PeerLinkPolicySetA2dpActive(void)
{
    MessageCancelFirst(MirrorProfile_GetTask(), MIRROR_INTERNAL_PEER_LINK_POLICY_IDLE_TIMEOUT);

    if (MirrorProfile_IsAudioSyncL2capConnected() &&
        MirrorProfile_Get()->peer_lp_mode != PEER_LP_A2DP)
    {
        const subrate_params_t *params;
        const bdaddr *peer_addr = &MirrorProfile_GetAudioSyncL2capState()->peer_addr;
        params = QcomConManagerIsFastExitSniffSubrateSupported(peer_addr) ?
                    &sniff_sub_rate_req_a2dp_active_fast_exit_subrate :
                    &sniff_sub_rate_req_a2dp_active;
        mirrorProfile_PeerSniffSubrate(params);
        MirrorProfile_Get()->peer_lp_mode = PEER_LP_A2DP;
    }
}

void MirrorProfile_PeerLinkPolicySetEscoActive(void)
{
    MessageCancelFirst(MirrorProfile_GetTask(), MIRROR_INTERNAL_PEER_LINK_POLICY_IDLE_TIMEOUT);

    if (MirrorProfile_IsAudioSyncL2capConnected() &&
        MirrorProfile_Get()->peer_lp_mode != PEER_LP_ESCO)
    {
        mirrorProfile_PeerSniffSubrate(&sniff_sub_rate_req_esco_active);
        MirrorProfile_Get()->peer_lp_mode = PEER_LP_ESCO;
    }
}

void MirrorProfile_PeerLinkPolicySetIdle(void)
{
    MessageCancelFirst(MirrorProfile_GetTask(), MIRROR_INTERNAL_PEER_LINK_POLICY_IDLE_TIMEOUT);

    if (MirrorProfile_IsAudioSyncL2capConnected() &&
        MirrorProfile_Get()->peer_lp_mode != PEER_LP_IDLE)
    {
        /* Move back to most agressive active parameters before timeout, then revert
           to idle parameters */
        MirrorProfile_PeerLinkPolicyTransitioning();
        MirrorProfile_SendLinkPolicyTimeout();
    }
}


void MirrorProfile_PeerLinkPolicyHandleIdleTimeout(void)
{
    if (MirrorProfile_IsAudioSyncL2capConnected())
    {
#ifndef DISABLE_MIRROR_IDLE_ROLE_SWITCH
        tp_bdaddr peer_address;
        BdaddrTpFromBredrBdaddr(&peer_address, &MirrorProfile_GetAudioSyncL2capState()->peer_addr);
#endif

        if (MirrorProfile_IsPrimary())
        {
#ifndef DISABLE_MIRROR_IDLE_ROLE_SWITCH
            if (VmGetAclRole(&peer_address) == HCI_SLAVE)
            {
                ConnectionSetRoleBdaddr(MirrorProfile_GetTask(),
                                        &MirrorProfile_GetAudioSyncL2capState()->peer_addr,
                                        hci_role_master);                                     
                MirrorProfile_SetPeerRoleSwitching();
                MIRROR_LOG("MirrorProfile_PeerLinkPolicyHandleIdleTimeout primary role switching");
            }
            else
#endif
            {
                MIRROR_LOG("MirrorProfile_PeerLinkPolicyHandleIdleTimeout primary idle subrating");
                mirrorProfile_PeerSniffSubrate(&sniff_sub_rate_req_idle_pri);
                MirrorProfile_ClearPeerRoleSwitching();
                MirrorProfile_Get()->peer_lp_mode = PEER_LP_IDLE;
            }
        }
        else
        {
#ifndef DISABLE_MIRROR_IDLE_ROLE_SWITCH
            if (VmGetAclRole(&peer_address) != HCI_SLAVE)
            {
                MIRROR_LOG("MirrorProfile_PeerLinkPolicyHandleIdleTimeout secondary waiting for role switch");
            }
            else
#endif
            {
                MIRROR_LOG("MirrorProfile_PeerLinkPolicyHandleIdleTimeout secondary idle subrating");
                mirrorProfile_PeerSniffSubrate(&sniff_sub_rate_req_idle_sec);
                MirrorProfile_Get()->peer_lp_mode = PEER_LP_IDLE;
            }
        }
    }
}

void MirrorProfile_UpdatePeerLinkPolicy(lp_power_mode mode)
{
    switch (mode)
    {
        case lp_sniff:
            mirrorProfile_PeerEnterSniff(&sniff_mode_req);
        break;
        case lp_active:
            mirrorProfile_PeerExitSniff();
        break;
        case lp_passive:
        break;
        default:
        break;
    }
}

void MirrorProfile_HandoverRefreshSubrate(void)
{
    if (MirrorProfile_Get()->peer_lp_mode == PEER_LP_IDLE)
    {
        MirrorProfile_ClearLinkPolicyInitialised();
        MirrorProfile_PeerLinkPolicyInit();
    }
}

void MirrorProfile_UpdatePeerLinkPolicyDfu(lp_power_mode mode)
{
    MessageCancelFirst(MirrorProfile_GetTask(), MIRROR_INTERNAL_PEER_ENTER_SNIFF);
    MirrorProfile_UpdatePeerLinkPolicy(mode);
}

bool MirrorProfile_WaitForPeerLinkMode(lp_power_mode mode, uint32 timeout_ms)
{
    tp_bdaddr tpbdaddr;
    uint32 timeout;

    BdaddrTpFromBredrBdaddr(&tpbdaddr, &MirrorProfile_GetAudioSyncL2capState()->peer_addr);
    timeout = VmGetClock() + timeout_ms;
    do
    {
        lp_power_mode current_mode = lp_active;

        switch (VmGetAclMode(&tpbdaddr))
        {
            case HCI_BT_MODE_ACTIVE:
                current_mode = lp_active;
                break;

            case HCI_BT_MODE_SNIFF:
                current_mode = lp_sniff;
                break;

            case HCI_BT_MODE_MAX:
                /* Connection no longer exists */
                MIRROR_LOG("MirrorProfile_WaitForPeerLinkMode link lost");
                return FALSE;

            default:
                Panic();
                break;
        }

        if (current_mode == mode)
        {
            return TRUE;
        }

    } while(VmGetClock() < timeout);

    return FALSE;
}

bool MirrorProfile_UpdatePeerLinkPolicyBlocking(lp_power_mode mode, uint32 timeout_ms)
{
    MirrorProfile_UpdatePeerLinkPolicy(mode);
    return MirrorProfile_WaitForPeerLinkMode(mode, timeout_ms);
}


void mirrorProfilePeerMode_HandleDmSniffSubRatingInd(const CL_DM_SNIFF_SUB_RATING_IND_T *ind)
{
    if (appDeviceIsPeer(&ind->bd_addr))
    {
        DEBUG_LOG("mirrorProfilePeerMode_HandleDmSniffSubRatingInd enum:hci_status:%d,tl=%u,rl=%u,rt=%u,lt=%u",
            ind->status, ind->transmit_latency, ind->receive_latency, ind->remote_timeout, ind->local_timeout);

        if (MirrorProfile_IsPrimary())
        {
            if (MirrorProfile_IsA2dpConnected())
            {
                MirrorProfile_PeerLinkPolicySetA2dpActive();
            }
        }
    }
}

uint32 MirrorProfilePeerLinkPolicy_GetExpectedTransmissionTime(void)
{
    uint32 peer_relay_time_usecs = 0;
    switch (MirrorProfile_Get()->peer_lp_mode)
    {
    case PEER_LP_IDLE:
        if (MirrorProfile_IsPrimary())
        {
            peer_relay_time_usecs = SNIFF_INTERVAL_US(SNIFF_SUBRATE_IDLE_PRI_REMOTE_LATENCY * SNIFF_INVERVAL * NUM_SNIFF_INSTANCES);
        }
        else
        {
            peer_relay_time_usecs = SNIFF_INTERVAL_US(SNIFF_SUBRATE_IDLE_SEC_REMOTE_LATENCY * SNIFF_INVERVAL * NUM_SNIFF_INSTANCES);
        }
        break;
    case PEER_LP_A2DP:
        peer_relay_time_usecs = SNIFF_INTERVAL_US(SNIFF_SUBRATE_A2DP_ACTIVE * SNIFF_INVERVAL * NUM_SNIFF_INSTANCES);
        break;
    case PEER_LP_ESCO:
        peer_relay_time_usecs = SNIFF_INTERVAL_US(SNIFF_SUBRATE_ESCO_ACTIVE * SNIFF_INVERVAL * NUM_SNIFF_INSTANCES);
        break;
    case PEER_LP_TRANSITIONING:
        peer_relay_time_usecs = SNIFF_INTERVAL_US(SNIFF_SUBRATE_TRANSITIONING * SNIFF_INVERVAL * NUM_SNIFF_INSTANCES);
        break;
    default:
        Panic();
        break;
    }
    return peer_relay_time_usecs;
}

bool MirrorProfile_PeerLinkPolicyLowLatencyKick(void)
{
    bool policy_changed = FALSE;

    DEBUG_LOG("MirrorProfile_PeerLinkPolicyLowLatencyKick");
    if(MirrorProfile_Get()->peer_lp_mode == PEER_LP_IDLE)
    {
        MirrorProfile_ClearLinkPolicyInitialised();
        MirrorProfile_PeerLinkPolicyInit();
        policy_changed = TRUE;
    }
    else
    {
        if(MessageCancelFirst(MirrorProfile_GetTask(), MIRROR_INTERNAL_PEER_LINK_POLICY_IDLE_TIMEOUT))
        {
            MirrorProfile_SendLinkPolicyTimeout();
        }
    }

    return policy_changed;
}

#endif

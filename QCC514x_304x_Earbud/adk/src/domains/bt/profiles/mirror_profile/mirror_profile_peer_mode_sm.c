/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      State machine to control peer mode (sniff or active)
*/

#ifdef INCLUDE_MIRRORING

#include "mirror_profile_private.h"
#include "mirror_profile_sm.h"
#include "mirror_profile_peer_mode_sm.h"
#include "bt_device.h"
#include "kymera.h"
#include "message.h"

static mirror_profile_peer_mode_state_t mirrorProfilePeerMode_SmTransition(void);
static void mirrorProfilePeerMode_SmKick(void);
static void mirrorProfilePeerMode_SetState(mirror_profile_peer_mode_state_t new_state);

static void mirrorProfilePeerMode_HandleDmModeChangeEvent(bdaddr* bd_addr, uint8 mode);
static void mirrorProfilePeerMode_HandleDmRoleInd(bdaddr* bd_addr, hci_role role, hci_status success);
static void mirrorProfilePeerMode_HandleDmRoleCfm(bdaddr* bd_addr, hci_role role, hci_status success);

void mirrorProfile_HandleTpConManagerDisconnectInd(const CON_MANAGER_TP_DISCONNECT_IND_T *ind)
{
    DEBUG_LOG("mirrorProfile_HandleTpConManagerDisconnectInd");
    if (appDeviceIsPeer(&ind->tpaddr.taddr.addr))
    {
       /* If peer disconnects, reset the state */
        MirrorProfilePeerMode_SetStateVar(MIRROR_PROFILE_PEER_MODE_STATE_DISCONNECTED);
        MirrorProfilePeerMode_SetTargetStateVar(MIRROR_PROFILE_PEER_MODE_STATE_ACTIVE);
        MirrorProfile_ClearTransitionLockBitPeerModeSm();
        MirrorProfile_ClearLinkPolicyInitialised();
        MessageCancelFirst(MirrorProfile_GetTask(), MIRROR_INTERNAL_PEER_ENTER_SNIFF);
        MirrorProfile_ClearPeerRoleSwitching();
        MessageCancelFirst(MirrorProfile_GetTask(), MIRROR_INTERNAL_PEER_LINK_POLICY_IDLE_TIMEOUT);
    }
}

void mirrorProfile_HandleTpConManagerConnectInd(const CON_MANAGER_TP_CONNECT_IND_T *ind)
{
    DEBUG_LOG("mirrorProfile_HandleTpConManagerConnectInd");
    if (appDeviceIsPeer(&ind->tpaddr.taddr.addr))
    {
        MirrorProfilePeerMode_SetStateVar(MIRROR_PROFILE_PEER_MODE_STATE_ACTIVE);
        MirrorProfilePeerMode_SetTargetStateVar(MIRROR_PROFILE_PEER_MODE_STATE_ACTIVE);
        MirrorProfile_ClearLinkPolicyInitialised();
        MirrorProfile_ClearPeerRoleSwitching();
        /* Message to trigger putting the peer link into sniff mode
           if state remains unchanged */
        MessageSendLater(MirrorProfile_GetTask(),
                         MIRROR_INTERNAL_PEER_ENTER_SNIFF, NULL,
                         mirrorProfileConfig_IdlePeerEnterSniffTimeout());
    }
}

bool MirrorProfile_HandleConnectionLibraryMessages(MessageId id, Message message,
                                                   bool already_handled)
{
    switch (id)
    {
        case CL_DM_MODE_CHANGE_EVENT:
        {
            CL_DM_MODE_CHANGE_EVENT_T *ev = (CL_DM_MODE_CHANGE_EVENT_T*)message;
            mirrorProfilePeerMode_HandleDmModeChangeEvent(&ev->bd_addr, ev->mode);
        }
        break;
        case CL_DM_ROLE_CFM:
        {
            CL_DM_ROLE_CFM_T * cfm = (CL_DM_ROLE_CFM_T *)message;
            mirrorProfilePeerMode_HandleDmRoleCfm(&cfm->bd_addr, cfm->role, cfm->status);
        }
        break;
        case CL_DM_ROLE_IND:
        {
            CL_DM_ROLE_IND_T * ind= (CL_DM_ROLE_IND_T *)message;
            mirrorProfilePeerMode_HandleDmRoleInd(&ind->bd_addr, ind->role, ind->status);
        }
        break;
        case CL_DM_SNIFF_SUB_RATING_IND:
            mirrorProfilePeerMode_HandleDmSniffSubRatingInd((const CL_DM_SNIFF_SUB_RATING_IND_T *)message);
        break;
        default:
        break;
    }
    return already_handled;
}

/*! \brief Logic to transition from current state to target state.

    \return The next state to enter in the transition to the target state.

    Generally, the logic determines the transitionary state to enter from the
    current steady state.
 */
static mirror_profile_peer_mode_state_t mirrorProfilePeerMode_SmTransition(void)
{
    switch ((MirrorProfilePeerMode_GetTargetState()))
    {
    case MIRROR_PROFILE_PEER_MODE_STATE_SNIFF:
        if (MirrorProfilePeerMode_GetState() == MIRROR_PROFILE_PEER_MODE_STATE_ACTIVE)
        {
            return MIRROR_PROFILE_PEER_MODE_STATE_ENTER_SNIFF;
        }
        break;

    case MIRROR_PROFILE_PEER_MODE_STATE_ACTIVE:
        if (MirrorProfilePeerMode_GetState() == MIRROR_PROFILE_PEER_MODE_STATE_SNIFF)
        {
            return MIRROR_PROFILE_PEER_MODE_STATE_EXIT_SNIFF;
        }
        break;

    default:
        Panic();
        break;
    }
    return MirrorProfilePeerMode_GetState();
}

/*! \brief Kick the state machine to transition to a new state if required */
static void mirrorProfilePeerMode_SmKick(void)
{
    /* Only allow when in steady state. */
    if (mirrorProfilePeerMode_IsInSteadyState())
    {
        mirror_profile_peer_mode_state_t next = mirrorProfilePeerMode_SmTransition();

        if (next == MIRROR_PROFILE_PEER_MODE_STATE_EXIT_SNIFF)
        {
            if (MirrorProfile_GetState() != MIRROR_PROFILE_STATE_ACL_CONNECTED)
            {
                /* Only allow transition to active mode from base ACL_CONNECTED
                   state */
                return;
            }
        }
        mirrorProfilePeerMode_SetState(next);
    }
}


/*! \brief Set a new state.
    \param new_state The new state to set.
*/
static void mirrorProfilePeerMode_SetState(mirror_profile_peer_mode_state_t new_state)
{
    mirror_profile_peer_mode_state_t current_state = MirrorProfilePeerMode_GetState();

    DEBUG_LOG("mirrorProfilePeerMode_SetState %d->%d", current_state, new_state);

    if (new_state == current_state)
    {
        return;
    }

    // Handle exiting states
    switch (current_state)
    {
        case MIRROR_PROFILE_PEER_MODE_STATE_ENTER_SNIFF:
            break;
        case MIRROR_PROFILE_PEER_MODE_STATE_SNIFF:
            break;
        case MIRROR_PROFILE_PEER_MODE_STATE_EXIT_SNIFF:
            break;
        case MIRROR_PROFILE_PEER_MODE_STATE_ACTIVE:
            break;
        default:
            Panic();
            break;
    }

    MirrorProfilePeerMode_SetStateVar(new_state);
    MirrorProfile_ClearTransitionLockBitPeerModeSm();

    // Handle entering states
    switch (new_state)
    {
        case MIRROR_PROFILE_PEER_MODE_STATE_ENTER_SNIFF:
            MirrorProfile_UpdatePeerLinkPolicy(lp_sniff);
            MirrorProfile_SetTransitionLockBitPeerModeSm();
            break;
        case MIRROR_PROFILE_PEER_MODE_STATE_SNIFF:
            break;
        case MIRROR_PROFILE_PEER_MODE_STATE_EXIT_SNIFF:
            MirrorProfile_UpdatePeerLinkPolicy(lp_active);
            MirrorProfile_SetTransitionLockBitPeerModeSm();
            break;
        case MIRROR_PROFILE_PEER_MODE_STATE_ACTIVE:
            break;
        default:
            Panic();
            break;
    }

    if (MirrorProfile_IsPrimary())
    {
        mirrorProfilePeerMode_SmKick();
        /* Don't kick mirror profile state machine immediately, let messages waiting
         * for lock release, get a chance to be delivered before the kick. */
        MessageCancelFirst(MirrorProfile_GetTask(), MIRROR_INTERNAL_DELAYED_KICK);
        MessageSend(MirrorProfile_GetTask(), MIRROR_INTERNAL_DELAYED_KICK, NULL);
    }
}

bool mirrorProfilePeerMode_IsInSteadyState(void)
{
    switch (MirrorProfilePeerMode_GetState())
    {
        case MIRROR_PROFILE_PEER_MODE_STATE_ACTIVE:
        case MIRROR_PROFILE_PEER_MODE_STATE_SNIFF:
            return !MirrorProfile_IsPeerRoleSwitching();
        default:
            return FALSE;
    }
}

bool mirrorProfilePeerMode_SetTargetState(mirror_profile_peer_mode_state_t target)
{
    if (MirrorProfilePeerMode_GetState() != MIRROR_PROFILE_PEER_MODE_STATE_DISCONNECTED)
    {
        DEBUG_LOG_INFO("mirrorProfilePeerMode_SetTargetState enum:mirror_profile_peer_mode_state_t:%d", target);

        MirrorProfilePeerMode_SetTargetStateVar(target);

        if (MirrorProfilePeerMode_GetState() != target)
        {
            mirrorProfilePeerMode_SmKick();
            return FALSE;
        }
        return TRUE;
    }
    return FALSE;
}

bool mirrorProfilePeerMode_ActiveModePeriod(uint32 period_ms)
{
    bool result = FALSE;
    if (MirrorProfilePeerMode_GetState() != MIRROR_PROFILE_PEER_MODE_STATE_DISCONNECTED)
    {
        DEBUG_LOG_INFO("mirrorProfilePeerMode_ActiveModePeriod %u ms", period_ms);

        mirrorProfilePeerMode_SetTargetState(MIRROR_PROFILE_PEER_MODE_STATE_ACTIVE);
        MessageCancelFirst(MirrorProfile_GetTask(), MIRROR_INTERNAL_PEER_ENTER_SNIFF);
        MessageSendLater(MirrorProfile_GetTask(),
                         MIRROR_INTERNAL_PEER_ENTER_SNIFF,
                         NULL, period_ms);
        result = TRUE;
    }
    return result;
}

void mirrorProfilePeerMode_HandleDmModeChangeEvent(bdaddr* bd_addr, uint8 mode)
{
    if (appDeviceIsPeer(bd_addr))
    {
        DEBUG_LOG("mirrorProfile_HandleDmModeChangeEvent peer mode enum:lp_power_mode:%d", mode);

        mirror_profile_peer_mode_state_t new_state = (mode == lp_active) ?
            MIRROR_PROFILE_PEER_MODE_STATE_ACTIVE :
            MIRROR_PROFILE_PEER_MODE_STATE_SNIFF;

        if (MirrorProfile_IsPrimary())
        {
            mirrorProfilePeerMode_SetState(new_state);
        }
        else
        {
            /* As secondary, ensure the target state is tracking the state
            controlled by the primary */
            MirrorProfilePeerMode_SetTargetStateVar(new_state);
            MirrorProfilePeerMode_SetStateVar(new_state);
        }

        MirrorPioSet();
        if (mode == lp_sniff)
        {
            MessageCancelFirst(MirrorProfile_GetTask(), MIRROR_INTERNAL_PEER_ENTER_SNIFF);
            MirrorProfile_PeerLinkPolicyInit();
        }
        MirrorPioClr();
    }

    else if (BdaddrIsSame(MirrorProfile_GetMirroredDeviceAddress(), bd_addr))
    {
        DEBUG_LOG("mirrorProfile_HandleDmModeChangeEvent handset mode enum:lp_power_mode:%d", mode);
        if (mode == lp_active)
        {
            /* Some state transitions require the handset to be in active mode */
            MirrorProfile_SmKick();
        }
    }
}

void mirrorProfilePeerMode_HandleDmRoleCfm(bdaddr* bd_addr, hci_role role, hci_status status)
{
    if (appDeviceIsPeer(bd_addr))
    {
        DEBUG_LOG("mirrorProfilePeerMode_HandleDmRoleCfm enum:hci_status:%d enum:hci_role:%d", status, role);
        if (status == hci_success)
        {
            if (role == hci_role_master)
            {
                MirrorProfile_PeerLinkPolicyHandleIdleTimeout();
                mirrorProfilePeerMode_SmKick();
            }
        }
        else
        {
            MirrorProfile_ClearPeerRoleSwitching();
            /* Try again later */
            MirrorProfile_SendLinkPolicyTimeout();
        }
    }
}

void mirrorProfilePeerMode_HandleDmRoleInd(bdaddr* bd_addr, hci_role role, hci_status status)
{
    if (appDeviceIsPeer(bd_addr))
    {
        DEBUG_LOG("mirrorProfilePeerMode_HandleDmRoleInd enum:hci_status:%d enum:hci_role:%d", status, role);
        if (status == hci_success)
        {
            if (MirrorProfile_IsSecondary())
            {
                if (role == hci_role_slave)
                {
                    MirrorProfile_PeerLinkPolicyHandleIdleTimeout();
                }
            }
        }
    }
}

void mirrorProfile_HandlePeerEnterSniff(void)
{
    mirrorProfilePeerMode_SetTargetState(MIRROR_PROFILE_PEER_MODE_STATE_SNIFF);
}

#endif /* INCLUDE_MIRRORING */

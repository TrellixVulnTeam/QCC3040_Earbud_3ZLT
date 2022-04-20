/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      State machine to control peer mode (sniff or active)
*/

/*! \brief Mirror Profile Peer Mode States

@startuml

    state ACTIVE : In active mode
    state ENTER_SNIFF : Entering sniff mode
    state SNIFF : In sniff mode
    state EXIT_SNIFF : Exiting sniff mode

    ACTIVE --> ENTER_SNIFF : Entering sniff mode
    ENTER_SNIFF --> SNIFF : mode change = lp_sniff
    SNIFF --> EXIT_SNIFF : Exiting sniff mode
    EXIT_SNIFF --> ACTIVE : mode change = lp_active

@enduml
*/

#ifdef INCLUDE_MIRRORING

#ifndef MIRROR_PROFILE_PEER_MODE_SM_H_
#define MIRROR_PROFILE_PEER_MODE_SM_H_

typedef enum
{
    /*! Disconnected from peer */
    MIRROR_PROFILE_PEER_MODE_STATE_DISCONNECTED,

    /*! In active mode - default after connection created */
    MIRROR_PROFILE_PEER_MODE_STATE_ACTIVE,

    /*! Entering sniff mode */
    MIRROR_PROFILE_PEER_MODE_STATE_ENTER_SNIFF,

    /*! In sniff mode */
    MIRROR_PROFILE_PEER_MODE_STATE_SNIFF,

    /*! Exiting sniff mode */
    MIRROR_PROFILE_PEER_MODE_STATE_EXIT_SNIFF,

} mirror_profile_peer_mode_state_t;

/*! \brief Set target peer link mode.
    \param target The target state.
    \return TRUE if already in the target state, FALSE is a state transition is
    required.
*/
bool mirrorProfilePeerMode_SetTargetState(mirror_profile_peer_mode_state_t target);

/*! \brief Request the peer link to enter active mode for finite period.
    \param period_ms The time in milliseconds to enter active mode.
    \note The link between the buds shall only be placed in active mode when mirroring
          is inactive or for a brief period when ACL or eSCO mirroring is active.
          It should never be put into active mode when A2DP mirroring is active -
          doing so will result in poor performance and audio glitches.
*/
bool mirrorProfilePeerMode_ActiveModePeriod(uint32 period_ms);

/*! \brief Query if peer mode SM is in steady state */
bool mirrorProfilePeerMode_IsInSteadyState(void);

/*! \brief Handle CON_MANAGER_TP_CONNECT_IND_T */
void mirrorProfile_HandleTpConManagerConnectInd(const CON_MANAGER_TP_CONNECT_IND_T *ind);

/*! \brief Handle CON_MANAGER_TP_DISCONNECT_IND_T */
void mirrorProfile_HandleTpConManagerDisconnectInd(const CON_MANAGER_TP_DISCONNECT_IND_T *ind);

/*! \brief Handle MIRROR_INTERNAL_PEER_ENTER_SNIFF */
void mirrorProfile_HandlePeerEnterSniff(void);

/*! \brief Handle CL_DM_SNIFF_SUB_RATING_IND */
void mirrorProfilePeerMode_HandleDmSniffSubRatingInd(const CL_DM_SNIFF_SUB_RATING_IND_T *ind);
#endif

#endif /* INCLUDE_MIRRORING */

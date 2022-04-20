/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Protected interface to mirror profile. These functions should not
            be used by customer code.
*/

#ifndef MIRROR_PROFILE_PROTECTED_H_
#define MIRROR_PROFILE_PROTECTED_H_

#include "mirror_profile.h"
#include "handover_if.h"

/*!
    @brief Exposes mirror profile interface for handover.
*/
extern const handover_interface mirror_handover_if;

/*! \brief Set peer link policy to the requested mode.

    \param mode The new mode to set.

    \note This function is only intened to be called by handover profile to
          control the peer link mode during handover.
 */
void MirrorProfile_UpdatePeerLinkPolicy(lp_power_mode mode);

/*! \brief Set peer link policy to the requested mode.

    \param mode The new mode to set.

    \note This function is only intened to be called by DFU to enable
          active mode during in-case DFU.
 */
void MirrorProfile_UpdatePeerLinkPolicyDfu(lp_power_mode mode);

/*! \brief Set peer link policy to the requested mode and block waiting for
           the mode to change.

    \param mode The new mode to set.
    \param timeout_ms  The time to wait for the mode to change in milli-seconds.

    \return TRUE if the mode was changed before the timeout expired. FALSE otherwise.

    \note This function is only intened to be called by handover profile to
          control the peer link mode during handover.
 */
bool MirrorProfile_UpdatePeerLinkPolicyBlocking(lp_power_mode mode, uint32 timeout_ms);

/*! \brief When the primary and secondary re-enter sniff mode near the completion
           of handover, if the subrate policy is in idle mode, it is reset to
           the lowest latency subrate again. This allows low delay communication
           to occur after handover (e.g. link disconnection if primary is
           put in the case) and also allows a role switch to be triggered (if
           required) when the policy reverts back to idle after a timeout.
           This function should be called by both primary and secondary earbuds
           so that both re-enter sniff mode with the same subrate policy.
*/
void MirrorProfile_HandoverRefreshSubrate(void);

/*! \brief Wait for the link to the peer to enter the defined mode.

    \param mode The mode.
    \param timeout_ms  The time to wait for the mode to change in milli-seconds.

    \return TRUE if the mode was changed before the timeout expired. FALSE otherwise.

    \note This function is only intened to be called by handover profile to
          control the peer link mode during handover.
*/
bool MirrorProfile_WaitForPeerLinkMode(lp_power_mode mode, uint32 timeout_ms);

/*! \brief Handle Veto check during handover

    \return TRUE If mirror profile is not in connected(ACL/ESCO/A2DP) then veto handover. FALSE otherwise.
*/
bool MirrorProfile_Veto(void);

/*! \brief Get the mirror profile state.

    \return The mirror profile state.
*/
uint16 MirrorProfile_GetMirrorState(void);

/*! \brief Send a message to secondary earbud informing it of the unmute time. */
void MirrorProfile_SendA2pdUnmuteTimeToPrimary(rtime_t unmute_time);

#ifdef INCLUDE_MIRRORING
/*! \brief Set the peer link policy to its lowest latency mode if the current mode is Idle, and return to
           Idle mode following a timeout defined by mirrorProfileConfig_LinkPolicyIdleTimeout()

    \return TRUE if the link policy changed. FALSE otherwise
*/

bool MirrorProfile_PeerLinkPolicyLowLatencyKick(void);
#else
#define MirrorProfile_PeerLinkPolicyLowLatencyKick() FALSE
#endif /* INCLUDE_MIRRORING */

/*! \brief Get the audio source corresponding to the mirrored device.
    \return The audio source.

    \note This function should only be called from the primary device since it
    gets the audio source for the mirrored handset - the audio source is only
    set for the mirrored handset on the primary.
*/
audio_source_t MirrorProfile_GetAudioSource(void);

/*! \brief Get the voice source corresponding to the mirrored device.
    \return The voice source.

    \note This function should only be called on the primary device since it
    gets the voice source for the mirrored handset - the voice source is only
    set for the mirrored handset on the primary.
*/
voice_source_t MirrorProfile_GetVoiceSource(void);

#endif

/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Configuration related definitions for the mirror_profile state machine.
*/

#ifndef MIRROR_PROFILE_CONFIG_H_
#define MIRROR_PROFILE_CONFIG_H_

#include <message.h>


/*! Delay to use on the Primary between a failed mirror ACL connect req and
    trying the mirror ACL connect req again.

    Most reasons for a mirror ACL connect req to fail will require indefinite
    retries. e.g. connection timeout (link-loss).

    It is recommended to keep this delay large enough to avoid flooding the
    firmware with mirror ACL requests.
*/
#define mirrorProfileConfig_MirrorAclRetryDelay()  D_SEC(1)

/*! This is the length of time after eSCO/A2DP mirroring stops before the link
    policy is set to a lower power (and higher latency) setting. */
#define mirrorProfileConfig_LinkPolicyIdleTimeout() D_SEC(5)

/*! The time to wait for QHS link to establish between buds before starting
    mirroring. Mirroring will be started before this timeout if QHS is
    established  */
#define mirrorProfileConfig_QhsStartTimeout() 500

/*! The time to wait with the peer link in active mode after connecting to
    the peer before placing the link into sniff mode if the profile remains idle.
*/
#define mirrorProfileConfig_IdlePeerEnterSniffTimeout() D_SEC(10)

/*! When TELEPHONY_CALL_ONGOING is received, the primary earbud brings the
    link with the secondary to active mode, this reduces the time taken to
    start eSCO mirroring. In some corner cases, TELEPHONY_CALL_ONGOING is
    received but the eSCO never connects. This configuration defines the maximum
    time to wait for the eSCO to connect before putting the link back into sniff
    mode. */
#define mirrorProfileConfig_PrepareForEscoMirrorActiveModeTimeout() D_SEC(1)

/*! The maximum time in milli-seconds to wait after receiving
    APP_HFP_SCO_CONNECTING_SYNC_IND before accepting the incoming SCO
    connection from HFP regardless of whether the mirror profile has switched
    to mirror the ACL of the handset with active eSCO. */
#define mirrorProfileConfig_ScoConnectingSyncTimeout() 600

/*! When the primary/secondary start eSCO audio in-sync, this configuration sets
    the time in future when audio will be unmuted after secondary starts its
    eSCO audio chain. */
#define mirrorProfileConfig_ScoSyncUnmuteDelayUs() 100000

#endif /* MIRROR_PROFILE_CONFIG_H_ */
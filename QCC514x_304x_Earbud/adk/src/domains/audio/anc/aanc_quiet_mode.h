/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       aanc_quiet_mode.h
\defgroup   aanc_quiet_mode anc 
\ingroup    audio_domain
\brief      Quiet mode header for Adative Active Noise Cancellation (ANC).
*/

#ifndef AANC_QUIET_MODE_H_
#define AANC_QUIET_MODE_H_

#include <marshal_common.h>

typedef struct
{
    bool aanc_quiet_mode_detected;
    bool aanc_quiet_mode_enabled;
    bool aanc_quiet_mode_enable_requested;
    bool aanc_quiet_mode_disable_requested;
    marshal_rtime_t timestamp;
}AANC_UPDATE_QUIETMODE_IND_T;


/*!
    \brief Sets the L2CAP sink in AANC domaain-required for wall clock
           calculation for peer synchronization.
    \param Sink sink -L2CAP sink.
    \return None
*/
#ifdef ENABLE_ADAPTIVE_ANC
void AancQuietMode_SetWallClock(Sink sink);
#else
#define AancQuietMode_SetWallClock(x) ((void)(0))
#endif

/*!
    \brief Returns status of quiet mode detection flag.
    \param None.
    \return bool -TRUE if quiet mode is detected locally.
*/
#ifdef ENABLE_ADAPTIVE_ANC
bool AancQuietMode_IsQuietModeDetected(void);
#else
#define AancQuietMode_IsQuietModeDetected() (FALSE)
#endif

/*!
    \brief Returns status of quiet mode Enabled flag.
    \param None.
    \return bool -TRUE if quiet mode is Enabled locally.
*/
#ifdef ENABLE_ADAPTIVE_ANC
bool AancQuietMode_IsQuietModeEnabled(void);
#else
#define AancQuietMode_IsQuietModeEnabled()  (FALSE)
#endif

/*!
    \brief Returns status of quiet mode Enable request flag.
    \param None.
    \return bool -TRUE if quiet mode is Enable is requested by local peer.
*/
#ifdef ENABLE_ADAPTIVE_ANC
bool AancQuietMode_IsQuietModeEnableRequested(void);
#else
#define AancQuietMode_IsQuietModeEnableRequested() (FALSE)
#endif

/*!
    \brief Returns status of quiet mode disable request flag.
    \param None.
    \return bool - TRUE if quiet mode is disable is requested by local peer.
*/
#ifdef ENABLE_ADAPTIVE_ANC
bool AancQuietMode_IsQuietModeDisableRequested(void);
#else
#define AancQuietMode_IsQuietModeDisableRequested()  (FALSE)
#endif

/*!
    \brief Returns timestamp at which peers will be synchronized.
    \param None.
    \return marshal_rtime_t - uint32 timestamp value calculated by local peer.
*/
#ifdef ENABLE_ADAPTIVE_ANC
marshal_rtime_t AancQuietMode_GetTimestamp(void);
#else
#define AancQuietMode_GetTimestamp() (0)
#endif

#endif /* AANC_QUIET_MODE_H_ */

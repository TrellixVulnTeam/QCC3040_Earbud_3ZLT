/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Kymera module to increase/decrease A2DP audio latency.

            The module has two modes:
            1. Continuous slow adjustments to latency in a seamless manner in
               response to changes in the TWM system performance.
               May be enabled using Kymera_LatencyManagerConfigEnableDynamicAdjustment().
            2. Fast adjustments to latency masked by muting the output and playing
               a tone. This is used for gross latency mode changes.
*/

#ifndef KYMERA_LATENCY_MANAGER_H_
#define KYMERA_LATENCY_MANAGER_H_

#include "operators.h"
#include <rtime.h>

/*! \brief The KYMERA_INTERNAL_LATENCY_RECONFIGURE_T message content. */
typedef struct
{
    /*!  The time at which to start the latency adjustment */
    rtime_t mute_instant;
    /*! Task data */
    Task clientTask;
    /*!  The tone to play whilst muted to indicate the latency change */
    const ringtone_note* tone;
}KYMERA_INTERNAL_LATENCY_RECONFIGURE_T;

#ifdef INCLUDE_LATENCY_MANAGER

#include <csrtypes.h>
#include <bdaddr.h>
#include "kymera_a2dp.h"
#include "latency_config.h"

/*! \brief Configure the mute and unmute period during rapid latency reconfig. */
#define Kymera_LatencyManagerConfigMuteTransitionPeriodMs() (50)
/*! \brief Configure the duration the output is muted after reconfiguration to
           mask audio glitches. */
#define Kymera_LatencyManagerConfigMuteDurationMs() (500)

/*! Periodic interval in ms to query/check for Low Latency A2DP streaming */
#define Kymera_LatencyManagerConfigLLStreamCheckIntervalMs()    (500)

/*! \brief Latency Manager State variables */
typedef struct
{
    /*! Current latency in milliseconds */
    uint16 current_latency;

    /*! Override latency with value provided by test. This value is applied
        if non-zero. */
    uint16 override_latency;

    /*! Set when latency manager is forcing a latency adjustment and used as
        a conditional message lock. */
    uint16 adjusting_latency;

    /*! Gaming mode enabled */
    unsigned gaming_mode_enabled : 1;

    unsigned ll_stream_state:1;

    /*! High Bandwidth (ie 96K) stream active */
    unsigned hbw_stream_state:1;

    /*! The a2dp start data needs to be stored as latency manager needs to
        restart the A2DP audio chain when entering/exiting gaming mode */
    KYMERA_INTERNAL_A2DP_START_T *a2dp_start_params;

}kymera_latency_manager_data_t;

/*! \brief Low Latency stream states */
typedef enum
{
    LOW_LATENCY_STREAM_INACTIVE,
    LOW_LATENCY_STREAM_ACTIVE,
}ll_stream_state_t;

typedef struct
{
    ll_stream_state_t state;
}KYMERA_LOW_LATENCY_STATE_CHANGED_IND_T;

/*! \brief High Bandwidth stream states */
typedef enum
{
    HIGH_BANDWIDTH_STREAM_INACTIVE,
    HIGH_BANDWIDTH_STREAM_ACTIVE,
}hbw_stream_state_t;

typedef struct
{
    hbw_stream_state_t state;
}KYMERA_HIGH_BANDWIDTH_STATE_CHANGED_IND_T;

extern kymera_latency_manager_data_t latency_data;


#define KymeraGetLatencyData() (&latency_data)
#define Kymera_LatencyManagerGetLatency() (KymeraGetLatencyData()->current_latency)
#define Kymera_LatencyManagerSetLatency(l) (KymeraGetLatencyData()->current_latency = l)
#define Kymera_LatencyManagerIsGamingModeEnabled() (KymeraGetLatencyData()->gaming_mode_enabled)
#define Kymera_LatencyManagerSetAdjustingLatency() (KymeraGetLatencyData()->adjusting_latency = 1)
#define Kymera_LatencyManagerClearAdjustingLatency() (KymeraGetLatencyData()->adjusting_latency = 0)
#define Kymera_LatencyMangerIsLLStreamActive() (KymeraGetLatencyData()->ll_stream_state == LOW_LATENCY_STREAM_ACTIVE)
#define Kymera_LatencyMangerIsHBStreamActive() (KymeraGetLatencyData()->hb_stream_state == HIGH_BANDWIDTH_STREAM_ACTIVE)

/*! \brief Initializes Latency Manager.
    \param enable_gaming_mode Whether to start with gaming mode enabled.
    \param override_latency_ms If non zero the latency will always be set to this
    value regardless of mode or codec.
*/
void Kymera_LatencyManagerInit(bool enable_gaming_mode, uint32 override_latency_ms);

/* \brief Enable Gaming Mode */
void Kymera_LatencyManagerEnableGamingMode(void);

/* \brief Disable Gaming Mode */
void Kymera_LatencyManagerDisableGamingMode(void);

/*! \brief Called when A2DP media starts in kymera */
void Kymera_LatencyManagerA2dpStart(const KYMERA_INTERNAL_A2DP_START_T* a2dp_start);

/*! \brief Called when A2DP media stops in kymera */
void Kymera_LatencyManagerA2dpStop(void);

/*! \brief Handles the kymera latency reconfiguration message */
void Kymera_LatencyManagerHandleLatencyReconfigure(const KYMERA_INTERNAL_LATENCY_RECONFIGURE_T * msg);

/*! \brief Reconfigure the audio latency in one rapid adjustment.
    \param clientTask gaming mode task data
    \param mute_instant The time at which to start the latency adjustment.
    \param tone The tone to play whilst muted to indicate the latency change.
    \note Audio is muted during the reconfiguration to avoid audible glitches.
 */
void Kymera_LatencyManagerReconfigureLatency(Task clientTask, rtime_t mute_instant, const ringtone_note *tone);

/*! \brief Adjust the current latency without stopping the audio stream.
    \param latency_ms The new latency value in milli-seconds.
*/
void Kymera_LatencyManagerAdjustLatency(uint16 latency_ms);

/*! \brief Handle trigger to mute */
void Kymera_LatencyManagerHandleMute(void);

/*! \brief Handle completion of a tone */
void Kymera_LatencyManagerHandleToneEnd(void);

/*! \brief Handle Mute completion */
void Kymera_LatencyManagerHandleMuteComplete(void);

/*! \brief Latency manager needs to know about volume changes so it can restore
    the volume to the correct level after a muted latency adjustment. This
    function should be called whenever the A2DP volume is changed.
*/
void Kymera_LatencyManagerHandleA2dpVolumeChange(int16 volume_in_db);

/*! \brief Handle mirror A2DP activating */
void Kymera_LatencyManagerHandleMirrorA2dpStreamActive(void);

/*! \brief Handle mirror A2DP deactivating */
void Kymera_LatencyManagerHandleMirrorA2dpStreamInactive(void);

/*! \brief Override the latency with the value provided.
    \param latency_ms The fixed latency to use at all times.
    \return TRUE if the latency override was set.
    \note Override with latency_ms=0 will disabled any override.
*/
bool Kymera_LatencyManagerSetOverrideLatency(uint32 latency_ms);

/*! \brief Marshal latency data.
    \param buf The buffer to write to.
    \param length The space in the buffer in bytes.
    \return The number of bytes written. Any positive value means marshalling is
    complete.
*/
uint16 Kymera_LatencyManagerMarshal(uint8 *buf, uint16 length);

/*! \brief Unarshal latency data.
    \param buf The buffer to read from.
    \param length The data in the buffer in bytes.
    \return The number of bytes read. Any positive value means unmarshalling is
    complete.
*/
uint16 Kymera_LatencyManagerUnmarshal(const uint8 *buf, uint16 length);

/*! \brief Commit to the new role.
    \param TRUE if the new role is primary
*/
void Kymera_LatencyManagerHandoverCommit(bool is_primary);

/*! \brief Check if latency reconfiguration is in progress */
#define Kymera_LatencyManagerIsReconfigInProgress() (KymeraGetLatencyData()->adjusting_latency)

/*! \brief Handle checking of low latency stream */
void Kymera_LatencyManagerHandleLLStreamCheck(void);
#else
#include "latency_config.h"
#define Kymera_LatencyManagerInit(enable_gaming_mode, override_latency) do {UNUSED(enable_gaming_mode); UNUSED(override_latency);} while(0)
#define Kymera_LatencyManagerA2dpStart(params) UNUSED(params)
#define Kymera_LatencyManagerA2dpStop()
#define Kymera_LatencyManagerHandleLatencyReconfigure(msg) UNUSED(msg)
#define Kymera_LatencyManagerAdjustLatency(latency) UNUSED(latency)
#define Kymera_LatencyManagerReconfigureLatency(t, i, n) do {UNUSED(t); UNUSED(i); UNUSED(n);} while(0)
#define Kymera_LatencyManagerEnableGamingMode()
#define Kymera_LatencyManagerDisableGamingMode()
#define Kymera_LatencyManagerHandleMute()
#define Kymera_LatencyManagerHandleToneEnd()
#define Kymera_LatencyManagerHandleMuteComplete()
#define Kymera_LatencyManagerIsReconfigInProgress() (FALSE)
#define Kymera_LatencyManagerIsGamingModeEnabled() (FALSE)
#define Kymera_LatencyManagerHandleA2dpVolumeChange(volume_in_db) UNUSED(volume_in_db)
#define Kymera_LatencyManagerSetOverrideLatency(latency_ms) (FALSE && latency_ms)
#define Kymera_LatencyManagerHandleMirrorA2dpStreamActive()
#define Kymera_LatencyManagerHandleMirrorA2dpStreamInactive()
#define Kymera_LatencyManagerHandoverCommit(is_primary)
#define Kymera_LatencyManagerHandleLLStreamCheck()


#endif /* INCLUDE_LATENCY_MANAGER */

/*! \brief Get the latency for a SEID in microseconds.
    \param seid The A2DP sink stream endpoint ID.
    \return TTP latency in microseconds.
    \note Latency should always be obtained using this function even if latency
    manager is not compiled.
*/
uint32 Kymera_LatencyManagerGetLatencyForSeidInUs(uint8 seid);

/*! \brief Get the latency for a codec type in microseconds.
    \param codec The RTP codec type.
    \return TTP latency in microseconds.
    \note Latency should always be obtained using this function even if latency
    manager is not compiled.
*/
uint32 Kymera_LatencyManagerGetLatencyForCodecInUs(rtp_codec_type_t codec);

#endif /* KYMERA_LATENCY_MANAGER_H_ */

/*!
\copyright  Copyright (c) 2019 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       kymera_config.h
\brief      Configuration related definitions for Kymera audio.
*/

#ifndef KYMERA_CONFIG_H
#define KYMERA_CONFIG_H

#include "kymera_kick_period_config.h"
#include "microphones.h"

/*! Default Sidetone step up time in milliseconds */
#define ST_GAIN_RAMP_STEP_TIME_MS  (25U)

/*! \brief Fixed tone volume in dB */
#define KYMERA_CONFIG_TONE_VOLUME               (-20)

/*! \brief Fixed prompt volume in dB */
#if (defined QCC3020_FF_ENTRY_LEVEL_AA) || (defined HAVE_RDP_UI)
#define KYMERA_CONFIG_PROMPT_VOLUME             (-16)   /* Reduce for the RDP platforms as -10dB is too loud */
#else
#define KYMERA_CONFIG_PROMPT_VOLUME             (-10)
#endif

#ifdef ENABLE_ADAPTIVE_ANC
#define KYMERA_CONFIG_ANC_GENTLE_MUTE_TIMER          (100) /*ms*/
#else
#define KYMERA_CONFIG_ANC_GENTLE_MUTE_TIMER          (0) /*ms*/
#endif

/*! \brief Defining USB as downloadable capability for Aura2.1 variant */
#if defined(HAVE_STR_ROM_2_0_1)
#define DOWNLOAD_USB_AUDIO
#endif
/*! @{ Which microphones to use for SCO */

#define EQ_BANK_USER 63

/* Always define mic 1..3. Number of microphones can be checked using Kymera_GetNumberOfMics() */
#if (defined HAVE_RDP_HW_YE134) || (defined HAVE_RDP_HW_18689)
#define appConfigMicVoice()                     (microphone_4)     /* Use microphone for 1st SCO mic */
#define appConfigMicExternal()                  (microphone_3)     /* Use microphone for SCO 2nd mic on RDP platform (CVC 2-mic) */
                                                                   
#define appConfigMicInternal()                  (microphone_2)     /* Use microphone for CVC_3Mic */
#elif defined(CORVUS_YD300)
#define appConfigMicVoice()                     (microphone_3)     /* 1mic CVC */
#define appConfigMicExternal()                  (microphone_none)
#define appConfigMicInternal()                  (microphone_none)
#else
#define appConfigMicVoice()                     (microphone_1)
#define appConfigMicExternal()                  (microphone_2)
#define appConfigMicInternal()                  (microphone_3)
#endif

#ifdef INCLUDE_BCM
#define appConfigMicBCM()                       (microphone_1)     /* internal bone conducting mic */
#else
#define appConfigMicBCM()                       (microphone_none)
#endif
#define appConfigMicVoiceLeft()                 (microphone_1)
#define appConfigMicVoiceRight()                (microphone_2)

#define appConfigVaMic1()                       appConfigMicVoice()
#define appConfigVaMic2()                       appConfigMicExternal()
/*! @} */

//!@{ @name ANC configuration */


#ifdef ENABLE_ENHANCED_ANC
#if (defined(__QCC304X__) || defined(__QCC514X__)) || defined(__QCC516x__)
#define ENHANCED_ANC_USE_2ND_DAC_ENDPOINT
#endif
#endif

#ifdef ENABLE_ENHANCED_ANC
#define appKymeraIsParallelAncFilterEnabled() (TRUE)
#else
#define appKymeraIsParallelAncFilterEnabled() (FALSE)
#endif

#ifdef ENHANCED_ANC_USE_2ND_DAC_ENDPOINT
#define appKymeraEnhancedAncRequiresSecondDAC() (TRUE)
#else
#define appKymeraEnhancedAncRequiresSecondDAC() (FALSE)
#endif

/*! Microphone framework: Max number of microphones expected */
#define MAX_NUM_OF_CONCURRENT_MICS (3)
/*! Microphone framework: Max number of mic users expected in parallel */
#define MAX_NUM_OF_CONCURRENT_MIC_USERS (3)
/*! Retry connection to microphone framework after X milliseconds */
#define MIC_CONNECT_RETRY_MS (100)
/*! Poll capability setting after x milliseconds */
#define POLL_SETTINGS_MS (501)

/* Headset application */
#ifdef INCLUDE_STEREO
#if defined (CORVUS_YD300)
#define appConfigAncPathEnable()                (hybrid_mode)
#define appConfigAncFeedForwardLeftMic()        (microphone_3)
#define appConfigAncFeedBackLeftMic()           (microphone_1)
#define appConfigAncFeedForwardRightMic()       (microphone_4)
#define appConfigAncFeedBackRightMic()          (microphone_2)
#else
#define appConfigAncPathEnable()                (feed_forward_mode)
#define appConfigAncFeedForwardLeftMic()        appConfigMicVoiceLeft()
#define appConfigAncFeedBackLeftMic()           (microphone_none)
#define appConfigAncFeedForwardRightMic()       appConfigMicVoiceRight()
#define appConfigAncFeedBackRightMic()          (microphone_none)
#endif

/*! ANC tuning monitor microphone */
#define appConfigAncTuningMonitorLeftMic()      (microphone_none)
#define appConfigAncTuningMonitorRightMic()     (microphone_none)
#else
/* Earbud application */
#if (defined HAVE_RDP_HW_YE134) || (defined HAVE_RDP_HW_18689)
#define appConfigAncPathEnable()                (hybrid_mode_left_only)
#define appConfigAncFeedForwardMic()            appConfigMicExternal()
#define appConfigAncFeedBackMic()               appConfigMicInternal()
#elif defined(CORVUS_YD300)
#define appConfigAncPathEnable()                (hybrid_mode_left_only)
#define appConfigAncFeedForwardMic()            (microphone_3)
#define appConfigAncFeedBackMic()               (microphone_4)
#else
#define appConfigAncPathEnable()                (feed_forward_mode_left_only)
#define appConfigAncFeedForwardMic()            appConfigMicVoice()
#define appConfigAncFeedBackMic()               (microphone_none)
#endif

/*! ANC tuning monitor microphone */
#define appConfigAncTuningMonitorMic()          (microphone_none)
#endif

#ifdef HAVE_RDP_UI
#define appConfigNumOfAncModes()                (5U)
#else
#define appConfigNumOfAncModes()                (10U)
#endif
#define appConfigAncMode()                      (anc_mode_1)

/*! Configure Toggle behavior.*/
#define ancConfigToggleWay1()                   (anc_toggle_config_mode_1)
#define ancConfigToggleWay2()                   (anc_toggle_config_mode_5)
#define ancConfigToggleWay3()                   (anc_toggle_config_not_configured)

/*! Configure ANC modes to be used for concurrency cases.*/
#define ancConfigStandalone()                   (anc_toggle_config_is_same_as_current)
#define ancConfigPlayback()                     (anc_toggle_config_is_same_as_current)
#define ancConfigVoiceAssistant()               (anc_toggle_config_is_same_as_current)

/*! ANC mode configured when Implicit enable of ANC is triggered during SCO call */
#ifdef HAVE_RDP_UI
#define ancConfigVoiceCall()                    (anc_toggle_config_mode_5)
#else
#define ancConfigVoiceCall()                    (anc_toggle_config_is_same_as_current)
#endif

/*Demo mode option on GAIA UI for ANC*/
#ifdef HAVE_RDP_UI
#define ancConfigDemoMode()                    (TRUE)
#else
#define ancConfigDemoMode()                    (FALSE)
#endif

//!@}

/*! Enable ANC tuning functionality */
#define appConfigAncTuningEnabled()             (FALSE)

/*! Time to play to be applied on this earbud, based on the Wesco
    value specified when creating the connection.
    A value of 0 will disable TTP.  */
#if (defined INCLUDE_MIRRORING) || (defined INCLUDE_STEREO)
#define appConfigScoChainTTP(wesco)     ((wesco * 0) + 30000)
#else
#define appConfigScoChainTTP(wesco)     (wesco * 0)
#endif

/*! Time duration in milliseconds for 8 packets of 8 milliseconds each. */
#define MAX_SCO_PACKETS_DURATION         64

/*! Maximum 8 packets of 8 ms can be decoded and buffered in case
    there is a stall in the downstream. */
#define appConfigScoBufferSize(rate)     (MAX_SCO_PACKETS_DURATION * rate/1000)

#ifdef INCLUDE_STEREO
#define appConfigOutputIsStereo() TRUE
#else
#define appConfigOutputIsStereo() FALSE
#endif

/*! The last time before the TTP at which a packet may be transmitted */
#define appConfigTwsDeadline()      MAX(35000, TWS_STANDARD_LATENCY_US-250000)

/*! @{ Define the hardware settings for the left audio */
/*! Define which channel the 'left' audio channel comes out of. */
#define appConfigLeftAudioChannel()              (AUDIO_CHANNEL_A)

/*! Define the type of Audio Hardware for the 'left' audio channel. */
#define appConfigLeftAudioHardware()             (AUDIO_HARDWARE_CODEC)

/*! Define the instance for the 'left' audio channel comes. */
#define appConfigLeftAudioInstance()             (AUDIO_INSTANCE_0)

#ifdef INCLUDE_STEREO
/*! @{ Define the hardware settings for the right audio */
/*! Define which channel the 'right' audio channel comes out of. */
#define appConfigRightAudioChannel()              (AUDIO_CHANNEL_B)

/*! Define the type of Audio Hardware for the 'right' audio channel. */
#define appConfigRightAudioHardware()             (AUDIO_HARDWARE_CODEC)

/*! Define the instance for the 'right' audio channel comes. */
#define appConfigRightAudioInstance()             (AUDIO_INSTANCE_0)
#else
#define appConfigRightAudioChannel()              (audio_channel)(0x0)
#define appConfigRightAudioHardware()           (audio_hardware)(0x0)
#define appConfigRightAudioInstance()             (audio_instance)(0x0)
#endif

/*! @} */

/*! Define whether audio should start with or without a soft volume ramp */
#define appConfigEnableSoftVolumeRampOnStart() (FALSE)

/*!@{ @name External AMP control
      @brief If required, allows the PIO/bank/masks used to control an external
             amp to be defined.
*/
#if defined(CE821_CF212) || defined(CF376_CF212) || defined(CE821_CE826) || defined(CF133)

#define appConfigExternalAmpControlRequired()    (TRUE)
#define appConfigExternalAmpControlPio()         (32)
#define appConfigExternalAmpControlPioBank()     (1)
#define appConfigExternalAmpControlEnableMask()  (0 << 0)
#define appConfigExternalAmpControlDisableMask() (1 << (appConfigExternalAmpControlPio() % 32))

#else

#define appConfigExternalAmpControlRequired()    (FALSE)
#define appConfigExternalAmpControlPio()         (0)
#define appConfigExternalAmpControlEnableMask()  (0)
#define appConfigExternalAmpControlDisableMask() (0)

#endif /* defined(CE821_CF212) or defined(CF376_CF212) or defined(CE821_CE826) */
//!@}

/*! Enable or disable voice quality measurements for TWS+. */
#define appConfigVoiceQualityMeasurementEnabled() TRUE

/*! The worst reportable voice quality */
#define appConfigVoiceQualityWorst() 0

/*! The best reportable voice quality */
#define appConfigVoiceQualityBest() 15

/*! The voice quality to report if measurement is disabled. Must be in the
    range appConfigVoiceQualityWorst() to appConfigVoiceQualityBest(). */
#define appConfigVoiceQualityWhenDisabled() appConfigVoiceQualityBest()

/*! Minimum volume gain in dB */
#define appConfigMinVolumedB() (-45)

/*! Maximum volume gain in dB */
#define appConfigMaxVolumedB() (0)

/*! This enables support for rendering a 50/50 mono mix of the left/right
    decoded aptX channels when only one earbud is in ear. This feature requires
    a stereo aptX licence since a stereo decode is performed, so it is disabled
    by default. If disabled (set to 0), with aptX, only the left/right channel
    audio will be rendered by the left/right earbud.

    SBC and AAC support stereo mixing by default.
*/
#define appConfigEnableAptxStereoMix() FALSE

#ifdef INCLUDE_APTX_ADAPTIVE
#define appConfigEnableAptxAdaptiveStereoMix() TRUE
#else
#define appConfigEnableAptxAdaptiveStereoMix() FALSE
#endif

/*! This enables support for downmixing aptX adaptive at 96K sample rate */
#define appConfigEnableAptxAdaptiveStereoMix96K() FALSE

/*! Will give significant audio heap savings.
    Achieved by buffering the local channel of the aptX demux output in place of buffering the decoder output.
*/
#define appConfigAptxNoPcmLatencyBuffer() TRUE

/*! Will give significant audio heap savings. Only works when AAC stereo is forwarded.
    Achieved by buffering the decoder input in place of buffering the decoder output.
*/
#define appConfigAacNoPcmLatencyBuffer()  TRUE

/*! Will give significant audio heap savings at the cost of MCPS
    Achieved by re-encoding and re-decoding the latency buffer (which buffers the local channel output of the decoder)
    LOW DSP clock is not enough to run SBC forwarding when this option is enabled
*/
#define appConfigSbcNoPcmLatencyBuffer()  FALSE

/*! After prospectively starting the audio subsystem, the length of time after
    which the audio subsystem will be powered-off again if still inactive */
#define appConfigProspectiveAudioOffTimeout() D_SEC(5)

/*! When the primary/secondary perform the synchronised unmute start
    procedure, this configuration sets the number of samples for the
    mute->unmute transition.
*/
#define appConfigSyncUnmuteTransitionSamples() 1000

/*! If the SCO chain is started muted due to a "synchronised_start", normally
    the function #Kymera_ScheduleScoSyncUnmute will be called to set the time
    at which to unmute. If this function isn't called for some reason, this
    configuration defines the timeout after which kymera will automatically
    unmute its output. */
#define appConfigScoSyncUnmuteTimeoutMs() D_SEC(1)

/*! When the secondary joins an a primary with active A2DP, it starts with its
    audio muted. After synchronising, it unmutes. The firmware indicates
    the time at which the audio will be synchronised. This is a trim time to adjust
    (positivitely) the time at which the output is unmuted to avoid any audible
    glitches.
    \note Must be positive.
*/
#define appConfigSecondaryJoinsSynchronisedTrimMs() 120

/*! For aptX adaptive in Q2Q mode, it is necessary to add some extra latency to the
 *  ttp. This is applied by the TWS+ packetiser that is used in the Q2Q chain.
 *  This is used for all versions of aptx adaptive, other than 2.1
 */
/*! Standard latency adjust for aptx adaptive*/
#define aptxAdaptiveTTPLatencyAdjustStandard() 70
/*! Reduced latency adjust figure used when in gaming mode */
#define aptxAdaptiveTTPLatencyAdjustGaming() 30

/*! If we're using aptX adaptive on P1 then we need to increase the size of the
 * latency buffer before the source sync.
 */
#if defined(INCLUDE_APTX_ADAPTIVE) && defined(INCLUDE_DECODERS_ON_P1)
#define outputLatencyBuffer()  1352
#else
#define outputLatencyBuffer()  0
#endif

/*! For aptX adaptive 2.1, Low Latency and High Quality modes in Q2Q, have different
 *  adjustments from the standard, based on the RTP SSRC sent in the stream
 */

/*! aptX adaptive low latency SSRC */
#define aptxAdaptiveLowLatencyStreamId_SSRC_Q2Q() 0xAD
/*! Latency in ms to add to incoming TTPs when in low latency mode */

#if defined(INCLUDE_STEREO)
#define aptxAdaptiveTTPLatencyAdjustLL() 0
#define aptxAdaptiveTTPLatencyMPAdjustLL() 15
#else
#define aptxAdaptiveTTPLatencyAdjustLL() 5
#define aptxAdaptiveTTPLatencyMPAdjustLL() 20
#endif

/*! aptX adaptive low latency SSRC AOSP LL-0 (2.4G wifi disabled)*/
#define aptxAdaptiveLowLatencyStreamId_SSRC_AOSP_LL_0() 0xA1
/*! aptX adaptive low latency SSRC AOSP LL-1 (2.4G wifi enabled)*/
#define aptxAdaptiveLowLatencyStreamId_SSRC_AOSP_LL_1() 0xA2

/*! aptX adaptive high quality SSRC */
#define aptxAdaptiveHQStreamId_SSRC() 0xAE

#if !defined(INCLUDE_STEREO)
/*! Latency in ms to add to incoming TTPs when in high quality standard mode */
#define aptxAdaptiveTTPLatencyAdjustHQStandard() 110
/*! Latency in ms to add to incoming TTPs when in high quality gaming mode.
    The negative value means the incoming TTPs are advanced lowering the latency */
#define aptxAdaptiveTTPLatencyAdjustHQGaming() -50
#else
/*! Latency in ms to add to incoming TTPs when in high quality standard mode */
#define aptxAdaptiveTTPLatencyAdjustHQStandard() 90
/*! Reduce by 130 ms when in gaming  mode */
#define aptxAdaptiveTTPLatencyAdjustHQGaming() -130
#endif

#endif /* KYMERA_CONFIG_H */

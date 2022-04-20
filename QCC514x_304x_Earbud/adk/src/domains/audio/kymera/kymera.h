/*!
\copyright  Copyright (c) 2017-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   kymera Kymera
\brief      The Kymera Manager API

*/

#ifndef KYMERA_H
#define KYMERA_H

#include <chain.h>
#include <transform.h>
#include <hfp.h>
#include <a2dp.h>
#include <anc.h>
#include <audio_plugin_common.h>
#include <kymera_config.h>
#include <rtime.h>
#include "va_audio_types.h"
#include "aec_leakthrough.h"
#include "ringtone/ringtone_if.h"
#include "file/file_if.h"
#include "usb_audio.h"
#include "kymera_aec.h"
#include "kymera_output_common_chain.h"

#include <kymera_adaptation_audio_protected.h>
#include <kymera_adaptation_voice_protected.h>

/*! Microphone: There are no microphone*/
#define NO_MIC (0)

/*! \brief List of all supported callbacks. */
typedef struct
{
    bool (*GetA2dpParametersPrediction)(uint32 *rate, uint8 *seid);
} kymera_callback_configs_t;



typedef struct
{
    /*! Pointer to the definition of tone being played. */
    const ringtone_note *tone;
} KYMERA_NOTIFICATION_TONE_STARTED_T;

typedef struct
{
    /*! File index of the voice prompt being played. */
    FILE_INDEX id;
} KYMERA_NOTIFICATION_PROMPT_STARTED_T;

typedef struct
{
    /*! Number of the first band being changed */
    uint8 start_band;
    /*! Number of the last band being changed */
    uint8 end_band;
} KYMERA_NOTIFCATION_USER_EQ_BANDS_UPDATED_T;

typedef struct
{
    /* The info variable is  interpreted according to message that it is delievered to clients.
            event_id  for  AANC_EVENT_CLEAR
            gain value for KYMERA_AANC_EVENT_ED_INACTIVE_GAIN_UNCHANGED
            flags recevied for KYMERA_AANC_EVENT_ED_ACTIVE
            NA    when KYMERA_AANC_EVENT_QUIET_MODE
        */
    uint16 info;
} kymera_aanc_event_msg_t;

typedef kymera_aanc_event_msg_t KYMERA_AANC_CLEAR_IND_T;
typedef kymera_aanc_event_msg_t KYMERA_AANC_ED_ACTIVE_TRIGGER_IND_T;
typedef kymera_aanc_event_msg_t KYMERA_AANC_ED_INACTIVE_TRIGGER_IND_T;
typedef kymera_aanc_event_msg_t KYMERA_AANC_QUIET_MODE_TRIGGER_IND_T;
typedef kymera_aanc_event_msg_t KYMERA_AANC_EVENT_IND_T;

/*! \brief Events that Kymera send to its registered clients */
typedef enum kymera_messages
{
    /*! A tone have started */
    KYMERA_NOTIFICATION_TONE_STARTED = KYMERA_MESSAGE_BASE,
    /*! A voice prompt have started */
    KYMERA_NOTIFICATION_PROMPT_STARTED,
    /*! Latency reconfiguration has completed */
    KYMERA_LATENCY_MANAGER_RECONFIG_COMPLETE_IND,
    /*! Latency reconfiguration has failed */
    KYMERA_LATENCY_MANAGER_RECONFIG_FAILED_IND,
    /*! EQ available notification */
    KYMERA_NOTIFICATION_EQ_AVAILABLE,
    /*! EQ unavailable notification */
    KYMERA_NOTIFICATION_EQ_UNAVAILABLE,
    /*! EQ bands updated notification */
    KYMERA_NOTIFCATION_USER_EQ_BANDS_UPDATED,

    KYMERA_AANC_ED_ACTIVE_TRIGGER_IND,
    KYMERA_AANC_ED_INACTIVE_TRIGGER_IND,
    KYMERA_AANC_QUIET_MODE_TRIGGER_IND,
    KYMERA_AANC_ED_ACTIVE_CLEAR_IND,
    KYMERA_AANC_ED_INACTIVE_CLEAR_IND,
    KYMERA_AANC_QUIET_MODE_CLEAR_IND,
    KYMERA_LOW_LATENCY_STATE_CHANGED_IND,

    KYMERA_AANC_BAD_ENVIRONMENT_TRIGGER_IND,
    KYMERA_AANC_BAD_ENVIRONMENT_CLEAR_IND,
    KYMERA_EFT_GOOD_FIT_IND,
    KYMERA_EFT_BAD_FIT_IND,
    KYMERA_PROMPT_END_IND,
    KYMERA_HIGH_BANDWIDTH_STATE_CHANGED_IND,

#ifdef INCLUDE_CVC_DEMO
    KYMERA_NOTIFICATION_CVC_SEND_MODE_CHANGED,
#endif

/*! This must be the final message */
    KYMERA_MESSAGE_END
} kymera_msg_t;

typedef enum
{
    NO_SCO,
    SCO_NB,
    SCO_WB,
    SCO_SWB,
    SCO_UWB
} appKymeraScoMode;

typedef struct
{
    appKymeraScoMode mode;
    uint8 mic_cfg;
    const chain_config_t *chain;
    uint32_t rate;
} appKymeraScoChainInfo;

/*! \brief The prompt file format */
typedef enum prompt_format
{
    PROMPT_FORMAT_PCM,
    PROMPT_FORMAT_SBC,
} promptFormat;

/*! \brief Defines the different codecs used for le audio */
typedef enum
{
    KYMERA_LE_AUDIO_CODEC_LC3
} appKymeraLeAudioCodec;


typedef struct
{
    uint8 mic_cfg;
    const chain_config_t *chain;
    uint16 rate;
    appKymeraLeAudioCodec codec_type;
} appKymeraLeMicChainInfo;

typedef struct
{
    const appKymeraLeMicChainInfo *chain_table;
    unsigned table_length;
} appKymeraLeMicChainTable;

/*! \brief Parameters used to determine the VA encode chain config to use */
typedef struct
{
    va_audio_codec_t encoder;
} kymera_va_encode_chain_params_t;

typedef struct
{
    kymera_va_encode_chain_params_t chain_params;
    const chain_config_t *chain_config;
} appKymeraVaEncodeChainInfo;

typedef struct
{
    const appKymeraVaEncodeChainInfo *chain_table;
    unsigned table_length;
} appKymeraVaEncodeChainTable;

/*! \brief Parameters used to determine the VA WuW chain config to use */
typedef struct
{
    va_wuw_engine_t wuw_engine;
} kymera_va_wuw_chain_params_t;

typedef struct
{
    kymera_va_wuw_chain_params_t chain_params;
    const chain_config_t *chain_config;
} appKymeraVaWuwChainInfo;

typedef struct
{
    const appKymeraVaWuwChainInfo *chain_table;
    unsigned table_length;
} appKymeraVaWuwChainTable;

/*! \brief Parameters used to determine the VA mic chain config to use */
typedef struct
{
    unsigned wake_up_word_detection:1;
    unsigned clear_voice_capture:1;
    uint8 number_of_mics;
} kymera_va_mic_chain_params_t;

typedef struct
{
    kymera_va_mic_chain_params_t chain_params;
    const chain_config_t *chain_config;
} appKymeraVaMicChainInfo;

typedef struct
{
    const appKymeraVaMicChainInfo *chain_table;
    unsigned table_length;
} appKymeraVaMicChainTable;

typedef void (* KymeraVoiceCaptureStarted)(Source capture_source);

/*! \brief Response to a Wake-Up-Word detected indication */
typedef struct
{
    bool start_capture;
    KymeraVoiceCaptureStarted capture_callback;
    va_audio_wuw_capture_params_t capture_params;
} kymera_wuw_detected_response_t;

typedef kymera_wuw_detected_response_t (* KymeraWakeUpWordDetected)(const va_audio_wuw_detection_info_t *wuw_info);

typedef struct
{
    const chain_config_t *chain_config;
} appKymeraLeAudioChainInfo;

typedef struct
{
    const appKymeraLeAudioChainInfo *chain_table;
    unsigned table_length;
} appKymeraLeAudioChainTable;

typedef struct
{
    uint8 mic_count;
    uint16 sample_rate;
    const chain_config_t *chain_config;
} appKymeraLeVoiceChainInfo;

typedef struct
{
    const appKymeraLeVoiceChainInfo *chain_table;
    unsigned table_length;
} appKymeraLeVoiceChainTable;

#define MAX_NUMBER_SUPPORTED_MICS 4

typedef struct
{
    uint16 filter_type;
    uint16 cut_off_freq;
    int16  gain;
    uint16 q;
} kymera_eq_paramter_set_t;

typedef struct
{
    uint8 number_of_bands;
    kymera_eq_paramter_set_t *params;
} kymera_user_eq_bank_t;

typedef struct
{
    usb_voice_mode_t mode;
    uint8 mic_cfg;
    const chain_config_t *chain;
    uint32_t rate;
} appKymeraUsbVoiceChainInfo;

/*! Structure defining a single hardware output */
typedef struct
{
    /*! The type of output hardware */
    audio_hardware hardware;
    /*! The specific hardware instance used for this output */
    audio_instance instance;
    /*! The specific channel used for the output */
    audio_channel  channel;
} appKymeraHardwareOutput;



/*! \brief Start streaming A2DP audio.
    \param client_lock If not NULL, bits set in client_lock_mask will be cleared
           in client_lock when A2DP is started, or if an A2DP stop is requested,
           before A2DP has started.
    \param client_lock_mask A mask of bits to clear in the client_lock.
    \param codec_settings The A2DP codec settings.
    \param max_bitrate The max bitrate for the input stream (in bps). Ignored if zero.
    \param volume_in_db The start volume.
    \param master_pre_start_delay This function always sends an internal message
    to request the module start kymera. The internal message is sent conditionally
    on the completion of other activities, e.g. a tone. The caller may request
    that the internal message is sent master_pre_start_delay additional times before the
    start of kymera commences. The intention of this is to allow the caller to
    delay the starting of kymera (with its long, blocking functions) to match
    the message pipeline of some concurrent message sequence the caller doesn't
    want to be blocked by the starting of kymera. This delay is only applied
    when starting the 'master' (a non-TWS sink SEID).
    \param q2q_mode The source device is a Qualcomm device that supports Q2Q mode
*/
void appKymeraA2dpStart(uint16 *client_lock, uint16 client_lock_mask,
                        const a2dp_codec_settings *codec_settings,
                        uint32 max_bitrate,
                        int16 volume_in_db, uint8 master_pre_start_delay,
                        uint8 q2q_mode, aptx_adaptive_ttp_latencies_t nq2q_ttp);

/*! \brief Stop streaming A2DP audio.
    \param seid The stream endpoint ID to stop.
    \param source The source associatied with the seid.
*/
void appKymeraA2dpStop(uint8 seid, Source source);

/*! \brief Set the A2DP streaming volume.
    \param volume_in_db.
*/
void appKymeraA2dpSetVolume(int16 volume_in_db);

/*! Callback function type for informing caller that SCO chain has started */
typedef void (*Kymera_ScoStartedHandler)(void);

/*! \brief Start SCO audio.
    \param audio_sink The SCO audio sink.
    \param codec WB-Speech codec bit masks.
    \param wesco The link Wesco.
    \param volume_in_db The starting volume.
    \param pre_start_delay This function always sends an internal message
    to request the module start SCO. The internal message is sent conditionally
    on the completion of other activities, e.g. a tone. The caller may request
    that the internal message is sent pre_start_delay additional times before
    starting kymera. The intention of this is to allow the caller to
    delay the start of kymera (with its long, blocking functions) to match
    the message pipeline of some concurrent message sequence the caller doesn't
    want to be blocked by the starting of kymera.
    \param synchronised_start If TRUE, the chain will be started muted.
    #Kymera_ScheduleScoSyncUnmute should be called to define the time
    at which the audio output will be unmuted. Internally, the module guards
    against the user not calling #Kymera_ScheduleScoSyncUnmute by scheduling
    an unmute appConfigScoSyncUnmuteTimeoutMs after the chain is started.
    \param handler Function pointer called when the SCO chain has been started.
*/
bool appKymeraScoStart(Sink audio_sink, appKymeraScoMode mode, uint8 wesco,
                       int16 volume_in_db, uint8 pre_start_delay,
                       bool synchronised_start, Kymera_ScoStartedHandler handler);


/*! \brief Stop SCO audio.
*/
void appKymeraScoStop(void);

/*! \brief Set SCO volume.

    \param[in] volume_in_db.
 */
void appKymeraScoSetVolume(int16 volume_in_db);

/*! \brief Enable or disable MIC muting.

    \param[in] mute TRUE to mute MIC, FALSE to unmute MIC.
 */
void appKymeraScoMicMute(bool mute);

/*! \brief Get the SCO CVC voice quality.
    \return The voice quality.
 */
uint8 appKymeraScoVoiceQuality(void);

/*! \brief Play a tone.
    \param tone The address of the tone to play.
    \param ttp Time to play the audio tone in microseconds.
    \param interruptible If TRUE, the tone may be interrupted by another event
           before it is completed. If FALSE, the tone may not be interrupted by
           another event and will play to completion.
    \param client_lock If not NULL, bits set in client_lock_mask will be cleared
           in client_lock when the tone finishes - either on completion, or when
           interrupted.
    \param client_lock_mask A mask of bits to clear in the client_lock.
*/
void appKymeraTonePlay(const ringtone_note *tone, rtime_t ttp, bool interruptible,
                       uint16 *client_lock, uint16 client_lock_mask);

/*! \brief Play a prompt.
    \param prompt The file index of the prompt to play.
    \param format The prompt file format.
    \param rate The prompt sample rate.
    \param ttp The time to play the audio prompt in microseconds.
    \param interruptible If TRUE, the prompt may be interrupted by another event
           before it is completed. If FALSE, the prompt may not be interrupted by
           another event and will play to completion.
    \param client_lock If not NULL, bits set in client_lock_mask will be cleared
           in client_lock when the prompt finishes - either on completion, or when
           interrupted.
    \param client_lock_mask A mask of bits to clear in the client_lock.
*/
void appKymeraPromptPlay(FILE_INDEX prompt, promptFormat format,
                         uint32 rate, rtime_t ttp, bool interruptible,
                         uint16 *client_lock, uint16 client_lock_mask);

/*! \brief Stop playing an active tone or prompt

    Cancel/stop the currently playing tone or prompt.

    \note This command will only cancel tones and prompts that are allowed
        to be interrupted. This is specified in the interruptible parameter
        used when playing a tone/prompt.

    \note This API should not normally be used. Tones and prompts have a
        limited duration and will end within a reasonable timescale.
        Starting a new tone/prompt will also cancel any currently active tone.
*/
void appKymeraTonePromptCancel(void);


/*! \brief Initialise the kymera module. */
bool appKymeraInit(Task init_task);

/*! \brief Helper function that checks if the Kymera sub-system is idle

    Checking this does not guarantee that a subsequent function call that starts
    kymera activity will succeed.

    \return TRUE if the kymera sub-system was in the idle state at the time
                the function was called, FALSE otherwise.
 */
bool Kymera_IsIdle(void);

/*! \brief Register a Task to receive notifications from Kymera.

    Once registered, #client_task will receive #shadow_profile_msg_t messages.

    \param client_task Task to register to receive shadow_profile notifications.
*/
void Kymera_ClientRegister(Task client_task);

/*! \brief Un-register a Task that is receiving notifications from Kymera.

    If the task is not currently registered then nothing will be changed.

    \param client_task Task to un-register from shadow_profile notifications.
*/
void Kymera_ClientUnregister(Task client_task);



/*! \brief Configure downloadable capabilities bundles.

    This function must be called before appKymeraInit(),
    otherwise no downloadable capabilities will be loaded.

    \param config Pointer to bundle configuration.
*/
void Kymera_SetBundleConfig(const capability_bundle_config_t *config);


/*! \brief Set table used to determine audio chain based on SCO parameters.

    Table set by this function applies to primary TWS device as well as standalone device.

    This function must be called before audio is used.

    \param info SCO audio chains mapping.
*/
void Kymera_SetScoChainTable(const appKymeraScoChainInfo *info);

/*! \brief Set table used to determine audio chain based on VA encode parameters.

    Table set by this function applies to primary TWS device as well as standalone device.

    This function must be called before audio is used.

    \param chain_table VA encode chains mapping.
    */
void Kymera_SetVaEncodeChainTable(const appKymeraVaEncodeChainTable *chain_table);

/*! \brief Set table used to determine audio chain based on VA Mic parameters.

    Table set by this function applies to primary TWS device as well as standalone device.

    This function must be called before audio is used.

    \param chain_table VA mic chains mapping.
    */
void Kymera_SetVaMicChainTable(const appKymeraVaMicChainTable *chain_table);

/*! \brief Set table used to determine audio chain based on VA Wake Up Word parameters.

    Table set by this function applies to primary TWS device as well as standalone device.

    This function must be called before audio is used.

    \param chain_table Wake Up Word chains mapping.
    */
void Kymera_SetVaWuwChainTable(const appKymeraVaWuwChainTable *chain_table);

/*! \brief Set the left/right mixer mode
    \param stereo_lr_mix If TRUE, a 50/50 mix of the left and right stereo channels
    will be output by the mixer to the local device, otherwise, the left/right-ness
    of the earbud will be used to output 100% l/r.
*/
void appKymeraSetStereoLeftRightMix(bool stereo_lr_mix);

/*! \brief Enable or disable an external amplifier.
    \param enable TRUE to enable, FALSE to disable.
*/
void appKymeraExternalAmpControl(bool enable);

/*! Connect parameters for ANC tuning  */
typedef struct
{
    uint32 usb_rate;
    Source spkr_src;
    Sink mic_sink;
    uint8 spkr_channels;
    uint8 mic_channels;
    uint8 frame_size;
} anc_tuning_connect_parameters_t;

/*! Disconnect parameters for ANC tuning  */
typedef struct
{
    Source spkr_src;
    Sink mic_sink;
    void (*kymera_stopped_handler)(Source source);
} anc_tuning_disconnect_parameters_t;

/*! \brief Start Anc tuning procedure.
           Note that Device has to be enumerated as USB audio device before calling this API.
    \param anc tuning connect parameters
    \return void
*/
void KymeraAnc_EnterTuning(const anc_tuning_connect_parameters_t * param);

/*! \brief Stop ANC tuning procedure.
    \param anc tuning disconnect parameters
    \return void
*/
void KymeraAnc_ExitTuning(const anc_tuning_disconnect_parameters_t * param);

/*! \brief Cancel any pending KYMERA_INTERNAL_A2DP_START message.
    \param void
    \return void
*/
void appKymeraCancelA2dpStart(void);

/*! \brief Check if tone is playing.

    \return TRUE if tone is playing.
*/
bool appKymeraIsTonePlaying(void);

/*! \brief Prospectively start the DSP (if not already started).
    After a period, the DSP will be automatically stopped again if no activity
    is started */
void appKymeraProspectiveDspPowerOn(void);

#ifdef INCLUDE_MIRRORING
/*! \brief Set the primary/secondary synchronised start time
 *  \param clock Local clock synchronisation time instant
 */
void appKymeraA2dpSetSyncStartTime(uint32 clock);

/*! \brief Set the primary unmute time during a synchronised unmute.
 *  \param unmute_time Local clock synchronised unmute instant
 */
void appKymeraA2dpSetSyncUnmuteTime(rtime_t unmute_time);

/*!
 * \brief kymera_a2dp_mirror_handover_if
 *
 *        Struct containing interface function pointers for marshalling and handover
 *        operations.  See handover_if library documentation for more information.
 */
extern const handover_interface kymera_a2dp_mirror_handover_if;
#endif /* INCLUDE_MIRRORING */

/*! \brief Unmute of the main output of the SCO chain after a delay.
 *  \param delay The delay after which to unmute.
 */
void Kymera_ScheduleScoSyncUnmute(Delay delay);

/*! \brief Connects passthrough operator to dac in order to mitigate tonal noise
    observed in QCC512x devices*/
void KymeraAnc_ConnectPassthroughSupportChainToDac(void);

/*! \brief Disconnects passthrough operator from dac used in conjunction with
     KymeraAncConnectPassthroughSupportChainToDac(void) */
void KymeraAnc_DisconnectPassthroughSupportChainFromDac(void);

/*! \brief Creates passthrough operator support chain used for mitigation of tonal
    noise observed with QCC512x devices*/
void KymeraAnc_CreatePassthroughSupportChain(void);

/*! \brief Destroys passthrough operator support chain which could have been used with
    Qcc512x devices for mitigating tonal noise*/
void KymeraAnc_DestroyPassthroughSupportChain(void);

/*! \brief Start voice capture.
    \param callback Called when capture has started.
    \param params Parameters based on which the voice capture will be configured.
*/
void Kymera_StartVoiceCapture(KymeraVoiceCaptureStarted callback, const va_audio_voice_capture_params_t *params);

/*! \brief Stop voice capture.
*/
void Kymera_StopVoiceCapture(void);

/*! \brief Start Wake-Up-Word detection.
    \param callback Called when Wake-Up-Word is detected, a voice capture can be started based on the return.
    \param params Parameters based on which the Wake-Up-Word detection will be configured.
*/
void Kymera_StartWakeUpWordDetection(KymeraWakeUpWordDetected callback, const va_audio_wuw_detection_params_t *params);

/*! \brief Stop Wake-Up-Word detection.
*/
void Kymera_StopWakeUpWordDetection(void);

/*! \brief Get the version number of the WuW engine operator.
    \param wuw_engine The ID of the engine.
    \param version The version number to be populated.
*/
void Kymera_GetWakeUpWordEngineVersion(va_wuw_engine_t wuw_engine, va_audio_wuw_engine_version_t *version);

/*! \brief Store the WuW engine with the largest PM allocation requirements.
*/
void Kymera_StoreLargestWuwEngine(void);

/*! \brief Updates the DSP clock to the fastest possible speed in case of
 * A2DP and SCO before enabling the ANC and changing the mode and revert back to the previous
 * clock speed later to reduce the higher latency in peer sync
 */
void KymeraAnc_UpdateDspClock(void);

/*! \brief Register for notification

    Registered task will receive KYMERA_NOTIFICATION_* messages.

    As it is intended to be used by test system it supports only one client.
    That is to minimise memory use.
    It can be extended to support arbitrary number of clients when necessary.

    \param task Listener task
*/
void Kymera_RegisterNotificationListener(Task task);

/*! Kymera API references for software leak-through */
/*! \brief Enables the leakthrough */
#ifdef ENABLE_AEC_LEAKTHROUGH
void Kymera_EnableLeakthrough(void);
#else
#define Kymera_EnableLeakthrough() ((void)(0))
#endif

/*! \brief Disable the leakthrough */
#ifdef ENABLE_AEC_LEAKTHROUGH
void Kymera_DisableLeakthrough(void);
#else
#define Kymera_DisableLeakthrough() ((void)(0))
#endif

/*! \brief Notify leakthough of a change in leakthrough mode */
#ifdef ENABLE_AEC_LEAKTHROUGH
void Kymera_LeakthroughUpdateMode(leakthrough_mode_t mode);
#else
#define Kymera_LeakthroughUpdateMode(x) ((void)(0))
#endif

/*! \brief Update leakthrough for AEC use case */
#ifdef ENABLE_AEC_LEAKTHROUGH
void Kymera_LeakthroughSetAecUseCase(aec_usecase_t usecase);
#else
#define Kymera_LeakthroughSetAecUseCase(x) ((void)(0))
#endif

/*! \brief Tries to enable the Adaptive ANC chain */
void Kymera_EnableAdaptiveAnc(bool in_ear, audio_anc_path_id path, adaptive_anc_hw_channel_t hw_channel, anc_mode_t mode);

/*! \brief Tries to disable the Adaptive ANC chain */
void Kymera_DisableAdaptiveAnc(void);

/*! \brief Returns if Adaptive ANC is enabled based on Kymera state */
bool Kymera_IsAdaptiveAncEnabled(void);

/*! \brief Obtain Current Adaptive ANC mode from AANC operator
    \param aanc_mode - pointer to get the value
    \return TRUE if current mode is stored in aanc_mode, else FALSE
*/
bool Kymera_ObtainCurrentApdativeAncMode(adaptive_anc_mode_t *aanc_mode);

/*! \brief Identify if noise level is below Quiet Mode threshold
    \param void
    \return TRUE if noise level is below threshold, otherwise FALSE
*/
bool Kymera_AdaptiveAncIsNoiseLevelBelowQuietModeThreshold(void);

/*! \brief Set up loop back from Mic input to DAC when kymera is ON

    \param mic_number select the mic to loop back
    \param sample_rate set the sampling rate for both input and output chain
 */
void appKymeraCreateLoopBackAudioChain(microphone_number_t mic_number, uint32 sample_rate);

/*! \brief Destroy loop back from Mic input to DAC chain

    \param mic_number select the mic to loop back

 */
void appKymeraDestroyLoopbackAudioChain(microphone_number_t mic_number);

/*! \brief Starts the wired analog audio chain
      \param volume_in_db Volume for wired analog
      \param rate sample rate at which wired analog audio needs to be played
      \param min_latency minimum latency identified for wired audio
      \param max_latency maximum latency identified for wired audio
      \param target_latency fixed latency identified for wired audio
 */
void Kymera_StartWiredAnalogAudio(int16 volume_in_db, uint32 rate, uint32 min_latency, uint32 max_latency, uint32 target_latency);

/*! \brief Stop the wired analog audio chain
 */
void Kymera_StopWiredAnalogAudio(void);

/*! \brief Set volume for Wired Audio chain.
    \param volume_in_db Volume to be set.
*/
void appKymeraWiredAudioSetVolume(int16 volume_in_db);

/*! \brief Set table used to determine audio chain based on LE Audio parameters.

    This function must be called before audio is used.

    \param info LE audio chains mapping.
*/
void Kymera_SetLeAudioChainTable(const appKymeraLeAudioChainTable *chain_table);

/*! \brief Start streaming LE audio.

    \param media_present speaker/music configuration present.
    \param microphone_present microphone configuration present
    \param volume_in_db The start volume.
    \param le_media_config_t media configuration
    \param le_microphone_config_t microphone configuration
*/
void Kymera_LeAudioStart(bool media_present, bool microphone_present,
                        int16 volume_in_db, const le_media_config_t *media,
                        const le_microphone_config_t *microphone);

/*! \brief Stop streaming LE audio.
*/
void Kymera_LeAudioStop(void);

/*! \brief Set the LE audio volume.
    \param volume_in_db.
*/
void Kymera_LeAudioSetVolume(int16 volume_in_db);

/*! \brief Unmute the LE audio output at a given time.

    \param unmute_time Time to unmute the LE audio output.
 */
void Kymera_LeAudioUnmute(rtime_t unmute_time);

/*! \brief Set table used to determine audio chain based on LE Voice parameters.

    This function must be called before LE voice is used.

    \param info LE voice chains mapping.
*/


void Kymera_SetLeVoiceChainTable(const appKymeraLeVoiceChainTable *chain_table);

/*! \brief Start streaming LE voice.

    \param source_iso_handle The ISO handle for the source.
    \param sink_iso_handle The ISO handle for the sink
    \param volume_in_db The start volume.
    \param le_voice_config_t LE voice configuration
*/
void Kymera_LeVoiceStart(uint16 source_iso_handle, uint16 sink_iso_handle, int16 volume_in_db, const le_voice_config_t *le_voice_config);

/*! \brief Stop streaming LE voice.
*/
void Kymera_LeVoiceStop(void);

/*! \brief Set the LE voice volume.
    \param volume_in_db.
*/
void Kymera_LeVoiceSetVolume(int16 volume_in_db);

/*! \brief Enable or disable MIC muting.

    \param[in] mute TRUE to mute MIC, FALSE to unmute MIC.
 */
void Kymera_LeVoiceMicMute(bool mute);

/*! \brief Mute to allow a syncronised unmute.

    This function will mute an already playing le broadcast stream.
    It can be un-muted with Kymera_LeAudioUnmute()
*/
void Kymera_LeAudioSyncMute(void);

/*! \brief Create and start USB Audio.
    \param channels USB Audio channels
    \param frame_size 16 bit/24bits.
    \param Sink USB OUT endpoint sink.
    \param volume_in_db Intal volume
    \param rate Sample Frequency.
    \param min_latency TTP minimum value in micro-seconds
    \param max_latency TTP max value in micro-seconds
    \param target_latency TTP default value in micro-seconds
*/
void appKymeraUsbAudioStart(uint8 channels, uint8 frame_size,
                            Source src, int16 volume_in_db,
                            uint32 rate, uint32 min_latency, uint32 max_latency,
                            uint32 target_latency);

/*! \brief Stop and destroy USB Audio chain.
    \param Source USB OUT endpoint source.
    \param kymera_stopped_handler Handler to call when Kymera chain is destroyed.
*/
void appKymeraUsbAudioStop(Source usb_src,
                           void (*kymera_stopped_handler)(Source source));

/*! \brief Set volume for USB Audio chain.
    \param volume_in_db Volume to be set.
*/
void appKymeraUsbAudioSetVolume(int16 volume_in_db);

/*! \brief Create and start USB Voice.
    \param cvc_2_mic 1-mic or 2-mic cvc config
    \param mic_sink Sink for mic USB TX endpoint.
    \param mode Type of mode (NB/WB etc)
    \param rate Sample Frequency.
    \param spkr_src Speaker Source of USB RX endpoint
    \param volume_in_db Initial volume
    \param min_latency TTP minimum value in micro-seconds
    \param max_latency TTP max value in micro-seconds
    \param target_latency TTP default value in micro-seconds
    \param kymera_stopped_handler Handler to call when Kymera chain is destroyed in the case of chain could not be started.
*/
void appKymeraUsbVoiceStart(usb_voice_mode_t mode, uint8 spkr_channels, uint32 spkr_sample_rate,
                            uint32 mic_sample_rate, Source spkr_src, Sink mic_sink, int16 volume_in_db,
                            uint32 min_latency, uint32 max_latency, uint32 target_latency,
                            void (*kymera_stopped_handler)(Source source));

/*! \brief Stop and destroy USB Audio chain.
    \param spkr_src USB OUT (from host) endpoint source.
    \param mic_sink USB IN (to host) endpoint sink.
    \param kymera_stopped_handler Handler to call when Kymera chain is destroyed.
*/
void appKymeraUsbVoiceStop(Source spkr_src, Sink mic_sink,
                           void (*kymera_stopped_handler)(Source source));

/*! \brief Set USB Voice volume.

    \param[in] volume_in_db.
 */
void appKymeraUsbVoiceSetVolume(int16 volume_in_db);

/*! \brief Enable or disable MIC muting.

    \param[in] mute TRUE to mute MIC, FALSE to unmute MIC.
*/
void appKymeraUsbVoiceMicMute(bool mute);

/*! \brief Prepare chains required to play prompt.

    \param[in] format The format of the prompt to prepare for
    \param[in] sample_rate The sample rate of the prompt to prepare for

    \return TRUE if chains prepared, otherwise FALSE
 */
bool Kymera_PrepareForPrompt(promptFormat format, uint16 sample_rate);

/*! \brief Check if chains have been prepared for prompt.

    \param[in] format The format of the prompt to play
    \param[in] sample_rate The sample rate of the prompt to play

    \return TRUE if ready for prompt, otherwise FALSE
 */
bool Kymera_IsReadyForPrompt(promptFormat format, uint16 sample_rate);

/*! \brief Get the stream transform connecting the A2DP media source to kymera
    \return The transform, or 0 if the A2DP audio chains are not active.
    \note This function will always return 0 if INCLUDE_MIRRORING is undefined.
*/
Transform Kymera_GetA2dpMediaStreamTransform(void);

void Kymera_SetA2dpOutputParams(a2dp_codec_settings * codec_settings);
void Kymera_ClearA2dpOutputParams(void);
bool Kymera_IsA2dpOutputPresent(void);

#ifndef INCLUDE_MIRRORING
/*! \brief Get the source application target latency value
    \return The target latecy.
*/
unsigned appKymeraGetCurrentLatency(void);

/*! \brief Set the target latency using the VM_TRANSFORM_PACKETISE_TTP_DELAY message.

    \param[in] target_latency - value to use for target latency.
*/
void appKymeraSetTargetLatency(uint16_t target_latency);
#endif

/*! \brief get the status of audio synchronization state
    \return TRUE if audio synchronization is completed, otherwise FALSE
*/
bool Kymera_IsA2dpSynchronisationNotInProgress(void);

/* When A2DP stars it is using eq_bank so select bank in EQ and select corresponding UCID.
   EQ presists its paramters.
   Kymera_SetUserEqBands() call is only required to change paramters,
   it is not required for initial setup of EQ as previously stored values will be used.*/

/* Returns numeber of User EQ bands.
   It doesn't require EQ to be running. */
/* Numebr of bands is #defined in kymera_config.h */
uint8 Kymera_GetNumberOfEqBands(void);

uint8 Kymera_GetNumberOfEqBanks(void);

/* Returns currently selected User EQ bank, applies to presets as well as user bank.
   It doesn't require EQ to be running */
uint8 Kymera_GetSelectedEqBank(void);

bool Kymera_SelectEqBank(uint32 delay_ms, uint8 bank);

/* Set gains for range of bands.
   It automatically switchies to user eq bank if other bank is selected.
   It requires EQ to be running.

   Returns FALSE if EQ is not running*/
bool Kymera_SetUserEqBands(uint32 delay_ms, uint8 start_band, uint8 end_band, int16 * gains);

void Kymera_GetEqBandInformation(uint8 band, kymera_eq_paramter_set_t *param_set);

/* Sends a message containg complete set of user EQ paramters.
   It requires EQ to be running.

   Returns FALSE if EQ is not running*/
bool Kymera_RequestUserEqParams(Task task);

/* Writes selected parts (like EQ bank) of kymera state to a ps key */
void Kymera_PersistState(void);

/* Checks if user eq is active */
uint8 Kymera_UserEqActive(void);

void Kymera_GetEqParams(uint8 band);
bool Kymera_ApplyGains(uint8 start_band, uint8 end_band);

/*! \brief Populate array of available presets

    It will populate presets array with ids of defined presets.
    It will scan audio ps keys to find which presets are defined,
    that takes time.

    Note that this functions ignores preset 'flat' UCID 1 and 'user eq' UCID 63.
    Only presets located between above are taken into account.

    When presets == NULL then it just returns number of presets.

    \param presets Array to be populated. In needs to be big enough to hold all preset ids.

    \return Number of presets defined.
*/
uint8 Kymera_PopulatePresets(uint8 *presets);


/*! \brief Populate all callback configurations for kymera.

    \param callback configs struct.
*/
void Kymera_SetCallbackConfigs(const kymera_callback_configs_t *configs);

/*! \brief Get a pointer to the callback configuration.

    \return callback configs struct.
*/
const kymera_callback_configs_t *Kymera_GetCallbackConfigs(void);

/*! \brief Check if Q2Q mode is enabled.

    \return TRUE if Q2Q mode is enabled.
*/
bool Kymera_IsQ2qModeEnabled(void);

typedef enum
{
    KYMERA_CVC_NOTHING_SET = 0,
    KYMERA_CVC_RECEIVE_FULL_PROCESSING = (1 << 0),
    KYMERA_CVC_RECEIVE_PASSTHROUGH = (1 << 1),
    KYMERA_CVC_SEND_FULL_PROCESSING = (1 << 2),
    KYMERA_CVC_SEND_PASSTHROUGH = (1 << 3),
} kymera_cvc_mode_t;

/*! \brief Sets SCO CVC Send and/or CVC Receive mode to Pass-Through or Full Processing
 *  \param mode full processing or pass-through mode for cvc send and cvc receive operator
 *         passthrough_mic microphone to pass through in case of pass-through mode
 *  \return TRUE when settings have changed
 */
bool Kymera_ScoSetCvcPassthroughMode(kymera_cvc_mode_t mode,
                                     uint8 passthrough_mic);
void Kymera_ScoSetCvcPassthroughInChain(kymera_chain_handle_t chain_containing_cvc,
                                        kymera_cvc_mode_t mode,
                                        uint8 passthrough_mic);

/*! \brief Reads give operator status data in the sco chain.
 *  \param Sco chain operator roles ( e.g. OPR_SCO_RECEIVE).
 *         Number of parameters in the operator status data.
 *  \return operator status data
 */
get_status_data_t* Kymera_GetOperatorStatusDataInScoChain(unsigned operator_role, uint8 number_of_params);

#ifdef INCLUDE_CVC_DEMO
/*! \brief Gets SCO CVC Send and CVC Receive Passthrough mode. */
void Kymera_ScoGetCvcPassthroughMode(kymera_cvc_mode_t *mode, uint8 *passthrough_mic);

/*! \brief Sets the microphone configuration in 3Mic CVC Send:
 *  \param mic_mode 1: 1-mic config using the omni mode setting
 *                  2: 2-mic config. Use this setting in combination with HW leakthrough
 *                  3: 3-mic config. Normal operation
 *  \return TRUE when settings have changed
 */
bool Kymera_ScoSetCvcSend3MicMicConfig(uint8 mic_mode);

/*! \brief Gets the microphone configuration from 3Mic CVC Send */
void Kymera_ScoGetCvcSend3MicMicConfig(uint8 *mic_config);

/*! \brief Gets the 3Mic CVC Send mode of operation (2Mic or 3Mic mode) */
void Kymera_ScoGetCvcSend3MicModeOfOperation(uint8 *mode_of_operation);

/*! \brief Polls the 3Mic CVC Send mode of operation (2Mic or 3Mic mode) */
void Kymera_ScoPollCvcSend3MicModeOfOperation(void);
#endif /*INCLUDE_CVC_DEMO*/

/*@}*/

#endif /* KYMERA_H */

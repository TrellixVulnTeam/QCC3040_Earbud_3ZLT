/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Definition of kymeraTaskData.
 
Definition of kymera data and data types needed only by kymeraTaskData.
*/

#ifndef KYMERA_DATA_H_
#define KYMERA_DATA_H_

#include <message.h>
#include <task_list.h>
#include <chain.h>
#include <a2dp.h>
#include "kymera.h"
#include "kymera_chain_config_callbacks.h"
#include "kymera_state_types.h"

/*@{*/

#define Kymera_IsMusicState(state)  (((state >= KYMERA_STATE_A2DP_STARTING_A) && (state <= KYMERA_STATE_A2DP_STREAMING_WITH_FORWARDING)) \
                                        || state == KYMERA_STATE_LE_AUDIO_ACTIVE)

#ifdef INCLUDE_MIRRORING
/*! \brief Enumeration of kymera audio sync states */
typedef enum
{
    /*!  default state */
    KYMERA_AUDIO_SYNC_STATE_INIT,
    /*! audio sync is in progress */
    KYMERA_AUDIO_SYNC_STATE_IN_PROGRESS,
     /*! audio sync has completed*/
    KYMERA_AUDIO_SYNC_STATE_COMPLETE
}appKymeraAudioSyncState;

/*! \brief Enumeration of kymera audio sync states. These are equivalent to the
    mirror_profile_a2dp_start_mode_t states. */
typedef enum
{
    KYMERA_AUDIO_SYNC_START_PRIMARY_UNSYNCHRONISED,
    KYMERA_AUDIO_SYNC_START_PRIMARY_SYNCHRONISED,
    KYMERA_AUDIO_SYNC_START_SECONDARY_SYNCHRONISED,
    KYMERA_AUDIO_SYNC_START_PRIMARY_SYNC_UNMUTE,
    KYMERA_AUDIO_SYNC_START_SECONDARY_SYNC_UNMUTE,
    KYMERA_AUDIO_SYNC_START_Q2Q
}appKymeraAudioSyncStartMode;

/*! \brief Kymera audio synchronisation information structure */
typedef struct
{
    appKymeraAudioSyncStartMode mode;
    appKymeraAudioSyncState state;
    Source source;
} appKymeraAudioSyncInfo;
#endif /* INCLUDE_MIRRORING */

typedef struct
{
    uint8 selected_eq_bank;
    uint8 number_of_presets;
    kymera_user_eq_bank_t user;
} kymera_user_eq_data_t;

/*! \brief Kymera instance structure.

    This structure contains all the information for Kymera audio chains.
*/
typedef struct
{
    /*! The kymera module's task. */
    TaskData          task;
    /*! The current state. */
    appKymeraState    state;
    /*! List of tasks registered for notifications */
    task_list_t * client_tasks;

    /*! Task registered to receive notifications. */
    task_list_t * listeners;

    /*! The input chain is used in TWS master and slave roles for A2DP streaming
        and is typified by containing a decoder. */
    kymera_chain_handle_t chain_input_handle;
    /*! The tone chain is used when a tone is played. */
    kymera_chain_handle_t chain_tone_handle;

    /*! The music processing chain. It implements things like EQ.
        It is inserted between input and output chains. */
    kymera_chain_handle_t chain_music_processing_handle;

    /*! The output chain usually contains at least OPR_SOURCE_SYNC/OPR_VOLUME_CONTROL.
        Its used to connect input chains (e.g. audio, music, voice) to the speaker/DACs.
        The OPR_VOLUME_CONTROL provides an auxiliary port where a secondary chain
        e.g. a prompt chain can be mixed in.
    */
    kymera_chain_handle_t chain_output_handle;

#ifdef INCLUDE_MIRRORING
    union{
        /*! The TWM hash transform generates a unique identifier for packets received
         * over the air from A2DP source device. It can be configured to construct and
         * append a RTP header to packet before writing them to the audio subsystem.
         */
        Transform hash;
        /* P0 Packetiser for use with Q2Q Mode */
        Transform packetiser;
    }hashu;

    /*! TWM convert clock transform used to convert TTP info (in local system
     * time) available in source stream into bluetooth wallclock time before
     * writing to sink stream.
     */
    Transform convert_ttp_to_wc;

    /*! TWM convert clock transform used to convert TTP info (in bluetooth wallclock
     * time) available in source stream into local system time before writing to sink
     * stream.
     */
    Transform convert_wc_to_ttp;

    /* Audio sync information */
    appKymeraAudioSyncInfo sync_info;

#else
    /*! The TWS master packetiser transform packs compressed audio frames
        (SBC, AAC, aptX) from the audio subsystem into TWS packets for transmission
        over the air to the TWS slave.
        The TWS slave packetiser transform receives TWS packets over the air from
        the TWS master. It unpacks compressed audio frames and writes them to the
        audio subsystem. */
    Transform packetiser;
#endif /* INCLUDE_MIRRORING */

    /* A2DP media source */
    Source media_source;

    /*! The current output sample rate. */
    uint32 output_rate;

    /*! A lock bitfield. Internal messages are typically sent conditionally on
        this lock meaning events are queued until the lock is cleared. */
    uint16 lock;
    uint16 busy_lock;

    /*! The current A2DP stream endpoint identifier. */
    uint8  a2dp_seid;

    /*! The current playing tone client's lock. */
    uint16 *tone_client_lock;

    /*! The current playing tone client lock mask - bits to clear in the lock
         when the tone is stopped. */
    uint16 tone_client_lock_mask;

    /*! Number of tones/prompts playing and queued up to be played */
    uint8 tone_count;

    const appKymeraScoChainInfo *sco_info;

    /*! The prompt file source whilst prompt is playing */
    Source prompt_source;

    anc_mic_params_t anc_mic_params;
    uint8 dac_amp_usage;

    /*! ANC tuning state */
    uint16 usb_rate;
    BundleID anc_tuning_bundle_id;

#ifdef DOWNLOAD_USB_AUDIO
    BundleID usb_audio_bundle_id;
#endif
    Operator usb_rx, anc_tuning, output_splitter, usb_tx;

#ifdef ENABLE_ADAPTIVE_ANC
    BundleID aanc_tuning_bundle_id;
    Operator aanc_tuning;
#endif

    /* If TRUE, a mono mix of the left/right audio channels will be rendered.
       If FALSE, either the left or right audio channel will be rendered. */
    unsigned enable_left_right_mix : 1;

    unsigned cp_header_enabled : 1;

    unsigned enable_cvc_passthrough : 1;

    /* aptx adaptive split tx mode */
    unsigned split_tx_mode : 1;

    unsigned q2q_mode;
#ifdef INCLUDE_ANC_PASSTHROUGH_SUPPORT_CHAIN
    /*! In Standalone ANC (none audio chains active) the passthrough operator will be connected to a DAC to suppress spurious tones  */
    Operator anc_passthough_operator;
#endif
    a2dp_codec_settings * a2dp_output_params;
#ifndef INCLUDE_MIRRORING
    uint16 source_latency_adjust;
#endif
    kymera_user_eq_data_t eq;

    Sink sink;

    kymera_chain_config_callbacks_t *chain_config_callbacks;
} kymeraTaskData;

/*!< State data for the DSP configuration */
extern kymeraTaskData  app_kymera;

/*! Get pointer to Kymera structure */
#define KymeraGetTaskData()  (&app_kymera)

/*! Get task from the Kymera structure */
#define KymeraGetTask()    (&app_kymera.task)

/*! \brief Get current Seid */
#define Kymera_GetCurrentSeid() (KymeraGetTaskData()->a2dp_seid)

/*@}*/

#endif /* KYMERA_DATA_H_ */

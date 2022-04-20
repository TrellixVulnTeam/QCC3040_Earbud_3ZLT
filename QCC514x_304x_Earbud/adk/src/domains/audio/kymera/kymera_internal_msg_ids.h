/*!
\copyright  Copyright (c) 2019-2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Kymera private header with internal message ids

*/

#ifndef KYMERA_INTERNAL_MSG_IDS_H_
#define KYMERA_INTERNAL_MSG_IDS_H_

#include <domain_message.h>

/*!@{ \name Internal message IDs */
enum app_kymera_internal_message_ids
{
    /*! Internal A2DP start message. */
    KYMERA_INTERNAL_A2DP_START = INTERNAL_MESSAGE_BASE,
    /*! Internal A2DP starting message. */
    KYMERA_INTERNAL_A2DP_STARTING,
    /*! Internal A2DP stop message. */
    KYMERA_INTERNAL_A2DP_STOP,
    /*! Internal A2DP stop forwarding message. */
    KYMERA_INTERNAL_A2DP_STOP_FORWARDING,
    /*! Internal A2DP set volume message. */
    KYMERA_INTERNAL_A2DP_SET_VOL,
    /*! Internal SCO start message, including start of SCO forwarding (if supported). */
    KYMERA_INTERNAL_SCO_START,
    /*! Internal message to set SCO volume */
    KYMERA_INTERNAL_SCO_SET_VOL,
    /*! Internal SCO stop message. */
    KYMERA_INTERNAL_SCO_STOP,
    /*! Internal SCO microphone mute message. */
    KYMERA_INTERNAL_SCO_MIC_MUTE,
    /*! Internal tone play message. */
    KYMERA_INTERNAL_TONE_PROMPT_PLAY,
    /*! Internal ANC tuning start message */
    KYMERA_INTERNAL_ANC_TUNING_START,
    /*! Internal ANC tuning stop message */
    KYMERA_INTERNAL_ANC_TUNING_STOP,
    /*! Internal Adaptive ANC tuning start message */
    KYMERA_INTERNAL_ADAPTIVE_ANC_TUNING_START,
    /*! Internal Adaptive ANC tuning stop message */
    KYMERA_INTERNAL_ADAPTIVE_ANC_TUNING_STOP,
    /*! Internal Adaptive ANC enable message */
    KYMERA_INTERNAL_AANC_ENABLE,
    /*! Internal Adaptive ANC disable message */
    KYMERA_INTERNAL_AANC_DISABLE,
    /*! Disable the audio SS (used for the DAC disable) */
    KYMERA_INTERNAL_AUDIO_SS_DISABLE,
    /*! Internal A2DP data sync indication timeout message */
    KYMERA_INTERNAL_A2DP_DATA_SYNC_IND_TIMEOUT,
    /*! Internal message indicating timeout waiting to receive #MESSAGE_MORE_DATA */
    KYMERA_INTERNAL_A2DP_MESSAGE_MORE_DATA_TIMEOUT,
    /*! Internal A2DP audio synchronisation message */
    KYMERA_INTERNAL_A2DP_AUDIO_SYNCHRONISED,
    /*! Internal eSCO audio synchronised message */
    KYMERA_INTERNAL_SCO_AUDIO_SYNCHRONISED,
    /*! Internal kick to switch off audio subsystem after prospective DSP start */
    KYMERA_INTERNAL_PROSPECTIVE_POWER_OFF,
    /*! Internal AEC LEAKTHROUGH Standalone chain creation message */
    KYMERA_INTERNAL_AEC_LEAKTHROUGH_CREATE_STANDALONE_CHAIN,
    /*! Internal AEC LEAKTHROUGH Standalone chain destroy message */
    KYMERA_INTERNAL_AEC_LEAKTHROUGH_DESTROY_STANDALONE_CHAIN,
    /*! Internal LEAKTHROUGH Side tone enable message */
    KYMERA_INTERNAL_AEC_LEAKTHROUGH_SIDETONE_ENABLE,
    /*! Internal LEAKTHROUGH Side tone gain for ramp up algorithm */
    KYMERA_INTERNAL_AEC_LEAKTHROUGH_SIDETONE_GAIN_RAMPUP,
    /*! Internal message to retry mic connection for client LeakThrough */
    KYMERA_INTERNAL_MIC_CONNECTION_TIMEOUT_LEAKTHROUGH,
    /*! Message to trigger A2DP Audio Latency adjustment */
    KYMERA_INTERNAL_LATENCY_CHECK_TIMEOUT,
    /*! Message to trigger muting of audio during fast latency adjustment */
    KYMERA_INTERNAL_LATENCY_MANAGER_MUTE,
    /*! Message to inform completion of mute duraiton */
    KYMERA_INTERNAL_LATENCY_MANAGER_MUTE_COMPLETE,
    /*! Message to trigger Latency reconfigure */
    KYMERA_INTERNAL_LATENCY_RECONFIGURE,
    /*! Internal tone play message. */
    KYMERA_INTERNAL_TONE_PROMPT_STOP,
    /*! Internal message to start wired analog audio */
    KYMERA_INTERNAL_WIRED_ANALOG_AUDIO_START,
    /*! Internal message to stop wired analog audio */
    KYMERA_INTERNAL_WIRED_ANALOG_AUDIO_STOP,
    /*! Internal message to set wired audio volume*/
    KYMERA_INTERNAL_WIRED_AUDIO_SET_VOL,
    /*! Internal LE Audio start message */
    KYMERA_INTERNAL_LE_AUDIO_START,
    /*! Internal LE Audio stop message */
    KYMERA_INTERNAL_LE_AUDIO_STOP,
    /*! Internal LE Audio set volume message */
    KYMERA_INTERNAL_LE_AUDIO_SET_VOLUME,
    /*! Internal LE Audio mute message */
    KYMERA_INTERNAL_LE_AUDIO_MUTE,
    /*! Internal LE Audio un-mute message */
    KYMERA_INTERNAL_LE_AUDIO_UNMUTE,
    /*! Internal LE Voice start message */
    KYMERA_INTERNAL_LE_VOICE_START,
    /*! Internal LE Voice stop message */
    KYMERA_INTERNAL_LE_VOICE_STOP,
    /*! Internal LE Voice set volume message */
    KYMERA_INTERNAL_LE_VOICE_SET_VOLUME,
    /*! Internal LE Voice microphone mute message */
    KYMERA_INTERNAL_LE_VOICE_MIC_MUTE,
    /*! Internal USB Audio start message. */
    KYMERA_INTERNAL_USB_AUDIO_START,
    /*! Internal USB Audio stop message. */
    KYMERA_INTERNAL_USB_AUDIO_STOP,
    /*! Internal USB Audio set volume message. */
    KYMERA_INTERNAL_USB_AUDIO_SET_VOL,
    /*! Internal USB Voice start message. */
    KYMERA_INTERNAL_USB_VOICE_START,
    /*! Internal USB Voice stop message. */
    KYMERA_INTERNAL_USB_VOICE_STOP,
    /*! Internal message to set USB Voice volume */
    KYMERA_INTERNAL_USB_VOICE_SET_VOL,
    /*! Internal USB Voice microphone mute message. */
    KYMERA_INTERNAL_USB_VOICE_MIC_MUTE,
    /*! Internal message indicating timeout waiting for prompt play */
    KYMERA_INTERNAL_PREPARE_FOR_PROMPT_TIMEOUT,
    /*! Internal message to retry mic connection for client ANC */
    KYMERA_INTERNAL_MIC_CONNECTION_TIMEOUT_ANC,
    /*! Internal message to trigger check for Aptx Adaptive Low Latency Stream */
    KYMERA_INTERNAL_LOW_LATENCY_STREAM_CHECK,
#if defined(INCLUDE_MUSIC_PROCESSING)
    /*! Internal message to select a new user EQ bank */
    KYMERA_INTERNAL_USER_EQ_SELECT_EQ_BANK,
    /*! Internal message to set new gains for the user EQ */
    KYMERA_INTERNAL_USER_EQ_SET_USER_GAINS,
    KYMERA_INTERNAL_USER_EQ_APPLY_GAINS,
#endif /* INCLUDE_MUSIC_PROCESSING */
    /*! Internal message to poll 3Mic cVc mode of operation */
    KYMERA_INTERNAL_CVC_3MIC_POLL_MODE_OF_OPERATION,

    /*! This must be the final message */
    KYMERA_INTERNAL_MESSAGE_END
};
/*!@}*/

ASSERT_INTERNAL_MESSAGES_NOT_OVERFLOWED(KYMERA_INTERNAL_MESSAGE_END)

#endif /* KYMERA_INTERNAL_MSG_IDS_H_ */

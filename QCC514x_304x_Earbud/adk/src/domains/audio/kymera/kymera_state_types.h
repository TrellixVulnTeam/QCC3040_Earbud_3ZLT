/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Kymera private header with internal state types
*/

#ifndef KYMERA_STATE_TYPES_H_
#define KYMERA_STATE_TYPES_H_

/*! \brief The kymera module states. */
typedef enum app_kymera_states
{
    /*! Kymera is idle. */
    KYMERA_STATE_IDLE,
    /*! Starting master A2DP kymera in three steps. */
    KYMERA_STATE_A2DP_STARTING_A,
    KYMERA_STATE_A2DP_STARTING_B,
    KYMERA_STATE_A2DP_STARTING_C,
    /*! Kymera is streaming A2DP locally. */
    KYMERA_STATE_A2DP_STREAMING,
    /*! Kymera is streaming A2DP locally and forwarding to the slave. */
    KYMERA_STATE_A2DP_STREAMING_WITH_FORWARDING,
    /*! Kymera is streaming SCO locally. */
    KYMERA_STATE_SCO_ACTIVE,
    /*! Kymera is receiving forwarded SCO over a link */
    KYMERA_STATE_SCO_SLAVE_ACTIVE,
    /*! Kymera is playing a tone or a prompt. */
    KYMERA_STATE_TONE_PLAYING,
    /*! Kymera is performing ANC tuning. */
    KYMERA_STATE_ANC_TUNING,
    /*! Kymera is performing Adaptive ANC */
    KYMERA_STATE_ADAPTIVE_ANC_STARTED,
    /*! Kymera is running standlone leakthrough Chain.*/
    KYMERA_STATE_STANDALONE_LEAKTHROUGH,
    /*! Kymera is performing a loopback. */
    KYMERA_STATE_MIC_LOOPBACK,
    /*! Kymera is playing wired audio, could be analog/USB */
    KYMERA_STATE_WIRED_AUDIO_PLAYING,
    /*! Kymera is streaming LE Audio locally. */
    KYMERA_STATE_LE_AUDIO_ACTIVE,
    /*! Kymera is streaming LE Voice locally. */
    KYMERA_STATE_LE_VOICE_ACTIVE,
    /*! Kymera is performing USB Audio. */
    KYMERA_STATE_USB_AUDIO_ACTIVE,
    KYMERA_STATE_USB_VOICE_ACTIVE,
    /*! Kymera is performing USB to SCO voice audio. */
    KYMERA_STATE_USB_SCO_VOICE_ACTIVE,
} appKymeraState;

#endif /* KYMERA_STATE_TYPES_H_ */

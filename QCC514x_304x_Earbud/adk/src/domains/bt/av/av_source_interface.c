/*!
\copyright  Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      AV Source callback interface implementation

    Implements the callback interface for the AV source instance type
*/

#include "av_callback_interface.h"
#include "a2dp.h"
#include "macros.h"
#include "av.h"
#include <logging.h>
#include <message.h>
#include <audio_sources.h>
#include "a2dp_profile_caps.h"
#ifdef INCLUDE_AV_SOURCE

#define APTX_AD_SAMPLING_FREQ_48000 (1 << 4)
#define APTX_AD_SAMPLING_FREQ_44100 (1 << 3)

#define APTX_AD_CHANNEL_MODE_TWS_PLUS (1 << 5)
#define APTX_AD_CHANNEL_MODE_STEREO (1 << 1)

#define APTX_AD_LL_TTP_MIN_IN_1MS  0        // Minimum latency in milliseconds for low-latency mode
#define APTX_AD_LL_TTP_MAX_IN_4MS  75       // Max latency for low-latency mode in 4ms units (i.e. 75*4ms)
#define APTX_AD_HQ_TTP_MIN_IN_1MS  0        // Minimum latency in milliseconds for HQ mode
#define APTX_AD_HQ_TTP_MAX_IN_4MS  75       // Max latency for HQ mode in 4ms units (i.e. 75*4ms)
#define APTX_AD_TWS_TTP_MIN_IN_1MS 100      // Minimum latency in milliseconds for TWS mode
#define APTX_AD_TWS_TTP_MAX_IN_4MS 75       // Max latency for TWS mode in 4ms units (i.e. 75*4ms)

#define APTX_AD_CAPABILITY_EXTENSION_VERSION_NUMBER           0x01
#define APTX_AD_SUPPORTED_FEATURES                            0x0000000F
#define APTX_AD_FIRST_SETUP_PREFERENCE                        0x02
#define APTX_AD_SECOND_SETUP_PREFERENCE                       0x03
#define APTX_AD_THIRD_SETUP_PREFERENCE                        0x03
#define APTX_AD_FOURTH_SETUP_PREFERENCE                       0x03
#define APTX_AD_NO_FURTHER_EXPANSION                          0x00
#define APTX_AD_CAPABILITY_EXTENSION_END                      0x00

const uint8 sbc_caps_src[] =
{
    AVDTP_SERVICE_MEDIA_TRANSPORT,
    0,
    AVDTP_SERVICE_MEDIA_CODEC,
    6,
    AVDTP_MEDIA_TYPE_AUDIO<<2,
    AVDTP_MEDIA_CODEC_SBC,

    SBC_SAMPLING_FREQ_48000    |
    SBC_CHANNEL_MODE_JOINT_STEREO   |
    SBC_CHANNEL_MODE_MONO,

    SBC_BLOCK_LENGTH_16         | SBC_SUBBANDS_8             | SBC_ALLOCATION_SNR         | SBC_ALLOCATION_LOUDNESS,

    SBC_BITPOOL_MIN,
    SBC_BITPOOL_HIGH_QUALITY
};

static const uint8 aptx_classic_src_caps[] =
{
    AVDTP_SERVICE_MEDIA_TRANSPORT,
    0,
    AVDTP_SERVICE_MEDIA_CODEC,
    9,
    AVDTP_MEDIA_TYPE_AUDIO << 2,
    AVDTP_MEDIA_CODEC_NONA2DP,

    (A2DP_APT_VENDOR_ID >> 24) & 0xFF,    /* A2DP_APT_VENDOR_ID is defined backwards (0x4f000000 for ID 0x4f), so write octets in reverse order */
    (A2DP_APT_VENDOR_ID >> 16) & 0xFF,
    (A2DP_APT_VENDOR_ID >>  8) & 0xFF,
    (A2DP_APT_VENDOR_ID >>  0) & 0xFF,

    (A2DP_CSR_APTX_CODEC_ID >> 8) & 0xFF, /* A2DP_CSR_APTX_CODEC_ID is defined backwards (0x0100 for ID 0x01), so write octets in reverse order */
    (A2DP_CSR_APTX_CODEC_ID >> 0) & 0xFF,

	APTX_SAMPLING_FREQ_44100 | APTX_SAMPLING_FREQ_48000 | APTX_CHANNEL_MODE_STEREO,

    AVDTP_SERVICE_CONTENT_PROTECTION,
    2,
    AVDTP_CP_TYPE_SCMS_LSB,
    AVDTP_CP_TYPE_SCMS_MSB,

    AVDTP_SERVICE_DELAY_REPORTING,
    0
};

const uint8 aptx_adaptive_src_caps[] =
{
    AVDTP_SERVICE_MEDIA_TRANSPORT,
    0,
    AVDTP_SERVICE_MEDIA_CODEC,
    42,
    AVDTP_MEDIA_TYPE_AUDIO << 2,
    AVDTP_MEDIA_CODEC_NONA2DP,

    (A2DP_QTI_VENDOR_ID >> 24) & 0xFF,
    (A2DP_QTI_VENDOR_ID >> 16) & 0xFF,
    (A2DP_QTI_VENDOR_ID >>  8) & 0xFF,
    (A2DP_QTI_VENDOR_ID >>  0) & 0xFF,

    (A2DP_QTI_APTX_AD_CODEC_ID >> 8) & 0xFF,
    (A2DP_QTI_APTX_AD_CODEC_ID >> 0) & 0xFF,

    APTX_AD_SAMPLING_FREQ_48000,
    APTX_AD_CHANNEL_MODE_STEREO,
    APTX_AD_LL_TTP_MIN_IN_1MS,
    APTX_AD_LL_TTP_MAX_IN_4MS,
    APTX_AD_HQ_TTP_MIN_IN_1MS,
    APTX_AD_HQ_TTP_MAX_IN_4MS,
    APTX_AD_TWS_TTP_MIN_IN_1MS,
    APTX_AD_TWS_TTP_MAX_IN_4MS,

    0x00,

    APTX_AD_CAPABILITY_EXTENSION_VERSION_NUMBER,
    SPLIT_IN_4_OCTETS(APTX_AD_SUPPORTED_FEATURES),
    APTX_AD_FIRST_SETUP_PREFERENCE,
    APTX_AD_SECOND_SETUP_PREFERENCE,
    APTX_AD_THIRD_SETUP_PREFERENCE,
    APTX_AD_FOURTH_SETUP_PREFERENCE,
    APTX_AD_NO_FURTHER_EXPANSION,
    APTX_AD_CAPABILITY_EXTENSION_END,

    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00,

    AVDTP_SERVICE_DELAY_REPORTING,
    0
};

const sep_config_type av_sbc_src_sep     = {AV_SEID_SBC_SRC,      ENCODE_RESOURCE_ID, sep_media_type_audio, a2dp_source, TRUE, 0, sizeof(sbc_caps_src),  sbc_caps_src};
const sep_config_type av_aptx_classic_src_sep     = {AV_SEID_APTX_CLASSIC_SRC,      ENCODE_RESOURCE_ID, sep_media_type_audio, a2dp_source, TRUE, 0, sizeof(aptx_classic_src_caps),  aptx_classic_src_caps};
const sep_config_type av_aptxad_src_sep     = {AV_SEID_APTX_ADAPTIVE_SRC,      ENCODE_RESOURCE_ID, sep_media_type_audio, a2dp_source, TRUE, 0, sizeof(aptx_adaptive_src_caps),  aptx_adaptive_src_caps};

const avrcp_init_params avrcpConfig =
{
    avrcp_target_and_controller,
    AVRCP_CATEGORY_2,
    AVRCP_CATEGORY_2 | AVRCP_CATEGORY_1,
    AVRCP_VERSION_1_6
};

static void avSourceInterface_InitialiseA2dp(Task client_task)
{
    /* Initialise A2DP role */
    uint16 role = A2DP_INIT_ROLE_SOURCE;

    sep_data_type seps[] = {
        /* Standard sinks */
        { .sep_config = &av_aptxad_src_sep,
          .in_use = 0,
        },
        { .sep_config = &av_aptx_classic_src_sep,
          .in_use = 0,
        },
        { .sep_config = &av_sbc_src_sep,
          .in_use = 0,
        }
    };

    DEBUG_LOG("appAvEnterInitialisingA2dp");
    /* Initialise the A2DP Library */
    A2dpInit(client_task, role, 0, ARRAY_DIM(seps), seps, 0);
}

static void avSourceInterface_Initialise(void)
{
    DEBUG_LOG_VERBOSE("avSourceInterface_Initialise");
}

static uint16 avSourceInterface_GetMediaChannelSeids(const uint8** seid_list_out)
{
    *seid_list_out = NULL;
    return 0;
}

static bool avSourceInterface_AvrcpPlay(bool pressed)
{
    /* USB does not report play state of media
     * therefore will toggle rather than mapping to
     * AudioSources_Play()
     */

    if (pressed)
    {
        AudioSources_PlayPause(audio_source_usb);
    }

    return TRUE;
}

static bool avSourceInterface_AvrcpPause(bool pressed)
{
    /* USB does not report play state of media
     * therefore will toggle rather than mapping to
     * AudioSources_Pause()
     */

    if (pressed)
    {
        AudioSources_PlayPause(audio_source_usb);
    }

    return TRUE;
}

static bool avSourceInterface_AvrcpForward(bool pressed)
{
    if (pressed)
    {
        AudioSources_Forward(audio_source_usb);
    }

    return TRUE;
}

static bool avSourceInterface_AvrcpBackward(bool pressed)
{
    if (pressed)
    {
        AudioSources_Back(audio_source_usb);
    }

    return TRUE;
}

static uint16 avSourceInterface_GetAvrcpEvents(void)
{
    uint16 events = (1 << avrcp_event_playback_status_changed) | (1 << avrcp_event_volume_changed);
    return events;
}

static const avrcp_init_params * avSourceInterface_GetAvrcpConfig(void)
{
    return &avrcpConfig;
}

const av_callback_interface_t av_plugin_interface = {
    .Initialise = avSourceInterface_Initialise,
    .InitialiseA2dp = avSourceInterface_InitialiseA2dp,
    .GetMediaChannelSeids = avSourceInterface_GetMediaChannelSeids,
    .OnAvrcpPlay = avSourceInterface_AvrcpPlay,
    .OnAvrcpPause = avSourceInterface_AvrcpPause,
    .OnAvrcpForward = avSourceInterface_AvrcpForward,
    .OnAvrcpBackward = avSourceInterface_AvrcpBackward,
    .GetAvrcpEvents = avSourceInterface_GetAvrcpEvents,
    .GetAvrcpConfig = avSourceInterface_GetAvrcpConfig
};

#endif

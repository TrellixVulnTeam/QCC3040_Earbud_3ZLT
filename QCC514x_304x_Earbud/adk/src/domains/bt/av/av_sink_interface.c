/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      AV sink callback interface implementation

    Implements the callback interface for the AV sink instance type used for earbuds and headset
*/

#include "av_callback_interface.h"
#include "a2dp.h"
#include "macros.h"
#include "av.h"
#include "av_config.h"
#include <logging.h>
#include <feature.h>
#include <ps_key_map.h>
#include <ps.h>
#include "a2dp_profile_caps.h"

#ifdef INCLUDE_APTX_ADAPTIVE
#include "a2dp_profile_caps_aptx_adaptive.h"
#endif
#ifndef INCLUDE_AV_SOURCE
#ifdef TEST_AV_CODEC_PSKEY

#define AV_CODEC_PS_BIT_SBC             (1<<0)
#define AV_CODEC_PS_BIT_AAC             (1<<1)
#define AV_CODEC_PS_BIT_APTX            (1<<2)
#define AV_CODEC_PS_BIT_APTX_ADAPTIVE   (1<<3)
#define AV_CODEC_PS_BIT_APTX_HD         (1<<4)

static uint16 av_codec_pskey = AV_CODEC_PS_BIT_SBC;

static void appAvCodecPskeyInit(void)
{
    PsRetrieve(PS_KEY_TEST_AV_CODEC, &av_codec_pskey, sizeof(av_codec_pskey));
    DEBUG_LOG_ALWAYS("appAvCodecPskeyInit 0x%x", av_codec_pskey);
}

#define AV_CODEC_PS_SBC_ENABLED()             ((av_codec_pskey & AV_CODEC_PS_BIT_SBC) == AV_CODEC_PS_BIT_SBC)
#define AV_CODEC_PS_AAC_ENABLED()             ((av_codec_pskey & AV_CODEC_PS_BIT_AAC) == AV_CODEC_PS_BIT_AAC)
#define AV_CODEC_PS_APTX_ENABLED()            ((av_codec_pskey & AV_CODEC_PS_BIT_APTX) == AV_CODEC_PS_BIT_APTX)
#define AV_CODEC_PS_APTX_ADAPTIVE_ENABLED()   ((av_codec_pskey & AV_CODEC_PS_BIT_APTX_ADAPTIVE) == AV_CODEC_PS_BIT_APTX_ADAPTIVE)
#define AV_CODEC_PS_APTX_HD_ENABLED()         ((av_codec_pskey & AV_CODEC_PS_BIT_APTX_HD) == AV_CODEC_PS_BIT_APTX_HD)

#else

#define AV_CODEC_PS_SBC_ENABLED()             (TRUE)
#define AV_CODEC_PS_AAC_ENABLED()             (TRUE)
#define AV_CODEC_PS_APTX_ENABLED()            (TRUE)
#define AV_CODEC_PS_APTX_ADAPTIVE_ENABLED()   (TRUE)
#define AV_CODEC_PS_APTX_HD_ENABLED()         (TRUE)

#endif

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

/*! Default SBC Capabilities
    Default capabilities that an application can pass to the A2DP library during initialisation.

    Support all features and full bitpool range. Note that we trust the source
    to choose a bitpool value suitable for the Bluetooth bandwidth.
*/
const uint8 sbc_caps_sink[] =
{
    AVDTP_SERVICE_MEDIA_TRANSPORT,
    0,
    AVDTP_SERVICE_MEDIA_CODEC,
    6,
    AVDTP_MEDIA_TYPE_AUDIO<<2,
    AVDTP_MEDIA_CODEC_SBC,

    SBC_SAMPLING_FREQ_44100     | SBC_SAMPLING_FREQ_48000    |
    SBC_CHANNEL_MODE_MONO       | SBC_CHANNEL_MODE_DUAL_CHAN | SBC_CHANNEL_MODE_STEREO    | SBC_CHANNEL_MODE_JOINT_STEREO,

    SBC_BLOCK_LENGTH_4          | SBC_BLOCK_LENGTH_8         | SBC_BLOCK_LENGTH_12        | SBC_BLOCK_LENGTH_16        |
    SBC_SUBBANDS_4              | SBC_SUBBANDS_8             | SBC_ALLOCATION_SNR         | SBC_ALLOCATION_LOUDNESS,

    SBC_BITPOOL_MIN,
    SBC_BITPOOL_HIGH_QUALITY,

    AVDTP_SERVICE_CONTENT_PROTECTION,
    2,
    AVDTP_CP_TYPE_SCMS_LSB,
    AVDTP_CP_TYPE_SCMS_MSB,

    AVDTP_SERVICE_DELAY_REPORTING,
    0
};
const uint8 sbc_caps_src[] =
{
    AVDTP_SERVICE_MEDIA_TRANSPORT,
    0,
    AVDTP_SERVICE_MEDIA_CODEC,
    6,
    AVDTP_MEDIA_TYPE_AUDIO<<2,
    AVDTP_MEDIA_CODEC_SBC,

    SBC_SAMPLING_FREQ_44100     | SBC_SAMPLING_FREQ_48000    |
    SBC_CHANNEL_MODE_MONO,

    SBC_BLOCK_LENGTH_16         | SBC_SUBBANDS_8             | SBC_ALLOCATION_SNR         | SBC_ALLOCATION_LOUDNESS,

    SBC_BITPOOL_MIN,
    SBC_BITPOOL_HIGH_QUALITY
};

/*! Default AAC/AAC+ Capabilities
    Default capabilities that an application can pass to the A2DP library during initialisation.

    Support all features.
*/
static const uint8 aac_caps_sink[] =
{
    AVDTP_SERVICE_MEDIA_TRANSPORT,
    0,
    AVDTP_SERVICE_MEDIA_CODEC,
    8,
    AVDTP_MEDIA_TYPE_AUDIO << 2,
    AVDTP_MEDIA_CODEC_MPEG2_4_AAC,

    AAC_MPEG2_AAC_LC | AAC_MPEG4_AAC_LC,
    AAC_SAMPLE_44100,
    AAC_SAMPLE_48000 | AAC_CHANNEL_1 | AAC_CHANNEL_2,
    AAC_VBR | AAC_BITRATE_3,
    AAC_BITRATE_4,
    AAC_BITRATE_5,

    AVDTP_SERVICE_CONTENT_PROTECTION,
    2,
    AVDTP_CP_TYPE_SCMS_LSB,
    AVDTP_CP_TYPE_SCMS_MSB,

    AVDTP_SERVICE_DELAY_REPORTING,
    0
};

/*! Default apt-X Capabilities
    Default capabilities that an application can pass to the A2DP library during initialisation.
*/
static const uint8 aptx_caps_sink[] =
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

    (A2DP_CSR_APTX_CODEC_ID >> 8) & 0xFF, /* A2DP_CSR_APTX_CODEC_ID is defined backwares (0x0100 for ID 0x01), so write octets in reverse order */
    (A2DP_CSR_APTX_CODEC_ID >> 0) & 0xFF,

    APTX_SAMPLING_FREQ_44100 | APTX_SAMPLING_FREQ_48000 | APTX_CHANNEL_MODE_STEREO,

    AVDTP_SERVICE_CONTENT_PROTECTION,
    2,
    AVDTP_CP_TYPE_SCMS_LSB,
    AVDTP_CP_TYPE_SCMS_MSB,

    AVDTP_SERVICE_DELAY_REPORTING,
    0
};

/*! Default apt-X Capabilities
    Default capabilities that an application can pass to the A2DP library during initialisation.
*/
static const uint8 aptxhd_caps_sink[] =
{
    AVDTP_SERVICE_MEDIA_TRANSPORT,
    0,
    AVDTP_SERVICE_MEDIA_CODEC,
    13,
    AVDTP_MEDIA_TYPE_AUDIO << 2,
    AVDTP_MEDIA_CODEC_NONA2DP,

    (A2DP_QTI_VENDOR_ID >> 24) & 0xFF,      /* A2DP_QTI_VENDOR_ID is defined backwards (0xd7000000 for ID 0xd7), so write octets in reverse order */
    (A2DP_QTI_VENDOR_ID >> 16) & 0xFF,
    (A2DP_QTI_VENDOR_ID >>  8) & 0xFF,
    (A2DP_QTI_VENDOR_ID >>  0) & 0xFF,

    (A2DP_QTI_APTXHD_CODEC_ID >> 8) & 0xFF, /* A2DP_QTI_APTXHD_CODEC_ID is defined backwards (0x2400 for ID 0x24), so write octets in reverse order */
    (A2DP_QTI_APTXHD_CODEC_ID >> 0) & 0xFF,

    APTX_SAMPLING_FREQ_44100 | APTX_SAMPLING_FREQ_48000 | APTX_CHANNEL_MODE_STEREO,

    APTX_HD_RESERVED_BYTE,
    APTX_HD_RESERVED_BYTE,
    APTX_HD_RESERVED_BYTE,
    APTX_HD_RESERVED_BYTE,

    AVDTP_SERVICE_CONTENT_PROTECTION,
    2,
    AVDTP_CP_TYPE_SCMS_LSB,
    AVDTP_CP_TYPE_SCMS_MSB,

    AVDTP_SERVICE_DELAY_REPORTING,
    0
};


/*!@{ \name Standard TWS sink endpoints
    \brief Predefined endpoints for audio Sink end point configurations, applicable to standard TWS and incoming TWS+ */
    /*! SBC */
const sep_config_type av_sbc_snk_sep     = {AV_SEID_SBC_SNK,      DECODE_RESOURCE_ID, sep_media_type_audio, a2dp_sink, TRUE, 0, sizeof(sbc_caps_sink),  sbc_caps_sink};
    /*! AAC */
const sep_config_type av_aac_snk_sep     = {AV_SEID_AAC_SNK,      DECODE_RESOURCE_ID, sep_media_type_audio, a2dp_sink, TRUE, 0, sizeof(aac_caps_sink),  aac_caps_sink};
    /*! APTX */
const sep_config_type av_aptx_snk_sep    = {AV_SEID_APTX_SNK,     DECODE_RESOURCE_ID, sep_media_type_audio, a2dp_sink, TRUE, 0, sizeof(aptx_caps_sink), aptx_caps_sink};
const sep_config_type av_aptxhd_snk_sep  = {AV_SEID_APTXHD_SNK,   DECODE_RESOURCE_ID, sep_media_type_audio, a2dp_sink, TRUE, 0, sizeof(aptxhd_caps_sink), aptxhd_caps_sink};
/*!@} */

/**/
const sep_config_type av_sbc_src_sep     = {AV_SEID_SBC_SRC,      ENCODE_RESOURCE_ID, sep_media_type_audio, a2dp_source, FALSE, 0, sizeof(sbc_caps_src),  sbc_caps_src};


const avrcp_init_params avrcpConfig =
{
    avrcp_target_and_controller,
    AVRCP_CATEGORY_1,
    AVRCP_CATEGORY_2 | AVRCP_CATEGORY_1,
    AVRCP_VERSION_1_6
};

static const uint8 sink_seids[] = {
                                   AV_SEID_APTX_ADAPTIVE_SNK,
                                   AV_SEID_APTXHD_SNK,
                                   AV_SEID_APTX_SNK,
                                   AV_SEID_AAC_SNK,
                                   AV_SEID_SBC_SNK
                                  };


static void avInterface_InitialiseA2dp(Task client_task)
{
    /* Initialise A2DP role */
    uint16 role = A2DP_INIT_ROLE_SINK;
#ifdef INCLUDE_APTX_ADAPTIVE
     bool enable_adaptive = FeatureVerifyLicense(APTX_ADAPTIVE_DECODE);
#ifdef INCLUDE_MIRRORING
     enable_adaptive |= FeatureVerifyLicense(APTX_ADAPTIVE_MONO_DECODE);
#endif
     /* Initialise the structure used by adaptive */
     A2dpProfileAptxAdInitServiceCapability();
#endif
     /* Initialise the Stream Endpoints... */
     sep_data_type seps[] = {
         /* Standard sinks */
         { .sep_config = &av_aptxhd_snk_sep,
           .in_use = (FeatureVerifyLicense(APTX_CLASSIC) && appConfigAptxHdEnabled() && AV_CODEC_PS_APTX_HD_ENABLED()) ? 0 : A2DP_SEP_UNAVAILABLE,
         },
         { .sep_config = &av_aptx_snk_sep,
           .in_use = (FeatureVerifyLicense(APTX_CLASSIC_MONO) && appConfigAptxEnabled() && AV_CODEC_PS_APTX_ENABLED()) ? 0 : A2DP_SEP_UNAVAILABLE,
         },
         { .sep_config = &av_aac_snk_sep,
           .in_use = (appConfigAacEnabled() && AV_CODEC_PS_AAC_ENABLED())? 0 : A2DP_SEP_UNAVAILABLE,
         },
         { .sep_config = &av_sbc_snk_sep,
           .in_use = (AV_CODEC_PS_SBC_ENABLED()) ? 0 : A2DP_SEP_UNAVAILABLE,
         },
#ifdef INCLUDE_APTX_ADAPTIVE
         { .sep_config = &av_aptx_adaptive_snk_sep,
           .in_use = (enable_adaptive && appConfigAptxAdaptiveEnabled() && AV_CODEC_PS_APTX_ADAPTIVE_ENABLED()) ? 0 : A2DP_SEP_UNAVAILABLE,
         },
#endif
     };
     DEBUG_LOG("avInterface_InitialiseA2dp");
     /* Initialise the A2DP Library */
     A2dpInit(client_task, role, 0, ARRAY_DIM(seps), seps, 0);
}

static void avInterface_Initialise(void)
{
#ifdef TEST_AV_CODEC_PSKEY
        appAvCodecPskeyInit();
#endif
}

static uint16 avInterface_GetMediaChannelSeids(const uint8** seid_list_out)
{
    *seid_list_out = sink_seids;
    return ARRAY_DIM(sink_seids);
}

static uint16 avInterface_GetAvrcpEvents(void)
{
    uint16 events = (1 << avrcp_event_playback_status_changed);
    return events;
}

static const avrcp_init_params * avInterface_GetAvrcpConfig(void)
{
    return &avrcpConfig;
}


const av_callback_interface_t av_plugin_interface = {
    .Initialise = avInterface_Initialise,
    .InitialiseA2dp = avInterface_InitialiseA2dp,
    .GetMediaChannelSeids = avInterface_GetMediaChannelSeids,
    .OnAvrcpPlay = NULL,
    .OnAvrcpPause = NULL,
    .OnAvrcpForward = NULL,
    .OnAvrcpBackward = NULL,
    .GetAvrcpEvents = avInterface_GetAvrcpEvents,
    .GetAvrcpConfig = avInterface_GetAvrcpConfig,
};

#endif

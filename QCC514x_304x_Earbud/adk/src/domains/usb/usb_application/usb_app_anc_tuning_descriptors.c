/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       usb_common.c
\brief      USB Audio class 1.0 descriptors for ANC tuning application.
*/

#ifdef ENABLE_ANC
#include "usb_audio.h"
#include "usb_audio_class_10_descriptors.h"

/* Unit/Terminal IDs */
#define SPKR_AUDIO_IT  0x01
#define SPKR_AUDIO_FU  0x02
#define SPKR_AUDIO_OT  0x03
#define MIC_AUDIO_IT  0x04
#define MIC_AUDIO_FU  0x05
#define MIC_AUDIO_OT  0x06

#define USB_AUDIO_SAMPLE_RATE      SAMPLE_RATE_48K

#define AUDIO_MIC_CHANNELS         2
#define AUDIO_SPKR_CHANNELS        2

#if AUDIO_SPKR_CHANNELS == 2
#define AUDIO_SPKR_CHANNEL_CONFIG  3
#else
#define AUDIO_SPKR_CHANNEL_CONFIG  1
#endif

#if AUDIO_MIC_CHANNELS == 2
#define AUDIO_MIC_CHANNEL_CONFIG   3
#else
#define AUDIO_MIC_CHANNEL_CONFIG   1
#endif

#define AUDIO_MIC_SUPPORTED_FREQUENCIES 1
#define AUDIO_SPKR_SUPPORTED_FREQUENCIES 1

#define USB_AUDIO_SAMPLE_SIZE                       USB_SAMPLE_SIZE_16_BIT /* 2 -> 16bit audio, 3 -> 24bit audio */

/* bmaControls should be changed with FU_DESC_CONTROL_SIZE */
#define FU_DESC_CONTROL_SIZE   0x01

static const uint8 control_intf_desc_audio_mic[] =
{
    /* Audio MIC IT */
    UAC_IT_TERM_DESC_SIZE,              /* bLength */
    UAC_CS_DESC_INTERFACE,              /* bDescriptorType = CS_INTERFACE */
    UAC_AC_DESC_INPUT_TERMINAL,         /* bDescriptorSubType = INPUT_TERMINAL */
    MIC_AUDIO_IT,                       /* bTerminalID */
    UAC_TRM_INPUT_MIC & 0xFF,           /* wTerminalType = Microphone */
    UAC_TRM_INPUT_MIC >> 8,
    0x00,                               /* bAssocTerminal = none */
    AUDIO_MIC_CHANNELS,                 /* bNrChannels = 2 */
    AUDIO_MIC_CHANNEL_CONFIG,           /* wChannelConfig   */
    AUDIO_MIC_CHANNEL_CONFIG >> 8,
    0x00,                               /* iChannelName = no string */
    0x00,                               /* iTerminal = same as USB product string */

    /* Audio MIC Features */
    UAC_FU_DESC_SIZE(AUDIO_MIC_CHANNELS,
                     FU_DESC_CONTROL_SIZE),   /*bLength*/
    UAC_CS_DESC_INTERFACE,              /* bDescriptorType = CS_INTERFACE */
    UAC_AC_DESC_FEATURE_UNIT,           /* bDescriptorSubType = FEATURE_UNIT*/
    MIC_AUDIO_FU,                       /* bUnitId*/
    MIC_AUDIO_IT,                       /* bSourceId - Speaker IT*/
    FU_DESC_CONTROL_SIZE,               /* bControlSize = 1 bytes per control*/
    UAC_FU_CONTROL_VOLUME,              /* bmaControls[0] (Master Channel - volume) */
    UAC_FU_CONTROL_UNDEFINED,           /* bmaControls[1] (Logical Channel 1 - nothing) */
    #if AUDIO_MIC_CHANNELS == 2
    UAC_FU_CONTROL_UNDEFINED,           /* bmaControls[2] (Logical Channel 2 - nothing) */
    #endif
    0x00,                               /* iFeature = same as USB product string*/

    /* Audio MIC OT */
    UAC_OT_TERM_DESC_SIZE,              /* bLength */
    UAC_CS_DESC_INTERFACE,              /* bDescriptorType = CS_INTERFACE */
    UAC_AC_DESC_OUTPUT_TERNINAL,        /* bDescriptorSubType = OUTPUT_TERMINAL */
    MIC_AUDIO_OT,                       /* bTerminalID */
    UAC_TRM_USB_STREAMING & 0xFF,       /* wTerminalType = Streaming Mic*/
    UAC_TRM_USB_STREAMING >> 8,
    0x00,                               /* bAssocTerminal = none */
    MIC_AUDIO_FU,                       /* bSourceID - Mic Features*/
    0x00                                /* iTerminal = same as USB product string */
};

static const uint8 control_intf_desc_audio_spkr[] =
{
    /* Speaker IT */
    UAC_IT_TERM_DESC_SIZE,              /* bLength */
    UAC_CS_DESC_INTERFACE,              /* bDescriptorType = CS_INTERFACE */
    UAC_AC_DESC_INPUT_TERMINAL,         /* bDescriptorSubType = INPUT_TERMINAL */
    SPKR_AUDIO_IT,                      /* bTerminalID */
    UAC_TRM_USB_STREAMING & 0xFF,       /* wTerminalType = USB streaming */
    UAC_TRM_USB_STREAMING >> 8,
    0x00,                               /* bAssocTerminal = none */
    AUDIO_SPKR_CHANNELS,                /* bNrChannels = 2 */
    AUDIO_SPKR_CHANNEL_CONFIG,          /* wChannelConfig   */
    AUDIO_SPKR_CHANNEL_CONFIG >> 8,
    0x00,                               /* iChannelName = no string */
    0x00,                               /* iTerminal = same as USB product string */

    /* Speaker Features */
    UAC_FU_DESC_SIZE(AUDIO_SPKR_CHANNELS,
                     FU_DESC_CONTROL_SIZE),   /*bLength*/
    UAC_CS_DESC_INTERFACE,              /* bDescriptorType = CS_INTERFACE */
    UAC_AC_DESC_FEATURE_UNIT,           /* bDescriptorSubType = FEATURE_UNIT*/
    SPKR_AUDIO_FU,                      /* bUnitId*/
    SPKR_AUDIO_IT,                      /* bSourceId - Speaker IT*/
    FU_DESC_CONTROL_SIZE,               /* bControlSize = 1 bytes per control*/
    UAC_FU_CONTROL_MUTE |
    UAC_FU_CONTROL_VOLUME,              /* bmaControls[0] (Master Channel - mute and volume) */
    UAC_FU_CONTROL_UNDEFINED,           /* bmaControls[1] (Logical Channel 1 - nothing) */
    #if AUDIO_SPKR_CHANNELS == 2
    UAC_FU_CONTROL_UNDEFINED,           /* bmaControls[2] (Logical Channel 2 - nothing) */
    #endif
    0x00,                               /* iFeature = same as USB product string*/

    /* Speaker OT */
    UAC_OT_TERM_DESC_SIZE,              /* bLength */
    UAC_CS_DESC_INTERFACE,              /* bDescriptorType = CS_INTERFACE */
    UAC_AC_DESC_OUTPUT_TERNINAL,        /* bDescriptorSubType = OUTPUT_TERMINAL */
    SPKR_AUDIO_OT,                      /* bTerminalID */
    UAC_TRM_OUTPUT_SPKR & 0xFF,         /* wTerminalType = Speaker*/
    UAC_TRM_OUTPUT_SPKR >> 8,
    0x00,                               /* bAssocTerminal = none */
    SPKR_AUDIO_FU,                      /* bSourceID - Speaker Features*/
    0x00                                /* iTerminal = same as USB product string */
};

/** USB streaming interface descriptors for mic */
static const uint8 streaming_intf_desc_audio_mic[] =
{
    /* Class Specific AS interface descriptor */
    UAC_AS_IF_DESC_SIZE,                /* bLength */
    UAC_CS_DESC_INTERFACE,              /* bDescriptorType = CS_INTERFACE */
    UAC_AS_DESC_GENERAL,                /* bDescriptorSubType = AS_GENERAL */
    MIC_AUDIO_OT,                       /* bTerminalLink = Speaker IT */
    0x00,                               /* bDelay */
    UAC_DATA_FORMAT_TYPE_I_PCM & 0xFF,  /* wFormatTag = PCM */
    UAC_DATA_FORMAT_TYPE_I_PCM >> 8,

    /* Type 1 format type descriptor */
    UAC_FORMAT_DESC_SIZE(AUDIO_MIC_SUPPORTED_FREQUENCIES),  /* bLength 8+((number of sampling frequencies)*3) */
    UAC_CS_DESC_INTERFACE,              /* bDescriptorType = CS_INTERFACE */
    UAC_AS_DESC_FORMAT_TYPE,            /* bDescriptorSubType = FORMAT_TYPE */
    UAC_AS_DESC_FORMAT_TYPE_I,          /* bFormatType = FORMAT_TYPE_I */
    AUDIO_MIC_CHANNELS,                /* bNumberOfChannels */
    USB_AUDIO_SAMPLE_SIZE,              /* bSubframeSize = 2 bytes */
    USB_AUDIO_SAMPLE_SIZE * 8,          /* bBitsResolution */
    AUDIO_MIC_SUPPORTED_FREQUENCIES,   /* bSampleFreqType = 6 discrete sampling frequencies */
    USB_AUDIO_SAMPLE_RATE & 0xFF,             /* tSampleFreq = 48000*/
    (USB_AUDIO_SAMPLE_RATE >> 8) & 0xFF,
    (USB_AUDIO_SAMPLE_RATE >> 16) & 0xFF,


    /* Class specific AS isochronous audio data endpoint descriptor */
    UAC_AS_DATA_EP_DESC_SIZE,           /* bLength */
    UAC_CS_DESC_ENDPOINT,               /* bDescriptorType = CS_ENDPOINT */
    UAC_AS_EP_DESC_GENERAL,             /* bDescriptorSubType = AS_GENERAL */
    UAC_EP_CONTROL_UNDEFINED,           /* bmAttributes = None */
    0x02,                               /* bLockDelayUnits = Decoded PCM samples */
    0x00, 0x00                          /* wLockDelay */
};

/** Default USB streaming interface descriptors for alt speaker */
static const uint8 streaming_intf_desc_audio_spkr[] =
{
    /* Class Specific AS interface descriptor */
    UAC_AS_IF_DESC_SIZE,                /* bLength */
    UAC_CS_DESC_INTERFACE,              /* bDescriptorType = CS_INTERFACE */
    UAC_AS_DESC_GENERAL,                /* bDescriptorSubType = AS_GENERAL */
    SPKR_AUDIO_IT,                           /* bTerminalLink = Speaker IT */
    0x00,                               /* bDelay */
    UAC_DATA_FORMAT_TYPE_I_PCM & 0xFF,  /* wFormatTag = PCM */
    UAC_DATA_FORMAT_TYPE_I_PCM >> 8,

    /* Type 1 format type descriptor */
    UAC_FORMAT_DESC_SIZE(AUDIO_SPKR_SUPPORTED_FREQUENCIES),  /* bLength 8+((number of sampling frequencies)*3) */
    UAC_CS_DESC_INTERFACE,              /* bDescriptorType = CS_INTERFACE */
    UAC_AS_DESC_FORMAT_TYPE,            /* bDescriptorSubType = FORMAT_TYPE */
    UAC_AS_DESC_FORMAT_TYPE_I,          /* bFormatType = FORMAT_TYPE_I */
    AUDIO_SPKR_CHANNELS,                /* bNumberOfChannels */
    USB_AUDIO_SAMPLE_SIZE,              /* bSubframeSize = 2 bytes */
    USB_AUDIO_SAMPLE_SIZE * 8,          /* bBitsResolution */
    AUDIO_SPKR_SUPPORTED_FREQUENCIES,   /* bSampleFreqType = 6 discrete sampling frequencies */
    USB_AUDIO_SAMPLE_RATE & 0xFF,             /* tSampleFreq = 48000*/
    (USB_AUDIO_SAMPLE_RATE >> 8) & 0xFF,
    (USB_AUDIO_SAMPLE_RATE >> 16) & 0xFF,

    /* Class specific AS isochronous audio data endpoint descriptor */
    UAC_AS_DATA_EP_DESC_SIZE,           /* bLength */
    UAC_CS_DESC_ENDPOINT,               /* bDescriptorType = CS_ENDPOINT */
    UAC_AS_EP_DESC_GENERAL,             /* bDescriptorSubType = AS_GENERAL */
    UAC_EP_CONTROL_SAMPLING_FREQ |
    UAC_EP_CONTROL_MAX_PACKETS_ONLY,    /* bmAttributes = MaxPacketsOnly and SamplingFrequency control */
    0x02,                               /* bLockDelayUnits = Decoded PCM samples */
    0x00, 0x00                          /* wLockDelay */
};

/* Audio Speaker interface descriptors */
static const uac_control_config_t anc_tuning_control_mic_desc = {
    control_intf_desc_audio_mic,
    sizeof(control_intf_desc_audio_mic)
};

static const uac_streaming_config_t anc_tuning_streaming_mic_desc = {
    streaming_intf_desc_audio_mic,
    sizeof(streaming_intf_desc_audio_mic)
};

static const uac_endpoint_config_t anc_tuning_mic_endpoint = {
    .is_to_host = 1,
    .wMaxPacketSize = 0,
    .bInterval = 1
};

/* Audio Speaker interface descriptors */
static const uac_control_config_t anc_tuning_control_spkr_desc = {
    control_intf_desc_audio_spkr,
    sizeof(control_intf_desc_audio_spkr)
};

static const uac_streaming_config_t anc_tuning_streaming_spkr_desc = {
    streaming_intf_desc_audio_spkr,
    sizeof(streaming_intf_desc_audio_spkr)
};

static const uac_endpoint_config_t anc_tuning_spkr_endpoint = {
    .is_to_host = 0,
    .wMaxPacketSize = 0,
    .bInterval = 1
};

static const usb_audio_interface_config_t anc_tuning_intf_list[] =
{
    {
        .type = USB_AUDIO_DEVICE_TYPE_AUDIO_SPEAKER,
        .control_desc =   &anc_tuning_control_spkr_desc,
        .streaming_desc = &anc_tuning_streaming_spkr_desc,
        .endpoint =       &anc_tuning_spkr_endpoint
    },
    {
        .type = USB_AUDIO_DEVICE_TYPE_AUDIO_MIC,
        .control_desc =   &anc_tuning_control_mic_desc,
        .streaming_desc = &anc_tuning_streaming_mic_desc,
        .endpoint =       &anc_tuning_mic_endpoint
    }
};

const usb_audio_interface_config_list_t anc_tuning_interfaces =
{
    .intf = anc_tuning_intf_list,
    .num_interfaces = ARRAY_DIM(anc_tuning_intf_list)
};

#endif /*ENABLE_ANC */

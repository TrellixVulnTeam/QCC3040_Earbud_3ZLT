/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for creating descriptors for USB Audio class 1.0 
*/

#ifndef USB_AUDIO_CLASS_10_DESCRIPTORS_H_
#define USB_AUDIO_CLASS_10_DESCRIPTORS_H_

#include "usb_audio_defines.h"

/*! Audio Device Class Specification Release Number in Binary-Coded Decimal */
/*! Audio Device Class v1.00 */
#define UAC_BCD_ADC_1_0     0x0100

/*! Audio Interface Class Code */
#define UAC_IF_CLASS_AUDIO  0x01

/*! Audio Interface Subclass Codes */
typedef enum
{
    UAC_IF_SUBCLASS_AUDIOCONTROL    = 0x01,
    UAC_IF_SUBCLASS_AUDIOSTREAMING  = 0x02,
} usb_audio_if_subclass_t;

/*! Audio Interface Protocol Codes  */
#define UAC_PROTOCOL_UNDEFINED       0x00

/*! Audio Class-Specific Descriptor Types  */
typedef enum
{
    UAC_CS_DESC_UNDEFINED           = 0x20,
    UAC_CS_DESC_DEVICE              = 0x21,
    UAC_CS_DESC_CONFIG              = 0x22,
    UAC_CS_DESC_STRING              = 0x23,
    UAC_CS_DESC_INTERFACE           = 0x24,
    UAC_CS_DESC_ENDPOINT            = 0x25
} usb_audio_cs_descriptor_type_t;

/*! Audio Class-Specific AC Interface Descriptor Subtypes */
typedef enum
{
    UAC_AC_DESC_HEADER          = 0x01,
    UAC_AC_DESC_INPUT_TERMINAL  = 0x02,
    UAC_AC_DESC_OUTPUT_TERNINAL = 0x03,
    UAC_AC_DESC_FEATURE_UNIT    = 0x06,
} usb_audio_ac_if_descriptor_subtype_t;

/*! Audio Class-Specific AS Interface Descriptor Subtypes */
typedef enum
{
    UAC_AS_DESC_UNDEFINED       = 0x00,
    UAC_AS_DESC_GENERAL         = 0x01,
    UAC_AS_DESC_FORMAT_TYPE     = 0x02,
    UAC_AS_DESC_FORMAT_SPECIFIC = 0x03
} usb_audio_as_if_descriptor_subtype_t;

/*! Audio Class-Specific Endpoint Descriptor Subtypes*/
typedef enum
{
    UAC_AS_EP_DESC_UNDEFINED    = 0x00,
    UAC_AS_EP_DESC_GENERAL      = 0x01,
} usb_audio_as_ep_descriptor_subtype_t;

/*! Audio Class-Specific AS Interface Descriptor Subtypes */
#define UAC_AS_DESC_FORMAT_TYPE_I   0x0001

/*! Audio Data Format Type I Codes */
#define UAC_DATA_FORMAT_TYPE_I_PCM  0x0001

/*! Feature Unit Control Selectors */
typedef enum
{
    UAC_FU_CONTROL_UNDEFINED         = 0x00,
    UAC_FU_CONTROL_MUTE              = 0x01,
    UAC_FU_CONTROL_VOLUME            = 0x02,
} usb_audio_feature_unit_control_selectors_t;

/*! Endpoint Control Selectors */
typedef enum
{
    UAC_EP_CONTROL_UNDEFINED        = 0x00,
    UAC_EP_CONTROL_SAMPLING_FREQ    = 0x01,
    UAC_EP_CONTROL_PITCH            = 0x02,
    UAC_EP_CONTROL_MAX_PACKETS_ONLY = 0x80
} usb_audio_endpoint_control_selectors_t;

/*! Input terminal channel config.
    Indicates spatial locations which are present in the cluster */
typedef enum
{
    /*! CHANNEL is MON_PREDEFINED*/
    UAC_CHANNEL_MASTER      = 0x0000,
    UAC_CHANNEL_LEFT        = 0x0001,
    UAC_CHANNEL_RIGHT       = 0x0002
}usb_audio_ch_config_t ;


/*! USB Device Class Definition for Terminal Types.
    The following is a list of possible Terminal Types. 
    This list is non-exhaustive and will only be expanded in the future */
typedef enum
{
    /*! Terminals that handle signals carried over the USB.
        These Terminal Types are valid for both Input and Output Terminals. */
    /*! USB Terminal, undefined Type. */
    UAC_TRM_USB_UNDEFINED           = 0x0100,
    /*! Dealing with a signal carried over an endpoint in an AudioStreaming interface */
    UAC_TRM_USB_STREAMING           = 0x0101,
    /*! Dealing with a signal carried over a vendor-specific interface */
    UAC_TRM_USB_VENDOR_SPECIFIC     = 0x01FF,

    /*! Terminals that are designed to record sounds.
        These Terminal Types are valid only for Input Terminals  */
    /*! Input Terminal, undefined Type */
    UAC_TRM_INPUT_UNDEFINED         = 0x0200,
    /*! Generic Microphone */
    UAC_TRM_INPUT_MIC               = 0x0201,
    /*! Desktop Microphone */
    UAC_TRM_INPUT_DSKT_MIC          = 0x0202,
    /*! Personal Microphone */
    UAC_TRM_INPUT_PERS_MIC          = 0x0203,
    /*! Omni-directional Microphone */
    UAC_TRM_INPUT_OMNI_MIC          = 0x0204,
    /*! Microphone Array */
    UAC_TRM_INPUT_MIC_ARRAY         = 0x0205,
    /*! Processing Microphone Array */
    UAC_TRM_INPUT_PROC_MIC_ARRAY    = 0x0206,

    /*! Terminals that produce audible signals.
        These Terminal Types are only valid for Output Terminals */
    /*! Output Terminal, undefined Type */
    UAC_TRM_OUTPUT_UNDEFINED        = 0x0300,
    /*! Generic Speaker */
    UAC_TRM_OUTPUT_SPKR             = 0x0301,
    /*! Headphones */
    UAC_TRM_OUTPUT_HEADPHONES       = 0x0302,
    /*! Head Mounted Display Audio */
    UAC_TRM_OUTPUT_HMDA             = 0x0303,
    /*! Desktop Speaker */
    UAC_TRM_OUTPUT_DSKT_SPKR        = 0x0304,
    /*! Room Speaker */
    UAC_TRM_OUTPUT_ROOM_SPKR        = 0x0305,
    /*! Communication  Speaker */
    UAC_TRM_OUTPUT_COMM_SPKR        = 0x0306,
    /*! Low frequency effects Speaker */
    UAC_TRM_OUTPUT_LFE_SPKR         = 0x0307,

    /*! Terminal Types describe an Input and an Output Terminal for voice communication that are closely related.
        They should be used together for bi-directional voice communication.
        They may be used separately for input only or output only */
    /*! Bi-directional Terminal, undefined Type. */
    UAC_TRM_BIDI_UNDEFINED          = 0x0400,
    /*! Handset */
    UAC_TRM_BIDI_HANDSET            = 0x0401,
    /*! Headset */
    UAC_TRM_BIDI_HEADSET            = 0x0402,
    /*! Speakerphone, no echo reduction */
    UAC_TRM_BIDI_SPKPH_NO_ECHO_RED  = 0x0403,
    /*! Echo-suppressing speakerphone */
    UAC_TRM_BIDI_SPKPH_ECHO_SUPP    = 0x0404,
    /*! Echo-canceling speakerphone */
    UAC_TRM_BIDI_SPKPH_ECHO_CANCL   = 0x0405,
} usb_audio_terminal_type_t;


/*! To get Class-Specific AC Interface Header Descriptor length from interface count */
#define UAC_AC_IF_HEADER_DESC_SIZE(if_count)     (8+(if_count))

/*! To get Input Terminal Descriptor length */
#define UAC_IT_TERM_DESC_SIZE                    (0x0C)

/*! To get Output Terminal Descriptor length*/
#define UAC_OT_TERM_DESC_SIZE                    (0x09)

/*! To get Feature Unit Descriptor length from channel count and control size */
#define UAC_FU_DESC_SIZE(ch_count, control_size) (0x07+(ch_count+1)*control_size)

/*! To get Class-Specific AS Interface Descriptor length */
#define UAC_AS_IF_DESC_SIZE                      (0x07)

/*! To get Type I Format Type Descriptor length from number of sampling frequencies supported*/
#define UAC_FORMAT_DESC_SIZE(n_s)                (0x08 + (n_s)*3)

/*! To get Class-Specific AS Isochronous Audio Data Endpoint Descriptor length */
#define UAC_AS_DATA_EP_DESC_SIZE                 (0x07)


#endif /* USB_AUDIO_CLASS_10_DESCRIPTORS_H_ */


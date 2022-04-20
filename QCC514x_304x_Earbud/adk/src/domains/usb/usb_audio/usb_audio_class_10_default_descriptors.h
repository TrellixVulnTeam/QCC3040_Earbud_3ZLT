/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for using default descriptors of USB Audio class 1.0 
*/

#ifndef USB_AUDIO_CLASS_10_DEFAULT_DESCRIPTORS_H_
#define USB_AUDIO_CLASS_10_DEFAULT_DESCRIPTORS_H_

#include "usb_audio.h"
#include "usb_audio_class_10_descriptors.h"

/* Unit/Terminal IDs Used by default descriptors*/
#define UAC1D_SPKR_VOICE_IT  0x01
#define UAC1D_SPKR_VOICE_FU  0x02
#define UAC1D_SPKR_VOICE_OT  0x03
#define UAC1D_MIC_VOICE_IT   0x04
#define UAC1D_MIC_VOICE_FU   0x05
#define UAC1D_MIC_VOICE_OT   0x06
#define UAC1D_SPKR_AUDIO_IT  0x07
#define UAC1D_SPKR_AUDIO_FU  0x08
#define UAC1D_SPKR_AUDIO_OT  0x09


#define UAC1D_VOICE_MIC_CHANNELS        USB_AUDIO_CHANNELS_MONO
#define UAC1D_VOICE_SPKR_CHANNELS       USB_AUDIO_CHANNELS_MONO
#define UAC1D_AUDIO_SPKR_CHANNELS       USB_AUDIO_CHANNELS_STEREO

#if UAC1D_VOICE_SPKR_CHANNELS == USB_AUDIO_CHANNELS_STEREO
#define UAC1D_VOICE_SPKR_CHANNEL_CONFIG  0x03
#elif UAC1D_VOICE_SPKR_CHANNELS == USB_AUDIO_CHANNELS_MONO
#define UAC1D_VOICE_SPKR_CHANNEL_CONFIG  0x01
#else
#error NOT_SUPPORTED
#endif

#if UAC1D_VOICE_MIC_CHANNELS == USB_AUDIO_CHANNELS_STEREO
#define UAC1D_VOICE_MIC_CHANNEL_CONFIG   0x03
#elif UAC1D_VOICE_MIC_CHANNELS == USB_AUDIO_CHANNELS_MONO
#define UAC1D_VOICE_MIC_CHANNEL_CONFIG   0x01
#else
#error NOT_SUPPORTED
#endif

#if UAC1D_AUDIO_SPKR_CHANNELS == USB_AUDIO_CHANNELS_STEREO
#define UAC1D_AUDIO_SPKR_CHANNEL_CONFIG  0x03
#elif UAC1D_AUDIO_SPKR_CHANNELS == USB_AUDIO_CHANNELS_MONO
#define UAC1D_AUDIO_SPKR_CHANNEL_CONFIG  0x01
#else
#error NOT_SUPPORTED
#endif

/* bmaControls should be changed with FU_DESC_CONTROL_SIZE */
#define UAC1D_FU_DESC_CONTROL_SIZE   0x01

#define UAC1D_VOICE_SPKR_SUPPORTED_FREQUENCIES   2
#define UAC1D_VOICE_MIC_SUPPORTED_FREQUENCIES    2
#define UAC1D_AUDIO_SPKR_SUPPORTED_FREQUENCIES   6


#define UAC1D_USB_AUDIO_SAMPLE_SIZE                       USB_SAMPLE_SIZE_16_BIT

/* Voice Mic interface descriptors */
extern const uac_control_config_t         uac1_voice_control_mic_desc;
extern const uac_streaming_config_t       uac1_voice_streaming_mic_desc;
extern const uac_endpoint_config_t        uac1_voice_mic_endpoint;

/* Voice Speaker interface descriptors */
extern const uac_control_config_t         uac1_voice_control_spkr_desc;
extern const uac_streaming_config_t       uac1_voice_streaming_spkr_desc;
extern const uac_endpoint_config_t        uac1_voice_spkr_endpoint;

extern const usb_audio_interface_config_list_t uac1_voice_interfaces;

/* Audio Speaker interface descriptors */
extern const uac_control_config_t         uac1_music_control_spkr_desc;
extern const uac_streaming_config_t       uac1_music_streaming_spkr_desc;
extern const uac_endpoint_config_t        uac1_music_spkr_endpoint;

extern const usb_audio_interface_config_list_t uac1_music_interfaces;

/* Audio Speaker Voice Mic interface descriptors */
extern const usb_audio_interface_config_list_t uac1_music_spkr_voice_mic_interfaces;

#endif /* USB_AUDIO_CLASS_10_DEFAULT_DESCRIPTORS_H_ */


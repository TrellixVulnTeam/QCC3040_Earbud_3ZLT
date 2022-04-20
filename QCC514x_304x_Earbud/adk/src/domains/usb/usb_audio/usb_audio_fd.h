/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Private header file for USB Audio Function driver.
*/

#ifndef USB_AUDIO_FD_H
#define USB_AUDIO_FD_H

#include "logging.h"

#include "volume_system.h"

#include <logging.h>
#include <panic.h>

#include <stdlib.h>
#include <stream.h>
#include <sink.h>
#include <source.h>
#include <string.h>
#include <usb.h>

#include "audio_sources_volume_interface.h"
#include "audio_sources_media_control_interface.h"

#include "usb_audio_class.h"

/* Device will enumerate both headset (speaker & mic) and headphone (speaker) interfaces, if enabled
 * and let the host select/switch between these. It is up to the host when to use which interface,
 * If host activates both interfaces, audio router may give higher priority to USB voice
 * and data from USB audio interface may discarded. It is possible for the host to activate
 * USB audio speaker and USB voice mic. Some Android hosts may not give option to choose usb speaker interface.
 * As a work around for this issue, if headphone speaker and headset mic are active and hadset speaker is not active,
 * then headphone speaker is supported in USB voice chain but with audio quality of headset mic.
 */
#define USB_SUPPORT_HEADPHONE_SPKR_IN_VOICE_CHAIN

#define USB_AUDIO_SPEAKER_CHANNELS   USB_AUDIO_CHANNELS_STEREO
#define USB_VOICE_SPEAKER_CHANNELS   USB_AUDIO_CHANNELS_MONO
#define USB_VOICE_MIC_CHANNELS       USB_AUDIO_CHANNELS_MONO

typedef struct
{
    /*! TRUE when speaker is enabled */
    bool                        spkr_enabled:1;
    /*! TRUE when mic is enabled */
    bool                        mic_enabled:1;
    /*! AudioStreaming interface is selected for streaming by the host with SET_INTERFACE(1) */
    bool                        spkr_active:1;
    /*! AudioStreaming interface is selected for streaming by the host with SET_INTERFACE(1) */
    bool                        mic_active:1;
    /*! TRUE from sending TELEPHONY_AUDIO_CONNECTED and until sending TELEPHONY_AUDIO_DISCONNECTED */
    bool                        source_connected:1;
    /*! TRUE from GetConnectParameters() and until GetDisconnectParameters() */
    bool                        chain_required:1;
    /*! TRUE from GetConnectParameters() and until KymeraStoppedHandler() */
    bool                        chain_active:1;
    /*! TRUE when alternate speaker is connected in usb voice chain */
    bool                        alt_spkr_connected:1;
    uint8                       mic_volume_steps;
    /*! Volume level in steps. */
    uint8                       spkr_volume_steps;
    voice_source_t              audio_source;
    Source                      spkr_src;
    Sink                        mic_sink;
    /*! Sample rate previously reported to Kymera in GetConnectParameters(). */
    uint32                      spkr_sample_rate;
    uint32                      mic_sample_rate;
} usb_audio_headset_info_t;


typedef struct
{
    /*! TRUE when speaker is enabled */
    bool                        spkr_enabled:1;
    /*! TRUE when mic is enabled */
    bool                        mic_enabled:1;
    /*! AudioStreaming interface is selected for streaming by the host with SET_INTERFACE(1) */
    bool                        spkr_active:1;
    /*! AudioStreaming interface is selected for streaming by the host with SET_INTERFACE(1) */
    bool                        mic_active:1;
    /*! TRUE from sending USB_AUDIO_CONNECTED_IND and until sending USB_AUDIO_DISCONNECTED_IND */
    bool                        source_connected:1;
    /*! TRUE from GetConnectParameters() and until GetDisconnectParameters() */
    bool                        chain_required:1;
    /*! TRUE from GetConnectParameters() and until KymeraStoppedHandler() */
    bool                        chain_active:1;
    uint8                       spkr_volume_steps;
    audio_source_t              audio_source;
    Source                      spkr_src;
    Sink                        mic_sink;
    uint32                      spkr_sample_rate;
    audio_source_provider_context_t audio_ctx;
} usb_audio_headphone_info_t;

typedef struct usb_audio_info_t
{

    uint8                           device_index;
    uint8                           num_interfaces;
    uint8                           is_pending_delete;
    uac_ctx_t                       *class_ctx;
    usb_audio_streaming_info_t      *streaming_info;
    usb_audio_headset_info_t        *headset;
    usb_audio_headphone_info_t      *headphone;
    const usb_fn_tbl_uac_if         *usb_fn_uac;
    const usb_audio_config_params_t *config;
    struct usb_audio_info_t         *next;
} usb_audio_info_t;

/*! \brief Get USB Audio Info for headphone device

    \return USB Audio Info.
 */
usb_audio_info_t *UsbAudioFd_GetHeadphoneInfo(audio_source_t source);

/*! \brief Get USB Audio Info by stream source

    \return USB Audio Info.
 */
usb_audio_info_t *UsbAudio_FindInfoBySource(Source source);

/*! \brief Scan interfaces and find streaming info of requested type
 *
 * \return USB Audio Streaming Info
 */
usb_audio_streaming_info_t *UsbAudio_GetStreamingInfo(usb_audio_info_t *usb_audio,
                                                      usb_audio_device_type_t type);

/*! \brief Get USB Audio Info for headset device

    \return USB Audio Info.
 */

usb_audio_info_t *UsbAudioFd_GetHeadsetInfo(audio_source_t source);

/*! \brief Gets the USB Audio volume interface.

    \return The audio source volume interface for an USB Audio
 */
const audio_source_volume_interface_t * UsbAudioFd_GetAudioSourceVolumeInterface(void);

/*! \brief Get USB Audio source interface for registration.

    \return USB Audio source interface.
 */
const audio_source_audio_interface_t * UsbAudioFd_GetSourceAudioInterface(void);

/*! \brief Get USB Audio media control interface for registration

    \return USB Audio media control interface.
 */
const media_control_interface_t * UsbAudioFd_GetMediaControlAudioInterface(void);

/*! \brief Get USB Voice source interface for registration.

    \return USB Voice source interface.
 */
const voice_source_audio_interface_t * UsbAudioFd_GetSourceVoiceInterface(void);

/*! \brief Gets the USB Voice volume interface.

    \return The voice source volume interface for an USB Voice
 */
const voice_source_volume_interface_t * UsbAudioFd_GetVoiceSourceVolumeInterface(void);

/*! \brief Release USB audio data structure and disconnect clients
 *
 *  \param usb_audio data structure to free
 *  \return USB_RESULT_OK if immediately released,
 *  USB_RESULT_BUSY if release is postponed waiting for client disconnect
 *  or error code otherwise.
 */
usb_result_t UsbAudio_TryFreeData(usb_audio_info_t *usb_audio);

#endif // USB_AUDIO_FD_H

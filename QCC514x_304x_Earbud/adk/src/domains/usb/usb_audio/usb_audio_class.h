/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Private interface file between USB Audio function driver and class driver.
*/

#ifndef USB_AUDIO_CLASS_H
#define USB_AUDIO_CLASS_H

#include "usb_audio.h"

/* USB Audio message comes from class driver */
typedef enum
{
    /* AUDIO_LEVELS message indicates value specified in usb_audio_class_levels_t*/
    USB_AUDIO_CLASS_MSG_LEVELS,
    USB_AUDIO_CLASS_MSG_SAMPLE_RATE,
    /* message limit */
    USB_AUDIO_MSG_TOP
} uac_message_t;

/*! Structure to hold voulme status. */
typedef struct
{
    /*! Master channel volume gain in dB */
    int8  volume_db;
    /*! Mute status of master channel */
    uint8 mute_status;
} uac_volume_status_t;

/*! Structure to hold information regarding streaming interface. */
typedef struct uac_streaming_info_t
{
    uint32                current_sampling_rate;
    uac_volume_status_t   volume_status;
    Source                source;
    UsbInterface          interface;
    uint8                 ep_address;
    uint8                 feature_unit_id;
    uint8                 channels;
    uint8                 frame_size;
} usb_audio_streaming_info_t;


/*! Opaque USB audio class context data */
typedef void * uac_ctx_t;

/*!
    USB audio event callback
    
    \param device_index USB device index
    \param id message ID
    \param message message payload, is owned by the caller and is not valid after the handler returns
*/
typedef  void (*uac_event_handler_t)(uac_ctx_t class_ctx,
                                     uint8 interface_index,
                                     uac_message_t uac_message);

/*!
    Below interfaces are implemented in usb_audio_class driver and will
    provide its instance using UsbAudioClassXX_GetFnTbl() api where XX is
    version.
*/
typedef struct
{
    uac_ctx_t (*Create)(usb_device_index_t device_index,
                        const usb_audio_config_params_t *config,
                        usb_audio_streaming_info_t **streaming_info,
                        uac_event_handler_t evt_handler);
    bool (*Reset)(uac_ctx_t class_ctx);
    bool (*Delete)(uac_ctx_t class_ctx);
} usb_fn_tbl_uac_if;


#endif /* USB_AUDIO_CLASS_H */

/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      USB Voice application - enumerates HID consumer transport,
            HID datalink and USB Audio (for voice calls with only wideband support) classes.
*/

#ifdef INCLUDE_USB_NB_WB_TEST

#include "usb_app_voice_wb.h"

/* USB trap API */
#include <usb_device.h>
#include <usb_hub.h>

/* device classes */
#include <usb_hid_consumer_transport_control.h>
#include <usb_hid_datalink.h>
#include <usb_audio.h>
#include <usb_audio_class_10_descriptors.h>
#include <usb_audio_class_10_default_descriptors.h>

#include "logging.h"

#include <panic.h>
#include <stdlib.h>

#define WB_VOICE_SPKR_SUPPORTED_FREQUENCIES   1
#define WB_VOICE_MIC_SUPPORTED_FREQUENCIES    1


/** Default USB streaming interface descriptors for speaker */
static const uint8 streaming_intf_desc_voice_wb_spkr[] =
{
    /* Class Specific AS interface descriptor */
    UAC_AS_IF_DESC_SIZE,                /* bLength */
    UAC_CS_DESC_INTERFACE,              /* bDescriptorType = CS_INTERFACE */
    UAC_AS_DESC_GENERAL,                /* bDescriptorSubType = AS_GENERAL */
    UAC1D_SPKR_VOICE_IT,                /* bTerminalLink = Speaker IT */
    0x00,                               /* bDelay */
    UAC_DATA_FORMAT_TYPE_I_PCM & 0xFF,  /* wFormatTag = PCM */
    UAC_DATA_FORMAT_TYPE_I_PCM >> 8,

    /* Type 1 format type descriptor */
    UAC_FORMAT_DESC_SIZE(WB_VOICE_SPKR_SUPPORTED_FREQUENCIES),  /* bLength 8+((number of sampling frequencies)*3) */
    UAC_CS_DESC_INTERFACE,              /* bDescriptorType = CS_INTERFACE */
    UAC_AS_DESC_FORMAT_TYPE,            /* bDescriptorSubType = FORMAT_TYPE */
    UAC_AS_DESC_FORMAT_TYPE_I,          /* bFormatType = FORMAT_TYPE_I */
    UAC1D_VOICE_SPKR_CHANNELS,          /* bNumberOfChannels */
    UAC1D_USB_AUDIO_SAMPLE_SIZE,        /* bSubframeSize = 2 bytes */
    UAC1D_USB_AUDIO_SAMPLE_SIZE * 8,    /* bBitsResolution */
    WB_VOICE_SPKR_SUPPORTED_FREQUENCIES,/* bSampleFreqType = 1 discrete sampling frequencies */
    SAMPLE_RATE_16K & 0xFF,             /* tSampleFreq = 16000 */
    (SAMPLE_RATE_16K >> 8 ) & 0xFF,
    (SAMPLE_RATE_16K >> 16) & 0xFF,


    /* Class specific AS isochronous audio data endpoint descriptor */
    UAC_AS_DATA_EP_DESC_SIZE,           /* bLength */
    UAC_CS_DESC_ENDPOINT,               /* bDescriptorType = CS_ENDPOINT */
    UAC_AS_EP_DESC_GENERAL,             /* bDescriptorSubType = AS_GENERAL */
    UAC_EP_CONTROL_SAMPLING_FREQ,       /* bmAttributes = SamplingFrequency control */
    0x02,                               /* bLockDelayUnits = Decoded PCM samples */
    0x00, 0x00                          /* wLockDelay */
};

/** Default USB streaming interface descriptors for mic */
static const uint8 streaming_intf_desc_voice_wb_mic[] =
{
    /* Class Specific AS interface descriptor */
    UAC_AS_IF_DESC_SIZE,                /* bLength */
    UAC_CS_DESC_INTERFACE,              /* bDescriptorType = CS_INTERFACE */
    UAC_AS_DESC_GENERAL,                /* bDescriptorSubType = AS_GENERAL */
    UAC1D_MIC_VOICE_OT,                 /* bTerminalLink = Microphone OT */
    0x00,                               /* bDelay */
    UAC_DATA_FORMAT_TYPE_I_PCM & 0xFF,  /* wFormatTag = PCM */
    UAC_DATA_FORMAT_TYPE_I_PCM >> 8,

    /* Type 1 format type descriptor */
    UAC_FORMAT_DESC_SIZE(WB_VOICE_MIC_SUPPORTED_FREQUENCIES),/* bLength */
    UAC_CS_DESC_INTERFACE,              /* bDescriptorType = CS_INTERFACE */
    UAC_AS_DESC_FORMAT_TYPE,            /* bDescriptorSubType = FORMAT_TYPE */
    UAC_AS_DESC_FORMAT_TYPE_I,          /* bFormatType = FORMAT_TYPE_I */
    UAC1D_VOICE_MIC_CHANNELS,           /* bNumberOfChannels */
    UAC1D_USB_AUDIO_SAMPLE_SIZE,        /* bSubframeSize = 2 bytes */
    UAC1D_USB_AUDIO_SAMPLE_SIZE * 8,    /* bBitsResolution */
    WB_VOICE_MIC_SUPPORTED_FREQUENCIES, /* bSampleFreqType = 1 discrete sampling freq */
    SAMPLE_RATE_16K & 0xFF,             /* tSampleFreq = 16000 */
    (SAMPLE_RATE_16K >> 8 ) & 0xFF,
    (SAMPLE_RATE_16K >> 16) & 0xFF,

    /* Class specific AS isochronous audio data endpoint descriptor */
    UAC_AS_DATA_EP_DESC_SIZE,           /* bLength */
    UAC_CS_DESC_ENDPOINT,               /* bDescriptorType = CS_ENDPOINT */
    UAC_AS_EP_DESC_GENERAL,             /* bDescriptorSubType = AS_GENERAL */
    UAC_EP_CONTROL_SAMPLING_FREQ,       /* bmAttributes = SamplingFrequency contro */
    0x02,                               /* bLockDelayUnits = Decoded PCM samples */
    0x00, 0x00                          /* wLockDelay */
};

static const uac_streaming_config_t voice_wb_streaming_mic_desc = {
    streaming_intf_desc_voice_wb_mic,
    sizeof(streaming_intf_desc_voice_wb_mic)
};


static const uac_streaming_config_t voice_wb_streaming_spkr_desc = {
    streaming_intf_desc_voice_wb_spkr,
    sizeof(streaming_intf_desc_voice_wb_spkr)
};

static const usb_audio_interface_config_t voice_wb_interface_list[] =
{
    {
        .type = USB_AUDIO_DEVICE_TYPE_VOICE_MIC,
        .control_desc =   &uac1_voice_control_mic_desc,
        .streaming_desc = &voice_wb_streaming_mic_desc,
        .endpoint =       &uac1_voice_mic_endpoint
    },
    {
        .type = USB_AUDIO_DEVICE_TYPE_VOICE_SPEAKER,
        .control_desc =   &uac1_voice_control_spkr_desc,
        .streaming_desc = &voice_wb_streaming_spkr_desc,
        .endpoint =       &uac1_voice_spkr_endpoint
    }
};

static const usb_audio_interface_config_list_t voice_wb_interfaces =
{
    .intf = voice_wb_interface_list,
    .num_interfaces = ARRAY_DIM(voice_wb_interface_list)
};


static const usb_audio_config_params_t usb_voice_wb_config =
{
    .rev                     = USB_AUDIO_CLASS_REV_1,
    .volume_config.min_db    = -45,
    .volume_config.max_db    = 0,
    .volume_config.target_db = -9,
    .volume_config.res_db    = 3,
    .min_latency_ms          = 10,
    .max_latency_ms          = 40,
    .target_latency_ms       = 30,

    .intf_list = &voice_wb_interfaces
};

static const usb_class_interface_t consumer_transport_if =
{
    .cb = &UsbHid_ConsumerTransport_Callbacks,
    .config_data = (usb_class_interface_config_data_t)&usb_hid_consumer_transport_config
};

static const usb_class_interface_t datalink_if =
{
    .cb = &UsbHid_Datalink_Callbacks,
    .config_data = (usb_class_interface_config_data_t)&usb_hid_datalink_config
};

static const usb_class_interface_t usb_voice_wb_if =
{
    .cb = &UsbAudio_Callbacks,
    .config_data = (usb_class_interface_config_data_t)&usb_voice_wb_config
};

/* ****************************************************************************
 * Declare class interface structures above, these can be passed into
 * UsbDevice_RegisterClass() to add class interfaces to USB device.
 *
 * Class interface callbacks are mandatory, they are provided by a class driver
 * and declared in its public header.
 *
 * Context data is optional and can be either provided by a class driver
 * or defined here. Context data format is specific to the class driver and
 * is opaque to the USB device framework.
 * ****************************************************************************/

static void usbAppVoiceWb_ConfigDevice(usb_device_index_t dev_index)
{
    usb_result_t result;

    /* Set USB PID */
    if (!UsbHubConfigKey(USB_DEVICE_CFG_PRODUCT_ID, 0x4007))
    {
        Panic();
    }

    result = UsbDevice_GenerateSerialNumber(dev_index, TRUE);
    assert(result == USB_RESULT_OK);
}


static void usbAppVoiceWb_Create(usb_device_index_t dev_index)
{
    usb_result_t result;

    DEBUG_LOG_INFO("UsbAppVoiceWb: Create");

    /* Configuration callback is called to configure device parameters, like
     * VID, PID, serial number, etc right before device is attached to
     * the hub. */
    result = UsbDevice_RegisterConfig(dev_index,
                                      usbAppVoiceWb_ConfigDevice);
    assert(result == USB_RESULT_OK);

    /* Register required USB classes with the framework */

    /* HID Consumer Transport class interface */
    result = UsbDevice_RegisterClass(dev_index,
                                     &consumer_transport_if);
    assert(result == USB_RESULT_OK);

    /* HID Datalink class interface */
    result = UsbDevice_RegisterClass(dev_index,
                                     &datalink_if);
    assert(result == USB_RESULT_OK);

    /* Audio class interface with voice support */
    result = UsbDevice_RegisterClass(dev_index,
                                     &usb_voice_wb_if);
    assert(result == USB_RESULT_OK);

    /* **********************************************************************
     * Register class interfaces above by calling UsbDevice_RegisterClass().
     *
     * USB device framework preserves the order of interfaces so those
     * registered earlier get lower interface numbers.
     *
     * Class interface structures are normally defined as static consts
     * before the usbAppVoice_Create() code.
     * **********************************************************************/
}

static void usbAppVoiceWb_Attach(usb_device_index_t dev_index)
{
    usb_result_t result;

    /* Attach device to the hub to make it visible to the host */
    result = UsbDevice_Attach(dev_index);
    assert(result == USB_RESULT_OK);
}

static void usbAppVoiceWb_Detach(usb_device_index_t dev_index)
{
    usb_result_t result;

    result = UsbDevice_Detach(dev_index);
    assert(result == USB_RESULT_OK);
}

static void usbAppVoiceWb_Close(usb_device_index_t dev_index)
{
    UNUSED(dev_index);

    DEBUG_LOG_INFO("UsbAppVoiceWb: Close");

    /* nothing to clear */
}

const usb_app_interface_t usb_app_voice_wb =
{
    .Create = usbAppVoiceWb_Create,
    .Attach = usbAppVoiceWb_Attach,
    .Detach = usbAppVoiceWb_Detach,
    .Destroy = usbAppVoiceWb_Close
};

#endif /* INCLUDE_USB_NB_WB_TEST */



/*!
\copyright  Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      USB Source Audio application - enumerates HID consumer transport,
            HID datalink and USB Audio classes.
*/

#include "usb_app_source_audio.h"

/* USB trap API */
#include <usb.h>
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

static const uint16 ucq_string[] =
{ 0x0055, /* U */
  0x0043, /* C */
  0x0051, /* Q */
  0x0030, /* 0 */
  0x0030, /* 0 */
  0x0030, /* 0 */
  0x0031, /* 1 */
  0x0030, /* 0 */
  0x0030, /* 0 */
  0x0030, /* 0 */
  0x0030, /* 0 */
  0x0031, /* 1 */
  0x0030, /* 0 */
  0x0031, /* 1 */
  0x0030, /* 0 */
  0x0030, /* 0 */
  0x0030, /* 0 */
  0x0000
};

extern usb_audio_interface_config_list_t usb_source_music_voice_interfaces;

const usb_audio_config_params_t usb_source_audio_config =
{
    .rev                     = USB_AUDIO_CLASS_REV_1,
    .volume_config.min_db    = -45,
    .volume_config.max_db    = 0,
    .volume_config.target_db = -9,
    .volume_config.res_db    = 3,
    .min_latency_ms          = 40,
    .max_latency_ms          = 150,
    .target_latency_ms       = 90,

    .intf_list = &usb_source_music_voice_interfaces
};


static void usbAppSourceAudio_ConfigDevice(usb_device_index_t dev_index)
{
    usb_result_t result;

    /* Set USB PID */
    if (!UsbHubConfigKey(USB_DEVICE_CFG_PRODUCT_ID, 0x4007))
    {
        Panic();
    }

    /* Add the UCQ string at index 0x21 (decimal 33) */
    if (!UsbAddStringDescriptor(0x21, ucq_string))
    {
        Panic();
    }

    result = UsbDevice_GenerateSerialNumber(dev_index, TRUE);
    assert(result == USB_RESULT_OK);
}

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

static const usb_class_interface_t usb_audio =
{
    .cb = &UsbAudio_Callbacks,
    .config_data = (usb_class_interface_config_data_t)&usb_source_audio_config
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


static void usbAppSourceAudio_Create(usb_device_index_t dev_index)
{
    usb_result_t result;

    DEBUG_LOG_INFO("UsbAppSourceAudio: Create");

    /* Configuration callback is called to configure device parameters, like
     * VID, PID, serial number, etc right before device is attached to
     * the hub. */
    result = UsbDevice_RegisterConfig(dev_index,
                                      usbAppSourceAudio_ConfigDevice);
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

    result = UsbDevice_RegisterClass(dev_index,
                                     &usb_audio);
    assert(result == USB_RESULT_OK);

}

static void usbAppSourceAudio_Attach(usb_device_index_t dev_index)
{
    usb_result_t result;

    /* Attach device to the hub to make it visible to the host */
    result = UsbDevice_Attach(dev_index);
    assert(result == USB_RESULT_OK);
}

static void usbAppSourceAudio_Detach(usb_device_index_t dev_index)
{
    usb_result_t result;

    result = UsbDevice_Detach(dev_index);
    assert(result == USB_RESULT_OK);
}

static void usbAppSourceAudio_Close(usb_device_index_t dev_index)
{
    UNUSED(dev_index);

    DEBUG_LOG_INFO("UsbAppSourceAudio: Close");

    /* nothing to clear */
}

const usb_app_interface_t usb_app_source_audio =
{
    .Create = usbAppSourceAudio_Create,
    .Attach = usbAppSourceAudio_Attach,
    .Detach = usbAppSourceAudio_Detach,
    .Destroy = usbAppSourceAudio_Close
};

/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      USB Compliance Test application - enumerates HID consumer transport,
            HID datalink, USB Audio and Mass Storage classes.
*/

#include "usb_app_compliance_test.h"

/* USB trap API */
#include <usb_device.h>
#include <usb_hub.h>

/* device classes */
#include <usb_hid_consumer_transport_control.h>
#include <usb_hid_datalink.h>
#include <usb_audio.h>
#include <usb_audio_class_10_descriptors.h>
#include <usb_audio_class_10_default_descriptors.h>
#include <usb_msc.h>

#include "logging.h"

#include <panic.h>
#include <stdlib.h>

static const usb_audio_config_params_t usb_audio_voice_config =
{
    .rev                     = USB_AUDIO_CLASS_REV_1,
    .volume_config.min_db    = -45,
    .volume_config.max_db    = 0,
    .volume_config.target_db = -9,
    .volume_config.res_db    = 3,
    .min_latency_ms          = 10,
    .max_latency_ms          = 40,
    .target_latency_ms       = 30,

    .intf_list = &uac1_music_spkr_voice_mic_interfaces
};

static usb_msc_config_params_t usb_msc_config;

const char usb_msc_root_name[] = "usb_root";
const char usb_msc_data_name[]  = "usb_data";
const char usb_msc_fat_name[]  = "usb_fat";

static void usbApp_ConfigDevice(usb_device_index_t dev_index)
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

static const usb_class_interface_t usb_audio_voice_if =
{
    .cb = &UsbAudio_Callbacks,
    .config_data = (usb_class_interface_config_data_t)&usb_audio_voice_config
};

static const usb_class_interface_t usb_msc_if =
{
    .cb = &UsbMsc_Callbacks,
    .config_data = (usb_class_interface_config_data_t)&usb_msc_config
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

static void usbApp_Create(usb_device_index_t dev_index)
{
    usb_result_t result;

    DEBUG_LOG_INFO("UsbAppComplianceTest: Create");


    /* Configuration callback is called to configure device parameters, like
     * VID, PID, serial number, etc right before device is attached to
     * the hub. */
    result = UsbDevice_RegisterConfig(dev_index,
                                      usbApp_ConfigDevice);
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
                                     &usb_audio_voice_if);
    assert(result == USB_RESULT_OK);

    if (UsbMsc_PrepareConfig(&usb_msc_config,
                             usb_msc_root_name,
                             usb_msc_data_name,
                             usb_msc_fat_name))
    {
        result = UsbDevice_RegisterClass(dev_index,
                                         &usb_msc_if);
        assert(result == USB_RESULT_OK);
    }
}

static void usbApp_Attach(usb_device_index_t dev_index)
{
    usb_result_t result;

    /* Attach device to the hub to make it visible to the host */
    result = UsbDevice_Attach(dev_index);
    assert(result == USB_RESULT_OK);
}

static void usbApp_Detach(usb_device_index_t dev_index)
{
    usb_result_t result;

    result = UsbDevice_Detach(dev_index);
    assert(result == USB_RESULT_OK);
}

static void usbApp_Close(usb_device_index_t dev_index)
{
    UNUSED(dev_index);

    DEBUG_LOG_INFO("UsbAppComplianceTest: Close");

    /* nothing to clear */
}

const usb_app_interface_t usb_app_compliance_test =
{
    .Create = usbApp_Create,
    .Attach = usbApp_Attach,
    .Detach = usbApp_Detach,
    .Destroy = usbApp_Close
};


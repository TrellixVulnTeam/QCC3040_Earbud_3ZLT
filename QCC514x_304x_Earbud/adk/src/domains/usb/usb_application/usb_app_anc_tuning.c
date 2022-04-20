/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      USB ANC tuning application - USB Audio class for ANC tuning.
*/

#ifdef ENABLE_ANC
#include "usb_app_anc_tuning.h"

/* USB trap API */
#include <usb_device.h>
#include <usb_device_utils.h>
#include <usb_hub.h>

/* device classes */
#include <usb_audio.h>
#include <usb_audio_class_10_descriptors.h>

#include "logging.h"

#include <panic.h>
#include <stdlib.h>

extern usb_audio_interface_config_list_t anc_tuning_interfaces;

static const usb_audio_config_params_t usb_audio_config =
{
    .rev                     = USB_AUDIO_CLASS_REV_1,
    .volume_config.min_db    = -45,
    .volume_config.max_db    = 0,
    .volume_config.target_db = -9,
    .volume_config.res_db    = 3,
    .min_latency_ms          = 10,
    .max_latency_ms          = 40,
    .target_latency_ms       = 30,

    .intf_list = &anc_tuning_interfaces
};

static void usbAppAudio_ConfigDevice(usb_device_index_t dev_index)
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

static const usb_class_interface_t usb_audio =
{
    .cb = &UsbAudio_Callbacks,
    .config_data = (usb_class_interface_config_data_t)&usb_audio_config
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


static void usbAppAudio_Create(usb_device_index_t dev_index)
{
    usb_result_t result;

    DEBUG_LOG_INFO("UsbAppAncTuning: Create");

    /* Configuration callback is called to configure device parameters, like
     * VID, PID, serial number, etc right before device is attached to
     * the hub. */
    result = UsbDevice_RegisterConfig(dev_index,
                                      usbAppAudio_ConfigDevice);
    assert(result == USB_RESULT_OK);

    /* Register required USB classes with the framework */

    result = UsbDevice_RegisterClass(dev_index,
                                     &usb_audio);
    assert(result == USB_RESULT_OK);

    /* **********************************************************************
     * Register class interfaces above by calling UsbDevice_RegisterClass().
     *
     * USB device framework preserves the order of interfaces so those
     * registered earlier get lower interface numbers.
     *
     * Class interface structures are normally defined as static consts
     * before the usbAppAudio_Create() code.
     * **********************************************************************/
}

static void usbAppAudio_Attach(usb_device_index_t dev_index)
{
    usb_result_t result;

    /* Attach device to the hub to make it visible to the host */
    result = UsbDevice_Attach(dev_index);
    assert(result == USB_RESULT_OK);
}

static void usbAppAudio_Detach(usb_device_index_t dev_index)
{
    usb_result_t result;

    result = UsbDevice_Detach(dev_index);
    assert(result == USB_RESULT_OK);
}

static void usbAppAudio_Close(usb_device_index_t dev_index)
{
    UNUSED(dev_index);

    DEBUG_LOG_INFO("UsbAppAncTuning: Close");

    /* nothing to clear */
}

const usb_app_interface_t usb_app_anc_tuning =
{
    .Create = usbAppAudio_Create,
    .Attach = usbAppAudio_Attach,
    .Detach = usbAppAudio_Detach,
    .Destroy = usbAppAudio_Close
};

#endif /*ENABLE_ANC */

/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Utility functions for USB device framework
*/

#ifndef USB_DEVICE_FRAMEWORK_H_
#define USB_DEVICE_FRAMEWORK_H_

/*! Stores pointer to class interface and context data returned
 * by class->Create() callback. */
typedef struct
{
    const usb_class_interface_t *class;
    usb_class_context_t context;
} usb_device_class_data_t;

/*! Per-device context data */
typedef struct usb_device_t
{
    usb_device_index_t index;

    uint8 alloc_to_host_eps;
    uint8 alloc_from_host_eps;
    uint8 alloc_string_index;

    usb_device_config_cb_t config_callback;

    usb_device_class_data_t *classes;
    usb_device_event_handler_t *event_handlers;

    usb_device_released_handler_t released_handler;

    unsigned num_classes:8;
    unsigned num_event_handlers:8;

    unsigned delete_requested:1;
    unsigned deconfigured:1;

    /*! Enable auto generation of USB serial number string descriptor */
    unsigned generate_serial_number:1;
    /*! Set when serial number descriptor has been generated and configured */
    unsigned serial_number_configured:1;

    struct usb_device_t *next;
} usb_device_t;

/* Linked list of USB devices */
extern usb_device_t *usb_devices;

/*! Points at the device currently attached to the hub with UsbHubAttach(). */
extern usb_device_t *attached_device;
/*! Points at the configured device - meaning it's interfaces, endpoints and
 * descriptors added with corresponding USB traps. */
extern usb_device_t *configured_device;


#endif /* USB_DEVICE_FRAMEWORK_H_ */

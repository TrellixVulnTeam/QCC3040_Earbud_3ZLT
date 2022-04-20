/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      USB device framework
*/

#include "usb_device.h"
#include "usb_device_framework.h"
#include "usb_device_utils.h"

#include <usb.h>
#include <usb_hub.h>

#include <bt_device.h>

#include "logging.h"

#include <hydra_macros.h>
#include <panic.h>
#include <stdlib.h>

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_TYPE(usb_device_msg_t)

#ifndef HOSTED_TEST_ENVIRONMENT

/*! There is checking that the messages assigned by this module do
not overrun into the next module's message ID allocation */
ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(USB_DEVICE, USB_DEVICE_MESSAGE_END)

#endif

#define end_point_index_max 0x7f

#define RESERVED_STRING_INDEX 4
#define MAX_USB_STRING_DESCRIPTORS 255

/* Linked list of USB devices */
usb_device_t *usb_devices;

/* Keep track of whether device is attach and configured. */
usb_device_t *attached_device, *configured_device, *pending_attach_device;

/* Find device by index */
static usb_device_t *usbDevice_Find(usb_device_index_t index)
{
    usb_device_t *ptr = usb_devices;
    while (ptr)
    {
        if (ptr->index == index)
        {
            break;
        }
        ptr = ptr->next;
    }
    return ptr;
}

#ifndef USB_DEVICE_DISABLE_AUTO_SERIAL_NUMBER

/* uint16 words of the serial number string are assigned as follows:
 * [0:7]  = USB descriptors hash in HEX
 * [8:19] = bdaddr in HEX
 * [20]   = 0x0000 */
static uint16 usb_device_serial_number[8 + 12 + 1];

static void usbDevice_SafeHexValue(uint16 *dest, int num_digits, uint32 value)
{
    int i;
    for (i=0; i < num_digits; i++)
    {
        uint8 digit = value & 0xf;
        dest[i] = (digit < 10) ? ('0' + digit) : ('A' + digit - 10);
        value >>= 4;
    }
}

#endif

usb_result_t UsbDevice_GenerateSerialNumber(usb_device_index_t index, bool enable)
{
    usb_device_t *device = usbDevice_Find(index);

    if (device == NULL)
    {
        return USB_RESULT_NOT_FOUND;
    }
#ifndef USB_DEVICE_DISABLE_AUTO_SERIAL_NUMBER
    device->generate_serial_number = enable ? 1 : 0;
    return USB_RESULT_OK;
#else
    UNUSED(enable);
    return USB_RESULT_NOT_SUPPORTED;
#endif
}

/* Configure USB device
 *
 * Clear previous configuration, call config callback and create USB classes. */
static void usbDevice_Configure(usb_device_t *device)
{
    if (device->config_callback)
    {
        device->config_callback(device->index);
    }

    for (uint8 i = 0; i < device->num_classes; i++)
    {
        usb_device_class_data_t *data = &device->classes[i];
        assert(data->class->cb->Create);
        assert(!data->context);

        data->context = data->class->cb->Create(device->index,
                                                data->class->config_data);
    }

#ifndef USB_DEVICE_DISABLE_AUTO_SERIAL_NUMBER
    if (device->generate_serial_number &&
            !device->serial_number_configured)
    {
        usb_result_t usb_result;
        uint8 i_string;

        /* Passing "0" means "all supported descriptors", currently that's same as:
         * ((1 << USB_DT_DEVICE) | (1 << USB_DT_CONFIG)) */
        uint32 serial_number = UsbDescriptorsHash(0);

        usbDevice_SafeHexValue(&usb_device_serial_number[0], 8, serial_number);

        bdaddr self_bd_addr;
        if (appDeviceGetMyBdAddr(&self_bd_addr))
        {
            usbDevice_SafeHexValue(&usb_device_serial_number[8], 6, self_bd_addr.lap);
            usbDevice_SafeHexValue(&usb_device_serial_number[14], 2, self_bd_addr.uap);
            usbDevice_SafeHexValue(&usb_device_serial_number[16], 4, self_bd_addr.nap);
        }

        /* Add a string descriptor with the serial number */
        usb_result = UsbDevice_AddStringDescriptor(device->index,
                                                   usb_device_serial_number,
                                                   &i_string);
        assert(usb_result == USB_RESULT_OK);

        /* Configure hub to use the string index returned for serial number */
        if (!UsbHubConfigKey(USB_DEVICE_CFG_SERIAL_NUMBER_STRING, i_string))
        {
            Panic();
        }
        DEBUG_LOG_WARN("UsbDevice: auto serial number");

        device->serial_number_configured = 1;
    }
#endif /* !USB_DEVICE_DISABLE_AUTO_SERIAL_NUMBER */
}

/* Deconfigure USB device
 *
 * Destroy USB classes and re-initialise counters. */
static bool usbDevice_Deconfigure(usb_device_t *device)
{
    if (!device->deconfigured)
    {
        for (uint8 i = 0; i < device->num_classes; i++)
        {
            usb_device_class_data_t *data = &device->classes[i];
            usb_result_t result;
            assert(data->class->cb->Destroy);

            result = data->class->cb->Destroy(data->context);
            switch (result)
            {
            case USB_RESULT_OK:
                data->context = NULL;
                break;

            case USB_RESULT_BUSY:
                /* not released yet */
                break;

            default:
                Panic();
            }
        }

        device->deconfigured = 1;
    }

    for (uint8 i = 0; i < device->num_classes; i++)
    {
        usb_device_class_data_t *data = &device->classes[i];
        if (data->context)
        {
            /* some classes are still waiting for background release to complete */
            return FALSE;
        }
    }

    /* all classes released, device can be deconfigured */
    device->alloc_from_host_eps = 0;
    device->alloc_to_host_eps = 0;
    device->alloc_string_index = RESERVED_STRING_INDEX;

    device->serial_number_configured = 0;

    (void)UsbHubConfigure(NULL);

    configured_device = NULL;

    return TRUE;
}

usb_result_t UsbDevice_Attach(usb_device_index_t index)
{
    usb_device_t *device = usbDevice_Find(index);

    if (device == NULL)
    {
        return USB_RESULT_NOT_FOUND;
    }

    if (attached_device)
    {
        return USB_RESULT_BUSY;
    }

    /* remember device to be attached */
    pending_attach_device = device;

    if (device != configured_device)
    {
        if (configured_device)
        {
            if (!usbDevice_Deconfigure(configured_device))
            {
                /* configured device needs more time to release all classes,
                 * new device will be attached once it is done. */
                return USB_RESULT_OK;
            }
        }
        usbDevice_Configure(device);
        configured_device = device;
    }

    pending_attach_device = NULL;
    attached_device = device;

    if (!UsbHubAttach())
    {
        Panic();
    }

    return USB_RESULT_OK;
}

usb_result_t UsbDevice_Detach(usb_device_index_t index)
{
    usb_device_t *device = usbDevice_Find(index);

    if (device == NULL)
    {
        return USB_RESULT_NOT_FOUND;
    }
    if (attached_device != device)
    {
        if (pending_attach_device == device)
        {
            pending_attach_device = NULL;
            return USB_RESULT_OK;
        }

        return USB_RESULT_FAIL;
    }

    if (!UsbHubDetach())
    {
        Panic();
    }

    attached_device = NULL;
    return USB_RESULT_OK;
}

static usb_device_index_t usbDevice_AllocateIndex(void)
{
    usb_device_index_t index = USB_DEVICE_INDEX_NONE + 1;
    while (index != USB_DEVICE_INDEX_NONE)
    {
        if (usbDevice_Find(index) == NULL)
        {
            break;
        }
        index++;
    }
    return index;
}

usb_result_t UsbDevice_Create(usb_device_index_t *index_ptr)
{
    usb_device_index_t index;
    usb_device_t *device;

    index = usbDevice_AllocateIndex();

    if (index == USB_DEVICE_INDEX_NONE)
    {
        return USB_RESULT_NO_SPACE;
    }
    device = (usb_device_t *)PanicUnlessMalloc(sizeof(usb_device_t));
    memset(device, 0, sizeof(*device));

    device->index = index;
    device->alloc_string_index = RESERVED_STRING_INDEX;

    device->next = usb_devices;

    usb_devices = device;

    if (index_ptr)
    {
        *index_ptr = device->index;
    }
    return USB_RESULT_OK;
}

usb_result_t UsbDevice_Delete(usb_device_index_t index,
                              usb_device_released_handler_t released_handler)
{
    usb_device_t **pp = &usb_devices;
    while (*pp)
    {
        if ((*pp)->index == index)
        {
            usb_device_t *device = *pp;

            device->released_handler = released_handler;

            if (attached_device == device)
            {
                UsbDevice_Detach(index);
            }

            if (configured_device == device)
            {
                device->delete_requested = 1;
                if (!usbDevice_Deconfigure(configured_device))
                {
                    return USB_RESULT_BUSY;
                }
            }

            *pp = (*pp)->next;

            free(device->classes);
            free(device->event_handlers);
            free(device);

            if (released_handler)
            {
                released_handler(index);
            }

            return USB_RESULT_OK;
        }
        pp = &(*pp)->next;
    }
    return USB_RESULT_NOT_FOUND;
}

usb_result_t UsbDevice_RegisterConfig(usb_device_index_t index,
                                      usb_device_config_cb_t config_cb)
{
    usb_device_t *device = usbDevice_Find(index);

    if (device == NULL)
    {
        return USB_RESULT_NOT_FOUND;
    }

    device->config_callback = config_cb;

    return USB_RESULT_OK;
}

usb_result_t UsbDevice_AddStringDescriptor(usb_device_index_t index,
                                           const uint16 *string_desc,
                                           uint8 *i_string_ptr)
{
    usb_device_t *device = usbDevice_Find(index);
    uint8 new_string_index;

    if (device == NULL)
    {
        Panic();
    }

    if (device->alloc_string_index == MAX_USB_STRING_DESCRIPTORS)
    {
        return USB_RESULT_NO_SPACE;
    }

    new_string_index = device->alloc_string_index + 1;

    if (!UsbAddStringDescriptor(new_string_index, string_desc))
    {
        return USB_RESULT_FAIL;
    }

    device->alloc_string_index = new_string_index;
    if (i_string_ptr)
    {
        *i_string_ptr = device->alloc_string_index;
    }
    return USB_RESULT_OK;
}

uint8 UsbDevice_AllocateEndpointAddress(usb_device_index_t index,
                                        bool is_to_host)
{
    usb_device_t *device = usbDevice_Find(index);

    if (device == NULL)
    {
        Panic();
    }

    if (is_to_host)
    {
        if (device->alloc_to_host_eps == end_point_index_max)
        {
            Panic();
        }
        device->alloc_to_host_eps++;
        return device->alloc_to_host_eps | end_point_to_host;
    }
    else
    {
        if (device->alloc_from_host_eps == end_point_index_max)
        {
            Panic();
        }
        device->alloc_from_host_eps++;
        return device->alloc_from_host_eps | end_point_from_host;
    }
}

usb_result_t UsbDevice_RegisterClass(usb_device_index_t index,
                                     const usb_class_interface_t *class)
{
    usb_device_t *device = usbDevice_Find(index);
    usb_device_class_data_t *new_classes;

    if (device == NULL)
    {
        return USB_RESULT_NOT_FOUND;
    }

    if (class == NULL || class->cb->Create == NULL || class->cb->Destroy == NULL)
    {
        return USB_RESULT_INVAL;
    }

    new_classes = (usb_device_class_data_t *)
            realloc(device->classes,
                    (device->num_classes + 1) * sizeof(usb_device_class_data_t));
    if (!new_classes)
    {
        return USB_RESULT_NOT_ENOUGH_MEM;
    }
    device->classes = new_classes;
    device->classes[device->num_classes].class = class;
    device->classes[device->num_classes].context = NULL;
    device->num_classes++;

    return USB_RESULT_OK;
}

void UsbDevice_ReleaseClass(usb_device_index_t index,
                            usb_class_context_t context)
{
    bool all_classes_released = TRUE;

    PanicNull(context);

    usb_device_t *device = usbDevice_Find(index);

    if (device == NULL)
    {
        return;
    }

    for (uint8 i = 0; i < device->num_classes; i++)
    {
        usb_device_class_data_t *data = &device->classes[i];
        if (data->context == context)
        {
            /* this class is now released */
            data->context = NULL;
        }

        if (data->context)
        {
            /* not all classes released yet */
            all_classes_released = FALSE;
        }
    }

    if (all_classes_released)
    {
        if (device->delete_requested)
        {
            UsbDevice_Delete(index, device->released_handler);
        }
        if (pending_attach_device)
        {
            UsbDevice_Attach(pending_attach_device->index);
        }
    }
}

usb_result_t UsbDevice_RegisterEventHandler(usb_device_index_t index,
                                            usb_device_event_handler_t handler)
{
    usb_device_event_handler_t *new_event_handlers;
    usb_device_t *device = usbDevice_Find(index);

    if (device == NULL)
    {
        return USB_RESULT_NOT_FOUND;
    }

    if (handler == NULL)
    {
        return USB_RESULT_INVAL;
    }

    new_event_handlers = (usb_device_event_handler_t *)
            realloc(device->event_handlers,
                    (device->num_event_handlers + 1) * sizeof(usb_device_event_handler_t));
    if (!new_event_handlers)
    {
        return USB_RESULT_NOT_ENOUGH_MEM;
    }
    new_event_handlers[device->num_event_handlers] = handler;
    device->event_handlers = new_event_handlers;
    device->num_event_handlers++;

    return USB_RESULT_OK;
}

usb_result_t UsbDevice_UnregisterEventHandler(usb_device_index_t index,
                                              usb_device_event_handler_t handler)
{
    int i;
    usb_device_t *device = usbDevice_Find(index);

    if (device == NULL)
    {
        return USB_RESULT_NOT_FOUND;
    }

    for (i=0; i < device->num_event_handlers; i++)
    {
        if (device->event_handlers[i] == handler)
        {
            memmove(&device->event_handlers[i], &device->event_handlers[i+1],
                    ((device->num_event_handlers - 1) - i) * sizeof(usb_device_event_handler_t));
            device->num_event_handlers--;
            return USB_RESULT_OK;
        }
    }
    return USB_RESULT_NOT_FOUND;
}



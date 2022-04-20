/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      USB application framework - switch between application.
*/

#include "usb_application.h"
#include "usb_device_utils.h"

#include "logging.h"

#include <panic.h>
#include <stdlib.h>

/*! Store context for the currently active application */
typedef struct usb_app_context_t
{
    /* uint8 used instead of bool to save memory */
    uint8 is_attached_to_hub;
    /*! Device index allocated for the active application.
     * Also if USB_DEVICE_INDEX_NONE indicates that currently active application
     * is being released */
    usb_device_index_t dev_index;

    /*! Currently active application */
    const usb_app_interface_t *active_app;

    /*! Application waiting to become active */
    const usb_app_interface_t *new_app;
} usb_app_context_t;

static usb_app_context_t usb_app_context;

static void usbApplication_Create(const usb_app_interface_t *app);
static void usbApplication_Destroy(void);

/*! Complete USB device release and switch to new application if requested
 *
 * \param index USB device index of the released device */
static void usbApplication_DeviceReleasedHandler(usb_device_index_t index)
{
    UNUSED(index);

    assert(usb_app_context.dev_index == USB_DEVICE_INDEX_NONE);
    assert(usb_app_context.active_app);

    usb_app_context.active_app = NULL;

    /* previous application is now released, create new one if requested */
    if (usb_app_context.new_app)
    {
        usbApplication_Create(usb_app_context.new_app);
        usb_app_context.new_app = NULL;
    }
}

/*! Allocate new USB device instance, create and attach new application.
 *
 * \param app application to be activated */
static void usbApplication_Create(const usb_app_interface_t *app)
{
    usb_result_t result;

    assert(!usb_app_context.active_app);
    assert(usb_app_context.dev_index == USB_DEVICE_INDEX_NONE);

    /* Create USB device instance */
    result = UsbDevice_Create(&usb_app_context.dev_index);
    assert(result == USB_RESULT_OK);

    /* Remember the new active application */
    usb_app_context.active_app = app;

    usb_app_context.active_app->Create(usb_app_context.dev_index);
    UsbApplication_Attach();
}

/*! Detach and destroy currently active application, then initiate destruction
 * of USB device instance. usbApplication_DeviceReleasedHandler() is called when
 * it is done to finalise. */
static void usbApplication_Destroy(void)
{
    /* dev_index == NONE indicates that currently active application is already
     * being released, in which case there is nothing more to do */
    if (usb_app_context.active_app &&
        usb_app_context.dev_index != USB_DEVICE_INDEX_NONE)
    {
        usb_device_index_t dev_index = usb_app_context.dev_index;
        usb_result_t result;

        /* Detach and close previous application */
        UsbApplication_Detach();
        usb_app_context.active_app->Destroy(dev_index);

        /* Indicate that release is pending */
        usb_app_context.dev_index = USB_DEVICE_INDEX_NONE;

        /* Delete USB device */
        result = UsbDevice_Delete(dev_index,
                                  usbApplication_DeviceReleasedHandler);
        /* "OK" indicates device has been released, "BUSY" means device release
         * has been initiated and needs some time to complete.
         * usbApplication_DeviceReleasedHandler() is called either way to
         * finalise. */
        assert(result == USB_RESULT_OK ||
               result == USB_RESULT_BUSY);
    }
}

void UsbApplication_Open(const usb_app_interface_t *app)
{
    DEBUG_LOG_WARN("UsbApplication_Open: 0x%x (active: 0x%x)",
                   app, usb_app_context.active_app);

    if (usb_app_context.active_app == app &&
        usb_app_context.dev_index != USB_DEVICE_INDEX_NONE)
    {
        /* Already active application and not pending release */
        return;
    }

    if (usb_app_context.active_app)
    {
        /* Remember the new application */
        usb_app_context.new_app = app;

        /* Destroy existing app (if not already pending release) */
        usbApplication_Destroy();

        /* Switching to the new app continues when usbApplication_DeviceReleasedHandler() is called */
    }
    else
    {
        /* No active app, switch to the new app immediately */
        usbApplication_Create(app);
    }
}

void UsbApplication_Close(void)
{
    DEBUG_LOG_WARN("UsbApplication_Close: active 0x%x",
                   usb_app_context.active_app);

    /* Destroy existing app (if not already pending release) */
    usbApplication_Destroy();
}

void UsbApplication_Attach(void)
{
    if (usb_app_context.active_app)
    {
        if(!usb_app_context.is_attached_to_hub)
        {
            usb_app_context.active_app->Attach(usb_app_context.dev_index);
            usb_app_context.is_attached_to_hub = 1;
        }
    }
}

void UsbApplication_Detach(void)
{
    if (usb_app_context.active_app)
    {
        if(usb_app_context.is_attached_to_hub)
        {
            usb_app_context.active_app->Detach(usb_app_context.dev_index);
            usb_app_context.is_attached_to_hub = 0;
        }
    }
}

bool UsbApplication_IsAttachedToHub(void)
{
    if (usb_app_context.active_app)
    {
        return usb_app_context.is_attached_to_hub ? TRUE : FALSE;
    }

    return FALSE;
}

bool UsbApplication_IsConnectedToHost(void)
{
    return (UsbApplication_IsAttachedToHub() && UsbDevice_IsConnectedToHost());
}

const usb_app_interface_t *UsbApplication_GetActiveApp(void)
{
    if(usb_app_context.new_app)
    {
        return usb_app_context.new_app;
    }

    return usb_app_context.active_app;
}

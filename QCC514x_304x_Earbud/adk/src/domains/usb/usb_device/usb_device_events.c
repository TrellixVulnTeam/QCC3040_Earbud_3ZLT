/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      USB device framework events
*/

#include "usb_device.h"
#include "usb_device_framework.h"

#include "logging.h"

#include <hydra_macros.h>
#include <stdlib.h>
#include <stream.h>
#include <panic.h>
#include <usb.h>

#include "charger_monitor.h"

/* ANC tuning is still using legacy usb_device_class library */
#include "usb_common.h"

#ifdef INCLUDE_USB_DEVICE

/*! List of tasks registered for notifications from USB device framework */
static task_list_t *usb_device_client_tasks;


/*! \brief Initialisation routine for Wired audio source detect module. */
bool UsbDevice_Init(Task init_task)
{
    UNUSED(init_task);

    StreamConfigure(VM_STREAM_USB_ATTACH_MSG_ENABLED, 1);

    return TRUE;
}


/*! Send USB device framework message to registered clients */
static void UsbDevice_NotifyClients(usb_device_msg_t msg_id)
{
    /* Don't waste time if there are no clients */
    if(TaskList_Size(usb_device_client_tasks) == 0)
        return;

   /* Send message to registered clients */
   TaskList_MessageSendId(usb_device_client_tasks, msg_id);
}

void UsbDevice_ClientRegister(Task client_task)
{
    if (!usb_device_client_tasks)
    {
        usb_device_client_tasks = TaskList_Create();
    }

    TaskList_AddTask(usb_device_client_tasks, client_task);
}

void UsbDevice_ClientUnregister(Task client_task)
{
    TaskList_RemoveTask(usb_device_client_tasks, client_task);
}

bool UsbDevice_IsConnectedToHost(void)
{
    usb_attached_status usb_status = Charger_AttachedStatus();

    if((usb_status == HOST_OR_HUB) || (usb_status == HOST_OR_HUB_CHARGER))
    {
        return TRUE;
    }

    return FALSE;
}
#endif /* INCLUDE_USB_DEVICE */

void UsbDevice_HandleMessage(MessageId id, Message message)
{
    usb_device_t *device = attached_device;

    if (device == NULL)
    {
        /* Pass into the legacy code in usb_common.c - it is still used
         * by the ANC tuning. */
        Usb_HandleMessage(NULL /* ignored */, id, message);
    }

#ifdef INCLUDE_USB_DEVICE

    switch(id)
    {
    case MESSAGE_USB_ALT_INTERFACE:
        if (device)
        {
            for (uint8 i=0; i < device->num_classes; i++)
            {
                usb_device_class_data_t *data = &device->classes[i];
                if (data->class->cb->SetInterface)
                {
                    const MessageUsbAltInterface *msg_alt = (const MessageUsbAltInterface *)message;
                    data->class->cb->SetInterface(data->context,
                                                 msg_alt->interface,
                                                 msg_alt->altsetting);
                }
            }
        }

        break;
    case MESSAGE_USB_ENUMERATED:
        UsbDevice_NotifyClients(USB_DEVICE_ENUMERATED);
        break;
    case MESSAGE_USB_DETACHED:
    case MESSAGE_USB_DECONFIGURED:
        UsbDevice_NotifyClients(USB_DEVICE_DECONFIGURED);
        break;

    case MESSAGE_USB_SUSPENDED:
        if (((MessageUsbSuspended *)message)->has_suspended)
        {
            UsbDevice_NotifyClients(USB_DEVICE_SUSPEND);
        }
        else
        {
            UsbDevice_NotifyClients(USB_DEVICE_RESUME);
        }

        break;

    case MESSAGE_USB_ATTACHED:
        break;
    }

    if (device)
    {
        for (uint8 i = 0; i < device->num_event_handlers; i++)
        {
            device->event_handlers[i](device->index, id, message);
        }
    }
#endif /* INCLUDE_USB_DEVICE */
}



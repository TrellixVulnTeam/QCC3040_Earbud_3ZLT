/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for USB device framework
*/

#ifndef USB_DEVICE_H_
#define USB_DEVICE_H_

#include "domain_message.h"
#include <message.h>
#include <task_list.h>


/*! Result of USB framework function call */
typedef enum
{
    /*! Call was a success */
    USB_RESULT_OK,

    /*! Referenced entity not found */
    USB_RESULT_NOT_FOUND,

    /*! Resource is not available at the moment */
    USB_RESULT_BUSY,

    /*! Resource can't be allocated because limit is reached */
    USB_RESULT_NO_SPACE,

    /*! Dynamic memory allocation failed */
    USB_RESULT_NOT_ENOUGH_MEM,

    /*! Incorrect argument supplied */
    USB_RESULT_INVAL,

    /*! Call was not successful because of a firmware or hardware error */
    USB_RESULT_FAIL,

    /*! Not supported in the current build configuration. */
    USB_RESULT_NOT_SUPPORTED
} usb_result_t;

/*! Unique index of USB device instance */
typedef uint8 usb_device_index_t;

/*! Device index reserved to indicate non existing device */
#define USB_DEVICE_INDEX_NONE ((usb_device_index_t)0)

/*! Create USB device instance and return unique index
 *
 * \param index_ptr Location to store unique USB device index assigned to
 *                  the newly created device.
 * \return result code indicating either success or reason for an error.
 */
usb_result_t UsbDevice_Create(usb_device_index_t *index_ptr);

/*! Function that is called when device is fully released.
 *
 * \param index USB device index */
typedef void (*usb_device_released_handler_t)(usb_device_index_t index);

/*! Delete USB device instance created previously
 *
 * Device can be either released immediately in which case USB_RESULT_OK
 * is returned or it can be postponed waiting for some background work
 * to complete. In the latter case USB_RESULT_BUSY is returned and the caller
 * must not de-allocate or de-configure USB device until released_handler()
 * is called.
 *
 * \param index USB device index
 * \param released_handler function to be called when device is fully released.
 * \return USB_RESULT_OK if device has been released immediately,
 * USB_RESULT_BUSY if release is postponed or otherwise error code.
 */
usb_result_t UsbDevice_Delete(usb_device_index_t index,
                              usb_device_released_handler_t released_handler);


/*! Callback for configuring device
 *
 * Called before device is attached to the host, can be used to set USB
 * device parameters, e.g. VID/PID, add required string descriptors, e.g.
 * serial number, manufacturer id.
 *
 * \param USB device index of the device being attached to the host.
 */
typedef void (*usb_device_config_cb_t)(usb_device_index_t index);

/*! Register device configuration callback
 *
 * \param index USB device index
 * \param config_cb Function to be called before device is attached to the host
 *                  to configure device parameters.
 * \return result code indicating either success or reason for an error.
 */
usb_result_t UsbDevice_RegisterConfig(usb_device_index_t index,
                                      usb_device_config_cb_t config_cb);

/*! Enable automatic generation of USB serial number strings
 *
 * If enabled, USB device framework generates unique string descriptors
 * to identify particular combination of interfaces, endpoints and descriptors
 * and which are also unique across devices.
 *
 * The iSerialNumber field of the USB device descriptor is updated to point
 * to this string descriptor.
 *
 * The generated string consists of two parts:
 * 1. 8-digit value obtained using cryptographic hash function of USB device and
 * USB configuration descriptors.
 * 2. 12-digit value containing Bluetooth address of the device.
 *
 * This feature is disabled by default and should be enabled by the application.
 *
 * If feature is not required, application can define
 * USB_DEVICE_DISABLE_AUTO_SERIAL_NUMBER to save some code and data memory.
 *
 * \param index USB device index
 * \param enable TRUE to enable the feature or FALSE to disable.
 * \return USB_RESULT_OK if success;
 * USB_RESULT_NOT_FOUND if device not found;
 * USB_RESULT_NOT_SUPPORTED if feature support is compiled out.
 */
usb_result_t UsbDevice_GenerateSerialNumber(usb_device_index_t index,
                                            bool enable);


/*! Add string descriptor
 *
 * The string descriptor is added and a unique index is allocated and returned
 * to the caller, this can be used to reference the string descriptor in
 * other descriptors.
 *
 * \param index USB device index
 * \param string_desc array with 16-bit wide characters of the string
 *                    descriptor
 * \param i_string_index location to return unique index assigned to the
 *                       string descriptor
 * \return result code indicating either success or reason for an error.
 */
usb_result_t UsbDevice_AddStringDescriptor(usb_device_index_t index,
                                           const uint16 *string_desc,
                                           uint8 *i_string_ptr);

/*! Allocates unique endpoint address
 *
 * Allocate and return unique endpoint address, this can be then passed into
 * UsbAddEndPoints() to add endpoint descriptors.
 *
 * \param index USB device index
 * \param is_to_host TRUE if requesting USB IN / to host endpoint or FALSE
 *                   for USB OUT / from host endpoint
 * \return result code indicating either success or reason for an error.
 */
uint8 UsbDevice_AllocateEndpointAddress(usb_device_index_t index,
                                        bool is_to_host);

/*! Opaque USB class context data
 *
 * USB framework uses it to reference particular instance of USB class driver.
 */
typedef void * usb_class_context_t;

/*! Class interface configuration data
 *
 * Application can use it to pass configuration data into the class driver */
typedef const void * usb_class_interface_config_data_t;

/*! Create USB class callback, should return context data
 *
 * \param index USB device index
 * \param config_data class specific configuration data
 * \return context Run-time data allocated by the class driver and identifying
 * particular instance of the class driver.
 */
typedef usb_class_context_t (*usb_class_create_cb_t)(usb_device_index_t index,
                                 usb_class_interface_config_data_t config_data);

/*! Delete USB class deallocating context data.
 *
 * \param context run-time data identifying particular instance of the class
 *                driver previously returned by usb_class_create_cb_t().
 * \return USB_RESULT_OK if it is safe to de-configure device or
 * USB_RESULT_BUSY if device framework has to wait until
 * UsbDevice_ClassReleased() is called. Any other code indicates an error.
 */
typedef usb_result_t (*usb_class_destroy_cb_t)(usb_class_context_t context);

/*! Handle SetInterface control request from the host
 *
 * Called every time a SetInterface request is received from the host, before
 * reacting class driver must check that it is addressed to the interface
 * that it owns.
 *
 * \param context run-time data identifying particular instance of the
 *                class driver previously returned by usb_class_create_cb_t()
 * \param interface interface number from the SetInterface request
 * \param altseting interface altsetting number from the SetInterface request
 */
typedef void (*usb_set_interface_cb_t)(usb_class_context_t context,
                                       uint16 interface,
                                       uint16 altsetting);

/*! Class interface callbacks */
typedef struct
{
    /*! Create class interfaces - add interfaces, descriptors, endpoints and
     * allocate run-time data required by the class driver. */
    usb_class_create_cb_t Create;

    /*! Clean up run-time data allocated by the Create() callback.
     *
     * Interfaces, descriptors and endpoints are not removed and USB
     * device framework has to take care of these.
     *
     * USB_RESULT_BUSY is returned to indicate that the class can't be
     * released yet waiting for background work to complete.
     * In this case UsbDevice_ClassReleased() is called when it is safe
     * to de-allocate the class data. */
    usb_class_destroy_cb_t Destroy;

    /*! Handle Set Interface control request. Class driver checks interface
     * number and only reacts if it is addressed to one of the interfaces
     * owned by the class. */
    usb_set_interface_cb_t SetInterface;
} usb_class_interface_cb_t;

/*! Class interface structure
 *
 * Consists of a set of callback and configuration data. */
typedef struct
{
    const usb_class_interface_cb_t *cb;
    usb_class_interface_config_data_t config_data;
} usb_class_interface_t;

/*! Register USB class with the framework.
 *
 * \param index USB device index
 * \param class structure with interface callbacks and configuration data.
 * \return result code indicating either success or reason for an error.
 */
usb_result_t UsbDevice_RegisterClass(usb_device_index_t index,
                                     const usb_class_interface_t *class);

/*! Notify USB device framework that device class has been fully released
 * and device can now be safely de-allocated.
 *
 * \param index USB device index
 * \param context run-time data identifying particular instance of the class
 *                driver previously returned by usb_class_create_cb_t().
 */
void UsbDevice_ReleaseClass(usb_device_index_t index,
                            usb_class_context_t context);

/*! Callback for handling USB events
 *
 * Every listener receives all USB related messages.
 *
 * \param index USB device index
 * \param id message id
 * \param message payload */
typedef  void (*usb_device_event_handler_t)(usb_device_index_t index,
                                            MessageId id, Message message);

/*! Attach USB device to the host
 *
 * Once attached device becomes visible to the host and can now be enumerated.
 *
 * \param index USB device index
 * \return result code indicating either success or reason for an error.
 */
usb_result_t UsbDevice_Attach(usb_device_index_t index);

/*! Detach USB device from the host
 *
 * \param index USB device index
 * \return result code indicating either success or reason for an error.
 */
usb_result_t UsbDevice_Detach(usb_device_index_t index);

/* Handling USB events and notifications */

/*! Handle USB messages received by the system task
 *
 * USB messages are sent to the system task, the system task handler should
 * call this function to pass USB messages into the USB device framework.
 * Other modules should then register with UsbDevice to receive these.
 *
 * \param id message id
 * \param message message payload */
void UsbDevice_HandleMessage(MessageId id, Message message);

/*! Register USB events handler
 *
 * Register callback to receive USB messages.
 *
 * \param index USB device index
 * \param handler callback for handling USB messages
 * \return result code indicating either success or reason for an error.
 */
usb_result_t UsbDevice_RegisterEventHandler(usb_device_index_t index,
                                            usb_device_event_handler_t handler);
/*! Unregister USB events handler
 *
 * Unregister previously registered callback.
 *
 * \param index USB device index
 * \param handler callback previously registered to handle USB messages
 * \return result code indicating either success or reason for an error.
 */
usb_result_t UsbDevice_UnregisterEventHandler(usb_device_index_t index,
                                              usb_device_event_handler_t handler);

/*! USB Device messages sent to registered clients */
typedef enum
{
    /*! Attached USB device has been enumerated by the host */
    USB_DEVICE_ENUMERATED = USB_DEVICE_MESSAGE_BASE,
    /*! USB device de-configured as a result of USB reset,
     * SET_CONFIGURATION(0) request or device being detached from the host */
    USB_DEVICE_DECONFIGURED,
    /*! USB device has been suspended */
    USB_DEVICE_SUSPEND,
    /*! USB device was suspended and is now resumed */
    USB_DEVICE_RESUME,

    /*! This must be the final message */
    USB_DEVICE_MESSAGE_END
} usb_device_msg_t;

/*! Initialise USB device framework
 *
 * \param init_task ignored
 * \return always returns TRUE */
#ifdef INCLUDE_USB_DEVICE
bool UsbDevice_Init(Task init_task);
#else
#define UsbDevice_Init(init_task) (FALSE)
#endif

/*! check whether USB attached to host or not
 *
 * \return returns TRUE if usb attached to host else FALSE */
#ifdef INCLUDE_USB_DEVICE
bool UsbDevice_IsConnectedToHost(void);
#else
#define UsbDevice_IsConnectedToHost() (FALSE)
#endif

/*! Register client task to receive USB device messages
 *
 * \param client_task client task data */
#ifdef INCLUDE_USB_DEVICE
void UsbDevice_ClientRegister(Task client_task);
#else
#define UsbDevice_ClientRegister(client_task) ((void)(client_task))
#endif

/*! UnRegister client task to stop receiving USB device messages
 *
 * \param client_task client task data */
#ifdef INCLUDE_USB_DEVICE
void UsbDevice_ClientUnregister(Task client_task);
#else
#define UsbDevice_ClientUnregister(client_task) ((void)(client_task))
#endif

#endif /* USB_DEVICE_H_ */

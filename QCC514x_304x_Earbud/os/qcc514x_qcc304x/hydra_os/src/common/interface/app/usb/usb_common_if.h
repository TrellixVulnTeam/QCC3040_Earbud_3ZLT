/* Copyright (c) 2017-2021 Qualcomm Technologies International, Ltd. */
/*   %%version */

/* Common constants and data types from USB specification */

#ifndef APP_USB_COMMON_IF_H
#define APP_USB_COMMON_IF_H

/** Enums for USB bus speed. */
typedef enum usb_device_speed
{
    USB_BUS_SPEED_LOW = 0,
    USB_BUS_SPEED_FULL = 1,
    USB_BUS_SPEED_HIGH = 2,
    USB_BUS_SPEED_SUPER = 3,
    USB_BUS_SPEED_UNDEF = 0xff
} usb_bus_speed;

/** Format of Setup Data for control transfers. */
typedef struct usb_control_setup
{
    /** Characteristics of request.
     * Bit 7: Data transfer direction \ref USB_ENDPOINT_DIRECTION;
     * Bits 6..5: Type \ref USB_REQUEST_TYPE;
     * Bits 4..0: Recipient \ref USB_REQUEST_RECIPIENT.
     */
    uint8  bmRequestType;

    /** Specific request, depends on bmRequestType::Type.
     * For standard request \ref USB_STANDARD_REQUEST. */
    uint8  bRequest;

    /** Value. Request dependent. */
    uint16 wValue;

    /** Index. Request dependent. */
    uint16 wIndex;

    /** Request length in bytes. */
    uint16 wLength;
} usb_control_setup;

/** Class codes */
typedef enum
{
    /** Use class code info from Interface Descriptors */
    USB_CLASS_DEVICE = 0,

    /** Audio class */
    USB_CLASS_AUDIO = 1,

    /** Communications and CDC Control class */
    USB_CLASS_CDC = 2,

    /** Human Interface Device class */
    USB_CLASS_HID = 3,

    /** Physical */
    USB_CLASS_PHYSICAL = 5,

    /** Still Imaging */
    USB_CLASS_IMAGE = 6,

    /** Printing Devices class */
    USB_CLASS_PRINTER = 7,

    /** Mass storage class */
    USB_CLASS_MASS_STORAGE = 8,

    /** Hub class */
    USB_CLASS_HUB = 9,

    /** CDC Data class */
    USB_CLASS_CDC_DATA = 10,

    /** Smart Card class */
    USB_CLASS_SMART_CARD = 0x0b,

    /** Content Security class */
    USB_CLASS_CONTENT_SECURITY = 0x0d,

    /** Video class */
    USB_CLASS_VIDEO = 0x0e,

    /** Personal Healthcare class */
    USB_CLASS_PERSONAL_HEALTHCARE = 0x0f,

    /** Audio/Video (AV) Devices class */
    USB_CLASS_AUDIO_VIDEO = 0x10,

    /** Billboard class */
    USB_CLASS_BILLBOARD = 0x11,

    /** USB Type-C Bridge class */
    USB_CLASS_TYPE_C_BRIDGE = 0x12,

    /** Diagnostic Device class */
    USB_CLASS_DIAGNOSTIC_DEVICE = 0xdc,

    /** Wireless Controller class */
    USB_CLASS_WIRELESS = 0xe0,

    /** Miscellaneous class */
    USB_CLASS_MISC = 0xef,

    /** Application Specific class */
    USB_CLASS_APPLICATION = 0xfe,

    /** Vendor Specific class */
    USB_CLASS_VENDOR_SPEC = 0xff
} USB_CLASS_CODES;

/** Size in bytes of data in the SETUP stage of control transfer */
#define USB_CONTROL_SETUP_SIZE 8

/** Descriptor types from USB specification. */
typedef enum
{
    /** Device descriptor. \ref USB_DEVICE_DESCRIPTOR. */
    USB_DT_DEVICE = 1,

    /** Configuration descriptor. \ref USB_CONFIG_DESCRIPTOR. */
    USB_DT_CONFIG = 2,

    /** String descriptor */
    USB_DT_STRING = 3,

    /** Interface descriptor. \ref USB_INTERFACE_DESCRIPTOR. */
    USB_DT_INTERFACE = 4,

    /** Endpoint descriptor. \ref USB_ENDPOINT_DESCRIPTOR. */
    USB_DT_ENDPOINT = 5,

    /** Device qualifier descriptor */
    USB_DT_DEVICE_QUALIFIER = 6,

    /** Other speed configuration descriptor */
    USB_DT_OTHER_SPEED_CONFIGURATION = 7,

    /** Interface power descriptor */
    USB_DT_INTERFACE_POWER = 8,

    /** BOS descriptor */
    USB_DT_BOS = 15,

    /** Device Capability descriptor */
    USB_DT_DEVICE_CAPABILITY = 16,

    /** HID descriptor */
    USB_DT_HID = 33,

    /** HID report descriptor */
    USB_DT_REPORT = 34,

    /** HID Physical descriptor */
    USB_DT_PHYSICAL = 35,

    /** Hub descriptor */
    USB_DT_HUB = 41
} USB_DESCRIPTOR_TYPE;

/** USB device descriptor size */
#define USB_DT_DEVICE_SIZE           18
/** USB device qualifier descriptor size */
#define USB_DT_DEVICE_QUALIFIER_SIZE 10
/** USB configuration descriptor size */
#define USB_DT_CONFIG_SIZE           9
/** USB interface descriptor size */
#define USB_DT_INTERFACE_SIZE        9
/** USB endpoint descriptor size */
#define USB_DT_ENDPOINT_SIZE         7
/** Size of string descriptor zero, specifying languages supported by the device */
#define USB_DT_LANGID_CODES_SIZE(num_codes) (2 + (num_codes) * 2)
/** USB endpoint descriptor size of an audio-class device */
#define USB_DT_ENDPOINT_AUDIO_SIZE       9   /* Audio extension */

/** The mask to get the endpoint address value from bEndpointAddress */
#define USB_ENDPOINT_ADDRESS_MASK    0x0f
/** The mask to get the endpoint direction bit from bEndpointAddress */
#define USB_ENDPOINT_DIR_MASK        0x80

/** Standard Feature Selectors types as defined by the USB specification. */
typedef enum
{
    /** Endpoint halt feature selector */
    ENDPOINT_HALT = 0x00,

    /** Device remote wake feature selector */
    DEVICE_REMOTE_WAKEUP = 0x01,

    /** Test mode feature selector */
    TEST_MODE = 0x02
} USB_FEATURE_SELECTOR_TYPE;

/** Endpoint direction. Bit #7 in USB_ENDPOINT_DESCRIPTOR::bEndpointAddress.
 */
typedef enum
{
    /** Device to host */
    USB_ENDPOINT_IN = 0x80,

    /** Host to device */
    USB_ENDPOINT_OUT = 0x00
}  USB_ENDPOINT_DIRECTION;

/** The mask to get ::USB_TRANSFER_TYPE from
 * \ref USB_ENDPOINT_DESCRIPTOR::bmAttributes */
#define USB_TRANSFER_TYPE_MASK           0x03    /* in bmAttributes */

/** Endpoint transfer type. Bits 1:0 in USB_ENDPOINT_DESCRIPTOR::bmAttributes.
 */
typedef enum
{
    /** Control endpoint type */
    USB_TRANSFER_TYPE_CONTROL = 0,

    /** Isochronous endpoint type */
    USB_TRANSFER_TYPE_ISOCHRONOUS = 1,

    /** Bulk endpoint type */
    USB_TRANSFER_TYPE_BULK = 2,

    /** Interrupt endpoint type */
    USB_TRANSFER_TYPE_INTERRUPT = 3
} USB_TRANSFER_TYPE;

/** Standard requests, as defined in table 9-5 of the USB 3.2 specifications. */
typedef enum
{
    /** Returns status for the specified recipient. */
    USB_REQUEST_GET_STATUS = 0x00,

    /** Clear or disable a specific feature. */
    USB_REQUEST_CLEAR_FEATURE = 0x01,

    /** Set or enable a specific feature. */
    USB_REQUEST_SET_FEATURE = 0x03,

    /** Sets the device address for all future device accesses. */
    USB_REQUEST_SET_ADDRESS = 0x05,

    /** Returns the specified descriptor if the descriptor exists. */
    USB_REQUEST_GET_DESCRIPTOR = 0x06,

    /** Optional and may be used to update existing descriptors or
     * new descriptors may be added.  */
    USB_REQUEST_SET_DESCRIPTOR = 0x07,

    /** Returns the current device configuration value. */
    USB_REQUEST_GET_CONFIGURATION = 0x08,

    /** Sets the device configuration. */
    USB_REQUEST_SET_CONFIGURATION = 0x09,

    /** Returns the selected alternate setting for the specified interface. */
    USB_REQUEST_GET_INTERFACE = 0x0A,

    /** Select an alternate setting for the specified interface. */
    USB_REQUEST_SET_INTERFACE = 0x0B,

    /** Set and then report an endpoint's synchronization frame. */
    USB_REQUEST_SYNCH_FRAME = 0x0C,

    /** Sets both the U1 and U2 System Exit Latency and the U1 or U2
     * exit latency for all the links between a device and a root
     * port on the host. */
    USB_REQUEST_SET_SEL = 0x30,

    /** Informs the device of the delay from the time a host transmits a packet
     * to the time it is received by the device */
    USB_SET_ISOCH_DELAY = 0x31
} USB_STANDARD_REQUEST;

/** Data transfer direction bit in \ref usb_control_setup::bmRequestType. */
typedef enum
{
    /** Host to device direction */
    USB_HOST_TO_DEVICE = 0x00,

    /** Device to host direction */
    USB_DEVICE_TO_HOST = 0x01
} USB_REQUEST_DIRECTION;

/** Request type bits in \ref usb_control_setup::bmRequestType. */
typedef enum
{
    /** Standard request. */
    USB_REQUEST_TYPE_STANDARD = 0x00,

    /** Class request. */
    USB_REQUEST_TYPE_CLASS = 0x01,

    /** Vendor request. */
    USB_REQUEST_TYPE_VENDOR = 0x02,

    /** Reserved, not used. */
    USB_REQUEST_TYPE_RESERVED = 0x03
} USB_REQUEST_TYPE;

/** Recipient bits in \ref usb_control_setup::bmRequestType. */
typedef enum
{
    /** Request to Device. */
    USB_RECIPIENT_DEVICE = 0x00,

    /** Request to Interface. */
    USB_RECIPIENT_INTERFACE = 0x01,

    /** Request to Endpoint. */
    USB_RECIPIENT_ENDPOINT = 0x02,

    /** None of above */
    USB_RECIPIENT_OTHER = 0x03
} USB_REQUEST_RECIPIENT;

/** The mask to get isochronous endpoint synchronisation type from
 * \ref USB_ENDPOINT_DESCRIPTOR::bmAttributes */
#define USB_ISO_SYNC_TYPE_MASK       0x0C

/** Types of isochronous endpoints synchronisation. Bits 3:2 in
 * \ref USB_ENDPOINT_DESCRIPTOR::bmAttributes.
 */
typedef enum
{
    /** Synchronization is not used */
    USB_ISO_SYNC_TYPE_NONE = 0,

    /** Asynchronous endpoint. */
    USB_ISO_SYNC_TYPE_ASYNC = 1,

    /** Adaptive endpoint. */
    USB_ISO_SYNC_TYPE_ADAPTIVE = 2,

    /** Synchronous endpoint. */
    USB_ISO_SYNC_TYPE_SYNC = 3
} USB_ISO_SYNC_TYPE;

/** The mask to get type of isochronous endpoint from
 * \ref USB_ENDPOINT_DESCRIPTOR::bmAttributes*/
#define USB_ISO_USAGE_TYPE_MASK 0x30

/** Isochronous endpoints usage type. Bits 5:4 in
 * \ref USB_ENDPOINT_DESCRIPTOR::bmAttributes.
 */
typedef enum {
    /** Data endpoint usage. */
    USB_ISO_USAGE_TYPE_DATA = 0,

    /** Feedback endpoint usage. */
    USB_ISO_USAGE_TYPE_FEEDBACK = 1,

    /** Implicit feedback Data endpoint usage. */
    USB_ISO_USAGE_TYPE_IMPLICIT = 2
} USB_ISO_USAGE_TYPE;

/** A structure representing a generic USB device descriptor header. */
typedef struct
{
    /** Size of this descriptor (in bytes) */
    uint8 bLength;

    /** Descriptor type. */
    uint8 bDescriptorType;
} USB_GENERIC_DESCRIPTOR;

/** USB 3.2 specification, 9.6.1, Standard Device Descriptor. */
typedef struct
{
    /** Size of this descriptor in bytes. */
    uint8  bLength;

    /** DEVICE Descriptor Type. */
    uint8  bDescriptorType;

    /** USB Specification Release Number in Binary-Coded Decimal
     * (i.e., 2.10 is 210H). This field identifies the release of the
     * USB Specification with which the device and its descriptors are
     * compliant. */
    uint16 bcdUSB;

    /** Class code (assigned by the USB-IF). */
    uint8  bDeviceClass;

    /** Subclass code (assigned by the USB-IF). */
    uint8  bDeviceSubClass;

    /** Protocol code (assigned by the USB-IF). */
    uint8  bDeviceProtocol;

    /** Maximum packet size for endpoint zero. */
    uint8  bMaxPacketSize0;

    /** Vendor ID (assigned by the USB-IF). */
    uint16 idVendor;

    /** Product ID (assigned by the USB-IF). */
    uint16 idProduct;

    /** Device release number in binary-coded decimal. */
    uint16 bcdDevice;

    /** Index of string descriptor describing manufacturer. */
    uint8  iManufacturer;

    /** Index of string descriptor describing product. */
    uint8  iProduct;

    /** Index of string descriptor describing the device's serial number. */
    uint8  iSerialNumber;

    /** Number of possible configurations. */
    uint8  bNumConfigurations;
} USB_DEVICE_DESCRIPTOR;

/** USB 3.2 specification, 9.6.3, Standard Configuration Descriptor. */
typedef struct
{
    /** Size of this descriptor in bytes. */
    uint8  bLength;

    /** CONFIGURATION Descriptor Type. */
    uint8  bDescriptorType;

    /** Total length of data returned for this configuration. */
    uint16 wTotalLength;

    /** Number of interfaces supported by this configuration. */
    uint8  bNumInterfaces;

    /** Value to use as an argument to the SetConfiguration() request
     * to select this configuration. */
    uint8  bConfigurationValue;

    /** Index of string descriptor describing this configuration. */
    uint8  iConfiguration;

    /** Configuration characteristics:
     * D7: Reserved (set to one)
     * D6: Self-powered
     * D5: Remote Wakeup
     * D4...0: Reserved (reset to zero) */
    uint8  bmAttributes;

    /** Maximum power consumption of the device from the bus in this
     * specific configuration when the device is fully operational.
     * Expressed in 2 mA units when the device is operating in FS/HS modes. */
    uint8  MaxPower;
} USB_CONFIG_DESCRIPTOR;

/** USB 3.2 specification, 9.6.5, Standard Interface Descriptor. */
typedef struct
{
    /** Size of this descriptor in bytes. */
    uint8  bLength;

    /** INTERFACE Descriptor Type. */
    uint8  bDescriptorType;

    /** Number of this interface. Zero-based value identifying the index
     * in the array of concurrent interfaces supported by this configuration. */
    uint8  bInterfaceNumber;

    /** Value used to select this alternate setting for the interface
     * identified in the prior field. */
    uint8  bAlternateSetting;

    /** Number of endpoints used by this interface (excluding the Default
     * Control Pipe). If this value is zero, this interface only uses the
     * Default Control Pipe. */
    uint8  bNumEndpoints;

    /** Class code (assigned by the USB-IF). */
    uint8  bInterfaceClass;

    /** Subclass code (assigned by the USB-IF). */
    uint8  bInterfaceSubClass;

    /** Protocol code (assigned by the USB). */
    uint8  bInterfaceProtocol;

    /** Index of string descriptor describing this interface. */
    uint8  iInterface;
} USB_INTERFACE_DESCRIPTOR;

/** USB 3.2 specification, 9.6.6, Standard Endpoint Descriptor. */
typedef struct
{
    /** Size of this descriptor in bytes. */
    uint8  bLength;

    /** ENDPOINT Descriptor Type. */
    uint8  bDescriptorType;

    /** The address of the endpoint on the device described by this
     * descriptor. The address is encoded as follows:
     * Bit 3...0: The endpoint number;
     * Bit 6...4: Reserved, reset to zero;
     * Bit 7: Direction, ignored for control endpoints
     * 0 = OUT endpoint
     * 1 = IN endpoint */
    uint8  bEndpointAddress;

    /** This field describes the endpoint's attributes when it is
     * configured using the bConfigurationValue.
     * Bits 1..0: Transfer Type
     * 00 = Control
     * 01 = Isochronous
     * 10 = Bulk
     * 11 = Interrupt
     *
     * If an interrupt endpoint, bits 5..2 are defined as follows:
     * Bits 3..2: Reserved
     * Bits 5..4: Usage Type
     * 00 = Periodic
     * 01 = Notification
     * 10 = Reserved
     * 11 = Reserved
     *
     * If isochronous, they are defined as follows:
     * Bits 3..2: Synchronization Type
     * 00 = No Synchronization
     * 01 = Asynchronous
     * 10 = Adaptive
     * 11 = Synchronous
     * Bits 5..4: Usage Type
     * 00 = Data endpoint
     * 01 = Feedback endpoint
     * 10 = Implicit feedback Data endpoint
     * 11 = Reserved
     *
     * If not an isochronous or interrupt endpoint, bits 5..2 are
     * reserved and shall be set to zero.
     * All other bits are reserved and shall be reset to zero.
     * Reserved bits shall be ignored by the host.
     */
    uint8  bmAttributes;

    /** Maximum packet size this endpoint is capable of sending or
     * receiving when this configuration is selected. */
    uint16 wMaxPacketSize;

    /** Interval for servicing the endpoint for data transfers.
     * Expressed in 125us units (HS) or 1000us units (FS). */
    uint8  bInterval;

    /** Only for audio class devices: synchronization feedback rate. */
    uint8  bRefresh;

    /** Only for audio class devices: synch endpoint address */
    uint8  bSynchAddress;
} USB_ENDPOINT_DESCRIPTOR;

#define ENDPOINT_TYPE(e) ((USB_TRANSFER_TYPE)((e)->bmAttributes & 3))

/** Check if the endpoint descriptor is of interrupt type */
#define IS_INTR_ENDPOINT(e) (ENDPOINT_TYPE(e) == USB_TRANSFER_TYPE_INTERRUPT)
/** Check if the endpoint descriptor is of control type */
#define IS_CTRL_ENDPOINT(e) (ENDPOINT_TYPE(e) == USB_TRANSFER_TYPE_CONTROL)
/** Check if the endpoint descriptor is of isochronous type */
#define IS_ISOC_ENDPOINT(e) (ENDPOINT_TYPE(e) == USB_TRANSFER_TYPE_ISOCHRONOUS)
/** Check if the endpoint descriptor is of bulk type */
#define IS_BULK_ENDPOINT(e) (ENDPOINT_TYPE(e) == USB_TRANSFER_TYPE_BULK)

#define IS_TO_HOST_ENDPOINT(e) ((e)->bEndpointAddress & USB_ENDPOINT_DIR_MASK)

/** Type for USB device address */
typedef uint8 usb_device_addr;

/** Type for USB device endpoint number */
typedef uint8 usb_endpoint_number;

#endif /* APP_USB_COMMON_IF_H */

/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Class specific definitions for USB HID
*/

#ifndef USB_HID_CLASS_H_
#define USB_HID_CLASS_H_

#define B_INTERFACE_CLASS_HID 0x03
#define B_INTERFACE_SUB_CLASS_HID_NO_BOOT 0x00
#define B_INTERFACE_PROTOCOL_HID_NO_BOOT 0x00

#define HID_DESCRIPTOR_LENGTH 9
#define B_DESCRIPTOR_TYPE_HID 0x21
#define B_DESCRIPTOR_TYPE_HID_REPORT 0x22

#define USB_REPORT_TYPE_INPUT (1 << 8)

/** HID 1.11 spec, 7.2 Class-Specific Requests */
typedef enum {
    HID_GET_REPORT = 0x01,
    HID_GET_IDLE = 0x02,
    HID_GET_PROTOCOL = 0x03,
    HID_SET_REPORT = 0x09,
    HID_SET_IDLE = 0x0A,
    HID_SET_PROTOCOL = 0x0B
} b_request_hid_t;

/** HID 1.11 spec, 6.2.2.2 Short Items */
typedef enum {
    HID_REPORT_MAIN = 0,
    HID_REPORT_GLOBAL = 1,
    HID_REPORT_LOCAL = 2,
} hid_report_b_type_t;

/** HID 1.11 spec, 6.2.2.4 Main Items */
typedef enum
{
    HID_REPORT_INPUT = 8,
    HID_REPORT_OUTPUT = 9,
    HID_REPORT_FEATURE = 11,
    HID_REPORT_COLLECTION = 10,
    HID_REPORT_END_COLLECTION = 12
} usb_hid_report_main_t;

/** HID 1.11 spec, 6.2.2.7 Global Items */
typedef enum
{
    HID_REPORT_USAGE_PAGE = 0,
    HID_REPORT_LOGICAL_MINIMUM = 1,
    HID_REPORT_LOGICAL_MAXIMUM = 2,
    HID_REPORT_PHYSICAL_MINIMUM = 3,
    HID_REPORT_PHYSICAL_MAXIMUM = 4,
    HID_REPORT_UNIT_EXPONENT = 5,
    HID_REPORT_UNIT = 6,
    HID_REPORT_SIZE = 7,
    HID_REPORT_ID = 8,
    HID_REPORT_COUNT = 9,
    HID_REPORT_PUSH = 10,
    HID_REPORT_POP = 11
} usb_hid_report_global_t;

/** HID 1.11 spec, 6.2.2.8 Local Items */
typedef enum
{
    HID_REPORT_USAGE = 0,
    HID_REPORT_USAGE_MINIMUM = 1,
    HID_REPORT_USAGE_MAXIMUM = 2,
    HID_REPORT_DESIGNATOR_INDEX = 3,
    HID_REPORT_DESIGNATOR_MINIMUM = 4,
    HID_REPORT_DESIGNATOR_MAXIMUM = 5,
    HID_REPORT_STRING_INDEX = 7,
    HID_REPORT_STRING_MIMIMUM = 8,
    HID_REPORT_STRING_MAXIMUM = 9,
    HID_REPORT_DELIMITER = 10
} usb_hid_report_local_t;

/*! Class-specific HID interface descriptor */
typedef struct
{
    const uint8*        descriptor;
    uint16              size_descriptor;
} usb_hid_class_desc_t;

/*! HID report descriptor */
typedef struct
{
    const uint8*        descriptor;
    uint16              size_descriptor;
} usb_hid_report_desc_t;

/*! HID endpoint settings */
typedef struct
{
    /*! Direction - "1": to_host or "0": from_host */
    uint8 is_to_host;

    /*! Polling interval */
    uint8 bInterval;

    /*! Maximum packet size in bytes */
    uint16 wMaxPacketSize;
} usb_hid_endpoint_desc_t;

/*! HID interface configuration */
typedef struct
{

    /*! Class-specific HID interface descriptor */
    const usb_hid_class_desc_t   *class_desc;

    /*! HID report descriptor */
    const usb_hid_report_desc_t *report_desc;

    /*! HID Endpoints list */
    const usb_hid_endpoint_desc_t  *endpoints;

    /*! Number of HID endpoints */
    int num_endpoints;
} usb_hid_config_params_t;

#endif /* USB_HID_CLASS_H_ */

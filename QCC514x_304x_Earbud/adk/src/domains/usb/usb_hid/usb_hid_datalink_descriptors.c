/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      USB HID Datalink default descriptors
*/

#include "usb_hid_datalink_descriptors.h"

static const uint8 report_descriptor_hid_datalink[] =
{
    0x06, 0x00, 0xFF,                   /* Vendor Defined Usage Page 1 */

    0x09, 0x01,                         /* Vendor Usage 1 */
    0xA1, 0x01,                         /* Collection (Application) */
    0x15, 0x00,                         /* Logical Minimum */
    0x26, 0xFF, 0x00,                   /* Logical Maximum */
    0x75, 0x08,                         /* Report size (8 bits) */

    0x09, 0x02,                         /* Vendor Usage 2 */
    0x96,                               /* Report count */
    (HID_REPORTID_DATA_TRANSFER_SIZE&0xff),
    (HID_REPORTID_DATA_TRANSFER_SIZE>>8),
    0x85, HID_REPORTID_DATA_TRANSFER,   /* Report ID */
    0x91, 0x02,                         /* OUTPUT Report */

    0x09, 0x02,                         /* Vendor Usage 2 */
    0x96,                               /* Report count */
    (HID_REPORTID_UPGRADE_DATA_TRANSFER_SIZE&0xff),
    (HID_REPORTID_UPGRADE_DATA_TRANSFER_SIZE>>8),
    0x85, HID_REPORTID_UPGRADE_DATA_TRANSFER, /* Report ID */
    0x91, 0x02,                         /* OUTPUT Report */

    0x09, 0x02,                         /* Vendor Usage 2 */
    0x95,                               /* Report count */
    (HID_REPORTID_RESPONSE_SIZE&0xff),
    0x85, HID_REPORTID_RESPONSE,        /* Report ID */
    0x81, 0x02,                         /* INPUT (Data,Var,Abs) */

    0x09, 0x02,                         /* Vendor Usage 2 */
    0x96,                               /* Report count */
    (HID_REPORTID_UPGRADE_RESPONSE_SIZE & 0xff),
    (HID_REPORTID_UPGRADE_RESPONSE_SIZE >> 8),
    0x85, HID_REPORTID_UPGRADE_RESPONSE,/* Report ID */
    0x81, 0x02,                         /* INPUT (Data,Var,Abs) */

    0x09, 0x02,                         /* Vendor Usage 2 */
    0x95,                               /* Report count */
    (HID_REPORTID_COMMAND_SIZE&0xff),
    0x85, HID_REPORTID_COMMAND,         /* Report ID */
    0xB1, 0x02,                         /* Feature Report */

    0x09, 0x02,                         /* Vendor Usage 2 */
    0x95,                               /* Report count */
    (HID_REPORTID_CONTROL_SIZE&0xff),
    0x85, HID_REPORTID_CONTROL,         /* Report ID */
    0xB1, 0x02,                         /* Feature Report */

    0xC0,                               /* End of Collection */

    0x09, 0x03,                         /* Vendor Usage 3 */
    0xA1, 0x01,                         /* Collection (Application) */

    0x09, 0x02,                         /* Vendor Usage 2 */
    0x96,                               /* Report count */
    (HID_REPORTID_TEST_TRANSFER_SIZE&0xff),
    (HID_REPORTID_TEST_TRANSFER_SIZE>>8),
    0x85, HID_REPORTID_TEST_TRANSFER,   /* Report ID */
    0x91, 0x02,                         /* OUTPUT Report */

    0x09, 0x02,                         /* Vendor Usage 2 */
    0x96,                               /* Report count */
    (HID_REPORTID_TEST_RESPONSE_SIZE & 0xff),
    (HID_REPORTID_TEST_RESPONSE_SIZE >> 8),
    0x85, HID_REPORTID_TEST_RESPONSE,   /* Report ID */
    0x81, 0x02,                         /* INPUT (Data,Var,Abs) */

    0x09, 0x02,                         /* Vendor Usage 2 */
    0x95,                               /* Report count */
    (HID_REPORTID_TEST_SHORT_RESPONSE_SIZE & 0xff),
    0x85, HID_REPORTID_TEST_SHORT_RESPONSE,   /* Report ID */
    0x81, 0x02,                         /* INPUT (Data,Var,Abs) */


    0xC0                                /* End of Collection */
};

/* See the USB HID 1.11 spec section 6.2.1 for description */
static const uint8 interface_descriptor_hid_datalink[] =
{
    HID_DESCRIPTOR_LENGTH,                  /* bLength */
    B_DESCRIPTOR_TYPE_HID,                  /* bDescriptorType */
    0x11, 0x01,                             /* HID class release number (1.00).
                                             * The 1st and the 2nd byte denotes
                                             * the minor & major Nos respectively
                                             */
    0x00,                                   /* Country code (None) */
    0x01,                                   /* Only one class descriptor to follow */
    B_DESCRIPTOR_TYPE_HID_REPORT,           /* Class descriptor type (HID Report) */
    sizeof(report_descriptor_hid_datalink), /* Report descriptor length. LSB first */
    0x00                                    /* followed by MSB */
};

const usb_hid_class_desc_t usb_hid_datalink_class_desc = {
        .descriptor = interface_descriptor_hid_datalink,
        .size_descriptor = sizeof(interface_descriptor_hid_datalink)
};

const usb_hid_report_desc_t usb_hid_datalink_report_desc = {
        .descriptor = report_descriptor_hid_datalink,
        .size_descriptor = sizeof(report_descriptor_hid_datalink)
};

const usb_hid_endpoint_desc_t usb_hid_datalink_endpoints[] = {
        {
            .is_to_host = FALSE,
            .wMaxPacketSize = 64,
            .bInterval = 1
        },
        {
            .is_to_host = TRUE,
            .wMaxPacketSize = 64,
            .bInterval = 1
        }
};

const usb_hid_config_params_t usb_hid_datalink_config = {
        .class_desc = &usb_hid_datalink_class_desc,
        .report_desc = &usb_hid_datalink_report_desc,
        .endpoints = usb_hid_datalink_endpoints,
        .num_endpoints = ARRAY_DIM(usb_hid_datalink_endpoints)
};

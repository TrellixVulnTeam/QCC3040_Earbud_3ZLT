/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      USB HID Consumer Transport default descriptors
*/

#include "usb_hid_consumer_transport_descriptors.h"


/* HID Report Descriptor - Consumer Transport Control Device */
static const uint8 report_descriptor_hid_consumer_transport[] =
{
    0x05, 0x0C,                  /* USAGE_PAGE (Consumer Devices) */
    0x09, 0x01,                  /* USAGE (Consumer Control) */
    0xA1, 0x01,                  /* COLLECTION (Application) */

    0x85, USB_HID_CONSUMER_TRANSPORT_REPORT_ID, /*   REPORT_ID (1) */

    0x15, 0x00,                  /*   LOGICAL_MINIMUM (0) */
    0x25, 0x01,                  /*   LOGICAL_MAXIMUM (1) */
    0x09, 0xCD,                  /*   USAGE (Play/Pause - OSC) */
    0x09, 0xB5,                  /*   USAGE (Next Track - OSC) */
    0x09, 0xB6,                  /*   USAGE (Previous Track - OSC) */
    0x09, 0xB7,                  /*   USAGE (Stop - OSC) */
    0x75, 0x01,                  /*   REPORT_SIZE (1) */
    0x95, 0x04,                  /*   REPORT_COUNT (4) */
    0x81, 0x02,                  /*   INPUT (Data,Var,Abs) */

    0x15, 0x00,                  /*   LOGICAL_MINIMUM (0) */
    0x25, 0x01,                  /*   LOGICAL_MAXIMUM (1) */
    0x09, 0xB0,                  /*   USAGE (Play - OOC) */
    0x09, 0xB1,                  /*   USAGE (Pause - OOC) */
    0x09, 0xB3,                  /*   USAGE (Fast Forward -OOC) */
    0x09, 0xB4,                  /*   USAGE (Rewind - OOC) */
    0x75, 0x01,                  /*   REPORT_SIZE (1) */
    0x95, 0x04,                  /*   REPORT_COUNT (4) */
    0x81, 0x22,                  /*   INPUT (Data,Var,Abs,NoPref) */

    0x15, 0x00,                  /*   LOGICAL_MINIMUM (0) */
    0x25, 0x01,                  /*   LOGICAL_MAXIMUM (1) */
    0x09, 0xE9,                  /*   USAGE (Volume Increment - RTC) */
    0x09, 0xEA,                  /*   USAGE (Volume Decrement - RTC) */
    0x75, 0x01,                  /*   REPORT_SIZE (1) */
    0x95, 0x02,                  /*   REPORT_COUNT (2) */
    0x81, 0x02,                  /*   INPUT (Data,Var,Abs,Bit Field) */

    0x09, 0xE2,                  /*   USAGE (Mute - OOC) */
    0x75, 0x01,                  /*   REPORT_SIZE (1) */
    0x95, 0x01,                  /*   REPORT_COUNT (1) */
    0x81, 0x06,                  /*   INPUT (Data,Var,Rel,Bit Field) */

    0x75, 0x01,                  /*   REPORT_SIZE (1) */
    0x95, 0x05,                  /*   REPORT_COUNT (5) */
    0x81, 0x01,                  /*   INPUT (Const) */

    0xC0,                        /* END_COLLECTION */

    0x05, 0x0B,                  /* USAGE_PAGE (Telephpny Devices) */
    0x09, 0x05,                  /* USAGE (0x05:Headset); (0x01:Phone)*/
    0xA1, 0x01,                  /* COLLECTION (Application) */

    0x85, USB_HID_TELEPHONY_REPORT_ID, /*   REPORT_ID (2) */
    0x05, 0x0B,                  /* USAGE_PAGE (Telephpny Devices) */
    0x15, 0x00,                  /*   LOGICAL_MINIMUM (0) */
    0x25, 0x01,                  /*   LOGICAL_MAXIMUM (1) */

    0x09, 0x2F,                  /*   USAGE (Phone Mute - OOC) */
    0x75, 0x01,                  /*   REPORT_SIZE (1) */
    0x95, 0x01,                  /*   REPORT_COUNT (1) */
    0x81, 0x06,                  /*   INPUT (Data, Var, Rel, PrefState, Bit Field) */

    0x09, 0x20,                  /*   USAGE (Hook Switch - OOC) */
    0x09, 0x21,                  /*   USAGE (Flash - MC) */
    0x75, 0x01,                  /*   REPORT_SIZE (1) */
    0x95, 0x02,                  /*   REPORT_COUNT (2) */
    0x81, 0x22,                  /*   INPUT (Data, Var, Abs, NoPref, Bit Field) */

    0x09, 0x07,                  /*   Usage (Programmable Button) */
    0x05, 0x09,                  /*   Usage Page (Button) */
    0x09, 0x01,                  /*   Usage (0x01) */
    0x75, 0x01,                  /*   Report Size (1) */
    0x95, 0x01,                  /*   Report Count (1) */
    0x81, 0x02,                  /*   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position) */

    0x75, 0x01,                  /*   REPORT_SIZE (1) */
    0x95, 0x0C,                  /*   REPORT_COUNT (12) */
    0x81, 0x01,                  /*   INPUT (Constant, Array, Abs, PrefState, Bit Field) */

    0x05, 0x08,                  /*   Usage Page (LEDs) */
    0x75, 0x01,                  /*   REPORT_SIZE (1) */

    0x85, USB_HID_LED_MUTE_REPORT_ID,                  /*   Report ID (9) */
    0x09, 0x09,                  /*   Usage (Mute)  */
    0x95, 0x01,                  /*   Report Count (1) */
    0x91, 0x22,                  /*   Output (Data,Var,Abs,NWrp,Lin,NPrf,NNul,NVol,Bit) */
    0x95, 0x07,                  /*   Report Count (7)  */
    0x91, 0x01,                  /*   Output (Cnst,Ary,Abs,NWrp,Lin,Pref,NNul,NVol,Bit) */

    0x85, USB_HID_LED_OFF_HOOK_REPORT_ID,                  /*   Report ID (23) */
    0x09, 0x17,                  /*   Usage (Off-Hook)  */
    0x95, 0x01,                  /*   Report Count (1) */
    0x91, 0x22,                  /*   Output (Data,Var,Abs,NWrp,Lin,NPrf,NNul,NVol,Bit) */
    0x95, 0x07,                  /*   Report Count (7)  */
    0x91, 0x01,                  /*   Output (Cnst,Ary,Abs,NWrp,Lin,Pref,NNul,NVol,Bit) */

    0x85, USB_HID_LED_RING_REPORT_ID,                  /*   Report ID (24) */
    0x09, 0x18,                  /*   Usage (Ring)  */
    0x95, 0x01,                  /*   Report Count (1) */
    0x91, 0x22,                  /*   Output (Data,Var,Abs,NWrp,Lin,NPrf,NNul,NVol,Bit) */
    0x95, 0x07,                  /*   Report Count (7)  */
    0x91, 0x01,                  /*   Output (Cnst,Ary,Abs,NWrp,Lin,Pref,NNul,NVol,Bit) */
    0xC0,                        /*   END_COLLECTION */

};

/* USB HID class descriptor - Consumer Transport Control Device*/
static const uint8 interface_descriptor_hid_consumer_transport[] =
{
    HID_DESCRIPTOR_LENGTH,              /* bLength */
    B_DESCRIPTOR_TYPE_HID,              /* bDescriptorType */
    0x11, 0x01,                         /* bcdHID */
    0,                                  /* bCountryCode */
    1,                                  /* bNumDescriptors */
    B_DESCRIPTOR_TYPE_HID_REPORT,       /* bDescriptorType */
    sizeof(report_descriptor_hid_consumer_transport),   /* wDescriptorLength */
    0                                   /* wDescriptorLength */
};

const usb_hid_class_desc_t usb_hid_consumer_transport_class_desc = {
        .descriptor = interface_descriptor_hid_consumer_transport,
        .size_descriptor = sizeof(interface_descriptor_hid_consumer_transport)
};

const usb_hid_report_desc_t usb_hid_consumer_transport_report_desc = {
        .descriptor = report_descriptor_hid_consumer_transport,
        .size_descriptor = sizeof(report_descriptor_hid_consumer_transport)
};

const usb_hid_endpoint_desc_t usb_hid_consumer_transport_endpoints[] = {
        {
                .is_to_host = TRUE,
                .bInterval = 8,
                .wMaxPacketSize = 64
        }
};

const usb_hid_config_params_t usb_hid_consumer_transport_config = {
        .class_desc = &usb_hid_consumer_transport_class_desc,
        .report_desc = &usb_hid_consumer_transport_report_desc,
        .endpoints = usb_hid_consumer_transport_endpoints,
        .num_endpoints = ARRAY_DIM(usb_hid_consumer_transport_endpoints)
};

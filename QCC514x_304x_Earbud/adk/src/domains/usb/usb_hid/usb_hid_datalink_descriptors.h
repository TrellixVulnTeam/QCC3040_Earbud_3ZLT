/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for using default descriptors of USB HID Consumer Transport
*/

#ifndef USB_HID_DATALINK_DESCRIPTORS_H
#define USB_HID_DATALINK_DESCRIPTORS_H

#include "usb_hid_class.h"

/*
 * Report size is limited by the size of the smallest of internal buffers.
 *
 * 1. from-host buffer for interrupt transfers, 128 bytes + 1 byte reserved for
 * buffer management:
 * 4096 bytes - 128 bytes - 1 byte = 3967 bytes
 *
 * 2. to-host buffer for interrupt transfers: 128 byte + 1 byte reserved for
 * buffer management, 128 bytes reserved as required by USB HW:
 * 2048 bytes - 128 bytes 1 byte - 128 bytes = 1791 bytes
 *
 * 3. from-host buffer for control transfers, 128 bytes + 1 byte reserved for
 * buffer management, has to be big enough for two transfers:
 * (1024 bytes - 128 bytes - 1 byte) / 2 = 447 bytes
 *
 * One byte is reserved for report_id, giving maximum report size:
 * 447 bytes - 1 byte = 446 bytes.
 */

#define HID_REPORTID_DATA_TRANSFER          1   /* data from host for AHI */
#define HID_REPORTID_DATA_TRANSFER_SIZE     62

#define HID_REPORTID_RESPONSE               2   /* device response for AHI */
#define HID_REPORTID_RESPONSE_SIZE          16

#define HID_REPORTID_COMMAND                3   /* command channel */
#define HID_REPORTID_COMMAND_SIZE          62

#define HID_REPORTID_CONTROL                4   /* control channel dedicated to HID library */
#define HID_REPORTID_CONTROL_SIZE          62

#define HID_REPORTID_UPGRADE_DATA_TRANSFER  5   /* data from host for Upgrade */
#define HID_REPORTID_UPGRADE_DATA_TRANSFER_SIZE 254

#define HID_REPORTID_UPGRADE_RESPONSE       6   /* device response for Upgrade */
#define HID_REPORTID_UPGRADE_RESPONSE_SIZE  12

#define HID_REPORTID_TEST_TRANSFER          7   /* from-host data for throughput test */
#define HID_REPORTID_TEST_TRANSFER_SIZE 446

#define HID_REPORTID_TEST_RESPONSE          8   /* to-host data for throughput test */
#define HID_REPORTID_TEST_RESPONSE_SIZE 446

#define HID_REPORTID_TEST_SHORT_RESPONSE 9   /* to-host data for throughput test */
#define HID_REPORTID_TEST_SHORT_RESPONSE_SIZE 11

/*! Default USB HID datalink class descriptor */
extern const usb_hid_class_desc_t usb_hid_datalink_class_desc;
/*! Default USB HID datalink report descriptor */
extern const usb_hid_report_desc_t usb_hid_datalink_report_desc;
/*! Default USB HID datalink endpoint config */
extern const usb_hid_endpoint_desc_t usb_hid_datalink_endpoint;
/*! Default USB HID datalink configuration */
extern const usb_hid_config_params_t usb_hid_datalink_config;


#endif /* USB_HID_DATALINK_DESCRIPTORS_H */

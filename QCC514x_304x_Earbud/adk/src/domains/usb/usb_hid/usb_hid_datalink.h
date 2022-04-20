/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      USB HID datalink class driver

USB HID datalink class can be used for vendor specific communication with USB host.

Application provides custom report descriptors and configuration data to the
class driver in a configuration structure.  Example is available
in usb_hid_datalink_descriptors.c and can be used as a reference.

Class can be configured with one or two interrupt endpoints. Bi-directional
communication is supported: from-host data arrives in interrupt or control transfers,
to-host data uses only interrupt transfers.

Upper layer protocol should implement flow control to avoid overflowing RX and
TX buffers. While buffer overflows are prevented (RX data is NAK-ed by device,
TX data is rejected) application can run out of memory quickly if it is
buffering TX data that does not fit into the Sink.

RX data can be fragmented if RX buffer is not large enough to fit the
whole transfer. In this case application first receives chunk(s) of data that
is(are) multiple of the interrupt endpoint MaxPacketSize. The last chunk has
the size that is not a multiple of MaxPacketSize. This affects only
interrupt data transfers.

Application must never attempt to send more data than can fit into the buffer,
the limits are as follows:

from-host interrupt transfers: 3839 bytes.
to-host interrupt transfers: 1791 bytes.
from-host SetReport control requests: 447 bytes.
*/

#ifndef USB_HID_DATALINK_H_
#define USB_HID_DATALINK_H_

#include <usb_device.h>
#include <usb_device_utils.h>
#include "usb_hid_class.h"

#include "usb_hid_datalink_descriptors.h"

/*! Handler for USB HID datalink data coming from the host
 *
 * The host can send report to device using either interrupt data endpoint
 * or in a control transfer. The data is passed as received from the host,
 * including Report ID in the first byte if present.
 *
 * \param report_id Report ID. For reports received over an interrupt endpoint
 * this is the first byte of the transfer. For reports received in the
 * SetReport control request, this is (wValue & 0xff).
 * \param data Report data. The data is not valid after the handler returns.
 * \param size Report data size in bytes.
 */
typedef void (*usb_hid_handler_t)(uint8 report_id, const uint8 *data, uint16 size);

/*! Register handler for report data coming from the host
 *
 * The handler is called every time report data is received from the host.
 * \param handler Handler for USB HID datalink data coming from the host */
void UsbHid_Datalink_RegisterHandler(usb_hid_handler_t handler);

/*! Deregister previously registered handler
 *
 * \param handler Previously registered handler */
void UsbHid_Datalink_UnregisterHandler(usb_hid_handler_t handler);

/*! Send HID report data to the host
 *
 * If report_id is not 0 it is sent in the first byte of the transfer followed
 * by the report data, otherwise the data is sent as it is:
 *
 * report_id = 0: [data(0)], [data(1)] ... [data(report_size - 1)]
 * report_id > 0: [report_id] [data(0)], [data(1)], ... [data(report_size-1)]
 *
 * If data_size is less than the report size then report is padded with zeros to
 * the report_size.
 *
 * \param report_id HID report id, sent in the first byte of the transfer.
 * \param report_data HID report data
 * \param data_size size of data to send
 * \return USB_RESULT_OK if data was successfully sent.
 * USB_RESULT_NO_SPACE if not enough space in the TX buffer.
 * USB_RESULT_NOT_FOUND if active USB application does not have
 * USB HID datalink class with to-host interrupt endpoint. */
usb_result_t UsbHid_Datalink_SendReport(uint8 report_id,
                                        const uint8 *report_data,
                                        uint16 data_size);

/*! USB HID datalink interface
 *
 * Custom report descriptors and configuration parameters shall be supplied
 * in a configuration structure of type "usb_hid_config_params_t". */
extern const usb_class_interface_cb_t UsbHid_Datalink_Callbacks;

#endif /* USB_HID_DATALINK_H_ */

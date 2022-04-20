/*******************************************************************************
Copyright (c) 2016 - 2021 Qualcomm Technologies International, Ltd.
 

FILE NAME
    hid_upgrade.h

DESCRIPTION
    Interface for the USB HID Upgrade Transport.
*******************************************************************************/

#ifndef USB_DEVICE_CLASS_REMOVE_HID

#ifndef HID_UPGRADE_H_
#define HID_UPGRADE_H_

#include <csrtypes.h>

/**
 * @file hid_upgrade.h
 * @brief Header file for HID USB Upgrade.
 *
 * @page hid_upgrade USB HID report handling for the upgrade transport.
 *
 * @section Incoming (from host) reports
 * @subsection Set Reports
 *
 * The following set reports are handled by this transport:
 *  - HID_REPORTID_UPGRADE_DATA_TRANSFER (5)
 *  - HID_REPORTID_COMMAND               (3)
 *
 * The HID_REPORTID_CONTROL (4) report is not handled as the functionality it
 * provides is not required or is duplicated by HID_REPORTID_COMMAND.
 *
 * HID_REPORTID_DATA_TRANSFER
 * The data and data size provided in this report are passed to
 * UpgradeProcessDataRequest (note: the data is buffered first).
 *
 * HID_REPORTID_COMMAND
 * The associated data contains a 2 octet 'command'. The supported values for
 * this command are:
 *  - HID_CMD_CONNECTION_REQ (2) - Calls UpgradeTransportConnectRequest
 *  - HID_CMD_DISCONNECT_REQ (7) - Calls UpgradeTransportDisconnectRequest
 *
 * Other commands are not supported.
 *
 * @subsection Get Reports
 *
 * No get reports are handled by this transport.
 *
 * @section Outgoing (to host) messages
 *
 * After connection, data will be sent to the host from this transport via calls
 * to the handler passed to HidUpgradeRegisterInputReportCb(). These will have
 * the report ID HID_REPORTID_UPGRADE_RESPONSE (6). The data size is passed
 * along with the report and will be a maximum length of 12 octets.
 *
 * UPGRADE_TRANSPORT_DATA_IND will send a response report containing a data
 * packet.
 *
 * UPGRADE_TRANSPORT_DATA_CFM does not send any response.
 *
 * UPGRADE_TRANSPORT_CONNECT_CFM will send a response report containing the
 * upgrade_status_t status.
 *
 * UPGRADE_TRANSPORT_DISCONNECT_CFM does not send any response.
 */

typedef void (*hid_upgrade_input_report_cb_t)(uint8 report_id, const uint8 *data, uint16 size);

/*! Register handler for report data going to the host (input reports).
 *
 * The handler is called every time report data needs to be sent to the host.
 * This is implemented as a callback function rather than a message, so that the
 * data pointer can be accessed directly and is used immediately (avoids the
 * need to buffer the data or add it to the message queue).
 *
 * \param handler Handler for USB HID datalink data going to the host */
void HidUpgradeRegisterInputReportCb(hid_upgrade_input_report_cb_t handler);

/**
 * @brief Handles a USB HID Output/Feature Report from the host.
 *
 * Generally called by an application's message handling task in response to
 * receiving report data from the Host via the USB HID class.
 *
 * @param [in] The message's report id.
 * @param [in] The message's report size.
 * @param [in] A pointer to the message's report data.
 *
 * @returns The number of reports currently in the queue, will be non-zero if the
 *          report was queued successfully. Exact value can be used for tracking
 *          the current queue length, statistics / diagnostics etc.
 */
uint16 HidUpgradeHandleReport(uint16 report_id,
                              uint16 data_in_size,
                              const uint8 *data_in);

uint16 HidUpgradeGetStatsMaxReportQueueLen(void);

#endif /* HID_UPGRADE_H_ */

#endif /* !USB_DEVICE_CLASS_REMOVE_HID */

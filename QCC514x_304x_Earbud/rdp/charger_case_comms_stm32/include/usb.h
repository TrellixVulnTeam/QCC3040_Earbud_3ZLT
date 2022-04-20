/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      USB
*/

#ifndef USB_H_
#define USB_H_

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include <stdbool.h>
#include <stdint.h>

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

void usb_init(void);
void usb_start(void);
void usb_stop(void);
void usb_tx_periodic(void);
void usb_rx_periodic(void);
void usb_tx_complete(void);
void usb_tx(uint8_t *data, uint16_t len);
void usb_rx(uint8_t *data, uint32_t len);
void usb_ready(void);
void usb_not_ready(void);
bool usb_has_enumerated(void);
void usb_chg_detected(void);
void usb_activate_bcd(void);
void usb_deactivate_bcd(void);
bool usb_dcd(void);
bool usb_pdet(void);
bool usb_sdet(void);
void usb_dcd_disable(void);
void usb_primary_detection_enable(void);
void usb_primary_detection_disable(void);
void usb_secondary_detection_enable(void);
void usb_secondary_detection_disable(void);
void usb_connected(void);
void usb_disconnected(void);
uint64_t usb_serial_num(void);

#endif /* USB_H_ */

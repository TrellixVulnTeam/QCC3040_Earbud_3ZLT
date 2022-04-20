/* Copyright (c) 2016 Qualcomm Technologies International, Ltd. */
/*   %%version */
/**
 * \file
 * \defgroup bus_message bus_message
 * \section bus_message_intro INTRODUCTION
 * \ingroup core
 * Hydra bus_message device "driver" (interface)
 *
 * This is only a hint of a driver really. The only substantial driving logic
 * currently lives in \ref submsg (the main, but not the only, client).
 *
 * This module does provide:-
 * - a means for competing clients to negotiate use of the device or
 * parts of it: See bus_message_tx_hardware_request()
 * - an interface to the main hardware functions.
 * - hiding of register/control details.
 * - a place to migrate driving logic if clients get more numerous and
 * demanding.
 *
 * Not so nice:-
 * - Exposes some low level functions that require intimate knowledge of
 * the device to use safely - this is unavoidable so long as the real
 * driving logic lives in clients.
 * - Ther is no tx queue internally so the only way for clients to wait for
 * tx hardware is to spin.
 * - Doesn't do much driving at all!
 *
 * Future:-
 * - move device-specific-driving logic from \ref submsg in here.
 */
#ifndef BUS_MESSAGE_H
#define BUS_MESSAGE_H

/*****************************************************************************
 * Dependencies
 ****************************************************************************/

#include "hydra/hydra.h"
#include "hydra/hydra_trb.h"
#include "hydra/hydra_txbus.h"

/*****************************************************************************
 * Interface - Primitives
 *
 * These should only be used by driver/clients that know what state the
 * bus_message device is in - and have reserved it if appropriate.
 ****************************************************************************/

/**
 * Transmit TRXBus 64 bit message via bus_message block.
 *
 * Will be sent subject to the various TRX retry settings.
 *
 * Caller/driver must be prepared to handle IRQ etc.
 */
extern void bus_message_transmit_message(
    hydra_ssid dest,
    uint16 tag,
    const hydra_trb_msg *msg,
    bool no_interrupt
);

/**
 * Transmit arbitrary transaction via bus_message block.
 *
 * Use with extreme care! This will put absolutely any old rubbish onto the
 * transaction bus and allows spoofing of the source into the bargain.
 */
extern void bus_message_transmit_arbitrary_transaction(
    const hydra_trb_trx *trx
);

/*****************************************************************************
 * Public - Hardware Reservation
 *
 * There is no dedicated driver so clients must be pretty savvy and
 * cooperate - this request/release may help.
 ****************************************************************************/

extern bool bus_message_tx_hardware_request(void);

extern void bus_message_tx_hardware_release(void);

/**
 * If the chip has the ability then wait for the bus_message hardware to
 * become available. This will raise the panic
 * \c PANIC_BUS_MESSAGE_HW_SEND_TIMED_OUT if it has to wait more than
 * \c HYDRA_TXBUS_TRANSACTION_SEND_TIMEOUT_USECS
 */
extern void bus_message_wait_for_hardware_idle_state(void);

/*****************************************************************************
 * Public - Blocking API
 *
 * In some limited cases it may be necessary/possible to make blocking
 * requests to the hardware.
 ****************************************************************************/

extern void bus_message_blocking_tx_hardware_request(void);

/**
 * Spin till TX hardware is free and transmit arbitrary transaction via
 * bus_message block.
 *
 * Use with extreme care! This will put absolutely any old rubbish onto the
 * transaction bus and allows spoofing of the source into the bargain.
 */
extern void bus_message_blocking_transmit_arbitrary_transaction(
    const hydra_trb_trx *trx
);

/**
 * Spin till TX hardware is free and perform a Debug Write to a remote
 * subsystem via bus_message block.
 */
extern void bus_message_blocking_debug_write(
    hydra_ssid dest_subsystem_id,
    HYDRA_TXBUS_ADDR dest_trbus_address,
    uint32 data,
    size_t num_signifigant_bytes
);

#endif /* ndef BUS_MESSAGE_H */

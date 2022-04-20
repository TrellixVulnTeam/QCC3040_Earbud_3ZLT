/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      UART
*/

#ifndef UART_H_
#define UART_H_

/*-----------------------------------------------------------------------------
------------------ TYPE DEFINITIONS -------------------------------------------
-----------------------------------------------------------------------------*/

typedef enum
{
    UART_CLI,
#ifdef SCHEME_B
    UART_DOCK,
#ifdef CHARGER_COMMS_FAKE_U
    UART_EARBUD,
#endif
#endif
    NO_OF_UARTS
}
UART_ID;

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

/**
 * \brief Intialises all UART's used by the system.
 */
void uart_init(void);

/**
 * \brief Disable the clock going to the UART.
 *        This must be done when dynamically changing the system clock or
 *        when entering low power modes where UART is not required.
 */
void uart_clock_disable(void);

/**
 * \brief Periodic function to process all data to transmit on all UARTs.
 */
void uart_tx_periodic(void);

/**
 * \brief Period function to process all incoming data on all UARTs.
 */
void uart_rx_periodic(void);

/**
 * \brief Request a chunk of data to be transmitted on a UART
 * \param uart_no The UART to transmit on.
 * \param data The buffer of data to transmit
 * \param len The number of octets to transmit from the data buffer
 */
void uart_tx(UART_ID uart_no, uint8_t *data, uint16_t len);

/**
 * \brief Force all queued UART transmit data onto the HW.
 *        This is predominantly designed for when the system locks up and
 *        we want to dump as much debug information as possible.
 */
void uart_dump(void);

#endif /* UART_H_ */

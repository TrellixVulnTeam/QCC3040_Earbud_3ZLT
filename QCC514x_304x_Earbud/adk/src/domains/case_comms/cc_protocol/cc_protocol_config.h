/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    case_comms
\brief      Case comms protocol configuration.
*/
/*! \addtogroup case_comms
@{
*/

#ifndef CC_PROTOCOL_CONFIG_H
#define CC_PROTOCOL_CONFIG_H

#ifdef INCLUDE_CASE_COMMS

#include <stream.h>

/*! Definition of the PIO to use for TX and RX in Scheme B single wire UART chargercomms.
    \note This must be an LED PIO and defaults to LED 4 but can be overriden in project
          properties. 
    \note The PIO can be configured in 2 ways:
            * On platforms where CHIP_LED_BASE_PIO is defined as the same pio as LED0,
              just a definition of the LED number via CASE_COMMS_LED is sufficient. For
              example CASE_COMMS_LED=1
            * Where CHIP_LED_BASE_PIO does not correspond to LED0 pio, then CASE_COMMS_PIO
              can be used to explicitly define the pio. In this instance CASE_COMMS_LED
              still needs to be defined, as the LED# is used elsewhere in this component.
              For example, CASE_COMMS_PIO=59 and CASE_COMMS_LED=1
*/
#ifndef CASE_COMMS_LED
#define CASE_COMMS_LED (4)
#endif
#ifdef CASE_COMMS_PIO
#define CcProtocol_ConfigSchemeBTxRxPio()   (CASE_COMMS_PIO)
#else
#define CcProtocol_ConfigSchemeBTxRxPio()   (CHIP_LED_BASE_PIO + CASE_COMMS_LED)
#endif

/*! Time to wait before sending poll to an Earbud to get outstanding response message. */
#define CcProtocol_ConfigPollScheduleTimeoutMs()   (20)

/*! Number of transmit failures before deciding to transmit a broadcast reset. */
#define CcProtocol_ConfigNumFailsToReset()          (1)

/*! Baud rate to use for Scheme B transport. */
#define CcProtocol_ConfigSchemeBBaudRate()          (VM_UART_RATE_1500K)

/*! For those transports which support being enabled and disabled, use this
    configuration to control if the transport is automatically enabled
    during system initialisation.

    If TRUE, then case comms is enabled on startup and available for use without
    further API calls.

    If FALSE, then case comms is not enabled on startup and CcProtocol_Enable()
    must be called before case comms can transmit or receive packets.

    \note Only scheme B transport supports this feature.
          The expected use case is to permit an alternate use of the UART at
          startup and then enable case comms, rather than always having to
          disable case comms at startup before being able to use the UART.
*/
#define CcProtocol_ConfigEnableTransportOnStartup() (TRUE)

#endif /* INCLUDE_CASE_COMMS */
#endif  /* CC_PROTOCOL_CONFIG_H */
/*! @} End of group documentation */

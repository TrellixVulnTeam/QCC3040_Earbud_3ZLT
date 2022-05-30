/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    case_comms
\brief      Transmit and receive handling for test UART transport.

 Each transport defines:-
    - A transport_scheme_data structure to hold transport specific state
    - Transmit function (and define to common ccProtocol_TransTransmit symbol
    - Receive function
    - Setup function (optional)

*/
/*! \addtogroup case_comms
@{
*/

#ifndef CC_PROTOCOL_TRANS_TEST_UART_H
#define CC_PROTOCOL_TRANS_TEST_UART_H

#ifdef INCLUDE_CASE_COMMS

#ifdef HAVE_CC_TRANS_TEST_UART
#include "cc_protocol.h"
#include "cc_protocol_private.h"

#include <source.h>
#include <sink.h>

/*! This UART test interface can be used in the absence of chargercomms scheme A
    or B support. A 4-wire UART connection needs to be made between two development
    boards.

    This transport only provides simple transmit and receive of messages,
    there are no acknowledgements, CRC checking or NAK handling.

   Tested on 20-CH140-1 (QCC5144) where the PIOs below are brought
   out on the I2S header J41 on a CF376 dev board.

   Connect pins 1..4 on J41 as follows:
        1 (RTS) --> 2 (CTS)
        2 (CTS) <-- 1 (RTS)
        3 (TX)  --> 4 (RX)
        4 (RX)  <-- 3 (TX)
*/

/*! PIO definitions for QCC5144 access to UART on I2S header.
@{ */
#define TEST_UART_RTS_PIO   (16)
#define TEST_UART_CTS_PIO   (17)
#define TEST_UART_TX_PIO    (18)
#define TEST_UART_RX_PIO    (19)
/*! @} */

/*! Test UART uses a single byte header to supply SRC/DST info and it is embedded
    in the stream data, rather than in a stream header. */
#define TEST_UART_CHARGERCOMMS_HEADER_OFFSET    (0)
#define TEST_UART_CHARGERCOMMS_HEADER_LEN       (1)
#define TEST_UART_CASECOMMS_HEADER_OFFSET       (1)
#define TEST_UART_CASECOMMS_PAYLOAD_OFFSET      (2)

/*! Test UART transport data. */
typedef struct
{
    /*! chargercomms stream sink for accessing UART chargercomms. */
    Sink cc_sink;
} transport_scheme_data_0;

/*! \brief Transmit a packet over a test UART transport.
    \param dest Case comms destination device ID.
    \param cid Case comms channel ID.
    \param mid Case comms message ID.
    \param data Pointer to the packet.
    \param len Length of data in bytes.
    \return bool TRUE packet accepted for transmission, FALSE otherwise.
*/
bool ccProtocol_TransTestUartTransmit(cc_dev_t dest, cc_cid_t cid, unsigned mid,
                                      uint8* data, uint16 len);

/*! Define this transport's transmit function as the common transmit symbol for cc_protocol.c */
#define ccProtocol_TransTransmit ccProtocol_TransTestUartTransmit

/*! \brief Handle receipt of a packet over a test UART transport. 
    \param ind Indication of incoming packet.
*/
void ccProtocol_TransTestUartReceive(Source src);

/*! \brief Initialise the test UART transport.
 * */
void ccProtocol_TransTestUartSetup(void);
/*! Define this transport's setup function as the common setup symbol for cc_protocol.c */
#define ccProtocol_TransSetup ccProtocol_TransTestUartSetup

/* Test UART transport does not require reset or polling, report that when queried. */
#define ccProtocol_TransRequiresPolling() (FALSE)
#define ccProtocol_TransRequiresReset() (FALSE)

/* Test UART transport cannot be enabled/disabled.
   Always report success for Enable or IsEnabled and failure for any attempt
   to disable. */
#define ccProtocol_TransIsEnabled() (TRUE)
#define ccProtocol_TransEnable()    (TRUE)
#define ccProtocol_TransDisable()   (FALSE)

#else

#define ccProtocol_TransTestUartTransmit(_dest, _cid, _mid, _data, _len)    (FALSE)
#define ccProtocol_TransTestUartReceive(_ind)                               (UNUSED(_ind))
#define ccProtocol_TransTestUartSetup()

#endif /* HAVE_CC_TRANS_TEST_UART */
#endif /* INCLUDE_CASE_COMMS */
#endif /* CC_PROTOCOL_TRANS_TEST_UART_H */
/*! @} End of group documentation */


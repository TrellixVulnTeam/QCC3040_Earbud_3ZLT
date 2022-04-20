/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    case_comms
\brief      Transmit and receive handling for a test UART transport.
*/
/*! \addtogroup case_comms
@{
*/

#ifdef INCLUDE_CASE_COMMS
#ifdef HAVE_CC_TRANS_TEST_UART

#include "cc_protocol.h"
#include "cc_protocol_private.h"
#include "cc_protocol_trans_test_uart.h"

#include <multidevice.h>

#include <logging.h>

#include <message.h>
#include <chargercomms.h>
#include <stdlib.h>
#include <stream.h>
#include <source.h>
#include <sink.h>
#include <pio.h>
#include <panic.h>

bool ccProtocol_TransTestUartTransmit(cc_dev_t dest, cc_cid_t cid, unsigned mid, 
                                      uint8* data, uint16 len)
{
    cc_protocol_t* td = CcProtocol_GetTaskData();
    uint8* snk = NULL;
    unsigned total_len = len + TEST_UART_CHARGERCOMMS_HEADER_LEN + CASECOMMS_HEADER_LEN;
    bool sent = FALSE;
    uint8 cc_header = 0;
    cc_dev_t src = CASECOMMS_DEVICE_CASE;

    UNUSED(dest);

    /* chargercomms lower layers insert the src device automatically, but for
     * the test UART it must be supplied explicitly */
    if (td->mode == CASECOMMS_MODE_EARBUD)
    {
        src = Multidevice_IsLeft() ? CASECOMMS_DEVICE_LEFT_EB : CASECOMMS_DEVICE_RIGHT_EB;
    }

    /* if there is space in the stream for the packet
     * claim the space
     * get pointer to the correct place in the stream to write */
    if (SinkSlack(td->scheme_data.cc_sink) >= total_len)
    {
        uint16 offset = SinkClaim(td->scheme_data.cc_sink, total_len);
        if (offset != 0xffff)
        {
            snk = (uint8*)PanicNull(SinkMap(td->scheme_data.cc_sink)) + offset;

            /* compose the packet */
            snk[TEST_UART_CHARGERCOMMS_HEADER_OFFSET] = src;
            ccProtocol_CaseCommsSetCID(&cc_header, cid);
            ccProtocol_CaseCommsSetMID(&cc_header, mid);
            snk[TEST_UART_CASECOMMS_HEADER_OFFSET] = cc_header;
            memcpy(&snk[TEST_UART_CASECOMMS_PAYLOAD_OFFSET], data, len);

            DEBUG_LOG_VERBOSE("ccProtocol_TransTestUartTransmit enum:cc_dev_t:%d enum:cc_cid_t:%d mid:%d len:%d", dest, cid, mid, total_len);
            
            /* flush the payload and the header */
            SinkFlush(td->scheme_data.cc_sink, total_len);

            sent = TRUE;
        }
    }

    return sent;
}

void ccProtocol_TransTestUartReceive(Source src)
{
    unsigned pkt_len = 0;
    cc_dev_t source_dev;

    PanicFalse(SourceIsValid(src));

    while ((pkt_len = SourceBoundary(src)) != 0)
    {
        const uint8* pkt = NULL;
        cc_cid_t cid = CASECOMMS_CID_INVALID;
        unsigned mid = 0;

        pkt = SourceMap(src);
        if (!pkt)
        {
            DEBUG_LOG_ERROR("ccProtocol_TransTestUartReceive len %d pkt %p", pkt_len, pkt);
            Panic();
        }

        source_dev = pkt[TEST_UART_CHARGERCOMMS_HEADER_OFFSET];

        cid = ccProtocol_CaseCommsGetCID(pkt[TEST_UART_CASECOMMS_HEADER_OFFSET]);
        mid = ccProtocol_CaseCommsGetMID(pkt[TEST_UART_CASECOMMS_HEADER_OFFSET]);

        DEBUG_LOG_VERBOSE("ccProtocol_TransTestUartReceive enum:cc_dev_t:%d enum:cc_cid_t:%d mid:%d len:%d", source_dev, cid, mid, pkt_len);

        /* pass packet to client, strip case comms header */
        ccProtocol_SendRXPacketToClient(pkt + TEST_UART_CHARGERCOMMS_HEADER_LEN + CASECOMMS_HEADER_LEN,
                                        pkt_len - TEST_UART_CHARGERCOMMS_HEADER_LEN - CASECOMMS_HEADER_LEN,
                                        cid, mid, source_dev);
        SourceDrop(src, pkt_len);
    }
}


void ccProtocol_TransTestUartSetup(void)
{
    cc_protocol_t* td = CcProtocol_GetTaskData();

    /* Configure PIOs for the uart */
    PioSetMapPins32Bank(TEST_UART_RTS_PIO/32, 1<<(TEST_UART_RTS_PIO%32), 0);
    PioSetFunction(TEST_UART_RTS_PIO, UART_RTS);
    PioSetMapPins32Bank(TEST_UART_CTS_PIO/32, 1<<(TEST_UART_CTS_PIO%32), 0);
    PioSetFunction(TEST_UART_CTS_PIO, UART_CTS);
    PioSetMapPins32Bank(TEST_UART_TX_PIO/32, 1<<(TEST_UART_TX_PIO%32), 0);
    PioSetFunction(TEST_UART_TX_PIO, UART_TX);
    PioSetMapPins32Bank(TEST_UART_RX_PIO/32, 1<<(TEST_UART_RX_PIO%32), 0);
    PioSetFunction(TEST_UART_RX_PIO, UART_RX);
    
    td->scheme_data.cc_sink = StreamUartSink();
    StreamUartConfigure(VM_UART_RATE_9K6, VM_UART_STOP_ONE, VM_UART_PARITY_NONE);
    
    /* ensure we get messages to the cc_protocol task */
    MessageStreamTaskFromSink(td->scheme_data.cc_sink, CcProtocol_GetTask());
    SourceConfigure(StreamSourceFromSink(td->scheme_data.cc_sink), VM_SOURCE_MESSAGES, VM_MESSAGES_ALL);
    SinkConfigure(td->scheme_data.cc_sink, VM_SINK_MESSAGES, VM_MESSAGES_ALL);
    
    /* check for data already in the stream source before we registered as the Task to
     * be informed */
    ccProtocol_ProcessStreamSource(StreamSourceFromSink(td->scheme_data.cc_sink));;
}

#endif /* HAVE_CC_TRANS_TEST_UART */
#endif /* INCLUDE_CASE_COMMS */
/*! @} End of group documentation */


/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    case_comms
\brief      Transmit and receive handling for Scheme B transport.
*/
/*! \addtogroup case_comms
@{
*/

#ifdef INCLUDE_CASE_COMMS
#ifdef HAVE_CC_TRANS_SCHEME_B

#include "cc_protocol.h"
#include "cc_protocol_private.h"
#include "cc_protocol_trans_schemeB.h"
#include "cc_protocol_trans_schemeB_hw.h"
#include "cc_protocol_config.h"

#include <logging.h>
#include <multidevice.h>
#include <pio_common.h>

#include <message.h>
#include <chargercomms.h>
#include <stdlib.h>
#include <stream.h>
#include <source.h>
#include <sink.h>
#include <panic.h>

bool ccProtocol_TransSchemeBTransmit(cc_dev_t dest, cc_cid_t cid, unsigned mid, 
                                     uint8* data, uint16 len)
{
    cc_protocol_t* td = CcProtocol_GetTaskData();
    uint8* snk = NULL;
    charger_comms_uart_tx_msg_header_t hdr;
    bool sent = FALSE;
    bool can_send = FALSE;

    hdr.dest_address = dest;

    /* no payload length and the CID is invalid, this is a poll message only
     * other this is a data message (and payload is optional) */
    if (len == 0 && cid == CASECOMMS_CID_INVALID)
    {
        hdr.type = CHARGER_COMMS_UART_MSG_TYPE_POLL;

        if (SinkSlack(td->scheme_data.cc_sink) != 0)
        {
            can_send = TRUE;
        }
    }
    else
    {
        /* data message always has casecomms header */
        hdr.type = CHARGER_COMMS_UART_MSG_TYPE_COMMS_DATA;
        ccProtocol_CaseCommsSetCID(&hdr.header, cid);
        ccProtocol_CaseCommsSetMID(&hdr.header, mid);

        /* if no payload to send, message construction complete, mark we can
           send and move on */
        if (len == 0)
        {
            can_send = TRUE;
        }
        else
        {
            /* there is payload to send, attmept to write it into the stream */
            if (SinkSlack(td->scheme_data.cc_sink) >= len)
            {
                uint16 offset = SinkClaim(td->scheme_data.cc_sink, len);
                if (offset != 0xffff)
                {
                    snk = (uint8*)PanicNull(SinkMap(td->scheme_data.cc_sink)) + offset;
                    memcpy(&snk[SCHEME_B_CASECOMMS_TX_PAYLOAD_OFFSET], data, len);
                    can_send = TRUE;
                }
            }
        }
    }

    if (can_send)
    {
        if (SinkFlushHeader(td->scheme_data.cc_sink, len, &hdr, sizeof(charger_comms_uart_tx_msg_header_t)))
        {
            DEBUG_LOG_V_VERBOSE("ccProtocol_TransSchemeBTransmit enum:charger_comms_uart_msg_type:%d enum:cc_dev_t:%d enum:cc_cid_t:%d mid:%d len:%d", hdr.type, dest, cid, mid, len);
            sent = TRUE;
        }
    }

    if (!sent)
    {
        DEBUG_LOG_WARN("ccProtocol_TransSchemeBTransmit TX FAILED enum:charger_comms_uart_msg_type:%d enum:cc_dev_t:%d enum:cc_cid_t:%d mid:%d len:%d", hdr.type, dest, cid, mid, len);
    }

    return sent;
}

void ccProtocol_TransSchemeBReceive(Source src)
{
    unsigned pkt_len = 0;

    PanicFalse(SourceIsValid(src));

    while (SourceSizeHeader(src) == sizeof(charger_comms_uart_rx_msg_header_t))
    {
        const uint8* pkt = NULL;
        cc_dev_t source_dev;
        cc_cid_t cid = CASECOMMS_CID_INVALID;
        unsigned mid = 0;
        const charger_comms_uart_rx_msg_header_t* hdr = (charger_comms_uart_rx_msg_header_t*)PanicNull((void*)SourceMapHeader(src));
        
        /* get source dev from the header */
        source_dev = hdr->src_address;

        /* access the packet and extract case comms header fields */
        pkt_len = SourceBoundary(src);
        pkt = SourceMap(src);
        if (!pkt_len || !pkt)
        {
            DEBUG_LOG_ERROR("ccProtocol_TransSchemeBReceive len %d pkt %p", pkt_len, pkt);
            Panic();
        }
        cid = ccProtocol_CaseCommsGetCID(pkt[SCHEME_B_CASECOMMS_RX_HEADER_OFFSET]);
        mid = ccProtocol_CaseCommsGetMID(pkt[SCHEME_B_CASECOMMS_RX_HEADER_OFFSET]);

        DEBUG_LOG_V_VERBOSE("ccProtocol_TransSchemeBReceive enum:cc_dev_t:%d enum:cc_cid_t:%d mid:%d len:%d", source_dev, cid, mid, pkt_len);

        /* pass packet to client, strip case comms header */
        ccProtocol_SendRXPacketToClient(pkt + CASECOMMS_HEADER_LEN,
                                        pkt_len - CASECOMMS_HEADER_LEN,
                                        cid, mid, source_dev);
        SourceDrop(src, pkt_len);
    }
}

/*! single-wire UART stream setup */
void ccProtocol_TransSchemeBSetup(void)
{
    cc_protocol_t* td = CcProtocol_GetTaskData();
    uint32 device_id = CHARGER_COMMS_UART_DEVICE_ID_CASE;

    /* setup PIO as the single wire for TX/RX */
    PioSetMapPins32Bank(PioCommonPioBank(CcProtocol_ConfigSchemeBTxRxPio()), 
                        PioCommonPioMask(CcProtocol_ConfigSchemeBTxRxPio()), 0);
    PioSetFunction(CcProtocol_ConfigSchemeBTxRxPio(), CHARGER_COMMS_UART_TX_RX);

    /* complete any hardware setup related to Scheme B */
    ccProtocol_TransSchemeBHwSetup();

    /* determine device ID if we're an earbud */
    if (td->mode == CASECOMMS_MODE_EARBUD)
    {
        device_id = Multidevice_IsLeft() ? CHARGER_COMMS_UART_DEVICE_ID_EB_L :
                                           CHARGER_COMMS_UART_DEVICE_ID_EB_R;
    }

    /* Configure chargercomms over the UART */
    PanicFalse(ChargerCommsUartConfigure(CHARGER_COMMS_UART_CFG_KEY_RX_ENABLE, 1));
    PanicFalse(ChargerCommsUartConfigure(CHARGER_COMMS_UART_CFG_KEY_DEVICE_ID, device_id));
    PanicFalse(ChargerCommsUartConfigure(CHARGER_COMMS_UART_CFG_KEY_TIME_OUT, 20));
    PanicFalse(ChargerCommsUartConfigure(CHARGER_COMMS_UART_CFG_KEY_BAUD_RATE, CcProtocol_ConfigSchemeBBaudRate()));
    PanicFalse(ChargerCommsUartConfigure(CHARGER_COMMS_UART_CFG_KEY_PARITY, VM_UART_PARITY_NONE));
    PanicFalse(ChargerCommsUartConfigure(CHARGER_COMMS_UART_CFG_KEY_STOP_BITS, VM_UART_STOP_ONE));

    /* ensure sink is NULL, which indicates not enabled */
    td->scheme_data.cc_sink = NULL;
}

bool ccProtocol_TransSchemeBEnable(void)
{
    cc_protocol_t* td = CcProtocol_GetTaskData();
    bool enabled = FALSE;

    /* return immediate success if already enabled */
    if (ccProtocol_TransSchemeBIsEnabled())
    {
        return TRUE;
    }

    /* Get the charger comms UART stream handle */
    td->scheme_data.cc_sink = StreamChargerCommsUartSink();

    if (td->scheme_data.cc_sink)
    {
        /* ensure we get messages to the cc_protocol task */
        MessageStreamTaskFromSink(td->scheme_data.cc_sink, CcProtocol_GetTask());
        SourceConfigure(StreamSourceFromSink(td->scheme_data.cc_sink), VM_SOURCE_MESSAGES, VM_MESSAGES_ALL);
        SinkConfigure(td->scheme_data.cc_sink, VM_SINK_MESSAGES, VM_MESSAGES_ALL);
    
        /* check for data already in the stream source before we registered as the Task to
         * be informed */
        ccProtocol_ProcessStreamSource(StreamSourceFromSink(td->scheme_data.cc_sink));

        enabled = TRUE;
    }

    return enabled;
}

bool ccProtocol_TransSchemeBDisable(void)
{
    cc_protocol_t* td = CcProtocol_GetTaskData();
    bool disabled = FALSE;
    
    /* return immediate success if already disabled */
    if (!ccProtocol_TransSchemeBIsEnabled())
    {
        return TRUE;
    }

    if (SinkClose(td->scheme_data.cc_sink))
    {
        disabled = TRUE;
        td->scheme_data.cc_sink = NULL;
    }

    return disabled;
}

bool ccProtocol_TransSchemeBIsEnabled(void)
{
    cc_protocol_t* td = CcProtocol_GetTaskData();
    return td->scheme_data.cc_sink != NULL;
}

#endif /* HAVE_CC_TRANS_SCHEME_B */
#endif /* INCLUDE_CASE_COMMS */
/*! @} End of group documentation */


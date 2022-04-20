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

#ifndef CC_PROTOCOL_TRANS_SCHEME_B_H
#define CC_PROTOCOL_TRANS_SCHEME_B_H

#ifdef INCLUDE_CASE_COMMS

#ifdef HAVE_CC_TRANS_SCHEME_B
#include "cc_protocol.h"
#include "cc_protocol_private.h"

#include <source.h>
#include <sink.h>

/*! Definitions related to the scheme B charger comms packet format. */
/*! @{ */
#define SCHEME_B_CASECOMMS_RX_HEADER_OFFSET          (0)
#define SCHEME_B_CASECOMMS_TX_PAYLOAD_OFFSET         (0)
/*! @} */

/*! Scheme B transport data. */
typedef struct
{
    /*! chargercomms stream sink for accessing UART chargercomms. */
    Sink cc_sink;
} transport_scheme_data;

/*! \brief Transmit a packet over a Scheme B transport.
    \param dest Case comms destination device ID.
    \param cid Case comms channel ID.
    \param mid Case comms message ID.
    \param data Pointer to the packet.
    \param len Length of data in bytes.
    \return bool TRUE packet accepted for transmission, FALSE otherwise.

    \note A return value of TRUE does not indicate receipt of the packet
          by the destination device, caller must wait for acknowledgement
          via CcProtocol_TxStatus_fn callback.
*/
bool ccProtocol_TransSchemeBTransmit(cc_dev_t dest, cc_cid_t cid, unsigned mid, 
                                       uint8* data, uint16 len);
/*! Define this transport's transmit function as the common transmit symbol for cc_protocol.c */
#define ccProtocol_TransTransmit ccProtocol_TransSchemeBTransmit

/*! \brief Handle receipt of a packet over a Scheme B transport. 
    \param ind Indication of incoming packet.
*/
void ccProtocol_TransSchemeBReceive(Source src);

/*! \brief Initialise the Scheme B transport.
*/
void ccProtocol_TransSchemeBSetup(void);
/*! Define this transport's setup function as the common setup symbol for cc_protocol.c */
#define ccProtocol_TransSetup ccProtocol_TransSchemeBSetup

bool ccProtocol_TransSchemeBEnable(void);
#define ccProtocol_TransEnable ccProtocol_TransSchemeBEnable
bool ccProtocol_TransSchemeBDisable(void);
#define ccProtocol_TransDisable ccProtocol_TransSchemeBDisable
bool ccProtocol_TransSchemeBIsEnabled(void);
#define ccProtocol_TransIsEnabled ccProtocol_TransSchemeBIsEnabled

/*! Report that Scheme B does require polling support from cc_protocol. */
#define ccProtocol_TransRequiresPolling() (TRUE)
/*! Report that Scheme B does require reset support from cc_protocol. */
#define ccProtocol_TransRequiresReset() (TRUE)

#else

#define ccProtocol_TransSchemeBTransmit(_dest, _cid, _mid, _data, _len) (FALSE)
#define ccProtocol_TransSchemeBReceive(_ind)                            (UNUSED(_ind))
#define ccProtocol_TransSchemeBSetup()
#define ccProtocol_TransSchemeBEnable()                                 (FALSE)
#define ccProtocol_TransSchemeBDisable()                                (FALSE)
#define ccProtocol_TransSchemeBIsEnabled()                              (FALSE)

#endif /* HAVE_CC_TRANS_SCHEME_B */
#endif /* INCLUDE_CASE_COMMS */
#endif /* CC_PROTOCOL_TRANS_SCHEME_B_H */
/*! @} End of group documentation */


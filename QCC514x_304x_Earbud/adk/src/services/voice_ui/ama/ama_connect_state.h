/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_connect_state.h
\brief  File consists of function decalration for Amazon Voice Service's connect state.
*/
#ifndef AMA_CONNECT_STATE_H
#define AMA_CONNECT_STATE_H

#include <bdaddr.h>
#include "ama_protocol.h"

/*! \brief Get the BT address for the active AMA connection.
    \return const bdaddr * pointer to the BT address.
*/
const bdaddr * Ama_GetBtAddress(void);

/*! \brief Inform the AMA component that Alexa setup is complete.
*/
void Ama_CompleteSetup(void);

/*! \brief Get the AMA transport over which data will be sent.
    \return ama_transport_t indicating the active transport.
*/
ama_transport_t Ama_GetActiveTransport(void);

/*! \brief Determine if an AMA protocol session is established.
    \return TRUE if AMA is connected, FALSE otherwise.
*/
bool Ama_IsConnected(void);

/*! \brief Determine if AMA is registered with the connected handset.
    \return TRUE if AMA is registered, FALSE otherwise.
*/
bool Ama_IsRegistered(void);

/*! \brief Inform the AMA component that the transport has changed.
    \param transport Identifies the transport in use
*/
void Ama_TransportSwitched(ama_transport_t transport);

/*! \brief Inform the AMA component that the transport has connected.
 *  \param ama_transport_t transport connected.
 *  \param const bdaddr * pointer to BT address of new connection.
*/
void Ama_TransportConnected(ama_transport_t transport, const bdaddr * bd_addr);

/*! \brief Inform the AMA component that the transport has disconnected.
    \param ama_trasport_t transport disconnected.
*/
void Ama_TransportDisconnected(ama_transport_t transport);

#endif // AMA_CONNECT_STATE_H

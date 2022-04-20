/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Protected pairing functions (used by peer pairing component)
 */
#ifndef PAIRING_PROTECTED_H_
#define PAIRING_PROTECTED_H_

#include "pairing.h"

/*! \brief Trigger pairing to send the PAIRING_COMPLETE message to clients.
    \note This is only expected to be used by the peer pair le component to trigger
    the completion and indication of the peer pairing procedure when fully complete,
    not at the point when the le pairing is complete.
*/
void Pairing_SendPairingCompleteMessageToClients(void);


#endif /* PAIRING_PROTECTED_H_ */

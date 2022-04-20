/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Provides TWS support in the accessory domain
*/


#ifndef AMA_TWS_H
#define AMA_TWS_H

#include "ama_rfcomm.h"

void AmaTws_HandleLocalDisconnectionCompleted(void);

#ifdef HOSTED_TEST_ENVIRONMENT
void Ama_tws_Reset(void);
#endif

void AmaTws_DisconnectIfRequired(ama_local_disconnect_reason_t reason);

bool AmaTws_IsDisconnectRequired(void);

#endif /* AMA_TWS_H */

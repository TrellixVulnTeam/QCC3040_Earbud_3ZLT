/*!
\copyright  Copyright (c) 2005 - 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       state_proxy_pairing.c
\brief      Setting flags associated with handset pairing.
*/

/* local includes */
#include "state_proxy.h"
#include "state_proxy_private.h"
#include "state_proxy_marshal_defs.h"
#include "state_proxy_pairing.h"
#include "state_proxy_flags.h"

/* framework includes */
#include <pairing.h>
#include <peer_signalling.h>
#include <bt_device.h>

/* system includes */
#include <panic.h>
#include <logging.h>
#include <stdlib.h>

void stateProxy_GetInitialPairingState(void)
{
    state_proxy_task_data_t *proxy = stateProxy_GetTaskData();
    bdaddr handset_addr;

    proxy->local_state->flags.is_pairing = FALSE;
    if (appDeviceGetHandsetBdAddr(&handset_addr))
    {
        proxy->local_state->flags.has_handset_pairing = TRUE;
    }
}

void stateProxy_HandlePairingHandsetActivity(const PAIRING_ACTIVITY_T* pha)
{
    DEBUG_LOG("stateProxy_HandlePairingHandsetActivity %u", pha->status);

    /* only interested in (Not)InProgress for notifying pairing
     * activity */
    if (pha->status == pairingActivityInProgress ||
        pha->status == pairingActivityNotInProgress)
    {
        bool is_pairing = pha->status == pairingActivityInProgress;
        
        stateProxy_FlagIndicationHandler(MARSHAL_TYPE_PAIRING_ACTIVITY_T,
                                         is_pairing, pha,
                                         sizeof(PAIRING_ACTIVITY_T));
    }

}

void stateProxy_HandleRemotePairingHandsetActivity(const PAIRING_ACTIVITY_T* pha)
{
    stateProxy_RemoteFlagIndicationHandler(MARSHAL_TYPE_PAIRING_ACTIVITY_T,
                                           pha->status == pairingActivityInProgress ? TRUE:FALSE,
                                           pha);
}

/****************************************************************************
Copyright (c) 2020 Qualcomm Technologies International, Ltd.


FILE NAME
    dm_read_afh_chan_map.h

DESCRIPTION
    This header is for the Read AFH Channel Map functionality.

NOTES

*/
#ifndef DM_READ_AFH_CHAN_MAP_H_
#define DM_READ_AFH_CHAN_MAP_H_

#include "connection_private.h"
#include "bdaddr.h"
#include <app/bluestack/dm_prim.h>

/****************************************************************************
NAME
    connectionHandleDmReadAfhChannelMapReq

DESCRIPTION
    Handle the internal message sent by
    ConnectionDmReadAfhChannelMapReq().

RETURNS
   void
*/
void connectionHandleDmReadAfhChannelMapReq(
        connectionDmReadAfhMapState *state,
        const CL_INTERNAL_DM_READ_AFH_CHANNEL_MAP_REQ_T *req
        );


/****************************************************************************
NAME
    connectionHandleDmReadAfhChannelMapCfm

DESCRIPTION
    Handle the DM_HCI_READ_AFH_CHANNEL_MAP_CFM from Bluestack, in response
    to the DM_HCI_READ_AFH_CHANNEL_MAP_REQ. Converts the PRIM to a CL CFM 
    message and sends it to the task that initiated the message scenario.

RETURNS
   void
*/
void connectionHandleDmReadAfhChannelMapCfm(
        connectionDmReadAfhMapState *state,
        const DM_HCI_READ_AFH_CHANNEL_MAP_CFM_T *cfm
        );

#endif /* DM_READ_AFH_CHAN_MAP_H_ */

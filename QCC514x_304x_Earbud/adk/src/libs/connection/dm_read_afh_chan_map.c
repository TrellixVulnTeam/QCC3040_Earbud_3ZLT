/****************************************************************************
Copyright (c) 2020 Qualcomm Technologies International, Ltd.


FILE NAME
    dm_read_afh_chan_map.c

DESCRIPTION
    This module is for the Read AFH Channel Map functionality.

NOTES

*/

#include "dm_read_afh_chan_map.h"
#include <vm.h>


/****************************************************************************
NAME
    ConnectionDmReadAfhChannelMapReq

DESCRIPTION
    Send an internal message so that this can be serialsed to Bluestack

RETURNS
   void
*/
void ConnectionDmReadAfhChannelMapReq(
        Task            theAppTask,
        const bdaddr    *bd_addr
        )
{
    if (!theAppTask)
    {
        CL_DEBUG(("'theAppTask' is NULL\n"));
    }
    else if (!bd_addr)
    {
        CL_DEBUG(("'bd_addr' is NULL\n"));
    }
    else
    {
        MAKE_CL_MESSAGE(CL_INTERNAL_DM_READ_AFH_CHANNEL_MAP_REQ);
        message->theAppTask = theAppTask;
        message->bd_addr = *bd_addr;
        MessageSend(
                connectionGetCmTask(),
                CL_INTERNAL_DM_READ_AFH_CHANNEL_MAP_REQ,
                message
                );
    }
}


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
        )
{
    if (!state->readAfhMapLock)
    {
        MAKE_PRIM_C(DM_HCI_READ_AFH_CHANNEL_MAP_REQ);

        /* This is not used in P1 and will be set by Bluestack. */
        prim->handle = 0;
        BdaddrConvertVmToBluestack(&prim->bd_addr, &req->bd_addr);
        VmSendDmPrim(prim);

        state->readAfhMapLock = req->theAppTask;
    }
    else /* There is already a Bluestack request scenario on-going. */
    {
        MAKE_CL_MESSAGE(CL_INTERNAL_DM_READ_AFH_CHANNEL_MAP_REQ);
        COPY_CL_MESSAGE(req, message);
        MessageSendConditionallyOnTask(
                connectionGetCmTask(),
                CL_INTERNAL_DM_READ_AFH_CHANNEL_MAP_REQ,
                message,
                &state->readAfhMapLock
                );
    }
}


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
        )
{
    if (state->readAfhMapLock)
    {
        MAKE_CL_MESSAGE(CL_DM_READ_AFH_CHANNEL_MAP_CFM);

        BdaddrConvertBluestackToVm(&message->bd_addr, &cfm->bd_addr);
        
        message->status = (cfm->status) ? fail : success;
        message->mode = cfm->mode;
        memmove(message->map, cfm->map, (10 * sizeof(uint8)));

        MessageSend(
                state->readAfhMapLock,
                CL_DM_READ_AFH_CHANNEL_MAP_CFM,
                message
                );

        state->readAfhMapLock = NULL;
    }
    else
    {
        CL_DEBUG(("DM_HCI_READ_AFH_CHANNEL_MAP_CFM received without lock\n"));
    }
}


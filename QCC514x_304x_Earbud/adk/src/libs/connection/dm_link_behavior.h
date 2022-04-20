/****************************************************************************
Copyright (c) 2020 Qualcomm Technologies International, Ltd.


FILE NAME
    dm_link_behavior.h

DESCRIPTION
    This header is for DM Link Behavior functionality.

NOTES

*/
#ifndef DM_LINK_BEHAVIOR_H
#define DM_LINK_BEHAVIOR_H

#include "connection_private.h"
#include "bdaddr.h"

#include <app/bluestack/dm_prim.h>

/****************************************************************************
NAME
    connectionHandleDmSetLinkBehaviorReq

DESCRIPTION
    Handle the internal message sent by ConnectionDmSetLinkBehaviorReq().

    If no other general Bluestack message scenario is on-going, then send the
    Bluestack prim immediately. Otherwise queue the message until the currently
    on-going scenario is complete.

RETURNS
   void
*/
void connectionHandleDmSetLinkBehaviorReq(
        connectionGeneralLockState *state,
        const CL_INTERNAL_DM_SET_LINK_BEHAVIOR_REQ_T *req
        );


/****************************************************************************
NAME
    connectionHandleDmSetLinkBehaviorCfm

DESCRIPTION

    Handle DM_SET_LINK_BEHAVIOR_CFM from Bluestack.  Converts the PRIM to a CL
    CFM message and sends it to the task that initiated the message scenario.

RETURNS
   void
*/
void connectionHandleDmSetLinkBehaviorCfm(
        connectionGeneralLockState *state,
        const DM_SET_LINK_BEHAVIOR_CFM_T *cfm
        );

#endif /* DM_LINK_BEHAVIOR_H */



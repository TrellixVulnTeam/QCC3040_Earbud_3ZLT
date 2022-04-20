/****************************************************************************
Copyright (c) 2020 Qualcomm Technologies International, Ltd.


FILE NAME
    dm_link_behavior.c

DESCRIPTION
    This module is for DM Link Behavior functionality.

NOTES

*/
#include "dm_link_behavior.h"
#include <vm.h>

/****************************************************************************
NAME
    ConnectionDmSetLinkBehaviorReq

DESCRIPTION
    Send an internal message so that this can be serialised to Bluestack.

RETURNS
   void
*/
void ConnectionDmSetLinkBehaviorReq(
            Task                    theAppTask,
            const typed_bdaddr*     taddr,
            bool                    l2cap_retry
            )
{
    MAKE_CL_MESSAGE(CL_INTERNAL_DM_SET_LINK_BEHAVIOR_REQ);

    message->theAppTask = theAppTask;
    message->taddr = *taddr;
    message->l2cap_retry = l2cap_retry;

    MessageSend(
            connectionGetCmTask(),
            CL_INTERNAL_DM_SET_LINK_BEHAVIOR_REQ,
            message
            );
}


/****************************************************************************
NAME
    connectionHandleDmSetLinkBehaviorReq

DESCRIPTION
    Handle the internal message sent by ConnectionDmSetLinkBehaviorReq().

    If no other general Bluestack message scenario is on-going, then send the
    Bluestack prim immediately. Otherwise queue the message until the currently
    on-going scenario is complete.

    DM_SET_LINK_BEHAVIOR_REQ uses a Configuration Table (conftab), similar to
    the way L2CAP Connection parameters are defined. However, there is only
    one option currently, so this function just hard codes the conftab
    structure.

RETURNS
   void
*/
void connectionHandleDmSetLinkBehaviorReq(
        connectionGeneralLockState *state,
        const CL_INTERNAL_DM_SET_LINK_BEHAVIOR_REQ_T *req
        )
{
    if (!state->taskLock)
    {
        MAKE_PRIM_T(DM_SET_LINK_BEHAVIOR_REQ);
        uint16 conftab_default[] = {
            0x8000,                             /* 0 Separator */
            DM_LINK_BEHAVIOR_L2CAP_RETRY,       /* 1 Key */
            DM_LINK_BEHAVIOR_L2CAP_RETRY_ON,    /* 2 Value */
            0xFF00                              /* 3 Terminator */
            };
        size_t conftab_size = sizeof(conftab_default);
        uint16 *conftab = (uint16 *)PanicUnlessMalloc(conftab_size);

        state->taskLock = req->theAppTask;

        BdaddrConvertTypedVmToBluestack(&prim->addrt, &req->taddr);

        /* L2CAP Retry is to be turned on by default. Set to off if
         * l2cap_retry is FALSE.
         */
        if (!req->l2cap_retry)
            conftab_default[2] = DM_LINK_BEHAVIOR_L2CAP_RETRY_OFF;

        prim->conftab_length = (uint16_t)(conftab_size/sizeof(uint16_t));
        /* Copy the conftab data to allocated memory and convert to a handle. */
        memcpy(conftab, conftab_default, conftab_size);
        prim->conftab = VmGetHandleFromPointer((void *)conftab);

        VmSendDmPrim(prim);
    }
    else /* There is a general scenario already on-going. */
    {
        MAKE_CL_MESSAGE(CL_INTERNAL_DM_SET_LINK_BEHAVIOR_REQ);
        COPY_CL_MESSAGE(req, message);

        MessageSendConditionallyOnTask(
                    connectionGetCmTask(),
                    CL_INTERNAL_DM_SET_LINK_BEHAVIOR_REQ,
                    message,
                    &state->taskLock
                    );
    }
}


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
        )
{
    if (state->taskLock)
    {
        MAKE_CL_MESSAGE(CL_DM_SET_LINK_BEHAVIOR_CFM);

        BdaddrConvertTypedBluestackToVm(&message->taddr, &cfm->addrt);

        /* Convert the DM_SET_LINK_BEHAVIOR_* status to connection library
         * status of just success or fail.
         */
        message->status = (cfm->status == DM_SET_LINK_BEHAVIOR_SUCCESS) ? success : fail;

        MessageSend(
                state->taskLock,
                CL_DM_SET_LINK_BEHAVIOR_CFM,
                message
                );
        /* Now that the CFM message has been sent, unset the serialisation
         * lock.
         */
        state->taskLock = NULL;
    }
    else
    {
        CL_DEBUG(("DM_SET_LINK_BEHAVIOR_CFM received without lock\n"));
    }
}



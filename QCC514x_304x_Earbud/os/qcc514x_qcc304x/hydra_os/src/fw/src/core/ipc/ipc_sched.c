/* Copyright (c) 2016 Qualcomm Technologies International, Ltd. */
/*   %%version */

#include "ipc/ipc_private.h"
#include "sched/sched.h"

void ipc_send_sched(qid remote_qid, uint16 mi, void *mv)
{
    IPC_SCHED_MSG_PRIM ipc_prim;
    ipc_prim.qid = remote_qid;
    ipc_prim.mi = mi;
    ipc_prim.mv = mv;
    ipc_send(IPC_SIGNAL_ID_SCHED_MSG_PRIM, &ipc_prim, sizeof(IPC_SCHED_MSG_PRIM));
}

void ipc_sched_handler(IPC_SIGNAL_ID id, const void *msg)
{
    const IPC_SCHED_MSG_PRIM *prim = (const IPC_SCHED_MSG_PRIM *)msg;
    assert(id == IPC_SIGNAL_ID_SCHED_MSG_PRIM);
    (void)put_message(prim->qid, prim->mi, prim->mv);
}

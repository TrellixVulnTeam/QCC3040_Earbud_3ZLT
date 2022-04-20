/* Copyright (c) 2016 Qualcomm Technologies International, Ltd. */
/*   %%version */

#include "ipc/ipc_private.h"

IPC_DATA ipc_data;
PANIC_DATA *panic_data;

#define LOG_PREFIX "IPC: "


void ipc_init(void)
{
    const uint32 *from_p0;

    /* We do a raw read from the known handle in order to get the pointer to the
     * IPC_BUFFER in shared memory */
    from_p0 = (const uint32 *) mmu_read_port_map_8bit(MMU_INDEX_RESERVED_IPC, 0);

#if IPC_PROTOCOL_ID
    if(IPC_SIGNATURE != *from_p0)
    {
        L0_DBG_MSG2(LOG_PREFIX "Bad signature, expected:0x%08x actual:0x%08x",
                    IPC_SIGNATURE, *from_p0);
        panic_diatribe(PANIC_IPC_BAD_SIGNATURE, *from_p0);
    }
    else
    {
        L4_DBG_MSG1(LOG_PREFIX "Signatures match (0x%08x)", *from_p0);
    }

    ++from_p0;

    if(IPC_PROTOCOL_ID != *from_p0)
    {
        L0_DBG_MSG2(LOG_PREFIX
                    "Incompatible protocol, expected:0x%08x actual:0x%08x",
                    IPC_PROTOCOL_ID, *from_p0);
        panic_diatribe(PANIC_IPC_PROTOCOL_INCOMPATIBILITY, *from_p0);
    }
    else
    {
        L4_DBG_MSG1(LOG_PREFIX "Protocols match (%u)", *from_p0);
    }

    ++from_p0;

    ipc_data.recv = (IPC_BUFFER *)*from_p0;

    /* The signature and protocol messages can only be freed once we have the
       receive buffer structure. */
    ipc_buffer_update_back(ipc_data.recv, sizeof(IPC_BUFFER *));
    ipc_buffer_update_back(ipc_data.recv, sizeof(IPC_BUFFER *));
#else
    ipc_data.recv = (IPC_BUFFER *)*from_p0;
#endif

    ipc_buffer_update_back(ipc_data.recv, sizeof(IPC_BUFFER *));

    IPC_RECV_POINTER(ipc_data.send, IPC_BUFFER);
    /* Need to wait until we know about ipc_data.send before we trigger an IPC
     * message by trying to free data from ipc_data.recv */
    ipc_buffer_update_tail_free(ipc_data.recv);

#ifdef CHIP_DEF_P1_SQIF_SHALLOW_SLEEP_WA_B_195036
    IPC_RECV_VALUE(ipc_data.p1_pm_flash_offset_from_p0);
#endif  /* CHIP_DEF_P1_SQIF_SHALLOW_SLEEP_WA_B_195036 */

    IPC_RECV_POINTER(panic_data, PANIC_DATA);
    assert(panic_data);

#if IPC_PROTOCOL_ID == 2
    ipc_recv_task_create();
#endif

    configure_interrupt(INT_SOURCE_INTERPROC_EVENT_1,
                        INT_LEVEL_FG,
                        ipc_interrupt_handler);

    configure_interrupt(INT_SOURCE_INTERPROC_EVENT_2,
                        INT_LEVEL_EXCEPTION,
                        panic_interrupt_handler);

    ipc_recv_messages_sent_before_init();

#if defined(FW_IPC_UNIT_TEST) && defined(ENABLE_APPCMD_TEST_ID_IPC)
    ipc_test_init();
#endif
}

#ifdef CHIP_DEF_P1_SQIF_SHALLOW_SLEEP_WA_B_195036


uint32 ipc_get_p1_flash_offset(void)
{
    return ipc_data.p1_pm_flash_offset_from_p0;
}

#endif /* CHIP_DEF_P1_SQIF_SHALLOW_SLEEP_WA_B_195036 */

void ipc_recv_buffer_mapping_policy_init(void)
{
    /* Nothing to do here on P1. P0 will send the
     * IPC_LEAVE_RECV_BUFFER_PAGES_MAPPED signal if the key is set. */
}

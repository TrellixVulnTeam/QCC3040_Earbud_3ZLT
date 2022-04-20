/* Copyright (c) 2017 Qualcomm Technologies International, Ltd. */
/*   %%version */
/**
 * \file
 */

#include "ipc/ipc_private.h"

#if defined(DESKTOP_TEST_BUILD) && TRAPSET_SD_MMC
void ipc_sd_mmc_handler(IPC_SIGNAL_ID id, const void *msg)
{
    switch(id)
    {
    case IPC_SIGNAL_ID_SD_MMC_SLOT_INIT_REQ:
    {
        const IPC_SD_MMC_SLOT_INIT *prim = (const IPC_SD_MMC_SLOT_INIT *)msg;
        vm_trap_SdMmcSlotInit(prim->init);
    }
        break;

    case IPC_SIGNAL_ID_SD_MMC_READ_DATA_REQ:
    {
        const IPC_SD_MMC_READ_DATA *prim = (const IPC_SD_MMC_READ_DATA *)msg;
        vm_trap_SdMmcReadData(prim->slot, prim->buff,
                              prim->start_block, prim->blocks_count);
    }
        break;

    default:
        L1_DBG_MSG1("ipc_sd_mmc_handler: unexpected signal ID 0x%x rxed", id);
        assert(FALSE);
        break;
    }
}
#endif /* (defined(PROCESSOR_P0) || defined(DESKTOP_TEST_BUILD)) && TRAPSET_SD_MMC */

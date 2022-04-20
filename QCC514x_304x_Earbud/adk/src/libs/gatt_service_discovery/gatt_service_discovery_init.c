/******************************************************************************
 Copyright (c) 2020 Qualcomm Technologies International, Ltd.
 All Rights Reserved.
 Qualcomm Technologies International, Ltd. Confidential and Proprietary.

 REVISION:      $Revision: #2 $
******************************************************************************/

#include <stdlib.h>

#include "gatt_service_discovery_init.h"
#include "gatt_service_discovery_handler.h"

#ifdef SYNERGY_GATT_SD_ENABLE
#include "csr_bt_gatt_lib.h"
#endif


/* GATT SD Instance */
static GGSD *mainInst = NULL;


bool gattServiceDiscoveryIsInit(void)
{
    return NULL != mainInst;
}


GGSD* gattServiceDiscoveryGetInstance(void)
{
    return mainInst;
}


#ifdef SYNERGY_GATT_SD_ENABLE

void GattServiceDiscoveryInit(void **gash)
{
    mainInst             = (GGSD *) GATT_SD_MALLOC(sizeof(GGSD));
    *gash                = (GGSD *) mainInst;

    memset(mainInst, 0, sizeof(GGSD));
    mainInst->app_task = APP_TASK_INVALID;

    CsrBtGattRegisterReqSend(CSR_BT_GATT_SRVC_DISC_IFACEQUEUE, 0);
}


void GattServiceDiscoveryDeinit(void **gash)
{
    uint16    msg_type;
    void*     msg;
    GGSD     *gatt_sd;

    gatt_sd    = (GGSD *) (*gash);

    while (CsrSchedMessageGet(&msg_type, &msg))
    {
        switch (msg_type)
        {
            case CSR_BT_GATT_PRIM:
            case GATT_SRVC_DISC_PRIM:
                break;
        }
        free(msg);
    }

    free(gatt_sd);
}

#else

bool GattServiceDiscoveryInit(Task appTask)
{
    if (appTask == NULL)
    {
        GATT_SD_PANIC(("Application Task NULL\n"));
    }
    else if (mainInst == NULL)
    {
        mainInst = (GGSD *) GATT_SD_MALLOC(sizeof(GGSD));

        if (mainInst)
        {
            /* reset GATT SD library */
            memset(mainInst, 0, sizeof(GGSD));

            mainInst->lib_task.handler = gattServiceDiscoveryMsgHandler;

            /* Store the Task function parameter.
             * All library messages need to be sent here */
            mainInst->app_task = appTask;
            return TRUE;
        }
    }
    return FALSE;
}


void GattServiceDiscoveryDeinit(void)
{
    /* Free GATT Service Discovery device list */
    GATT_SD_DL_CLEANUP(mainInst->deviceList);
    /* free main instance */
    free(mainInst);
}


#endif

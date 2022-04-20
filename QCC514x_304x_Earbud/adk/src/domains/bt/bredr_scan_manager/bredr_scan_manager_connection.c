/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\defgroup   bredr_scan_manager
\brief	    BREDR scan manager interface to connection library.
*/

#include "bredr_scan_manager_private.h"
#include "connection_abstraction.h"

void bredrScanManager_ConnectionWriteScanEnable(void)
{
    hci_scan_enable enable = hci_scan_enable_off;
    bsm_scan_context_t *page_scan = bredrScanManager_PageScanContext();
    bsm_scan_context_t *inquiry_scan = bredrScanManager_InquiryScanContext();
    bool page = FALSE;
    bool inquiry = FALSE;

    if (page_scan->state & (BSM_SCAN_ENABLED | BSM_SCAN_ENABLING))
    {
        page = TRUE;
    }
    if (inquiry_scan->state & (BSM_SCAN_ENABLED | BSM_SCAN_ENABLING))
    {
        inquiry = TRUE;
    }

    if (page)
    {
        enable = inquiry ? hci_scan_enable_inq_and_page : hci_scan_enable_page;
    }
    else
    {
        enable = inquiry ? hci_scan_enable_inq : hci_scan_enable_off;
    }

    DEBUG_LOG("bredrScanManager_ConnectionWriteScanEnable enable %d", enable);

    ConnectionWriteScanEnable(enable);
}

void bredrScanManager_ConnectionWriteScanActivity(bsm_scan_context_t *context)
{
    bredr_scan_manager_scan_parameters_t *params = &context->scan_params;

    if (context == bredrScanManager_PageScanContext())
    {
        ConnectionWritePageScanType(params->scan_type);
        ConnectionWritePagescanActivity(params->interval, params->window);
    }
    else if (context == bredrScanManager_InquiryScanContext())
    {
        ConnectionWriteInquiryscanActivity(params->interval, params->window);
    }
    else
    {
        /* Unknown context */
        Panic();
    }
}

void bredrScanManager_ConnectionHandleClDmWriteScanEnableCfm(bool status, uint16 outstanding)
{
    PanicFalse(status == TRUE);
    /* Zero indicates all outstanding scan enable requests have been processed
           and the state in the controller is now at the last requested state */
    if(outstanding == 0)
    {
        bredrScanManager_InstanceCompleteTransition(bredrScanManager_PageScanContext());
        bredrScanManager_InstanceCompleteTransition(bredrScanManager_InquiryScanContext());

        if (bredrScanManager_IsDisabled() && BredrScanManager_IsScanDisabled())
        {
            bredrScanManager_SendDisableCfm(TRUE);
        }
    }
}

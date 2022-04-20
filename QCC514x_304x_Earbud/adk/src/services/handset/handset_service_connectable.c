/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Handset service connectable
*/
#include "handset_service_connectable.h"
#include "handset_service_protected.h"

#include <bredr_scan_manager.h>

static bool allow_bredr;
static bool enabled_bredr;
static bool observing_connections;

void handsetService_ConnectableInit(void)
{
    allow_bredr = FALSE;
    enabled_bredr = FALSE;
    observing_connections = FALSE;
}

void handsetService_ObserveConnections(void)
{
    if(!observing_connections)
    {
        ConManagerRegisterTpConnectionsObserver(cm_transport_all, HandsetService_GetTask());
        observing_connections = TRUE;
    }
}

void handsetService_ConnectableEnableBredr(bool enable)
{
    if(enable)
    {
        if(!enabled_bredr && allow_bredr)
        {
            handsetService_ObserveConnections();
            BredrScanManager_PageScanRequest(HandsetService_GetTask(), SCAN_MAN_PARAMS_TYPE_SLOW);
            enabled_bredr = TRUE;
        }
    }
    else
    {
        if(enabled_bredr)
        {
            BredrScanManager_PageScanRelease(HandsetService_GetTask());
            enabled_bredr = FALSE;
        }
    }
}

void handsetService_ConnectableAllowBredr(bool allow)
{
    allow_bredr = allow;
}
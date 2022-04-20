/****************************************************************************
Copyright (c) 2011 - 2020 Qualcomm Technologies International, Ltd.


FILE NAME
    dm_ble_scanning.c

DESCRIPTION
    This file contains the implementation of Low Energy scan configuration.

NOTES

*/

#include "connection.h"
#include "connection_private.h"
#include "dm_ble_scanning.h"

#include <vm.h>
#include <bdaddr.h>
#include <stream.h>
#include <source.h>
#include <logging.h>

#ifndef DISABLE_BLE

#if HYDRACORE
#define NO_CFM_MESSAGE  ((Task)0x0FFFFFFF)
#else
#define NO_CFM_MESSAGE  ((Task)0x0000FFFF)
#endif

/****************************************************************************
NAME
    ConnectionDmBleSetScanEnable

DESCRIPTION
    Enables or disables BLE Scanning. The CFM is not passed on.

RETURNS
    void
*/

void ConnectionDmBleSetScanEnable(bool enable)
{
    ConnectionDmBleSetScanEnableReq(NO_CFM_MESSAGE, enable);
}

/****************************************************************************
NAME
    ConnectionDmBleSetScanEnableReq

DESCRIPTION
    Enables or disables BLE Scanning. If theAppTask is anything other than NULL
    (0) then that is treated as the task to return the CFM message to.

RETURNS
    void
*/

void ConnectionDmBleSetScanEnableReq(Task theAppTask, bool enable)
{
    MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_SET_SCAN_ENABLE_REQ);
    message->theAppTask = theAppTask;
    message->enable = enable;
    MessageSend(
                connectionGetCmTask(),
                CL_INTERNAL_DM_BLE_SET_SCAN_ENABLE_REQ,
                message
                );
}

/****************************************************************************
NAME
    connectionHandleDmBleSetScanEnableReq

DESCRIPTION
    This function will initiate a Scan Enable request.

RETURNS
    void
*/
void connectionHandleDmBleSetScanEnableReq(
                                connectionBleScanAdState *state,
                                const CL_INTERNAL_DM_BLE_SET_SCAN_ENABLE_REQ_T *req)
{
    /* Check the state of the task lock before doing anything. */
    if (!state->bleScanAdLock)
    {
        MAKE_PRIM_C(DM_HCI_ULP_SET_SCAN_ENABLE_REQ);

        /* One request at a time, set the scan lock. */
        state->bleScanAdLock = req->theAppTask;

        prim->scan_enable = (req->enable) ? 1 : 0;
        prim->filter_duplicates = 1;;
        VmSendDmPrim(prim);
    }
    else
    {
        /* Scan or Ad request already outstanding, queue up the request. */
        MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_SET_SCAN_ENABLE_REQ);
        COPY_CL_MESSAGE(req, message);
        MessageSendConditionallyOnTask(
                    connectionGetCmTask(),
                    CL_INTERNAL_DM_BLE_SET_SCAN_ENABLE_REQ,
                    message, &state->bleScanAdLock
                    );
    }
}


/****************************************************************************
NAME
    connectionHandleDmBleSetScanEnableCfm

DESCRIPTION
    Handle the DM_HCI_ULP_SET_SCAN_ENABLE_CFM from Bluestack.

RETURNS
    void
*/
void connectionHandleDmBleSetScanEnableCfm(
                         connectionBleScanAdState *state,
                         const DM_HCI_ULP_SET_SCAN_ENABLE_CFM_T* cfm
                         )
{
    if (state->bleScanAdLock != NO_CFM_MESSAGE)
    {
        MAKE_CL_MESSAGE(CL_DM_BLE_SET_SCAN_ENABLE_CFM);
        message->status = connectionConvertHciStatus(cfm->status);
        MessageSend(
                    state->bleScanAdLock,
                    CL_DM_BLE_SET_SCAN_ENABLE_CFM,
                    message
                    );
    }

    /* Reset the scan lock */
    state->bleScanAdLock = NULL;
}

/****************************************************************************
NAME
    ConnectionBleAddAdvertisingReportFilter

DESCRIPTION
    Set a filter for advertising reports so that only those that match the
    filter are reported to the VM. Always an OR operation when adding a filter.

RETURNS
    TRUE if the filter is added, otherwise FALSE if it failed or there was not
    enough memory to add a new filter.
*/
bool ConnectionBleAddAdvertisingReportFilter(
    ble_ad_type     ad_type,
    uint16          interval,
    uint16          size_pattern,
    const uint8*    pattern
    )
{
#ifdef CONNECTION_DEBUG_LIB
    /* Check parameters. */
    if (interval > BLE_AD_PDU_SIZE)
    {
        CL_DEBUG(("Interval greater than ad data length\n"));
    }
    if (size_pattern == 0 || size_pattern > BLE_AD_PDU_SIZE)
    {
        CL_DEBUG(("Pattern length is zero\n"));
    }
    if (pattern == 0)
    {
        CL_DEBUG(("Pattern is null\n"));
    }
#endif
    {
        /* Copy the data to a memory slot, which will be freed after
         * the function call.
         *
         * Data is uint8* but trap takes uint16 on xap
         */
        uint8 *pattern2 = (uint8*) PanicUnlessMalloc(size_pattern);
        memmove(pattern2, pattern, size_pattern);

        return VmAddAdvertisingReportFilter(
                    0,                          /* Operation is always OR */
                    ad_type,
                    interval,
                    size_pattern,
#ifdef HYDRACORE
                    pattern2
#else
                    (uint16)pattern2
#endif
                    );
    }
}

/****************************************************************************
NAME
    ConnectionBleClearAdvertisingReportFilter

DESCRIPTION
    Clear any existing filters.

RETURNS
    TRUE if the filters were cleared.
*/
bool ConnectionBleClearAdvertisingReportFilter(void)
{
    return VmClearAdvertisingReportFilter();
}

/****************************************************************************
NAME
    ConnectionDmBleSetScanParametersReq

DESCRIPTION
    Set up parameters to be used for BLE scanning.

RETURNS
    None.
*/
void ConnectionDmBleSetScanParametersReq(
        bool    enable_active_scanning,
        uint8   own_address,
        bool    white_list_only,
        uint16  scan_interval,
        uint16  scan_window
        )
{
#ifdef CONNECTION_DEBUG_LIB
    /* Check parameters. */
    if (scan_interval < 0x0004 || scan_interval > 0x4000  )
    {
        CL_DEBUG(("scan_interval outside range 0x0004..0x4000\n"));
    }
    if (scan_window < 0x0004 || scan_window > 0x4000  )
    {
        CL_DEBUG(("scan_window outside range 0x0004..0x4000\n"));
    }
    if (scan_window > scan_interval)
    {
        CL_DEBUG(("scan_window must be less than or equal to scan interval\n"));
    }
#endif
    {
        MAKE_PRIM_C(DM_HCI_ULP_SET_SCAN_PARAMETERS_REQ);

        prim->scan_type                 = (enable_active_scanning) ? 1 : 0;
        prim->scan_interval             = scan_interval;
        prim->scan_window               = scan_window;
        prim->own_address_type          = connectionConvertOwnAddress(own_address);
        prim->scanning_filter_policy    = (white_list_only) ? 1: 0;

        VmSendDmPrim(prim);
    }
}

/****************************************************************************
NAME
    ConnectionDmBleSetScanResponseDataReq

DESCRIPTION
    Sets BLE Scan Response data (0..31 octets).

RETURNS
   void
*/
void ConnectionDmBleSetScanResponseDataReq(uint8 size_sr_data, const uint8 *sr_data)
{

#ifdef CONNECTION_DEBUG_LIB
        /* Check parameters. */
    if (size_sr_data == 0 || size_sr_data > BLE_SR_PDU_SIZE)
    {
        CL_DEBUG(("Data length is zero\n"));
    }
    if (sr_data == 0)
    {
        CL_DEBUG(("Data is null\n"));
    }
#endif
    {
        MAKE_PRIM_C(DM_HCI_ULP_SET_SCAN_RESPONSE_DATA_REQ);
        prim->scan_response_data_len = size_sr_data;
        memmove(prim->scan_response_data, sr_data, size_sr_data);
        VmSendDmPrim(prim);
    }
}

/*****************************************************************************
 *                  Extended Scanning functions                              *
 *****************************************************************************/
#ifndef CL_EXCLUDE_ISOC

#define AD_STRUCT_FLAGS_PRESENT (1 << 7)

/* The following static globals were created to store associations between
 * application tasks and the scanners (extended or periodic) they have registered
 * interest in, in order to be able to correctly route received advertising
 * reports.
 *
 * They were not added to the connectionState library-wide struct as their
 * scope is limited to this file only, and are not meant to be accessed by other
 * library files, or the application itself. */
static task_scan_handles_pair task_scan_handles[MAX_SCAN_HANDLES];
static uint8 task_scan_handle_index = 0;

static task_scan_handles_pair task_scan_train_handles[MAX_TRAIN_SCAN_HANDLES];
static uint8 task_scan_train_handle_index = 0;

static task_sync_handles_pair task_sync_handles[MAX_SYNC_HANDLES];
static uint8 task_sync_handle_index = 0;

/* This static global holds the source reference for the Extended Scanning
 * stream, which is used to distinguish between the different streams when a
 * MESSAGE_MORE_DATA is received. */
static Source ExtScanSrc = 0;

/****************************************************************************
NAME
    ConnectionDmBleExtScanEnableReq

DESCRIPTION
    Enables or disables BLE Extended Scanning. If theAppTask is anything other
    than NULL (0) then that is treated as the task to return the CFM message
    to.

RETURNS
    void
*/

void ConnectionDmBleExtScanEnableReq(Task theAppTask, bool enable, uint8 num_of_scanners, uint8 scan_handle[], uint16 duration[])
{
    uint8 i;

    MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_EXT_SCAN_ENABLE_REQ);
    message->theAppTask = theAppTask;
    message->enable = enable;
    message->num_of_scanners = num_of_scanners;

    for (i = 0; i < num_of_scanners; i++)
    {
        message->scan_handle[i] = scan_handle[i];
        message->duration[i]    = duration[i];
    }

    MessageSend(
                connectionGetCmTask(),
                CL_INTERNAL_DM_BLE_EXT_SCAN_ENABLE_REQ,
                message
                );
}

/****************************************************************************
NAME
    connectionHandleDmBleSetScanEnableReq

DESCRIPTION
    This function will initiate a Scan Enable request.

RETURNS
    void
*/
void connectionHandleDmBleExtScanEnableReq(
                                connectionDmExtScanState *state,
                                const CL_INTERNAL_DM_BLE_EXT_SCAN_ENABLE_REQ_T *req)
{
    /* Check the state of the task lock before doing anything. */
    if (!state->dmExtScanLock)
    {
        uint8 i;

        MAKE_PRIM_T(DM_ULP_EXT_SCAN_ENABLE_SCANNERS_REQ);

        /* One request at a time, set the scan lock. */
        state->dmExtScanLock = req->theAppTask;

        prim->enable = (req->enable) ? 1 : 0;
        prim->num_of_scanners = req->num_of_scanners;

        for (i = 0; i < req->num_of_scanners; i++)
        {
            prim->scanners[i].scan_handle   = req->scan_handle[i];
            prim->scanners[i].duration      = req->duration[i];
        }

        VmSendDmPrim(prim);
    }
    else
    {
        /* Scan or Ad request already outstanding, queue up the request. */
        MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_EXT_SCAN_ENABLE_REQ);
        COPY_CL_MESSAGE(req, message);
        MessageSendConditionallyOnTask(
                    connectionGetCmTask(),
                    CL_INTERNAL_DM_BLE_EXT_SCAN_ENABLE_REQ,
                    message, &state->dmExtScanLock
                    );
    }
}

/****************************************************************************
NAME
    connectionHandleDmBleExtScanEnableCfm

DESCRIPTION
    Handle the DM_HCI_ULP_EXT_SCAN_ENABLE_CFM from Bluestack.

RETURNS
    void
*/
void connectionHandleDmBleExtScanEnableCfm(connectionDmExtScanState *state,
                         const DM_ULP_EXT_SCAN_ENABLE_SCANNERS_CFM_T *cfm
                         )
{
    if (state->dmExtScanLock != NO_CFM_MESSAGE)
    {
        MAKE_CL_MESSAGE(CL_DM_BLE_EXT_SCAN_ENABLE_CFM);
        message->status = (cfm->status == HCI_SUCCESS) ? success : fail;
        MessageSend(
                    state->dmExtScanLock,
                    CL_DM_BLE_EXT_SCAN_ENABLE_CFM,
                    message
                    );
    }

    /* Reset the scan lock */
    state->dmExtScanLock = NULL;
}

/****************************************************************************
NAME
    ConnectionDmBleExtScanGetGlobalParamsReq

DESCRIPTION
    Read the global parameters to be used when scanning.

RETURNS
    void
*/
void ConnectionDmBleExtScanGetGlobalParamsReq(Task theAppTask)
{
    MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_EXT_SCAN_GET_GLOBAL_PARAMS_REQ);

    message->theAppTask             = theAppTask;

    MessageSend(
                connectionGetCmTask(),
                CL_INTERNAL_DM_BLE_EXT_SCAN_GET_GLOBAL_PARAMS_REQ,
                message);
}

/****************************************************************************
NAME
    connectionHandleDmBleExtScanGetGlobalParamsReq

DESCRIPTION
    This function will initiate an Extended Scanning Get Global Parameters request.

RETURNS
    void
*/
void connectionHandleDmBleExtScanGetGlobalParamsReq(
        connectionDmExtScanState *state,
        const CL_INTERNAL_DM_BLE_EXT_SCAN_GET_GLOBAL_PARAMS_REQ_T *req
        )
{
    /* Check the state of the task lock before doing anything. */
    if (!state->dmExtScanLock)
    {
        MAKE_PRIM_T(DM_ULP_EXT_SCAN_GET_GLOBAL_PARAMS_REQ);

        /* One request at a time, set the lock. */
        state->dmExtScanLock            = req->theAppTask;

        VmSendDmPrim(prim);
    }
    else
    {
        /* Ext Scan request already outstanding, queue up this request. */
        MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_EXT_SCAN_GET_GLOBAL_PARAMS_REQ);
        COPY_CL_MESSAGE(req, message);
        MessageSendConditionallyOnTask(
                    connectionGetCmTask(),
                    CL_INTERNAL_DM_BLE_EXT_SCAN_GET_GLOBAL_PARAMS_REQ,
                    message,
                    &state->dmExtScanLock
                    );
    }
}

/****************************************************************************
NAME
    connectionHandleDmBleExtScanGetGlobalParamsCfm

DESCRIPTION
    Handles status of Extended Scanning Get Global Parameters request

RETURNS
    void
*/
void connectionHandleDmBleExtScanGetGlobalParamsCfm(connectionDmExtScanState *state,
    const DM_ULP_EXT_SCAN_GET_GLOBAL_PARAMS_CFM_T *cfm)
{
    if (state->dmExtScanLock)
    {
        uint8 i;
        MAKE_CL_MESSAGE(CL_DM_BLE_EXT_SCAN_GET_GLOBAL_PARAMS_CFM);

        message->flags                  = cfm->flags;
        message->own_address_type       = cfm->own_address_type;
        message->scanning_filter_policy = cfm->scanning_filter_policy;
        message->filter_duplicates      = cfm->filter_duplicates;
        message->scanning_phys          = cfm->scanning_phys;

        for (i = 0; i < EXT_SCAN_MAX_SCANNING_PHYS; i++)
        {
            message->phys[i].scan_interval = cfm->phys[i].scan_interval;
            message->phys[i].scan_type     = cfm->phys[i].scan_type;
            message->phys[i].scan_window   = cfm->phys[i].scan_window;
        }

        MessageSend(
                state->dmExtScanLock,
                CL_DM_BLE_EXT_SCAN_GET_GLOBAL_PARAMS_CFM,
                message
                );
    }

    /* Reset the lock. */
    state->dmExtScanLock = NULL;
}

/****************************************************************************
NAME
    ConnectionDmBleExtScanSetParamsReq

DESCRIPTION
    Set up parameters to be used for BLE Extended scanning.

RETURNS
    void
*/
void ConnectionDmBleExtScanSetParamsReq(
        Task                    theAppTask,
        uint8                   flags,
        uint8                   own_address_type,
        uint8                   scanning_filter_policy,
        uint8                   filter_duplicates,
        uint8                   scanning_phys,
        CL_ES_SCANNING_PHY_T    phy_params[EXT_SCAN_MAX_SCANNING_PHYS]
        )
{
    uint8 i;

    MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_EXT_SCAN_SET_GLOBAL_PARAMS_REQ);

    message->theAppTask             = theAppTask;
    message->flags                  = flags;
    message->own_address_type       = own_address_type;
    message->scanning_filter_policy = scanning_filter_policy;
    message->filter_duplicates      = filter_duplicates;
    message->scanning_phys          = scanning_phys;

    for (i = 0; i < EXT_SCAN_MAX_SCANNING_PHYS; i++)
    {
        message->phy_params[i].scan_interval = phy_params[i].scan_interval;
        message->phy_params[i].scan_type     = phy_params[i].scan_type;
        message->phy_params[i].scan_window   = phy_params[i].scan_window;
    }

    MessageSend(
                connectionGetCmTask(),
                CL_INTERNAL_DM_BLE_EXT_SCAN_SET_GLOBAL_PARAMS_REQ,
                message);
}

/****************************************************************************
NAME
    connectionHandleDmBleExtScanSetParamsReq

DESCRIPTION
    This function will initiate an Extended Scanning Set Parameters request.

RETURNS
    void
*/
void connectionHandleDmBleExtScanSetParamsReq(
        connectionDmExtScanState *state,
        const CL_INTERNAL_DM_BLE_EXT_SCAN_SET_GLOBAL_PARAMS_REQ_T *req
        )
{
    /* Check the state of the task lock before doing anything. */
    if (!state->dmExtScanLock)
    {
        uint8 i;

        MAKE_PRIM_T(DM_ULP_EXT_SCAN_SET_GLOBAL_PARAMS_REQ);

        /* One request at a time, set the lock. */
        state->dmExtScanLock            = req->theAppTask;

        prim->flags                     = req->flags;
        prim->own_address_type          = req->own_address_type;
        prim->scanning_filter_policy    = req->scanning_filter_policy;
        prim->filter_duplicates         = req->filter_duplicates;
        prim->scanning_phys             = req->scanning_phys;

        for (i = 0; i < EXT_SCAN_MAX_SCANNING_PHYS; i++)
        {
            prim->phys[i].scan_interval = req->phy_params[i].scan_interval;
            prim->phys[i].scan_type     = req->phy_params[i].scan_type;
            prim->phys[i].scan_window   = req->phy_params[i].scan_window;
        }

        VmSendDmPrim(prim);
    }
    else
    {
        /* Ext Scan request already outstanding, queue up this request. */
        MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_EXT_SCAN_SET_GLOBAL_PARAMS_REQ);
        COPY_CL_MESSAGE(req, message);
        MessageSendConditionallyOnTask(
                    connectionGetCmTask(),
                    CL_INTERNAL_DM_BLE_EXT_SCAN_SET_GLOBAL_PARAMS_REQ,
                    message,
                    &state->dmExtScanLock
                    );
    }
}

/****************************************************************************
NAME
    connectionHandleDmBleExtScanSetParamsCfm

DESCRIPTION
    Handles status of Extended Scanning Parameters request

RETURNS
    void
*/
void connectionHandleDmBleExtScanSetParamsCfm(connectionDmExtScanState *state,
    const DM_ULP_EXT_SCAN_SET_GLOBAL_PARAMS_CFM_T *cfm)
{
    if (state->dmExtScanLock)
    {
        MAKE_CL_MESSAGE(CL_DM_BLE_SET_EXT_SCAN_PARAMS_CFM);
        message->status = (cfm->status == HCI_SUCCESS) ? success : fail;
        MessageSend(
                state->dmExtScanLock,
                CL_DM_BLE_SET_EXT_SCAN_PARAMS_CFM,
                message
                );
    }

    /* Reset the lock. */
    state->dmExtScanLock = NULL;
}

/****************************************************************************
NAME
    ConnectionDmBleExtScanRegisterScannerReq

DESCRIPTION
    Register a scanner and filter rules to be used.

RETURNS
    void
*/
void ConnectionDmBleExtScanRegisterScannerReq(
        Task    theAppTask,
        uint32  flags,
        uint16  adv_filter,
        uint16  adv_filter_sub_field1,
        uint32  adv_filter_sub_field2,
        uint16  ad_structure_filter,
        uint16  ad_structure_filter_sub_field1,
        uint32  ad_structure_filter_sub_field2,
        uint16  ad_structure_info_len,
        uint8   *ad_structure_info[CL_AD_STRUCT_INFO_BYTE_PTRS]
        )
{
    uint8 i;

    MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_EXT_SCAN_REGISTER_SCANNER_REQ);

    message->theAppTask                     = theAppTask;
    message->flags                          = flags;
    message->adv_filter                     = adv_filter;
    message->adv_filter_sub_field1          = adv_filter_sub_field1;
    message->adv_filter_sub_field2          = adv_filter_sub_field2;
    message->ad_structure_filter            = ad_structure_filter;
    message->ad_structure_filter_sub_field1 = ad_structure_filter_sub_field1;
    message->ad_structure_filter_sub_field2 = ad_structure_filter_sub_field2;
    message->ad_structure_info_len          = ad_structure_info_len;

    for (i = 0; i < CL_AD_STRUCT_INFO_BYTE_PTRS; i++)
    {
        message->ad_structure_info[i] = VmGetHandleFromPointer(ad_structure_info[i]);
    }

    MessageSend(
                connectionGetCmTask(),
                CL_INTERNAL_DM_BLE_EXT_SCAN_REGISTER_SCANNER_REQ,
                message);
}

/****************************************************************************
NAME
    connectionHandleDmBleExtScanRegisterScannerReq

DESCRIPTION
    This function will initiate an Extended Scanning Register Scanner request.

RETURNS
    void
*/
void connectionHandleDmBleExtScanRegisterScannerReq(
        connectionDmExtScanState *state,
        const CL_INTERNAL_DM_BLE_EXT_SCAN_REGISTER_SCANNER_REQ_T *req
        )
{
    /* Check the state of the task lock before doing anything. */
    if (!state->dmExtScanLock)
    {
        uint8 i;

        MAKE_PRIM_T(DM_ULP_EXT_SCAN_REGISTER_SCANNER_REQ);

        /* One request at a time, set the lock. */
        state->dmExtScanLock            = req->theAppTask;

        prim->flags                          = req->flags;
        prim->adv_filter                     = req->adv_filter;
        prim->adv_filter_sub_field1          = req->adv_filter_sub_field1;
        prim->adv_filter_sub_field2          = req->adv_filter_sub_field2;
        prim->ad_structure_filter            = req->ad_structure_filter;
        prim->ad_structure_filter_sub_field1 = req->ad_structure_filter_sub_field1;
        prim->ad_structure_filter_sub_field2 = req->ad_structure_filter_sub_field2;
        prim->ad_structure_info_len          = req->ad_structure_info_len;

        for (i = 0; i < CL_AD_STRUCT_INFO_BYTE_PTRS; i++)
        {
            prim->ad_structure_info[i] = req->ad_structure_info[i];
        }

        VmSendDmPrim(prim);
    }
    else
    {
        /* Ext Scan request already outstanding, queue up this request. */
        MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_EXT_SCAN_REGISTER_SCANNER_REQ);
        COPY_CL_MESSAGE(req, message);
        MessageSendConditionallyOnTask(
                    connectionGetCmTask(),
                    CL_INTERNAL_DM_BLE_EXT_SCAN_REGISTER_SCANNER_REQ,
                    message,
                    &state->dmExtScanLock
                    );
    }
}

/****************************************************************************
NAME
    connectionHandleDmBleExtScanRegisterScannerCfm

DESCRIPTION
    Handles status of Extended Scanning Register Scanner request

RETURNS
    void
*/
void connectionHandleDmBleExtScanRegisterScannerCfm(connectionDmExtScanState *state,
    const DM_ULP_EXT_SCAN_REGISTER_SCANNER_CFM_T *cfm)
{
    if (state->dmExtScanLock)
    {
        /* Store and associate the returned scan_handle and the requesting task.
         * If this would mean more than the maximum amount of scanners and
         * conn lib is in debug mode, panic as the request should have been
         * rejected. */
        if (!cfm->status)
        {
            if (task_scan_handle_index < MAX_SCAN_HANDLES)
            {
                task_scan_handles[task_scan_handle_index].registering_task = state->dmExtScanLock;
                task_scan_handles[task_scan_handle_index].scan_handle = cfm->scan_handle;
            }
            else
            {
                CL_DEBUG(("Maximum number of registered scanners reached."));
                return;
            }

            task_scan_handle_index++;
        }

        MAKE_CL_MESSAGE(CL_DM_BLE_EXT_SCAN_REGISTER_SCANNER_CFM);
        message->status = (cfm->status == HCI_SUCCESS) ? success : fail;
        message->scan_handle = cfm->scan_handle;
        MessageSend(
                state->dmExtScanLock,
                CL_DM_BLE_EXT_SCAN_REGISTER_SCANNER_CFM,
                message
                );
    }

    /* Reset the lock. */
    state->dmExtScanLock = NULL;
}

/****************************************************************************
NAME
    connectionHandleDmBleExtScanCtrlScanInfoInd

DESCRIPTION
    Handles the Extended Scanning Control Scan Info indication, sent any time
    the Contoller's LE Scanner config is changed or new scanners are enabled/disabled.

RETURNS
    void
*/
void connectionHandleDmBleExtScanCtrlScanInfoInd(const DM_ULP_EXT_SCAN_CTRL_SCAN_INFO_IND_T *ind)
{
    uint8       i;
    unsigned    extended_scanners = ind->num_of_enabled_scanners;

    MAKE_CL_MESSAGE(CL_DM_BLE_EXT_SCAN_CTRL_SCAN_INFO_IND);

    message->reason                     = ind->reason;
    message->controller_updated         = ind->controller_updated;
    message->num_of_enabled_scanners    = extended_scanners;
    message->legacy_scanner_enabled      = ind->legacy_scanner_enabled;

    if (ind->legacy_scanner_enabled && extended_scanners)
    {
        extended_scanners--;
    }

    /* Extended scan messages come via a stream.
       Check whether we need to connect to a new stream, 
       or dispose of our old one */
    if (extended_scanners)
    {
        Source src = StreamExtScanSource();
        if (src && src != ExtScanSrc)
        {
            ExtScanSrc = StreamExtScanSource();
            MessageStreamTaskFromSource(ExtScanSrc, connectionGetCmTask());
        }
        else if (!ExtScanSrc)
        {
            DEBUG_LOG_WARN("connectionHandleDmBleExtScanCtrlScanInfoInd. Should have %d scanners, but no stream available",
                            extended_scanners);
        }
    }
    else
    {
        ExtScanSrc = 0;
    }

    message->duration                   = ind->duration;
    message->scanning_phys              = ind->scanning_phys;

    for (i = 0; i < EXT_SCAN_MAX_SCANNING_PHYS; i++)
    {
        message->phys[i].scan_interval  = ind->phys[i].scan_interval;
        message->phys[i].scan_type      = ind->phys[i].scan_type;
        message->phys[i].scan_window    = ind->phys[i].scan_window;
    }

    MessageSend(
                connectionGetAppTask(),
                CL_DM_BLE_EXT_SCAN_CTRL_SCAN_INFO_IND,
                message
                );
}

/****************************************************************************
NAME
    ConnectionDmBleExtScanUnregisterScannerReq

DESCRIPTION
    Unregister a scanner.

RETURNS
    void
*/
bool ConnectionDmBleExtScanUnregisterScannerReq(
        Task    theAppTask,
        uint8   scan_handle
        )
{
    /* Verify that there is at least one remaining association to remove, and
     * that the task is indeed associated with the scanner it's trying to
     * unregister. */
    if (task_scan_handle_index > 0)
    {
        uint8 i = 0;

        for (i = 0; i < task_scan_handle_index; i++)
        {
            if (scan_handle == task_scan_handles[i].scan_handle)
            {
                if (theAppTask == task_scan_handles[i].registering_task)
                {
                    MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_EXT_SCAN_UNREGISTER_SCANNER_REQ);

                    message->theAppTask     = theAppTask;
                    message->scan_handle    = scan_handle;

                    MessageSend(
                                connectionGetCmTask(),
                                CL_INTERNAL_DM_BLE_EXT_SCAN_UNREGISTER_SCANNER_REQ,
                                message);
                    return TRUE;
                }
                else
                {
                    CL_DEBUG_INFO(("Requesting task is not associated with the scanner it tried to terminate."));
                    Panic();
                    return FALSE;
                }
            }
        }

        CL_DEBUG_INFO(("Scan handle requested for removal not found."));
        return FALSE;
    }
    else
    {
        CL_DEBUG_INFO(("There are no more scanners to unregister."));
        return FALSE;
    }
}

/****************************************************************************
NAME
    connectionHandleDmBleExtScanUnregisterScannerReq

DESCRIPTION
    This function will initiate an Extended Scanning Unregister Scanner request.

RETURNS
    void
*/
void connectionHandleDmBleExtScanUnregisterScannerReq(
        connectionDmExtScanState *state,
        const CL_INTERNAL_DM_BLE_EXT_SCAN_UNREGISTER_SCANNER_REQ_T *req
        )
{
    /* Check the state of the task lock before doing anything. */
    if (!state->dmExtScanLock)
    {
        MAKE_PRIM_T(DM_ULP_EXT_SCAN_UNREGISTER_SCANNER_REQ);

        /* One request at a time, set the lock. */
        state->dmExtScanLock    = req->theAppTask;

        prim->scan_handle       = req->scan_handle;

        VmSendDmPrim(prim);
    }
    else
    {
        /* Ext Scan request already outstanding, queue up this request. */
        MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_EXT_SCAN_UNREGISTER_SCANNER_REQ);
        COPY_CL_MESSAGE(req, message);
        MessageSendConditionallyOnTask(
                    connectionGetCmTask(),
                    CL_INTERNAL_DM_BLE_EXT_SCAN_UNREGISTER_SCANNER_REQ,
                    message,
                    &state->dmExtScanLock
                    );
    }
}

/****************************************************************************
NAME
    connectionHandleDmBleExtScanUnregisterScannerCfm

DESCRIPTION
    Handles status of Extended Scanning Unregister Scanner request

RETURNS
    void
*/
void connectionHandleDmBleExtScanUnregisterScannerCfm(connectionDmExtScanState *state,
    const DM_ULP_EXT_SCAN_UNREGISTER_SCANNER_CFM_T *cfm)
{
    if (state->dmExtScanLock)
    {
        uint8 i;
        bool task_found = FALSE;

        if (cfm->status == HCI_SUCCESS)
        {
            /* Remove the task-handle association from the tracking struct and shift
             * all remaining pairs after it one position down. */
            for (i = 0; i < task_scan_handle_index; i++)
            {
                if (task_found)
                {
                    task_scan_handles[i-1] = task_scan_handles[i];
                }
                else if (state->dmExtScanLock == task_scan_handles[i].registering_task)
                {
                    task_found = TRUE;
                }
            }

            if (task_found)
            {
                /* Finally, clear the last element and decrement the index. */
                task_scan_handle_index -= 1;
                memset(&task_scan_handles[task_scan_handle_index], 0, sizeof(task_scan_handles_pair));
            }
            else
            {
                /* Panic in debug mode */
                CL_DEBUG_INFO(("Task in dmExtScanLock not found in task_scan_Handles_pair."));
                return;
            }
        }

        /* Send the CFM message */
        {
            MAKE_CL_MESSAGE(CL_DM_BLE_EXT_SCAN_UNREGISTER_SCANNER_CFM);
            message->status = (cfm->status == HCI_SUCCESS) ? success : fail;

            MessageSend(
                        state->dmExtScanLock,
                        CL_DM_BLE_EXT_SCAN_UNREGISTER_SCANNER_CFM,
                        message
                        );
        }
    }

    /* Reset the lock. */
    state->dmExtScanLock = NULL;
}

/****************************************************************************
NAME
    connectionHandleDmBleExtScanFilteredAdvReportInd

DESCRIPTION
    Handles BLE Extended Scanning Filtered Advertising report indication

RETURNS
    void
*/
void connectionHandleDmBleExtScanFilteredAdvReportInd(const DM_ULP_EXT_SCAN_FILTERED_ADV_REPORT_IND_T *ind)
{
    uint8 i;
    Task sending_task_array[MAX_SCAN_HANDLES+1];    /* Allow one for NULL termination */
    uint8 arr_idx = 0;

    if (!ExtScanSrc)
    {
        return;
    }

    for (i = 0; i < ind->num_of_scan_handles; i++)
    {
        uint8 j;

        for (j = 0; j < task_scan_handle_index; j++)
        {
            if (ind->scan_handles[i] == task_scan_handles[j].scan_handle)
            {
                uint8 k;
                uint8 already_added = 0;

                for (k = 0; k < arr_idx; k++)
                {
                    if (sending_task_array[k] == task_scan_handles[j].registering_task)
                    {
                        already_added = 1;
                        break;
                    }
                }

                if (!already_added)
                {
                    sending_task_array[arr_idx] = task_scan_handles[j].registering_task;
                    arr_idx++;
                }
                else
                {
                    break;
                }
            }
        }

        for (j = 0; j < task_scan_train_handle_index; j++)
        {
            if (ind->scan_handles[i] == task_scan_train_handles[j].scan_handle)
            {
                uint8 k;
                uint8 already_added = 0;
        
                for (k = 0; k < arr_idx; k++)
                {
                    if (sending_task_array[k] == task_scan_train_handles[j].registering_task)
                    {
                        already_added = 1;
                        break;
                    }
                }
        
                if (!already_added)
                {
                    sending_task_array[arr_idx] = task_scan_train_handles[j].registering_task;
                    arr_idx++;
                }
                else
                {
                    break;
                }
            }
        }
    }

    CL_INTERNAL_DM_BLE_EXT_SCAN_ADV_REPORT_DONE_IND_T * const msg = PanicUnlessNew(CL_INTERNAL_DM_BLE_EXT_SCAN_ADV_REPORT_DONE_IND_T);
    msg->size = SourceBoundary(ExtScanSrc);
    msg->source = ExtScanSrc;

    /* Only generate message if somebody wants it */
    if (arr_idx)
    {
        MAKE_CL_MESSAGE(CL_DM_BLE_EXT_SCAN_FILTERED_ADV_REPORT_IND);

        message->event_type     = ind->event_type;
        message->primary_phy    = ind->primary_phy;
        message->secondary_phy  = ind->secondary_phy;
        message->adv_sid        = ind->adv_sid;

        message->current_addr.type   = ind->current_addr_type;
        BdaddrConvertBluestackToVm(&message->current_addr.addr, &ind->current_addr);
        message->permanent_addr.type = ind->permanent_addr_type;
        BdaddrConvertBluestackToVm(&message->permanent_addr.addr, &ind->permanent_addr);
        message->direct_addr.type    = ind->direct_addr_type;
        BdaddrConvertBluestackToVm(&message->direct_addr.addr, &ind->direct_addr);

        message->tx_power       = ind->tx_power;
        message->rssi           = ind->rssi;
        message->periodic_adv_interval = ind->periodic_adv_interval;

        message->adv_data_info  = ind->adv_data_info;
        message->ad_flags       = (ind->adv_data_info & AD_STRUCT_FLAGS_PRESENT ? ind->ad_flags : 0);

        message->adv_data_len   = msg->size;
        message->adv_data       = SourceMap(ExtScanSrc);

        sending_task_array[arr_idx++]=NULL;

        MessageSendMulticast(sending_task_array,
                             CL_DM_BLE_EXT_SCAN_FILTERED_ADV_REPORT_IND,
                             message);
    }

    /* Finally, send a message to conn lib to indicate that all tasks have been
     * notified. This will only be processed after all tasks have returned from
     * their handling function, and thus signals the conn lib that it is safe to
     * free the underlying memory in the stream. */


    MessageSend(connectionGetCmTask(),
                CL_INTERNAL_DM_BLE_EXT_SCAN_ADV_REPORT_DONE_IND,
                msg);
}

/****************************************************************************
NAME
    connectionHandleDmBleExtScanFilteredAdvReportDoneInd

DESCRIPTION
    Once all interested tasks have processed the BLE Extended Scanning Filtered
    Advertising report indication, clear the report from the incoming stream.

RETURNS
    void
*/
void connectionHandleDmBleExtScanFilteredAdvReportDoneInd(const CL_INTERNAL_DM_BLE_EXT_SCAN_ADV_REPORT_DONE_IND_T *ind)
{
    SourceDrop(ind->source, ind->size);

    /* Check if the stream has more data. */
    if (SourceSize(ind->source) != 0)
    {
        MAKE_CL_MESSAGE(CL_INTERNAL_MESSAGE_MORE_DATA);

        message->source = ind->source;
        message->stream_type = ext_scan_stream;

        MessageSend(connectionGetCmTask(),
                CL_INTERNAL_MESSAGE_MORE_DATA,
                message);
    }
}

/****************************************************************************
NAME
    ConnectionDmBleExtScanGetCtrlScanInfoReq

DESCRIPTION
    Get information on how the LE controller's scanner has been configured.

RETURNS
    void
*/
void ConnectionDmBleExtScanGetCtrlScanInfoReq(Task theAppTask)
{
    MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_EXT_SCAN_GET_CTRL_SCAN_INFO_REQ);

    message->theAppTask             = theAppTask;

    MessageSend(
                connectionGetCmTask(),
                CL_INTERNAL_DM_BLE_EXT_SCAN_GET_CTRL_SCAN_INFO_REQ,
                message);
}

/****************************************************************************
NAME
    connectionHandleDmBleExtScanGetCtrlScanInfoReq

DESCRIPTION
    This function will initiate an Extended Scanning Get Controller Scanner Info
    request.

RETURNS
    void
*/
void connectionHandleDmBleExtScanGetCtrlScanInfoReq(
        connectionDmExtScanState *state,
        const CL_INTERNAL_DM_BLE_EXT_SCAN_GET_CTRL_SCAN_INFO_REQ_T *req
        )
{
    /* Check the state of the task lock before doing anything. */
    if (!state->dmExtScanLock)
    {
        MAKE_PRIM_T(DM_ULP_EXT_SCAN_GET_CTRL_SCAN_INFO_REQ);

        /* One request at a time, set the lock. */
        state->dmExtScanLock            = req->theAppTask;

        VmSendDmPrim(prim);
    }
    else
    {
        /* Ext Scan request already outstanding, queue up this request. */
        MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_EXT_SCAN_GET_CTRL_SCAN_INFO_REQ);
        COPY_CL_MESSAGE(req, message);
        MessageSendConditionallyOnTask(
                    connectionGetCmTask(),
                    CL_INTERNAL_DM_BLE_EXT_SCAN_GET_CTRL_SCAN_INFO_REQ,
                    message,
                    &state->dmExtScanLock
                    );
    }
}

/****************************************************************************
NAME
    ConnectionDmBleExtScanGetCtrlScanInfoCfm

DESCRIPTION
    Handles status of an Extended Scanning Get Controller Scanner Info request

RETURNS
    void
*/
void ConnectionDmBleExtScanGetCtrlScanInfoCfm(connectionDmExtScanState *state,
    const DM_ULP_EXT_SCAN_GET_CTRL_SCAN_INFO_CFM_T *cfm)
{
    if (state->dmExtScanLock)
    {
        uint8 i;
        MAKE_CL_MESSAGE(CL_DM_BLE_EXT_SCAN_GET_CTRL_SCAN_INFO_CFM);

        message->num_of_enabled_scanners    = cfm->num_of_enabled_scanners;
        message->legacy_scanner_enabled     = cfm->legacy_scanner_enabled;
        message->duration                   = cfm->duration;
        message->scanning_phys              = cfm->scanning_phys;

        for (i = 0; i < EXT_SCAN_MAX_SCANNING_PHYS; i++)
        {
            message->phys[i].scan_interval = cfm->phys[i].scan_interval;
            message->phys[i].scan_type     = cfm->phys[i].scan_type;
            message->phys[i].scan_window   = cfm->phys[i].scan_window;
        }

        MessageSend(
                state->dmExtScanLock,
                CL_DM_BLE_EXT_SCAN_GET_CTRL_SCAN_INFO_CFM,
                message
                );
    }

    /* Reset the lock. */
    state->dmExtScanLock = NULL;
}

/****************************************************************************
NAME
    connectionHandleDmBleExtScanDurationExpiredInd

DESCRIPTION
    Handles the Extended Scanning Duration Expired indication, sent any time a
    duration timer expires for a scanner. The scanner will no longer be scanning.

    If the scan handle has been unregistered, Connection library will also
    remove the association to the registering task from its tracking struct.
    Any advertising reports from this scanner in the stream that have not yet
    been processed will be silently consumed.

RETURNS
    void
*/
void connectionHandleDmBleExtScanDurationExpiredInd(const DM_ULP_EXT_SCAN_DURATION_EXPIRED_IND_T *ind)
{
    Task associated_task = NULL;
    bool scan_handle_found = FALSE;

    /* Verify that there is at least one remaining scanner and that the
     * scan handle sent is already in the tracking struct. */
    if (task_scan_handle_index > 0)
    {
        uint8 i;

        for (i = 0; i < task_scan_handle_index; i++)
        {
            if (scan_handle_found)
            {
                task_scan_handles[i-1] = task_scan_handles[i];
            }
            else if (ind->scan_handle == task_scan_handles[i].scan_handle)
            {
                scan_handle_found = 1;
                associated_task = task_scan_handles[i].registering_task;
            }
        }

        if (scan_handle_found)
        {
            /* Finally, clear the last element and decrement the index. */
            task_scan_handle_index -= 1;
            memset(&task_scan_handles[task_scan_handle_index], 0, sizeof(task_scan_handles_pair));
        }
        else
        {
            CL_DEBUG_INFO(("Scan handle requested for removal not found."));
            Panic();
        }
    }
    else
    {
        CL_DEBUG_INFO(("There are no more scanners to unregister."));
        Panic();
    }

    {
        MAKE_CL_MESSAGE(CL_DM_BLE_EXT_SCAN_DURATION_EXPIRED_IND);

        message->scan_handle = ind->scan_handle;
        message->scan_handle_unregistered = ind->scan_handle_unregistered;

        MessageSend(
                    associated_task,
                    CL_DM_BLE_EXT_SCAN_DURATION_EXPIRED_IND,
                    message
                    );
    }
}

/*****************************************************************************
 *                  Periodic Scanning functions                              *
 *****************************************************************************/

/****************************************************************************
NAME
    ConnectionDmBlePeriodicScanSyncTrainReq

DESCRIPTION
    Establish sync to one of the periodic trains.

RETURNS
    void
*/
void ConnectionDmBlePeriodicScanSyncTrainReq(
    Task    theAppTask,
    uint8   report_periodic,
    uint16  skip,
    uint16  sync_timeout,
    uint8   sync_cte_type,
    uint16  attempt_sync_for_x_seconds,
    uint8   number_of_periodic_trains,
    CL_DM_ULP_PERIODIC_SCAN_TRAINS_T periodic_trains[CL_MAX_PERIODIC_TRAIN_LIST_SIZE])
{
    uint8 i;

    MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_PERIODIC_SCAN_SYNC_TO_TRAIN_REQ);

    message->theAppTask                    = theAppTask;
    message->report_periodic               = report_periodic;
    message->skip                          = skip;
    message->sync_timeout                  = sync_timeout;
    message->sync_cte_type                 = sync_cte_type;
    message->attempt_sync_for_x_seconds    = attempt_sync_for_x_seconds;
    message->number_of_periodic_trains     = number_of_periodic_trains;

    for (i = 0; i < number_of_periodic_trains; i++)
    {
        message->periodic_trains[i].adv_sid = periodic_trains[i].adv_sid;
        message->periodic_trains[i].taddr = periodic_trains[i].taddr;
    }

    MessageSend(
                connectionGetCmTask(),
                CL_INTERNAL_DM_BLE_PERIODIC_SCAN_SYNC_TO_TRAIN_REQ,
                message);
}

/****************************************************************************
NAME
    connectionHandleDmBlePeriodicScanSyncTrainReq

DESCRIPTION
    This function will initiate a Periodic Scanning Sync to Train request.

RETURNS
    void
*/
void connectionHandleDmBlePeriodicScanSyncTrainReq(connectionDmPerScanState *state,
        const CL_INTERNAL_DM_BLE_PERIODIC_SCAN_SYNC_TO_TRAIN_REQ_T *req
        )
{
    /* Check the state of the task lock before doing anything. */
    if (!state->dmPerScanLock)
    {
        uint8 i;

        MAKE_PRIM_T(DM_ULP_PERIODIC_SCAN_SYNC_TO_TRAIN_REQ);

        /* One request at a time, set the lock. */
        state->dmPerScanLock                = req->theAppTask;

        prim->report_periodic               = req->report_periodic;
        prim->skip                          = req->skip;
        prim->sync_timeout                  = req->sync_timeout;
        prim->sync_cte_type                 = req->sync_cte_type;
        prim->attempt_sync_for_x_seconds    = req->attempt_sync_for_x_seconds;
        prim->number_of_periodic_trains     = req->number_of_periodic_trains;


        for (i = 0; i < req->number_of_periodic_trains; i++)
        {
            prim->periodic_trains[i].adv_sid = req->periodic_trains[i].adv_sid;
            BdaddrConvertTypedVmToBluestack(&prim->periodic_trains[i].addrt, &req->periodic_trains[i].taddr);
        }

        VmSendDmPrim(prim);
    }
    else
    {
        /* Per Scan request already outstanding, queue up this request. */
        MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_PERIODIC_SCAN_SYNC_TO_TRAIN_REQ);
        COPY_CL_MESSAGE(req, message);
        MessageSendConditionallyOnTask(
                    connectionGetCmTask(),
                    CL_INTERNAL_DM_BLE_PERIODIC_SCAN_SYNC_TO_TRAIN_REQ,
                    message,
                    &state->dmPerScanLock
                    );
    }
}

/****************************************************************************
NAME
    connectionHandleDmBlePeriodicScanSyncTrainCfm

DESCRIPTION
    Handles status of Periodic Scanning Sync to Train request.

RETURNS
    void
*/
void connectionHandleDmBlePeriodicScanSyncTrainCfm(connectionDmPerScanState *state,
    const DM_ULP_PERIODIC_SCAN_SYNC_TO_TRAIN_CFM_T *cfm)
{
    if (state->dmPerScanLock)
    {
        /* Store and associate the returned sync_handle and its connected stream
         * with the requesting task. If this would mean more than the maximum
         * number of scanners and conn lib is in debug mode, panic, as the
         * request should have been rejected. */
        if (!cfm->status)
        {
            if (task_sync_handle_index < MAX_SYNC_HANDLES)
            {
                task_sync_handles[task_sync_handle_index].registering_task = state->dmPerScanLock;
                task_sync_handles[task_sync_handle_index].sync_handle = cfm->sync_handle;
                task_sync_handles[task_sync_handle_index].source = StreamPeriodicScanSource(cfm->sync_handle);
                MessageStreamTaskFromSource(task_sync_handles[task_sync_handle_index].source, connectionGetCmTask());
            }
            else
            {
                CL_DEBUG(("Maximum number of periodic trains sync'ed reached."));
                return;
            }

            task_sync_handle_index++;
        }

        MAKE_CL_MESSAGE(CL_DM_BLE_PERIODIC_SCAN_SYNC_TO_TRAIN_CFM);

        message->status = (cfm->status == 0xFFFF) ? cfm->status : connectionConvertHciStatus(cfm->status);
        message->sync_handle    = cfm->sync_handle;
        message->adv_sid        = cfm->adv_sid;

        BdaddrConvertTypedBluestackToVm(&message->taddr, &cfm->addrt);

        message->adv_phy                = cfm->adv_phy;
        message->periodic_adv_interval  = cfm->periodic_adv_interval;
        message->adv_clock_accuracy     = cfm->adv_clock_accuracy;

        MessageSend(
                state->dmPerScanLock,
                CL_DM_BLE_PERIODIC_SCAN_SYNC_TO_TRAIN_CFM,
                message
                );
    }

    /* If the request is not still pending, reset the lock. */
    if (cfm->status != 0xFFFF)
    {
        state->dmPerScanLock = NULL;
    }
}

/****************************************************************************
NAME
    ConnectionDmBlePeriodicScanSyncCancelReq

DESCRIPTION
    Cancel an attempt to synchronise on to a periodic train.

RETURNS
    void
*/
void ConnectionDmBlePeriodicScanSyncCancelReq(void)
{
    MAKE_PRIM_T(DM_ULP_PERIODIC_SCAN_SYNC_TO_TRAIN_CANCEL_REQ);

    /* This prim is meant to bypass the relevant lock. */
    VmSendDmPrim(prim);
}

/****************************************************************************
NAME
    connectionHandleDmBlePeriodicScanSyncCancelCfm

DESCRIPTION
    Handles status of Periodic Scanning Sync Cancel request.

RETURNS
    void
*/
void connectionHandleDmBlePeriodicScanSyncCancelCfm(connectionDmPerScanState *state,
    const DM_ULP_PERIODIC_SCAN_SYNC_TO_TRAIN_CANCEL_CFM_T *cfm)
{
    if (state->dmPerScanLock)
    {
        MAKE_CL_MESSAGE(CL_DM_BLE_PERIODIC_SCAN_SYNC_CANCEL_CFM);

        message->status = connectionConvertHciStatus(cfm->status);

        MessageSend(
                state->dmPerScanLock,
                CL_DM_BLE_PERIODIC_SCAN_SYNC_CANCEL_CFM,
                message
                );
    }

    /* Reset the lock. */
    state->dmPerScanLock = NULL;
}

/****************************************************************************
NAME
    ConnectionDmBlePeriodicScanSyncTerminateReq

DESCRIPTION
    Terminate sync to a currently synced periodic train.

RETURNS
    void
*/
bool ConnectionDmBlePeriodicScanSyncTerminateReq(Task theAppTask, uint16 sync_handle)
{
    /* Verify that there is at least one remaining association to remove, and
     * that the task is indeed associated with the train it's trying to terminate
     * the sync to. */
    if (task_sync_handle_index > 0)
    {
        uint8 i;

        for (i = 0; i < task_sync_handle_index; i++)
        {
            if (sync_handle == task_sync_handles[i].sync_handle)
            {
                if (theAppTask == task_sync_handles[i].registering_task)
                {

                    MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_PERIODIC_SCAN_SYNC_TERMINATE_REQ);

                    message->theAppTask     = theAppTask;
                    message->sync_handle    = sync_handle;

                    MessageSend(
                                connectionGetCmTask(),
                                CL_INTERNAL_DM_BLE_PERIODIC_SCAN_SYNC_TERMINATE_REQ,
                                message);
                    return TRUE;
                }
                else
                {
                    CL_DEBUG_INFO(("Requesting task is not associated with the sync'ed train it tried to terminate."));
                    Panic();
                    return FALSE;
                }
            }
        }

        CL_DEBUG_INFO(("Sync handle requested for removal not found."));
        return FALSE;
    }
    else
    {
        CL_DEBUG_INFO(("There are no more sync'ed trains to terminate."));
        return FALSE;
    }
}

/****************************************************************************
NAME
    connectionHandleDmBlePeriodicScanSyncTerminateReq

DESCRIPTION
    This function will initiate a Periodic Scanning Sync Terminate request.

RETURNS
    void
*/
void connectionHandleDmBlePeriodicScanSyncTerminateReq(connectionDmPerScanState *state,
        const CL_INTERNAL_DM_BLE_PERIODIC_SCAN_SYNC_TERMINATE_REQ_T *req
        )
{
    /* Check the state of the task lock before doing anything. */
    if (!state->dmPerScanLock)
    {
        MAKE_PRIM_T(DM_ULP_PERIODIC_SCAN_SYNC_TERMINATE_REQ);

        /* One request at a time, set the lock. */
        state->dmPerScanLock    = req->theAppTask;

        prim->sync_handle       = req->sync_handle;

        VmSendDmPrim(prim);
    }
    else
    {
        /* Per Scan request already outstanding, queue up this request. */
        MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_PERIODIC_SCAN_SYNC_TERMINATE_REQ);
        COPY_CL_MESSAGE(req, message);
        MessageSendConditionallyOnTask(
                    connectionGetCmTask(),
                    CL_INTERNAL_DM_BLE_PERIODIC_SCAN_SYNC_TERMINATE_REQ,
                    message,
                    &state->dmPerScanLock
                    );
    }
}

/****************************************************************************
NAME
    connectionHandleDmBlePeriodicScanSyncTerminateCfm

DESCRIPTION
    Handles status of Periodic Scanning Sync Terminate request.

RETURNS
    void
*/
void connectionHandleDmBlePeriodicScanSyncTerminateCfm(connectionDmPerScanState *state,
    const DM_ULP_PERIODIC_SCAN_SYNC_TERMINATE_CFM_T *cfm)
{
    if (state->dmPerScanLock)
    {
        uint8 i;
        bool task_found = FALSE;

        /* Remove the task-handle-stream association from the tracking struct
         * and shift all remaining elements after it one position down. */
        for (i = 0; i < task_sync_handle_index; i++)
        {
            if (task_found)
            {
                task_sync_handles[i-1] = task_sync_handles[i];
            }
            else if (state->dmPerScanLock == task_sync_handles[i].registering_task)
            {
                task_found = TRUE;
            }
        }

        if (task_found)
        {
            task_sync_handle_index -= 1;
            memset(&task_sync_handles[task_sync_handle_index], 0, sizeof(task_sync_handles_pair));
        }
        else
        {
            CL_DEBUG_INFO(("state->dmPerScanLock not found in task_sync_handles\n"));
            return;
        }

        {
            MAKE_CL_MESSAGE(CL_DM_BLE_PERIODIC_SCAN_SYNC_TERMINATE_CFM);

            message->status = (cfm->status) ? fail : success;
            message->sync_handle = cfm->sync_handle;

            MessageSend(
                    state->dmPerScanLock,
                    CL_DM_BLE_PERIODIC_SCAN_SYNC_TERMINATE_CFM,
                    message
                    );
        }
    }

    /* Reset the lock. */
    state->dmPerScanLock = NULL;
}

/****************************************************************************
NAME
    ConnectionDmBlePeriodicScanSyncTransferReq

DESCRIPTION
    Using the sync handle provided by the local Controller after synchronising
    to a periodic advertising train, instruct the Controller to transfer SyncInfo
    related to this PA train to a connected peer.

RETURNS
    void
*/
void ConnectionDmBlePeriodicScanSyncTransferReq(Task theAppTask,
                                                typed_bdaddr taddr,
                                                uint16 service_data,
                                                uint16 sync_handle)
{
    MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_PERIODIC_SCAN_SYNC_TRANSFER_REQ);

    message->theAppTask     = theAppTask;
    message->taddr          = taddr;
    message->service_data   = service_data;
    message->sync_handle    = sync_handle;

    MessageSend(
                connectionGetCmTask(),
                CL_INTERNAL_DM_BLE_PERIODIC_SCAN_SYNC_TRANSFER_REQ,
                message);
}

/****************************************************************************
NAME
    connectionHandleDmBlePeriodicScanSyncTransferReq

DESCRIPTION
    This function will initiate a Periodic Scanning Sync Transfer request.

RETURNS
    void
*/
void connectionHandleDmBlePeriodicScanSyncTransferReq(connectionDmPerScanState *state,
        const CL_INTERNAL_DM_BLE_PERIODIC_SCAN_SYNC_TRANSFER_REQ_T *req
        )
{
    /* Check the state of the task lock before doing anything. */
    if (!state->dmPerScanLock)
    {
        MAKE_PRIM_T(DM_ULP_PERIODIC_SCAN_SYNC_TRANSFER_REQ);

        /* One request at a time, set the lock. */
        state->dmPerScanLock    = req->theAppTask;

        BdaddrConvertTypedVmToBluestack(&prim->addrt, &req->taddr);
        prim->service_data   = req->service_data;
        prim->sync_handle       = req->sync_handle;

        VmSendDmPrim(prim);
    }
    else
    {
        /* Per Scan request already outstanding, queue up this request. */
        MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_PERIODIC_SCAN_SYNC_TRANSFER_REQ);
        COPY_CL_MESSAGE(req, message);
        MessageSendConditionallyOnTask(
                    connectionGetCmTask(),
                    CL_INTERNAL_DM_BLE_PERIODIC_SCAN_SYNC_TRANSFER_REQ,
                    message,
                    &state->dmPerScanLock
                    );
    }
}

/****************************************************************************
NAME
    connectionHandleDmBlePeriodicScanSyncTransferCfm

DESCRIPTION
    Handles status of Periodic Scanning Sync Transfer request.

RETURNS
    void
*/
void connectionHandleDmBlePeriodicScanSyncTransferCfm(connectionDmPerScanState *state,
    const DM_ULP_PERIODIC_SCAN_SYNC_TRANSFER_CFM_T *cfm)
{
    if (state->dmPerScanLock)
    {
        MAKE_CL_MESSAGE(CL_DM_BLE_PERIODIC_SCAN_SYNC_TRANSFER_CFM);

        message->status = (cfm->status) ? fail : success;
        message->sync_handle = cfm->sync_handle;

        MessageSend(
                state->dmPerScanLock,
                CL_DM_BLE_PERIODIC_SCAN_SYNC_TRANSFER_CFM,
                message
                );
    }

    /* Reset the lock. */
    state->dmPerScanLock = NULL;
}

/****************************************************************************
NAME
    ConnectionDmBlePeriodicScanSyncTransferParamsReq

DESCRIPTION
    Configures the Controller's future default response for all incoming sync
    information procedures.

RETURNS
    void
*/
void ConnectionDmBlePeriodicScanSyncTransferParamsReq(
    Task theAppTask,
    typed_bdaddr taddr,
    uint16 skip,
    uint16 sync_timeout,
    uint8 mode,
    uint8 cte_type)
{
    MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_PERIODIC_SCAN_SYNC_TRANSFER_PARAMS_REQ);

    message->theAppTask     = theAppTask;
    message->taddr          = taddr;
    message->skip           = skip;
    message->sync_timeout   = sync_timeout;
    message->mode           = mode;
    message->cte_type       = cte_type;

    MessageSend(
                connectionGetCmTask(),
                CL_INTERNAL_DM_BLE_PERIODIC_SCAN_SYNC_TRANSFER_PARAMS_REQ,
                message);
}

/****************************************************************************
NAME
    connectionHandleDmBlePeriodicScanSyncTransferParamsReq

DESCRIPTION
    This function will initiate a Periodic Scanning Sync Transfer Params request.

RETURNS
    void
*/
void connectionHandleDmBlePeriodicScanSyncTransferParamsReq(connectionDmPerScanState *state,
        const CL_INTERNAL_DM_BLE_PERIODIC_SCAN_SYNC_TRANSFER_PARAMS_REQ_T *req
        )
{
    /* Check the state of the task lock before doing anything. */
    if (!state->dmPerScanLock)
    {
        MAKE_PRIM_T(DM_ULP_PERIODIC_SCAN_SYNC_TRANSFER_PARAMS_REQ);

        /* One request at a time, set the lock. */
        state->dmPerScanLock    = req->theAppTask;

        BdaddrConvertTypedVmToBluestack(&prim->addrt, &req->taddr);
        prim->skip           = req->skip;
        prim->sync_timeout   = req->sync_timeout;
        prim->mode           = req->mode;
        prim->cte_type       = req->cte_type;

        VmSendDmPrim(prim);
    }
    else
    {
        /* Per Scan request already outstanding, queue up this request. */
        MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_PERIODIC_SCAN_SYNC_TRANSFER_PARAMS_REQ);
        COPY_CL_MESSAGE(req, message);
        MessageSendConditionallyOnTask(
                    connectionGetCmTask(),
                    CL_INTERNAL_DM_BLE_PERIODIC_SCAN_SYNC_TRANSFER_PARAMS_REQ,
                    message,
                    &state->dmPerScanLock
                    );
    }
}

/****************************************************************************
NAME
    connectionHandleDmBlePeriodicScanSyncTransferParamsCfm

DESCRIPTION
    Handles status of Periodic Scanning Sync Transfer Params request.

RETURNS
    void
*/
void connectionHandleDmBlePeriodicScanSyncTransferParamsCfm(connectionDmPerScanState *state,
    const DM_ULP_PERIODIC_SCAN_SYNC_TRANSFER_PARAMS_CFM_T *cfm)
{
    if (state->dmPerScanLock)
    {
        MAKE_CL_MESSAGE(CL_DM_BLE_PERIODIC_SCAN_SYNC_TRANSFER_PARAMS_CFM);

        message->status = (cfm->status) ? fail : success;
        BdaddrConvertTypedBluestackToVm(&message->taddr, &cfm->addrt);

        MessageSend(
                state->dmPerScanLock,
                CL_DM_BLE_PERIODIC_SCAN_SYNC_TRANSFER_PARAMS_CFM,
                message
                );
    }

    /* Reset the lock. */
    state->dmPerScanLock = NULL;
}

/****************************************************************************
NAME
    ConnectionDmBlePeriodicScanStartFindTrainsReq

DESCRIPTION
    Search for periodic trains that meet a specified ad_structure filter.

RETURNS
    void
*/
void ConnectionDmBlePeriodicScanStartFindTrainsReq(
    Task    theAppTask,
    uint32  flags,
    uint16  scan_for_x_seconds,
    uint16  ad_structure_filter,
    uint16  ad_structure_filter_sub_field1,
    uint32  ad_structure_filter_sub_field2,
    uint16  ad_structure_info_len,
    uint8   *ad_structure_info[CL_AD_STRUCT_INFO_BYTE_PTRS])
{
    uint8 i;

    MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_PERIODIC_SCAN_START_FIND_TRAINS_REQ);

    message->theAppTask                     = theAppTask;
    message->flags                          = flags;
    message->scan_for_x_seconds             = scan_for_x_seconds;
    message->ad_structure_filter            = ad_structure_filter;
    message->ad_structure_filter_sub_field1 = ad_structure_filter_sub_field1;
    message->ad_structure_filter_sub_field2 = ad_structure_filter_sub_field2;
    message->ad_structure_info_len          = ad_structure_info_len;

    for (i = 0; i < CL_AD_STRUCT_INFO_BYTE_PTRS; i++)
    {
        message->ad_structure_info[i] = VmGetHandleFromPointer(ad_structure_info[i]);
    }

    MessageSend(
                connectionGetCmTask(),
                CL_INTERNAL_DM_BLE_PERIODIC_SCAN_START_FIND_TRAINS_REQ,
                message);
}

/****************************************************************************
NAME
    connectionHandleDmBlePeriodicScanStartFindTrainsReq

DESCRIPTION
    This function will initiate a Periodic Scanning Start Find Trains request.

RETURNS
    void
*/
void connectionHandleDmBlePeriodicScanStartFindTrainsReq(connectionDmPerScanState *state,
        const CL_INTERNAL_DM_BLE_PERIODIC_SCAN_START_FIND_TRAINS_REQ_T *req
        )
{
    /* Check the state of the task lock before doing anything. */
    if (!state->dmPerScanLock)
    {
        uint8 i;

        MAKE_PRIM_T(DM_ULP_PERIODIC_SCAN_START_FIND_TRAINS_REQ);

        /* One request at a time, set the lock. */
        state->dmPerScanLock    = req->theAppTask;

        prim->flags                          = req->flags;
        prim->scan_for_x_seconds             = req->scan_for_x_seconds;
        prim->ad_structure_filter            = req->ad_structure_filter;
        prim->ad_structure_filter_sub_field1 = req->ad_structure_filter_sub_field1;
        prim->ad_structure_filter_sub_field2 = req->ad_structure_filter_sub_field2;
        prim->ad_structure_info_len          = req->ad_structure_info_len;

        for (i = 0; i < CL_AD_STRUCT_INFO_BYTE_PTRS; i++)
        {
            prim->ad_structure_info[i] = req->ad_structure_info[i];
        }


        VmSendDmPrim(prim);
    }
    else
    {
        /* Per Scan request already outstanding, queue up this request. */
        MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_PERIODIC_SCAN_START_FIND_TRAINS_REQ);
        COPY_CL_MESSAGE(req, message);
        MessageSendConditionallyOnTask(
                    connectionGetCmTask(),
                    CL_INTERNAL_DM_BLE_PERIODIC_SCAN_START_FIND_TRAINS_REQ,
                    message,
                    &state->dmPerScanLock
                    );
    }
}

/****************************************************************************
NAME
    connectionHandleDmBlePeriodicScanStartFindTrainsCfm

DESCRIPTION
    Handles status of Periodic Scanning Start Find Trains request.

RETURNS
    void
*/
void connectionHandleDmBlePeriodicScanStartFindTrainsCfm(connectionDmPerScanState *state,
    const DM_ULP_PERIODIC_SCAN_START_FIND_TRAINS_CFM_T *cfm)
{
    if (state->dmPerScanLock)
    {
        MAKE_CL_MESSAGE(CL_DM_BLE_PERIODIC_SCAN_START_FIND_TRAINS_CFM);

        message->status = (cfm->status) ? fail : success;
        message->scan_handle = cfm->scan_handle;

        MessageSend(
                state->dmPerScanLock,
                CL_DM_BLE_PERIODIC_SCAN_START_FIND_TRAINS_CFM,
                message
                );

        if (cfm->status == HCI_SUCCESS)
        {
            if (task_scan_train_handle_index < MAX_TRAIN_SCAN_HANDLES)
            {
                task_scan_train_handles[task_scan_train_handle_index].registering_task = state->dmPerScanLock;
                task_scan_train_handles[task_scan_train_handle_index].scan_handle = cfm->scan_handle;
                task_scan_train_handle_index++;
            }
            else
            {
                DEBUG_LOG_WARN("connectionHandleDmBlePeriodicScanStartFindTrainsCfm. Maximum number of registered scanners reached.");
            }
        }
    }

    /* Reset the lock. */
    state->dmPerScanLock = NULL;
}

/****************************************************************************
NAME
    ConnectionDmBlePeriodicScanStopFindTrainsReq

DESCRIPTION
    Stop scanning for periodic trains.

RETURNS
    void
*/
void ConnectionDmBlePeriodicScanStopFindTrainsReq(
    Task    theAppTask,
    uint8   scan_handle)
{
    /* Verify that the task is associated with the scanner it's trying to
     * unregister. */
    if (task_scan_train_handle_index > 0)
    {
        uint8 i;

        for (i = 0; i < task_scan_train_handle_index; i++)
        {
            if (scan_handle == task_scan_train_handles[i].scan_handle)
            {
                if (theAppTask == task_scan_train_handles[i].registering_task)
                {
                    MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_PERIODIC_SCAN_STOP_FIND_TRAINS_REQ);
                    
                    message->theAppTask     = theAppTask;
                    message->scan_handle    = scan_handle;
                    
                    MessageSend(connectionGetCmTask(),
                                CL_INTERNAL_DM_BLE_PERIODIC_SCAN_STOP_FIND_TRAINS_REQ,
                                message);
                    return;
                }
                else
                {
                    CL_DEBUG_INFO(("Requesting task is not associated with the scanner it tried to terminate."));
                    Panic();
                }
            }
        }
        CL_DEBUG_INFO(("Scan handle requested for removal not found."));
    }
    else
    {
        CL_DEBUG_INFO(("There are no more scanners to unregister."));
    }

    MESSAGE_MAKE(confirm, CL_DM_BLE_PERIODIC_SCAN_STOP_FIND_TRAINS_CFM_T);
    confirm->status == fail;
    MessageSend(theAppTask, CL_DM_BLE_PERIODIC_SCAN_STOP_FIND_TRAINS_CFM, confirm);
}

/****************************************************************************
NAME
    connectionHandleDmBlePeriodicScanStopFindTrainsReq

DESCRIPTION
    This function will initiate a Periodic Scanning Stop Find Trains request.

RETURNS
    void
*/
void connectionHandleDmBlePeriodicScanStopFindTrainsReq(connectionDmPerScanState *state,
        const CL_INTERNAL_DM_BLE_PERIODIC_SCAN_STOP_FIND_TRAINS_REQ_T *req
        )
{
    /* Check the state of the task lock before doing anything. */
    if (!state->dmPerScanLock)
    {
        MAKE_PRIM_T(DM_ULP_PERIODIC_SCAN_STOP_FIND_TRAINS_REQ);

        /* One request at a time, set the lock. */
        state->dmPerScanLock    = req->theAppTask;

        prim->scan_handle       = req->scan_handle;

        VmSendDmPrim(prim);
    }
    else
    {
        /* Per Scan request already outstanding, queue up this request. */
        MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_PERIODIC_SCAN_STOP_FIND_TRAINS_REQ);
        COPY_CL_MESSAGE(req, message);
        MessageSendConditionallyOnTask(
                    connectionGetCmTask(),
                    CL_INTERNAL_DM_BLE_PERIODIC_SCAN_STOP_FIND_TRAINS_REQ,
                    message,
                    &state->dmPerScanLock
                    );
    }
}

/****************************************************************************
NAME
    connectionHandleDmBlePeriodicScanStopFindTrainsCfm

DESCRIPTION
    Handles status of Periodic Scanning Stop Find Trains request.

RETURNS
    void
*/
void connectionHandleDmBlePeriodicScanStopFindTrainsCfm(connectionDmPerScanState *state,
    const DM_ULP_PERIODIC_SCAN_STOP_FIND_TRAINS_CFM_T *cfm)
{
    if (state->dmPerScanLock)
    {
        uint8 i;
        bool task_found = FALSE;

        /* Remove the task-handle association from the tracking struct and shift
         * all remaining pairs after it one position down. */
        for(i = 0; i < task_scan_train_handle_index; i++)
        {
            if (task_found)
            {
                task_scan_train_handles[i-1] = task_scan_train_handles[i];
            }
            else if (state->dmPerScanLock == task_scan_train_handles[i].registering_task)
            {
                task_found = TRUE;
            }
        }


        if (task_found)
        {
            /* Finally, clear the last element and decrement the index. */
            task_scan_train_handle_index -= 1;
            memset(&task_scan_train_handles[task_scan_train_handle_index], 0, sizeof(task_scan_handles_pair));
        }
        else
        {
            CL_DEBUG_INFO(("state->dmPerScanLock was not found in task_scan_train_handles.\n"));
            return;

        }

        {
            MAKE_CL_MESSAGE(CL_DM_BLE_PERIODIC_SCAN_STOP_FIND_TRAINS_CFM);
            message->status = (cfm->status) ? fail : success;

            MessageSend(
                    state->dmPerScanLock,
                    CL_DM_BLE_PERIODIC_SCAN_STOP_FIND_TRAINS_CFM,
                    message
                    );
        }
    }

    /* Reset the lock. */
    state->dmPerScanLock = NULL;
}

/****************************************************************************
NAME
    ConnectionDmBlePeriodicScanSyncAdvReportEnableReq

DESCRIPTION
    Sets whether a DM_ULP_PERIODIC_SCAN_SYNC_ADV_REPORT_IND should be reported
    for a synced periodic train.

RETURNS
    void
*/
void ConnectionDmBlePeriodicScanSyncAdvReportEnableReq(
    Task    theAppTask,
    uint16  sync_handle,
    uint8   enable)
{
    MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_PERIODIC_SCAN_SYNC_ADV_REPORT_ENABLE_REQ);

    message->theAppTask     = theAppTask;
    message->sync_handle    = sync_handle;
    message->enable         = enable;

    MessageSend(
                connectionGetCmTask(),
                CL_INTERNAL_DM_BLE_PERIODIC_SCAN_SYNC_ADV_REPORT_ENABLE_REQ,
                message);
}

/****************************************************************************
NAME
    connectionHandleDmBlePeriodicScanSyncAdvReportEnableReq

DESCRIPTION
    This function will initiate a Periodic Scanning Sync Advertising Report Enable request.

RETURNS
    void
*/
void connectionHandleDmBlePeriodicScanSyncAdvReportEnableReq(connectionDmPerScanState *state,
        const CL_INTERNAL_DM_BLE_PERIODIC_SCAN_SYNC_ADV_REPORT_ENABLE_REQ_T *req
        )
{
    /* Check the state of the task lock before doing anything. */
    if (!state->dmPerScanLock)
    {
        MAKE_PRIM_T(DM_ULP_PERIODIC_SCAN_SYNC_ADV_REPORT_ENABLE_REQ);

        /* One request at a time, set the lock. */
        state->dmPerScanLock    = req->theAppTask;

        prim->sync_handle   = req->sync_handle;
        prim->enable        = req->enable;

        VmSendDmPrim(prim);
    }
    else
    {
        /* Per Scan request already outstanding, queue up this request. */
        MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_PERIODIC_SCAN_SYNC_ADV_REPORT_ENABLE_REQ);
        COPY_CL_MESSAGE(req, message);
        MessageSendConditionallyOnTask(
                    connectionGetCmTask(),
                    CL_INTERNAL_DM_BLE_PERIODIC_SCAN_SYNC_ADV_REPORT_ENABLE_REQ,
                    message,
                    &state->dmPerScanLock
                    );
    }
}

/****************************************************************************
NAME
    connectionHandleDmBlePeriodicScanSyncAdvReportEnableCfm

DESCRIPTION
    Handles status of Periodic Scanning Sync Advertising Report Enable request.

RETURNS
    void
*/
void connectionHandleDmBlePeriodicScanSyncAdvReportEnableCfm(connectionDmPerScanState *state,
    const DM_ULP_PERIODIC_SCAN_SYNC_ADV_REPORT_ENABLE_CFM_T *cfm)
{
    if (state->dmPerScanLock)
    {
        MAKE_CL_MESSAGE(CL_DM_BLE_PERIODIC_SCAN_SYNC_ADV_REPORT_ENABLE_CFM);

        message->status = (cfm->status) ? fail : success;

        MessageSend(
                state->dmPerScanLock,
                CL_DM_BLE_PERIODIC_SCAN_SYNC_ADV_REPORT_ENABLE_CFM,
                message
                );
    }

    /* Reset the lock. */
    state->dmPerScanLock = NULL;
}

/****************************************************************************
NAME
    connectionHandleDmBlePeriodicScanSyncLostInd

DESCRIPTION
    Sync lost to periodic train.

RETURNS
    void
*/
void connectionHandleDmBlePeriodicScanSyncLostInd(
        const DM_ULP_PERIODIC_SCAN_SYNC_LOST_IND_T *ind
        )
{
    uint8 i;
    Task associated_task = 0;

    for (i = 0; i < task_sync_handle_index; i++)
    {
        if (ind->sync_handle == task_sync_handles[i].sync_handle)
        {
            associated_task = task_sync_handles[i].registering_task;
        }
    }

    if (associated_task)
    {
        MAKE_CL_MESSAGE(CL_DM_BLE_PERIODIC_SCAN_SYNC_LOST_IND);
        message->sync_handle = ind->sync_handle;
        MessageSend(
                associated_task,
                CL_DM_BLE_PERIODIC_SCAN_SYNC_LOST_IND,
                message
                );
    }
    else
    {
        CL_DEBUG(("The sync_handle on which sync was lost was not found."))
    }

}

/****************************************************************************
NAME
    ConnectionDmBlePeriodicScanSyncLostRsp

DESCRIPTION
    Response that the application has stopped reading periodic train adv data
    for this train.

RETURNS
    void
*/
void ConnectionDmBlePeriodicScanSyncLostRsp(uint16 sync_handle)
{
    if (task_sync_handle_index > 0)
    {
        uint8 i = 0;
        bool handle_found = FALSE;

        /* Remove the task-handle association from the tracking struct by shifting
         * all remaining elements down one position.
         */
        for (i = 0; i < task_sync_handle_index; i++)
        {
            if (handle_found)
            {
                task_sync_handles[i-1] = task_sync_handles[i];
            }
            else if (task_sync_handles[i].sync_handle == sync_handle)
            {
                handle_found = TRUE;
            }
        }

        if (handle_found)
        {
            /* Finally, decrement the index and clear the last entry. */
            task_sync_handle_index -= 1;
            memset(&task_sync_handles[task_sync_handle_index], 0, sizeof(task_sync_handles_pair));
        }
        else
        {
            CL_DEBUG(("Sync handle requested for removal not found."));
            return;
        }
    }
    else
    {
        CL_DEBUG_INFO(("There are no more sync'ed trains to terminate."));
        return;
    }

    MAKE_PRIM_T(DM_ULP_PERIODIC_SCAN_SYNC_LOST_RSP);

    prim->sync_handle = sync_handle;

    VmSendDmPrim(prim);
}

/****************************************************************************
NAME
    connectionHandleDmBlePeriodicScanSyncTransferInd

DESCRIPTION
    An indication sent to the Profile/Application following an attempt by the
    local Controller to synchronize to a periodic advertising train.

RETURNS
    void
*/
void connectionHandleDmBlePeriodicScanSyncTransferInd(
        const DM_ULP_PERIODIC_SCAN_SYNC_TRANSFER_IND_T *ind
        )
{
    MAKE_CL_MESSAGE(CL_DM_BLE_PERIODIC_SCAN_SYNC_TRANSFER_IND);

    message->status = (ind->status) ? fail : success;
    message->adv_sid = ind->adv_sid;
    message->sync_handle = ind->sync_handle;

    /* Store and associate the returned sync_handle and its connected stream
     * with the requesting task. If this would mean more than the maximum
     * number of scanners and conn lib is in debug mode, panic, as the
     * request should have been rejected. */
    if (!ind->status)
    {
        if (task_sync_handle_index < MAX_SYNC_HANDLES)
        {
            task_sync_handles[task_sync_handle_index].registering_task = connectionGetAppTask();
            task_sync_handles[task_sync_handle_index].sync_handle = ind->sync_handle;
            task_sync_handles[task_sync_handle_index].source = StreamPeriodicScanSource(ind->sync_handle);
            MessageStreamTaskFromSource(task_sync_handles[task_sync_handle_index].source, connectionGetCmTask());
        }
        else
        {
            CL_DEBUG(("Maximum number of periodic trains sync'ed reached."));
            return;
        }

        task_sync_handle_index++;
    }

    message->service_data = ind->service_data;
    BdaddrConvertTypedBluestackToVm(&message->adv_addr, &ind->adv_addr);

    MessageSend(
            connectionGetAppTask(),
            CL_DM_BLE_PERIODIC_SCAN_SYNC_TRANSFER_IND,
            message
            );
}

/****************************************************************************
NAME
    ConnectionUpdateTaskToSyncHandleAssociation

DESCRIPTION
    Allows the application to update an existing association between a sync_handle
    and its task, effectively switching which task the sync reports get routed
    to, as well as being allowed to terminate that sync.

RETURNS
    connection_lib_status - A fail would mean the provided sync_handle was not found.
*/
connection_lib_status ConnectionUpdateTaskToSyncHandleAssociation(uint16 sync_handle, Task theAppTask)
{
    uint8 i;

    for (i = 0; i < task_sync_handle_index; i++)
    {
        if (sync_handle == task_sync_handles[i].sync_handle)
        {
            task_sync_handles[i].registering_task = theAppTask;
            return success;
        }
    }

    /* Provided sync_handle was not found; return a fail result. */
    return fail;
}

/****************************************************************************
NAME
    connectionHandleDmBlePeriodicScanSyncAdvReportInd

DESCRIPTION
    Handles BLE Periodic Scanning Sync Advertising report indication

RETURNS
    void
*/
void connectionHandleDmBlePeriodicScanSyncAdvReportInd(const DM_ULP_PERIODIC_SCAN_SYNC_ADV_REPORT_IND_T *ind)
{
    uint8 i;
    Source source = 0;
    Task receiving_task = 0;

    MAKE_CL_MESSAGE(CL_DM_BLE_PERIODIC_SCAN_SYNC_ADV_REPORT_IND);

    message->sync_handle    = ind->sync_handle;
    message->tx_power       = ind->tx_power;
    message->rssi           = ind->rssi;
    message->cte_type       = ind->cte_type;

    for (i = 0; i < MAX_SYNC_HANDLES; i++)
    {
        if (ind->sync_handle == task_sync_handles[i].sync_handle)
        {
            source = task_sync_handles[i].source;
            receiving_task = task_sync_handles[i].registering_task;
            break;
        }
    }

    if (source)
    {
        message->adv_data_len   = SourceBoundary(source);
        message->adv_data       = SourceMap(source);
    }
    else
    {
        CL_DEBUG(("Sync handle for incoming adv report not found."));
        return;
    }
    /* Cancel any pending incoming adv report */
    MessageCancelFirst(receiving_task, CL_DM_BLE_PERIODIC_SCAN_SYNC_ADV_REPORT_IND);

    MessageSend(
            receiving_task,
            CL_DM_BLE_PERIODIC_SCAN_SYNC_ADV_REPORT_IND,
            message
            );

    /* Send a message to conn lib to indicate that the relevant task has been
     * notified. This will only be processed after the task has returned from
     * their handling function, and thus signals the conn lib that it is safe to
     * free the underlying memory in the stream. */

    CL_INTERNAL_DM_BLE_PER_SCAN_ADV_REPORT_DONE_IND_T * const msg = PanicUnlessNew(CL_INTERNAL_DM_BLE_PER_SCAN_ADV_REPORT_DONE_IND_T);

    msg->size = SourceBoundary(source);
    msg->source = source;

    MessageSend(connectionGetCmTask(),
                CL_INTERNAL_DM_BLE_PER_SCAN_ADV_REPORT_DONE_IND,
                msg);
}

/****************************************************************************
NAME
    connectionHandleDmBlePerScanAdvReportDoneInd

DESCRIPTION
    Once all the associated task have processed the BLE Periodic Scanning Sync
    Advertising report indication, clear the report from the incoming stream.

RETURNS
    void
*/
void connectionHandleDmBlePerScanAdvReportDoneInd(const CL_INTERNAL_DM_BLE_PER_SCAN_ADV_REPORT_DONE_IND_T *ind)
{
    SourceDrop(ind->source, ind->size);

    /* Check if the stream has more data. */
    /* SourceSizeHeader() will return non zero if there are any messages to read from the source.
       SourceSize() and SourceBoundary()are not a reliable way to determine if the source is empty
       as they will return zero if there are messages in the source with zero length data.*/
    if (SourceSizeHeader(ind->source) != 0)
    {
        MAKE_CL_MESSAGE(CL_INTERNAL_MESSAGE_MORE_DATA);

        message->source = ind->source;
        message->stream_type = per_scan_stream;

        MessageSend(connectionGetCmTask(),
                CL_INTERNAL_MESSAGE_MORE_DATA,
                message);
    }
}

/****************************************************************************
NAME
    connectionHandleMoreData

DESCRIPTION
    Handles MESSAGE_MORE_DATA from streams connected to the Connection library.
    Determines which stream (Periodic or Extended currently) the message is
    from, extracts the data from it and routes it to the relevant handler.

RETURNS
    void
*/
void connectionHandleMoreData(Source src, conn_lib_stream_types stream_type)
{
    uint8 i;
    conn_lib_stream_types strm_type = unidentified_stream;
    uint8 prim_size = 0;
    const void *prim_ptr = 0;

    /* Check if the stream actually has more data in. */
    if (SourceSize(src) != 0)
    {
        prim_size = SourceSizeHeader(src);
        prim_ptr = SourceMapHeader(src);

        if (prim_size == 0)
        {
            CL_DEBUG_INFO(("Incoming message has no header (size).\n"));
        }
        else if (prim_ptr == 0)
        {
            CL_DEBUG_INFO(("Incoming message has no header (location).\n"));
        }

        /* Determine which stream this is from. */
        if (stream_type == unidentified_stream)
        {
            if (src == ExtScanSrc)
            {
                strm_type = ext_scan_stream;
            }
            else
            {
                for (i = 0; i < task_sync_handle_index; i++)
                {
                    if (src == task_sync_handles[i].source)
                    {
                        strm_type = per_scan_stream;
                        break;
                    }
                }
            }
        }

        switch(strm_type) {
            case ext_scan_stream:
            {
                MAKE_PRIM_T(DM_ULP_EXT_SCAN_FILTERED_ADV_REPORT_IND);

                PanicFalse(prim_size <= sizeof(DM_ULP_EXT_SCAN_FILTERED_ADV_REPORT_IND_T));
                memmove(prim, prim_ptr, prim_size);

                /* Send the DM prim under an internal message type instead of
                 * its normal DM PRIM type to avoid causing memory freeing
                 * underflows in P0. */
                MessageSend(connectionGetCmTask(),
                            CL_INTERNAL_DM_BLE_EXT_SCAN_FILTERED_ADV_REPORT_IND,
                            prim);
                break;
            }
            case per_scan_stream:
            {
                MAKE_PRIM_T(DM_ULP_PERIODIC_SCAN_SYNC_ADV_REPORT_IND);
                PanicFalse(prim_size <= sizeof(DM_ULP_PERIODIC_SCAN_SYNC_ADV_REPORT_IND_T));

                memmove(prim, prim_ptr, prim_size);

                /* Send the DM prim under an internal message type instead of
                 * its normal DM PRIM type to avoid causing memory freeing
                 * underflows in P0. */
                MessageSend(connectionGetCmTask(),
                            CL_INTERNAL_DM_BLE_PERIODIC_SCAN_SYNC_ADV_REPORT_IND,
                            prim);
                break;
            }
            default:
                CL_DEBUG(("Stream notification for more data from unrecognised stream received."));
        }
    }
    else
    {
        CL_DEBUG(("No more data in the received stream.\n"));
    }
}
#endif /* CL_EXCLUDE_ISOC */

#endif /* DISABLE_BLE */

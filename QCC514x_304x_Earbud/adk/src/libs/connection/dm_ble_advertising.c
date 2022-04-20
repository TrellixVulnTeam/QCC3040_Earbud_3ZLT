/****************************************************************************
Copyright (c) 2011 - 2021 Qualcomm Technologies International, Ltd.


FILE NAME
    dm_ble_scanning.c

DESCRIPTION
    This file contains the implementation of Low Energy scan configuration.

NOTES

*/

#include "connection.h"
#include "connection_private.h"
#include "common.h"
#include "bdaddr.h"
#include "dm_ble_advertising.h"

#include <vm.h>

#ifndef DISABLE_BLE

#if HYDRACORE
#define NO_CFM_MESSAGE ((Task)0x0FFFFFFF)
#else
#define NO_CFM_MESSAGE ((Task)0x0000FFFF)
#endif

/****************************************************************************
NAME
    ConnectionDmBleSetAdvertisingDataReq

DESCRIPTION
    Sets BLE Advertising data (0..31 octets).

RETURNS
   void
*/
void ConnectionDmBleSetAdvertisingDataReq(uint8 size_ad_data, const uint8 *ad_data)
{

#ifdef CONNECTION_DEBUG_LIB
        /* Check parameters. */
    if (size_ad_data == 0 || size_ad_data > BLE_AD_PDU_SIZE)
    {
        CL_DEBUG(("Pattern length is zero\n"));
    }
    if (ad_data == 0)
    {
        CL_DEBUG(("Pattern is null\n"));
    }
#endif
    {
        MAKE_PRIM_C(DM_HCI_ULP_SET_ADVERTISING_DATA_REQ);
        prim->advertising_data_len = size_ad_data;
        memmove(prim->advertising_data, ad_data, size_ad_data);
        VmSendDmPrim(prim);
    }
}

/****************************************************************************
NAME
    ConnectionDmBleSetAdvertiseEnable

DESCRIPTION
    Enable or Disable BLE Advertising.

RETURNS
   void
*/
void ConnectionDmBleSetAdvertiseEnable(bool enable)
{
    ConnectionDmBleSetAdvertiseEnableReq(NO_CFM_MESSAGE, enable);
}

/****************************************************************************
NAME
    ConnectionDmBleSetAdvertiseEnableReq

DESCRIPTION
    Enables or disables BLE Advertising. If theAppTask is anthing other than
    NULL(0) then that is treated as the task to return the CFM message to.

RETURNS
   void
*/
void ConnectionDmBleSetAdvertiseEnableReq(Task theAppTask, bool enable)
{
    MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_SET_ADVERTISE_ENABLE_REQ);
    message->theAppTask = theAppTask;
    message->enable = enable;
    MessageSend(
                connectionGetCmTask(),
                CL_INTERNAL_DM_BLE_SET_ADVERTISE_ENABLE_REQ,
                message);
}

/****************************************************************************
NAME
    connectionHandleDmBleSetAdvertiseEnableReq.

DESCRIPTION
    This function will initiate an Advertising Enable request.

RETURNS
   void
*/
void connectionHandleDmBleSetAdvertiseEnableReq(
        connectionBleScanAdState *state,
        const CL_INTERNAL_DM_BLE_SET_ADVERTISE_ENABLE_REQ_T *req
        )
 {
    /* Check the state of the task lock before doing anything. */
    if (!state->bleScanAdLock)
    {
        MAKE_PRIM_C(DM_HCI_ULP_SET_ADVERTISE_ENABLE_REQ);

        /* One request at a time, set the ad lock. */
        state->bleScanAdLock = req->theAppTask;

        prim->advertising_enable = (req->enable) ? 1 : 0;

        VmSendDmPrim(prim);
    }
    else
    {
        /* Scan or Ad request already outstanding, queue up this request. */
        MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_SET_ADVERTISE_ENABLE_REQ);
        COPY_CL_MESSAGE(req, message);
        MessageSendConditionallyOnTask(
                    connectionGetCmTask(),
                    CL_INTERNAL_DM_BLE_SET_ADVERTISE_ENABLE_REQ,
                    message,
                    &state->bleScanAdLock
                    );
    }
}

/****************************************************************************
NAME
    connectionHandleDmBleSetAdvertiseEnableCfm

DESCRIPTION
    Handle the DM_HCI_ULP_SET_ADVERTISE_ENABLE_CFM from Bluestack.

RETURNS
   void
*/
void connectionHandleDmBleSetAdvertiseEnableCfm(
        connectionBleScanAdState *state,
        const DM_HCI_ULP_SET_ADVERTISE_ENABLE_CFM_T *cfm
        )
{
    if (state->bleScanAdLock != NO_CFM_MESSAGE)
    {
        MAKE_CL_MESSAGE(CL_DM_BLE_SET_ADVERTISE_ENABLE_CFM);
        message->status = connectionConvertHciStatus(cfm->status);
        MessageSend(
                    state->bleScanAdLock,
                    CL_DM_BLE_SET_ADVERTISE_ENABLE_CFM,
                    message
                    );
    }

    /* Reset the ad lock. */
    state->bleScanAdLock = NULL;
}


/****************************************************************************
NAME
    ConnectionDmBleSetAdvertisingParametersReq

DESCRIPTION
    Sets BLE Advertising parameters

RETURNS
   void
*/
void ConnectionDmBleSetAdvertisingParamsReq(
        ble_adv_type adv_type,
        uint8 own_address,
        uint8 channel_map,
        const ble_adv_params_t *adv_params
        )
{
    MAKE_PRIM_C(DM_HCI_ULP_SET_ADVERTISING_PARAMETERS_REQ);

    /* Set defaults to avoid HCI validation failures */
    prim->direct_address_type = HCI_ULP_ADDRESS_PUBLIC;
    prim->adv_interval_max = 0x0800; /* 1.28s */
    prim->adv_interval_min = 0x0800;
    prim->advertising_filter_policy = HCI_ULP_ADV_FP_ALLOW_ANY;

    switch(adv_type)
    {
    case ble_adv_ind:
        prim->advertising_type =
                HCI_ULP_ADVERT_CONNECTABLE_UNDIRECTED;
        break;
    case ble_adv_direct_ind:
    case ble_adv_direct_ind_high_duty:
        prim->advertising_type =
                HCI_ULP_ADVERT_CONNECTABLE_DIRECTED_HIGH_DUTY;
        break;
    case ble_adv_scan_ind:
        prim->advertising_type =
                HCI_ULP_ADVERT_DISCOVERABLE;
        break;
    case ble_adv_nonconn_ind:
        prim->advertising_type =
                HCI_ULP_ADVERT_NON_CONNECTABLE;
        break;
    case ble_adv_direct_ind_low_duty:
        prim->advertising_type =
                HCI_ULP_ADVERT_CONNECTABLE_DIRECTED_LOW_DUTY;
        break;
    }

    prim->own_address_type = connectionConvertOwnAddress(own_address);

    channel_map &= BLE_ADV_CHANNEL_ALL;

    prim->advertising_channel_map = channel_map & BLE_ADV_CHANNEL_ALL;

    if (adv_type ==  ble_adv_direct_ind ||
            adv_type ==  ble_adv_direct_ind_high_duty ||
            adv_type ==  ble_adv_direct_ind_low_duty )
    {
        /* Without an address, this cannot proceed. */
        if (
                !adv_params ||
                BdaddrIsZero(&adv_params->direct_adv.direct_addr)
                )
            Panic();

        /* Use the ble_directed_adv_params_t type of the union for all
             * 'direct' advertising params, as it is same as the
             * ble_directed_low_duty_adv_params_t type for the first two
             * elements.
             */

        prim->direct_address_type =
                (adv_params->low_duty_direct_adv.random_direct_address) ?
                    HCI_ULP_ADDRESS_RANDOM : HCI_ULP_ADDRESS_PUBLIC;

        BdaddrConvertVmToBluestack(
                    &prim->direct_address,
                    &adv_params->low_duty_direct_adv.direct_addr
                    );

        if (adv_type == ble_adv_direct_ind_low_duty)
        {
            prim->adv_interval_min = adv_params->low_duty_direct_adv.adv_interval_min;
            prim->adv_interval_max = adv_params->low_duty_direct_adv.adv_interval_max;
        }
    }
    else
    {
        if (adv_params)
        {
            /* These params are validated by HCI. */
            prim->adv_interval_min
                    = adv_params->undirect_adv.adv_interval_min;
            prim->adv_interval_max
                    = adv_params->undirect_adv.adv_interval_max;

            switch (adv_params->undirect_adv.filter_policy)
            {
            case ble_filter_none:
                prim->advertising_filter_policy =
                        HCI_ULP_ADV_FP_ALLOW_ANY;
                break;
            case ble_filter_scan_only:
                prim->advertising_filter_policy =
                        HCI_ULP_ADV_FP_ALLOW_CONNECTIONS;
                break;
            case ble_filter_connect_only:
                prim->advertising_filter_policy =
                        HCI_ULP_ADV_FP_ALLOW_SCANNING;
                break;
            case ble_filter_both:
                prim->advertising_filter_policy =
                        HCI_ULP_ADV_FP_ALLOW_WHITELIST;
                break;
            }

            /* Set the direct address & type to 0, as they are not used. */
            prim->direct_address_type = 0;
            BdaddrSetZero(&prim->direct_address);
        }
        /* otherwise, if 'adv_params' is null, defaults are used. */
    }

    VmSendDmPrim(prim);
}


/****************************************************************************
NAME
    connectionHandleDmBleAdvParamUpdateInd

DESCRIPTION
    Handle the DM_ULP_ADV_PARAM_UPDATE_IND message from Bluestack and pass it
    on to the appliction that initialised the CL.

RETURNS
    void
*/
void connectionHandleDmBleAdvParamUpdateInd(
        const DM_ULP_ADV_PARAM_UPDATE_IND_T *ind
        )
{
    MAKE_CL_MESSAGE(CL_DM_BLE_ADVERTISING_PARAM_UPDATE_IND);

    message->adv_interval_min = ind->adv_interval_min;
    message->adv_interval_max = ind->adv_interval_max;

    switch(ind->advertising_type)
    {
        case HCI_ULP_ADVERT_CONNECTABLE_UNDIRECTED:
            message->advertising_type = ble_adv_ind;
            break;
        case HCI_ULP_ADVERT_CONNECTABLE_DIRECTED_HIGH_DUTY:
            message->advertising_type = ble_adv_direct_ind_high_duty;
            break;
        case HCI_ULP_ADVERT_DISCOVERABLE:
            message->advertising_type = ble_adv_scan_ind;
            break;
        case HCI_ULP_ADVERT_NON_CONNECTABLE:
            message->advertising_type = ble_adv_nonconn_ind;
            break;
        case HCI_ULP_ADVERT_CONNECTABLE_DIRECTED_LOW_DUTY:
            message->advertising_type = ble_adv_direct_ind_low_duty;
            break;
        default:
            CL_DEBUG((
                "Received unknown advertising type: %d\n",
                ind->advertising_type
                ));
            message->advertising_type = 0xFF;
            break;
    }

    message->own_address_type = ind->own_address_type;
    message->direct_address_type = ind->direct_address_type;
    BdaddrConvertBluestackToVm(&message->direct_bd_addr, &ind->direct_address);
    message->advertising_channel_map = ind->advertising_channel_map;

    switch (ind->advertising_filter_policy)
    {
        case HCI_ULP_ADV_FP_ALLOW_ANY:
            message->advertising_filter_policy = ble_filter_none;
            break;
        case HCI_ULP_ADV_FP_ALLOW_CONNECTIONS:
            message->advertising_filter_policy = ble_filter_scan_only;
            break;
        case HCI_ULP_ADV_FP_ALLOW_SCANNING:
            message->advertising_filter_policy = ble_filter_connect_only;
            break;
        case HCI_ULP_ADV_FP_ALLOW_WHITELIST:
            message->advertising_filter_policy = ble_filter_both;
            break;
        default:
            CL_DEBUG((
                "Received unknown advertising filter policy: %d\n",
                ind->advertising_type
                ));
            message->advertising_filter_policy = 0xFF;
            break;
    }

    MessageSend(
            connectionGetAppTask(),
            CL_DM_BLE_ADVERTISING_PARAM_UPDATE_IND,
            message);
}

/*****************************************************************************
 *                  Extended Advertising functions                           *
 *****************************************************************************/

/****************************************************************************
NAME
    ConnectionDmBleGetAdvScanCapabilitiesReq

DESCRIPTION
    Supplies information on what APIs are available and size limitations.

RETURNS
   void
*/
void ConnectionDmBleGetAdvScanCapabilitiesReq(Task theAppTask)
{
    MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_GET_ADV_SCAN_CAPABILITIES_REQ);

    message->theAppTask = theAppTask;

    MessageSend(
                connectionGetCmTask(),
                CL_INTERNAL_DM_BLE_GET_ADV_SCAN_CAPABILITIES_REQ,
                message);
}

/****************************************************************************
NAME
    connectionHandleDmBleeGetAdvScanCapabilitiesReq

DESCRIPTION
    This function will initiate an Get Advertising/Scanning Capabilities request.

RETURNS
   void
*/
void connectionHandleDmBleGetAdvScanCapabilitiesReq(
        connectionDmExtAdvState *state,
        const CL_INTERNAL_DM_BLE_GET_ADV_SCAN_CAPABILITIES_REQ_T *req
        )
{
    /* Check the state of the task lock before doing anything. */
    if (!state->dmExtAdvLock)
    {
        MAKE_PRIM_T(DM_ULP_GET_ADV_SCAN_CAPABILITIES_REQ);

        /* One request at a time, set the ad lock. */
        state->dmExtAdvLock     = req->theAppTask;

        VmSendDmPrim(prim);
    }
    else
    {
        /* Ext Adv request already outstanding, queue up this request. */
        MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_GET_ADV_SCAN_CAPABILITIES_REQ);
        COPY_CL_MESSAGE(req, message);
        MessageSendConditionallyOnTask(
                    connectionGetCmTask(),
                    CL_INTERNAL_DM_BLE_GET_ADV_SCAN_CAPABILITIES_REQ,
                    message,
                    &state->dmExtAdvLock
                    );
    }
}

/****************************************************************************
NAME
    connectionHandleDmBleExtAdvSetsInfoCfm

DESCRIPTION
    Handles status of an Get Advertising/Scanning Capabilities Request

RETURNS
    void
*/
void connectionHandleDmBleGetAdvScanCapabilitiesCfm(
        connectionDmExtAdvState *state,
        const DM_ULP_GET_ADV_SCAN_CAPABILITIES_CFM_T *cfm
        )
{
    if (state->dmExtAdvLock)
    {
        MAKE_CL_MESSAGE(CL_DM_BLE_GET_ADV_SCAN_CAPABILITIES_CFM);

        message->status = (cfm->status) ? fail : success;
        message->available_api = cfm->available_api;
        message->available_adv_sets = cfm->available_adv_sets;
        message->stack_reserved_adv_sets = cfm->stack_reserved_adv_sets;
        message->max_periodic_sync_list_size = cfm->max_periodic_sync_list_size;
        message->supported_phys = cfm->supported_phys;
        message->max_potential_size_of_tx_adv_data = cfm->max_potential_size_of_tx_adv_data;
        message->max_potential_size_of_tx_periodic_adv_data = cfm->max_potential_size_of_tx_periodic_adv_data;
        message->max_potential_size_of_rx_adv_data = cfm->max_potential_size_of_rx_adv_data;
        message->max_potential_size_of_rx_periodic_adv_data = cfm->max_potential_size_of_rx_periodic_adv_data;

        MessageSend(
                    state->dmExtAdvLock,
                    CL_DM_BLE_GET_ADV_SCAN_CAPABILITIES_CFM,
                    message
                    );
    }

    /* Reset the ad lock. */
    state->dmExtAdvLock = NULL;
}

/****************************************************************************
NAME
    ConnectionDmBleExtAdvSetsInfoReq

DESCRIPTION
    Reports information about all advertising sets (e.g. advertising/registered).

RETURNS
   void
*/
void ConnectionDmBleExtAdvSetsInfoReq(Task theAppTask)
{
    MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_EXT_ADV_SETS_INFO_REQ);

    message->theAppTask = theAppTask;

    MessageSend(
                connectionGetCmTask(),
                CL_INTERNAL_DM_BLE_EXT_ADV_SETS_INFO_REQ,
                message);
}

/****************************************************************************
NAME
    connectionHandleDmBleExtAdvSetsInfoReq

DESCRIPTION
    This function will initiate an Extended Advertising Sets Info request.

RETURNS
   void
*/
void connectionHandleDmBleExtAdvSetsInfoReq(
        connectionDmExtAdvState *state,
        const CL_INTERNAL_DM_BLE_EXT_ADV_SETS_INFO_REQ_T *req
        )
{
    /* Check the state of the task lock before doing anything. */
    if (!state->dmExtAdvLock)
    {
        MAKE_PRIM_T(DM_ULP_EXT_ADV_SETS_INFO_REQ);

        /* One request at a time, set the ad lock. */
        state->dmExtAdvLock     = req->theAppTask;

        VmSendDmPrim(prim);
    }
    else
    {
        /* Ext Adv request already outstanding, queue up this request. */
        MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_EXT_ADV_SETS_INFO_REQ);
        COPY_CL_MESSAGE(req, message);
        MessageSendConditionallyOnTask(
                    connectionGetCmTask(),
                    CL_INTERNAL_DM_BLE_EXT_ADV_SETS_INFO_REQ,
                    message,
                    &state->dmExtAdvLock
                    );
    }
}

/****************************************************************************
NAME
    connectionHandleDmBleExtAdvSetsInfoCfm

DESCRIPTION
    Handles status of Extended Advertising Sets Info Request

RETURNS
    void
*/
void connectionHandleDmBleExtAdvSetsInfoCfm(
        connectionDmExtAdvState *state,
        const DM_ULP_EXT_ADV_SETS_INFO_CFM_T *cfm
        )
{
    if (state->dmExtAdvLock)
    {
        uint8 i;
        MAKE_CL_MESSAGE(CL_DM_BLE_SET_EXT_ADV_SETS_INFO_CFM);

        message->flags = cfm->flags;
        message->num_adv_sets = cfm->num_adv_sets;

        for (i = 0; i < CL_DM_BLE_EXT_ADV_MAX_REPORTED_ADV_SETS; i++)
        {
            message->adv_sets[i].registered    = cfm->adv_sets[i].registered;
            message->adv_sets[i].advertising   = cfm->adv_sets[i].advertising;
            message->adv_sets[i].info          = cfm->adv_sets[i].info;
        }

        MessageSend(
                    state->dmExtAdvLock,
                    CL_DM_BLE_SET_EXT_ADV_SETS_INFO_CFM,
                    message
                    );
    }

    /* Reset the ad lock. */
    state->dmExtAdvLock = NULL;
}

/****************************************************************************
NAME
    ConnectionDmBleExtAdvRegisterAppAdvSetReq

DESCRIPTION
    Allows an application to register for use of an advertising set.

RETURNS
   void
*/
void ConnectionDmBleExtAdvRegisterAppAdvSetReq(Task theAppTask, uint8 adv_handle)
{
    MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_EXT_ADV_REGISTER_APP_ADV_SET_REQ);
    message->theAppTask = theAppTask;
    message->adv_handle = adv_handle;
    message->flags      = 0;
    MessageSend(
                connectionGetCmTask(),
                CL_INTERNAL_DM_BLE_EXT_ADV_REGISTER_APP_ADV_SET_REQ,
                message);
}

/****************************************************************************
NAME
    connectionHandleDmBleRegisterAppAdvSetReq

DESCRIPTION
    This function will initiate an Extended Advertising Register App Adv Set request.

RETURNS
   void
*/
void connectionHandleDmBleRegisterAppAdvSetReq(
        connectionDmExtAdvState *state,
        const CL_INTERNAL_DM_BLE_EXT_ADV_REGISTER_APP_ADV_SET_REQ_T *req
        )
{
    /* Check the state of the task lock before doing anything. */
    if (!state->dmExtAdvLock)
    {
        MAKE_PRIM_T(DM_ULP_EXT_ADV_REGISTER_APP_ADV_SET_REQ);

        /* One request at a time, set the ad lock. */
        state->dmExtAdvLock = req->theAppTask;

        prim->adv_handle    = req->adv_handle;
        prim->flags         = req->flags;

        VmSendDmPrim(prim);
    }
    else
    {
        /* ExtAdv request already outstanding, queue up this request. */
        MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_EXT_ADV_REGISTER_APP_ADV_SET_REQ);
        COPY_CL_MESSAGE(req, message);
        MessageSendConditionallyOnTask(
                    connectionGetCmTask(),
                    CL_INTERNAL_DM_BLE_EXT_ADV_REGISTER_APP_ADV_SET_REQ,
                    message,
                    &state->dmExtAdvLock
                    );
    }
}

/****************************************************************************
NAME
    connectionHandleDmBleExtAdvRegisterAppAdvSetCfm

DESCRIPTION
    Handle the DM_ULP_EXT_ADV_REGISTER_APP_ADV_SET_CFM from Bluestack.

RETURNS
   void
*/
void connectionHandleDmBleExtAdvRegisterAppAdvSetCfm(connectionDmExtAdvState *state,
        const DM_ULP_EXT_ADV_REGISTER_APP_ADV_SET_CFM_T *cfm
        )
{
    if (state->dmExtAdvLock)
    {
        MAKE_CL_MESSAGE(CL_DM_BLE_EXT_ADV_REGISTER_APP_ADV_SET_CFM);
        message->status = connectionConvertHciStatus(cfm->status);
        MessageSend(
                    state->dmExtAdvLock,
                    CL_DM_BLE_EXT_ADV_REGISTER_APP_ADV_SET_CFM,
                    message
                    );
    }

    /* Reset the ad lock. */
    state->dmExtAdvLock = NULL;
}

/****************************************************************************
NAME
    ConnectionDmBleExtAdvUnregisterAppAdvSetReq

DESCRIPTION
    Allows an application to unregister use of an advertising set.

RETURNS
   void
*/
void ConnectionDmBleExtAdvUnregisterAppAdvSetReq(Task theAppTask, uint8 adv_handle)
{
    MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_EXT_ADV_UNREGISTER_APP_ADV_SET_REQ);
    message->theAppTask = theAppTask;
    message->adv_handle = adv_handle;
    MessageSend(
                connectionGetCmTask(),
                CL_INTERNAL_DM_BLE_EXT_ADV_UNREGISTER_APP_ADV_SET_REQ,
                message);
}

/****************************************************************************
NAME
    connectionHandleDmBleUnregisterAppAdvSetReq

DESCRIPTION
    This function will initiate an Extended Advertising Unregister App Adv Set request.

RETURNS
   void
*/
void connectionHandleDmBleUnregisterAppAdvSetReq(
        connectionDmExtAdvState *state,
        const CL_INTERNAL_DM_BLE_EXT_ADV_UNREGISTER_APP_ADV_SET_REQ_T *req
        )
{
    /* Check the state of the task lock before doing anything. */
    if (!state->dmExtAdvLock)
    {
        MAKE_PRIM_T(DM_ULP_EXT_ADV_UNREGISTER_APP_ADV_SET_REQ);

        /* One request at a time, set the ad lock. */
        state->dmExtAdvLock = req->theAppTask;

        prim->adv_handle    = req->adv_handle;

        VmSendDmPrim(prim);
    }
    else
    {
        /* ExtAdv request already outstanding, queue up this request. */
        MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_EXT_ADV_UNREGISTER_APP_ADV_SET_REQ);
        COPY_CL_MESSAGE(req, message);
        MessageSendConditionallyOnTask(
                    connectionGetCmTask(),
                    CL_INTERNAL_DM_BLE_EXT_ADV_UNREGISTER_APP_ADV_SET_REQ,
                    message,
                    &state->dmExtAdvLock
                    );
    }
}

/****************************************************************************
NAME
    connectionHandleDmBleExtAdvUnregisterAppAdvSetCfm

DESCRIPTION
    Handle the DM_ULP_EXT_ADV_UNREGISTER_APP_ADV_SET_CFM from Bluestack.

RETURNS
   void
*/
void connectionHandleDmBleExtAdvUnregisterAppAdvSetCfm(connectionDmExtAdvState *state,
        const DM_ULP_EXT_ADV_UNREGISTER_APP_ADV_SET_CFM_T *cfm
        )
{
    if (state->dmExtAdvLock)
    {
        MAKE_CL_MESSAGE(CL_DM_BLE_EXT_ADV_UNREGISTER_APP_ADV_SET_CFM);
        message->status = connectionConvertHciStatus(cfm->status);
        MessageSend(
                    state->dmExtAdvLock,
                    CL_DM_BLE_EXT_ADV_UNREGISTER_APP_ADV_SET_CFM,
                    message
                    );
    }

    /* Reset the ad lock. */
    state->dmExtAdvLock = NULL;
}

/****************************************************************************
NAME
    ConnectionDmBleExtAdvertiseEnableReq

DESCRIPTION
    Enables or disables BLE Extended Advertising. If theAppTask is anthing
    other than NULL(0) then that is treated as the task to return the CFM
    message to.

RETURNS
   void
*/
void ConnectionDmBleExtAdvertiseEnableReq(Task theAppTask, bool enable, uint8 adv_handle)
{
    MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_EXT_ADVERTISE_ENABLE_REQ);
    message->theAppTask = theAppTask;
    message->enable = enable;
    message->adv_handle = adv_handle;
    MessageSend(
                connectionGetCmTask(),
                CL_INTERNAL_DM_BLE_EXT_ADVERTISE_ENABLE_REQ,
                message);
}

/****************************************************************************
NAME
    connectionHandleDmBleSetAdvertiseEnableReq

DESCRIPTION
    This function will initiate an Extended Advertising Enable request.

RETURNS
   void
*/
void connectionHandleDmBleExtAdvertiseEnableReq(
        connectionDmExtAdvState *state,
        const CL_INTERNAL_DM_BLE_EXT_ADVERTISE_ENABLE_REQ_T *req
        )
{
    /* Check the state of the task lock before doing anything. */
    if (!state->dmExtAdvLock)
    {
        MAKE_PRIM_T(DM_ULP_EXT_ADV_ENABLE_REQ);

        /* One request at a time, set the ad lock. */
        state->dmExtAdvLock = req->theAppTask;

        prim->enable = (req->enable) ? 1 : 0;
        prim->adv_handle = req->adv_handle;

        VmSendDmPrim(prim);
    }
    else
    {
        /* ExtAdv request already outstanding, queue up this request. */
        MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_EXT_ADVERTISE_ENABLE_REQ);
        COPY_CL_MESSAGE(req, message);
        MessageSendConditionallyOnTask(
                    connectionGetCmTask(),
                    CL_INTERNAL_DM_BLE_EXT_ADVERTISE_ENABLE_REQ,
                    message,
                    &state->dmExtAdvLock
                    );
    }
}

/****************************************************************************
NAME
    connectionHandleDmBleExtAdvertiseEnableCfm

DESCRIPTION
    Handle the DM_ULP_EXT_ADV_ENABLE_CFM from Bluestack.

RETURNS
   void
*/
void connectionHandleDmBleExtAdvertiseEnableCfm(connectionDmExtAdvState *state,
        const DM_ULP_EXT_ADV_ENABLE_CFM_T *cfm
        )
{
    if (state->dmExtAdvLock)
    {
        MAKE_CL_MESSAGE(CL_DM_BLE_EXT_ADVERTISE_ENABLE_CFM);
        message->status = connectionConvertHciStatus(cfm->status);
        MessageSend(
                    state->dmExtAdvLock,
                    CL_DM_BLE_EXT_ADVERTISE_ENABLE_CFM,
                    message
                    );
    }

    /* Reset the ad lock. */
    state->dmExtAdvLock = NULL;
}

/****************************************************************************
NAME
    ConnectionDmBleExtAdvMultiEnableReq

DESCRIPTION
    Enable advertising for X advertising set. This allows multiple advertising
    sets to have advertising enabled or disabled. It also allows advertising to
    occur for a fixed duration or number of extended advertising events.

RETURNS
   void
*/
void ConnectionDmBleExtAdvMultiEnableReq(   Task theAppTask,
                                            uint8 enable,
                                            uint8 num_sets,
                                            CL_EA_ENABLE_CONFIG_T config[CL_DM_BLE_EXT_ADV_MAX_NUM_ENABLE])
{
    uint8 i;

    MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_EXT_ADV_MULTI_ENABLE_REQ);
    message->theAppTask = theAppTask;
    message->enable = enable;
    message->num_sets = num_sets;

    for (i = 0; i < num_sets; i++)
    {
        message->config[i].adv_handle      = config[i].adv_handle;
        message->config[i].max_ea_events   = config[i].max_ea_events;
        message->config[i].duration        = config[i].duration;
    }

    MessageSend(
                connectionGetCmTask(),
                CL_INTERNAL_DM_BLE_EXT_ADV_MULTI_ENABLE_REQ,
                message);
}

/****************************************************************************
NAME
    connectionHandleDmBleSetAdvMultiEnableReq

DESCRIPTION
    This function will initiate an Extended Advertising Multi Enable request.

RETURNS
   void
*/
void connectionHandleDmBleSetAdvMultiEnableReq(
        connectionDmExtAdvState *state,
        const CL_INTERNAL_DM_BLE_EXT_ADV_MULTI_ENABLE_REQ_T *req
        )
{
    /* Check the state of the task lock before doing anything. */
    if (!state->dmExtAdvLock)
    {
        uint8 i;
        MAKE_PRIM_T(DM_ULP_EXT_ADV_MULTI_ENABLE_REQ);

        /* One request at a time, set the ad lock. */
        state->dmExtAdvLock = req->theAppTask;

        prim->enable    = req->enable;
        prim->num_sets  = req->num_sets;

        for (i = 0; i < req->num_sets; i++)
        {
            prim->config[i].adv_handle      = req->config[i].adv_handle;
            prim->config[i].max_ea_events   = req->config[i].max_ea_events;
            prim->config[i].duration        = req->config[i].duration;
        }

        VmSendDmPrim(prim);
    }
    else
    {
        /* ExtAdv request already outstanding, queue up this request. */
        MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_EXT_ADV_MULTI_ENABLE_REQ);
        COPY_CL_MESSAGE(req, message);
        MessageSendConditionallyOnTask(
                    connectionGetCmTask(),
                    CL_INTERNAL_DM_BLE_EXT_ADV_MULTI_ENABLE_REQ,
                    message,
                    &state->dmExtAdvLock
                    );
    }
}

/****************************************************************************
NAME
    connectionHandleDmBleExtAdvMultiEnableCfm

DESCRIPTION
    Handle the DM_ULP_EXT_ADV_MULTI_ENABLE_CFM from Bluestack.

RETURNS
   void
*/
void connectionHandleDmBleExtAdvMultiEnableCfm(connectionDmExtAdvState *state,
        const DM_ULP_EXT_ADV_MULTI_ENABLE_CFM_T *cfm
        )
{
    if (state->dmExtAdvLock)
    {
        MAKE_CL_MESSAGE(CL_DM_BLE_EXT_ADV_MULTI_ENABLE_CFM);
        message->status = (cfm->status) ? fail : success;
        message->max_adv_sets = cfm->max_adv_sets;
        message->adv_bits = cfm->adv_bits;
        MessageSend(
                    state->dmExtAdvLock,
                    CL_DM_BLE_EXT_ADV_MULTI_ENABLE_CFM,
                    message
                    );
    }

    /* Reset the ad lock. */
    state->dmExtAdvLock = NULL;
}

/****************************************************************************
NAME
    ConnectionDmBleExtAdvSetParamsReq

DESCRIPTION
    Configures how the advertising set should advertise.

RETURNS
   void
*/
void ConnectionDmBleExtAdvSetParamsReq(Task theAppTask, uint8 adv_handle, uint16 adv_event_properties,
    uint32 primary_adv_interval_min, uint32 primary_adv_interval_max, uint8 primary_adv_channel_map, uint8 own_addr_type,
    typed_bdaddr taddr, uint8 adv_filter_policy, uint16 primary_adv_phy, uint8 secondary_adv_max_skip, uint16 secondary_adv_phy,
    uint16 adv_sid)
{
    MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_EXT_ADV_SET_PARAMS_REQ);
    message->theAppTask = theAppTask;
    message->adv_handle = adv_handle;
    message->adv_event_properties = adv_event_properties;
    message->primary_adv_interval_min = primary_adv_interval_min;
    message->primary_adv_interval_max = primary_adv_interval_max;
    message->primary_adv_channel_map = primary_adv_channel_map;
    message->own_addr_type = own_addr_type;
    message->taddr = taddr;
    message->adv_filter_policy = adv_filter_policy;
    message->primary_adv_phy = primary_adv_phy;
    message->secondary_adv_max_skip = secondary_adv_max_skip;
    message->secondary_adv_phy = secondary_adv_phy;
    message->adv_sid = adv_sid;
    message->reserved = 0;

    MessageSend(
                connectionGetCmTask(),
                CL_INTERNAL_DM_BLE_EXT_ADV_SET_PARAMS_REQ,
                message);
}

/****************************************************************************
NAME
    connectionHandleDmBleExtAdvSetParamsReq

DESCRIPTION
    This function will initiate an Extended Advertising Set Parameters request.

RETURNS
   void
*/
void connectionHandleDmBleExtAdvSetParamsReq(
        connectionDmExtAdvState *state,
        const CL_INTERNAL_DM_BLE_EXT_ADV_SET_PARAMS_REQ_T *req
        )
{
    /* Check the state of the task lock before doing anything. */
    if (!state->dmExtAdvLock)
    {
        MAKE_PRIM_T(DM_ULP_EXT_ADV_SET_PARAMS_REQ);

        /* One request at a time, set the ad lock. */
        state->dmExtAdvLock = req->theAppTask;

        prim->adv_handle = req->adv_handle;
        prim->adv_event_properties = req->adv_event_properties;
        prim->primary_adv_interval_min = req->primary_adv_interval_min;
        prim->primary_adv_interval_max = req->primary_adv_interval_max;
        prim->primary_adv_channel_map = req->primary_adv_channel_map;
        prim->own_addr_type = req->own_addr_type;
        BdaddrConvertTypedVmToBluestack(&prim->peer_addr, &req->taddr);
        prim->adv_filter_policy = req->adv_filter_policy;
        prim->primary_adv_phy = req->primary_adv_phy;
        prim->secondary_adv_max_skip = req->secondary_adv_max_skip;
        prim->secondary_adv_phy = req->secondary_adv_phy;
        prim->adv_sid = req->adv_sid;
        prim->reserved = req->reserved;

        VmSendDmPrim(prim);
    }
    else
    {
        /* Ext Adv request already outstanding, queue up this request. */
        MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_EXT_ADV_SET_PARAMS_REQ);
        COPY_CL_MESSAGE(req, message);
        MessageSendConditionallyOnTask(
                    connectionGetCmTask(),
                    CL_INTERNAL_DM_BLE_EXT_ADV_SET_PARAMS_REQ,
                    message,
                    &state->dmExtAdvLock
                    );
    }
}

/****************************************************************************
NAME
    connectionHandleDmBleSetExtAdvParamsCfm

DESCRIPTION
    Handles status of Extended Advertising Parameters Request

RETURNS
    void
*/
void connectionHandleDmBleExtAdvSetParamsCfm(connectionDmExtAdvState *state, const DM_ULP_EXT_ADV_SET_PARAMS_CFM_T *cfm)
{
    if (state->dmExtAdvLock)
    {
        MAKE_CL_MESSAGE(CL_DM_BLE_SET_EXT_ADV_PARAMS_CFM);
        message->status = (cfm->status) ? fail : success;
        message->adv_sid = cfm->adv_sid;
        MessageSend(
                    state->dmExtAdvLock,
                    CL_DM_BLE_SET_EXT_ADV_PARAMS_CFM,
                    message
                    );
    }

    /* Reset the ad lock. */
    state->dmExtAdvLock = NULL;
}

/****************************************************************************
NAME
    ConnectionDmBleExtAdvSetRandomAddressReq

DESCRIPTION
    Set the advertising set's random device address to be used when configured
    for use in ConnectionDmBleExtAdvSetParamsReq.

RETURNS
   void
*/
void ConnectionDmBleExtAdvSetRandomAddressReq(Task theAppTask,
                                                uint8 adv_handle,
                                                ble_local_addr_type action,
                                                bdaddr random_addr)
{
    MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_EXT_ADV_SET_RANDOM_ADDRESS_REQ);

    message->theAppTask     = theAppTask;
    message->adv_handle     = adv_handle;
    message->action         = action;
    message->random_addr    = random_addr;

    MessageSend(
                connectionGetCmTask(),
                CL_INTERNAL_DM_BLE_EXT_ADV_SET_RANDOM_ADDRESS_REQ,
                message);
}

/****************************************************************************
NAME
    connectionHandleDmBleExtAdvSetRandomAddressReq

DESCRIPTION
    This function will initiate an Extended Advertising Set Random Address request.

RETURNS
   void
*/
void connectionHandleDmBleExtAdvSetRandomAddressReq(
        connectionDmExtAdvState *state,
        const CL_INTERNAL_DM_BLE_EXT_ADV_SET_RANDOM_ADDRESS_REQ_T *req
        )
{
    /* Check the state of the task lock before doing anything. */
    if (!state->dmExtAdvLock)
    {
        MAKE_PRIM_T(DM_ULP_EXT_ADV_SET_RANDOM_ADDR_REQ);

        /* One request at a time, set the ad lock. */
        state->dmExtAdvLock = req->theAppTask;

        prim->adv_handle    = req->adv_handle;

        /* Typecast the conn lib enum since DM prim uses uint16. */
        prim->action        = (uint16) req->action;

        BdaddrConvertVmToBluestack(&prim->random_addr, &req->random_addr);

        VmSendDmPrim(prim);
    }
    else
    {
        /* Ext Adv request already outstanding, queue up this request. */
        MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_EXT_ADV_SET_RANDOM_ADDRESS_REQ);
        COPY_CL_MESSAGE(req, message);
        MessageSendConditionallyOnTask(
                    connectionGetCmTask(),
                    CL_INTERNAL_DM_BLE_EXT_ADV_SET_RANDOM_ADDRESS_REQ,
                    message,
                    &state->dmExtAdvLock
                    );
    }
}

/****************************************************************************
NAME
    connectionHandleDmBleExtAdvSetRandomAddressCfm

DESCRIPTION
    Handles status of Extended Advertising Set Random Address request

RETURNS
    void
*/
void connectionHandleDmBleExtAdvSetRandomAddressCfm(
        connectionDmExtAdvState *state,
        const DM_ULP_EXT_ADV_SET_RANDOM_ADDR_CFM_T *cfm
        )
{
    if (state->dmExtAdvLock)
    {
        MAKE_CL_MESSAGE(CL_DM_BLE_EXT_ADV_SET_RANDOM_ADDRESS_CFM);
        message->status = connectionConvertHciStatus(cfm->status);
        message->adv_handle = cfm->adv_handle;
        BdaddrConvertBluestackToVm(&message->random_addr, &cfm->random_addr);

        MessageSend(
                    state->dmExtAdvLock,
                    CL_DM_BLE_EXT_ADV_SET_RANDOM_ADDRESS_CFM,
                    message
                    );
    }

    /* Reset the ad lock. */
    state->dmExtAdvLock = NULL;
}


/****************************************************************************
NAME
    ConnectionDmBleExtAdvSetDataReq

DESCRIPTION
    Sets Extended Advertising data.

RETURNS
   void
*/
void ConnectionDmBleExtAdvSetDataReq(Task theAppTask,
                                    uint8 adv_handle,
                                    set_data_req_operation operation,
                                    uint8 adv_data_len,
                                    uint8 *adv_data[8])
{
    uint8 i;
    MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_EXT_ADV_SET_DATA_REQ);

    message->theAppTask = theAppTask;
    message->adv_handle = adv_handle;
    message->operation = operation;
    message->adv_data_len = adv_data_len;

    for (i = 0; i < 8; i++)
    {
        message->adv_data[i] = VmGetHandleFromPointer(adv_data[i]);
    }

    MessageSend(
                connectionGetCmTask(),
                CL_INTERNAL_DM_BLE_EXT_ADV_SET_DATA_REQ,
                message);
}

/****************************************************************************
NAME
    connectionHandleDmBleExtAdvSetDataReq

DESCRIPTION
    This function will initiate an Extended Advertising Set Data request.

RETURNS
   void
*/
void connectionHandleDmBleExtAdvSetDataReq(
        connectionDmExtAdvState *state,
        const CL_INTERNAL_DM_BLE_EXT_ADV_SET_DATA_REQ_T *req
        )
{
    /* Check the state of the task lock before doing anything. */
    if (!state->dmExtAdvLock)
    {
        uint8 i;
        MAKE_PRIM_C(DM_HCI_ULP_EXT_ADV_SET_DATA_REQ);

        /* One request at a time, set the ad lock. */
        state->dmExtAdvLock     = req->theAppTask;

        prim->adv_handle        = req->adv_handle;
        prim->operation         = req->operation;
        prim->frag_preference   = 0;
        prim->adv_data_len      = req->adv_data_len;

        for (i = 0; i < 8; i++)
        {
            prim->adv_data[i] = req->adv_data[i];
        }
        VmSendDmPrim(prim);
    }
    else
    {
        /* Ext Adv request already outstanding, queue up this request. */
        MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_EXT_ADV_SET_DATA_REQ);
        COPY_CL_MESSAGE(req, message);
        MessageSendConditionallyOnTask(
                    connectionGetCmTask(),
                    CL_INTERNAL_DM_BLE_EXT_ADV_SET_DATA_REQ,
                    message,
                    &state->dmExtAdvLock
                    );
    }
}

/****************************************************************************
NAME
    connectionHandleDmBleSetExtAdvDataCfm

DESCRIPTION
    Handles status of Extended Advertising Data Request

RETURNS
    void
*/
void connectionHandleDmBleExtAdvSetDataCfm(
        connectionDmExtAdvState *state,
        const DM_HCI_ULP_EXT_ADV_SET_DATA_CFM_T *cfm
        )
{
    if (state->dmExtAdvLock)
    {
        MAKE_CL_MESSAGE(CL_DM_BLE_SET_EXT_ADV_DATA_CFM);
        message->status = (cfm->status) ? fail : success;
        MessageSend(
                    state->dmExtAdvLock,
                    CL_DM_BLE_SET_EXT_ADV_DATA_CFM,
                    message
                    );
    }

    /* Reset the ad lock. */
    state->dmExtAdvLock = NULL;
}


/****************************************************************************
NAME
    ConnectionDmBleExtAdvSetScanRespDataReq

DESCRIPTION
    Sets Extended Advertising Scan Response data.

RETURNS
   void
*/
void ConnectionDmBleExtAdvSetScanRespDataReq(Task theAppTask,
                                            uint8 adv_handle,
                                            set_data_req_operation operation,
                                            uint8 scan_resp_data_len,
                                            uint8 *scan_resp_data[8])
{
    uint8 i;
    MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_EXT_ADV_SET_SCAN_RESP_DATA_REQ);

    message->theAppTask = theAppTask;
    message->adv_handle = adv_handle;
    message->operation = operation;
    message->scan_resp_data_len = scan_resp_data_len;

    for (i = 0; i < 8; i++)
    {
        message->scan_resp_data[i] = VmGetHandleFromPointer(scan_resp_data[i]);
    }

    memmove(message->scan_resp_data, scan_resp_data, 8);

    MessageSend(
                connectionGetCmTask(),
                CL_INTERNAL_DM_BLE_EXT_ADV_SET_SCAN_RESP_DATA_REQ,
                message);
}

/****************************************************************************
NAME
    connectionHandleDmBleExtAdvSetScanRespDataReq

DESCRIPTION
    This function will initiate an Extended Advertising Set Scan Response Data request.

RETURNS
   void
*/
void connectionHandleDmBleExtAdvSetScanRespDataReq(
        connectionDmExtAdvState *state,
        const CL_INTERNAL_DM_BLE_EXT_ADV_SET_SCAN_RESP_DATA_REQ_T *req
        )
{
    /* Check the state of the task lock before doing anything. */
    if (!state->dmExtAdvLock)
    {
        uint8 i;
        MAKE_PRIM_C(DM_HCI_ULP_EXT_ADV_SET_SCAN_RESP_DATA_REQ);

        /* One request at a time, set the ad lock. */
        state->dmExtAdvLock         = req->theAppTask;

        prim->adv_handle            = req->adv_handle;
        prim->operation             = req->operation;
        prim->frag_preference       = 0;
        prim->scan_resp_data_len    = req->scan_resp_data_len;

        for (i = 0; i < 8; i++)
        {
            prim->scan_resp_data[i] = req->scan_resp_data[i];
        }
        VmSendDmPrim(prim);
    }
    else
    {
        /* Ext Adv request already outstanding, queue up this request. */
        MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_EXT_ADV_SET_SCAN_RESP_DATA_REQ);
        COPY_CL_MESSAGE(req, message);
        MessageSendConditionallyOnTask(
                    connectionGetCmTask(),
                    CL_INTERNAL_DM_BLE_EXT_ADV_SET_SCAN_RESP_DATA_REQ,
                    message,
                    &state->dmExtAdvLock
                    );
    }
}

/****************************************************************************
NAME
    connectionHandleDmBleSetExtAdvScanRespDataCfm

DESCRIPTION
    Handles status of Extended Advertising Scan Response Data Request

RETURNS
    void
*/


void connectionHandleDmBleExtAdvSetScanRespDataCfm(
        connectionDmExtAdvState *state,
        const DM_HCI_ULP_EXT_ADV_SET_SCAN_RESP_DATA_CFM_T *cfm
        )
{
    if (state->dmExtAdvLock)
    {
        MAKE_CL_MESSAGE(CL_DM_BLE_EXT_ADV_SET_SCAN_RESPONSE_DATA_CFM);
        message->status = (cfm->status) ? fail : success;
        MessageSend(
                    state->dmExtAdvLock,
                    CL_DM_BLE_EXT_ADV_SET_SCAN_RESPONSE_DATA_CFM,
                    message
                    );
    }

    /* Reset the ad lock. */
    state->dmExtAdvLock = NULL;
}

/****************************************************************************
NAME
    ConnectionDmBleExtAdvReadMaxAdvDataLenReq

DESCRIPTION
    Reads the max allowed advertising data for an advertising set.

RETURNS
   void
*/
void ConnectionDmBleExtAdvReadMaxAdvDataLenReq(Task theAppTask, uint8 adv_handle)
{
    MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_EXT_ADV_READ_MAX_ADV_DATA_LEN_REQ);

    message->theAppTask = theAppTask;
    message->adv_handle = adv_handle;

    MessageSend(
                connectionGetCmTask(),
                CL_INTERNAL_DM_BLE_EXT_ADV_READ_MAX_ADV_DATA_LEN_REQ,
                message);
}

/****************************************************************************
NAME
    connectionHandleDmBleExtAdvReadMaxAdvDataLenReq

DESCRIPTION
    This function will initiate an Extended Advertising Read Max Adv Data Len request.

RETURNS
   void
*/
void connectionHandleDmBleExtAdvReadMaxAdvDataLenReq(
        connectionDmExtAdvState *state,
        const CL_INTERNAL_DM_BLE_EXT_ADV_READ_MAX_ADV_DATA_LEN_REQ_T *req
        )
{
    /* Check the state of the task lock before doing anything. */
    if (!state->dmExtAdvLock)
    {
        MAKE_PRIM_T(DM_ULP_EXT_ADV_READ_MAX_ADV_DATA_LEN_REQ);

        /* One request at a time, set the ad lock. */
        state->dmExtAdvLock         = req->theAppTask;

        prim->adv_handle            = req->adv_handle;
    }
    else
    {
        /* Ext Adv request already outstanding, queue up this request. */
        MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_EXT_ADV_READ_MAX_ADV_DATA_LEN_REQ);
        COPY_CL_MESSAGE(req, message);
        MessageSendConditionallyOnTask(
                    connectionGetCmTask(),
                    CL_INTERNAL_DM_BLE_EXT_ADV_READ_MAX_ADV_DATA_LEN_REQ,
                    message,
                    &state->dmExtAdvLock
                    );
    }
}

/****************************************************************************
NAME
    connectionHandleDmBleExtAdvReadMaxAdvDataLenCfm

DESCRIPTION
    Handles status of Extended Advertising Read Max Adv Data Len Request

RETURNS
    void
*/
void connectionHandleDmBleExtAdvReadMaxAdvDataLenCfm(
        connectionDmExtAdvState *state,
        const DM_ULP_EXT_ADV_READ_MAX_ADV_DATA_LEN_CFM_T *cfm
        )
{
    if (state->dmExtAdvLock)
    {
        MAKE_CL_MESSAGE(CL_DM_BLE_EXT_ADV_READ_MAX_ADV_DATA_LEN_CFM);

        message->status = (cfm->status) ? fail : success;

        message->max_adv_data       = cfm->max_adv_data;
        message->max_scan_resp_data = cfm->max_scan_resp_data;


        MessageSend(
                    state->dmExtAdvLock,
                    CL_DM_BLE_EXT_ADV_READ_MAX_ADV_DATA_LEN_CFM,
                    message
                    );
    }

    /* Reset the ad lock. */
    state->dmExtAdvLock = NULL;
}

/****************************************************************************
NAME
    connectionHandleDmBleExtAdvTerminatedInd

DESCRIPTION
    Handles the Extended Advertising terminated indication, sent any time
    advertising is stopped by the controller due to duration expiring or max
    extended advertising event limit reached or connection establishment.

RETURNS
    void
*/
void connectionHandleDmBleExtAdvTerminatedInd(const DM_ULP_EXT_ADV_TERMINATED_IND_T *ind)
{
    MAKE_CL_MESSAGE(CL_DM_BLE_EXT_ADV_TERMINATED_IND);

    message->adv_handle = ind->adv_handle;
    message->reason = ind->reason;
    BdaddrConvertTypedBluestackToVm(&message->taddr, &ind->addrt);
    message->ea_events = ind->ea_events;
    message->max_adv_sets = ind->max_adv_sets;
    message->adv_bits = ind->adv_bits;

    MessageSend(
                connectionGetAppTask(),
                CL_DM_BLE_EXT_ADV_TERMINATED_IND,
                message
                );
}

/*****************************************************************************
 *                  Periodic Advertising functions                           *
 *****************************************************************************/

/****************************************************************************
NAME
    ConnectionDmBlePerAdvSetParamsReq

DESCRIPTION
    Configures how the advertising set to periodic advertise should do so.

RETURNS
   void
*/
void ConnectionDmBlePerAdvSetParamsReq( Task theAppTask,
                                        uint8 adv_handle,
                                        uint32 flags,
                                        uint16 periodic_adv_interval_min,
                                        uint16 periodic_adv_interval_max,
                                        uint16 periodic_adv_properties)
{
    MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_PER_ADV_SET_PARAMS_REQ);
    message->theAppTask = theAppTask;
    message->adv_handle = adv_handle;
    message->flags = flags;
    message->periodic_adv_interval_min = periodic_adv_interval_min;
    message->periodic_adv_interval_max = periodic_adv_interval_max;
    message->periodic_adv_properties = periodic_adv_properties;

    MessageSend(
                connectionGetCmTask(),
                CL_INTERNAL_DM_BLE_PER_ADV_SET_PARAMS_REQ,
                message);
}

/****************************************************************************
NAME
    connectionHandleDmBlePerAdvSetParamsReq

DESCRIPTION
    This function will initiate an Periodic Advertising Set Parameters request.

RETURNS
   void
*/
void connectionHandleDmBlePerAdvSetParamsReq(
        connectionDmPerAdvState *state,
        const CL_INTERNAL_DM_BLE_PER_ADV_SET_PARAMS_REQ_T *req
        )
{
    /* Check the state of the task lock before doing anything. */
    if (!state->dmPerAdvLock)
    {
        MAKE_PRIM_T(DM_ULP_PERIODIC_ADV_SET_PARAMS_REQ);

        /* One request at a time, set the ad lock. */
        state->dmPerAdvLock = req->theAppTask;

        prim->adv_handle                = req->adv_handle;
        prim->flags                     = req->flags;
        prim->periodic_adv_interval_min = req->periodic_adv_interval_min;
        prim->periodic_adv_interval_max = req->periodic_adv_interval_max;
        prim->periodic_adv_properties   = req->periodic_adv_properties;

        VmSendDmPrim(prim);
    }
    else
    {
        /* Ext Adv request already outstanding, queue up this request. */
        MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_PER_ADV_SET_PARAMS_REQ);
        COPY_CL_MESSAGE(req, message);
        MessageSendConditionallyOnTask(
                    connectionGetCmTask(),
                    CL_INTERNAL_DM_BLE_PER_ADV_SET_PARAMS_REQ,
                    message,
                    &state->dmPerAdvLock
                    );
    }
}

/****************************************************************************
NAME
    connectionHandleDmBlePerAdvSetParamsCfm

DESCRIPTION
    Handles status of Periodic Advertising Parameters Request

RETURNS
    void
*/

void connectionHandleDmBlePerAdvSetParamsCfm(connectionDmPerAdvState *state,
                            const DM_ULP_PERIODIC_ADV_SET_PARAMS_CFM_T *cfm)
{
    if (state->dmPerAdvLock)
    {
        MAKE_CL_MESSAGE(CL_DM_BLE_PER_ADV_SET_PARAMS_CFM);
        message->status = connectionConvertHciStatus(cfm->status);
        MessageSend(
                    state->dmPerAdvLock,
                    CL_DM_BLE_PER_ADV_SET_PARAMS_CFM,
                    message
                    );
    }

    /* Reset the ad lock. */
    state->dmPerAdvLock = NULL;
}

/****************************************************************************
NAME
    ConnectionDmBlePerAdvSetDataReq

DESCRIPTION
    Sets Periodic Advertising data.

RETURNS
   void
*/
void ConnectionDmBlePerAdvSetDataReq(Task theAppTask,
                                    uint8 adv_handle,
                                    set_data_req_operation operation,
                                    uint8 adv_data_len,
                                    uint8 *adv_data[8])
{
    uint8 i;
    MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_PER_ADV_SET_DATA_REQ);

    message->theAppTask = theAppTask;
    message->adv_handle = adv_handle;
    message->operation = operation;
    message->adv_data_len = adv_data_len;

    for (i = 0; i < 8; i++)
    {
        message->adv_data[i] = VmGetHandleFromPointer(adv_data[i]);
    }

    MessageSend(
                connectionGetCmTask(),
                CL_INTERNAL_DM_BLE_PER_ADV_SET_DATA_REQ,
                message);
}

/****************************************************************************
NAME
    connectionHandleDmBlePerAdvSetDataReq

DESCRIPTION
    This function will initiate an Periodic Advertising Set Data request.

RETURNS
   void
*/
void connectionHandleDmBlePerAdvSetDataReq(
        connectionDmPerAdvState *state,
        const CL_INTERNAL_DM_BLE_PER_ADV_SET_DATA_REQ_T *req
        )
{
    /* Check the state of the task lock before doing anything. */
    if (!state->dmPerAdvLock)
    {
        uint8 i;
        MAKE_PRIM_C(DM_HCI_ULP_PERIODIC_ADV_SET_DATA_REQ);

        /* One request at a time, set the ad lock. */
        state->dmPerAdvLock     = req->theAppTask;

        prim->adv_handle        = req->adv_handle;
        prim->operation         = req->operation;
        prim->adv_data_len      = req->adv_data_len;

        for (i = 0; i < 8; i++)
        {
            prim->adv_data[i] = req->adv_data[i];
        }

        VmSendDmPrim(prim);
    }
    else
    {
        /* Per Adv request already outstanding, queue up this request. */
        MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_PER_ADV_SET_DATA_REQ);
        COPY_CL_MESSAGE(req, message);
        MessageSendConditionallyOnTask(
                    connectionGetCmTask(),
                    CL_INTERNAL_DM_BLE_PER_ADV_SET_DATA_REQ,
                    message,
                    &state->dmPerAdvLock
                    );
    }
}

/****************************************************************************
NAME
    connectionHandleDmBlePerAdvSetDataCfm

DESCRIPTION
    Handles status of Periodic Advertising Data Request

RETURNS
    void
*/
void connectionHandleDmBlePerAdvSetDataCfm(connectionDmPerAdvState *state,
        const DM_HCI_ULP_PERIODIC_ADV_SET_DATA_CFM_T *cfm
        )
{
    if (state->dmPerAdvLock)
    {
        MAKE_CL_MESSAGE(CL_DM_BLE_PER_ADV_SET_DATA_CFM);
        message->status = connectionConvertHciStatus(cfm->status);
        MessageSend(
                state->dmPerAdvLock,
                CL_DM_BLE_PER_ADV_SET_DATA_CFM,
                message
                );
    }

    /* Reset the ad lock. */
    state->dmPerAdvLock = NULL;
}

/****************************************************************************
NAME
    ConnectionDmBlePerAdvStartReq

DESCRIPTION
    Starts a periodic advertising train.

RETURNS
   void
*/
void ConnectionDmBlePerAdvStartReq(Task theAppTask, uint8 adv_handle)
{
    MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_PER_ADV_START_REQ);
    message->theAppTask = theAppTask;
    message->adv_handle = adv_handle;

    MessageSend(
                connectionGetCmTask(),
                CL_INTERNAL_DM_BLE_PER_ADV_START_REQ,
                message);
}

/****************************************************************************
NAME
    connectionHandleDmBlePerAdvStartReq

DESCRIPTION
    This function will initiate an Periodic Advertising Start request.

RETURNS
   void
*/
void connectionHandleDmBlePerAdvStartReq(
        connectionDmPerAdvState *state,
        const CL_INTERNAL_DM_BLE_PER_ADV_START_REQ_T *req
        )
{
    /* Check the state of the task lock before doing anything. */
    if (!state->dmPerAdvLock)
    {
        MAKE_PRIM_T(DM_ULP_PERIODIC_ADV_START_REQ);

        /* One request at a time, set the ad lock. */
        state->dmPerAdvLock = req->theAppTask;

        prim->adv_handle                = req->adv_handle;

        VmSendDmPrim(prim);
    }
    else
    {
        /* Ext Adv request already outstanding, queue up this request. */
        MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_PER_ADV_START_REQ);
        COPY_CL_MESSAGE(req, message);
        MessageSendConditionallyOnTask(
                    connectionGetCmTask(),
                    CL_INTERNAL_DM_BLE_PER_ADV_START_REQ,
                    message,
                    &state->dmPerAdvLock
                    );
    }
}

/****************************************************************************
NAME
    connectionHandleDmBlePerAdvStartCfm

DESCRIPTION
    Handles status of Periodic Advertising Start Request

RETURNS
    void
*/

void connectionHandleDmBlePerAdvStartCfm(connectionDmPerAdvState *state,
                            const DM_ULP_PERIODIC_ADV_START_CFM_T *cfm)
{
    if (state->dmPerAdvLock)
    {
        MAKE_CL_MESSAGE(CL_DM_BLE_PER_ADV_START_CFM);
        message->status = connectionConvertHciStatus(cfm->status);
        MessageSend(
                    state->dmPerAdvLock,
                    CL_DM_BLE_PER_ADV_START_CFM,
                    message
                    );
    }

    /* Reset the ad lock. */
    state->dmPerAdvLock = NULL;
}

/****************************************************************************
NAME
    ConnectionDmBlePerAdvStopReq

DESCRIPTION
    Stops a periodic advertising train or just the associated extended advertising.

RETURNS
   void
*/
void ConnectionDmBlePerAdvStopReq(Task theAppTask, uint8 adv_handle, uint8 stop_advertising)
{
    MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_PER_ADV_STOP_REQ);
    message->theAppTask         = theAppTask;
    message->adv_handle         = adv_handle;
    message->stop_advertising   = stop_advertising;

    MessageSend(
                connectionGetCmTask(),
                CL_INTERNAL_DM_BLE_PER_ADV_STOP_REQ,
                message);
}

/****************************************************************************
NAME
    connectionHandleDmBlePerAdvStopReq

DESCRIPTION
    This function will initiate an Periodic Advertising Stop request.

RETURNS
   void
*/
void connectionHandleDmBlePerAdvStopReq(
        connectionDmPerAdvState *state,
        const CL_INTERNAL_DM_BLE_PER_ADV_STOP_REQ_T *req
        )
{
    /* Check the state of the task lock before doing anything. */
    if (!state->dmPerAdvLock)
    {
        MAKE_PRIM_T(DM_ULP_PERIODIC_ADV_STOP_REQ);

        /* One request at a time, set the ad lock. */
        state->dmPerAdvLock = req->theAppTask;

        prim->adv_handle        = req->adv_handle;
        prim->stop_advertising  = req->stop_advertising;

        VmSendDmPrim(prim);
    }
    else
    {
        /* Ext Adv request already outstanding, queue up this request. */
        MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_PER_ADV_STOP_REQ);
        COPY_CL_MESSAGE(req, message);
        MessageSendConditionallyOnTask(
                    connectionGetCmTask(),
                    CL_INTERNAL_DM_BLE_PER_ADV_STOP_REQ,
                    message,
                    &state->dmPerAdvLock
                    );
    }
}

/****************************************************************************
NAME
    connectionHandleDmBlePerAdvStopCfm

DESCRIPTION
    Handles status of Periodic Advertising Stop Request

RETURNS
    void
*/

void connectionHandleDmBlePerAdvStopCfm(connectionDmPerAdvState *state,
                            const DM_ULP_PERIODIC_ADV_STOP_CFM_T *cfm)
{
    if (state->dmPerAdvLock)
    {
        MAKE_CL_MESSAGE(CL_DM_BLE_PER_ADV_STOP_CFM);
        message->status = connectionConvertHciStatus(cfm->status);
        MessageSend(
                    state->dmPerAdvLock,
                    CL_DM_BLE_PER_ADV_STOP_CFM,
                    message
                    );
    }

    /* Reset the ad lock. */
    state->dmPerAdvLock = NULL;
}

/****************************************************************************
NAME
    ConnectionDmBlePerAdvSetTransferReq

DESCRIPTION
    Instructs the Controller to communicate sync info for an advertising train
    that is being broadcast from the local Controller to a connected Peer.

RETURNS
   void
*/
void ConnectionDmBlePerAdvSetTransferReq(Task theAppTask,
                                        typed_bdaddr taddr,
                                        uint16 service_data,
                                        uint8 adv_handle)
{
    MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_PER_ADV_SET_TRANSFER_REQ);
    message->theAppTask         = theAppTask;

    message->taddr          = taddr;
    message->service_data   = service_data;
    message->adv_handle     = adv_handle;

    MessageSend(
                connectionGetCmTask(),
                CL_INTERNAL_DM_BLE_PER_ADV_SET_TRANSFER_REQ,
                message);
}

/****************************************************************************
NAME
    connectionHandleDmBlePerAdvSetTransferReq

DESCRIPTION
    This function will initiate an Periodic Advertising Set Transfer request.

RETURNS
   void
*/
void connectionHandleDmBlePerAdvSetTransferReq(
        connectionDmPerAdvState *state,
        const CL_INTERNAL_DM_BLE_PER_ADV_SET_TRANSFER_REQ_T *req
        )
{
    /* Check the state of the task lock before doing anything. */
    if (!state->dmPerAdvLock)
    {
        MAKE_PRIM_T(DM_ULP_PERIODIC_ADV_SET_TRANSFER_REQ);

        /* One request at a time, set the ad lock. */
        state->dmPerAdvLock = req->theAppTask;

        BdaddrConvertTypedVmToBluestack(&(prim->addrt), &(req->taddr));
        prim->service_data  = req->service_data;
        prim->adv_handle    = req->adv_handle;

        VmSendDmPrim(prim);
    }
    else
    {
        /* Ext Adv request already outstanding, queue up this request. */
        MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_PER_ADV_SET_TRANSFER_REQ);
        COPY_CL_MESSAGE(req, message);
        MessageSendConditionallyOnTask(
                    connectionGetCmTask(),
                    CL_INTERNAL_DM_BLE_PER_ADV_SET_TRANSFER_REQ,
                    message,
                    &state->dmPerAdvLock
                    );
    }
}

/****************************************************************************
NAME
    connectionHandleDmBlePerAdvSetTransferCfm

DESCRIPTION
    Handles status of Periodic Advertising Set Transfer Request

RETURNS
    void
*/

void connectionHandleDmBlePerAdvSetTransferCfm(connectionDmPerAdvState *state,
                            const DM_ULP_PERIODIC_ADV_SET_TRANSFER_CFM_T *cfm)
{
    if (state->dmPerAdvLock)
    {
        MAKE_CL_MESSAGE(CL_DM_BLE_PER_ADV_SET_TRANSFER_CFM);

        message->adv_handle = cfm->adv_handle;
        message->status = connectionConvertHciStatus(cfm->status);

        MessageSend(
                    state->dmPerAdvLock,
                    CL_DM_BLE_PER_ADV_SET_TRANSFER_CFM,
                    message
                    );
    }

    /* Reset the ad lock. */
    state->dmPerAdvLock = NULL;
}

/****************************************************************************
NAME
    ConnectionDmBlePerAdvReadMaxAdvDataLenReq

DESCRIPTION
    Reads the max allowed periodic advertising data for an advertising set.

RETURNS
   void
*/
void ConnectionDmBlePerAdvReadMaxAdvDataLenReq(Task theAppTask, uint8 adv_handle)
{
    MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_PER_ADV_READ_MAX_ADV_DATA_LEN_REQ);

    message->theAppTask = theAppTask;
    message->adv_handle = adv_handle;

    MessageSend(
                connectionGetCmTask(),
                CL_INTERNAL_DM_BLE_PER_ADV_READ_MAX_ADV_DATA_LEN_REQ,
                message);
}

/****************************************************************************
NAME
    connectionHandleDmBlePerAdvReadMaxAdvDataLenReq

DESCRIPTION
    This function will initiate an Periodic Advertising Read Max Adv Data Len request.

RETURNS
   void
*/
void connectionHandleDmBlePerAdvReadMaxAdvDataLenReq(
        connectionDmPerAdvState *state,
        const CL_INTERNAL_DM_BLE_PER_ADV_READ_MAX_ADV_DATA_LEN_REQ_T *req
        )
{
    /* Check the state of the task lock before doing anything. */
    if (!state->dmPerAdvLock)
    {
        MAKE_PRIM_T(DM_ULP_PERIODIC_ADV_READ_MAX_ADV_DATA_LEN_REQ);

        /* One request at a time, set the ad lock. */
        state->dmPerAdvLock         = req->theAppTask;

        prim->adv_handle            = req->adv_handle;
    }
    else
    {
        /* Periodic Adv request already outstanding, queue up this request. */
        MAKE_CL_MESSAGE(CL_INTERNAL_DM_BLE_PER_ADV_READ_MAX_ADV_DATA_LEN_REQ);
        COPY_CL_MESSAGE(req, message);
        MessageSendConditionallyOnTask(
                    connectionGetCmTask(),
                    CL_INTERNAL_DM_BLE_PER_ADV_READ_MAX_ADV_DATA_LEN_REQ,
                    message,
                    &state->dmPerAdvLock
                    );
    }
}

/****************************************************************************
NAME
    connectionHandleDmBlePerAdvReadMaxAdvDataLenCfm

DESCRIPTION
    Handles status of Periodic Advertising Read Max Adv Data Len Request

RETURNS
    void
*/
void connectionHandleDmBlePerAdvReadMaxAdvDataLenCfm(
        connectionDmPerAdvState *state,
        const DM_ULP_PERIODIC_ADV_READ_MAX_ADV_DATA_LEN_CFM_T *cfm
        )
{
    if (state->dmPerAdvLock)
    {
        MAKE_CL_MESSAGE(CL_DM_BLE_PER_ADV_READ_MAX_ADV_DATA_LEN_CFM);

        message->status = (cfm->status) ? fail : success;
        message->max_adv_data = cfm->max_adv_data;

        MessageSend(
                    state->dmPerAdvLock,
                    CL_DM_BLE_PER_ADV_READ_MAX_ADV_DATA_LEN_CFM,
                    message
                    );
    }

    /* Reset the ad lock. */
    state->dmPerAdvLock = NULL;
}

#endif /* DISABLE_BLE */


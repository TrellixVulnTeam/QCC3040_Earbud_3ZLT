/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of common testing functions.
*/

#include "common_test.h"

#include <logging.h>
#include <connection_manager.h>
#include <vm.h>
#include "hfp_profile.h"
#include "hfp_profile_instance.h"
#include "hfp_profile_typedef.h"
#include "kymera_output_common_chain.h"
#include "handset_service.h"

#ifdef GC_SECTIONS
/* Move all functions in KEEP_PM section to ensure they are not removed during
 * garbage collection */
#pragma unitcodesection KEEP_PM
#endif

bool appTestIsHandsetQhsConnectedAddr(const bdaddr* handset_bd_addr)
{
    bool qhs_connected_status = FALSE;

    if (handset_bd_addr != NULL)
    {
        qhs_connected_status = ConManagerGetQhsConnectStatus(handset_bd_addr);

        DEBUG_LOG_ALWAYS("appTestIsHandsetQhsConnectedAddr addr [%04x,%02x,%06lx] qhs_connected:%d", 
                         handset_bd_addr->nap,
                         handset_bd_addr->uap,
                         handset_bd_addr->lap,
                         qhs_connected_status);
    }
    else
    {
        DEBUG_LOG_WARN("appTestIsHandsetQhsConnectedAddr BT adrress is NULL");
    }

    return qhs_connected_status;
}

bool appTestIsHandsetAddrConnected(const bdaddr* handset_bd_addr)
{
    bool is_connected = FALSE;

    if (handset_bd_addr != NULL)
    {
        device_t device = BtDevice_GetDeviceForBdAddr(handset_bd_addr);
        if (device != NULL)
        {
            uint32 connected_profiles = BtDevice_GetConnectedProfiles(device);
            if ((connected_profiles & (DEVICE_PROFILE_HFP | DEVICE_PROFILE_A2DP | DEVICE_PROFILE_AVRCP)) != 0)
            {
                is_connected = TRUE;
            }
        }

        DEBUG_LOG_ALWAYS("appTestIsHandsetAddrConnected addr [%04x,%02x,%06lx] device:%p is_connected:%d",
                         handset_bd_addr->nap,
                         handset_bd_addr->uap,
                         handset_bd_addr->lap,
                         device,
                         is_connected);
    }
    else
    {
        DEBUG_LOG_WARN("appTestIsHandsetAddrConnected BT address is NULL");
    }

    return is_connected;
}

bool appTestIsHandsetHfpScoActiveAddr(const bdaddr* handset_bd_addr)
{
    bool is_sco_active = FALSE;

    if (handset_bd_addr != NULL)
    {
        hfpInstanceTaskData* instance = HfpProfileInstance_GetInstanceForBdaddr(handset_bd_addr);

        is_sco_active = HfpProfile_IsScoActiveForInstance(instance);
        DEBUG_LOG_ALWAYS("appTestIsHandsetHfpScoActiveAddr addr [%04x,%02x,%06lx] is_sco_active:%d", 
                         handset_bd_addr->nap,
                         handset_bd_addr->uap,
                         handset_bd_addr->lap,
                         is_sco_active);
    }
    else
    {
        DEBUG_LOG_WARN("appTestIsHandsetHfpScoActiveAddr BT adrress is NULL");
    }

    return is_sco_active;
}

void appTestEnableCommonChain(void)
{
    DEBUG_LOG_ALWAYS("appTestEnableCommonChain");
    Kymera_OutputCommonChainEnable();
}

void appTestDisableCommonChain(void)
{
    DEBUG_LOG_ALWAYS("appTestDisableCommonChain");
    Kymera_OutputCommonChainDisable();
}

int16 appTestGetRssiOfTpAddr(tp_bdaddr *tpaddr)
{
    int16 rssi = 0;
    if(VmBdAddrGetRssi(tpaddr, &rssi) == FALSE)
    {
        rssi = 0;
    }
    DEBUG_LOG_ALWAYS("appTestGetRssiOfConnectedTpAddr transport=%d tpaddr=%04x,%02x,%06lx RSSI=%d",
                    tpaddr->transport, tpaddr->taddr.addr.lap, tpaddr->taddr.addr.uap, tpaddr->taddr.addr.nap, rssi);
    return rssi;
}

int16 appTestGetBredrRssiOfConnectedHandset(void)
{
    tp_bdaddr tp_addr;
    if(HandsetService_GetConnectedBredrHandsetTpAddress(&tp_addr))
    {
        return appTestGetRssiOfTpAddr(&tp_addr);
    }
    return 0;
}

int16 appTestGetLeRssiOfConnectedHandset(void)
{
    tp_bdaddr tp_addr;
    if(HandsetService_GetConnectedLeHandsetTpAddress(&tp_addr))
    {
        return appTestGetRssiOfTpAddr(&tp_addr);
    }
    return 0;
}

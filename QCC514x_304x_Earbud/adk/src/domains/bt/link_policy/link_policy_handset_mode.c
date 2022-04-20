/*!
\copyright  Copyright (c) 2008 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Link policy manager control of link mode with the handset.
*/

#include <adk_log.h>
#include <bt_device.h>
#include <connection_manager.h>
#include <hfp_profile_instance.h>
#include <av.h>
#include <focus_audio_source.h>
#include <focus_device.h>
#include <panic.h>
#include <connection_abstraction.h>
#include <stream.h>
#include <va_profile.h>
#include <upgrade_gaia_plugin.h>
#include "link_policy_private.h"

/*! Lower power table when idle or unfocused (i.e. no streaming or SCO) */
static const lp_power_table powertable_singlepoint_idle[]=
{
    /* mode,        min_interval, max_interval, attempt, timeout, duration */
    {lp_passive,    0,            0,            0,       0,       2},  /* Passive mode for 2 sec */
    {lp_sniff,      48,           400,          2,       4,       0}   /* Enter sniff mode*/
};

static const lp_power_table powertable_multipoint_idle[]=
{
    /* mode,        min_interval, max_interval, attempt, timeout, duration */
    {lp_passive,    0,            0,            0,       0,       1},  /* Passive mode for 1 sec */
    {lp_sniff,      310,          310,          4,       4,       0}   /* Enter sniff mode*/
};

/*! When broadcast is active, more sniff attempts are required to allow
    receiving the broadcast to be prioritised whilst still maintaining the ACL
    link in sniff mode */
static const lp_power_table powertable_idle_with_broadcast_active[] =
{
    /* mode,        min_interval, max_interval, attempt, timeout, duration */
    {lp_passive,    0,            0,            0,       0,       1}, /* Passive mode */
    {lp_sniff,      48,           400,          2,       4,       0}  /* Enter sniff mode */
};


/*! Lower power table when VA is active */
static const lp_power_table powertable_va_active[]=
{
    /* mode,        min_interval, max_interval, attempt, timeout, duration */
    {lp_active,     0,            0,            0,       0,       5},  /* Active mode for 5 sec */
    {lp_passive,    0,            0,            0,       0,       1},  /* Passive mode for 1 sec */
    {lp_sniff,      48,           400,          2,       4,       0}   /* Enter sniff mode*/
};

/*! Lower power table when only DFU is active */
static const lp_power_table powertable_dfu[]=
{
    /* mode,        min_interval, max_interval, attempt, timeout, duration */
    {lp_active,     0,            0,            0,       0,       10}, /* Active mode for 10 sec */
    {lp_sniff,      48,           400,          2,       4,       0}   /* Enter sniff mode*/
};

/*! Lower power table when A2DP streaming */
static const lp_power_table powertable_a2dp_streaming[]=
{
    /* mode,        min_interval, max_interval, attempt, timeout, duration */
    {lp_active,     0,            0,            0,       0,       5},  /* Active mode for 5 sec */
    {lp_passive,    0,            0,            0,       0,       1},  /* Passive mode for 1 sec */
    {lp_sniff,      48,           48,           2,       4,       0}   /* Enter sniff mode*/
};

/*! Lower power table when SCO active */
static const lp_power_table powertable_sco_active[]=
{
    /* mode,        min_interval, max_interval, attempt, timeout, duration */
    {lp_passive,    0,            0,            0,       0,       1},  /* Passive mode */
    {lp_sniff,      48,           144,          2,       8,       0}   /* Enter sniff mode (30-90ms)*/
};

/*! \cond helper */
#define ARRAY_AND_DIM(ARRAY) (ARRAY), ARRAY_DIM(ARRAY)
/*! \endcond helper */

/*! Structure for storing power tables */
struct powertable_data
{
    const lp_power_table *table;
    uint16 rows;
};

/*! Array of structs used to store the power tables for standard phones */
static const struct powertable_data powertables_standard[] = {
    [POWERTABLE_IDLE] = {ARRAY_AND_DIM(powertable_singlepoint_idle)},
    [POWERTABLE_MULTIPOINT_IDLE] = {ARRAY_AND_DIM(powertable_multipoint_idle)},
    [POWERTABLE_IDLE_WITH_BROADCAST] = {ARRAY_AND_DIM(powertable_idle_with_broadcast_active)},
    [POWERTABLE_VA_ACTIVE] = {ARRAY_AND_DIM(powertable_va_active)},
    [POWERTABLE_DFU] = {ARRAY_AND_DIM(powertable_dfu)},
    [POWERTABLE_A2DP_STREAMING] = {ARRAY_AND_DIM(powertable_a2dp_streaming)},
    [POWERTABLE_SCO_ACTIVE] = {ARRAY_AND_DIM(powertable_sco_active)},
};

static bool appLinkPolicyIsDeviceInFocus(const bdaddr *bd_addr)
{
    bool focused = FALSE;
    device_t device = BtDevice_GetDeviceForBdAddr(bd_addr);

    if (Focus_GetFocusForDevice(device) == focus_foreground)
    {
        focused = TRUE;
    }

    return focused;
}

static Sink appLinkPolicyGetSink(const bdaddr *bdaddr)
{
    tp_bdaddr tbdaddr;
    uint16 max = 1;
    Sink sink = 0;
    BdaddrTpFromBredrBdaddr(&tbdaddr, bdaddr);
    StreamSinksFromBdAddr(&max, &sink, &tbdaddr);
    return max ? sink : 0;
}

static bool appLinkPolicyIsScoActive(const bdaddr *bdaddr)
{
    bool active = FALSE;
#ifdef INCLUDE_HFP
    hfpInstanceTaskData *hfp_inst = HfpProfileInstance_GetInstanceForBdaddr(bdaddr);
    active = (hfp_inst && HfpProfile_IsScoActiveForInstance(hfp_inst));
#endif
    return active;
}

static bool appLinkPolicyIsA2dpStreaming(const bdaddr *bdaddr)
{
    bool active = FALSE;
#ifdef INCLUDE_AV
        avInstanceTaskData *av_inst = appAvInstanceFindFromBdAddr(bdaddr);
        active = (av_inst && appA2dpIsStreaming(av_inst));
#endif
    return active;
}

static bool appLinkPolicyIsVaActive(const bdaddr * bdaddr)
{
    return VaProfile_IsVaActiveAtBdaddr(bdaddr);
}

static bool appLinkPolicyIsDfuActive(const bdaddr *bdaddr)
{
    bool active = FALSE;
#if defined(INCLUDE_GAIA) && defined(INCLUDE_DFU)
    tp_bdaddr tp_bd_addr;
    BdaddrTpFromBredrBdaddr(&tp_bd_addr, bdaddr);
    active = UpgradeGaiaPlugin_IsHandsetTransferActive(&tp_bd_addr);
#else
    UNUSED(bdaddr);
#endif
    return active;
}

static bool appLinkPolicySetPowerTable(const bdaddr *bd_addr, lpPowerTableIndex index)
{
    const struct powertable_data *selected = &powertables_standard[index];

    Sink sink = appLinkPolicyGetSink(bd_addr);
    if (sink)
    {
        ConnectionSetLinkPolicy(sink, selected->rows, selected->table);
    }
    return (sink != 0);
}

/* \brief Select link mode settings to reduce power consumption.

    This function checks what activity the application currently has,
    and decides what the best link settings are for the connection
    to the specified device. This may include full power (#lp_active),
    sniff (#lp_sniff), or passive(#lp_passive) where full power is
    no longer required but the application would prefer not to enter
    sniff mode yet.

    The function also considers multipoint scenarios where a device may be
    the focus device or the out of focus device.

    \param bd_addr  Bluetooth address of the device to update link settings
    \param force The link policy will be updated, even if no change in link
    is detected.
*/
static void appLinkPolicyUpdatePowerTableImpl(const bdaddr *bd_addr, bool force)
{
    lpPerConnectionState lp_state;
    lpPowerTableIndex pt_index = POWERTABLE_IDLE;

    if (BtDevice_GetNumberOfHandsetsConnectedOverBredr() > 1)
    {
        pt_index = POWERTABLE_MULTIPOINT_IDLE;
    }

    if (appLinkPolicyIsDeviceInFocus(bd_addr))
    {
        if (appLinkPolicyIsScoActive(bd_addr))
        {
            pt_index = POWERTABLE_SCO_ACTIVE;
        }
        else if (appLinkPolicyIsA2dpStreaming(bd_addr))
        {
            pt_index = POWERTABLE_A2DP_STREAMING;
        }
        else if (appLinkPolicyIsDfuActive(bd_addr))
        {
            pt_index = POWERTABLE_DFU;
        }
		else if (appLinkPolicyIsVaActive(bd_addr))
        {
            pt_index = POWERTABLE_VA_ACTIVE;
        }
    }

    if (pt_index == POWERTABLE_IDLE)
    {
        if (Focus_GetFocusForAudioSource(audio_source_le_audio_broadcast) == focus_foreground)
        {
            pt_index = POWERTABLE_IDLE_WITH_BROADCAST;
        }
    }

    ConManagerGetLpState(bd_addr, &lp_state);
    lpPowerTableIndex old_index = lp_state.pt_index;

    if ((pt_index != old_index) || force)
    {
        lp_state.pt_index = pt_index;

#if defined(INCLUDE_LEA_LINK_POLICY)
        tp_bdaddr leaddress = {.transport=TRANSPORT_BLE_ACL, .taddr.type = TYPED_BDADDR_PUBLIC, .taddr.addr=*bd_addr};
        lpPerConnectionState le_state;

        if (ConManagerGetLpStateTp(&leaddress, &le_state))
        {
            /* Want to increase connection interval if A2DP streaming.
               As an interim solution, vary the qos when starting A2DP and release
               the qos when stopping. 

               Record a powertable entry against the LE entry in connection manager.
               This allows us to catch the cases where an LE connection is 
               established while already A2DP streaming */
            if (pt_index == POWERTABLE_A2DP_STREAMING)
            {
                /* Check if the LE connection has already been updated */
                if (le_state.pt_index != POWERTABLE_A2DP_STREAMING)
                {
                    ConManagerRequestDeviceQos(&leaddress, cm_qos_lea_idle);
                    ConManagerSetLpStateTp(&leaddress, lp_state);
                }
            }
            else if (pt_index != POWERTABLE_A2DP_STREAMING && old_index == POWERTABLE_A2DP_STREAMING)
            {
                /* This is the default once LE connected */
                ConManagerRequestDefaultQos(cm_transport_ble, cm_qos_low_latency);

                if (le_state.pt_index == POWERTABLE_A2DP_STREAMING)
                {
                    ConManagerReleaseDeviceQos(&leaddress, cm_qos_lea_idle);

                    le_state.pt_index = POWERTABLE_UNASSIGNED;
                    ConManagerSetLpStateTp(&leaddress, le_state);
                }
            }
        }
#endif
        if (appLinkPolicySetPowerTable(bd_addr, pt_index))
        {
            DEBUG_LOG("appLinkPolicyUpdatePowerTableImpl lap=%x, from enum:lpPowerTableIndex:%d to enum:lpPowerTableIndex:%d", bd_addr->lap, lp_state.pt_index, pt_index);
            ConManagerSetLpState(bd_addr, lp_state);
        }
    }
}

static void appLinkPolicyUpdateAllHandsetsAndSinks(bool force)
{
    tp_bdaddr addr;
    cm_connection_iterator_t iterator;
    if (ConManager_IterateFirstActiveConnection(&iterator, &addr))
    {
        do
        {
            bdaddr *bredr_addr = &addr.taddr.addr;
            if (addr.transport == TRANSPORT_BREDR_ACL &&
                (appDeviceTypeIsHandset(bredr_addr) ||
                 appDeviceTypeIsSink(bredr_addr)))
            {
                appLinkPolicyUpdatePowerTableImpl(bredr_addr, force);
            }
        } while (ConManager_IterateNextActiveConnection(&iterator, &addr));
    }
}

void appLinkPolicyUpdatePowerTable(const bdaddr *bd_addr)
{
    /* The bd_addr triggering the update is not currently used, but may be in
       future so is retained in the interface and ignored */
    UNUSED(bd_addr);
    appLinkPolicyUpdateAllHandsetsAndSinks(FALSE);
}

void appLinkPolicyForceUpdatePowerTable(const bdaddr *bd_addr)
{
    /* The bd_addr triggering the update is not currently used, but may be in
       future so is retained in the interface and ignored */
    UNUSED(bd_addr);
    appLinkPolicyUpdateAllHandsetsAndSinks(TRUE);
}

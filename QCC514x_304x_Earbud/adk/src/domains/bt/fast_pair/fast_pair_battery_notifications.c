/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    domains

\brief      Fast Pair battery notification handling. This module receives battery values from case and from state proxy.
            The case battery value is used only when if the device(local or peer) is in-case. This is to avoid UNKNOWN when
            the device is out-of-case. State proxy values are used when the device is not-in-case.
*/

#include "fast_pair_battery_notifications.h"
#include "fast_pair_bloom_filter.h"
#include "fast_pair_advertising.h"
#include "fast_pair_msg_stream_dev_info.h"
#include "state_proxy.h"
#include "state_of_charge.h"

#include <cc_with_case.h>
#include <multidevice.h>
#include <logging.h>


/*! Number of battery values. */
#define FP_BATTERY_NUM_VALUES                   (0x3)
/*! Battery values bit offset. */
#define FP_BATTERY_NUM_VALUES_BIT_OFFSET        (4)
/*! Length component of lengthtype field. */
#define FP_BATTERY_LENGTH                       (FP_BATTERY_NUM_VALUES << FP_BATTERY_NUM_VALUES_BIT_OFFSET)
/*! Show battery notification on UI. */
#define FP_BATTERY_TYPE_UI_SHOW                 (0x3)
/*! Hide battery notification on UI. */
#define FP_BATTERY_TYPE_UI_HIDE                 (0x4)
/*! Combined length and type show */
#define FP_BATTERY_LENGTHTYPE_SHOW              (FP_BATTERY_LENGTH + FP_BATTERY_TYPE_UI_SHOW)
/*! Combined length and type hide */
#define FP_BATTERY_LENGTHTYPE_HIDE              (FP_BATTERY_LENGTH + FP_BATTERY_TYPE_UI_HIDE)

#if defined(INCLUDE_CASE_COMMS) || defined (INCLUDE_TWS)
/*! Data used for battery notifications as optional extention to account key data in unidentifiable adverts.
    \note The ordering of fields matches the FastPair spec requirements and should not be changed.
*/
static uint8 fp_battery_ntf_data[FP_BATTERY_NOTFICATION_SIZE] = {FP_BATTERY_LENGTHTYPE_HIDE,
                                                                 BATTERY_STATUS_UNKNOWN,
                                                                 BATTERY_STATUS_UNKNOWN,
                                                                 BATTERY_STATUS_UNKNOWN};

uint8* fastPair_BatteryGetData(void)
{
    DEBUG_LOG("fastPair_BatteryGetData");
    return fp_battery_ntf_data;
}

#endif

#ifdef INCLUDE_CASE_COMMS

void fastPair_BatteryHandleCasePowerState(const CASE_POWER_STATE_T* cps)
{
    /* Only if the device, peer is in-case, update the values. This is to avoid UNKNOWN when the device is out-of-case. */
    bool is_local_in_case = (PHY_STATE_IN_CASE ==appPhyStateGetState());
    bool is_peer_in_case = StateProxy_IsPeerInCase();

    uint8 battery_state_left_old,battery_state_right_old,battery_state_case_old;

    DEBUG_LOG("fastPair_BatteryHandleCasePowerState: local %d, peer %d, case %d is_local_in_case %d is_peer_in_case %d",
                   cps->local_battery_state,cps->peer_battery_state,cps->case_battery_state,is_local_in_case,is_peer_in_case);

    /* Store the battery info to be compared with the new values */
    battery_state_left_old = fp_battery_ntf_data[FP_BATTERY_NTF_DATA_LEFT_STATE_OFFSET];
    battery_state_right_old = fp_battery_ntf_data[FP_BATTERY_NTF_DATA_RIGHT_STATE_OFFSET];
    battery_state_case_old = fp_battery_ntf_data[FP_BATTERY_NTF_DATA_CASE_STATE_OFFSET];

    if (Multidevice_IsLeft())
    {
        if(is_local_in_case)
            fp_battery_ntf_data[FP_BATTERY_NTF_DATA_LEFT_STATE_OFFSET] = cps->local_battery_state;
        if(is_peer_in_case)
            fp_battery_ntf_data[FP_BATTERY_NTF_DATA_RIGHT_STATE_OFFSET] = cps->peer_battery_state;
    }
    else
    {
        if(is_peer_in_case)
            fp_battery_ntf_data[FP_BATTERY_NTF_DATA_LEFT_STATE_OFFSET] = cps->peer_battery_state;
        if(is_local_in_case)
            fp_battery_ntf_data[FP_BATTERY_NTF_DATA_RIGHT_STATE_OFFSET] = cps->local_battery_state;
    }
    fp_battery_ntf_data[FP_BATTERY_NTF_DATA_CASE_STATE_OFFSET] = cps->case_battery_state;

    /* bloom filter includes battery state in the hash generation phase, so needs to be updated */
    fastPair_GenerateBloomFilter();
    fastPair_AdvNotifyDataChange();

    /* If the battery values have changed, update it to the message stream devInfo  */
    if( (battery_state_left_old != fp_battery_ntf_data[FP_BATTERY_NTF_DATA_LEFT_STATE_OFFSET]) ||
        (battery_state_right_old != fp_battery_ntf_data[FP_BATTERY_NTF_DATA_RIGHT_STATE_OFFSET]) ||
        (battery_state_case_old != fp_battery_ntf_data[FP_BATTERY_NTF_DATA_CASE_STATE_OFFSET]) )
    {
        /* Inform message stream that a battery update is available */
        fastPair_MsgStreamDevInfo_BatteryUpdateAvailable();
    }
}

void fastPair_BatteryHandleCaseLidState(const CASE_LID_STATE_T* cls)
{
    DEBUG_LOG("fastPair_BatteryHandleCaseLidState");

    if (cls->lid_state == CASE_LID_STATE_OPEN)
    {
        fp_battery_ntf_data[FP_BATTERY_NTF_DATA_LENGTHTYPE_OFFSET] = FP_BATTERY_LENGTHTYPE_SHOW;
    }
    else
    {
        fp_battery_ntf_data[FP_BATTERY_NTF_DATA_LENGTHTYPE_OFFSET] = FP_BATTERY_LENGTHTYPE_HIDE;
    }
    fastPair_GenerateBloomFilter();
    fastPair_AdvNotifyDataChange();
}

#endif

#ifdef INCLUDE_TWS

/*! \brief Handle state proxy events. This function is used to get battery data when earbuds are not in-case.
    \param[in] sp_event State Proxy event message.
*/
void fastPair_HandleStateProxyEvent(const STATE_PROXY_EVENT_T* sp_event)
{
    switch(sp_event->type)
    {
        case state_proxy_event_type_battery_voltage:
        {
            MESSAGE_BATTERY_LEVEL_UPDATE_VOLTAGE_T *battery_voltage = (MESSAGE_BATTERY_LEVEL_UPDATE_VOLTAGE_T *)(&(sp_event->event));
            uint8 battery_percent = Soc_ConvertLevelToPercentage(battery_voltage->voltage_mv);
            state_proxy_source source = sp_event->source;
            bool local_is_left = Multidevice_IsLeft();
            bool isUpdated = FALSE;

            /* Only if the local/peer is NOT in-case, update the values */
            bool is_local_not_in_case = (PHY_STATE_IN_CASE != appPhyStateGetState());
            bool is_peer_not_in_case = !StateProxy_IsPeerInCase();
            bool is_eligible_for_update = FALSE;

            DEBUG_LOG("fastpair_HandleStateProxyEvent: source %u type %u battery_percent %d is_local_not_in_case %d is_peer_not_in_case %d",
                      sp_event->source,sp_event->type,battery_percent,is_local_not_in_case,is_peer_not_in_case);

            is_eligible_for_update = ((source==state_proxy_source_local)&&is_local_not_in_case)||
                                       ((source==state_proxy_source_remote)&&is_peer_not_in_case);

            if(FALSE == is_eligible_for_update)
            {
                DEBUG_LOG("fastpair_HandleStateProxyEvent: Not eligible for update source %u is_local_not_in_case %d is_peer_not_in_case %d "
                          "is_eligible_for_update %d",sp_event->source,is_local_not_in_case,is_peer_not_in_case,is_eligible_for_update);
                return;
            }

            /* If [ local device is left and source is local ] OR [ local device is right and source is remote ] */
            if(((TRUE == local_is_left)&&(source==state_proxy_source_local))||
                    ((FALSE == local_is_left)&&(source==state_proxy_source_remote )))
            {
                if(fp_battery_ntf_data[FP_BATTERY_NTF_DATA_LEFT_STATE_OFFSET] != battery_percent)
                {
                    fp_battery_ntf_data[FP_BATTERY_NTF_DATA_LEFT_STATE_OFFSET] = battery_percent;
                    isUpdated = TRUE;
                }
            }
            else
            {
                if(fp_battery_ntf_data[FP_BATTERY_NTF_DATA_RIGHT_STATE_OFFSET] != battery_percent)
                {
                    fp_battery_ntf_data[FP_BATTERY_NTF_DATA_RIGHT_STATE_OFFSET] = battery_percent;
                    isUpdated = TRUE;
                }
            }

            if(TRUE == isUpdated)
            {
                /* Inform message stream that a battery update is available */
                fastPair_MsgStreamDevInfo_BatteryUpdateAvailable();
            }
        }
        break;
        default:
        {
            DEBUG_LOG("fastpair_HandleStateProxyEvent: Unhandled event source %u type %u", sp_event->source, sp_event->type);
        }
        break;
    }
}
#endif

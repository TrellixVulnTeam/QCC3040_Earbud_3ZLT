/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       fast_pair_profile.c
\brief  Implementation of the profile interface for Google Fast Pair
*/

#ifdef INCLUDE_FAST_PAIR
#include <profile_manager.h>
#include <logging.h>
#include <csrtypes.h>
#include <stdlib.h>
#include <panic.h>
#include <task_list.h>
#include <device_properties.h>

#include "fast_pair.h"
#include "fast_pair_profile.h"
#include "fast_pair_rfcomm.h"
#include "fast_pair_msg_stream_dev_action.h"
#include "bt_device.h"

static void fastPairProfile_ProfileManagerMessageHandler(Task task, MessageId id, Message message);
static const TaskData profile_manager_task = { fastPairProfile_ProfileManagerMessageHandler };

void fastPair_ProfileInit(void)
{
    DEBUG_LOG("fastPair_ProfileInit");
    ProfileManager_ClientRegister((Task) &profile_manager_task);
}

/* On Android handset doesn't send a disconnect request when the disconnection is triggered
   from the BT device menu so we piggy back off the A2DP. */
static void fastPairProfile_HandleDisconnectedProfileInd(DISCONNECTED_PROFILE_IND_T *ind)
{
    if (ind->profile == DEVICE_PROFILE_A2DP)
    {
        bdaddr addr = DeviceProperties_GetBdAddr(ind->device);

        if (appDeviceTypeIsHandset(&addr))
        {
            uint16* msg = PanicUnlessMalloc(sizeof(uint16));
            
            *msg = FP_STOP_RING_BOTH;

            DEBUG_LOG("fastPairProfile_HandleDisconnectedProfileInd: a2dp with %04x %02x %06x", addr.nap, addr.uap, addr.lap);

            /* Do not mute the earbuds when handover is due to RSSI or link quality or battery level. So whenever the new secondary
               device receives the A2DP profile disconnection check for the disconnection reason and if it is due to link transfer
               to new primary device then do not perform anything. During incase handover, fast pair module will mute the earbud as
               part of handling physical state change indication.
             */
            if (ind->reason == profile_manager_disconnected_link_transfer)
            {
                free(msg);
                return;
            }
            else if (ind->reason == profile_manager_disconnected_link_loss)
            {
                /* In link loss scenario stop ringing after 30 seconds */
                MessageSendLater(fpRingDevice_GetTask(), fast_pair_ring_stop_event, msg, FAST_PAIR_STOP_RING_TIMEOUT);
            }
            else
            {
                /* Get the RFCOMM connection  instance for the given bd address */
                fast_pair_rfcomm_data_t* instance = fastPair_RfcommGetInstance(&addr);
                /* Stop ringing as user initiated disconnection */
                MessageSend(fpRingDevice_GetTask(), fast_pair_ring_stop_event, msg);

                if(instance)
                {
                    /* Disconnect RFCOMM connection for this instance */
                    fastPair_RfcommDisconnectInstance(instance);
                }
            }
        }
    }
}

static void fastPairProfile_ProfileManagerMessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch (id)
    {
        case DISCONNECTED_PROFILE_IND:
            fastPairProfile_HandleDisconnectedProfileInd((DISCONNECTED_PROFILE_IND_T *) message);
            break;

        default:
            break;
    }
}

#endif /* INCLUDE_FAST_PAIR */


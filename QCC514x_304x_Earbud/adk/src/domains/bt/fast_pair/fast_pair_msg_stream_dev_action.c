/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       fast_pair_msg_stream_dev_action.c
\brief      Component handling fast pair device action.
*/

#include <bdaddr.h>

#include "fast_pair.h"
#include "fast_pair_config.h"
#include "fast_pair_msg_stream.h"
#include "fast_pair_msg_stream_dev_action.h"
#include "bt_device.h"
#include "multidevice.h"
#include "peer_signalling.h"
#include "fast_pair_rfcomm.h"

#define FASTPAIR_DEVICEACTION_CODE_INDEX  0
#define FASTPAIR_DEVICEACTION_ADD_DATA_LEN_UPPER_INDEX  1
#define FASTPAIR_DEVICEACTION_ADD_DATA_LEN_LOWER_INDEX  2
#define FASTPAIR_DEVICEACTION_ADD_DATA_INDEX 3

#define FASTPAIR_DEVICEACTION_ADD_DATA_LEN 2

#define DEFAULT_RING_TIMES 5

fast_pair_msg_stream_dev_action dev_action_data;

/*! Global Instance of Ring Device Task Data */
fp_ring_device_task_data_t ring_device;

/*! Definition of marshalled messages used for ring device feature. */
const marshal_member_descriptor_t fp_ring_device_req_member_descriptors[] =
{
    MAKE_MARSHAL_MEMBER(fast_pair_ring_device_req_t, uint8, ring_start_stop),
    MAKE_MARSHAL_MEMBER(fast_pair_ring_device_req_t, uint8, ring_time),
};

const marshal_type_descriptor_t marshal_type_descriptor_fast_pair_ring_device_req_t =
    MAKE_MARSHAL_TYPE_DEFINITION(fast_pair_ring_device_req_t, fp_ring_device_req_member_descriptors);

const marshal_type_descriptor_t marshal_type_descriptor_fast_pair_ring_device_cfm_t =
    MAKE_MARSHAL_TYPE_DEFINITION_BASIC(sizeof(fast_pair_ring_device_cfm_t));

/*! X-Macro generate ring device marshal type descriptor set that can be passed to a (un)marshaller to initialise it.
 */
#define EXPAND_AS_TYPE_DEFINITION(type) (const marshal_type_descriptor_t *)&marshal_type_descriptor_##type,
const marshal_type_descriptor_t * const fp_ring_device_marshal_type_descriptors[NUMBER_OF_MARSHAL_DEVICE_ACTION_SYNC_OBJECT_TYPES] = {
    MARSHAL_COMMON_TYPES_TABLE(EXPAND_AS_TYPE_DEFINITION)
    MARSHAL_TYPES_TABLE_RING_DEVICE(EXPAND_AS_TYPE_DEFINITION)
};
#undef EXPAND_AS_TYPE_DEFINITION

#define RINGTONE_STOP_FP  RINGTONE_NOTE(REST, HEMIDEMISEMIQUAVER), RINGTONE_END

/* The ringtone to be played for ringing the device. */

const ringtone_note ringtone_vol32[] =
{
    RINGTONE_VOLUME(32), RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(B6,   SEMIQUAVER),
    RINGTONE_NOTE(G6,   SEMIQUAVER),
    RINGTONE_NOTE(D7,   SEMIQUAVER),
    RINGTONE_NOTE(REST, SEMIQUAVER),
    RINGTONE_NOTE(B6,   SEMIQUAVER),
    RINGTONE_NOTE(G6,   SEMIQUAVER),
    RINGTONE_NOTE(D7,   SEMIQUAVER),
    RINGTONE_STOP_FP
};

const ringtone_note ringtone_vol64[] =
{
    RINGTONE_VOLUME(64), RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(B6,   SEMIQUAVER),
    RINGTONE_NOTE(G6,   SEMIQUAVER),
    RINGTONE_NOTE(D7,   SEMIQUAVER),
    RINGTONE_NOTE(REST, SEMIQUAVER),
    RINGTONE_NOTE(B6,   SEMIQUAVER),
    RINGTONE_NOTE(G6,   SEMIQUAVER),
    RINGTONE_NOTE(D7,   SEMIQUAVER),
    RINGTONE_STOP_FP
};

const ringtone_note ringtone_vol128[] =
{
    RINGTONE_VOLUME(128), RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(B6,   SEMIQUAVER),
    RINGTONE_NOTE(G6,   SEMIQUAVER),
    RINGTONE_NOTE(D7,   SEMIQUAVER),
    RINGTONE_NOTE(REST, SEMIQUAVER),
    RINGTONE_NOTE(B6,   SEMIQUAVER),
    RINGTONE_NOTE(G6,   SEMIQUAVER),
    RINGTONE_NOTE(D7,   SEMIQUAVER),
    RINGTONE_STOP_FP
};

const ringtone_note ringtone_volmax[] =
{
    RINGTONE_VOLUME(255), RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(B6,   SEMIQUAVER),
    RINGTONE_NOTE(G6,   SEMIQUAVER),
    RINGTONE_NOTE(D7,   SEMIQUAVER),
    RINGTONE_NOTE(REST, SEMIQUAVER),
    RINGTONE_NOTE(B6,   SEMIQUAVER),
    RINGTONE_NOTE(G6,   SEMIQUAVER),
    RINGTONE_NOTE(D7,   SEMIQUAVER),
    RINGTONE_STOP_FP
};

static void fastPair_RingDevice_SendMarshalledData(bool ring_start_stop, uint8 ring_time)
{
    fast_pair_ring_device_req_t *ring_device_data;

    if(appPeerSigIsConnected())
    {
        DEBUG_LOG("fastPair_RingDeviceTime_SendMarshalledData. Send Marshalled Data to the peer.");
        ring_device_data = PanicUnlessMalloc(sizeof(fast_pair_ring_device_req_t));

        ring_device_data->ring_start_stop = ring_start_stop;
        ring_device_data->ring_time = ring_time;

        /*! send the ring time to counterpart on other earbud */
        appPeerSigMarshalledMsgChannelTx(fpRingDevice_GetTask(),
                                         PEER_SIG_MSG_CHANNEL_FP_RING_EVENT,
                                         ring_device_data, MARSHAL_TYPE_fast_pair_ring_device_req_t);
    }
    else
    {
        DEBUG_LOG("fastPair_RingDeviceTime_SendMarshalledData. No Peer to send to.");
    }
}

static void fastPair_RingDevice_SendConfirmation(bool synced)
{
    if (appPeerSigIsConnected())
    {
        fast_pair_ring_device_cfm_t* cfm = PanicUnlessMalloc(sizeof(fast_pair_ring_device_cfm_t));
        cfm->synced = synced;
        DEBUG_LOG("fastPair_RingDevice_SendConfirmation. Send confirmation to the peer.");
        /*! send confirmation of ring device received */
        appPeerSigMarshalledMsgChannelTx(fpRingDevice_GetTask(),
                                         PEER_SIG_MSG_CHANNEL_FP_RING_EVENT,
                                         cfm, MARSHAL_TYPE_fast_pair_ring_device_cfm_t);
    }
    else
    {
        DEBUG_LOG("fastPair_RingDevice_SendConfirmation. No Peer to send to.");
    }
}

/*! \brief Parse additional data in device action message.
 */
static void fastPair_MsgStream_RingDevAction_Set(const uint8* dev_action_ring_info, uint16 dev_action_ring_len)
{
    if (dev_action_ring_len == FASTPAIR_DEVICEACTION_ADD_DATA_LEN)
    {
        dev_action_data.ring_component = dev_action_ring_info[0];
        dev_action_data.ring_timeout = dev_action_ring_info[1];
    }
    else
    {
        dev_action_data.ring_component = dev_action_ring_info[0];
        dev_action_data.ring_timeout = 0;
    }
    DEBUG_LOG("fastPair_MsgStream_RingDevAction_Set: ring component - 0x%x", dev_action_data.ring_component);
    DEBUG_LOG("fastPair_MsgStream_RingDevAction_Set: ring duration - 0x%x", dev_action_data.ring_timeout);
}

/*! \brief When device action message to ring is received, mark device is currently ringing and
           start playing the ringtone from low volume level.
 */
static void fastPair_RingTone_Properties_Set(void)
{
        ring_device.is_device_ring = TRUE;
        ring_device.vol_level = ring_vol32;
        ring_device.ringtimes = DEFAULT_RING_TIMES;
}

/*! \brief Keep ringing the device for duration of ring_timeout value in seconds.
 */
static void fastPair_CheckRingDevice_Timeout(void)
{
    if (dev_action_data.ring_timeout != 0)
    {           
        uint16* ring_stop = PanicUnlessMalloc(sizeof(uint16));
            
        *ring_stop = FP_STOP_RING_CURRENT;
        MessageSendLater(fpRingDevice_GetTask(), fast_pair_ring_stop_event, ring_stop, D_SEC(dev_action_data.ring_timeout));
    }
}

/*! \brief Start ringing the device and rampup the volume from low volume to max voume over time.
 */
static void fastPair_HandleRingTone(bool is_ring)
{
    if (is_ring)
    {
        DEBUG_LOG("fastPair_HandleRingTone: Ringing device.");
        if (ring_device.vol_level == ring_vol32)
        {
            appKymeraTonePlay(ringtone_vol32, 0, TRUE, NULL, 0);
            ring_device.ringtimes--;
            if (ring_device.ringtimes == 0) 
            {
                ring_device.vol_level = ring_vol64;
                ring_device.ringtimes = DEFAULT_RING_TIMES;
            }
        }

        else if (ring_device.vol_level == ring_vol64)
        {
            appKymeraTonePlay(ringtone_vol64, 0, TRUE, NULL, 0);
            ring_device.ringtimes--;
            if (ring_device.ringtimes == 0) 
            {
                ring_device.vol_level = ring_vol128;
                ring_device.ringtimes = DEFAULT_RING_TIMES;
            }
        }

        else if (ring_device.vol_level == ring_vol128)
        {
            appKymeraTonePlay(ringtone_vol128, 0, TRUE, NULL, 0);
            ring_device.ringtimes--;
            if (ring_device.ringtimes == 0)
            {
                ring_device.vol_level = ring_volmax;
            }
        }

        /* Play the ringtone at max level until device action is mute. */
        else
        {
            appKymeraTonePlay(ringtone_volmax, 0, TRUE, NULL, 0);
        }

        MessageSendLater(fpRingDevice_GetTask(), fast_pair_ring_event, NULL, D_SEC(3));
    }
    else
    {
        DEBUG_LOG("fastPair_HandleRingTone: Muting device");
        MessageCancelAll(fpRingDevice_GetTask(), fast_pair_ring_event);
        appKymeraTonePromptCancel();
        ring_device.is_device_ring = FALSE;
    }
}

/*! \brief Handle device component ring and mute functionality.
 */
static void fastPair_RingMuteDevice(void)
{
    DEBUG_LOG("fastPair_RingMuteDevice");
    uint8 provider_device_type = Multidevice_GetType();

    DEBUG_LOG("Provider device: 0x%x", provider_device_type);

    /* Do not perform any action if the ring component is invalid. For headset 0x02 is treated as invalid component */
    if ((dev_action_data.ring_component > FASTPAIR_DEVICEACTION_RING_RIGHT_LEFT) || 
        ((provider_device_type == multidevice_type_single) && (dev_action_data.ring_component == FASTPAIR_DEVICEACTION_RING_LEFT_MUTE_RIGHT)))
    {
      DEBUG_LOG("fastPair_RingMuteDevice: Invalid component 0x%x", dev_action_data.ring_component);
      /* Send NACK message with reason as not supported. */
      fastPair_MsgStreamSendNAK(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_DEVCIE_ACTION_EVENT, FASTPAIR_MESSAGESTREAM_DEVACTION_RING_EVENT,
                                FASTPAIR_MESSAGESTREAM_NAK_REASON_NOT_SUPORTED);
      return;
    }

    /* Ring Headset if first byte in additional data is 0x01 or 0x03. */
    if ((provider_device_type == multidevice_type_single) &&
        ((dev_action_data.ring_component == FASTPAIR_DEVICEACTION_RING_RIGHT_MUTE_LEFT) ||
         (dev_action_data.ring_component == FASTPAIR_DEVICEACTION_RING_RIGHT_LEFT)))
    {
        /* When ring device is already initiated by AG1 do not perform ring device again from AG2 */
        if (ring_device.is_device_ring == FALSE)
        {
            fastPair_RingTone_Properties_Set();
            fastPair_CheckRingDevice_Timeout();
            fastPair_HandleRingTone(TRUE);
        }
        return;
    }

    if (dev_action_data.ring_component == FASTPAIR_DEVICEACTION_RING_RIGHT_MUTE_LEFT)
    {
        /* Before performing ring right earbud, 'is_device_ring' flag is set to FALSE. When ring right earbud is initiated,
           start ringing only the right earbud. When mute left earbud is initiated, mute only the left earbud.
         */
        if (ring_device.is_device_ring == FALSE)
        {
            /* If current device is right bud, then ring the device. */
            if (!Multidevice_IsLeft())
            {
                fastPair_RingTone_Properties_Set();
                fastPair_CheckRingDevice_Timeout();
                fastPair_HandleRingTone(TRUE);
            }
            /* If current device is not right bud, the peer must be the right bud. Indicate peer to ring. */
            else
            {
                /* Second byte in additional data determines how long the device should ring. Send this info to peer. */
                fastPair_RingDevice_SendMarshalledData(TRUE, dev_action_data.ring_timeout);
            }
        }

        /* Mute left earbud */
        else
        {
            if (Multidevice_IsLeft())
            {
                fastPair_HandleRingTone(FALSE);
            }
            else
            {
                /* Inform peer left device to mute */
                fastPair_RingDevice_SendMarshalledData(FALSE, dev_action_data.ring_timeout);
            }
        }
    }

    else if (dev_action_data.ring_component == FASTPAIR_DEVICEACTION_RING_LEFT_MUTE_RIGHT)
    {
        /* Before performing ring left earbud, 'is_device_ring' flag is set to FALSE. When ring left earbud is initiated,
           start ringing only the left earbud. When mute right earbud is initiated, mute only the right earbud.
         */
        if (ring_device.is_device_ring == FALSE)
        {
            /* If current device is left bud, then ring the device. */
            if (Multidevice_IsLeft())
            {
                fastPair_RingTone_Properties_Set();
                fastPair_CheckRingDevice_Timeout();
                fastPair_HandleRingTone(TRUE);
            }
            /* If current device is not left bud, indicate peer to ring. */
            else
            {
                fastPair_RingDevice_SendMarshalledData(TRUE, dev_action_data.ring_timeout);
            }
        }

        /* Mute right earbud */
        else
        {
            if (!Multidevice_IsLeft())
            {
                fastPair_HandleRingTone(FALSE);
            }
            else
            {
                /* Inform peer right device to mute */
                fastPair_RingDevice_SendMarshalledData(FALSE, dev_action_data.ring_timeout);
            }
        }
    }

    else if (dev_action_data.ring_component == FASTPAIR_DEVICEACTION_RING_RIGHT_LEFT)
    {
        /* Mute the device first and start ringing the buds in sync. */
        fastPair_RingDevice_SendMarshalledData(FALSE, dev_action_data.ring_timeout);
        fastPair_HandleRingTone(FALSE);

        /* Set ringtone properties */
        fastPair_RingTone_Properties_Set();
        fastPair_CheckRingDevice_Timeout();

        /* Ring both the device. */
        fastPair_RingDevice_SendMarshalledData(TRUE, dev_action_data.ring_timeout);
        fastPair_HandleRingTone(TRUE);
    }

    else
    {
        /* If the ring timeout is still active and user mutes the device then cancel the message
           which was sent to mute device. */
        if (dev_action_data.ring_timeout != 0)
        {
            MessageCancelAll(fpRingDevice_GetTask(), fast_pair_ring_stop_event);
        }
        /* Stop Ringing both the device. */
        if (provider_device_type == multidevice_type_pair)
        {
            fastPair_RingDevice_SendMarshalledData(FALSE, dev_action_data.ring_timeout);
        }
        fastPair_HandleRingTone(FALSE);
    }
}

/*! \brief Handle incoming device action message sent by Seeker.
 */
static void fastPair_DevAction_HandleIncomingData(const uint8* msg_data, uint16 len)
{
    uint16 additional_data_len;
    uint8 msg_code;

    if ((msg_data == NULL) || (len < FASTPAIR_DEVICEACTION_ADD_DATA_INDEX))
    {
        DEBUG_LOG_ERROR("fastPair_DevAction_HandleIncomingData: UNEXPECTED ERROR - Length is %d is less than minimum of %d or data is NULL",
                        len,FASTPAIR_DEVICEACTION_ADD_DATA_INDEX);
        return;
    }

    additional_data_len = (msg_data[FASTPAIR_DEVICEACTION_ADD_DATA_LEN_UPPER_INDEX]<<8) + 
                           msg_data[FASTPAIR_DEVICEACTION_ADD_DATA_LEN_LOWER_INDEX];

    if((FASTPAIR_DEVICEACTION_ADD_DATA_INDEX+additional_data_len) != len)
    {
        DEBUG_LOG_ERROR("fastPair_DevAction_HandleIncomingData: UNEXPECTED length ERROR Length is %d. Should be %d",
                        len,(FASTPAIR_DEVICEACTION_ADD_DATA_INDEX+additional_data_len));
        return;
    }

    msg_code = msg_data[FASTPAIR_DEVICEACTION_CODE_INDEX];
    switch(msg_code)
    {
        case FASTPAIR_MESSAGESTREAM_DEVACTION_RING_EVENT:
        {
          /* Acknowledge ring device message */
          fastPair_MsgStreamSendACK(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_DEVCIE_ACTION_EVENT,FASTPAIR_MESSAGESTREAM_DEVACTION_RING_EVENT);
          fastPair_MsgStream_RingDevAction_Set(&msg_data[FASTPAIR_DEVICEACTION_ADD_DATA_INDEX], additional_data_len);

          /* Check for number of Rfcomm Instances. If RfComm instances are two, sync the ringing status to Handsets */
          if (fastPair_RfcommGetRFCommConnectedInstances() == FASTPAIR_RFCOMM_CONNECTIONS_MAX)
          {
              /* When Ring Left/Right is initiated from AG1, send the ring device message also to AG2 and vice-versa. */
              send_data_to_fp_seeker_number = (ack_msg_to_fp_seeker_number == 1) ? 2 : 1;

              /* Reset FP seeker number for sending ACK message as ACK message for the current ring device message is already sent. */
              ack_msg_to_fp_seeker_number = 0;

              /* Send ring device message to other AG (AG1 or AG2) depending on the value of 'send_data_to_fp_seeker_number' */
              fastPair_MsgStreamSendData(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_DEVCIE_ACTION_EVENT,
                                         FASTPAIR_MESSAGESTREAM_DEVACTION_RING_EVENT,
                                         &dev_action_data.ring_component,
                                         FASTPAIR_DEVICEACTION_RING_RSP_ADD_DATA_LEN);

              /* Reset this global to 0 so that when BT is turned off in AG2 and ring device is initiated from AG1 (and vice-versa),
                 the audio device has to sent ACK message to the current AG */
              send_data_to_fp_seeker_number = 0;
          }

          /* Perform ring/mute device */
          fastPair_RingMuteDevice();
          
        }
        break;

        default:
        {
            /* Acknowledge message */
            fastPair_MsgStreamSendACK(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_DEVCIE_ACTION_EVENT,msg_code);

            DEBUG_LOG_ERROR("fastPair_DevAction_HandleIncomingData: UNHANDLED code %d. ",msg_code);
            return;
        }
    }
}

/* Handle messages from Message stream */
static void fastPair_DevAction_MsgStreamMessageHandler(FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE msg_type, const uint8 *msg_data, uint16 len)
{
    switch(msg_type)
    {
        case FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_CONNECT_IND:
            DEBUG_LOG("FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_CONNECT_IND");
            break;
        case FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_SERVER_CONNECT_CFM:
            DEBUG_LOG("FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_SERVER_CONNECT_CFM");
            break;
        case FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_INCOMING_DATA:
            DEBUG_LOG("FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_INCOMING_DATA");
            fastPair_DevAction_HandleIncomingData(msg_data, len); 
            break;
        case FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_DISCONNECT_IND:
            DEBUG_LOG("FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_DISCONNECT_IND");
            break;
        case FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_DISCONNECT_CFM:
            DEBUG_LOG("FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_DISCONNECT_IND");
            break;
        default:
            DEBUG_LOG("fastPair_DevAction_MsgStreamMessageHandler: unknown message=%x", msg_type);
            break;
    }
}

static void fastPair_RingDevice_HandleMarshalledMsgChannelTxCfm(const PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM_T* cfm)
{
    DEBUG_LOG("fastPair_RingDevice_HandleMarshalledMsgChannelTxCfm channel %u status %u", cfm->channel, cfm->status);
}

/*! \brief Handle incoming marshalled messages from peer ring device component.
 */
static void fastPair_RingDevice_HandleMarshalledMsgChannelRxInd(PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND_T* ind)
{
    switch (ind->type)
    {
        case MARSHAL_TYPE_fast_pair_ring_device_req_t:
        {
            fast_pair_ring_device_req_t* req = (fast_pair_ring_device_req_t*)ind->msg;
            DEBUG_LOG("fastPair_RingDevice_HandleMarshalledMsgChannelRxInd RX Ring Device ");

            if(req->ring_start_stop == TRUE)
            {
                fastPair_RingTone_Properties_Set();
                dev_action_data.ring_timeout = req->ring_time;
                fastPair_CheckRingDevice_Timeout();
                fastPair_HandleRingTone(TRUE);
            }
            else
            {
                if (dev_action_data.ring_timeout != 0)
                {
                    MessageCancelAll(fpRingDevice_GetTask(), fast_pair_ring_stop_event);
                }
                fastPair_HandleRingTone(FALSE);
            }
            /*! Send the confirmation to the peer */
            fastPair_RingDevice_SendConfirmation(TRUE);
            free(req);
        }
        break;

        case MARSHAL_TYPE_fast_pair_ring_device_cfm_t:
        {
            fast_pair_ring_device_cfm_t *cfm = (fast_pair_ring_device_cfm_t*)ind->msg;

            /* Make sure if peer is also ringing and the ringing is syncronized between the buds. */
            if(cfm->synced)
            {
                DEBUG_LOG("fastPair_RingDevice_HandleMarshalledMsgChannelRxInd. Ring Start/Stop Successful.");
            }
            else
            {
                DEBUG_LOG("fastPair_RingDevice_HandleMarshalledMsgChannelRxInd. Failed to Ring Start/Stop.");
            }
            free(cfm);
        }
        break;

        default:
        break;
    }
}

/*! \brief This function is the message handler for fast pair ring device module.
 */
static void fastPair_RingDevice_HandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch (id)
    {
        /* marshalled messaging */
        case PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND:
            DEBUG_LOG("fastPair_RingDevice_HandleMessage. PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND");
            fastPair_RingDevice_HandleMarshalledMsgChannelRxInd((PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND_T*)message);
            break;
        case PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM:
            DEBUG_LOG("fastPair_RingDevice_HandleMessage. PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM");
            fastPair_RingDevice_HandleMarshalledMsgChannelTxCfm((PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM_T*)message);
            break;
        case fast_pair_ring_event:
            DEBUG_LOG("fastPair_RingDevice_HandleMessage. fast_pair_ring_event");
            fastPair_HandleRingTone(TRUE);
            break;
        case fast_pair_ring_stop_event:
            DEBUG_LOG("fastPair_RingDevice_HandleMessage. fast_pair_ring_stop_event");
            if (dev_action_data.ring_timeout != 0)
            {
                MessageCancelAll(fpRingDevice_GetTask(), fast_pair_ring_stop_event);
            }
            /* When earbud goes into case or headset is powered off, stop only ringing device tone. */
            if (ring_device.is_device_ring)
            {
                fastPair_HandleRingTone(FALSE);
            }
            /* If ringing stops due to handset disconnection stop both buds */
            {
                uint16* ring_stop = (uint16*)message;
                if(*ring_stop == FP_STOP_RING_BOTH)
                {
                    uint8 provider_device_type = Multidevice_GetType();
                    if (provider_device_type == multidevice_type_pair)
                    {
                        fastPair_RingDevice_SendMarshalledData(FALSE, dev_action_data.ring_timeout);
                    }
                }
            }
            memset(&dev_action_data, 0, sizeof(dev_action_data));
            break;
        default:
            DEBUG_LOG("fastPair_RingDevice_HandleMessage: unknown message=%x", id);
            break;
    }
}

/*! \brief Fast Pair Device Action Initialization.
 */
void fastPair_MsgStreamDevAction_Init(void)
{
    DEBUG_LOG("fastPair_MsgStreamDevAction_Init");
    memset(&dev_action_data, 0, sizeof(dev_action_data));
    fp_ring_device_task_data_t *ring_device_task_data = fpRingDevice_GetTaskData();

    /* Initialize component task data */
    memset(ring_device_task_data, 0, sizeof(*ring_device_task_data));
    ring_device_task_data->task.handler = fastPair_RingDevice_HandleMessage;

    /* Register with peer signalling to use ring device msg channel */
    appPeerSigMarshalledMsgChannelTaskRegister(fpRingDevice_GetTask(),
                                               PEER_SIG_MSG_CHANNEL_FP_RING_EVENT,
                                               fp_ring_device_marshal_type_descriptors,
                                               NUMBER_OF_MARSHAL_DEVICE_ACTION_SYNC_OBJECT_TYPES);
    /* Handle Device Action messages from Message stream */
    fastPair_MsgStreamRegisterGroupMessages(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_DEVCIE_ACTION_EVENT,
                                            fastPair_DevAction_MsgStreamMessageHandler);
}

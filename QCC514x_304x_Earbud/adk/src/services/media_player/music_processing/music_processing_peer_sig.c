/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   media_player Music Player
\ingroup    services
\brief      A component responsible for updating the peer device with the user EQ information

The Media Player uses \ref audio_domain Audio domain.
*/


#if defined(INCLUDE_MUSIC_PROCESSING) && defined(INCLUDE_MUSIC_PROCESSING_PEER)

#include <music_processing_peer_sig.h>
#include <music_processing.h>
#include "music_processing_marshal_desc.h"

#include <peer_signalling.h>
#include <bt_device.h>
#include <panic.h>
#include <logging.h>
#include <stdlib.h>
#include <byte_utils.h>
#include <system_clock.h>
#include <kymera.h>

#define US_TO_MS(us) ((us) / US_PER_MS)

/* Delay(ms) to allow time to transmit new user EQ information to peer earbud before
   transitioning to new state. */
#define USER_EQ_DELAY (200)

/*! EQ change type */
typedef enum
{
    /*! EQ preset change */
    EQ_CHANGE_TYPE_PRESET = 0,
    /*! EQ gains change */
    EQ_CHANGE_TYPE_GAINS,
}eq_change_type_t;

static void musicProcessingPeerSig_MessageHandler(Task task, MessageId id, Message message);

static const TaskData music_processing_peer_sig_task = {musicProcessingPeerSig_MessageHandler};

static void musicProcessingPeerSig_HandlePeerSigConnectInd(const PEER_SIG_CONNECTION_IND_T *ind);
static void musicProcessingPeerSig_HandlePeerSigMarshalledMsgChannelRxInd(const PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND_T *ind);
static void musicProcessingPeerSig_HandlePeerSigMarshalledMsgChannelTxCfm(const PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM_T *cfm);
static void musicProcessingPeerSig_HandleEQChangeType(eq_change_type_t change_type, marshal_rtime_t timestamp, uint8 payload_length, uint8 *payload);
static void musicProcessingPeerSig_HandleEQChangeTypePreset(marshal_rtime_t timestamp, uint8 payload_length, uint8 *payload);
static void musicProcessingPeerSig_HandleEQChangeTypeUserEqGains(marshal_rtime_t timestamp, uint8 payload_length, uint8 *payload);
inline static void musicProcessingPeerSig_ConvertPayloadEqGains(uint8 number_of_bands, uint8 * payload, int16 * gain);
inline static void musicProcessingPeerSig_ConvertEqGainsToPayload(uint8 number_of_bands, int16 * gain, uint8 * payload);


static rtime_t musicProcessingPeerSig_DelayToTimestamp(uint32 delay)
{
    rtime_t now = SystemClockGetTimerTime();

    DEBUG_LOG_VERBOSE("musicProcessingPeerSig_DelayToTimestamp Now=%d Delay=%d DelayMS=%d", now, delay, (delay * US_PER_MS));

    return rtime_add(now, (delay * US_PER_MS));
}

void MusicProcessingPeerSig_Init(void)
{
    appPeerSigClientRegister((Task)&music_processing_peer_sig_task);
    appPeerSigMarshalledMsgChannelTaskRegister((Task)&music_processing_peer_sig_task,
                                               PEER_SIG_MSG_CHANNEL_USER_EQ,
                                               music_processessing_marshal_type_descriptors,
                                               NUMBER_OF_MARSHAL_OBJECT_TYPES);
}

bool MusicProcessingPeerSig_SetPreset(uint32 *delay, uint8 preset)
{
    bool preset_set = FALSE;

    if(appPeerSigIsConnected())
    {
        music_processing_eq_info_t *msg = PanicUnlessMalloc(sizeof(music_processing_eq_info_t));
        marshal_rtime_t timestamp = SystemClockGetTimerTime();

        if(delay)
        {
            *delay = USER_EQ_DELAY;
            timestamp = musicProcessingPeerSig_DelayToTimestamp(USER_EQ_DELAY);
        }

        msg->timestamp = timestamp;
        msg->eq_change_type = (uint8)EQ_CHANGE_TYPE_PRESET;
        msg->payload_length = 1;
        msg->payload[0] = preset;

        appPeerSigMarshalledMsgChannelTx((Task)&music_processing_peer_sig_task,
                                        PEER_SIG_MSG_CHANNEL_USER_EQ,
                                        msg,
                                        MARSHAL_TYPE_music_processing_eq_info_t);

        preset_set = TRUE;
    }

    return preset_set;
}

bool MusicProcessingPeerSig_SetUserEqBands(uint32 *delay, uint8 start_band, uint8 end_band, int16 * gain)
{
    bool preset_set = FALSE;

    if(appPeerSigIsConnected())
    {
        uint8 number_of_bands = (end_band - start_band) + 1;
        uint8 payload_length = (number_of_bands * sizeof(int16)) + 2;
        music_processing_eq_info_t *msg = PanicUnlessMalloc(sizeof(music_processing_eq_info_t)+ payload_length - 1);
        marshal_rtime_t timestamp = SystemClockGetTimerTime();

        if(delay)
        {
            *delay = USER_EQ_DELAY;
            timestamp = musicProcessingPeerSig_DelayToTimestamp(USER_EQ_DELAY);
        }

        msg->timestamp = timestamp;
        msg->eq_change_type = (uint8)EQ_CHANGE_TYPE_GAINS;
        msg->payload_length = payload_length;
        msg->payload[0] = start_band;
        msg->payload[1] = end_band;

        musicProcessingPeerSig_ConvertEqGainsToPayload(number_of_bands, gain, &msg->payload[2]);

        DEBUG_LOG_VERBOSE("MusicProcessingPeerSig_SetUserEqBands start band %d, end band %d, first gain %d",
                msg->payload[0], msg->payload[1], msg->payload[2]);
        DEBUG_LOG_DATA_VERBOSE(msg->payload, msg->payload_length);

        appPeerSigMarshalledMsgChannelTx((Task)&music_processing_peer_sig_task,
                                        PEER_SIG_MSG_CHANNEL_USER_EQ,
                                        msg,
                                        MARSHAL_TYPE_music_processing_eq_info_t);

        preset_set = TRUE;
    }

    return preset_set;
}

/*! \brief Handles peer signalling messages

    \param task     Task to handle

    \param id       Message ID

    \param message  Message
*/
static void musicProcessingPeerSig_MessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);

    DEBUG_LOG_VERBOSE("musicProcessingPeerSig_MessageHandler ID = %d", id);

    switch (id)
    {
        case PEER_SIG_CONNECTION_IND:
            musicProcessingPeerSig_HandlePeerSigConnectInd((PEER_SIG_CONNECTION_IND_T *)message);
            break;

        case PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND:
            musicProcessingPeerSig_HandlePeerSigMarshalledMsgChannelRxInd((PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND_T *)message);
            break;

        case PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM:
            musicProcessingPeerSig_HandlePeerSigMarshalledMsgChannelTxCfm((PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM_T *)message);
            break;

        default:
            break;
    }
}

/*! \brief Handles a peer signalling connection indication

    \param ind  Indication
*/
static void musicProcessingPeerSig_HandlePeerSigConnectInd(const PEER_SIG_CONNECTION_IND_T *ind)
{
    DEBUG_LOG_VERBOSE("musicProcessingPeerSig_HandlePeerSigConnectInd");
    if(ind->status == peerSigStatusConnected && BtDevice_IsMyAddressPrimary())
    {
        uint8 num_of_bands = MusicProcessing_GetNumberOfActiveBands();
        int16 *gains = PanicUnlessMalloc(num_of_bands * sizeof(int16));
        uint8 i;

        for(i = 0; i < num_of_bands; ++i)
        {
            kymera_eq_paramter_set_t param_set;
            Kymera_GetEqBandInformation(i, &param_set);
            gains[i] = param_set.gain;
        }

        MusicProcessingPeerSig_SetUserEqBands(NULL, 0, num_of_bands - 1, gains);

        free(gains);

        MusicProcessingPeerSig_SetPreset(NULL, MusicProcessing_GetActiveEqType());
    }
}

/*! \brief Handles a peer signalling Rx message

    \param ind  Indication
*/
static void musicProcessingPeerSig_HandlePeerSigMarshalledMsgChannelRxInd(const PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND_T *ind)
{
    DEBUG_LOG_VERBOSE("musicProcessingPeerSig_HandlePeerSigMarshalledMsgChannelRxInd, channel=%d type=%d",
              ind->channel, ind->type);

    if(ind->type == MARSHAL_TYPE_music_processing_eq_info_t)
    {
        /* Store the received information in the database. */
        music_processing_eq_info_t *msg = (music_processing_eq_info_t *)ind->msg;

        DEBUG_LOG_VERBOSE("musicProcessingPeerSig_HandlePeerSigMarshalledMsgChannelRxInd: EQ change type = %d, payload length = %d",
                  msg->eq_change_type, msg->payload_length);

        musicProcessingPeerSig_HandleEQChangeType(msg->eq_change_type, msg->timestamp, msg->payload_length, msg->payload);

        /* Free unmarshalled message after use. */
        free(ind->msg);
    }
}

/*! \brief Handles a peer signalling Tx message

    \param ind  Indication
*/
static void musicProcessingPeerSig_HandlePeerSigMarshalledMsgChannelTxCfm(const PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM_T *cfm)
{
    DEBUG_LOG_VERBOSE("musicProcessingPeerSig_HandlePeerSigMarshalledMsgChannelTxCfm, channel=%d type=%d status=%d",
              cfm->channel, cfm->type, cfm->status);

    if ((cfm->channel == PEER_SIG_MSG_CHANNEL_VOICE_UI) &&
        (cfm->type == MARSHAL_TYPE_music_processing_eq_info_t) &&
        (cfm->status == peerSigStatusSuccess))
    {
        DEBUG_LOG_VERBOSE("musicProcessingPeerSig_HandlePeerSigMarshalledMsgChannelTxCfm, peer sync complete");
    }
}

/*! \brief Main handler of change types

    \param change_type      User EQ change type

    \param payload_length   Payload length

    \param payload          Payload
*/
static void musicProcessingPeerSig_HandleEQChangeType(eq_change_type_t change_type, marshal_rtime_t timestamp, uint8 payload_length, uint8 *payload)
{
    DEBUG_LOG_VERBOSE("musicProcessingPeerSig_MessageHandler Change type = %d", change_type);

    if ((payload_length > 0) && (payload != NULL))
    {
        switch (change_type)
        {
            case EQ_CHANGE_TYPE_PRESET:
                musicProcessingPeerSig_HandleEQChangeTypePreset(timestamp, payload_length, payload);
                break;

            case EQ_CHANGE_TYPE_GAINS:
                musicProcessingPeerSig_HandleEQChangeTypeUserEqGains(timestamp, payload_length, payload);
                break;

            default:
                DEBUG_LOG_VERBOSE("musicProcessingPeerSig_MessageHandler UNKNOWN CHANGE TYPE");
                break;
        }
    }
    else
    {
        DEBUG_LOG_VERBOSE("musicProcessingPeerSig_MessageHandler INVALID payload length or payload");
    }
}

/*! \brief Handles a User EQ preset change

    \param payload_length   Payload length

    \param payload          Payload
*/
static void musicProcessingPeerSig_HandleEQChangeTypePreset(marshal_rtime_t timestamp, uint8 payload_length, uint8 *payload)
{
    DEBUG_LOG_VERBOSE("musicProcessingPeerSig_HandleEQChangeTypePreset");

    if (payload_length == 1)
    {
        rtime_t now = SystemClockGetTimerTime();
        uint32 delay_ms = 0;
        if(rtime_gt(timestamp, now))
        {
            delay_ms = rtime_sub(timestamp, now)/1000;
        }

        DEBUG_LOG_VERBOSE("musicProcessingPeerSig_HandleEQChangeTypePreset delay %ld preset %d", delay_ms, payload[0]);
        Kymera_SelectEqBank(delay_ms, payload[0]);
    }
}

/*! \brief Handles a User EQ gains change

    \param payload_length   Payload length

    \param payload          Payload
*/
static void musicProcessingPeerSig_HandleEQChangeTypeUserEqGains(marshal_rtime_t timestamp, uint8 payload_length, uint8 *payload)
{
    DEBUG_LOG_VERBOSE("musicProcessingPeerSig_HandleEQChangeTypeUserEqGains");

    if(payload_length > 2)
    {
        uint8 start_band = payload[0];
        uint8 end_band = payload[1];
        uint8 number_of_bands = (end_band - start_band) + 1;

        if (payload_length == ((number_of_bands * 2) + 2))
        {
            int16 * gains = PanicUnlessMalloc(number_of_bands * sizeof(int16));
            rtime_t now = SystemClockGetTimerTime();
            uint32 delay_ms = 0;
            if(rtime_gt(timestamp, now))
            {
                delay_ms = rtime_sub(timestamp, now)/1000;
            }

            DEBUG_LOG_VERBOSE("musicProcessingPeerSig_HandleEQChangeTypeUserEqGains, delay %ld start band=%d, end band=%d",
                    delay_ms, start_band, end_band);
            DEBUG_LOG_DATA_VERBOSE(payload, payload_length);

            musicProcessingPeerSig_ConvertPayloadEqGains(number_of_bands, &payload[2], gains);

            Kymera_SetUserEqBands(delay_ms, start_band, end_band, gains);

            free(gains);
        }
        else
        {
            DEBUG_LOG_VERBOSE("musicProcessingPeerSig_HandleEQChangeTypeUserEqGains, invalid payload = %d on the number of bands check", payload_length);
        }
    }
    else
    {
        DEBUG_LOG_VERBOSE("musicProcessingPeerSig_HandleEQChangeTypeUserEqGains, invalid starting payload = %d", payload_length);
    }

}

/*! \brief Converts the payloads to EQ band gains

    \param number_of_bands  Number EQ band

    \param payload          Payload

    \param gain             EQ gain
*/
inline static void musicProcessingPeerSig_ConvertPayloadEqGains(uint8 number_of_bands, uint8 * payload, int16 * gain)
{
    uint8 current_band;
    uint8 data_pointer = 0;

    DEBUG_LOG_VERBOSE("musicProcessingPeerSig_ConvertPayloadEqGains");

    for (current_band = 0; current_band < number_of_bands; current_band++)
    {
        gain[current_band] = (int16)ByteUtilsGet2BytesFromStream(&payload[data_pointer]);
        data_pointer += sizeof(int16);

        DEBUG_LOG_VERBOSE("musicProcessingPeerSig_ConvertPayloadEqGains, band=%d, gain=%d", current_band, gain[current_band]);
    }
}

/*! \brief Converts the EQ band gains to payload

    \param number_of_bands  Number EQ band

    \param payload          Payload

    \param gain             EQ gain
*/
inline static void musicProcessingPeerSig_ConvertEqGainsToPayload(uint8 number_of_bands, int16 * gain, uint8 * payload)
{
    uint8 current_band;
    uint8 data_pointer = 0;

    DEBUG_LOG_VERBOSE("musicProcessingPeerSig_ConvertEqGainsToPayload");

    for (current_band = 0; current_band <= number_of_bands; current_band++)
    {
        uint16 cast_gain = (uint16)gain[current_band];
        data_pointer += ByteUtilsMemCpyUnpackString(&payload[data_pointer], &cast_gain, sizeof(uint16));
    }
}

#endif /* INCLUDE_MUSIC_PROCESSING && INCLUDE_MUSIC_PROCESSING_PEER */

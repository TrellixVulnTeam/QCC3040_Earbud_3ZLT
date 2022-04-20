/*!
\copyright  Copyright (c) 2017-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera.c
\brief      Kymera Manager
*/

#include "kymera.h"
#include "kymera_a2dp.h"
#include "kymera_wired_analog.h"
#include "kymera_le_audio.h"
#include "kymera_le_voice.h"
#include "kymera_common.h"
#include "kymera_config.h"
#include "kymera_sco_private.h"
#include "kymera_adaptive_anc.h"
#include "kymera_aec.h"
#include "kymera_loopback_audio.h"
#include "kymera_music_processing.h"
#include "kymera_lock.h"
#include "kymera_state.h"
#include "kymera_op_msg.h"
#include "kymera_internal_msg_ids.h"
#include "kymera_output.h"
#include "kymera_anc.h"
#include "av.h"
#include "a2dp_profile.h"
#include "usb_common.h"
#include "power_manager.h"
#include "kymera_latency_manager.h"
#include "kymera_dynamic_latency.h"
#include "mirror_profile.h"
#include "anc_state_manager.h"
#include <vmal.h>
#include "kymera_usb_audio.h"
#include "kymera_usb_voice.h"
#include "kymera_usb_sco.h"
#include "kymera_va.h"
#include "kymera_fit_test.h"
#include "kymera_tones_prompts.h"
#include "kymera_leakthrough.h"
#include "kymera_setup.h"
#include <task_list.h>
#include <logging.h>
#include <ps.h>
#include <rtime.h>

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_ENUM(app_kymera_internal_message_ids)
LOGGING_PRESERVE_MESSAGE_ENUM(kymera_messages)
LOGGING_PRESERVE_MESSAGE_TYPE(kymera_msg_t)

#ifndef HOSTED_TEST_ENVIRONMENT

/*! There is checking that the messages assigned by this module do
not overrun into the next module's message ID allocation */
ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(KYMERA, KYMERA_MESSAGE_END)

#endif


/*!< State data for the DSP configuration */
kymeraTaskData  app_kymera;

/*! Macro for creating messages */
#define MAKE_KYMERA_MESSAGE(TYPE) \
    TYPE##_T *message = PanicUnlessNew(TYPE##_T);

/*! \brief The KYMERA_INTERNAL_LE_VOICE_MIC_MUTE message content. */
typedef KYMERA_INTERNAL_SCO_MIC_MUTE_T KYMERA_INTERNAL_LE_VOICE_MIC_MUTE_T;

static const appKymeraScoChainInfo *appKymeraScoChainTable = NULL;

static const capability_bundle_config_t *bundle_config = NULL;

static const kymera_chain_configs_t *chain_configs = NULL;

static const kymera_callback_configs_t *callback_configs = NULL;

#define GetKymeraClients() TaskList_GetBaseTaskList(&KymeraGetTaskData()->client_tasks)

static void appKymeraScoStartHelper(Sink audio_sink, const appKymeraScoChainInfo *info, uint8 wesco,
                                    int16 volume_in_db, uint8 pre_start_delay, bool conditionally,
                                    bool synchronised_start, Kymera_ScoStartedHandler started_handler)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_SCO_START);
    PanicNull(audio_sink);

    message->audio_sink = audio_sink;
    message->wesco      = wesco;
    message->volume_in_db     = volume_in_db;
    message->pre_start_delay = pre_start_delay;
    message->sco_info   = info;
    message->synchronised_start = synchronised_start;
    message->started_handler = started_handler;
    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_SCO_START, message, conditionally ? &theKymera->lock : NULL);
}

static const appKymeraScoChainInfo *appKymeraScoFindChain(const appKymeraScoChainInfo *info, appKymeraScoMode mode, uint8 mic_cfg)
{
    while (info->mode != NO_SCO)
    {
        if ((info->mode == mode) && (info->mic_cfg == mic_cfg))
        {
            return info;
        }

        info++;
    }
    return NULL;
}

/*! \brief Notify Kymera to update registered clients. */
static void appKymera_MsgRegisteredClients( MessageId id, uint16 info )
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    if(theKymera->client_tasks) /* Check if any of client registered */
    {
        MESSAGE_MAKE(ind, kymera_aanc_event_msg_t);
        ind->info = info;

        TaskList_MessageSendWithSize(theKymera->client_tasks, id, ind, sizeof(ind));
    }
}

static void kymera_dsp_msg_handler(MessageFromOperator *op_msg)
{
    uint16 msg_id, event_id;


    msg_id = op_msg->message[KYMERA_OP_MSG_WORD_MSG_ID];
    event_id = op_msg->message[KYMERA_OP_MSG_WORD_EVENT_ID];

    DEBUG_LOG("KYMERA_OP_UNSOLICITED_MSG_ID: enum:kymera_op_unsolicited_message_ids_t:%d, EVENT_ID:%d", msg_id, event_id);

    switch (msg_id)
    {
        case KYMERA_OP_MSG_ID_TONE_END:
        {
            DEBUG_LOG("KYMERA_OP_MSG_ID_TONE_END");


            PanicFalse(op_msg->len == KYMERA_OP_MSG_LEN);

            appKymeraTonePromptStop();
            Kymera_LatencyManagerHandleToneEnd();
        }
        break;

        case KYMERA_OP_MSG_ID_AANC_EVENT_TRIGGER:
        {
            DEBUG_LOG("KYMERA_OP_MSG_ID_AANC_EVENT_TRIGGER Event: enum:kymera_aanc_op_event_ids_t:%d",event_id);
            if (event_id ==  KYMERA_AANC_EVENT_ED_ACTIVE )
            {
                appKymera_MsgRegisteredClients(KYMERA_AANC_ED_ACTIVE_TRIGGER_IND,
                                               op_msg->message[KYMERA_OP_MSG_WORD_PAYLOAD_0]);
            }
            else if (event_id ==  KYMERA_AANC_EVENT_ED_INACTIVE_GAIN_UNCHANGED )
            {
                appKymera_MsgRegisteredClients(KYMERA_AANC_ED_INACTIVE_TRIGGER_IND,
                                               op_msg->message[KYMERA_OP_MSG_WORD_PAYLOAD_0]);
            }
            else if (event_id ==  KYMERA_AANC_EVENT_QUIET_MODE )
            {
                appKymera_MsgRegisteredClients(KYMERA_AANC_QUIET_MODE_TRIGGER_IND,
                                               KYMERA_OP_MSG_WORD_PAYLOAD_NA);
            }
            else if (event_id ==  KYMERA_AANC_EVENT_BAD_ENVIRONMENT )
            {
                appKymera_MsgRegisteredClients(KYMERA_AANC_BAD_ENVIRONMENT_TRIGGER_IND,
                                               op_msg->message[KYMERA_OP_MSG_WORD_PAYLOAD_0]);
            }
            else
            {
                /* ignore */
            }
        }
        break;

        case KYMERA_OP_MSG_ID_AANC_EVENT_CLEAR:
        {
            DEBUG_LOG("KYMERA_OP_MSG_ID_AANC_EVENT_CLEAR Event: enum:kymera_aanc_op_event_ids_t:%d", event_id);

            if (event_id ==  KYMERA_AANC_EVENT_ED_ACTIVE )
            {
                appKymera_MsgRegisteredClients(KYMERA_AANC_ED_ACTIVE_CLEAR_IND,
                                               op_msg->message[KYMERA_OP_MSG_WORD_PAYLOAD_0]);
            }
            else if (event_id ==  KYMERA_AANC_EVENT_ED_INACTIVE_GAIN_UNCHANGED )
            {
                appKymera_MsgRegisteredClients(KYMERA_AANC_ED_INACTIVE_CLEAR_IND,
                                               op_msg->message[KYMERA_OP_MSG_WORD_PAYLOAD_0]);
            }
            else if (event_id ==  KYMERA_AANC_EVENT_QUIET_MODE )
            {
                appKymera_MsgRegisteredClients(KYMERA_AANC_QUIET_MODE_CLEAR_IND,
                                               KYMERA_OP_MSG_WORD_PAYLOAD_NA);
            }
            else if (event_id ==  KYMERA_AANC_EVENT_BAD_ENVIRONMENT )
            {
                appKymera_MsgRegisteredClients(KYMERA_AANC_BAD_ENVIRONMENT_CLEAR_IND,
                                               op_msg->message[KYMERA_OP_MSG_WORD_PAYLOAD_0]);
            }
            else
            {
                /* ignore */
            }
        }
        break;

        case KYMERA_OP_MSG_ID_FIT_TEST:
        {
            if(event_id == KYMERA_FIT_TEST_EVENT_ID)
            {
                if(op_msg->message[KYMERA_OP_MSG_WORD_PAYLOAD_0] != KYMERA_FIT_TEST_RESULT_BAD)
                {
                    appKymera_MsgRegisteredClients(KYMERA_EFT_GOOD_FIT_IND, KYMERA_OP_MSG_WORD_PAYLOAD_NA);
                    DEBUG_LOG_ALWAYS("kymera_dsp_msg_handler, Good Fit!!");
                }
                else
                {
                    appKymera_MsgRegisteredClients(KYMERA_EFT_BAD_FIT_IND, KYMERA_OP_MSG_WORD_PAYLOAD_NA);
                    DEBUG_LOG_ALWAYS("kymera_dsp_msg_handler, Bad Fit!!");
                }
            }
        }
        break;

        default:
        break;
    }
}

void appKymeraPromptPlay(FILE_INDEX prompt, promptFormat format, uint32 rate, rtime_t ttp,
                         bool interruptible, uint16 *client_lock, uint16 client_lock_mask)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    DEBUG_LOG("appKymeraPromptPlay, queue prompt %d, int %u", prompt, interruptible);

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_TONE_PROMPT_PLAY);
    message->tone = NULL;
    message->prompt = prompt;
    message->prompt_format = format;
    message->rate = rate;
    message->time_to_play = ttp;
    message->interruptible = interruptible;
    message->client_lock = client_lock;
    message->client_lock_mask = client_lock_mask;

    MessageCancelFirst(&theKymera->task, KYMERA_INTERNAL_PREPARE_FOR_PROMPT_TIMEOUT);
    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_TONE_PROMPT_PLAY, message, &theKymera->lock);
    theKymera->tone_count++;
}

void appKymeraTonePlay(const ringtone_note *tone, rtime_t ttp, bool interruptible,
                       uint16 *client_lock, uint16 client_lock_mask)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    DEBUG_LOG("appKymeraTonePlay, queue tone %p, int %u", tone, interruptible);

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_TONE_PROMPT_PLAY);
    message->tone = tone;
    message->prompt = FILE_NONE;
    message->rate = KYMERA_TONE_GEN_RATE;
    message->time_to_play = ttp;
    message->interruptible = interruptible;
    message->client_lock = client_lock;
    message->client_lock_mask = client_lock_mask;

    MessageCancelFirst(&theKymera->task, KYMERA_INTERNAL_PREPARE_FOR_PROMPT_TIMEOUT);
    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_TONE_PROMPT_PLAY, message, &theKymera->lock);
    theKymera->tone_count++;
}

void appKymeraTonePromptCancel(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    DEBUG_LOG("appKymeraTonePromptCancel");

    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_TONE_PROMPT_STOP, NULL, &theKymera->lock);
}

void appKymeraCancelA2dpStart(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    MessageCancelAll(&theKymera->task, KYMERA_INTERNAL_A2DP_START);
    appKymeraClearA2dpStartingLock(theKymera);
}

void appKymeraA2dpStart(uint16 *client_lock, uint16 client_lock_mask,
                        const a2dp_codec_settings *codec_settings,
                        uint32 max_bitrate,
                        int16 volume_in_db, uint8 master_pre_start_delay,
                        uint8 q2q_mode, aptx_adaptive_ttp_latencies_t nq2q_ttp)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOG("appKymeraA2dpStart, seid %u, lock %u, busy_lock %u, q2q %u, features 0x%x", codec_settings->seid, theKymera->lock, theKymera->busy_lock, q2q_mode, codec_settings->codecData.aptx_ad_params.features);

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_A2DP_START);
    message->lock = client_lock;
    message->lock_mask = client_lock_mask;
    message->codec_settings = *codec_settings;
    message->volume_in_db = volume_in_db;
    message->master_pre_start_delay = master_pre_start_delay;
    message->q2q_mode = q2q_mode;
    message->nq2q_ttp = nq2q_ttp;
    message->max_bitrate = max_bitrate;
    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_A2DP_START,
                             message,
                             &theKymera->lock);
}

void appKymeraA2dpStop(uint8 seid, Source source)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    MessageId mid = appA2dpIsSeidSource(seid) ? KYMERA_INTERNAL_A2DP_STOP_FORWARDING :
                                                KYMERA_INTERNAL_A2DP_STOP;
    DEBUG_LOG("appKymeraA2dpStop, seid %u", seid);

    /*Cancel any pending KYMERA_INTERNAL_A2DP_AUDIO_SYNCHRONISED message.
      A2DP might have been stopped while Audio Synchronization is still incomplete,
      in which case this timed message needs to be cancelled.
     */
    MessageCancelAll(&theKymera->task, KYMERA_INTERNAL_A2DP_AUDIO_SYNCHRONISED);

    /*Cancel any pending KYMERA_INTERNAL_A2DP_START message.*/
    appKymeraCancelA2dpStart();

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_A2DP_STOP);
    message->seid = seid;
    message->source = source;
    MessageSendConditionally(&theKymera->task, mid, message, &theKymera->lock);
}

void appKymeraA2dpSetVolume(int16 volume_in_db)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOG("appKymeraA2dpSetVolume, volume %d", volume_in_db);

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_A2DP_SET_VOL);
    message->volume_in_db = volume_in_db;
    MessageCancelFirst(&theKymera->task, KYMERA_INTERNAL_A2DP_SET_VOL);
    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_A2DP_SET_VOL, message, &theKymera->lock);
}

bool appKymeraScoStart(Sink audio_sink, appKymeraScoMode mode, uint8 wesco, int16 volume_in_db, uint8 pre_start_delay,
                       bool synchronised_start, Kymera_ScoStartedHandler handler)
{
    uint8 mic_cfg = Kymera_GetNumberOfMics();
    const appKymeraScoChainInfo *info = appKymeraScoFindChain(appKymeraScoChainTable, mode, mic_cfg);
    if (!info)
        info = appKymeraScoFindChain(appKymeraScoChainTable,
                                     mode, mic_cfg);

    if (info)
    {
        DEBUG_LOG("appKymeraScoStart, queue sink 0x%x", audio_sink);

        if (audio_sink)
        {
            DEBUG_LOG("appKymeraScoStart, queue sink 0x%x", audio_sink);
            appKymeraScoStartHelper(audio_sink, info, wesco, volume_in_db, pre_start_delay, TRUE,
                                    synchronised_start, handler);
            return TRUE;
        }
        else
        {
            DEBUG_LOG("appKymeraScoStart, invalid sink");
            return FALSE;
        }
    }
    else
    {
        DEBUG_LOG("appKymeraScoStart, failed to find suitable SCO chain");
        return FALSE;
    }
}

void appKymeraScoStop(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOG("appKymeraScoStop");

    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_SCO_STOP, NULL, &theKymera->lock);
}

void appKymeraScoSetVolume(int16 volume_in_db)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    DEBUG_LOG("appKymeraScoSetVolume msg, vol %u", volume_in_db);

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_SCO_SET_VOL);
    message->volume_in_db = volume_in_db;
    MessageCancelFirst(&theKymera->task, KYMERA_INTERNAL_SCO_SET_VOL);
    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_SCO_SET_VOL, message, &theKymera->lock);
}

void appKymeraScoMicMute(bool mute)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    DEBUG_LOG("appKymeraScoMicMute msg, mute %u", mute);

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_SCO_MIC_MUTE);
    message->mute = mute;
    MessageSend(&theKymera->task, KYMERA_INTERNAL_SCO_MIC_MUTE, message);
}

void appKymeraProspectiveDspPowerOn(void)
{
    switch (appKymeraGetState())
    {
        case KYMERA_STATE_IDLE:
        case KYMERA_STATE_A2DP_STARTING_A:
        case KYMERA_STATE_A2DP_STARTING_B:
        case KYMERA_STATE_A2DP_STARTING_C:
        case KYMERA_STATE_A2DP_STREAMING:
        case KYMERA_STATE_A2DP_STREAMING_WITH_FORWARDING:
        case KYMERA_STATE_SCO_ACTIVE:
        case KYMERA_STATE_SCO_SLAVE_ACTIVE:
        case KYMERA_STATE_TONE_PLAYING:
        case KYMERA_STATE_USB_AUDIO_ACTIVE:
        case KYMERA_STATE_USB_VOICE_ACTIVE:
        case KYMERA_STATE_USB_SCO_VOICE_ACTIVE:
            if (MessageCancelFirst(KymeraGetTask(), KYMERA_INTERNAL_PROSPECTIVE_POWER_OFF))
            {
                /* Already prospectively on, just re-start off timer */
                DEBUG_LOG("appKymeraProspectiveDspPowerOn already on, restart timer");
            }
            else
            {
                DEBUG_LOG("appKymeraProspectiveDspPowerOn starting");
                appPowerPerformanceProfileRequest();
                OperatorsFrameworkEnable();
                appPowerPerformanceProfileRelinquish();
            }
            MessageSendLater(KymeraGetTask(), KYMERA_INTERNAL_PROSPECTIVE_POWER_OFF, NULL,
                             appConfigProspectiveAudioOffTimeout());
        break;

        default:
        break;
    }
}

/* Handle KYMERA_INTERNAL_PROSPECTIVE_POWER_OFF - switch off DSP again */
static void appKymeraHandleProspectivePowerOff(void)
{
    DEBUG_LOG("appKymeraHandleProspectivePowerOff");
    OperatorsFrameworkDisable();
}

static void appKymeraHandleInternalScoAudioSynchronised(void)
{
    if (appKymeraGetState() == KYMERA_STATE_SCO_ACTIVE)
    {
        DEBUG_LOG("appKymeraHandleInternalScoAudioSynchronised");
        KymeraOutput_MuteMainChannel(FALSE);
    }
}

static void kymera_msg_handler(Task task, MessageId id, Message msg)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    UNUSED(task);

    switch (id)
    {
        case MESSAGE_FROM_OPERATOR:
            kymera_dsp_msg_handler((MessageFromOperator *)msg);
        break;

        case MESSAGE_SOURCE_EMPTY:
        break;

        case MESSAGE_STREAM_DISCONNECT:
        {
            DEBUG_LOG("appKymera MESSAGE_STREAM_DISCONNECT");
#ifdef INCLUDE_MIRRORING
            const MessageStreamDisconnect *msd = msg;
            if (msd->source == theKymera->sync_info.source ||
                msd->source == MirrorProfile_GetA2dpAudioSyncTransportSource())
            {
                /* This is the stream associated with the TWM audio sync stream,
                   not the tone stream, do not stop playing the tone. */
                break;
            }
#endif

#ifdef ENABLE_EARBUD_FIT_TEST
            if(KymeraFitTest_PromptReplayRequired())
            {
                KymeraFitTest_ReplayPrompt();
                break;
            }
#endif

            appKymeraTonePromptStop();
            appKymera_MsgRegisteredClients(KYMERA_PROMPT_END_IND,KYMERA_OP_MSG_WORD_PAYLOAD_NA);
        }
        break;

        case KYMERA_INTERNAL_A2DP_START:
        {
            const KYMERA_INTERNAL_A2DP_START_T *m = (const KYMERA_INTERNAL_A2DP_START_T *)msg;
            uint8 seid = m->codec_settings.seid;

            /* Check if we are busy (due to other chain in use) */
            if (!appA2dpIsSeidSource(seid) && theKymera->busy_lock)
            {
               /* Re-send message blocked on busy_lock */
                MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_A2DP_START);
                *message = *m;
                MessageSendConditionally(&theKymera->task, id, message, &theKymera->busy_lock);
                break;
            }

            /* If there is no pre-start delay, or during the pre-start delay, the
            start can be cancelled if there is a stop on the message queue */
            MessageId mid = appA2dpIsSeidSource(seid) ? KYMERA_INTERNAL_A2DP_STOP_FORWARDING :
                                                        KYMERA_INTERNAL_A2DP_STOP;
            if (MessageCancelFirst(&theKymera->task, mid))
            {
                /* A stop on the queue was cancelled, clear the starter's lock
                and stop starting */
                DEBUG_LOG("appKymera not starting due to queued stop, seid=%u", seid);
                if (m->lock)
                {
                    *m->lock &= ~m->lock_mask;
                }
                /* Also clear kymera's lock, since no longer starting */
                appKymeraClearA2dpStartingLock(theKymera);
                break;
            }
            if (m->master_pre_start_delay)
            {
                /* Send another message before starting kymera. */
                MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_A2DP_START);
                *message = *m;
                --message->master_pre_start_delay;
                MessageSend(&theKymera->task, id, message);
                appKymeraSetA2dpStartingLock(theKymera);
                break;
            }
        }
        // fallthrough (no message cancelled, zero master_pre_start_delay)
        case KYMERA_INTERNAL_A2DP_STARTING:
        {
#if defined(INCLUDE_MIRRORING) || defined(INCLUDE_STEREO)
            const KYMERA_INTERNAL_A2DP_START_T *m = (const KYMERA_INTERNAL_A2DP_START_T *)msg;
            if (Kymera_A2dpHandleInternalStart(m))
            {
                /* Start complete, clear locks. */
                appKymeraClearA2dpStartingLock(theKymera);
                if (m->lock)
                {
                    *m->lock &= ~m->lock_mask;
                }

                TaskList_MessageSendId(theKymera->listeners, KYMERA_NOTIFICATION_EQ_AVAILABLE);
            }
            else
            {
                /* Start incomplete, send another message. */
                MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_A2DP_START);
                *message = *m;
                MessageSend(&theKymera->task, KYMERA_INTERNAL_A2DP_STARTING, message);
                appKymeraSetA2dpStartingLock(theKymera);
            }
#endif
        }
        break;

        case KYMERA_INTERNAL_A2DP_STOP:
        case KYMERA_INTERNAL_A2DP_STOP_FORWARDING:
#if defined(INCLUDE_MIRRORING) || defined(INCLUDE_STEREO)
            Kymera_A2dpHandleInternalStop(msg);
            TaskList_MessageSendId(theKymera->listeners, KYMERA_NOTIFICATION_EQ_UNAVAILABLE);
#endif
        break;

        case KYMERA_INTERNAL_A2DP_SET_VOL:
        {
#if defined(INCLUDE_MIRRORING) || defined(INCLUDE_STEREO)
            KYMERA_INTERNAL_A2DP_SET_VOL_T *m = (KYMERA_INTERNAL_A2DP_SET_VOL_T *)msg;
            Kymera_A2dpHandleInternalSetVolume(m->volume_in_db);
#endif
        }
        break;

        case KYMERA_INTERNAL_SCO_START:
        {
            const KYMERA_INTERNAL_SCO_START_T *m = (const KYMERA_INTERNAL_SCO_START_T *)msg;

            if (theKymera->busy_lock)
            {
                MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_SCO_START);
                *message = *m;
               /* Another audio chain is active, re-send message blocked on busy_lock */
                MessageSendConditionally(&theKymera->task, id, message, &theKymera->busy_lock);
                break;
            }

            if (m->pre_start_delay)
            {
                /* Resends are sent unconditonally, but the lock is set blocking
                   other new messages */
                appKymeraSetScoStartingLock(KymeraGetTaskData());
                appKymeraScoStartHelper(m->audio_sink, m->sco_info, m->wesco, m->volume_in_db,
                                        m->pre_start_delay - 1, FALSE,
                                        m->synchronised_start, m->started_handler);
            }
            else
            {
                /* Check if there is already a concurrent use-case active. If yes, take appropriate step */
                if(appKymeraHandleInternalScoStart(m->audio_sink, m->sco_info, m->wesco, m->volume_in_db, m->synchronised_start))
                {                    
                    /* Schedule auto-unmute after timeout if Kymera_ScheduleScoSyncUnmute is not called */
                    Kymera_ScheduleScoSyncUnmute(appConfigScoSyncUnmuteTimeoutMs());
                    if (m->started_handler)
                    {
                        m->started_handler();
                    }
                }
                appKymeraClearScoStartingLock(KymeraGetTaskData());
            }
        }
        break;

        case KYMERA_INTERNAL_SCO_SET_VOL:
        {
            KYMERA_INTERNAL_SCO_SET_VOL_T *m = (KYMERA_INTERNAL_SCO_SET_VOL_T *)msg;
            appKymeraHandleInternalScoSetVolume(m->volume_in_db);
        }
        break;

        case KYMERA_INTERNAL_SCO_MIC_MUTE:
        {
            KYMERA_INTERNAL_SCO_MIC_MUTE_T *m = (KYMERA_INTERNAL_SCO_MIC_MUTE_T *)msg;
            appKymeraHandleInternalScoMicMute(m->mute);
        }
        break;

        case KYMERA_INTERNAL_SCO_STOP:
        {
            appKymeraHandleInternalScoStop();
            MessageCancelFirst(KymeraGetTask(), KYMERA_INTERNAL_SCO_AUDIO_SYNCHRONISED);
        }
        break;

        case KYMERA_INTERNAL_TONE_PROMPT_PLAY:
            appKymeraHandleInternalTonePromptPlay(msg);
        break;

        case KYMERA_INTERNAL_TONE_PROMPT_STOP:
        case KYMERA_INTERNAL_PREPARE_FOR_PROMPT_TIMEOUT:
            appKymeraTonePromptStop();
        break;

    case KYMERA_INTERNAL_ANC_TUNING_START:
        KymeraAnc_TuningCreateChain((const KYMERA_INTERNAL_ANC_TUNING_START_T *)msg);
    break;

    case KYMERA_INTERNAL_ANC_TUNING_STOP:
        KymeraAnc_TuningDestroyChain((const KYMERA_INTERNAL_ANC_TUNING_STOP_T *)msg);
    break;

    case KYMERA_INTERNAL_ADAPTIVE_ANC_TUNING_START:
        kymeraAdaptiveAnc_CreateAdaptiveAncTuningChain((const KYMERA_INTERNAL_ADAPTIVE_ANC_TUNING_START_T *)msg);
    break;

    case KYMERA_INTERNAL_ADAPTIVE_ANC_TUNING_STOP:
        kymeraAdaptiveAnc_DestroyAdaptiveAncTuningChain((const KYMERA_INTERNAL_ADAPTIVE_ANC_TUNING_STOP_T *)msg);
    break;

        case KYMERA_INTERNAL_AANC_ENABLE:
        case KYMERA_INTERNAL_MIC_CONNECTION_TIMEOUT_ANC:
            KymeraAdaptiveAnc_Enable((const KYMERA_INTERNAL_AANC_ENABLE_T *)msg);
        break;

        case KYMERA_INTERNAL_AANC_DISABLE:
            KymeraAdaptiveAnc_Disable();
        break;

        case KYMERA_INTERNAL_PROSPECTIVE_POWER_OFF:
            appKymeraHandleProspectivePowerOff();
        break;

        case KYMERA_INTERNAL_AUDIO_SS_DISABLE:
            DEBUG_LOG("appKymera KYMERA_INTERNAL_AUDIO_SS_DISABLE");
            OperatorsFrameworkDisable();
            break;
#ifdef INCLUDE_MIRRORING
        case MESSAGE_SINK_AUDIO_SYNCHRONISED:
        case MESSAGE_SOURCE_AUDIO_SYNCHRONISED:
            appKymeraA2dpHandleAudioSyncStreamInd(id, msg);
        break;

        case KYMERA_INTERNAL_A2DP_DATA_SYNC_IND_TIMEOUT:
            appKymeraA2dpHandleDataSyncIndTimeout();
        break;

        case KYMERA_INTERNAL_A2DP_MESSAGE_MORE_DATA_TIMEOUT:
            appKymeraA2dpHandleMessageMoreDataTimeout();
        break;

        case KYMERA_INTERNAL_A2DP_AUDIO_SYNCHRONISED:
             appKymeraA2dpHandleAudioSynchronisedInd();
        break;

        case MESSAGE_MORE_DATA:
            appKymeraA2dpHandleMessageMoreData((const MessageMoreData *)msg);
        break;

        case MIRROR_PROFILE_A2DP_STREAM_ACTIVE_IND:
            Kymera_LatencyManagerHandleMirrorA2dpStreamActive();
        break;

        case MIRROR_PROFILE_A2DP_STREAM_INACTIVE_IND:
            Kymera_LatencyManagerHandleMirrorA2dpStreamInactive();
        break;

#endif /* INCLUDE_MIRRORING */

        case KYMERA_INTERNAL_SCO_AUDIO_SYNCHRONISED:
            appKymeraHandleInternalScoAudioSynchronised();
        break;

        case KYMERA_INTERNAL_LATENCY_MANAGER_MUTE:
            Kymera_LatencyManagerHandleMute();
        break;

        case KYMERA_INTERNAL_LATENCY_CHECK_TIMEOUT:
            Kymera_DynamicLatencyHandleLatencyTimeout();
		break;

        case KYMERA_INTERNAL_LATENCY_RECONFIGURE:
            Kymera_LatencyManagerHandleLatencyReconfigure((const KYMERA_INTERNAL_LATENCY_RECONFIGURE_T *)msg);
        break;

        case KYMERA_INTERNAL_LATENCY_MANAGER_MUTE_COMPLETE:
            Kymera_LatencyManagerHandleMuteComplete();
        break;

        case KYMERA_INTERNAL_AEC_LEAKTHROUGH_CREATE_STANDALONE_CHAIN:
            OperatorsFrameworkEnable();
            /* fall through */
        case KYMERA_INTERNAL_MIC_CONNECTION_TIMEOUT_LEAKTHROUGH:
            Kymera_CreateLeakthroughChain();
        break;
        case KYMERA_INTERNAL_AEC_LEAKTHROUGH_DESTROY_STANDALONE_CHAIN:
        {
            Kymera_DestroyLeakthroughChain();
            MessageSendLater(KymeraGetTask(), KYMERA_INTERNAL_PROSPECTIVE_POWER_OFF, NULL,
                             appConfigProspectiveAudioOffTimeout());
        }
        break;
        case KYMERA_INTERNAL_AEC_LEAKTHROUGH_SIDETONE_ENABLE:
        {
            Kymera_LeakthroughSetupSTGain();
        }
        break;
        case KYMERA_INTERNAL_AEC_LEAKTHROUGH_SIDETONE_GAIN_RAMPUP:
        {
            Kymera_LeakthroughStepupSTGain();
        }
        break;
        case KYMERA_INTERNAL_WIRED_ANALOG_AUDIO_START:
        {
            const KYMERA_INTERNAL_WIRED_ANALOG_AUDIO_START_T *m = (const KYMERA_INTERNAL_WIRED_ANALOG_AUDIO_START_T *)msg;

            if (theKymera->busy_lock)
            {
                MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_WIRED_ANALOG_AUDIO_START);
                *message = *m;
                /* Another audio chain is active, re-send message blocked on busy_lock */
                MessageSendConditionally(&theKymera->task, id, message, &theKymera->busy_lock);
                break;
            }
            /* Call the function in Kymera_Wired_Analog_Audio to start the audio chain */
            KymeraWiredAnalog_StartPlayingAudio(m);
        }
        break;
        case KYMERA_INTERNAL_WIRED_ANALOG_AUDIO_STOP:
        {
            /*Call the function in Kymera_Wired_Analog_Audio to stop the audio chain */
            KymeraWiredAnalog_StopPlayingAudio();
        }
        break;
        case KYMERA_INTERNAL_WIRED_AUDIO_SET_VOL:
        {
            KYMERA_INTERNAL_WIRED_AUDIO_SET_VOL_T *m = (KYMERA_INTERNAL_WIRED_AUDIO_SET_VOL_T *)msg;
            KymeraWiredAnalog_SetVolume(m->volume_in_db);
        }
        break;
        case KYMERA_INTERNAL_USB_AUDIO_START:
        {
            const KYMERA_INTERNAL_USB_AUDIO_START_T *m = (const KYMERA_INTERNAL_USB_AUDIO_START_T *)msg;

            if (theKymera->busy_lock)
            {
                MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_USB_AUDIO_START);
                *message = *m;
                /* Another audio chain is active, re-send message blocked on busy_lock */
                MessageSendConditionally(&theKymera->task, id, message, &theKymera->busy_lock);
                break;
            }

            KymeraUsbAudio_Start((KYMERA_INTERNAL_USB_AUDIO_START_T *)msg);
        }
        break;
        case KYMERA_INTERNAL_USB_AUDIO_STOP:
        {
            KymeraUsbAudio_Stop((KYMERA_INTERNAL_USB_AUDIO_STOP_T *)msg);
        }
        break;
        case KYMERA_INTERNAL_USB_AUDIO_SET_VOL:
        {
            KYMERA_INTERNAL_USB_AUDIO_SET_VOL_T *m = (KYMERA_INTERNAL_USB_AUDIO_SET_VOL_T *)msg;
            KymeraUsbAudio_SetVolume(m->volume_in_db);
        }
        break;
        case KYMERA_INTERNAL_USB_VOICE_START:
        {
            const KYMERA_INTERNAL_USB_VOICE_START_T *m = (const KYMERA_INTERNAL_USB_VOICE_START_T *)msg;

            if (theKymera->busy_lock)
            {
                MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_USB_VOICE_START);
                *message = *m;
                /* Another audio chain is active, re-send message blocked on busy_lock */
                MessageSendConditionally(&theKymera->task, id, message, &theKymera->busy_lock);
                break;
            }
            KymeraUsbVoice_Start((KYMERA_INTERNAL_USB_VOICE_START_T *)msg);
        }
        break;
        case KYMERA_INTERNAL_USB_VOICE_STOP:
        {
            KymeraUsbVoice_Stop((KYMERA_INTERNAL_USB_VOICE_STOP_T *)msg);
        }
        break;
        case KYMERA_INTERNAL_USB_VOICE_SET_VOL:
        {
            KYMERA_INTERNAL_USB_VOICE_SET_VOL_T *m = (KYMERA_INTERNAL_USB_VOICE_SET_VOL_T *)msg;
            KymeraUsbVoice_SetVolume(m->volume_in_db);
        }
        break;
        case KYMERA_INTERNAL_USB_VOICE_MIC_MUTE:
        {
            KYMERA_INTERNAL_USB_VOICE_MIC_MUTE_T *m = (KYMERA_INTERNAL_USB_VOICE_MIC_MUTE_T *)msg;
            KymeraUsbVoice_MicMute(m->mute);
        }
        break;
        case KYMERA_INTERNAL_LOW_LATENCY_STREAM_CHECK:
        {
            Kymera_LatencyManagerHandleLLStreamCheck();
        }
        break;
#if defined(INCLUDE_MUSIC_PROCESSING)
        case KYMERA_INTERNAL_USER_EQ_SELECT_EQ_BANK:
        {
            DEBUG_LOG("KYMERA_INTERNAL_USER_EQ_SELECT_EQ_BANK");
            KYMERA_INTERNAL_USER_EQ_SELECT_EQ_BANK_T *m = (KYMERA_INTERNAL_USER_EQ_SELECT_EQ_BANK_T *)msg;
            Kymera_SelectEqBankNow(m->preset);
        }
        break;
        case KYMERA_INTERNAL_USER_EQ_SET_USER_GAINS:
        {
            DEBUG_LOG("KYMERA_INTERNAL_USER_EQ_SET_USER_GAINS");
            KYMERA_INTERNAL_USER_EQ_SET_USER_GAINS_T *m = (KYMERA_INTERNAL_USER_EQ_SET_USER_GAINS_T *)msg;
            Kymera_SetUserEqBandsNow(m->start_band, m->end_band, m->gain);
            free(m->gain);

            KYMERA_NOTIFCATION_USER_EQ_BANDS_UPDATED_T *message = PanicUnlessMalloc(sizeof(KYMERA_NOTIFCATION_USER_EQ_BANDS_UPDATED_T));
            message->start_band = m->start_band;
            message->end_band = m->end_band;
            TaskList_MessageSendWithSize(theKymera->listeners, KYMERA_NOTIFCATION_USER_EQ_BANDS_UPDATED, message, sizeof(KYMERA_NOTIFCATION_USER_EQ_BANDS_UPDATED_T));
        }
        break;
        case KYMERA_INTERNAL_USER_EQ_APPLY_GAINS:
        {
            if(KymeraGetTaskData()->eq.selected_eq_bank == EQ_BANK_USER)
            {
                Kymera_ApplyGains(0, KymeraGetTaskData()->eq.user.number_of_bands - 1);
            }
        }
        break;
#endif /* INCLUDE_MUSIC_PROCESSING */

#if defined(INCLUDE_CVC_DEMO)
        case KYMERA_INTERNAL_CVC_3MIC_POLL_MODE_OF_OPERATION:
            Kymera_ScoPollCvcSend3MicModeOfOperation();
        break;
#endif

        default:
            break;
    }
}

bool appKymeraInit(Task init_task)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    memset(theKymera, 0, sizeof(*theKymera));
    theKymera->task.handler = kymera_msg_handler;
    theKymera->state = KYMERA_STATE_IDLE;
    theKymera->output_rate = 0;
    theKymera->lock = theKymera->busy_lock = 0;
    theKymera->a2dp_seid = AV_SEID_INVALID;
    theKymera->tone_count = 0;
    theKymera->split_tx_mode = 0;
    theKymera->q2q_mode = 0;
    theKymera->enable_left_right_mix = TRUE;
    theKymera->listeners = TaskList_Create();
    appKymeraExternalAmpSetup();
    if (bundle_config && bundle_config->number_of_capability_bundles > 0)
    {
        DEBUG_LOG("appKymeraInit number of bundles %d", bundle_config->number_of_capability_bundles);
        ChainSetDownloadableCapabilityBundleConfig(bundle_config);
    }
    else
    {
        DEBUG_LOG("appKymeraInit bundle config not valid");
    }

    Microphones_Init();

#ifdef INCLUDE_ANC_PASSTHROUGH_SUPPORT_CHAIN
    theKymera->anc_passthough_operator = INVALID_OPERATOR;
#endif

#ifdef ENABLE_AEC_LEAKTHROUGH
    Kymera_LeakthroughInit();
#endif
    theKymera->client_tasks = TaskList_Create();

    Kymera_ScoInit();
    KymeraUsbVoice_Init();

    KymeraAdaptiveAnc_Init();
    Kymera_VaInit();
#if defined(INCLUDE_MIRRORING) || defined(INCLUDE_STEREO)
    Kymera_A2dpInit();
#endif
    appKymeraTonePromptInit();
    KymeraWiredAnalog_Init();
    appKymeraLoopbackInit();

#if !defined(INCLUDE_A2DP_USB_SOURCE)
    KymeraUsbAudio_Init();
#endif



    Kymera_LatencyManagerInit(FALSE, 0);
    Kymera_DynamicLatencyInit();
    MirrorProfile_ClientRegister(&theKymera->task);
    AudioPluginCommonRegisterMicBiasVoltageCallback(Kymera_GetMicrophoneBiasVoltage);

    Kymera_InitMusicProcessing();

    KymeraFitTest_Init();

    UNUSED(init_task);
    return TRUE;
}

bool Kymera_IsIdle(void)
{
    return (KymeraGetTaskData()->state == KYMERA_STATE_IDLE);
}

void Kymera_ClientRegister(Task client_task)
{
    DEBUG_LOG("Kymera_ClientRegister");
    kymeraTaskData *kymera_sm = KymeraGetTaskData();
    TaskList_AddTask(kymera_sm->client_tasks, client_task);
}

void Kymera_ClientUnregister(Task client_task)
{
    DEBUG_LOG("Kymera_ClientRegister");
    kymeraTaskData *kymera_sm = KymeraGetTaskData();
    TaskList_RemoveTask(kymera_sm->client_tasks, client_task);
}


void Kymera_SetBundleConfig(const capability_bundle_config_t *config)
{
    bundle_config = config;
}

void Kymera_SetChainConfigs(const kymera_chain_configs_t *configs)
{
    chain_configs = configs;
}

const kymera_chain_configs_t *Kymera_GetChainConfigs(void)
{
    PanicNull((void *)chain_configs);
    return chain_configs;
}

void Kymera_SetScoChainTable(const appKymeraScoChainInfo *info)
{
    appKymeraScoChainTable = info;
}

void Kymera_RegisterNotificationListener(Task task)
{
    kymeraTaskData * theKymera = KymeraGetTaskData();
    TaskList_AddTask(theKymera->listeners, task);
}

void Kymera_StartWiredAnalogAudio(int16 volume_in_db, uint32 rate, uint32 min_latency, uint32 max_latency, uint32 target_latency)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_WIRED_ANALOG_AUDIO_START);
    DEBUG_LOG("Kymera_StartWiredAnalogAudio");

    message->rate      = rate;
    message->volume_in_db     = volume_in_db;
    message->min_latency = min_latency;
    message->max_latency = max_latency;
    message->target_latency = target_latency;
    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_WIRED_ANALOG_AUDIO_START, message, &theKymera->lock);
}

void Kymera_StopWiredAnalogAudio(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOG("Kymera_StopWiredAnalogAudio");
    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_WIRED_ANALOG_AUDIO_STOP, NULL, &theKymera->lock);
}

void appKymeraWiredAudioSetVolume(int16 volume_in_db)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOG("appKymeraWiredAudioSetVolume, volume %u", volume_in_db);

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_WIRED_AUDIO_SET_VOL);
    message->volume_in_db = volume_in_db;
    MessageCancelFirst(&theKymera->task, KYMERA_INTERNAL_WIRED_AUDIO_SET_VOL);
    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_WIRED_AUDIO_SET_VOL, message, &theKymera->lock);
}



void appKymeraUsbAudioStart(uint8 channels, uint8 frame_size, Source src,
                            int16 volume_in_db, uint32 rate, uint32 min_latency,
                            uint32 max_latency, uint32 target_latency)
{
    DEBUG_LOG("appKymeraUsbAudioStart");
    kymeraTaskData *theKymera = KymeraGetTaskData();

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_USB_AUDIO_START);
    message->channels = channels;
    message->frame_size = frame_size;
    message->sample_freq = rate;
    message->spkr_src = src;
    message->volume_in_db = volume_in_db;
    message->min_latency_ms = min_latency;
    message->max_latency_ms = max_latency;
    message->target_latency_ms = target_latency;

    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_USB_AUDIO_START, message, &theKymera->lock);
}

void appKymeraUsbAudioStop(Source usb_src,
                           void (*kymera_stopped_handler)(Source source))
{
    DEBUG_LOG("appKymeraUsbAudioStop");
    kymeraTaskData *theKymera = KymeraGetTaskData();

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_USB_AUDIO_STOP);
    message->source = usb_src;
    message->kymera_stopped_handler = kymera_stopped_handler;

    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_USB_AUDIO_STOP, message, &theKymera->lock);
}

void appKymeraUsbAudioSetVolume(int16 volume_in_db)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOG("appKymeraUsbAudioSetVolume, volume %u", volume_in_db);

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_USB_AUDIO_SET_VOL);
    message->volume_in_db = volume_in_db;
    MessageCancelFirst(&theKymera->task, KYMERA_INTERNAL_USB_AUDIO_SET_VOL);
    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_USB_AUDIO_SET_VOL, message, &theKymera->lock);
}

void appKymeraUsbVoiceStart(usb_voice_mode_t mode, uint8 spkr_channels, uint32 spkr_sample_rate,
                            uint32 mic_sample_rate, Source spkr_src, Sink mic_sink, int16 volume_in_db,
                            uint32 min_latency, uint32 max_latency, uint32 target_latency,
                            void (*kymera_stopped_handler)(Source source))
{
    DEBUG_LOG("appKymeraUsbVoiceStart");
    kymeraTaskData *theKymera = KymeraGetTaskData();

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_USB_VOICE_START);
    message->mode = mode;
    message->spkr_channels = spkr_channels;
    message->spkr_sample_rate = spkr_sample_rate;
    message->mic_sample_rate = mic_sample_rate;
    message->spkr_src = spkr_src;
    message->mic_sink = mic_sink;
    message->volume = volume_in_db;
    message->min_latency_ms = min_latency;
    message->max_latency_ms = max_latency;
    message->target_latency_ms = target_latency;
    message->kymera_stopped_handler = kymera_stopped_handler;

    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_USB_VOICE_START, message, &theKymera->lock);
}

void appKymeraUsbVoiceStop(Source spkr_src, Sink mic_sink,
                           void (*kymera_stopped_handler)(Source source))
{
    DEBUG_LOG("appKymeraUsbVoiceStop");
    kymeraTaskData *theKymera = KymeraGetTaskData();

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_USB_VOICE_STOP);
    message->spkr_src = spkr_src;
    message->mic_sink = mic_sink;
    message->kymera_stopped_handler = kymera_stopped_handler;

    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_USB_VOICE_STOP, message, &theKymera->lock);
}

void appKymeraUsbVoiceSetVolume(int16 volume_in_db)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_USB_VOICE_SET_VOL);
    message->volume_in_db = volume_in_db;
    MessageCancelFirst(&theKymera->task, KYMERA_INTERNAL_USB_VOICE_SET_VOL);
    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_USB_VOICE_SET_VOL, message, &theKymera->lock);
}

void appKymeraUsbVoiceMicMute(bool mute)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_USB_VOICE_MIC_MUTE);
    message->mute = mute;
    MessageSend(&theKymera->task, KYMERA_INTERNAL_USB_VOICE_MIC_MUTE, message);
}

Transform Kymera_GetA2dpMediaStreamTransform(void)
{
    Transform trans = (Transform)0;

#ifdef INCLUDE_MIRRORING
    kymeraTaskData *theKymera = KymeraGetTaskData();

    if (theKymera->state == KYMERA_STATE_A2DP_STREAMING ||
        theKymera->state == KYMERA_STATE_A2DP_STREAMING_WITH_FORWARDING)
    {
        trans = theKymera->hashu.hash;
    }
#endif

    return trans;
}

void Kymera_EnableAdaptiveAnc(bool in_ear, audio_anc_path_id path, adaptive_anc_hw_channel_t hw_channel, anc_mode_t mode)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_AANC_ENABLE);

    message->in_ear         = in_ear;
    message->control_path   = path;
    message->hw_channel     = hw_channel;
    message->current_mode   = mode;
    MessageSend(&theKymera->task, KYMERA_INTERNAL_AANC_ENABLE, message);
}

void Kymera_DisableAdaptiveAnc(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    MessageSend(&theKymera->task, KYMERA_INTERNAL_AANC_DISABLE, NULL);
}

bool Kymera_IsAdaptiveAncEnabled(void)
{
    return KymeraAdaptiveAnc_IsEnabled();
}

bool Kymera_ObtainCurrentApdativeAncMode(adaptive_anc_mode_t *aanc_mode)
{
    PanicNull(aanc_mode);
    return KymeraAdaptiveAnc_ObtainCurrentAancMode(aanc_mode);
}

bool Kymera_AdaptiveAncIsNoiseLevelBelowQuietModeThreshold(void)
{
    return KymeraAdaptiveAnc_IsNoiseLevelBelowQmThreshold();
}

void Kymera_SetA2dpOutputParams(a2dp_codec_settings * codec_settings)
{
    kymeraTaskData *the_kymera = KymeraGetTaskData();
    the_kymera->a2dp_output_params = codec_settings;
}

void Kymera_ClearA2dpOutputParams(void)
{
    kymeraTaskData *the_kymera = KymeraGetTaskData();
    the_kymera->a2dp_output_params = NULL;
}

bool Kymera_IsA2dpOutputPresent(void)
{
    kymeraTaskData *the_kymera = KymeraGetTaskData();
    return the_kymera->a2dp_output_params ? TRUE : FALSE;
}


bool Kymera_IsA2dpSynchronisationNotInProgress(void)
{
#ifdef INCLUDE_MIRRORING
    kymeraTaskData *theKymera = KymeraGetTaskData();

    if ((theKymera->state == KYMERA_STATE_A2DP_STREAMING ||
         theKymera->state == KYMERA_STATE_A2DP_STREAMING_WITH_FORWARDING) &&
        (theKymera->sync_info.state != KYMERA_AUDIO_SYNC_STATE_COMPLETE))
    {
        DEBUG_LOG("Kymera_IsA2dpSynchronisationNotInProgress: audio sync incomplete");
        return FALSE;
    }
#endif /* INCLUDE_MIRRORING */
    return TRUE;
}

void Kymera_SetCallbackConfigs(const kymera_callback_configs_t *configs)
{
    DEBUG_LOG("Kymera_SetCallbackConfigs");
    callback_configs = configs;
}

const kymera_callback_configs_t *Kymera_GetCallbackConfigs(void)
{
    return callback_configs;
}

bool Kymera_IsQ2qModeEnabled(void)
{
    return KymeraGetTaskData()->q2q_mode;
}

bool appKymeraIsTonePlaying(void)
{
    return (KymeraGetTaskData()->tone_count > 0);
}

void Kymera_RegisterConfigCallbacks(kymera_chain_config_callbacks_t *callbacks)
{
    KymeraGetTaskData()->chain_config_callbacks = callbacks;
}

void Kymera_ScheduleScoSyncUnmute(Delay delay)
{
    DEBUG_LOG("Kymera_ScheduleScoSyncUnmute, unmute in %dms", delay);
    MessageCancelFirst(KymeraGetTask(), KYMERA_INTERNAL_SCO_AUDIO_SYNCHRONISED);
    MessageSendLater(KymeraGetTask(), KYMERA_INTERNAL_SCO_AUDIO_SYNCHRONISED,
                     NULL, delay);
}

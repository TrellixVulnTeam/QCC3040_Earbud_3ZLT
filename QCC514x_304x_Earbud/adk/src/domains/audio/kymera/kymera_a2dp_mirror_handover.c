/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera_a2dp_mirror_handover.c
\brief      Handover of TWM kymera A2DP.
*/

#ifdef INCLUDE_MIRRORING
#include "kymera_a2dp_private.h"
#include "kymera_common.h"
#include "kymera_data.h"
#include "kymera_latency_manager.h"
#include "kymera_dynamic_latency.h"
#include "mirror_profile.h"
#include "app_handover_if.h"

/******************************************************************************
 * Local Function Prototypes
 ******************************************************************************/
static bool appKymeraA2dpMirrorVeto(void);
static bool appKymeraA2dpMirrorMarshal(const tp_bdaddr *tp_bd_addr,
                       uint8 *buf,
                       uint16 length,
                       uint16 *written);

static bool appKymeraA2dpMirrorUnmarshal(const tp_bdaddr *tp_bd_addr,
                         const uint8 *buf,
                         uint16 length,
                         uint16 *consumed);

static void appKymeraA2dpMirrorHandoverCommit(const tp_bdaddr *tp_bd_addr,
                                     const bool newRole );

static void appKymeraA2dpMirrorHandoverComplete(const bool newRole);


static void appKymeraA2dpMirrorHandoverAbort(void);

const handover_interface kymera_a2dp_mirror_handover_if =  {
        &appKymeraA2dpMirrorVeto,
        &appKymeraA2dpMirrorMarshal,
        &appKymeraA2dpMirrorUnmarshal,
        &appKymeraA2dpMirrorHandoverCommit,
        &appKymeraA2dpMirrorHandoverComplete,
        &appKymeraA2dpMirrorHandoverAbort};

/******************************************************************************
 * Local Function Definitions
 ******************************************************************************/

/*!
    \brief Handle Veto check during handover
    \return bool
*/
static bool appKymeraA2dpMirrorVeto(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    if (theKymera->state == KYMERA_STATE_A2DP_STREAMING ||
        theKymera->state == KYMERA_STATE_A2DP_STREAMING_WITH_FORWARDING)
    {
        /* veto the handover if audio synchronisation hasn't completed */
        if (theKymera->sync_info.state != KYMERA_AUDIO_SYNC_STATE_COMPLETE)
        {
            DEBUG_LOG_INFO("appKymeraA2dpMirrorVeto audio sync incomplete");
            return TRUE;
        }
#ifdef INCLUDE_LATENCY_MANAGER
        if (Kymera_LatencyManagerIsReconfigInProgress())
        {
            DEBUG_LOG_INFO("appKymeraA2dpMirrorVeto Latency reconfiguration is in progress");
            return TRUE;
        }
#endif        
    }

    return FALSE;
}

/*! \brief Handle request for marshaling data (if any) associated with
 *         TWM kymera A2DP
 *  \return TRUE if data marshalling is complete, FALSE otherwise.
 */
static bool appKymeraA2dpMirrorMarshal(const tp_bdaddr *tp_bd_addr,
                                        uint8 *buf,
                                        uint16 length,
                                        uint16 *written)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    bool result = TRUE;

#ifndef INCLUDE_LATENCY_MANAGER
    UNUSED(buf);
    UNUSED(length);
    UNUSED(written);
    UNUSED(tp_bd_addr);
#else
    /* Only mirror this state once for the mirrored handset */
    if ((tp_bd_addr->transport == TRANSPORT_BREDR_ACL) &&
        BdaddrIsSame(MirrorProfile_GetMirroredDeviceAddress(), &tp_bd_addr->taddr.addr))
    {
        uint16 copied1, copied2;
        copied1 = Kymera_DynamicLatencyMarshal(buf, length);
        if (copied1)
        {
            buf += copied1;
            length -= copied1;
            copied2 = Kymera_LatencyManagerMarshal(buf, length);
            if (copied2)
            {
                *written = copied1 + copied2;
            }
        }
    }
#endif
    /* Q2Q mode has no transfer of ttp info so ignoring. */
    if (theKymera->q2q_mode)
    {
        return result;
    }

    DEBUG_LOG("appKymeraA2dpMirrorMarshal, state enum:appKymeraState:%d",theKymera->state);
    if (theKymera->state == KYMERA_STATE_A2DP_STREAMING_WITH_FORWARDING)
    {
        /* Disabling audio sync adjustments */
        switch(theKymera->sync_info.mode)
        {
        case KYMERA_AUDIO_SYNC_START_PRIMARY_SYNCHRONISED:
        case KYMERA_AUDIO_SYNC_START_PRIMARY_SYNC_UNMUTE:
        {
            PanicZero(theKymera->sync_info.source);
            DEBUG_LOG("appKymeraA2dpMirrorMarshal, reset audio_sync source interval");
            /* reset audio sync source interval to stop transfer
             * of ttp_info to secondary during handover.
             */
             (void) SourceConfigure(theKymera->sync_info.source,
                                    STREAM_AUDIO_SYNC_SOURCE_INTERVAL, 0);
        }
        break;
        case KYMERA_AUDIO_SYNC_START_SECONDARY_SYNCHRONISED:
        case KYMERA_AUDIO_SYNC_START_SECONDARY_SYNC_UNMUTE:
        {
            PanicZero(theKymera->sync_info.source);
            DEBUG_LOG("appKymeraA2dpMirrorMarshal, set audio_sync sink to ignore data");
            /* set audio sync sink data mode to ignore inflight ttp_info data
             * received from primary during handover
             */
            (void) SinkConfigure(StreamSinkFromSource(theKymera->sync_info.source),
                                 STREAM_AUDIO_SYNC_SINK_MODE,
                                 SINK_MODE_DEFAULT);
        }
        break;
        default:
            break;
        }
    }
    return result;
}
/*! \brief Handle request for unmarshalling data (if any) associated with
 *         TWM kymera A2DP
 *  \return TRUE if data unmarshalling is complete, FALSE otherwise.
 */
static bool appKymeraA2dpMirrorUnmarshal(const tp_bdaddr *tp_bd_addr,
                                         const uint8 *buf,
                                         uint16 length,
                                         uint16 *consumed)
{
    DEBUG_LOG("appKymeraA2dpMirrorUnmarshal");
    bool result = TRUE;
    UNUSED(tp_bd_addr);

#ifndef INCLUDE_LATENCY_MANAGER
    UNUSED(buf);
    UNUSED(length);
    UNUSED(consumed);
#else
    uint16 copied1, copied2;
    copied1 = Kymera_DynamicLatencyUnmarshal(buf, length);
    if (copied1)
    {
        buf += copied1;
        length -= copied1;
        copied2 = Kymera_LatencyManagerUnmarshal(buf, length);
        if (copied2)
        {
            *consumed = copied1 + copied2;
        }
    }
#endif
    return result;
}

/*!
    \brief Take any time critical actions necessary to commit to the
     specified role

    \param[in] tp_bd_addr Bluetooth address of the connected device.
    \param[in] is_primary TRUE if device's new role is primary, else secondary

*/
static void appKymeraA2dpMirrorHandoverCommit(const tp_bdaddr *tp_bd_addr, bool is_primary)
{
    UNUSED(tp_bd_addr);
    UNUSED(is_primary);
}

/*!
    \brief Take any pending actions necessary to commit to the specified role

    \param[in] is_primary TRUE if device's new role is primary, else secondary

*/
static void appKymeraA2dpMirrorHandoverComplete(const bool is_primary)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOG("appKymeraA2dpMirrorHandoverComplete primary:%d", is_primary);
    if ((theKymera->state == KYMERA_STATE_A2DP_STREAMING_WITH_FORWARDING) && (!theKymera->q2q_mode))
    {
        PanicZero(theKymera->sync_info.source);
        Operator op_rtp;
        GET_OP_FROM_CHAIN(op_rtp, theKymera->chain_input_handle, OPR_RTP_DECODER);

        Kymera_LatencyManagerHandoverCommit(is_primary);
        Kymera_DynamicLatencyHandoverCommit(is_primary);

        if (is_primary)
        {
            theKymera->sync_info.mode = KYMERA_AUDIO_SYNC_START_PRIMARY_SYNCHRONISED;

            /* switch audio RTP operator in normal (or TTP_FULL) mode */
            DEBUG_LOG("appKymeraA2dpMirrorHandoverCommit, configure RTP operator in ttp_full_only mode");
            OperatorsStandardSetTtpState(op_rtp, ttp_full_only, 0, 0, 
                                         Kymera_LatencyManagerGetLatencyForSeidInUs(theKymera->a2dp_seid));

            /* set audio sync source interval to start transfer
                * of ttp_info to secondary (old primary). */
            (void) SourceConfigure(theKymera->sync_info.source,
                                    STREAM_AUDIO_SYNC_SOURCE_INTERVAL,
                                    AUDIO_SYNC_MS_INTERVAL  * US_PER_MS);

            /* set audio sync sink data mode to ignore inflight ttp_info data
                * received from secondary (old primary) during handover */
            (void) SinkConfigure(StreamSinkFromSource(theKymera->sync_info.source),
                                    STREAM_AUDIO_SYNC_SINK_MODE,
                                    SINK_MODE_DEFAULT);
        }
        else
        {
            theKymera->sync_info.mode = KYMERA_AUDIO_SYNC_START_SECONDARY_SYNCHRONISED;
  
            /* switch audio RTP operator in free_run mode */
            DEBUG_LOG("appKymeraA2dpMirrorHandoverCommit, configure RTP operator in ttp_free_run_only mode");
            OperatorsStandardSetTtpState(op_rtp, ttp_free_run_only, 0, 0, 
                                         Kymera_LatencyManagerGetLatencyForSeidInUs(theKymera->a2dp_seid));

            /* set audio sync source interval to zero to stop transfer of
                * ttp_info to primary (old secondary). */
            (void) SourceConfigure(theKymera->sync_info.source,
                                    STREAM_AUDIO_SYNC_SOURCE_INTERVAL,
                                    0);
            /* set audio sync sink data mode to process ttp_info data
                * received from secondary (old primary) */
            (void) SinkConfigure(StreamSinkFromSource(theKymera->sync_info.source),
                                    STREAM_AUDIO_SYNC_SINK_MODE,
                                    SINK_MODE_STARTUP);
        }
    }
}

static void appKymeraA2dpMirrorHandoverAbort(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    DEBUG_LOG("appKymeraA2dpMirrorHandoverAbort, state:%d",theKymera->state);
    if ((theKymera->state == KYMERA_STATE_A2DP_STREAMING_WITH_FORWARDING) && (!theKymera->q2q_mode))
    {
        PanicZero(theKymera->sync_info.source);
        switch(theKymera->sync_info.mode)
        {
        case KYMERA_AUDIO_SYNC_START_PRIMARY_SYNCHRONISED:
        case KYMERA_AUDIO_SYNC_START_PRIMARY_SYNC_UNMUTE:
        {
            DEBUG_LOG("appKymeraA2dpMirrorHandoverAbort, set audio_sync source interval");
            /* set audio sync source interval to resume transfer
             * of ttp_info to secondary.
             */
             (void) SourceConfigure(theKymera->sync_info.source,
                                    STREAM_AUDIO_SYNC_SOURCE_INTERVAL,
                                    AUDIO_SYNC_MS_INTERVAL  * US_PER_MS);
        }
        break;
        case KYMERA_AUDIO_SYNC_START_SECONDARY_SYNCHRONISED:
        case KYMERA_AUDIO_SYNC_START_SECONDARY_SYNC_UNMUTE:
        {
            DEBUG_LOG("appKymeraA2dpMirrorHandoverAbort, set audio_sync sink mode to process data");
            /* set audio sync sink data mode to ignore inflight ttp_info data
             * received from primary during handover
             */
            (void) SinkConfigure(StreamSinkFromSource(theKymera->sync_info.source),
                                 STREAM_AUDIO_SYNC_SINK_MODE,
                                 SINK_MODE_STARTUP);
        }
        break;
        default:
            break;
        }
    }
}

#endif /* INCLUDE_MIRRORING */

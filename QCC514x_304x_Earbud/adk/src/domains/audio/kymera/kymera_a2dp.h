#ifndef KYMERA_A2DP_H
#define KYMERA_A2DP_H

#include <source.h>
#include <message.h>
#include <a2dp.h>

/* AptX Adaptive encoder version defines */
#define APTX_AD_ENCODER_R2_1 21
#define APTX_AD_ENCODER_R1_1 11

#define APTX_MONO_CODEC_RATE_KBPS     (192)
#define APTX_STEREO_CODEC_RATE_KBPS   (384)
#define APTXHD_STEREO_CODEC_RATE_KBPS (576)
#define APTX_AD_CODEC_RATE_KBPS       (500)

/* Maximum bitrates for aptX adaptive */
/* Bitrates for 48K modes HS and TWM are the same */
#define APTX_AD_CODEC_RATE_NQHS_48K_KBPS     (427)
#define APTX_AD_CODEC_RATE_QHS_48K_KBPS      (430)

/* Maxium bitrates for 96K modes */
#define APTX_AD_CODEC_RATE_HS_QHS_96K_KBPS   (820) /* QHS Headset mode */
#define APTX_AD_CODEC_RATE_HS_NQHS_96K_KBPS  (646) /* Non-QHS Headset mode */

#define APTX_AD_CODEC_RATE_TWM_QHS_96K_KBPS  (650)  /* QHS TWM mode */
#define APTX_AD_CODEC_RATE_TWM_NQHS_96K_KBPS (510)  /* Non-QHS TWM mode */

#define APTX_AD_CODEC_RATE_TWM_QHS_SPLIT_TX_96K_KBPS  (325)  /* QHS TWM mode for split tx is half stereo mode */
#define APTX_AD_CODEC_RATE_TWM_NQHS_SPLIT_TX_96K_KBPS (265)  /* Non-QHS TWM mode for split tx is half stereo mode */

/*! Maximum codec rate expected by this application */
#define MAX_CODEC_RATE_KBPS (APTXHD_STEREO_CODEC_RATE_KBPS)

/*!@{ \name Buffer sizes required to hold enough audio to achieve the TTP latency */
#define PRE_DECODER_BUFFER_SIZE     (MS_TO_BUFFER_SIZE_CODEC(PRE_DECODER_BUFFER_MS, MAX_CODEC_RATE_KBPS))

/*! \brief The KYMERA_INTERNAL_A2DP_SET_VOL message content. */
typedef struct
{
    /*! The volume to set. */
    int16 volume_in_db;
} KYMERA_INTERNAL_A2DP_SET_VOL_T;

/*! \brief The KYMERA_INTERNAL_A2DP_START and KYMERA_INTERNAL_A2DP_STARTING message content. */
typedef struct
{
    /*! The client's lock. Bits set in lock_mask will be cleared when A2DP is started. */
    uint16 *lock;
    /*! The bits to clear in the client lock. */
    uint16 lock_mask;
    /*! The A2DP codec settings */
    a2dp_codec_settings codec_settings;
    /*! The starting volume */
    int16 volume_in_db;
    /*! The number of times remaining the kymera module will resend this message to
        itself (having entered the locked KYMERA_STATE_A2DP_STARTING) state before
        proceeding to commence starting kymera. Starting will commence when received
        with value 0. Only applies to starting the master. */
    uint8 master_pre_start_delay;
    /*! The max bitrate for the input stream (in bps). Ignored if zero. */
    uint32 max_bitrate;
    uint8 q2q_mode; /* 1 = Q2Q mode enabled, 0 = Generic Mode */
    aptx_adaptive_ttp_latencies_t nq2q_ttp;
} KYMERA_INTERNAL_A2DP_START_T;

/*! \brief The KYMERA_INTERNAL_A2DP_STOP and KYMERA_INTERNAL_A2DP_STOP_FORWARDING message content. */
typedef struct
{
    /*! The A2DP seid */
    uint8 seid;
    /*! The media sink */
    Source source;
} KYMERA_INTERNAL_A2DP_STOP_T;

/*! \brief Initialise a2dp module.
*/
void Kymera_A2dpInit(void);

/*! \brief Handle request to start A2DP.
    \param msg The request message.
    \return TRUE if A2DP start is complete. FALSE if A2DP start is incomplete.
*/
bool Kymera_A2dpHandleInternalStart(const KYMERA_INTERNAL_A2DP_START_T *msg);

/*! \brief Handle request to stop A2DP.
    \param msg The request message.
*/
void Kymera_A2dpHandleInternalStop(const KYMERA_INTERNAL_A2DP_STOP_T *msg);

/*! \brief Handle request to set A2DP volume.
    \param volume_in_db The requested volume.
*/
void Kymera_A2dpHandleInternalSetVolume(int16 volume_in_db);

/*! \brief Start local A2DP.

    \param codec_settings The A2DP codec settings to use.
    \param max_bitrate The max bitrate for the input stream (in bps). Ignored if zero.
    \param volume_in_db The initial volume to use.
    \param nq2q_ttp The aptX adaptive NQ2Q TTP Latency settings.

    \return TRUE if start is completed, else FALSE.

 */
bool Kymera_A2dpStart(const a2dp_codec_settings *codec_settings, uint32 max_bitrate, int16 volume_in_db,
                      aptx_adaptive_ttp_latencies_t nq2q_ttp);

/*! \brief Start A2DP forwarding from the Primary to the Secondary.

    \param codec_settings The A2DP codec settings to use.

    In TWS legacy, this function starts forwarding media.
    In TWM, this function starts 'forwarding' audio synchronisation.
 */
void Kymera_A2dpStartForwarding(const a2dp_codec_settings *codec_settings);

/*! \brief Stop A2DP forwarding from the Primary to the Secondary.

    In TWS legacy, this function stops forwarding media.
    In TWM, this function stops 'forwarding' audio synchronisation.
 */
void Kymera_A2dpStopForwarding(Source source);

/*! \brief Stop A2DP operation.

    Common function to all device types.
 */
void Kymera_A2dpCommonStop(Source source);

/*! \brief Configure RTP decoder startup period.
    \param op The operator id of the RTP decoder.
    \param startup_period At the start of stream the RTP decoder can be configured
           to wait for a period of time and evaluate the amount of data received
           and make a latency correction which corrects any error in TTP latency.
           The startup time should be less than the configured TTP latency.
*/
void Kymera_A2dpConfigureRtpDecoderStartupPeriod(Operator op, uint16 startup_period);

#ifdef INCLUDE_MIRRORING
void appKymeraA2dpHandleDataSyncIndTimeout(void);
void appKymeraA2dpHandleMessageMoreDataTimeout(void);
void appKymeraA2dpHandleAudioSyncStreamInd(MessageId id, Message msg);
void appKymeraA2dpHandleAudioSynchronisedInd(void);
void appKymeraA2dpHandleMessageMoreData(const MessageMoreData *mmd);
#endif /* INCLUDE_MIRRORING */

#endif // KYMERA_A2DP_H

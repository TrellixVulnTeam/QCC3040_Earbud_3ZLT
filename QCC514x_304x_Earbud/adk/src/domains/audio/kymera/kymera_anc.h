/*!
\copyright  Copyright (c) 2017 - 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Private header for ANC functionality
*/

#ifndef KYMERA_ANC_H_
#define KYMERA_ANC_H_

#include <kymera.h>
#include "kymera_data.h"

typedef struct
{
    uint32 usb_rate;
    Source spkr_src;
    Sink mic_sink;
    uint8 spkr_channels;
    uint8 mic_channels;
    uint8 frame_size;
} KYMERA_INTERNAL_ANC_TUNING_START_T;

/*! \brief The KYMERA_INTERNAL_ANC_TUNING_STOP message content. */
typedef struct
{
    Source spkr_src;
    Sink mic_sink;
    void (*kymera_stopped_handler)(Source source);
} KYMERA_INTERNAL_ANC_TUNING_STOP_T;

#ifdef INCLUDE_STEREO
#define getAncFeedForwardRightMic()   (appConfigAncFeedForwardRightMic())
#define getAncFeedBackRightMic()      (appConfigAncFeedBackRightMic())
#define getAncFeedForwardLeftMic()    (appConfigAncFeedForwardLeftMic())
#define getAncFeedBackLeftMic()       (appConfigAncFeedBackLeftMic())

#define getAncTuningMonitorLeftMic()  (appConfigAncTuningMonitorLeftMic())
#define getAncTuningMonitorRightMic() (appConfigAncTuningMonitorRightMic())
#else
#define getAncFeedForwardRightMic()   (microphone_none)
#define getAncFeedBackRightMic()      (microphone_none)
#define getAncFeedForwardLeftMic()    (appConfigAncFeedForwardMic())
#define getAncFeedBackLeftMic()       (appConfigAncFeedBackMic())

#define getAncTuningMonitorLeftMic()  (appConfigAncTuningMonitorMic())
#define getAncTuningMonitorRightMic() (microphone_none)
#endif

/*!
 * \brief Makes the support chain ready for ANC hardware. applicable only for QCC512x devices
 * \param appKymeraState state current kymera state.
 *
 */
#if defined INCLUDE_ANC_PASSTHROUGH_SUPPORT_CHAIN && defined ENABLE_ANC
void KymeraAnc_PreStateTransition(appKymeraState state);
#else
#define KymeraAnc_PreStateTransition(x) ((void)(x))
#endif

/*! \brief Creates the Kymera Tuning Chain
    \param msg internal message which has the anc tuning connect parameters
*/
void KymeraAnc_TuningCreateChain(const KYMERA_INTERNAL_ANC_TUNING_START_T *msg);


/*! \brief Destroys the Kymera Tuning Chain
    \param msg internal message which has the anc tuning disconnect parameters
*/
void KymeraAnc_TuningDestroyChain(const KYMERA_INTERNAL_ANC_TUNING_STOP_T *msg);

#endif /* KYMERA_ANC_H_ */

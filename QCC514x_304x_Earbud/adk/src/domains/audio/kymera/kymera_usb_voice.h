/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief     The Kymera USB Voice API

*/

#ifndef KYMERA_USB_VOICE_H
#define KYMERA_USB_VOICE_H

#include <usb_audio.h>
#include <source.h>
#include <sink.h>

/*! \brief The connectivity message for USB Voice. */
typedef struct
{
    usb_voice_mode_t mode;
    uint8 spkr_channels;
    Source spkr_src;
    Sink mic_sink;
    uint32 spkr_sample_rate;
    uint32 mic_sample_rate;
    int16 volume;
    uint32 min_latency_ms;
    uint32 max_latency_ms;
    uint32 target_latency_ms;
    void (*kymera_stopped_handler)(Source source);
} KYMERA_INTERNAL_USB_VOICE_START_T;

/*! \brief Disconnect message for USB Voice. */
typedef struct
{
     Source spkr_src;
     Sink mic_sink;
     void (*kymera_stopped_handler)(Source source);
} KYMERA_INTERNAL_USB_VOICE_STOP_T;

/*! \brief The KYMERA_INTERNAL_USB_VOICE_SET_VOL message content. */
typedef struct
{
    /*! The volume to set. */
    int16 volume_in_db;
} KYMERA_INTERNAL_USB_VOICE_SET_VOL_T;


/*! \brief The KYMERA_INTERNAL_USB_VOICE_MIC_MUTE message content. */
typedef struct
{
    /*! TRUE to enable mute, FALSE to disable mute. */
    bool mute;
} KYMERA_INTERNAL_USB_VOICE_MIC_MUTE_T;

/*! \brief Create and start USB Voice chain.
    \param voice_params Parameters for USB voice chain connect.
*/
#ifdef INCLUDE_USB_DEVICE
void KymeraUsbVoice_Start(KYMERA_INTERNAL_USB_VOICE_START_T *voice_params);
#else
#define KymeraUsbVoice_Start(x) UNUSED(x)
#endif
/*! \brief Stop and destroy USB Voice chain.
    \param voice_params Parameters for USB voice chain disconnect.
*/
#ifdef INCLUDE_USB_DEVICE
void KymeraUsbVoice_Stop(KYMERA_INTERNAL_USB_VOICE_STOP_T *voice_params);
#else
#define KymeraUsbVoice_Stop(x) UNUSED(x)
#endif

/*! \brief Set USB Voice volume.

    \param[in] volume_in_db.
 */
#ifdef INCLUDE_USB_DEVICE
void KymeraUsbVoice_SetVolume(int16 volume_in_db);
#else
#define KymeraUsbVoice_SetVolume(x) UNUSED(x)
#endif

/*! \brief Enable or disable MIC muting.

    \param[in] mute TRUE to mute MIC, FALSE to unmute MIC.
*/
#ifdef INCLUDE_USB_DEVICE
void KymeraUsbVoice_MicMute(bool mute);
#else
#define KymeraUsbVoice_MicMute(x) UNUSED(x)
#endif

/*! \brief Init USB voice component
*/
#ifdef INCLUDE_USB_DEVICE
void KymeraUsbVoice_Init(void);
#else
#define KymeraUsbVoice_Init(x) ((void)(0))
#endif

#endif /* KYMERA_USB_VOICE_H */

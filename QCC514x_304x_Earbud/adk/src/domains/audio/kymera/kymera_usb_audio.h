#ifndef KYMERA_USB_AUDIO_H
#define KYMERA_USB_AUDIO_H

/*! \brief The connectivity message for USB Audio. */
typedef struct
{
    uint8 channels;
    uint8 frame_size;
    Source spkr_src;
    int16 volume_in_db;
    uint32 sample_freq;
    uint32 min_latency_ms;
    uint32 max_latency_ms;
    uint32 target_latency_ms;
} KYMERA_INTERNAL_USB_AUDIO_START_T;

/*! \brief Disconnect message for USB Audio. */
typedef struct
{
    Source source;
    void (*kymera_stopped_handler)(Source source);
} KYMERA_INTERNAL_USB_AUDIO_STOP_T;

/*! \brief The KYMERA_INTERNAL_USB_AUDIO_SET_VOL message content. */
typedef struct
{
    /*! The volume to set. */
    int16 volume_in_db;
} KYMERA_INTERNAL_USB_AUDIO_SET_VOL_T;

/*! \brief Initialise USB audio module.
*/
#ifdef INCLUDE_USB_DEVICE
void KymeraUsbAudio_Init(void);
#else
#define KymeraUsbAudio_Init() ((void)(0))
#endif

/*! \brief Start USB Audio.
    \param audio_params connect parameters defined by USB audio source.
*/
#ifdef INCLUDE_USB_DEVICE
void KymeraUsbAudio_Start(KYMERA_INTERNAL_USB_AUDIO_START_T *audio_params);
#else
#define KymeraUsbAudio_Start(x) UNUSED(x)
#endif

/*! \brief Stop USB Audio.
    \param audio_params disconnect parameters defined by USB audio source.
*/
#ifdef INCLUDE_USB_DEVICE
void KymeraUsbAudio_Stop(KYMERA_INTERNAL_USB_AUDIO_STOP_T *audio_params);
#else
#define KymeraUsbAudio_Stop(x) UNUSED(x)
#endif

/*! \brief Set vloume for USB Audio.
    \param volume_in_db Volume value to be set for Audio chain.
*/
#ifdef INCLUDE_USB_DEVICE
void KymeraUsbAudio_SetVolume(int16 volume_in_db);
#else
#define KymeraUsbAudio_SetVolume(x) UNUSED(x)
#endif

#endif // KYMERA_USB_AUDIO_H

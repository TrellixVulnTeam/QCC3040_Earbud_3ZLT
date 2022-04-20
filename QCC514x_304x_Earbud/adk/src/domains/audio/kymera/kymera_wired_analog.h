/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Kymera header for Wired Analog
*/
#ifndef KYMERA_WIRED_ANALOG_H_
#define KYMERA_WIRED_ANALOG_H_

#include <stdint.h>

/*! \brief The KYMERA_INTERNAL_WIRED_ANALOG_AUDIO_START message content. */
typedef struct
{
    /*! The volume to set. */
    int16_t volume_in_db;
    /*! sampling rate */
    uint32_t rate;
    /*! configure the required latency range */
    uint32_t min_latency;
    uint32_t max_latency;
    uint32_t target_latency;
} KYMERA_INTERNAL_WIRED_ANALOG_AUDIO_START_T;

/*! \brief The KYMERA_INTERNAL_WIRED_AUDIO_SET_VOL message content. */
typedef struct
{
    /*! The volume to set. */
    int16_t volume_in_db;
} KYMERA_INTERNAL_WIRED_AUDIO_SET_VOL_T;

#if defined(INCLUDE_WIRED_ANALOG_AUDIO) || defined(INCLUDE_A2DP_ANALOG_SOURCE)
/*! \brief Create wired analog chain and start playing the audio
      \param msg internal message which has the configuration for wired analog chain */
void KymeraWiredAnalog_StartPlayingAudio(const KYMERA_INTERNAL_WIRED_ANALOG_AUDIO_START_T *msg);
#else
#define KymeraWiredAnalog_StartPlayingAudio(msg) ((void)(0))
#endif

#if defined(INCLUDE_WIRED_ANALOG_AUDIO) || defined(INCLUDE_A2DP_ANALOG_SOURCE)
/*! \brief Destroy the wired audio chain */
void KymeraWiredAnalog_StopPlayingAudio(void);
#else
#define KymeraWiredAnalog_StopPlayingAudio() ((void)(0))
#endif

#ifdef INCLUDE_WIRED_ANALOG_AUDIO
/*! \brief Set the wired analog audio volume */
void KymeraWiredAnalog_SetVolume(int16 volume_in_db);
#else
#define KymeraWiredAnalog_SetVolume(x) UNUSED(x)
#endif

#ifdef INCLUDE_WIRED_ANALOG_AUDIO
/*! \brief Init wired analog audio module */
void KymeraWiredAnalog_Init(void);
#else
#define KymeraWiredAnalog_Init() ((void)(0))
#endif

#endif // KYMERA_WIRED_ANALOG_H_

/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   wired_audio_source wired audio source
\ingroup    wired_audio_source
\brief      Provides mechanism to interface with wired audio module
*/

#ifndef WIRED_AUDIO_SOURCE_H_
#define WIRED_AUDIO_SOURCE_H_

#include "domain_message.h"
#include "audio_sources_list.h"

/*! Macro that defines a invalid PIO. */
#define WIRED_AUDIO_INVALID_PIO               (0xFF)
#define WIRED_AUDIO_DEFAULT_VOLUME 10

/*! \brief Message to send to registered clients when a wired audio device attaches/detaches */
typedef enum
{
    /*! Wired audio device connected */
    WIRED_AUDIO_DEVICE_CONNECT_IND = WIRED_AUDIO_DETECT_MESSAGE_BASE,

    /*! Wired audio device disconnected */
    WIRED_AUDIO_DEVICE_DISCONNECT_IND,

    /*! This must be the final message */
    WIRED_AUDIO_DETECT_MESSAGE_END
} wired_audio_detect_msg_t;

/*! Definition of message sent to registered clients upon wired device connection */
typedef struct
{
   audio_source_t audio_source;
} WIRED_AUDIO_DEVICE_CONNECT_IND_T;

/*! Definition of message sent to registered clients upon wired device disconnection */
typedef struct
{
   audio_source_t audio_source;
} WIRED_AUDIO_DEVICE_DISCONNECT_IND_T;

/*! Definition of wired audio source PIO's to monitor */
typedef struct
{
  uint8 line_in_pio;
} wired_audio_pio_t;

/*! Definition of wired audio configuration */
typedef struct
{
    uint32 rate; /*! input rate */
    uint32 min_latency; /*! in milli-seconds */
    uint32 max_latency; /*! in milli-seconds */
    uint32 target_latency; /*! in milli-seconds */
}wired_audio_config_t;


/*! \brief Initialises the Wired Audio Detect.

    The task list will be created upon initialisation.
    \param source_pio   Source PIO's to monitor.
*/
#if defined(INCLUDE_WIRED_ANALOG_AUDIO) || defined(INCLUDE_A2DP_ANALOG_SOURCE)
void WiredAudioSource_Init(const wired_audio_pio_t *source_pio);
#else
#define WiredAudioSource_Init(source_pio) ((void)(source_pio))
#endif

/*! \brief Configure the wired audio configuration
    \param config   wired analog audio configuration
*/
#if defined(INCLUDE_WIRED_ANALOG_AUDIO) || defined(INCLUDE_A2DP_ANALOG_SOURCE)
void WiredAudioSource_Configure(const wired_audio_config_t *config);
#else
#define WiredAudioSource_Configure(config) ((void)(config))
#endif

/*! \brief Register a Task to receive notification when wired audio device is plugged in/removed.

    Once registered, #client_task will receive #wired_audio_detect_msg_t messages.

    \param client_task Task to register to receive wired audio detect notifications.
*/
#if defined(INCLUDE_WIRED_ANALOG_AUDIO) || defined(INCLUDE_A2DP_ANALOG_SOURCE)
void WiredAudioSource_ClientRegister(Task client_task);
#else
#define WiredAudioSource_ClientRegister(client_task) ((void)(0))
#endif

/*! \brief Un-register a Task that is receiving notifications from wired audio detect

    If the task is not currently registered then nothing will be changed.

    \param client_task Task to un-register from wired audio source detect notifications.
*/
#if defined(INCLUDE_WIRED_ANALOG_AUDIO) || defined(INCLUDE_A2DP_ANALOG_SOURCE)
void WiredAudioSource_ClientUnregister(Task client_task);
#else
#define WiredAudioSource_ClientUnregister(client_task) ((void)(0))
#endif

/*! \brief Start Wired audio pio monitoring.

    \param requesting_task Task to be notified with the results.
*/
#if defined(INCLUDE_WIRED_ANALOG_AUDIO) || defined(INCLUDE_A2DP_ANALOG_SOURCE)
bool WiredAudioSource_StartMonitoring(Task requesting_task);
#else
#define WiredAudioSource_StartMonitoring(task) ((void)(0))
#endif


/*! \brief Stop Wired audio pio monitoring.

    \param requesting_task Task to be notified with the results.
*/
#if defined(INCLUDE_WIRED_ANALOG_AUDIO) || defined(INCLUDE_A2DP_ANALOG_SOURCE)
bool WiredAudioSource_StopMonitoring(Task requesting_task);
#else
#define WiredAudioSource_StopMonitoring(task) ((void)(0))
#endif

/*! \brief Check if any Wired audio device is available.

    \param
*/
#if defined(INCLUDE_WIRED_ANALOG_AUDIO) || defined(INCLUDE_A2DP_ANALOG_SOURCE)
bool WiredAudioSource_IsAudioAvailable(audio_source_t source);
#else
#define  WiredAudioSource_IsAudioAvailable(source) (FALSE)
#endif

#endif /* WIRED_AUDIO_SOURCE_H_ */

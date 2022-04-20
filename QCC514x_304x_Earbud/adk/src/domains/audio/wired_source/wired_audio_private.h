/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup  Wired Audio
\ingroup    wired audio
\brief  private utility interface declarations of wired audio
*/

#ifndef WIRED_AUDIO_PRIVATE_H_
#define WIRED_AUDIO_PRIVATE_H_

#include "audio_sources_list.h"
#include "wired_audio_source.h"
#include <csrtypes.h>
#include <task_list.h>

/*! Macro that defines wired audio line_in source type */
#define WIRED_AUDIO_SOURCE_LINE_IN            (0x1)

/*! wired audio detect task data */
typedef struct
{
    TaskData task;

    /*! List of tasks registered for notifications from wired audio detect */
    task_list_t *client_tasks;

    /*! Mask that tells which wired audio device type is connected currently */
    uint8 wired_devices_mask;

    /*! Line in PIO to monitor */
    uint8 line_in_pio;

    uint32 rate; /*! input rate */
    uint32 min_latency; /*! in milli-seconds */
    uint32 max_latency; /*! in milli-seconds */
    uint32 target_latency; /*! in milli-seconds */
    bool allow_pio_monitor_events; /*! wired pio events are allowed */
} wiredAudioSourceTaskData;

extern wiredAudioSourceTaskData wiredaudio_source_taskdata;

#define WiredAudioSourceGetTaskData()     (&wiredaudio_source_taskdata)

/*! Wired audio detect message handler */
void WiredAudioDetect_HandleMessage(Task task, MessageId id, Message message);

void WiredAudioSource_UpdateClient(void);

void WiredAudioDetect_ConfigurePio(const wired_audio_pio_t * source_pio);
bool WiredAudioDetect_IsSet(uint8 mask);
void WiredAudioDetect_UpdateMask(bool pio_state, uint8 mask);
void WiredAudioDetect_ResetMask(void);

bool WiredAudioDetect_StartMonitoring(void);
bool WiredAudioDetect_StopMonitoring(void);

unsigned WiredAudioSource_GetContext(audio_source_t source);

/*! \brief Send Wired audio status messages to clients. */
void WiredAudioSource_SendStatusMessage(bool connected, audio_source_t src);

/*! \brief Increment wired audio volume */
void WiredAudioSource_IncrementVolume(audio_source_t source);

/*! \brief Decrement wired audio volume */
void WiredAudioSource_DecrementVolume(audio_source_t source);

#endif /* WIRED_AUDIO_PRIVATE_H_ */


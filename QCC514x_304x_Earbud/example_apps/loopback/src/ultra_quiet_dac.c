/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    %%version
\file       ultra_quiet_mode.c
\brief      Loopback module
*/

#include <ultra_quiet_dac.h>
#include <panic.h>
#include <sink.h>
#include <stream.h>
#include <logging.h>

enum ultra_quiet_messages
{
    ULTRA_QUIET_MODE_ENABLE_MESSAGE,
    ULTRA_QUIET_MODE_DISABLE_MESSAGE,
    /*! This must be the final message */
    ULTRA_QUIET_MODE_MESSAGE_END
};

typedef enum app_kymera_states
{
    UltraQuietModeEnable,
    UltraQuietModeDisable
}QuietModeState;

typedef struct
{
    TaskData task;
    /*! The current state. */
    QuietModeState    state;
}UltraQuietModeTaskData;

/*!< State data for the DSP configuration */
extern UltraQuietModeTaskData  app_ultra_quiet_mode;

/*! Get pointer to ultra_quiet_mode structure */
#define  getUltraQuietDacTaskData()  (&app_ultra_quiet_mode)

/*! Get task from the ultra_quiet_mode */
#define getUltraQuietDacTask()    (&app_ultra_quiet_mode.task)

UltraQuietModeTaskData app_ultra_quiet_mode;

static void ultraQuietMode_msg_handler(Task task, MessageId id, Message msg)
{
    UltraQuietModeTaskData *ultraQuietMode =  getUltraQuietDacTaskData();

    switch(id)
    {
    case ULTRA_QUIET_MODE_ENABLE_MESSAGE:
        LoopbackTest_SetEnableUltraQuietMode();
        ultraQuietMode->state = UltraQuietModeEnable;
        break;
    case ULTRA_QUIET_MODE_DISABLE_MESSAGE:
        LoopbackTest_SetDisableUltraQuietMode();
        ultraQuietMode->state = UltraQuietModeDisable;
        break;
    default:
        break; /*do-nothing*/
    }
    UNUSED(task);
    UNUSED(msg);
}

void setupUltraQuietDac(void)
{
    UltraQuietModeTaskData *ultraQuietMode = getUltraQuietDacTaskData();
    memset(ultraQuietMode, 0, sizeof(*ultraQuietMode));
    ultraQuietMode->task.handler = ultraQuietMode_msg_handler;
    ultraQuietMode->state = UltraQuietModeDisable ;
}

bool LoopbackTest_SetEnableUltraQuietMode(void)
{
#define STREAM_CODEC_DAC_QUIET_MODE (0x0320U)
#define ENABLE_DAC_QUIET_MODE (0x1U)
    UltraQuietModeTaskData *ultraQuietMode = getUltraQuietDacTaskData();
    if(ultraQuietMode->state == UltraQuietModeDisable)
    {
        Sink dac_sink = StreamAudioSink(AUDIO_HARDWARE_CODEC, AUDIO_INSTANCE_0, AUDIO_CHANNEL_A);
        if(dac_sink)
        {
            PanicFalse(SinkConfigure(dac_sink, STREAM_CODEC_DAC_QUIET_MODE, ENABLE_DAC_QUIET_MODE));
            ultraQuietMode->state = UltraQuietModeEnable;
            DEBUG_LOG("The ultra quiet mode is enabled");
            return TRUE;
        }
    }
    else
    {
        /*Do-nothing*/
    }
    return FALSE;
}

bool LoopbackTest_SetDisableUltraQuietMode(void)
{
#define STREAM_CODEC_DAC_QUIET_MODE (0x0320U)
#define DISABLE_DAC_QUIET_MODE (0x0U)
    UltraQuietModeTaskData *ultraQuietMode = getUltraQuietDacTaskData();
    if(ultraQuietMode->state == UltraQuietModeEnable)
    {
        Sink dac_sink = StreamAudioSink(AUDIO_HARDWARE_CODEC, AUDIO_INSTANCE_0, AUDIO_CHANNEL_A);
        if(dac_sink)
        {
            ultraQuietMode->state=UltraQuietModeDisable;
            PanicFalse(SinkConfigure(dac_sink, STREAM_CODEC_DAC_QUIET_MODE, DISABLE_DAC_QUIET_MODE));
            DEBUG_LOG("The ultra quiet mode is disabled");
            return TRUE;
        }
    }
    else
    {
        /*Do-nothing*/
    }
    return FALSE;
}

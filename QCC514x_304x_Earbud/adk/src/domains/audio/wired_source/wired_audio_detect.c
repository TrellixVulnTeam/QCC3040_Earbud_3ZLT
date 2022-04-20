/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       wired_audio_detect.c
\brief      Provides a mechanism to detect wired audio sources plugged in/out.
*/

#include <stdlib.h>

#include "logging.h"
#include "wired_audio_source.h"
#include "wired_audio_private.h"
#include "pio_common.h"
#include "pio_monitor.h"

#define NUM_VALID_PIOS                    (0x1F)
#define WiredAudioDetectGetTask()         (&wiredaudio_source_taskdata.task)
#define Pio_GetMask(pio)                  ((uint32)1 << ((pio) & NUM_VALID_PIOS))
#define Line_In_Ready()                   (wiredaudio_source_taskdata.line_in_pio != WIRED_AUDIO_INVALID_PIO)
#define IsPioMonitorEventsAllowed()       (wiredaudio_source_taskdata.allow_pio_monitor_events)
#define IsLineInAvailable(sp)             ((!PioCommonGetPio(sp->line_in_pio))? TRUE : FALSE)

/*! Function to setup the wired audio PIO's */
static bool wiredAudioDetect_SetupPio(unsigned pio)
{
    pio_common_allbits mask;

    if(pio == WIRED_AUDIO_INVALID_PIO)
        return FALSE;

    PioCommonBitsInit(&mask);
    PioCommonBitsSetBit(&mask, pio);
    PioCommonSetMap(&mask, &mask);
 
    return PioCommonSetPio(pio, pio_pull, TRUE);
}

/*! Function to add the newly arrived wired audio device to the devices mask */
static void wiredAudioDetect_SetMask(uint8 mask)
{
    wiredAudioSourceTaskData *sp = WiredAudioSourceGetTaskData();
    sp->wired_devices_mask |= mask;
}

/*! Function to remove the wired audio device from the devices mask */
static void wiredAudioDetect_ClearMask(uint8 mask)
{
    wiredAudioSourceTaskData *sp = WiredAudioSourceGetTaskData();
    sp->wired_devices_mask &= ~mask;
}

/*! Function to handle the wired audio PIO status messages.*/
static bool wiredAudioChanged(const MessagePioChanged* mpc)
{
    wiredAudioSourceTaskData *sp = WiredAudioSourceGetTaskData(); 
    bool update_client = FALSE; 
 
    uint8 prev_line_in_state = sp->wired_devices_mask & WIRED_AUDIO_SOURCE_LINE_IN;
    DEBUG_LOG("wiredAudioDetect_HandleMessage() Received MESSAGE_PIO_CHANGED");

    /* Merge both bank PIO's together and verify with the PIO mask */
    const uint32 pio_state = (mpc->state) + ((uint32)mpc->state16to31 << 16);
    bool line_in_pio_state = (Pio_GetMask(sp->line_in_pio) & pio_state);
    WiredAudioDetect_UpdateMask(line_in_pio_state, WIRED_AUDIO_SOURCE_LINE_IN);
    update_client = (prev_line_in_state != (sp->wired_devices_mask & WIRED_AUDIO_SOURCE_LINE_IN));

    return update_client;
}

/*! \brief Message handler for Wired audio detect module.*/
void WiredAudioDetect_HandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);

    switch (id)
    {
        case MESSAGE_PIO_CHANGED:
        { 
            /* Process PIO events from PIO monitor only when monitoring is enabled */ 
            if(IsPioMonitorEventsAllowed())
            {
               if(wiredAudioChanged((const MessagePioChanged *)message)) 
                  WiredAudioSource_UpdateClient();
            }
        }
        break;

        default:
        break;
    }
}

/*! Function to add/remove the wired audio device from the devices mask */
void WiredAudioDetect_UpdateMask(bool pio_state, uint8 mask)
{
    if(!pio_state)
        wiredAudioDetect_SetMask(mask);
    else
        wiredAudioDetect_ClearMask(mask);
}


/*! \brief configure PIO to detect wired audio */
void WiredAudioDetect_ConfigurePio(const wired_audio_pio_t * source_pio)
{
    wiredAudioSourceTaskData *sp = WiredAudioSourceGetTaskData();

    sp->line_in_pio = source_pio->line_in_pio;

    /* Setup PIOs */
    if(wiredAudioDetect_SetupPio(sp->line_in_pio))
    {
       /* Register with PIO Monitor library */
       PioMonitorRegisterTask(WiredAudioDetectGetTask(),sp->line_in_pio);
    }
}

bool WiredAudioDetect_IsSet(uint8 mask)
{
    wiredAudioSourceTaskData *sp = WiredAudioSourceGetTaskData();
    return ((sp->wired_devices_mask & mask) ? TRUE : FALSE);
}

void WiredAudioDetect_ResetMask(void)
{
    uint8 reset_mask = WIRED_AUDIO_SOURCE_LINE_IN;
    wiredAudioDetect_ClearMask(reset_mask);
}

bool WiredAudioDetect_StartMonitoring(void)
{
    bool status = FALSE;
    wiredAudioSourceTaskData *sp = WiredAudioSourceGetTaskData();
    
    if(Line_In_Ready())
    {
        sp->allow_pio_monitor_events = TRUE;
        status = TRUE;

        /* A wired device already plugged in */
        if(IsLineInAvailable(sp))
        {
           /* Set the mask correctly */
           wiredAudioDetect_SetMask(WIRED_AUDIO_SOURCE_LINE_IN);

           /* Send a WIRED_AUDIO_CONNECT_IND message to clients */
           WiredAudioSource_SendStatusMessage(TRUE,audio_source_line_in);
        }
    }
    
    return status;
}

bool WiredAudioDetect_StopMonitoring(void)
{
    bool status = FALSE;
    wiredAudioSourceTaskData *sp = WiredAudioSourceGetTaskData();
    
    if(Line_In_Ready())
    {
        sp->allow_pio_monitor_events = FALSE;
        status = TRUE;

        /* A wired device already plugged in(power off scenario) */
        if(IsLineInAvailable(sp))
        {
           /* Clear the devices mask */
           wiredAudioDetect_ClearMask(WIRED_AUDIO_SOURCE_LINE_IN);

           /* Send a wired audio disconnect message to registered clients */
           WiredAudioSource_SendStatusMessage(FALSE,audio_source_line_in);
        }
    }
    
    return status;
}


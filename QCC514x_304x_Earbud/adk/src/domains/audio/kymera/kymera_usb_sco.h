/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera_usb_sco.h
\brief      Kymera USB to SCO Driver
*/

#ifndef KYMERA_USB_SCO_H
#define KYMERA_USB_SCO_H

#include <usb_audio.h>
#include <sink.h>
#include <source.h>


typedef struct
{
    usb_voice_mode_t mode;
    uint8 spkr_channels;
    Source spkr_src;
    Sink mic_sink;
    Sink sco_sink;
    uint32 spkr_sample_rate;
    uint32 mic_sample_rate;
    uint32_t sco_sample_rate;
    uint32 min_latency_ms;
    uint32 max_latency_ms;
    uint32 target_latency_ms;
} KYMERA_INTERNAL_USB_SCO_VOICE_START_T;

/*! \brief Disconnect message for USB Voice. */
typedef struct
{
     Source spkr_src;
     Sink mic_sink;
     Sink sco_sink;
     void (*kymera_stopped_handler)(Source source);
} KYMERA_INTERNAL_USB_SCO_VOICE_STOP_T;

void KymeraUsbScoVoice_Start(KYMERA_INTERNAL_USB_SCO_VOICE_START_T *usb_sco_voice);
void KymeraUsbScoVoice_Stop(KYMERA_INTERNAL_USB_SCO_VOICE_STOP_T *usb_sco_stop);

#endif // KYMERA_USB_SCO_H

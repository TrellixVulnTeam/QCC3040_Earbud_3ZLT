/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Kymera module to manage MIC resampler chain used for MIC concurrency
*/

#ifndef KYMERA_MIC_RESAMPLER_H_
#define KYMERA_MIC_RESAMPLER_H_

void Kymera_MicResamplerCreate(uint8 stream_index, uint32 input_sample_rate, uint32 output_sample_rate);
void Kymera_MicResamplerDestroy(uint8 stream_index);

void Kymera_MicResamplerStart(uint8 stream_index);
void Kymera_MicResamplerStop(uint8 stream_index);

bool Kymera_MicResamplerIsCreated(uint8 stream_index);

Sink Kymera_MicResamplerGetAecInput(uint8 stream_index);
Source Kymera_MicResamplerGetAecOutput(uint8 stream_index);

Sink Kymera_MicResamplerGetMicInput(uint8 stream_index, uint8 mic_index);
Source Kymera_MicResamplerGetMicOutput(uint8 stream_index, uint8 mic_index);

void Kymera_MicResamplerSleep(void);
void Kymera_MicResamplerWake(void);

#endif // KYMERA_MIC_RESAMPLER_H_

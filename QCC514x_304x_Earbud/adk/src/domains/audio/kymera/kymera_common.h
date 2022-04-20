/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Kymera private header for common helper definitions
*/

#ifndef KYMERA_COMMON_H_
#define KYMERA_COMMON_H_

#include <source.h>
#include <sink.h>
#include <chain.h>
#include <rtime.h>
#include "microphones.h"

#define TTP_BUFFER_SIZE 4096

#define SAMPLING_RATE_96000 96000

#define DEFAULT_LOW_POWER_CLK_SPEED_MHZ (32)
#define BOOSTED_LOW_POWER_CLK_SPEED_MHZ (45)

/*!:{ \name Macros to calculate buffer sizes required to hold a specific (timed) amount of audio. */
#define CODEC_BITS_PER_MEMORY_WORD (16)
#define MS_TO_BUFFER_SIZE_MONO_PCM(time_ms, sample_rate) ((((time_ms) * (sample_rate)) + (MS_PER_SEC-1)) / MS_PER_SEC)
#define US_TO_BUFFER_SIZE_MONO_PCM(time_us, sample_rate) ((((time_us) * (sample_rate)) + (US_PER_SEC-1)) / US_PER_SEC)
#define MS_TO_BUFFER_SIZE_CODEC(time_ms, codec_rate_kbps) ((((time_ms) * (codec_rate_kbps)) + (CODEC_BITS_PER_MEMORY_WORD-1)) / CODEC_BITS_PER_MEMORY_WORD)
/*!@}*/

/*! Convert x into 1.31 format */
#define FRACTIONAL(x) ( (int32)( (x) * (((uint32)1<<31) - 1) ))

/*! Default DAC disconnection delay in milliseconds */
#define appKymeraDacDisconnectionDelayMs() (30000)

/*! \brief Macro to help getting an operator from chain.
    \param op The returned operator, or INVALID_OPERATOR if the operator was not
           found in the chain.
    \param chain_handle The chain handle.
    \param role The operator role to get from the chain.
    \return TRUE if the operator was found, else FALSE
 */
#define GET_OP_FROM_CHAIN(op, chain_handle, role) \
    (INVALID_OPERATOR != ((op) = ChainGetOperatorByRole((chain_handle), (role))))

/**
 *  \brief Connect if both Source and Sink are valid.
 *  \param source The Source data will be taken from.
 *  \param sink The Sink data will be written to.
 *  \note In the case of connection failuar, it will panics the application.
 * */
void Kymera_ConnectIfValid(Source source, Sink sink);

/**
 *  \brief Break any existing automatic connection involving the source *or* sink.
 *   Source or sink may be NULL.
 *  \param source The source which needs to be disconnected.
 *  \param sink The sink which needs to be disconnected.
 * */
void Kymera_DisconnectIfValid(Source source, Sink sink);

/*  \brief Connect audio output chain endpoints to appropriate hardware outputs
    \param left source of Left output channel
    \param right source of Right output channel if stereo output supported
    \param output_sample_rate The output sample rate to be set
*/
void Kymera_ConnectOutputSource(Source left, Source right, uint32 output_sample_rate);

/*! \brief Setup an external amplifier. */
void appKymeraExternalAmpSetup(void);

/*! \brief Update the DSP clock speed settings for the clock speed enums for the lowest
           power consumption possible based on the current state / codec.
*/
void appKymeraConfigureDspClockSpeed(void);

/*! \brief Configure power mode and clock frequencies of the DSP for the lowest
           power consumption possible based on the current state / codec.

   \note Calling this function with chains already started may cause audible
   glitches if using I2S output.
*/
void appKymeraConfigureDspPowerMode(void);

/*! \brief Configure the active DSP clock.
    \return TRUE on success.
    \note Changing the clock with chains already started may cause audible
    glitches if using I2S output.
*/
bool appKymeraSetActiveDspClock(audio_dsp_clock_type type);

/*! \brief returns the microphone bias voltage

    \param microphone bias id.
*/
unsigned Kymera_GetMicrophoneBiasVoltage(mic_bias_id id);

Source Kymera_GetMicrophoneSource(microphone_number_t microphone_number, Source source_to_synchronise_with, uint32 sample_rate,
                                        microphone_user_type_t microphone_user_type);

void Kymera_CloseMicrophone(microphone_number_t microphone_number, microphone_user_type_t microphone_user_type);

/*! \brief return the number of microphones used

    \param None
    \return number of microphones, default is 1
*/
uint8 Kymera_GetNumberOfMics(void);

#endif /* KYMERA_COMMON_H_ */

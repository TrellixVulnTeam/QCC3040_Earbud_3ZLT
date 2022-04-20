/****************************************************************************
Copyright (c) 2013 - 2020 Qualcomm Technologies International, Ltd.

FILE NAME
    audio_i2s_common.h

DESCRIPTION

*/

/*!
@file   audio_i2s_common.h
@brief  Base functionality for I2S audio input and output devices.
     
*/


#ifndef _AUDIO_I2S_COMMON_H_
#define _AUDIO_I2S_COMMON_H_

#include <library.h>
#include <stream.h>

typedef enum
{
    i2s_out_1_left = 0,
    i2s_out_1_right,
    i2s_out_2_left,
    i2s_out_2_right
} i2s_out_t;

/* PSKEY_USR35 I2S pskey configuration*/
typedef struct
{
    /* master or slave operation: 0 = slave, 1 = master */
    unsigned master_mode:4;
    /* data format: 0 = left justified, 1 = right justified */
    unsigned data_format:2;
    /* left justified bit delay: 0 = none, 1 = delay MSB by 1 bit (I2S standard) */
    unsigned bit_delay:2;
    /* bits per sample */
    uint8 bits_per_sample;
    /* i2s bit clock frequency scale factor */
    uint16 bclk_scaling;
    /* i2s master clock frequency scale factor */
    uint16 mclk_scaling;
     /* external amplifier enable pio */
    uint8 enable_pio;

} i2s_config_t;


/****************************************************************************
DESCRIPTION:

    This function configures the I2S interface ready for a connection.

PARAMETERS:
    Sink sink   - The Sink to configure
    uint32 rate - Sample rate of the data coming from the DSP

RETURNS:
    Nothing.
*/
void AudioI2SConfigureSink(Sink sink, uint32 rate);

/****************************************************************************
DESCRIPTION:

    This function enables or disables the MCLK signal for an I2S interface,
    if required. It is separate from sink configuration as it must happen
    after all channels for a given interface have been configured, or after
    all channels of the interface have been disconnected, if disabling MCLK.

PARAMETERS:
    Sink sink   - The Sink to enable/disable MCLK for
    Bool enable - TRUE to enable MCLK output, FALSE to disable

RETURNS:
    none
*/
void AudioI2SEnableMasterClockIfRequired(Sink sink, bool enable);

/****************************************************************************
DESCRIPTION:

    This function enables or disables the MCLK signal for an I2S interface,
    if required. It is separate from source configuration as it must happen
    after all channels for a given interface have been configured, or after
    all channels of the interface have been disconnected, if disabling MCLK.

PARAMETERS:
    Source source   - The Source to enable/disable MCLK for
    Bool enable     - TRUE to enable MCLK output, FALSE to disable

RETURNS:
    none
*/
void AudioI2SSourceEnableMasterClockIfRequired(Source source, bool enable);

/****************************************************************************
DESCRIPTION:

    Powers on and configures the I2S device.

    NOTE: To be implemented by the application level driver.

PARAMETERS:
    uint32 sample_rate - sample rate of data coming from dsp

RETURNS:
    Whether the device was successfully initialised.
*/    
bool AudioI2SDeviceInitialise(uint32 sample_rate);

/****************************************************************************
DESCRIPTION:

    Shuts down and powers off the I2S device.

    NOTE: To be implemented by the application level driver.

PARAMETERS:
    none
    
RETURNS:
    Whether the device was successfully shut down.
*/    
bool AudioI2SDeviceShutdown(void);

/******************************************************************************
DESCRIPTION:

    Sets the inital hardware gain of a single I2S channel, on a specific I2S
    device, via the I2C interface. The gain is passed in as a value in 1/60th's
    of a dB (with range -7200 to 0, representing -120dB to 0dB). Some chips may
    support positive gain levels higher than 0dB, but this is device-specific.

    NOTE: To be implemented by the application level driver.

PARAMETERS:
    i2s_out_t channel   The I2S device and channel to set the volume of.
    int16 gain          The gain level required for that channel, in 1/60dB.

RETURNS:
    Whether gain was successfully changed for the requested device channel.
*/
bool AudioI2SDeviceSetChannelGain(i2s_out_t channel, int16 gain);

/******************************************************************************
DESCRIPTION:

    Convenience function to set the initial hardware gain of all I2S channels to
    the same level, or set the master gain, if available on a specific chip.
    Useful for muting, as an example.

    NOTE: To be implemented by the application level driver.

PARAMETERS:
    int16 gain - The gain level to set, in dB/60.

RETURNS:
    Whether gain was successfully applied to all channels.
*/
bool AudioI2SDeviceSetGain(int16 gain);

/****************************************************************************
DESCRIPTION:

    Gets the overall delay of the I2S hardware, for TTP adjustment.

    NOTE: To be implemented by the application level driver.

PARAMETERS:
    none

RETURNS:
    Delay in milliseconds.
*/
uint16 AudioI2SDeviceGetDelay(void);

/****************************************************************************
DESCRIPTION:

    This function configures the supplied source

PARAMETERS:

    Source

RETURNS:
    none
*/
void AudioI2SConfigureSource(Source source, uint32 rate, uint16 bit_resolution);

/****************************************************************************
DESCRIPTION:

    This function returns the I2S operation mode 

PARAMETERS:
    
    none

RETURNS:
    
    TRUE : Operation mode is I2S Master
    FALSE : Operation mode is I2S Slave 
    
*/    
bool AudioI2SMasterIsEnabled(void);

/****************************************************************************
DESCRIPTION:

    This function returns true if I2S has been configured to output an MCLK signal

PARAMETERS:

    none

RETURNS:

    TRUE : I2S MCLK required
    FALSE : I2S MCLK not required
*/
bool AudioI2SIsMasterClockRequired(void);

/****************************************************************************
DESCRIPTION:
    Returns pointer to the current config data set.

PARAMETERS:
    none
*/
const i2s_config_t* AudioI2SGetConfig(void);

/****************************************************************************
DESCRIPTION:
    Override the default config with a new set of config data.

PARAMETERS:
    config - Pointer to new (caller malloc'd) config data structure.

RETURNS:
    none
*/
void AudioI2SSetConfig(const i2s_config_t* config);


#endif

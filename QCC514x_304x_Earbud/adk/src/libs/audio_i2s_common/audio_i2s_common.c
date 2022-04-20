/****************************************************************************
Copyright (c) 2013 - 2020 Qualcomm Technologies International, Ltd.

FILE NAME
    audio_i2s_common.c
DESCRIPTION
    Base support for external I2S audio devices
NOTES
*/

#include <audio_plugin_if.h>
#include <gain_utils.h>
#include <stdlib.h>
#include <panic.h>
#include <print.h>
#include <file.h>
#include <stream.h> 
#include <sink.h>
#include <source.h>
#include <kalimba.h>
#include <kalimba_standard_messages.h>
#include <message.h>
#include <transform.h>
#include <string.h>

#include "audio_i2s_common.h"
#include "audio_i2s_mclk.h"


/*============================================================================*
 *  Private Data
 *============================================================================*/

/* Default config data set that should be provided by a chip-specific driver */
extern const i2s_config_t device_i2s_config;

/* I2S configuration data */
const i2s_config_t* I2S_config = &device_i2s_config;

#define POLARITY_LEFT_WHEN_WS_HIGH  0
#define POLARITY_RIGHT_WHEN_WS_HIGH 1


/*============================================================================*
 * Public Function Implementations
 *============================================================================*/

/****************************************************************************
DESCRIPTION:

    This function configures the I2S interface ready for a connection.

PARAMETERS:

    Sink sink   - The Sink to configure
    uint32 rate - Sample rate of the data coming from the DSP

RETURNS:
    Nothing.
*/
void AudioI2SConfigureSink(Sink sink, uint32 rate)
{
    /* configure the I2S interface operating mode, run in master or slave mode */
    PanicFalse(SinkConfigure(sink, STREAM_I2S_MASTER_MODE, I2S_config->master_mode));

    /* set the bit clock (BCLK) rate for master mode */
    PanicFalse(SinkConfigure(sink, STREAM_I2S_MASTER_CLOCK_RATE, (rate * I2S_config->bclk_scaling)));

    /* set the word clock (SYNC) rate of the dsp audio data */
    PanicFalse(SinkConfigure(sink, STREAM_I2S_SYNC_RATE, rate));

    /* left justified or i2s data */
    PanicFalse(SinkConfigure(sink, STREAM_I2S_JSTFY_FORMAT, I2S_config->data_format));

    /* MSB of data occurs on the second CLK */
    PanicFalse(SinkConfigure(sink, STREAM_I2S_LFT_JSTFY_DLY, I2S_config->bit_delay));

    /* AUDIO_CHANNEL_SLOT_0 = left, AUDIO_CHANNEL_SLOT_1 = right */
    PanicFalse(SinkConfigure(sink, STREAM_I2S_CHNL_PLRTY, POLARITY_RIGHT_WHEN_WS_HIGH));

    /* number of data bits per sample, 16 or 24 */
    PanicFalse(SinkConfigure(sink, STREAM_I2S_BITS_PER_SAMPLE, I2S_config->bits_per_sample));

    /* configure MCLK for the interface */
    AudioI2SConfigureMasterClockIfRequired(sink, rate);

    PRINT(("I2S: AudioI2SConfigureSink A&B, %luHz\n", (unsigned long) rate));
}

/****************************************************************************
DESCRIPTION:

    This function configures the supplied source

PARAMETERS:

    Source

RETURNS:
    none
*/
void AudioI2SConfigureSource(Source source, uint32 rate, uint16 bit_resolution)
{
    uint16 I2S_audio_sample_size = RESOLUTION_MODE_16BIT;

    /* configure the I2S interface operating mode, run in master or slave mode */
    PanicFalse(SourceConfigure(source, STREAM_I2S_MASTER_MODE, I2S_config->master_mode));
    
    /* set the sample rate of the dsp audio data */
    PanicFalse(SourceConfigure(source, STREAM_I2S_MASTER_CLOCK_RATE, (rate * I2S_config->bclk_scaling)));

    /* set the sample rate of the dsp audio data */
    PanicFalse(SourceConfigure(source, STREAM_I2S_SYNC_RATE, rate));
              
    /* left justified or i2s data */
    PanicFalse(SourceConfigure(source, STREAM_I2S_JSTFY_FORMAT, I2S_config->data_format));
     
    /* MSB of data occurs on the second SCLK */
    PanicFalse(SourceConfigure(source, STREAM_I2S_LFT_JSTFY_DLY, I2S_config->bit_delay));

    /* AUDIO_CHANNEL_SLOT_0 = left, AUDIO_CHANNEL_SLOT_1 = right */
    PanicFalse(SourceConfigure(source, STREAM_I2S_CHNL_PLRTY, POLARITY_RIGHT_WHEN_WS_HIGH));
     
    /* number of data bits per sample, 16 */
    PanicFalse(SourceConfigure(source, STREAM_I2S_BITS_PER_SAMPLE, I2S_config->bits_per_sample));

    if(I2S_config->bits_per_sample >= bit_resolution)
    {
        I2S_audio_sample_size = bit_resolution;
    }

    /* Specify the bits per sample for the audio input */
   PanicFalse(SourceConfigure(source, STREAM_AUDIO_SAMPLE_SIZE, I2S_audio_sample_size));
   
    /* configure MCLK for the interface */
    AudioI2SSourceConfigureMasterClockIfRequired(source, rate);
}


/****************************************************************************
DESCRIPTION:

    This function returns the I2S operation mode 

PARAMETERS:
    
    none

RETURNS:
    
    TRUE : Operation mode is I2S Master
    FALSE : Operation mode is I2S Slave 
    
*/    
bool AudioI2SMasterIsEnabled(void)
{
    if(I2S_config->master_mode)
        return TRUE;
    else
        return FALSE;
}

/****************************************************************************
DESCRIPTION:

    This function returns true if I2S has been configured to output an MCLK signal

PARAMETERS:

    none

RETURNS:

    TRUE : I2S MCLK required
    FALSE : I2S MCLK not required
*/
bool AudioI2SIsMasterClockRequired(void)
{
    if (I2S_config->mclk_scaling > 0)
        return TRUE;
    else
        return FALSE;
}

/****************************************************************************
DESCRIPTION:
    Returns pointer to the current config data set.

PARAMETERS:
    none
*/
const i2s_config_t* AudioI2SGetConfig(void) {
    return I2S_config;
}

/****************************************************************************
DESCRIPTION:
    Override the default config with a new set of config data.

PARAMETERS:
    config - Pointer to new (caller malloc'd) config data structure.

RETURNS:
    none
*/
void AudioI2SSetConfig(const i2s_config_t* config) {
    if (config) {
        I2S_config = config;
    }
}

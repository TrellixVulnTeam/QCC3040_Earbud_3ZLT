/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       audio_i2s_device.c
\brief      Support for I2S audio output, implementing the 'Device' API defined
            in audio_i2s_common.h.
*/

#include <audio_i2s_common.h>
#include <pio_common.h>


/*! Default I2S configuration data for this chip */
const i2s_config_t device_i2s_config =
{
    .master_mode = 1,       /*!< Default to master mode for pure I2S output */
    .data_format = 0,       /*!< I2S left justified */
    .bit_delay = 1,         /*!< I2S delay 1 bit */
    .bits_per_sample = 16,  /*!< 16-bit data words per channel */
    .bclk_scaling = 64,     /*!< 32 bit clocks per data word per channel */
    .mclk_scaling = 0,      /*!< External master clock disabled */
    .enable_pio = PIN_INVALID
};


/*! \brief Power on and configure the I2S device. */
bool AudioI2SDeviceInitialise(uint32 sample_rate)
{
    UNUSED(sample_rate);
    return TRUE;
}

/*! \brief Shut down and power off the I2S device. */
bool AudioI2SDeviceShutdown(void)
{
    return TRUE;
}

/*! \brief Set the initial hardware gain of the I2S device, per channel. */
bool AudioI2SDeviceSetChannelGain(i2s_out_t channel, int16 gain)
{
    UNUSED(channel);
    UNUSED(gain);
    return TRUE;
}

/*! \brief Set the initial hardware gain of all I2S channels to the same level. */
bool AudioI2SDeviceSetGain(int16 gain)
{
    UNUSED(gain);
    return TRUE;
}

/*! \brief Get the overall delay of the I2S hardware, for TTP adjustment. */
uint16 AudioI2SDeviceGetDelay(void)
{
    return 0;
}

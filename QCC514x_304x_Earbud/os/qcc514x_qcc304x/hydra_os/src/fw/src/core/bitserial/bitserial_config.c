/* Copyright (c) 2017 - 2021 Qualcomm Technologies International, Ltd. */
/*   %%version */
/**
 * \file
 * Implementation of configuration of bitserial hardware.
 */

#include "bitserial/bitserial.h"
#include "bitserial/bitserial_private.h"

#ifdef INSTALL_BITSERIAL

/******************************************************************************
 *
 *  bitserial_configure_spi
 *
 *  Configure the hardware for SPI operations
 */
static void bitserial_configure_spi(bitserial_instance i,
                                    const bitserial_config *vm_config,
                                    bool is_slave)
{
    const bitserial_spi_config *spi_cfg = &vm_config->u.spi_cfg;
    uint16 value;

    uint32 clock_sample_offset = 0;

    if (spi_cfg->clock_sample_offset)
    {
        uint16 clock_period_hi, clock_period_lo;

        hal_bitserial_clk_rate_get(i, &clock_period_hi, &clock_period_lo);

        clock_sample_offset = clock_period_lo + 1;
        clock_sample_offset *= spi_cfg->clock_sample_offset;
        clock_sample_offset /= 0x10000;
    }

    /* Set BITSERIAL_CONFIG */
    value = 0;

    if (clock_sample_offset)
    {
        value |= HAL_BITSERIAL_CONFIG_CLK_OFFSET_EN_MASK;

        hal_bitserial_clk_samp_offset_set(i, (uint16)(clock_sample_offset - 1));
    }

    switch (BITSERIAL_SPI_MODE_GET(spi_cfg->flags))
    {
    case BITSERIAL_SPI_MODE_0:
        break;
    case BITSERIAL_SPI_MODE_1:
        value |= HAL_BITSERIAL_CONFIG_NEG_EDGE_SAMP_EN_MASK |
                 HAL_BITSERIAL_CONFIG_POSEDGE_LAUNCH_MODE_EN_MASK;
        break;
    case BITSERIAL_SPI_MODE_2:
        value |= HAL_BITSERIAL_CONFIG_CLK_INVERT_MASK;
        break;
    case BITSERIAL_SPI_MODE_3:
        value |= HAL_BITSERIAL_CONFIG_CLK_INVERT_MASK |
                 HAL_BITSERIAL_CONFIG_NEG_EDGE_SAMP_EN_MASK |
                 HAL_BITSERIAL_CONFIG_POSEDGE_LAUNCH_MODE_EN_MASK;
        break;
    default:
        assert(0);
    }

    if (spi_cfg->sel_enabled)
    {
        /* Chip select enabled */
        value |= HAL_BITSERIAL_CONFIG_SEL_EN_MASK;

        value |= (spi_cfg->flags & BITSERIAL_SPI_CS_ACTIVE_HIGH) ?
                0 : HAL_BITSERIAL_CONFIG_SEL_INVERT_MASK;
    }

    value |= HAL_BITSERIAL_CONFIG_INT_EVENT_SUBSYSTEM_EN_MASK;

    value |= (spi_cfg->flags & BITSERIAL_SPI_DATA_IN_INVERT) ?
            HAL_BITSERIAL_CONFIG_DIN_INVERT_MASK : 0;

    value |= (spi_cfg->flags & BITSERIAL_SPI_DATA_OUT_INVERT) ?
            HAL_BITSERIAL_CONFIG_DOUT_INVERT_MASK : 0;

    value |= (spi_cfg->flags & BITSERIAL_SPI_BYTE_SWAP) ?
            HAL_BITSERIAL_CONFIG_BYTESWAP_EN_MASK : 0;

    value |= (spi_cfg->flags & BITSERIAL_SPI_BIT_REVERSE) ?
            HAL_BITSERIAL_CONFIG_BITREVERSE_EN_MASK : 0;

    if (spi_cfg->select_time_offset)
    {
        value |= HAL_BITSERIAL_CONFIG_SEL_TIME_EN_MASK;

        /* BITSERIAL_CONFIG_SEL_TIME & BITSERIAL_CONFIG_SEL_TIME2 */
        hal_bitserial_sel_time_set(i, (uint16)(spi_cfg->select_time_offset - 1));
    }

    hal_bitserial_config_set(i, value);

    /* Set BITSERIAL_CONFIG2 */
    value = 0;

    value |= (spi_cfg->sel_enabled) ? HAL_BITSERIAL_CONFIG2_SEL_EN2_MASK : 0;

    switch (BITSERIAL_SPI_DOUT_IDLE_GET(spi_cfg->flags))
    {
    case BITSERIAL_SPI_DOUT_IDLE_0:
        value |= (uint16)(HAL_BITSERIAL_CONFIG2_DOUT_IDLE_SEL_LOW <<
                                  HAL_BITSERIAL_CONFIG2_DOUT_IDLE_SEL_LSB_POSN);
        break;
    case BITSERIAL_SPI_DOUT_IDLE_1:
        value |= (uint16)(HAL_BITSERIAL_CONFIG2_DOUT_IDLE_SEL_HIGH <<
                                  HAL_BITSERIAL_CONFIG2_DOUT_IDLE_SEL_LSB_POSN);
        break;
    case BITSERIAL_SPI_DOUT_IDLE_LAST:
        value |= (uint16)(HAL_BITSERIAL_CONFIG2_DOUT_IDLE_SEL_LAST <<
                                  HAL_BITSERIAL_CONFIG2_DOUT_IDLE_SEL_LSB_POSN);
        break;
    default:
        assert(0);
    }

    hal_bitserial_config2_set(i, value);

    /* BITSERIAL_WORD_CONFIG */
    value = 0;

    switch (BITSERIAL_SPI_WORD_BYTES_GET(spi_cfg->flags))
    {
    case BITSERIAL_SPI_WORD_BYTES_1:
        value |= HAL_BITSERIAL_WORD_CONFIG_NUM_BYTES_ONE;
        break;
    case BITSERIAL_SPI_WORD_BYTES_2:
        value |= HAL_BITSERIAL_WORD_CONFIG_NUM_BYTES_TWO;
        break;
    case BITSERIAL_SPI_WORD_BYTES_3:
        value |= HAL_BITSERIAL_WORD_CONFIG_NUM_BYTES_THREE;
        break;
    case BITSERIAL_SPI_WORD_BYTES_4:
        value |= HAL_BITSERIAL_WORD_CONFIG_NUM_BYTES_FOUR;
        break;
    default:
        assert(0);
    }

    hal_bitserial_word_config_set(i, value);

    /* Check that the field reserved for the deprecated "interword_spacing"
     * is cleared */
    if (spi_cfg->reserved != 0)
    {
        panic(PANIC_BITSERIAL_CONFIGURATION_NOT_SUPPORTED);
    }

    if (is_slave)
    {
        hal_bitserial_slave_underflow_byte_set(i, SLAVE_UNDERFLOW_BYTE);
        hal_bitserial_config2_set(i, (uint16)(hal_bitserial_config2_get(i) |
                               HAL_BITSERIAL_CONFIG2_SLAVE_MODE_MASK |
                  HAL_BITSERIAL_CONFIG2_DATA_READY_WORD_DISABLE_MASK |
               (HAL_BITSERIAL_CONFIG2_SLAVE_READ_MODE_SWITCH_MANUAL <<
              HAL_BITSERIAL_CONFIG2_SLAVE_READ_MODE_SWITCH_LSB_POSN)));
    }
}


/******************************************************************************
 *
 *  bitserial_configure_i2c
 *
 *  Configure the hardware for I2C operations
 */
static void bitserial_configure_i2c(bitserial_instance i,
                                    const bitserial_config *vm_config,
                                    bool is_slave)
{
    const bitserial_i2c_config *i2c_cfg = &vm_config->u.i2c_cfg;
    uint16 value;

    /* Set BITSERIAL_CONFIG */
    value = HAL_BITSERIAL_CONFIG_I2C_MODE_EN_MASK
          | HAL_BITSERIAL_CONFIG_CLK_OFFSET_EN_MASK /* I2C always has clk offset. */
          | HAL_BITSERIAL_CONFIG_INT_EVENT_SUBSYSTEM_EN_MASK;

    hal_bitserial_config_set(i, value);

    /* Set BITSERIAL_CONFIG2 */
    value = 0;

    value |= HAL_BITSERIAL_CONFIG2_DOUT_IDLE_SEL_HIGH <<
                                   HAL_BITSERIAL_CONFIG2_DOUT_IDLE_SEL_LSB_POSN;
    hal_bitserial_config2_set(i, value);

    /* Set BITSERIAL0_CONFIG3 */
    value = 0;

    switch (BITSERIAL_I2C_ACT_ON_NAK_GET(i2c_cfg->flags))
    {
    case BITSERIAL_I2C_ACT_ON_NAK_CONTINUE:
        value |= (uint16)(HAL_BITSERIAL_CONFIG3_ACT_ON_NAK_LEGACY <<
                                     HAL_BITSERIAL_CONFIG3_ACT_ON_NAK_LSB_POSN);
        break;
    case BITSERIAL_I2C_ACT_ON_NAK_STOP:
        value |= (uint16)(HAL_BITSERIAL_CONFIG3_ACT_ON_NAK_STOP <<
                                     HAL_BITSERIAL_CONFIG3_ACT_ON_NAK_LSB_POSN);
        break;
    case BITSERIAL_I2C_ACT_ON_NAK_RESTART:
        value |= (uint16)(HAL_BITSERIAL_CONFIG3_ACT_ON_NAK_RESTART <<
                                     HAL_BITSERIAL_CONFIG3_ACT_ON_NAK_LSB_POSN);
        break;
    default:
        assert(0);
    }

    hal_bitserial_config3_set(i, value);

    /* BITSERIAL_INTERBYTE_SPACING->BITSERIAL_INTERBYTE_SPACING_EN */
    value = 0;
    value |= HAL_BITSERIAL_INTERBYTE_SPACING_EN_MASK;
    hal_bitserial_interbyte_spacing_set(i, value);

    hal_bitserial_word_config_set(i, HAL_BITSERIAL_WORD_CONFIG_NUM_BYTES_ONE);

    /* Set I2C address */
    hal_bitserial_i2c_address_set(i, i2c_cfg->i2c_address);

    if (is_slave)
    {
        hal_bitserial_slave_underflow_byte_set(i, SLAVE_UNDERFLOW_BYTE);
        hal_bitserial_config2_set(i, (uint16)(hal_bitserial_config2_get(i) |
                               HAL_BITSERIAL_CONFIG2_SLAVE_MODE_MASK |
                  HAL_BITSERIAL_CONFIG2_DATA_READY_WORD_DISABLE_MASK |
               (HAL_BITSERIAL_CONFIG2_SLAVE_READ_MODE_SWITCH_MANUAL <<
              HAL_BITSERIAL_CONFIG2_SLAVE_READ_MODE_SWITCH_LSB_POSN)));
    }
}


/******************************************************************************
 *
 *  bitserial_config_clock_freq_set
 *
 *  Validate the requested bitserial clock and set the HW registers.
 */
void bitserial_config_clock_freq_set(
    bitserial_instance i,
    bitserial_mode bs_mode,
    uint32 config_clock_frequency_khz,
    bool freq_change, /* Is this just a frequency change. */
    uint32 *byte_time_ns
)
{
    uint16 prev_clock_period_hi;
    uint16 prev_clock_period_lo;
    uint32 clock_period_hi;
    uint32 clock_period_lo;
    uint32 clock_speed_khz;
    uint32 clock_periods;
    uint32 actual_clock;

    /* Set clock high & low periods */
    clock_speed_khz = HAL_BITSERIAL_HOST_SS_FREQ_KHZ /
                      hal_get_host_sys_build_options_clk_80m_div_min();

    /* Can't be faster than half the blocks clock rate. */ 
    if (config_clock_frequency_khz > clock_speed_khz / 2)
    {
        config_clock_frequency_khz = clock_speed_khz / 2;
    }
    
    /* EC-1403/6/7 Need to restrict clock due to timing concerns. */
    if (config_clock_frequency_khz > 8000)
    {
        config_clock_frequency_khz = 8000;
    }

    /* I2C Master has stricter limits. */
    if (bs_mode == BITSERIAL_MODE_I2C_MASTER)
    {
        if (config_clock_frequency_khz > 1000)
        {
            config_clock_frequency_khz = 1000;
        }
    }
    /* Round clock_periods up to next integer so that frequency is not higher
     * than requested.
     */
    clock_periods = (clock_speed_khz + config_clock_frequency_khz - 1) / config_clock_frequency_khz;
    /* Round clock_periods up to next even number.
     * At low frequencies (high number of periods) this is OK.
     * At high freqencies for SPI (low number of periods) the HI and LO cycles
     * have the same number to make the timing correct in all SPI modes.
     */
    clock_periods += clock_periods % 2;
    /* Calculate LO periods.  We may have to adjust it. */
    clock_period_lo = clock_periods / 2;
    if (bs_mode == BITSERIAL_MODE_I2C_MASTER)
    {
        uint32 clock_period_lo_min;
        /* Make LO time less than HI time because it will be increased by
         * rise time through the pull-up.
         * The serialiser waits until the clock goes high before timing the HI
         * duration, in case the slave has implemented clock stretching.  It
         * does not know if the delay from LO to HI is due to the circuit
         * characteristics or clock stretching.
         */
        clock_period_lo--;
        /* Calculate min number of LO clock periods. */
        if (config_clock_frequency_khz <= 400)
        {
            /* lo_min = 1300ns */
            clock_period_lo_min = (clock_speed_khz * 1300 + 500000) / 1000000;
        }
        else
        {
            /* lo_min = 500ns */
            clock_period_lo_min = (clock_speed_khz * 500 + 500000) / 1000000;
        }
        if (clock_period_lo < clock_period_lo_min)
        {
            clock_period_lo = clock_period_lo_min;
        }
    }
    /* Calculate hi clock periods. */
    clock_period_hi = clock_periods - clock_period_lo;

    if (bs_mode == BITSERIAL_MODE_I2C_MASTER)
    {
        /* Set clock sample offset - I2C requires offset of 1/4 serial clock period  */
        hal_bitserial_clk_samp_offset_set(i, (uint16)(clock_period_lo / 2 - 1));
        L4_DBG_MSG2("I2C clk_samp_offset 0x%02X %3d", i, clock_period_lo / 2 - 1);
    }
    else
    {
        if (freq_change && (hal_bitserial_config_get(i) | HAL_BITSERIAL_CONFIG_CLK_OFFSET_EN_MASK))
        {
            uint16 clock_sample_offset = (uint16)(hal_bitserial_clk_samp_offset_get(i) + 1);

            /* SPI frequency is being changed so need to adjust clock offset as well. */
            hal_bitserial_clk_rate_get(i, &prev_clock_period_hi, &prev_clock_period_lo);
            prev_clock_period_lo++; /* Stored value is one less than actual. */

            /* We don't have the original config value, so just alter by ratio. */
            clock_sample_offset = (uint16)(clock_sample_offset * clock_period_lo / prev_clock_period_lo);
            if (clock_sample_offset >= clock_period_lo)
            {
                if (clock_period_lo > 0)
                {
                    /* Offset must be < clock_period_lo. */
                    clock_sample_offset = (uint16)(clock_period_lo - 1);
                }
                else
                {
                    clock_sample_offset = 0;
                }
            }
            if (clock_sample_offset > 0)
            {
                hal_bitserial_clk_samp_offset_set(i, (uint16)(clock_sample_offset - 1));
                L4_DBG_MSG2("SPI clk_samp_offset 0x%02X %3d", i, clock_sample_offset - 1);
            }
            else
            {
                /* Clear the offset flag. */
                uint16 bs_config = hal_bitserial_config_get(i) & ~HAL_BITSERIAL_CONFIG_CLK_OFFSET_EN_MASK;
                hal_bitserial_config_set(i, bs_config);
                L4_DBG_MSG2("SPI 0x%02X config = 0x%04X", i, bs_config);
            }
        }
    }
    actual_clock = clock_speed_khz / clock_periods;
    *byte_time_ns = 9 * 1000000 / actual_clock;
    L4_DBG_MSG1("byte_time_ns = %3d", *byte_time_ns);

    /* Clock registers are one less than required, so subtract 1. */
    clock_period_hi--;
    clock_period_lo--;

    L4_DBG_MSG3(
        "bitserial_configure: HOST f = %d, clk_80m_div_min = %d, host clk = %d",
        HAL_BITSERIAL_HOST_SS_FREQ_KHZ,
        hal_get_host_sys_build_options_clk_80m_div_min(),
        clock_speed_khz
    );
    L4_DBG_MSG4(
        "bitserial_configure: config_clk = %d, hi = %d, lo = %d, actual_clk = %d",
        config_clock_frequency_khz,
        clock_period_hi,
        clock_period_lo,
        actual_clock
    );

    hal_bitserial_clk_rate_set(i, (uint16)clock_period_hi, (uint16)clock_period_lo);
}


/******************************************************************************
 *
 *  bitserial_configure
 *
 *  Entry point for configuring the bitserial hardware from bitserial_open
 */
void bitserial_configure(
    bitserial_instance i,
    const bitserial_config *vm_config,
    uint32 *byte_time_ns
)
{
    bitserial_mode bs_mode = vm_config->mode;
    
    /* Enable bitserial */
    hal_bitserial_enable_set(i, TRUE);

    bitserial_config_clock_freq_set(
        i,
        bs_mode,
        vm_config->clock_frequency_khz,
        FALSE, /* Not just a frequency change. */
        byte_time_ns
    );

    switch (bs_mode)
    {
    case BITSERIAL_MODE_SPI_MASTER:
        bitserial_configure_spi(i, vm_config, FALSE /* is_slave */);
        break;

    case BITSERIAL_MODE_I2C_MASTER:
        bitserial_configure_i2c(i, vm_config, FALSE /* is_slave */);
        break;

    default:
        assert(0);
    }

    hal_bitserial_clk_force_enable(i, FALSE);
}


/******************************************************************************
 *
 *  bitserial_configure_pio
 *
 */
void bitserial_configure_pio(bitserial_block_index blk_idx,
                             bitserial_signal signal,
                             uint8 pio_index)
{
    bitserial_instance i = BITSERIAL_BLOCK_INDEX_TO_INSTANCE(blk_idx);

    /* This function must only be called from P0 */
    if (BITSERIAL_HANDLE_ON_P1(BITSERIAL_BLOCK_INDEX_TO_HANDLE(blk_idx)))
    {
        panic(PANIC_BITSERIAL_HAL_ERROR);
    }

    switch(signal)
    {
        case BITSERIAL_CLOCK_IN:
            hal_bitserial_clk_input_pio_set(i, pio_index);
            break;
        case BITSERIAL_CLOCK_OUT:
            hal_bitserial_clk_output_pio_set(i, pio_index);
            break;
        case BITSERIAL_DATA_IN:
            hal_bitserial_data_input_pio_set(i, pio_index);
            break;
        case BITSERIAL_DATA_OUT:
            hal_bitserial_data_output_pio_set(i, pio_index);
            break;
        case BITSERIAL_SEL_IN:
            hal_bitserial_sel_input_pio_set(i, pio_index);
            break;
        case BITSERIAL_SEL_OUT:
            hal_bitserial_sel_output_pio_set(i, pio_index);
            break;
        default:
            assert(0);
    }
}

#endif /* INSTALL_BITSERIAL */

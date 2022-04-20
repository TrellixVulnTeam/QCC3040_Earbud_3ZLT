/*!
\copyright  Copyright (c) 2019 - 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       txcpa224.c
\brief      Support for the txcpa224 proximity sensor
*/

#ifdef INCLUDE_PROXIMITY
#ifdef HAVE_TXCPA224
#include <bitserial_api.h>
#include <panic.h>
#include <pio.h>
#include <pio_monitor.h>
#include <pio_common.h>
#include <stdlib.h>

#include "adk_log.h"
#include "proximity.h"
#include "proximity_config.h"
#include "txcpa224.h"
#include "touch.h"

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_ENUM(proximity_messages)


const struct __proximity_config proximity_config = {
    .threshold_low = TXCPA224_PS_OFFSET_MIN,
    .threshold_high = TXCPA224_PS_OFFSET_MAX,
    .threshold_counts = 0,
    .rate = 0,
    .i2c_clock_khz = 100,
    .pios = {
        /* The PROXIMITY_PIO definitions are defined in the platform x2p file */
        .on = RDP_PIO_ON_PROX,
        .i2c_scl = RDP_PIO_I2C_SCL,
        .i2c_sda = RDP_PIO_I2C_SDA,
        .interrupt = RDP_PIO_INT_PROX,
    },
};

/*!< Task information for proximity sensor */
proximityTaskData app_proximity;

/*! \brief Read a register from the proximity sensor */
static bool txcpa224_ReadRegister(uint8 reg,  uint8 *value)
{
    bitserial_result result;
    bitserial_handle handle;

    bitserial_config bsconfig;
    proximityTaskData *prox = ProximityGetTaskData();

    /* Configure Bitserial to work with txcpa224 proximity sensor */
    memset(&bsconfig, 0, sizeof(bsconfig));
    bsconfig.mode = BITSERIAL_MODE_I2C_MASTER;
    bsconfig.clock_frequency_khz = prox->config->i2c_clock_khz;
    bsconfig.u.i2c_cfg.i2c_address = TXCPA224_I2C_ADDRESS;
    handle = BitserialOpen((bitserial_block_index)BITSERIAL_BLOCK_1, &bsconfig);

    result = BitserialWrite(handle,
                            BITSERIAL_NO_MSG,
                            &reg, 1,
                            BITSERIAL_FLAG_BLOCK);
    if (result == BITSERIAL_RESULT_SUCCESS)
    {
        /* Now read the actual data in the register */
        result = BitserialRead(handle,
                                BITSERIAL_NO_MSG,
                                value, 1,
                                BITSERIAL_FLAG_BLOCK);
    }

    BitserialClose(handle);

    return (result == BITSERIAL_RESULT_SUCCESS);
}

/*! \brief Write to a proximity sensor register */
static bool txcpa224_WriteRegister(uint8 reg, uint8 value)
{
    bitserial_result result;
    bitserial_handle handle;

    uint8 command[2] = {reg, value};
    bitserial_config bsconfig;
    proximityTaskData *prox = ProximityGetTaskData();

    /* Configure Bitserial to work with txcpa224 proximity sensor */
    memset(&bsconfig, 0, sizeof(bsconfig));
    bsconfig.mode = BITSERIAL_MODE_I2C_MASTER;
    bsconfig.clock_frequency_khz = prox->config->i2c_clock_khz;
    bsconfig.u.i2c_cfg.i2c_address = TXCPA224_I2C_ADDRESS;
    handle = BitserialOpen((bitserial_block_index)BITSERIAL_BLOCK_1, &bsconfig);

    /* Write the write command and register */
    result = BitserialWrite(handle,
                            BITSERIAL_NO_MSG,
                            command, 2,
                            BITSERIAL_FLAG_BLOCK);

    BitserialClose(handle);

    return (result == BITSERIAL_RESULT_SUCCESS);
}


static bool txcpa224_ReadInterruptStatus(uint8 *reg)
{
    return txcpa224_ReadRegister(TXCPA224_CFG2_REG, reg);
}

/*! \brief  Retrieve proximity readings */
static bool txcpa224_PSReading(uint8 *value)
{
    return txcpa224_ReadRegister(TXCPA224_PDH_REG, value);

}

/*! \brief Enable proximity readings */
static bool txcpa224_EnableProximity(uint8 enable)
{
    bool ret = FALSE;
    uint8 value = 0;
    value |= (enable<<1);
    if(txcpa224_WriteRegister(TXCPA224_CFG0_REG, value))
    {
        ret = txcpa224_ReadRegister(TXCPA224_CFG0_REG, &value);
    }
    return ret;
}

/*! \brief Read the proximity sensor device ID */
static bool txcpa224_ReadingDeviceID(uint8 *data)
{
    return txcpa224_ReadRegister(TXCPA224_CHIP_ID_REG, data);
}

static bool txcpa224_SetCfg3Reg(uint8 type, uint8 sleep_cycle)
{
    bool ret = FALSE;
    uint8 value = 0;
    value |= (type << 6);
    value |= (sleep_cycle << 3);

    if(txcpa224_WriteRegister(TXCPA224_CFG3_REG, value))
    {
        ret = txcpa224_ReadRegister(TXCPA224_CFG3_REG, &value);
    }
    return ret;
}

static bool txcpa224_SetCfg1Reg(uint8 lesc, uint8 psprst)
{
    bool ret = FALSE;
    uint8 value = 0;
    
    value |= (lesc << 4);
    value |= (psprst << 2);
    if( txcpa224_WriteRegister(TXCPA224_CFG1_REG, value))
    {
        ret = txcpa224_ReadRegister(TXCPA224_CFG1_REG, &value);
    }
    return ret;
}

static bool txcpa224_SetCfg2Reg(uint8 ps_mode, uint8 intsrc)
{
    uint8 value = 0;
    bool ret = FALSE;

    value |= (ps_mode << 6);
    value |= (intsrc << 2);
    if(txcpa224_WriteRegister(TXCPA224_CFG2_REG, value))
    {
        ret = txcpa224_ReadRegister(TXCPA224_CFG2_REG, &value);
    }
    return ret;
}

static bool txcpa224_SetHighThreshold(uint8 threshold)
{
    bool ret = FALSE;
    uint8 value;

    if( txcpa224_WriteRegister(TXCPA224_PTH_REG, threshold))
    {
        ret = txcpa224_ReadRegister(TXCPA224_PTH_REG, &value);
    }
    return ret;
}

static bool txcpa224_SetLowThreshold(uint8 threshold)
{
    bool ret = FALSE;
    uint8 value;
    if( txcpa224_WriteRegister(TXCPA224_PTL_REG, threshold))
    {
        ret = txcpa224_ReadRegister(TXCPA224_PTL_REG, &value);
    }
    return ret;
}

static bool txcpa224_SetPSOffset(uint8 offset)
{
    bool ret = FALSE;
    uint8 value;
    if( txcpa224_WriteRegister(TXCPA224_POFS1_REG, offset))
    {
        ret = txcpa224_ReadRegister(TXCPA224_POFS1_REG, &value);
    }
    return ret;
}
static bool txcpa224_SetCfg4Reg(uint8 filter_coef, uint8 light_pulse)
{
    bool ret = FALSE;
    uint8 value = 0x0C;
    value |= (filter_coef << 4);
    value |= (light_pulse << 1);
    if( txcpa224_WriteRegister(TXCPA224_CFG4_REG, value))
    {
        ret = txcpa224_ReadRegister(TXCPA224_CFG4_REG, &value);
    }
    return ret;
}

static bool txcpa224_SetPOFS2PSReg(void)
{

    bool ret = FALSE;
    uint8 value;
    if( txcpa224_WriteRegister(TXCPA224_POFS2_REG, 0x82))
    {
        ret = txcpa224_ReadRegister(TXCPA224_POFS2_REG, &value);
    }
    return ret;
}

static void txcpa224_serviceProximityEvent(proximityTaskData *proximity)
{
    uint8 isr;
    uint8 ps_data;
    uint8 is_interrupted;
    PanicFalse(txcpa224_PSReading(&ps_data));
    PanicFalse(txcpa224_ReadInterruptStatus(&isr));
    DEBUG_LOG_V_VERBOSE("ps data %u isr %u", ps_data, isr);

    /* check if interrupt bit is set */
    is_interrupted = isr&0x02;
    if (is_interrupted)
    {
        if (ps_data >= proximity->config->threshold_high)
        {
            DEBUG_LOG_VERBOSE("txcpa224InterruptHandler in proximity");
            /* update thresholds so no more interrupt generated for the same state */
            PanicFalse(txcpa224_SetHighThreshold(TXCPA224_PS_OFFSET_FULL_RANGE));
            PanicFalse(txcpa224_SetLowThreshold(proximity->config->threshold_low));
            if (proximity->state->proximity != proximity_state_in_proximity)
            {
                proximity->state->proximity = proximity_state_in_proximity;
                /* Inform clients */
                TaskList_MessageSendId(proximity->clients, PROXIMITY_MESSAGE_IN_PROXIMITY);
            }
        }
        else if (ps_data <= proximity->config->threshold_low)
        {
            DEBUG_LOG_VERBOSE("txcpa224InterruptHandler not in proximity");
            // update thresholds so no more interrupt generated for the same state
            PanicFalse(txcpa224_SetLowThreshold(TXCPA224_PS_OFFSET_LOWEST));
            PanicFalse(txcpa224_SetHighThreshold(proximity->config->threshold_high));
            if (proximity->state->proximity != proximity_state_not_in_proximity)
            {
                proximity->state->proximity = proximity_state_not_in_proximity;
                /* Inform clients */
                TaskList_MessageSendId(proximity->clients, PROXIMITY_MESSAGE_NOT_IN_PROXIMITY);
            }
        }
    }

    /* clear interrupt flag */
    isr &= 0xFD;
    PanicFalse(txcpa224_WriteRegister(TXCPA224_CFG2_REG, isr));
}

/*! \brief Handle the proximity interrupt */
static void txcpa224_InterruptHandler(Task task, MessageId id, Message msg)
{
    proximityTaskData *proximity = (proximityTaskData *) task;
    switch(id)
    {
        case MESSAGE_PIO_CHANGED:
            {
                const MessagePioChanged *mpc = (const MessagePioChanged *)msg;
                const proximityConfig *config = proximity->config;
                bool pio_set;

                if (PioMonitorIsPioInMessage(mpc, config->pios.interrupt, &pio_set))
                {
                    DEBUG_LOG_V_VERBOSE("txcpa224_InterruptHandler. pio state %u", pio_set);

                    if (!pio_set)
                    {
                        txcpa224_serviceProximityEvent(proximity);
                    }
                }
            }
            break;

        case PROXIMITY_INTERNAL_KEEP_ALIVE_TIMER:
            /* work around for interrupt service not served */
            txcpa224_serviceProximityEvent(proximity);
            MessageSendLater(&proximity->task, PROXIMITY_INTERNAL_KEEP_ALIVE_TIMER, NULL, D_SEC(1));
            break;

        default:
            break;
    }
}

/*! \brief Enable the proximity sensor */
static void txcpa224_Enable(const proximityConfig *config)
{
    uint16 bank;
    uint32 mask;
    uint32 i;

    struct
    {
        uint16 pio;
        pin_function_id func;
    } i2c_pios[] = {{config->pios.i2c_scl, BITSERIAL_1_CLOCK_OUT},
                    {config->pios.i2c_scl, BITSERIAL_1_CLOCK_IN},
                    {config->pios.i2c_sda, BITSERIAL_1_DATA_OUT},
                    {config->pios.i2c_sda, BITSERIAL_1_DATA_IN}};

    if (config->pios.on != TXCPA224_ON_PIO_UNUSED)
    {
        /* Setup power PIO then power-on the sensor */
        bank = PioCommonPioBank(config->pios.on);
        mask = PioCommonPioMask(config->pios.on);
        PanicNotZero(PioSetMapPins32Bank(bank, mask, mask));
        PanicNotZero(PioSetDir32Bank(bank, mask, mask));
        PanicNotZero(PioSet32Bank(bank, mask, mask));
    }

    /* Setup Interrupt as input with weak pull up */
    bank = PioCommonPioBank(config->pios.interrupt);
    mask = PioCommonPioMask(config->pios.interrupt);
    PanicNotZero(PioSetMapPins32Bank(bank, mask, mask));
    PanicNotZero(PioSetDir32Bank(bank, mask, 0));
    PanicNotZero(PioSet32Bank(bank, mask, mask));
    for (i = 0; i < ARRAY_DIM(i2c_pios); i++)
    {
        uint16 pio = i2c_pios[i].pio;
        bank = PioCommonPioBank(pio);
        mask = PioCommonPioMask(pio);

        /* Setup I2C PIOs with strong pull-up */
        PanicNotZero(PioSetMapPins32Bank(bank, mask, 0));
        PanicFalse(PioSetFunction(pio, i2c_pios[i].func));
        PanicNotZero(PioSetDir32Bank(bank, mask, 0));
        PanicNotZero(PioSet32Bank(bank, mask, mask));
        PanicNotZero(PioSetStrongBias32Bank(bank, mask, mask));
    }
}

/*! \brief Disable the proximity sensor txcpa224Disable*/
static void txcpa224_Disable(const proximityConfig *config)
{
    uint16 bank;
    uint32 mask;
    DEBUG_LOG_INFO("txcpa224_Disable");

    /* Disable interrupt and set weak pull down */
    bank = PioCommonPioBank(config->pios.interrupt);
    mask = PioCommonPioMask(config->pios.interrupt);
    PanicNotZero(PioSet32Bank(bank, mask, 0));

    if (config->pios.on != TXCPA224_ON_PIO_UNUSED)
    {
        /* Power off the proximity sensor */
        PanicNotZero(PioSet32Bank(PioCommonPioBank(config->pios.on),
                                  PioCommonPioMask(config->pios.on),
                                  0));
    }
}

static void txcpa224_sensorInitialization(proximityTaskData *prox)
{
    uint8 first_data = 0xFF;

    /* config the registers */
    PanicFalse(txcpa224_SetCfg3Reg(TXCPA224_PS_ISR_WINDOW_TYPE, TXCPA224_PS_SLEEP_PERIOD_5000));
    PanicFalse(txcpa224_SetPOFS2PSReg());
    PanicFalse(txcpa224_SetCfg4Reg(TXCPA224_FILTER_COEFF, TXCPA224_LIGHT_PULSE));
    PanicFalse(txcpa224_SetPSOffset(TXCPA224_PS_DEFAULT_CROSSTALK));

    /* set high/low to 255/0 */
    PanicFalse(txcpa224_SetHighThreshold(TXCPA224_PS_OFFSET_FULL_RANGE));
    PanicFalse(txcpa224_SetLowThreshold(TXCPA224_PS_OFFSET_LOWEST));

    PanicFalse(txcpa224_SetCfg1Reg(TXCPA224_PS_CURRENT_10MA, TXCPA224_PS_PRST_2PTS));
    PanicFalse(txcpa224_SetCfg2Reg(TXCPA224_PS_MODE_OFFSET, TXCPA224_PS_INT_SELECT_MODE));

    PanicFalse(txcpa224_EnableProximity(TXCPA224_PS_MODE_ENABLE));

    /* read first set of data */
    PanicFalse(txcpa224_PSReading(&first_data));
    DEBUG_LOG_INFO("First Sensor reading : %02x", first_data);
    if (first_data <= prox->config->threshold_low)
    {
        prox->state->proximity = proximity_state_not_in_proximity;
        /* update thresholds so no more interrupt generated for the same state */
        PanicFalse(txcpa224_SetLowThreshold(TXCPA224_PS_OFFSET_LOWEST));
        PanicFalse(txcpa224_SetHighThreshold(prox->config->threshold_high));
    
    }
    else
    {
        prox->state->proximity = proximity_state_in_proximity;
        /* update thresholds so no more interrupt generated for the same state */
        PanicFalse(txcpa224_SetHighThreshold(TXCPA224_PS_OFFSET_FULL_RANGE));
        PanicFalse(txcpa224_SetLowThreshold(prox->config->threshold_low));
    }

}
bool appProximityClientRegister(Task task)
{
    proximityTaskData *prox = ProximityGetTaskData();

    if (NULL == prox->clients)
    {
        uint8 first_data = 0xFF;
        const proximityConfig *config = appConfigProximity();
        prox->config = config;
        prox->state = PanicUnlessNew(proximityState);
        prox->state->proximity = proximity_state_unknown;
        prox->clients = TaskList_Create();

        txcpa224_Enable(config);
        PanicFalse(txcpa224_ReadingDeviceID(&first_data));
        DEBUG_LOG_VERBOSE("Sensor ID : %02x", first_data);

        txcpa224_sensorInitialization(prox);

        /* Register for interrupt events */
        prox->task.handler = txcpa224_InterruptHandler;
        PioMonitorRegisterTask(&prox->task, prox->config->pios.interrupt);
        /* Schedule a timer to check sensor alive */
        MessageSendLater(&prox->task, PROXIMITY_INTERNAL_KEEP_ALIVE_TIMER, NULL, D_SEC(1));
    }

    /* Send initial message to client */
    switch (prox->state->proximity)
    {
        case proximity_state_in_proximity:
            MessageSend(task, PROXIMITY_MESSAGE_IN_PROXIMITY, NULL);
            break;
        case proximity_state_not_in_proximity:
            MessageSend(task, PROXIMITY_MESSAGE_NOT_IN_PROXIMITY, NULL);
            break;
        case proximity_state_unknown:
        default:
            /* The client will be informed after the first interrupt */
            break;
    }
    DEBUG_LOG_VERBOSE("Proximity Client registered");

    return TaskList_AddTask(prox->clients, task);
}

void appProximityClientUnregister(Task task)
{
    proximityTaskData *prox = ProximityGetTaskData();
    TaskList_RemoveTask(prox->clients, task);
    if (0 == TaskList_Size(prox->clients))
    {
        TaskList_Destroy(prox->clients);
        prox->clients = NULL;
        free(prox->state);
        prox->state = NULL;

        MessageCancelAll(&prox->task, PROXIMITY_INTERNAL_KEEP_ALIVE_TIMER);
        /* Unregister for interrupt events */
        PioMonitorUnregisterTask(&prox->task, prox->config->pios.interrupt);

        /* Reset into lowest power mode in case the sensor is not powered off. */
        txcpa224_Disable(prox->config);
    }
}
/* This function is to switch on/off sensor for power saving*/
void appProximityEnableSensor(Task task, bool enable)
{
    UNUSED(task);
    proximityTaskData *prox = ProximityGetTaskData();

    if (enable)
    {
        txcpa224_sensorInitialization(prox);
    }
    else
    {
        /* Switching off PS sensing, Extend Sensor sleeping to max
           Ideally we want to switch off the supply line (3V_LDO)
           but current design if we switch this off, it'll switch off other peripherals
           hence only put sensor in power saving mode
        */
        /* set high/low to 255/0 */
        PanicFalse(txcpa224_SetHighThreshold(TXCPA224_PS_OFFSET_FULL_RANGE));
        PanicFalse(txcpa224_SetLowThreshold(TXCPA224_PS_OFFSET_LOWEST));
        /* extend sensor sleep time to max */
        PanicFalse(txcpa224_SetCfg3Reg(TXCPA224_PS_ISR_WINDOW_TYPE, TXCPA224_PS_SLEEP_PERIOD_5000));
        /* disable PS */
        PanicFalse(txcpa224_EnableProximity(TXCPA224_PS_MODE_DISABLE));

    }
}

#endif /* HAVE_TXCPA224 */
#endif /* INCLUDE_PROXIMITY */

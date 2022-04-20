/* Copyright (c) 2016 - 2021 Qualcomm Technologies International, Ltd. */
/*   %%version */
/**
 * \file 
 * Implementation of hydra bus interrupt subsystem.
 *
 * See bus_interrupt.h for more details.
 */

#include "bus_interrupt/bus_interrupt_private.h"

#if CHIP_HAS_BANKED_BUS_INTERRUPTS
struct {
    bool interrupt_configured;
    void (*handler[CHIP_NUMBER_OF_BUS_INTERRUPTS])(void);
} bus_int_data;
#else

/* Could turn this look up table into a calculation by assuming events
 * are contiguous */
static const int_source int_num_to_source_lut[] = {
        (int_source)INT_SOURCE_LOW_PRI_TBUS_INT_ADPTR_EVENT_1,
        (int_source)INT_SOURCE_LOW_PRI_TBUS_INT_ADPTR_EVENT_2,
        (int_source)INT_SOURCE_LOW_PRI_TBUS_INT_ADPTR_EVENT_3,
        (int_source)INT_SOURCE_LOW_PRI_TBUS_INT_ADPTR_EVENT_4};
#endif /* CHIP_HAS_BANKED_BUS_INTERRUPTS */

void bus_interrupt_generate_user_int(system_bus src_id,
                                     system_bus dest_id,
                                     int dest_block_id,
                                     uint16 int_status)
{
#if CHIP_HAS_BUS_INTERRUPT_GENERATE_HW
    UNUSED(src_id);
    HYDRA_POLL_WITH_TIMEOUT(
        ! hal_get_reg_bus_int_send_int_send_status(),
        BUS_INTERRUPT_HW_SEND_TIMEOUT_VALUE,
        PANIC_BUS_INTERRUPT_HW_SEND_TIMED_OUT);
    hal_set_bus_int_send_int_config_subsystem(dest_id);
    hal_set_bus_int_send_int_config_src_block_id(dest_block_id);
    hal_set_bus_int_send_int_config_dest_block_id(dest_block_id);
    hal_set_reg_bus_int_send_int_status_field(int_status);
#else /* CHIP_HAS_BUS_INTERRUPT_GENERATE_HW */
    hydra_trb_trx interrupt_trx;

    /* The transaction shape we want to send is:
     * Opcode = T_TRANSACTION_MAJOR_OPCODE_T_EXTENDED
     * Sub-system source ID = Who is doing the kicking
     * Sub-system block source ID = blockid serv provider asked to be kicked on
     * Sub-system destination ID = address of service provider Sub-system
     * block destination ID = blockid service provider asked to be kicked on
     * Tag = 0 Don't care value transaction isn't tracked.
     * Payload = Extd Opcode 4 bits = 0
     *               T_TRANSACTION_MINOR_OPCODE_T_INTERRUPT_EVENT
     *           Interrupt Status 16 bits = status chosen by provider to
     *               indicate shunt instance
     *           Unused 52 bits
     *
     * N.B. Sub-system source ID and destination ID are set the same. Sending of
     * these is mutually exclusive so we set both to be the same to make sure
     * Destination sees the value we want it to.
     */
    hydra_trb_trx_header_init(&interrupt_trx,
            (uint16)T_TRANSACTION_MAJOR_OPCODE_T_EXTENDED,
            src_id,
            dest_block_id,
            dest_id,
            dest_block_id,
            0,
            ((T_TRANSACTION_MINOR_OPCODE_T_INTERRUPT_EVENT << 4) |
                    ((int_status >> 12) & 0xf)));

    interrupt_trx.data[2] = ((int_status << 4) & 0xFFF0);

    bus_message_blocking_transmit_arbitrary_transaction(&interrupt_trx);

#endif /* CHIP_HAS_BUS_INTERRUPT_GENERATE_HW */
}

#if CHIP_HAS_BANKED_BUS_INTERRUPTS
static void bus_interrupt_isr(void)
{
    uint16f i;
    uint32 bus_int_status;
    bus_int_status = hal_get_reg_apps_banked_tbus_int_p1_status();
    hal_set_reg_apps_banked_tbus_int_p1_status(bus_int_status);
    for(i=0; i<CHIP_NUMBER_OF_BUS_INTERRUPTS; ++i)
    {
        if((bus_int_status & (1<<i)) && bus_int_data.handler[i])
        {
            bus_int_data.handler[i]();
        }
    }
}
#endif

void bus_interrupt_configure(bus_interrupt_number int_num,
                             const bus_interrupt_configuration* config)
{
    assert(int_num < CHIP_NUMBER_OF_BUS_INTERRUPTS);

    /* Block interrupts to avoid race with interrupt handlers */
    block_interrupts();

    /* Set the banking register to select the interrupt number */
    hal_set_reg_bus_int_select(int_num);
    
    /* Configure the interrupt - which other subsystem and block or we
     * listening to */
    hal_set_bus_int_config_subsystem_config(config->subsystem_id);
    hal_set_bus_int_config_block_config(config->block_id);
    hal_set_bus_int_config_enable_config(config->enable);
    hal_set_bus_int_config_status_clear_on_read_config(config->clear_on_read);

    /* Which events from that subsystem are we interested in */
    hal_set_reg_bus_int_mask(config->interrupt_mask);

    unblock_interrupts();

#if CHIP_HAS_BANKED_BUS_INTERRUPTS
    hal_set_reg_apps_banked_tbus_int_p1_enables(
            hal_get_reg_apps_banked_tbus_int_p1_enables() | (1<<int_num));

    bus_int_data.handler[int_num] = config->handler;

    /* With all bus interrupts sharing a single processor interrupt, they
     * all have to run at the same priority. */
    assert(config->level == INT_LEVEL_FG);

    if(!bus_int_data.interrupt_configured)
    {
        configure_interrupt((int_source)INT_SOURCE_TBUS_INT_ADPTR_EVENT,
                                        INT_LEVEL_FG, bus_interrupt_isr);
        bus_int_data.interrupt_configured = TRUE;
    }
#else
    /* Priority and handler */
    configure_interrupt(int_num_to_source_lut[int_num],
                        config->level,
                        config->handler);
#endif
}




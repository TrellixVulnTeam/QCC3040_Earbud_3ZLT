/* Copyright (c) 2016 - 2021 Qualcomm Technologies International, Ltd. */
/*   %%version */
/**
 * \file 
 * Hydra bus interrupt configuration public header file
 *
 * \defgroup bus_interrupt bus_interrupt
 * \ingroup core
 * \section bus_interrupt_h_usage USAGE
 * This is the public header file for the bus interrupt configuration module.
 * The principal expected client of this module is expected to be the host
 * transport code.
 */
/**
 * \defgroup bus_interrupt bus_interrupt
 * \ingroup core
 * \section bus_interrupt_intro INTRODUCTION
 *
 * A hydra subsystem (for example the Curator) can be configured to raise
 * interrupts to its processor(s) based on events generated on other
 * subsystems and sent over the transaction bus. The events contain
 * both a subsystem and a block_id field.
 *
 * The most obvious example sources of these events are the various
 * host interface blocks (UART, SDIO, USB, ...) within the hostif
 * subsystem or other subsystem firmware generating 'kicks'. At the
 * time of writing the XAP subsystems can
 * generate four unique bus interrupts, each based on a different
 * subsystem + block_id combination. This would allow a subsystem such as
 * the Curator to operate with four simultaneous host transports.
 * Other subsystems may have more bus interrupt sources in a banked set
 * of registers that share one or more processor interrupt lines. The
 * config file for the chip specifies the number supported as
 * \c CHIP_NUMBER_OF_BUS_INTERRUPTS and indicates the banked hardware with
 * \c CHIP_HAS_BANKED_BUS_INTERRUPTS
 */

#ifndef BUS_INTERRUPT_H
#define BUS_INTERRUPT_H

#include "hydra/hydra_types.h"
#include "int/int.h"
/*lint -e750 -e962*/ #define IO_DEFS_MODULE_BUS_INTERRUPT
/*lint -e750 -e962*/ #define IO_DEFS_MODULE_CHIP
#include "io/io.h"

/**
 * Enumerates valid bus interrupt numbers up to
 * \c CHIP_NUMBER_OF_BUS_INTERRUPTS
 */
typedef uint16 bus_interrupt_number;

/** Zero indexed bus interrupt number for BT Transport  */
#define BUS_INTERRUPT_ID_BT_TRANSPORT       (0)

/** Zero indexed bus interrupt number for WLAN transport  */
#define BUS_INTERRUPT_ID_WLAN_TRANSPORT     (1)

/** Zero indexed bus interrupt number for AUDIO transport  */
#define BUS_INTERRUPT_ID_AUDIO_TRANSPORT    (2)

/** Zero indexed bus interrupt number for USB hostif block  */
#define BUS_INTERRUPT_ID_USB                (3)

/** Zero indexed bus interrupt number for UART hostif block  */
#define BUS_INTERRUPT_ID_UART               (4)

/** Zero indexed bus interrupt number for PIO controller  */
#define BUS_INTERRUPT_ID_PIO_8051           (5)

/** Zero indexed bus interrupt number for bitserial0 hostif block  */
#define BUS_INTERRUPT_ID_BITSERIAL0         (6)

/** Zero indexed bus interrupt number for bitserial1 hostif block  */
#define BUS_INTERRUPT_ID_BITSERIAL1         (7)

/** Zero indexed bus interrupt number for bitserial2 hostif block  */
#define BUS_INTERRUPT_ID_BITSERIAL2         (8)

/** Zero indexed bus interrupt number for CSB Processing Service  */
#define BUS_INTERRUPT_ID_CSB                (9)

/**
 * The bus_interrupt_configuration structure
 */
typedef struct
{
    /**
     * The subsystem whose events we want to turn into interrupts
     */
    system_bus subsystem_id;
    /**
     * The block within that subsystem
     */
    uint16 block_id;
    /** Enable or disable events */
    bool enable;
    /** If true, reading bus_int_status register will clear events */
    bool clear_on_read;
    /** A mask indicating the interrupts we are interested in. Each block may 
     * generate interrupts for different events and we may only care about
     * some */
    uint16 interrupt_mask;
    /** Specifies the interrupt level */
    int_level level;
    /** The interrupt handler to call */
    void (*handler)(void);
}bus_interrupt_configuration;

/**
 * Raise a user interrupt on remote system
 * \param src_id The source subsystem
 * \param dest_id The destination subsystem
 * \param dest_block_id The block ID in the destination
 * \param int_status The bitmap of interrupt status to raise
 */
void bus_interrupt_generate_user_int(system_bus src_id,
                                     system_bus dest_id,
                                     int dest_block_id,
                                     uint16 int_status);

/**
 * Configure one of the bus interrupts
 *
 * \param int_num is the interrupt number to configure
 *
 * \param config is a structure containing the configuration information
 */
void bus_interrupt_configure(bus_interrupt_number int_num,
                             const bus_interrupt_configuration* config);

#endif /* BUS_INTERRUPT_H */

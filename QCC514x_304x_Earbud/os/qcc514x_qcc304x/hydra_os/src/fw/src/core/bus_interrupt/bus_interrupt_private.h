/* Copyright (c) 2016 - 2021 Qualcomm Technologies International, Ltd. */
/*   %%version */
/**
 * \file 
 * Private header file for hydra bus interrupt subsystem
 *
 * See bus_interrupt.h for more details.
 */

#ifndef BUS_INTERRUPT_PRIVATE_H
#define BUS_INTERRUPT_PRIVATE_H

/*lint -e750 -e962*/ #define IO_DEFS_MODULE_BUS_INTERRUPT
/*lint -e750 -e962*/ #define IO_DEFS_MODULE_APPS_BANKED_TBUS_INT_P1
/*lint -e750 -e962*/ #define IO_DEFS_MODULE_K32_INTERRUPT
/*lint -e750 -e962*/ #define IO_DEFS_MODULE_CHIP

#include "bus_interrupt/bus_interrupt.h"
#include "bus_message/bus_message.h"
#include "hal/hal_macros.h"
#include "io/io_map.h"
#include "hydra/hydra_trb.h"
#include "hydra/hydra_txbus.h"
#include "hal/haltime.h"
#include "bus_message/bus_message.h"
#include "panic/panic.h"
#include "assert.h"

#endif /* BUS_INTERRUPT_PRIVATE_H */

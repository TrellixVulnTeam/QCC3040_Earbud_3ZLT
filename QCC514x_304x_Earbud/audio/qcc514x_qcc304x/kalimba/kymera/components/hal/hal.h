/****************************************************************************
 * Copyright (c) 2011 - 2017 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 * \defgroup HAL Hardware Abstraction Layer
 * \file  hal.h
 *
 * Public header file for HAL functions.
 * Currently just initialisation
 * Likely to get split between functional areas later.
 * \ingroup HAL
 *
 */

#ifndef HAL_H
#define HAL_H

/****************************************************************************
Include Files
*/

#include "types.h"
#include "io_map.h"
#include "hal_macros.h"
#include "hal_time.h"
#include "hal_perfstats.h"
#include "hal_alias.h"

#ifdef CHIP_HAS_MPU_KEYHOLE
#include "hal_interproc_keyhole.h"
#endif
#if defined(SUPPORTS_MULTI_CORE)
#include "hal_multi_core.h"
#endif 
#if defined(INSTALL_EXTERNAL_MEM)
#include "hal_sram.h"
#endif 

/****************************************************************************
Public Type Declarations
*/

/****************************************************************************
Public Constant Declarations
*/

/****************************************************************************
Public Macro Declarations
*/

/****************************************************************************
Public Variable Definitions
*/

/****************************************************************************
Public Function Definitions
*/

/**
 * \brief  Initialise the hardware
 */
extern void hal_init(void);

#ifdef KAL_ARCH4
/**
 * \brief Initialise of PM RAM access
 */
extern void hal_init_initial_pm(void);
#endif

#ifdef INSTALL_CACHE
/**
 * \brief  Enable or disable PM cache
 */
extern void hal_cache_configure(bool enable);

/**
 * \brief  Flush caches
 */
extern void hal_cache_flush(void);

#endif /* INSTALL_CACHE */

#endif /* HAL_H */


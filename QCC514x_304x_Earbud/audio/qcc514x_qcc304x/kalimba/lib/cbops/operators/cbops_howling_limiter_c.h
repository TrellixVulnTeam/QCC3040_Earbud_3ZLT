/****************************************************************************
 * Copyright (c) 2020 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 * \file cbops_howling_limiter_c.h
 * \ingroup cbops
 *
 */

#ifndef _CBOPS_HOWLING_LIMITER_C_H_
#define _CBOPS_HOWLING_LIMITER_C_H_

#include "hl_limiter.h"

/****************************************************************************
Public Type Declarations
*/

/** Structure of the Howling Limiter cbops operator specific data */
typedef HL_LIMITER_DATA cbops_howling_limiter;

/** The address of the function vector table. This is aliased in ASM */
extern unsigned cbops_howling_limiter_table[];

#endif /* _CBOPS_HOWLING_LIMITER_C_H_ */

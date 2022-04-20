/****************************************************************************
 * Copyright (c) 2020 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 * \defgroup aec_reference
 * \file  hl_limiter_defs.h
 * \ingroup capabilities
 *
 *  header file for howling limiter defines used by c-code and assembly. <br>
 *
 */


#ifndef __HL_LIMITER_DEFS_H__
#define __HL_LIMITER_DEFS_H__

#define HL_PROCESS_ASM_OPTIMIZED
#define HL_INT32_MAX                    0x7fffffff 

#define HL_PROCESS_DECIMATION_BIT       4
#define HL_PROCESS_DECIMATION           (1<<HL_PROCESS_DECIMATION_BIT)
#define HL_PROCESS_DECIMATION_MASK      (HL_PROCESS_DECIMATION-1)
#define HL_GAIN_RAMP                    0x20000000                      // 0x20000000 = 1/4
#define HL_ENERGY_VAR_COEFFICIENT       0x33300000                      // 0x33300000 =  1.0/2.5  
#define HL_DETECT_RELEASE_TIME_MS       200                             // 200ms
#define HL_TC_ATTACK_MS                 2                               // 2 ms
#endif /* __HL_LIMITER_DEFS_H__ */

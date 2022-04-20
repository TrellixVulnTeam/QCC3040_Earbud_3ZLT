// -----------------------------------------------------------------------------
// Copyright (c) 2021                  Qualcomm Technologies International, Ltd.
//
#ifndef __GVA_GEN_ASM_H__
#define __GVA_GEN_ASM_H__

// CodeBase IDs
.CONST $M.GVA_GVA_CAP_ID       	0x00C5;
.CONST $M.GVA_GVA_ALT_CAP_ID_0       	0x409B;
.CONST $M.GVA_GVA_SAMPLE_RATE       	16000;
.CONST $M.GVA_GVA_VERSION_MAJOR       	1;

// Constant Values


// Piecewise Disables


// Statistic Block
.CONST $M.GVA.STATUS.CUR_MODE       		0*ADDR_PER_WORD;
.CONST $M.GVA.STATUS.OVR_CONTROL    		1*ADDR_PER_WORD;
.CONST $M.GVA.STATUS.GVA_DETECTED   		2*ADDR_PER_WORD;
.CONST $M.GVA.STATUS.GVA_KEYWORD_LEN		3*ADDR_PER_WORD;
.CONST $M.GVA.STATUS.BLOCK_SIZE          	4;

// System Mode
.CONST $M.GVA.SYSMODE.STATIC   		0;
.CONST $M.GVA.SYSMODE.FULL_PROC		1;
.CONST $M.GVA.SYSMODE.PASS_THRU		2;
.CONST $M.GVA.SYSMODE.MAX_MODES		3;

// System Control
.CONST $M.GVA.CONTROL.MODE_OVERRIDE		0x2000;


#endif // __GVA_GEN_ASM_H__

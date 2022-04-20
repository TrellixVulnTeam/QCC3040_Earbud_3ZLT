// -----------------------------------------------------------------------------
// Copyright (c) 2021                  Qualcomm Technologies International, Ltd.
//
#ifndef __APVA_GEN_ASM_H__
#define __APVA_GEN_ASM_H__

// CodeBase IDs
.CONST $M.APVA_APVA_CAP_ID       	0x00C6;
.CONST $M.APVA_APVA_ALT_CAP_ID_0       	0x409C;
.CONST $M.APVA_APVA_SAMPLE_RATE       	16000;
.CONST $M.APVA_APVA_VERSION_MAJOR       	1;

// Constant Values


// Piecewise Disables


// Statistic Block
.CONST $M.APVA.STATUS.CUR_MODE              		0*ADDR_PER_WORD;
.CONST $M.APVA.STATUS.OVR_CONTROL           		1*ADDR_PER_WORD;
.CONST $M.APVA.STATUS.APVA_DETECTED         		2*ADDR_PER_WORD;
.CONST $M.APVA.STATUS.APVA_RESULT_CONFIDENCE		3*ADDR_PER_WORD;
.CONST $M.APVA.STATUS.APVA_KEYWORD_LEN      		4*ADDR_PER_WORD;
.CONST $M.APVA.STATUS.BLOCK_SIZE                 	5;

// System Mode
.CONST $M.APVA.SYSMODE.STATIC   		0;
.CONST $M.APVA.SYSMODE.FULL_PROC		1;
.CONST $M.APVA.SYSMODE.PASS_THRU		2;
.CONST $M.APVA.SYSMODE.MAX_MODES		3;

// System Control
.CONST $M.APVA.CONTROL.MODE_OVERRIDE		0x2000;

// Parameter Block
.CONST $M.APVA.PARAMETERS.OFFSET_APVA_THRESHOLD		0*ADDR_PER_WORD;
.CONST $M.APVA.PARAMETERS.STRUCT_SIZE         		1;


#endif // __APVA_GEN_ASM_H__

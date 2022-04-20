// -----------------------------------------------------------------------------
// Copyright (c) 2021                  Qualcomm Technologies International, Ltd.
//
#include "aanc_lite_gen_c.h"

#ifndef __GNUC__ 
_Pragma("datasection CONST")
#endif /* __GNUC__ */

static unsigned defaults_aanc_liteAANC_LITE_16K[] = {
   0x00000000u,			// AANC_LITE_CONFIG
   0x00180000u			// GAIN_RAMP_TIMER
};

unsigned *AANC_LITE_GetDefaults(unsigned capid){
	switch(capid){
		case 0x00C9: return defaults_aanc_liteAANC_LITE_16K;
		case 0x40A1: return defaults_aanc_liteAANC_LITE_16K;
	}
	return((unsigned *)0);
}

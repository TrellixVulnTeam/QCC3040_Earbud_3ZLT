// -----------------------------------------------------------------------------
// Copyright (c) 2021                  Qualcomm Technologies International, Ltd.
//
#include "earbud_fit_test_gen_c.h"

#ifndef __GNUC__ 
_Pragma("datasection CONST")
#endif /* __GNUC__ */

static unsigned defaults_earbud_fit_testEARBUD_FIT_TEST_16K[] = {
   0x7EB851ECu,			// POWER_SMOOTH_FACTOR
   0x05000000u,			// FIT_THRESHOLD
   0x00300000u			// EVENT_GOOD_FIT
};

unsigned *EARBUD_FIT_TEST_GetDefaults(unsigned capid){
	switch(capid){
		case 0x00CA: return defaults_earbud_fit_testEARBUD_FIT_TEST_16K;
		case 0x40A2: return defaults_earbud_fit_testEARBUD_FIT_TEST_16K;
	}
	return((unsigned *)0);
}

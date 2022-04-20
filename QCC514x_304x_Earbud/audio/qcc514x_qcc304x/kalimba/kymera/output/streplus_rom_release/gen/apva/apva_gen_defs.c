// -----------------------------------------------------------------------------
// Copyright (c) 2021                  Qualcomm Technologies International, Ltd.
//
#include "apva_gen_c.h"

#ifndef __GNUC__ 
_Pragma("datasection CONST")
#endif /* __GNUC__ */

static unsigned defaults_apvaAPVA[] = {
   0x000001F4u			// APVA_THRESHOLD
};

unsigned *APVA_GetDefaults(unsigned capid){
	switch(capid){
		case 0x00C6: return defaults_apvaAPVA;
		case 0x409C: return defaults_apvaAPVA;
	}
	return((unsigned *)0);
}

// *****************************************************************************
// Copyright (c) 2005 - 2017 Qualcomm Technologies International, Ltd.        http://www.csr.com
// %%version
//
// $Change$  $DateTime$
// *****************************************************************************

#ifndef INCLUDED_KALSIM_H
#define INCLUDED_KALSIM_H

// Macros used to control debug functions in Kalsim
// These have no effect on real hardware

// TERMINATE
// Shuts down Kalsim
// Causes an infinite loop on a real chip
// Usage:  TERMINATE

#define TERMINATE                \
   kalcode(0x8D008000);          \
   terminate_loop:               \
   jump terminate_loop;

#endif // INCLUDED_KALSIM_H


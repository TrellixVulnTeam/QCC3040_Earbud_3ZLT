// Copyright (c) 2020 Qualcomm Technologies International, Ltd.
//   %%version
//!
//! \file udiv3216_opt.asm
//!  Assembler function called from C code
//!
//! \ingroup platform
//!


.MODULE $M._udiv3216;
   .CODESEGMENT PM;
   .DATASEGMENT DM;
   .MINIM;

//
// uint32 udiv3216(uint16 *remainder, uint16 denominator, uint32 numerator);
//

$_udiv3216:
    rMAC = r2 LSHIFT 0 (LO); // Kalimba divide is signed, so avoid 'sign extension'
    Div = rMAC / r1;
    Null = r0 + Null;
    if Z jump ud1;
    r1 = DivRemainder;
    MH[r0 + Null] = r1;
ud1:
    r0 = DivResult;
    rts;
  
.ENDMODULE;

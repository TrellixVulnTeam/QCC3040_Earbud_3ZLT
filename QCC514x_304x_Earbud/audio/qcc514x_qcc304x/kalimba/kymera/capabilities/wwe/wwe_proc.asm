#include "wwe_struct_asm_defs.h"
#include "stack.h"
#include "portability_macros.h"


// *****************************************************************************
// MODULE:
//    $M.my_cap_proc_func
//
// DESCRIPTION:
//    Define here your processing function
//
// INPUTS:
//   - Your input registers
//
// OUTPUTS:
//   - Your output registers
//
// TRASHED REGISTERS:
//    C calling convention respected.
//
// NOTES:
//
// *****************************************************************************
.MODULE $M.wwe_proc_func;

.CODESEGMENT PM;

$_cbuffer_copy_and_shift:
    PUSH_ALL_C

    //r0 has input buffer, r1 output buffer, r2 copy size
    r10 = r2;   // size
    r3 = r1;    // output buffer
    call $cbuffer.get_read_address_and_size_and_start_address;
    push r2;
    pop B0;     // start address
    I0 = r0;    // read address
    L0 = r1;    // cbuffer size
    r2 = r3;    // output buffer

    do copy_loop;
        r0 = M[I0, MK1];
        r0 = r0 ASHIFT -16;
        MH[r2] = r0;
        r2 = r2 + LOG2_ADDR_PER_WORD;
    copy_loop:

    push NULL;
    pop B0;
    L0 = NULL;
    POP_ALL_C
    rts;

.ENDMODULE;

/***************************************************************
 * Copyright (c) 2020 Qualcomm Technologies International, Ltd.
 **************************************************************/

/* These public C functions, are declared in cbops_howling_limiter.h, and
 * are resolved to absolute addresses exported from the patch build.
 */
#include "subsys3_patch0_fw00001BD0_map_public.h"

#ifdef DISABLE_PATCH_BUILD_ID_CHECK

.const $_create_howling_limiter_op              ENTRY_POINT_CREATE_HOWLING_LIMITER_OP_PATCH;
.const $_initialize_howling_limiter_op          ENTRY_POINT_INITIALIZE_HOWLING_LIMITER_OP_PATCH;
.const $_configure_howling_limiter_op           ENTRY_POINT_CONFIGURE_HOWLING_LIMITER_OP_PATCH;

#else

.MODULE $M.download_support_lib.create_howling_limiter_op;
.CODESEGMENT PM;
.MINIM;

$_create_howling_limiter_op:
#ifdef ENTRY_POINT_CREATE_HOWLING_LIMITER_OP_PATCH
    rMAC = M[$_patched_fw_version];
    Null = rMAC - PATCH_BUILD_ID;
    if EQ jump ENTRY_POINT_CREATE_HOWLING_LIMITER_OP_PATCH;
#endif
L_pb_mismatch:
    /* Stub: return (uintptr_t 0); */
    r0 = 0;
    rts;

.ENDMODULE;

.MODULE $M.download_support_lib.initialize_howling_limiter_op;
.CODESEGMENT PM;
.MINIM;

$_initialize_howling_limiter_op:
#ifdef ENTRY_POINT_INITIALIZE_HOWLING_LIMITER_OP_PATCH
    rMAC = M[$_patched_fw_version];
    Null = rMAC - PATCH_BUILD_ID;
    if EQ jump ENTRY_POINT_INITIALIZE_HOWLING_LIMITER_OP_PATCH;
#endif
L_pb_mismatch:
    /* Stub: return (uintptr_t 0); */
    r0 = 0;
    rts;

.ENDMODULE;

.MODULE $M.download_support_lib.configure_howling_limiter_op;
.CODESEGMENT PM;
.MINIM;

$_configure_howling_limiter_op:
#ifdef ENTRY_POINT_CONFIGURE_HOWLING_LIMITER_OP_PATCH
    rMAC = M[$_patched_fw_version];
    Null = rMAC - PATCH_BUILD_ID;
    if EQ jump ENTRY_POINT_CONFIGURE_HOWLING_LIMITER_OP_PATCH;
#endif
L_pb_mismatch:
    /* Stub: return (uintptr_t 0); */
    r0 = 0;
    rts;

.ENDMODULE;

#endif

/***************************************************************
 * Copyright (c) 2019 Qualcomm Technologies International, Ltd.
 **************************************************************/

/* This public C function, declared in ttp_patches.h, is resolved
 * to an absolute addresses exported from the patch build.
 */
#include "subsys3_patch0_fw00001BD0_map_public.h"

#ifdef DISABLE_PATCH_BUILD_ID_CHECK

.const $_ttp_configure_sp_adjustment      ENTRY_POINT_TTP_CONFIGURE_SP_ADJUSTMENT
.const $_ttp_adjust_ttp_timestamp         ENTRY_POINT_TTP_ADJUST_TTP_TIMESTAMP
.const $_ttp_get_msg_state_params         ENTRY_POINT_TTP_GET_MSG_STATE_PARAMS
.const $_ttp_configure_state_params       ENTRY_POINT_TTP_CONFIGURE_STATE_PARAMS
.const $_ttp_get_state_params_status      ENTRY_POINT_TTP_GET_STATE_PARAMS_STATUS
.const $_ttp_get_msg_sp_adjustment        ENTRY_POINT_TTP_GET_MSG_SP_ADJUSTMENT
.const $_ttp_get_msg_adjust_ttp_timestamp ENTRY_POINT_TTP_GET_MSG_ADJUST_TTP_TIMESTAMP

#else

.MODULE $M.download_support_lib.ttp_configure_sp_adjustment;
.CODESEGMENT PM;
.MINIM;

$_ttp_configure_sp_adjustment:
#ifdef ENTRY_POINT_TTP_CONFIGURE_SP_ADJUSTMENT
    rMAC = M[$_patched_fw_version];
    Null = rMAC - PATCH_BUILD_ID;
    if EQ jump ENTRY_POINT_TTP_CONFIGURE_SP_ADJUSTMENT;
#endif
L_pb_mismatch:
    /* Stub: return (uintptr_t 0); */
    r0 = 0;
    rts;

.ENDMODULE;

.MODULE $M.download_support_lib.ttp_adjust_ttp_timestamp;
.CODESEGMENT PM;
.MINIM;

$_ttp_adjust_ttp_timestamp:
#ifdef ENTRY_POINT_TTP_ADJUST_TTP_TIMESTAMP
    rMAC = M[$_patched_fw_version];
    Null = rMAC - PATCH_BUILD_ID;
    if EQ jump ENTRY_POINT_TTP_ADJUST_TTP_TIMESTAMP;
#endif
L_pb_mismatch:
    /* Stub: return (uintptr_t 0); */
    r0 = 0;
    rts;

.ENDMODULE;

.MODULE $M.download_support_lib.ttp_get_msg_state_params;
.CODESEGMENT PM;
.MINIM;

$_ttp_get_msg_state_params:
#ifdef ENTRY_POINT_TTP_GET_MSG_STATE_PARAMS
    rMAC = M[$_patched_fw_version];
    Null = rMAC - PATCH_BUILD_ID;
    if EQ jump ENTRY_POINT_TTP_GET_MSG_STATE_PARAMS;
#endif
L_pb_mismatch:
    /* Stub: return (uintptr_t 0); */
    r0 = 0;
    rts;

.ENDMODULE;

.MODULE $M.download_support_lib.ttp_configure_state_params;
.CODESEGMENT PM;
.MINIM;

$_ttp_configure_state_params:
#ifdef ENTRY_POINT_TTP_CONFIGURE_STATE_PARAMS
    rMAC = M[$_patched_fw_version];
    Null = rMAC - PATCH_BUILD_ID;
    if EQ jump ENTRY_POINT_TTP_CONFIGURE_STATE_PARAMS;
#endif
L_pb_mismatch:
    /* Stub: return (uintptr_t 0); */
    r0 = 0;
    rts;

.ENDMODULE;

.MODULE $M.download_support_lib.ttp_get_state_params_status;
.CODESEGMENT PM;
.MINIM;

$_ttp_get_state_params_status:
#ifdef ENTRY_POINT_TTP_GET_STATE_PARAMS_STATUS
    rMAC = M[$_patched_fw_version];
    Null = rMAC - PATCH_BUILD_ID;
    if EQ jump ENTRY_POINT_TTP_GET_STATE_PARAMS_STATUS;
#endif
L_pb_mismatch:
    /* Stub: return (uintptr_t 0); */
    r0 = 0;
    rts;

.ENDMODULE;

.MODULE $M.download_support_lib.ttp_get_msg_sp_adjustment;
.CODESEGMENT PM;
.MINIM;

$_ttp_get_msg_sp_adjustment:
#ifdef ENTRY_POINT_TTP_GET_MSG_SP_ADJUSTMENT
    rMAC = M[$_patched_fw_version];
    Null = rMAC - PATCH_BUILD_ID;
    if EQ jump ENTRY_POINT_TTP_GET_MSG_SP_ADJUSTMENT;
#endif
L_pb_mismatch:
    /* Stub: return (uintptr_t 0); */
    r0 = 0;
    rts;

.ENDMODULE;

.MODULE $M.download_support_lib.ttp_get_msg_adjust_ttp_timestamp;
.CODESEGMENT PM;
.MINIM;

$_ttp_get_msg_adjust_ttp_timestamp:
#ifdef ENTRY_POINT_TTP_GET_MSG_ADJUST_TTP_TIMESTAMP
    rMAC = M[$_patched_fw_version];
    Null = rMAC - PATCH_BUILD_ID;
    if EQ jump ENTRY_POINT_TTP_GET_MSG_ADJUST_TTP_TIMESTAMP;
#endif
L_pb_mismatch:
    /* Stub: return (uintptr_t 0); */
    r0 = 0;
    rts;

.ENDMODULE;


#endif
/* Copyright (c) 2016 Qualcomm Technologies International, Ltd. */
/*   %%version */
/**
 * \file
 * General Utilities.
 */

#ifndef UTILS_PRIVATE_H
#define UTILS_PRIVATE_H

#include "pbc.h"
#include "utils/utils.h"
#include "hydra/hydra_patch.h"
#include "hydra_log/hydra_log.h"
#include "hydra/hydra_macros.h" /* Moved last to avoid offsetof problems on win32 */

/**
 * Args passed to utils_fsm_apply_event and utils_fsm_unhandled_event
 * patch points.
 */
typedef struct UTILS_FSM_APPLY_EVENT_PATCH_ARGS {
    /** The target FSM (instance) */
    utils_fsm *fsm;
    /** The event */
    const utils_fsmevent *event;
} UTILS_FSM_APPLY_EVENT_PATCH_ARGS;

/* Used exclusively to test patching */
extern uint16 utils_patch_val;

#endif /* UTILS_PRIVATE_H */

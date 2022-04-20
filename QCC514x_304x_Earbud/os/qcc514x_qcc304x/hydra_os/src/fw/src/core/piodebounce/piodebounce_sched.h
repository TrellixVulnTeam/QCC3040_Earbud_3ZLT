/* Copyright (c) 2016 Qualcomm Technologies International, Ltd. */
/*   %%version */
/**
 * \file
 * Scheduler header for piodebounce.
 */

#ifndef PIODEBOUNCE_SCHED_H
#define PIODEBOUNCE_SCHED_H


/** Task not used. */
#define CORE_PIODEBOUNCE_SCHED_TASK(m) 

/** Background interrupt definition. */
#define CORE_PIODEBOUNCE_BG_INT(m) \
    BG_INT(m, (piodebounce_bg, piodebounce_bg_int_handler))

#endif /* PIODEBOUNCE_SCHED_H */


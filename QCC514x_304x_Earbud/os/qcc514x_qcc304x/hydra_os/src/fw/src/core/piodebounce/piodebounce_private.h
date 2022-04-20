/* Copyright (c) 2016-2020 Qualcomm Technologies International, Ltd. */
/*   %%version */
/**
 * \file
 * Internal header file.
 */
#ifndef __PIODEBOUNCE_PRIVATE_H__
#define __PIODEBOUNCE_PRIVATE_H__

#include "piodebounce/piodebounce.h"
#include "hydra_log/hydra_log.h"
#include "int/int.h"
#include "pioint/pioint.h"
#include "pl_timers/pl_timers.h"
#include "sched/sched.h"
#include <assert.h>

/** No. of outputs per group that can be queued up for background handler.*/
/** The value has been carefully chosen to support button double press in
 *  application use cases.
 */
#define PIODEBOUNCE_NUM_OUTPUTS (4)

/** Type definition of each entry in output queue */
typedef struct piodebounce_output_entry {
    /** Stable value of pio debounce output */
    pio_size_bits output;
    /** Time when this output was triggered */
    TIME timestamp;
    /** The PIO bank that triggered the output */
    uint16 bank;
    /** Group which triggered the output */
    uint16 group;
} piodebounce_output_entry_t;

/** Type definition of group output queue */
typedef struct piodebounce_output_queue{
    /* Array of entries in the queue */
    piodebounce_output_entry_t entry[PIODEBOUNCE_NUM_OUTPUTS];
    /* Write index of the queue */
    uint8 index;
    /* Read index of the queue */
    uint8 outdex;
    /* No. of valid entries in the queue */
    uint8 count;
} piodebounce_output_queue_t;

/* Helper macros for circular output queue */
#define OUTPUT_NEXT_INDEX(index) (((index) + 1) % PIODEBOUNCE_NUM_OUTPUTS)
#define OUTPUT_NEXT_OUTDEX(outdex) (((outdex) + 1) % PIODEBOUNCE_NUM_OUTPUTS)
#define OUTPUT_QUEUE_IS_EMPTY(count) (!(count))
#define OUTPUT_QUEUE_IS_FULL(count) ((count) == PIODEBOUNCE_NUM_OUTPUTS)

/** Type definition for piodebounce group database. */
typedef struct piodebounce_db
{
    /** The bits being monitored on the pio port. */
    pio_size_bits mask[NUMBER_OF_PIO_BANKS];
    /** The number of samples that must be the same before accepting.  Value 0
        has a special meaning - see piodebounce.h. */
    uint16 nreads;
    /** The number of milliseconds to wait between samples. */
    uint16 period;
    /** The candidate new value for piodb_output. */
    pio_size_bits tmp[NUMBER_OF_PIO_BANKS];
    /** The number of samples remaining to be taken. */
    uint16 nleft;
    /** The callback to be called when a stable change is detected. */
    piodebounce_cb callback;
    /** Last known stable value from pio debounce */
    pio_size_bits last_output[NUMBER_OF_PIO_BANKS];
}piodebounce_db_t;

#endif  /* __PIODEBOUNCE_PRIVATE_H__ */

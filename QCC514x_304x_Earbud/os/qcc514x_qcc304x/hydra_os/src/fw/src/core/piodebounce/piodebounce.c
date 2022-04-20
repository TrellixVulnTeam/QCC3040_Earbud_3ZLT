/* Copyright (c) 2016-2020 Qualcomm Technologies International, Ltd. */
/*   %%version */
/**
 * \file
 * Debounce input signal on the pio port.
 */

#include "piodebounce/piodebounce_private.h"

/** Timer API "timer_cancel_event_by_function" can't identify group 
 * specific timer events if event data_pointer is set to NULL. Hence,
 * below macros are used to convert group ID to/from non-NULL event
 * data_pointer value.
 */
#define CONVERT_GROUP_TO_TIMER_EVENT_DATA(group) ((void *)(uint32) ~(group))
#define CONVERT_TIMER_EVENT_DATA_TO_GROUP(event) ((uint16) ~((uint32)(event)))

/** Stores the piodebounce database. */
static piodebounce_db_t pio_debounce[PIODEBOUNCE_NUMBER_OF_GROUPS];
/** Stores the outputs queued up for background handler */
static piodebounce_output_queue_t pio_debounce_outputs;

/* Forward references. */
static void sched_debounce_timer(uint16 group);
static void update_debounce_output_queue(uint16 group, uint16 bank);
static void register_debounce_kick(uint16 group);
static void piodebounce_kick_0(void);
static void piodebounce_kick_1(void);
static void piodebounce_kick_2(void);

void init_piodebounce(void)
{
    uint16 group, bank;

    for (group = 0; group < PIODEBOUNCE_NUMBER_OF_GROUPS; group++)
    {
        for (bank = 0; bank < NUMBER_OF_PIO_BANKS; bank++)
        {
            pio_debounce[group].mask[bank] = 0x00000000;
            pio_debounce[group].last_output[bank] = 0x00000000;
        }
        register_debounce_kick(group);
    }
}

void piodebounce_config(uint16 group,
                        uint16 bank,
                        pio_size_bits mask,
                        uint16 nreads,
                        uint16 period,
                        bool priority,
                        piodebounce_cb callback)
{
    assert(group < PIODEBOUNCE_NUMBER_OF_GROUPS);

    L4_DBG_MSG4("Piodebounce: Registering callback 0x%08x on bank %d "
                "mask 0x%08x for group %d", callback, bank, mask, group);

    pio_debounce[group].mask[bank] = mask;
    pio_debounce[group].period = period;
    pio_debounce[group].nreads = nreads;
    pio_debounce[group].callback = callback;
    UNUSED(priority);

    /* Set an initial output value. If pio_debounce[group].mask is zero
     * this will return zero. */
    pio_debounce[group].last_output[bank] = pio_get_levels_all(bank) & mask;
    register_debounce_kick(group);
}
static void piodebounce_kick_0(void)
{
    piodebounce_kick(0);
}

static void piodebounce_kick_1(void)
{
    piodebounce_kick(1);
}

static void piodebounce_kick_2(void)
{
    piodebounce_kick(2);
}

/* Utility function to register pio interrupt handler for specified group */
static void register_debounce_kick(uint16 group)
{
    uint16 bank;

    for (bank = 0; bank < NUMBER_OF_PIO_BANKS; bank++)
    {
        pioint_configure(bank, pio_debounce[group].mask[bank],
                            group ? ((group == 1) ? piodebounce_kick_1: piodebounce_kick_2) : piodebounce_kick_0);
    }
}

/* Utility function to find next write index of output queue */
static bool get_output_queue_index(uint16 *index)
{
    piodebounce_output_queue_t *queue = &pio_debounce_outputs;

    if (OUTPUT_QUEUE_IS_FULL(queue->count))
    {
        return FALSE;
    }

    assert(index);
    *index = queue->index;
    return TRUE;
}

/* Utility function to find read index of output queue */
static bool get_output_queue_outdex(uint16 *outdex)
{
    piodebounce_output_queue_t *queue = &pio_debounce_outputs;

    if (OUTPUT_QUEUE_IS_EMPTY(queue->count))
    {
        return FALSE;
    }

    assert(outdex);
    *outdex = queue->outdex;
    return TRUE;
}

/* Utility function to update write index of output queue.
 * NOTE: This function must be called in interrupt context.
 */
static void advance_output_queue_index(void)
{
    piodebounce_output_queue_t *queue = &pio_debounce_outputs;

    queue->index = OUTPUT_NEXT_INDEX(queue->index);

    /* count is decremented in background task handler which can't
     * preempt interrupt. Hence, there is no need to protect
     * below update.
     */
    queue->count++;
}

/* Utility function to update read index of output queue */
static void advance_output_queue_outdex(void)
{
    piodebounce_output_queue_t *queue = &pio_debounce_outputs;

    queue->outdex = OUTPUT_NEXT_OUTDEX(queue->outdex);

    /* output queue can become full on addition of new entry in
     * update_debounce_output() which runs in strict timer (or interrupt)
     * context. If interrupt preempts below count update, the count
     * will be one less than actual no. of entries in queue. This mismatch
     * will result in new entry (added in interrupt) to be overwritten on
     * subsequent debounce entries. Hence, below count update must be
     * protected against interrupts.
     */
    ATOMIC_BLOCK_START
    {
        queue->count--;
    } ATOMIC_BLOCK_END;
}

/* Interrupt handler to trigger pio debounce algorithm for specified group */
void piodebounce_kick(uint16 group)
{
    uint16 bank;

    assert(group < PIODEBOUNCE_NUMBER_OF_GROUPS);

    L4_DBG_MSG2("Piodebounce: Received kick for group %d at %u usec", group, get_time());
    for (bank = 0; bank < NUMBER_OF_PIO_BANKS; bank++)
    {
        pio_debounce[group].tmp[bank] = pio_get_levels_mask(bank, pio_debounce[group].mask[bank]);
    }

    switch (pio_debounce[group].nreads)
    {
        case 0:
            /* Special case 1: if pio_debounce[group].nreads is 0 then always
             * accept the input pins current value and always raise an event. */
            {
                update_debounce_output_queue(group, 0);
            }
            break;
        case 1:
            /* Special case 2: if pio_debounce[group].nreads is 1 then always
             * accept the input pins current value and raise an event if it
             * differs from the last debounced value. */
            for (bank = 0; bank < NUMBER_OF_PIO_BANKS; bank++)
            {
                if (pio_debounce[group].last_output[bank] != pio_debounce[group].tmp[bank])
                {
                    {
                        update_debounce_output_queue(group, 0);
                    }
                    break;
                }
            }
            break;
        default:
            /* The normal case: start a sequence of reads. */
            pio_debounce[group].nleft = (uint16)(pio_debounce[group].nreads - 1);
            sched_debounce_timer(group);
            break;
    }
}

/* Timed event handler used to perform PIO debouncing.
 * The group ID is obtained from the event_data parameter.
 */
static void debounce_timer_handler(void *event_data)
{
    uint32 current[NUMBER_OF_PIO_BANKS];
    uint16 bank;
    uint16 group = CONVERT_TIMER_EVENT_DATA_TO_GROUP(event_data);

    L4_DBG_MSG2("Piodebounce: Polling PIOs in group %d at %u usec", group, get_time());

    for (bank = 0; bank < NUMBER_OF_PIO_BANKS; bank++)
    {
        current[bank] = pio_get_levels_mask(bank, pio_debounce[group].mask[bank]);

        /* Restart the sequence of reads if the current port value does
         * not match the candidate new debounce value.
         */
        if (pio_debounce[group].tmp[bank] != current[bank])
        {
            L4_DBG_MSG1("Piodebounce: PIOs in group %d changed, kicking", group);
            piodebounce_kick(group);
            return;
        }
    }

    if (pio_debounce[group].nleft)
    {
        /* Go round again if we've not read the pins enough times. */
        --pio_debounce[group].nleft;
        sched_debounce_timer(group);
    }
    else
    {
        /* Accept candidate value as stable and signal the change. */
        L4_DBG_MSG1("Piodebounce: PIOs in group %d are stable", group);
        for (bank = 0; bank < NUMBER_OF_PIO_BANKS; bank++)
        {
            if (pio_debounce[group].last_output[bank] != pio_debounce[group].tmp[bank])
            {
                {
                    update_debounce_output_queue(group, 0);
                }
            }
        }
    }
}

/* Schedule a strict timed event for next debounce
 * operation for specified group.
 * NOTE: This must be called in interrupt context
 */
static void sched_debounce_timer(uint16 group)
{

    /* Cancel any pending timed event for debounce */
    timer_cancel_event_by_function(debounce_timer_handler,
                                   CONVERT_GROUP_TO_TIMER_EVENT_DATA(group));

    (void)timer_schedule_event_in((pio_debounce[group].period * MILLISECOND),
                                  debounce_timer_handler,
                                  CONVERT_GROUP_TO_TIMER_EVENT_DATA(group));
}

/* Function to update output queue and schedule background
 * task to inform the clients about the new output.
 * NOTE: This must be called in interrupt context
 */
static void update_debounce_output_queue(uint16 group, uint16 bank)
{
    pio_size_bits output;
    uint32 i;
    uint16 index;

    /* Update last stable output as well*/
    pio_debounce[group].last_output[bank] = pio_debounce[group].tmp[bank];

    /* Ignore update request if output queue is full */
    if (!get_output_queue_index(&index))
    {
        return;
    }
    L4_DBG_MSG3("Piodebounce: PIOs in group %d bank %d index %d changed",
                group, bank, index);

    /* Update the bank and output */
    pio_debounce_outputs.entry[index].bank = bank;

    /* Include the PIO state for all groups in the output so that message
       handlers do not need to calculate state based on the group. */
    output = 0;
    for(i = 0; i < PIODEBOUNCE_NUMBER_OF_GROUPS; ++i)
    {
        output |= pio_debounce[i].last_output[bank];
    }
    pio_debounce_outputs.entry[index].output = output;

    pio_debounce_outputs.entry[index].group = group;
    pio_debounce_outputs.entry[index].timestamp = get_time();
    advance_output_queue_index();

    /* Drop to background and call the handler. */
    GEN_BG_INT(piodebounce_bg);
}

/* Background task handler to process piodebounce output_queue */
void piodebounce_bg_int_handler(void)
{
    piodebounce_output_queue_t *output_queue = &pio_debounce_outputs;
    uint16 outdex;

    /* Scan through available entries in output queue */
    while (get_output_queue_outdex(&outdex))
    {
        const piodebounce_output_entry_t *entry = &output_queue->entry[outdex];
        L4_DBG_MSG3("Piodebounce: PIOs in group %d, bank %d, outdex %d "
                    "changed, calling callback", entry->group, entry->bank,
                    outdex);

        if (pio_debounce[entry->group].callback != NULL)
        {
            pio_debounce[entry->group].callback(entry->group,
                                                entry->bank,
                                                entry->output,
                                                entry->timestamp);
        }

        L4_DBG_MSG4("Piodebounce: PIOs in group %d, bank %d, outdex %d "
                    "changed, finished callback at %u usec", entry->group, entry->bank,
                    outdex, get_time());
        advance_output_queue_outdex();
    }
}



/* Copyright (c) 2016, 2019 Qualcomm Technologies International, Ltd. */
/*   %%version */

/****************************************************************************
Include Files
*/
#include "dorm/dorm_private.h"
#include "int/int.h"
#include "pl_timers/pl_timers.h"
#include "sched/sched.h"
#include "assert.h"
#include "ipc/ipc.h"
#include "mmu/mmu.h"
#include "hal/hal_cross_cpu_registers.h"

PRESERVE_ENUM_FOR_DEBUGGING(dorm_state)

/****************************************************************************
Private Types
*/
enum best_case_sleep {
    best_case_no_sleep,
    best_case_shallow_sleep,
    best_case_deep_sleep
};

/****************************************************************************
Private Function Prototypes
*/
static enum best_case_sleep sleep_depth_to_consider(void);
static bool is_worth_deep_sleeping(TIME latest, bool deadline);
static void safe_enable_deep_sleep(TIME earliest, TIME latest, bool deadline);

/****************************************************************************
Private Variable Definitions
*/

/****************************************************************************
Private Function Definitions
*/

#if defined(ENABLE_FAST_WAKE) && defined(CHIP_HAS_SCALED_CLOCK)
/**
 * \brief Number of microseconds early to wake to veto scaled clock.
 */
#define EARLY_WAKE_TIME_US (50)

/**
 * \brief Handler for the early wake timer.
 *
 * Does not need to do anything, causing the ISR to run does enough.
 *
 * \param [in] event_data Unused
 */
static void early_wake_handler(void *event_data)
{
    UNUSED(event_data);
}

/**
 * \brief Cancel the early wake timer if it is present.
 */
static void early_wake_cancel(void)
{
    timer_cancel_event_by_function(early_wake_handler, NULL);
}

/**
 * \brief Wake the chip up early so clocks are at full speed for the deadline.
 *
 * \param [in] latest    The time the chip needs to wake if there is a deadline.
 * \param [in] deadline  Whether there is a wake deadline or not.
 */
static void early_wake_enable(TIME latest, bool deadline)
{
    if(deadline)
    {
        TIME wake_time;

        /* The handler for this timed event does nothing. It relies on the timer
         * interrupt causing the hardware to veto scaled clock and the interrupt
         * handler to remove any clock scaling so the core clocks are at full
         * speed when the real timer interrupt expires.
         *
         * The deep sleep deadlines that we tell curator are not adjusted. If
         * curator actually puts the chip into deep sleep then we don't need the
         * early wake timer anyway, it's there for waking from scaled clock.
         *
         * This early wake event is not strictly required when apps is set to
         * run at a higher clock frequency (VM_PERFORMANCE), but running it
         * anyway doesn't cost much and does give marginal latency improvements
         * (2us faster) when the application is likely concerned with
         * performance and not power.
         */
        early_wake_cancel();
        wake_time = (TIME)time_sub(latest, EARLY_WAKE_TIME_US);
        (void)timer_schedule_event_at(wake_time, early_wake_handler, NULL);
    }
}
#else
#define early_wake_enable(_latest, _deadline) do { } while(0)
#define early_wake_cancel() do { } while(0)
#endif /* defined(ENABLE_FAST_WAKE) && defined(CHIP_HAS_SCALED_CLOCK) */

/****************************************************************************
Public Function Definitions
*/

/* Configure the way we sleep */
void init_dorm(void)
{
}

/* dorm_sleep */

/** Main entry point for when the processor background is idle. */
void dorm_sleep_sched(void)
{
    switch(sleep_depth_to_consider())
    {
    case best_case_no_sleep:
        return;
    case best_case_deep_sleep:
    {
        /* Deep sleep is a possibility so we need to check further. */
        TIME earliest, latest;
        bool deadline;

        deadline = sched_get_sleep_deadline(&earliest, &latest);

        if (is_worth_deep_sleeping(latest, deadline))
        {
            /* Fast wake only needs to be enabled on P0. P0 decides when to deep
             * sleep for P0 and P1 and vetoing scaled clock is system wide so P1
             * will also be up to speed when the timer expires.
             */
            early_wake_enable(latest, deadline);
            safe_enable_deep_sleep(earliest, latest, deadline);
            early_wake_cancel();
            return;
        }
        enter_shallow_sleep();
        return;
    }
    default:
        /* It must be shallow sleep in this case. */
        enter_shallow_sleep();
        return;
    }
}

void dorm_shallow_sleep(TIME latest)
{
    uint16 kip_flags = dorm_get_combined_kip_flags();

    UNUSED(latest);

    if ((kip_flags & DORM_STATE_NO_SHALLOW) == 0)
    {
        enter_shallow_sleep();
    }
}


/**
 * \brief Decides which sleep level apps should consider.
 *
 * \note The point of this function is to allow us to short-circuit expensive
 *       calculations. Hence this should include only really cheap tests. Test
 *       should be ordered to have the test with the average least cost first.
 *       If this function says that deep sleep is worth considering the you
 *       still need to call is_worth_deep_sleeping() which can do the
 *       expensive tests. If you want both then call should_deep_sleep().
 *
 * \return best_case_no_sleep if we're not allowed to sleep at all,
 *         best_case_shallow_sleep if we're definitely not allowed to deep
 *         sleep or best_case_deep_sleep if deep sleep is a possibility.
 */
static enum best_case_sleep sleep_depth_to_consider(void)
{
    uint16 kip_flags = dorm_get_combined_kip_flags();

    if ((kip_flags & DORM_STATE_NO_SHALLOW) != 0)
    {
        return best_case_no_sleep;
    }

    if ((kip_flags & DORM_STATE_NO_DEEP) != 0)
    {
        return best_case_shallow_sleep;
    }

    return best_case_deep_sleep;
}

/**
 * \brief Given that we could deep sleep, is it worth it?
 *
 * \note This completes the tests for deep sleep started in
 *       sleep_depth_to_consider(). You must call that function before
 *       this one and proceed to this one only if that function returns
 *       best_case_deep_sleep. If you want both in one go then call
 *       should_deep_sleep();
 *
 * \return TRUE if we should proceed with deep sleeping, FALSE if it's not
 *         worth it.
 */
static bool is_worth_deep_sleeping(TIME latest, bool deadline)
{
    TIME earliest_deep_sleep = time_add(get_time(), DEEP_SLEEP_MIN_TIME);
    if (deadline && time_gt(earliest_deep_sleep, latest))
    {
        L4_DBG_MSG1("Not enough time to deep sleep (%uus)",
                    time_sub(latest, get_time()));
        return FALSE;
    }

    return TRUE;
}

#ifdef OS_FREERTOS

typedef struct dorm_p1_deep_sleep_state_
{
    bool enabled;
    TIME earliest;
    TIME latest;
} dorm_p1_deep_sleep_state_t;

/**
 * Stores the deep sleep settings that P1 has told P0 about, on P1.
 * P1 uses this to know whether P0 needs its deep sleep info updating.
 */
static dorm_p1_deep_sleep_state_t p0_view_of_p1_deep_sleep =
{
    FALSE,
    DORM_EARLIEST_WAKEUP_TIME_NO_DEADLINE,
    DORM_LATEST_WAKEUP_TIME_NO_DEADLINE
};

void dorm_disable_deep_sleep_if_enabled(void)
{
    if(p0_view_of_p1_deep_sleep.enabled)
    {
        hal_set_reg_proc_deep_sleep_en(0);
        ipc_send_p1_deep_sleep_msg(FALSE, 0L, 0L);
        p0_view_of_p1_deep_sleep.enabled = FALSE;
    }
}

#endif /* defined(PROCESSOR_P1) && defined(OS_FREERTOS) */

/**
 * \brief P1 sleep procedure
 * P1 delegates P0 for any deep sleep communication to curator
 * \param earliest the earliest time P1'd like to be woken. Will be
 * ignored if deadline parameter is set to FALSE.
 * \param latest the deadline by which P1 must be woken. Will be
 * ignored if deadline parameter is set to FALSE.
 * \param deadline TRUE if deep sleep has been requested with deadline,
 * FALSE otherwise.
 */
static void safe_enable_deep_sleep(TIME earliest, TIME latest, bool deadline)
{
    /* P1 sets the deep sleep registry and let P0 drive the deep sleep through curator. */
    hal_set_reg_proc_deep_sleep_en(1);

    /* Use special values if deadline flag is not set */
    if (!deadline)
    {
        earliest = (TIME) DORM_EARLIEST_WAKEUP_TIME_NO_DEADLINE;
        latest = (TIME) DORM_LATEST_WAKEUP_TIME_NO_DEADLINE;
    }

#ifdef OS_FREERTOS
    if(!(p0_view_of_p1_deep_sleep.enabled &&
         p0_view_of_p1_deep_sleep.earliest == earliest &&
         p0_view_of_p1_deep_sleep.latest == latest))
    {
        ipc_send_p1_deep_sleep_msg(TRUE, earliest, latest);

        p0_view_of_p1_deep_sleep.enabled = TRUE;
        p0_view_of_p1_deep_sleep.earliest = earliest;
        p0_view_of_p1_deep_sleep.latest = latest;
    }
    enter_shallow_sleep();
#endif /* OS_FREERTOS */

#ifdef OS_OXYGOS
    ipc_send_p1_deep_sleep_msg(TRUE, earliest, latest);

    enter_shallow_sleep();

    /* Reset deep sleep registry and inform P0 no more deep sleep is required. */
    hal_set_reg_proc_deep_sleep_en(0);
    ipc_send_p1_deep_sleep_msg(FALSE, 0L, 0L);
#endif /* OS_OXYGOS */
}

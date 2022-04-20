/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Timers
*/

#ifndef TIMER_H_
#define TIMER_H_

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include "stm32f0xx.h"
#include "cli.h"

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

#define TIMER_FREQUENCY_HZ 100

/*-----------------------------------------------------------------------------
------------------ DEBUG MACROS -----------------------------------------------
-----------------------------------------------------------------------------*/

#ifdef FAST_TIMER_INTERRUPT

#define TIMER_DEBUG_VARIABLES(x) \
uint64_t x##_total_time_taken; \
uint64_t x##_start_measurement_time; \
uint32_t x##_no_of_measurements; \
uint64_t x##_slowest_time;

#define TIMER_DEBUG_START(x) \
x##_start_measurement_time = global_time_us;

#define TIMER_DEBUG_STOP(x) \
{ \
    uint64_t t_taken = global_time_us - x##_start_measurement_time; \
    x##_no_of_measurements++; \
    x##_total_time_taken += t_taken; \
    if (t_taken > x##_slowest_time) \
    { \
        x##_slowest_time = t_taken; \
    } \
}

#else

#define TIMER_DEBUG_VARIABLES(x) \
uint64_t x##_total_time_taken; \
uint32_t x##_start_measurement_time; \
uint32_t x##_no_of_measurements; \
uint64_t x##_slowest_time;

#define TIMER_DEBUG_START(x) \
x##_start_measurement_time = TIM14->CNT;

#define TIMER_DEBUG_STOP(x) \
{ \
    uint32_t t_now = TIM14->CNT; \
    uint32_t t_taken; \
    if (t_now < x##_start_measurement_time) \
    { \
        t_taken = (0xFFFF - x##_start_measurement_time) + t_now + 1; \
    } \
    else \
    { \
        t_taken = t_now - x##_start_measurement_time; \
    } \
    x##_no_of_measurements++; \
    x##_total_time_taken += t_taken; \
    if (t_taken > x##_slowest_time) \
    { \
        x##_slowest_time = t_taken; \
    } \
}

#endif /*FAST_TIMER_INTERRUPT*/

#define TIMER_DEBUG_CMD(x) \
void x##_timer_debug_cmd(uint8_t cmd_source) \
{ \
    PRINTF("%d %d %d", x##_no_of_measurements, \
    (uint32_t)(x##_total_time_taken / x##_no_of_measurements), \
    (uint32_t)x##_slowest_time); \
}

/*-----------------------------------------------------------------------------
------------------ TYPE DEFINITIONS -------------------------------------------
-----------------------------------------------------------------------------*/

typedef uint32_t timer_ticks_t;

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

#ifdef FAST_TIMER_INTERRUPT
extern volatile uint64_t global_time_us;
#endif

extern volatile bool systick_has_ticked;
extern uint32_t slow_count;
extern volatile uint32_t ticks;

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

void timer_init(void);
void timer_clock_disable(void);
CLI_RESULT timer_cmd(uint8_t cmd_source);
void delay_ms(int ms);
void timer_comms_tick_start(void);
void timer_comms_tick_stop(void);
void timer_sleep(void);
void timer_wake(void);
uint16_t timer_seed_value(void);

#endif // TIMER_H_

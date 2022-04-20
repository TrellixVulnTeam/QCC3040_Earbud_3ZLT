/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Timestamp events.
*/

#include "timestamp_event.h"

#include "vm.h"
#include "panic.h"
#include "logging.h"

#ifndef DISABLE_TIMESTAMP_EVENT

#ifdef GC_SECTIONS
/* Move all functions in KEEP_PM section to ensure they are not removed during
 * garbage collection */
#pragma unitcodesection KEEP_PM
#endif

#define ASSERT_ID_IS_VALID(id) PanicFalse((id) < NUMBER_OF_TIMESTAMP_EVENTS)

/* Store the timestamp when the event occurred */
static uint16 timestamp_events[NUMBER_OF_TIMESTAMP_EVENTS];

void TimestampEvent(timestamp_event_id_t id)
{
    ASSERT_ID_IS_VALID(id);

    timestamp_events[id] = (uint16)VmGetClock();
}

void TimestampEvent_Offset(timestamp_event_id_t id, uint16 offset_ms)
{
    ASSERT_ID_IS_VALID(id);

    timestamp_events[id] = (uint16)VmGetClock() + offset_ms;
}

static uint16 timestampEvent_Get(timestamp_event_id_t id)
{
    ASSERT_ID_IS_VALID(id);

    return timestamp_events[id];
}

uint32 TimestampEvent_Delta(timestamp_event_id_t id1, timestamp_event_id_t id2)
{
    uint16 delta = (uint16)(timestampEvent_Get(id2) - timestampEvent_Get(id1));
    DEBUG_LOG_VERBOSE("TimestampEvent_Delta (%d) id2 %d - id1 %d = delta %d",
            timestampEvent_Get(id2) > timestampEvent_Get(id1), timestampEvent_Get(id2), timestampEvent_Get(id1), delta);

    return (uint32)delta;
}

uint32 TimestampEvent_DeltaFrom(timestamp_event_id_t start_id)
{
    uint16 delta = (uint16)((uint16)VmGetClock() - timestampEvent_Get(start_id));

    return (uint32)delta;
}

uint16 TimestampEvent_GetTime(timestamp_event_id_t id)
{
    return timestampEvent_Get(id);
}

#endif /* DISABLE_TIMESTAMP_EVENT */

/*
Copyright (c) 2018-2021  Qualcomm Technologies International, Ltd.
*/

#ifndef INPUT_EVENT_MANAGER_H_
#define INPUT_EVENT_MANAGER_H_

#include <message.h>
#include "library.h"

#ifndef PIOS_PER_BANK
#include <hydra_dev.h>
#endif

#define IEM_NUM_PIOS  (NUMBER_OF_PIO_BANKS * PIOS_PER_BANK)

typedef uint32 input_event_bits_t;

typedef enum
{
    ENTER,
    HELD,
    RELEASE,
    HELD_RELEASE,
    MULTI_CLICK
} InputEventAction_t;

/* Used as the type name of a data structure in ButtonParseXML.py */
/* The member names are not used, but the order is assumed. */
typedef struct
{
    input_event_bits_t  bits;
    input_event_bits_t  mask;
    InputEventAction_t  action;
    uint16              timeout;
    uint16              repeat;     /* Only used for HELD and ENTER actions */
    uint16              count;      /* Only used for MULTI_CLICK actions */
    MessageId           message;
} InputActionMessage_t;

/* Used as the type name of a data structure in ButtonParseXML.py */
/* The member names are not used, but the order is assumed. */
typedef struct
{
    int8                pio_to_iem_id[IEM_NUM_PIOS];
    uint32              pio_input_mask[NUMBER_OF_PIO_BANKS];
    uint16              debounce_reads;
    uint16              debounce_period;
} InputEventConfig_t;

/*! @brief Initialise the input event manager. */
Task InputEventManagerInit(Task client,
                           const InputActionMessage_t *action_table,
                           uint32 action_table_dim,
                           const InputEventConfig_t *input_config);

void InputEventManager_RegisterClient(Task client);

#endif /* INPUT_EVENT_MANAGER_H_ */

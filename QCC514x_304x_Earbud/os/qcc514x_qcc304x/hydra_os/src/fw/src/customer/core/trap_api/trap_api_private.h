/* Copyright (c) 2016 Qualcomm Technologies International, Ltd. */
/*   %%version */
#ifndef TRAP_API_PRIVATE_H_
#define TRAP_API_PRIVATE_H_

#include "trap_api/trap_api.h"
#include "trap_api/api.h"
#include "sched/sched.h"
#include "ipc/ipc_msg_types.h"
#include "ipc/ipc.h"
#include "pio/pio.h"
#include "piodebounce/piodebounce.h"
#include "pmalloc/pmalloc.h"
#include "utils/utils_bit.h"
#include <message.h>
#include <sink_.h>
#include <source_.h>
#include "hydra_log/hydra_log.h"
#define IO_DEFS_MODULE_K32_CORE
#include "hal/hal_macros.h"

/**
 * Enumerated type definition for the types of items that are logged.
 * These correspond to the log types in xIDE VM message logging.
 */
typedef enum TRAP_API_LOG_ACTION
{
    TRAP_API_LOG_SEND,
    TRAP_API_LOG_DELIVER,
    TRAP_API_LOG_FREE,
    TRAP_API_LOG_CANCEL
} TRAP_API_LOG_ACTION;

struct SINK_SOURCE_HDLR_ENTRY;

/**
 * Array of App-Task/sched-taskid pairings for registered message handlers
 *
 * \note The initialiser for this array must correspond to the enumeration above
 */
extern Task registered_hdlrs[];

/**
 * Array of App-Task/pio-group pairings for registered message handlers
 *
 * \note The initialiser for this array must correspond to the enumeration above
 */
extern Task registered_pio_hdlrs[];

typedef void (*Handler)(Task t, MessageId id, Message m);

/** Enumeration of possible bit-widths of the variable on which a message send
* is conditionalised.
*/
typedef enum
{
    CONDITION_WIDTH_UNUSED = 0,/*!< The condition is not used */
    CONDITION_WIDTH_16BIT = 16,/*!< 16 bits */
    CONDITION_WIDTH_32BIT = 32 /*!< 32 bits */
    /* Don't add wider types: 32 bits is the most the conditional send logic
     * is expecting */
} CONDITION_WIDTH;

/** Structure used for queue entries that can be either simple messages,
 * conditional or timed.
 */
typedef struct AppMessage
{
    struct AppMessage *next;
    uint32 due;                  /**< Millisecond time to deliver this message */
    union
    {
        Task task;               /**< Receiving task (if unicast) */
        Task *tlist;             /**< Ptr to receiving task list (if multicast) */
    } t;
    void *message;               /**< Pointer to the message payload */
    const void *condition_addr;  /**< Pointer to condition value */
    uint16 id;                   /**< Message ID */
    CONDITION_WIDTH c_width;     /**< Width of condition value */
    uint8 multicast;             /**< If multicast, task is a null-terminated list */
    uint8 refcount;              /**< Initialised to 1, the structure and message
                                    payload is freed when this hits 0. */
} AppMessage;

/**
 * \brief Cast the const away from the Message type.
 *
 * Occasionally needed as even though the message types are passed as const
 * sometimes they are modified, e.g. USB messages can be replaced in the message
 * queue and when they're free'd they need to be passed to pfree which accepts
 * a non-const pointer.
 *
 * \param[in] _message  A pointer to a Message type.
 * \return \p _message with the const qualifier casted away.
 */
#define MESSAGE_REMOVE_CONST(_message) ((void *)(uintptr)(_message))

/**
 * Magic value for blocking out a task in a multicast list
 */
#define INVALIDATED_TASK (1)

/**
 * Macro to determine if a buffer is a stream. Stubbed out for now.
 */
#define IS_STREAM(x) FALSE

/**
 * Helper function to register a trap API Task with the Oxygen scheduler,
 * returning the previously-registered Task (NULL if none)
 * @param task Task to be registered
 * @param msg_type_id Index of the handler in the msg handler array
 * @return Task previously registered in the task record (NULL if none)
 */
Task trap_api_register_message_task(Task task, IPC_MSG_TYPE msg_type_id);

/**
 * Helper function to register a trap API Task with the FreeRTOS scheduler,
 * returning the previously-registered Task (NULL if none)
 * @param task Task to be registered
 * @param group PIO group for which task is being registered
 * @return Task previously registered in the task record (NULL if none)
 */
Task trap_api_register_message_group_task(Task task, uint16 group);

/**
 * Helper function to get the currently registered task for a message id.
 */
#define trap_api_lookup_message_task(_msg_type_id) \
    registered_hdlrs[(_msg_type_id)]

struct SINK_SOURCE_HDLR_ENTRY *trap_api_get_sink_source_hdlr_entry(Sink sink,
                                                                   Source source,
                                                                   bool create);

/**
 * Handler function that can be used to create a staging area for messages that
 * come up from the core.  They can be retrieved individually by calling
 * \c trap_api_test_get_next()
 */
void trap_api_test_task_handler(Task t, MessageId id, Message m);

struct TEST_MESSAGE_LIST;
/**
 *  Returns the head of the list of delivered messages
 * @return The first entry in the list.  Ownership is passed to the caller
 */
struct TEST_MESSAGE_LIST *trap_api_test_get_next(void);

/**
 * Resets the amount of space the test code reserves to make copies of message
 * bodies, since the handler can't know the size of message.  The default is 32.
 */
uint32 trap_api_test_reset_max_message_body_bytes(uint32 new_size);

/**
 * Do an arbitrary write to the given offset inside VM BUFFER window
 * to force the hardware to map in a VM page
 * @param offset Offset inside VM BUFFER window to write to.
 * Probably the start of an MMU page.
 */
void trap_api_test_map_page_at(uint32 offset);

/**
 * Log the state of a message now.
 * \param action What is happening to the message.
 * \param msg The AppMessage structure for the message.
 * \param now The current time in milliseconds
 */
void trap_api_message_log_now(
    TRAP_API_LOG_ACTION action,
    AppMessage *        msg,
    uint32 now);

/**
 * Log the state of a message
 * \param action: what is happening to the message.
 * \param msg: the AppMessage structure for the message.
 */
#define trap_api_message_log(action, msg) trap_api_message_log_now(action, msg, get_milli_time())


#ifdef OS_FREERTOS

/**
 * Macros for declaring and writing fixed size types at various byte alignments.
 */

#define U8_OFF0(_name) uint8 _name
#define U8_OFF0_SET(_dst, _src) do { \
    ((_dst) = (uint8)(_src)); \
    } while(0)

#define U16_OFF0(_name) uint16 _name
#define U16_OFF0_SET(_dst, _src) do { \
    ((_dst) = (uint16)(_src)); \
    } while(0)

#define U16_OFF1(_name) uint8 _name##_low; uint8 _name##_high
#define U16_OFF1_SET(_dst, _src) do { \
    (_dst##_low) = (uint8)(_src); \
    (_dst##_high) = (uint8)((_src) >> 8); \
    } while(0)

#define U32_OFF0(_name) uint32 _name
#define U32_OFF1(_name) uint8 _name##_low; uint16 _name##_mid; uint8 _name##_high
#define U32_OFF1_SET(_dst, _src) do { \
    (_dst##_low) = (uint8)(_src); \
    (_dst##_mid) = (uint16)((_src) >> 8); \
    (_dst##_high) = (uint8)((_src) >> 24); \
    } while(0)

#define U32_OFF2(_name) uint16 _name##_low; uint16 _name##_high
#define U32_OFF2_SET(_dst, _src) do { \
    (_dst##_low) = (uint16)(_src); \
    (_dst##_high) = (uint16)((_src) >> 16); \
    } while (0)

/**
 * Trap API message header.
 *
 * Structured to that once initialised it can be directly copied to the log
 * buffer.
 */
typedef struct trap_msg_header_
{
    /* This structure must be packed and KCC doesn't support packing structures.
       Pack the structure manually by declaring the type along with how far away
       it is from its naturally aligned boundary. */
    U16_OFF0(record_length);
    U32_OFF2(delimiter);
    U16_OFF0(seq_num);
    U32_OFF2(now_ms);
    U8_OFF0(action);
    U32_OFF1(task);
    U32_OFF1(handler);
    U16_OFF1(id);
    U32_OFF1(condition_address);
    U32_OFF1(due_ms);
    U16_OFF1(msg_len);

    /* Dummy member for calculating the size of this header at compile time
       without including any padding at the end of the structure. Must always
       be at the end of the structure. */
    uint8 _sentinel;
} trap_msg_header_t;

/**
 * \brief Log the state of a message
 * \param [in] header  A partially initialised message header structure.
 * The caller must initialise action, task, handler, id, condition_address and
 * due_ms.
 * \param [in] msg  The application allocated message pointer.
 */
void trap_api_multitask_message_log(trap_msg_header_t *header, Message msg);

#endif /* OS_FREERTOS */

/*@}*/

#endif /* TRAP_API_PRIVATE_H_ */
/*@}*/

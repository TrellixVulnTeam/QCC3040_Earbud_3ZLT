/* Copyright (c) 2016 Qualcomm Technologies International, Ltd. */
/*   %%version */
#ifndef IPC_PRIVATE_H_
#define IPC_PRIVATE_H_

#define IO_DEFS_MODULE_KALIMBA_INTERPROC_INT
#define IO_DEFS_MODULE_K32_CORE
#define IO_DEFS_MODULE_APPS_SYS_CPU_MEMORY_MAP
#include "ipc/ipc.h"
#include "hydra/hydra.h"
#include "hydra/hydra_types.h"

#ifdef DESKTOP_TEST_BUILD
# include "smalloc/smalloc.h"
#endif /* DESKTOP_TEST_BUILD */

#include "ipc/ipc_buffer.h"
#include "panic/panic.h"
#include "hydra_log/hydra_log.h"
#include "assert.h"
#include "sched/sched.h"
#include "int/int.h"
#include "pmalloc/pmalloc.h"
#include "hal/hal_macros.h"
#include "dorm/dorm.h"
#include "utils/utils_bits_and_bobs.h"
#include "fault/fault.h"


#include "memory_map.h"         /* For redirection of const pointers */
#include "trap_api/trap_api.h"
#include "trap_version/trap_version.h"

#if defined(DESKTOP_TEST_BUILD) && TRAPSET_UART
#include "vm/vm_trap.h"
#endif /* (defined(PROCESSOR_P0) || defined(DESKTOP_TEST_BUILD)) && TRAPSET_UART */

#ifdef OS_FREERTOS
#include "FreeRTOS.h"
#include "task.h"
#endif /* OS_FREERTOS */


/**
 * The maximum number of messages to process in one invocation of the receive
 * handler
 *
 * \ingroup ipc_recv_impl
 */
#define IPC_MAX_RECV_MSGS 10

#define hal_set_reg_interproc_event_1 hal_set_reg_p1_to_p0_interproc_event_1
#define hal_set_reg_interproc_event_2 hal_set_reg_p1_to_p0_interproc_event_2

#if IPC_PROTOCOL_ID < 2
/**
 * Type for implementing the queue of pending messages, i.e. those that have
 * been posted for sending but haven't made it into the buffer yet due to a lack
 * of space
 *
 * \ingroup ipc_send_impl
 */
typedef struct IPC_MSG_QUEUE
{
    struct IPC_MSG_QUEUE *next; /**< Linked list impl. */
    IPC_SIGNAL_ID         msg_id; /**< Message ID */
    void                 *msg;    /**< Message body */
    uint16                length_bytes; /**< Message length */
} IPC_MSG_QUEUE;
#endif /* IPC_PROTOCOL_ID < 2 */

#ifdef OS_FREERTOS
/**
 * \brief The size of the stack for the IPC receive task in bytes.
 *
 * Running apps1.fw.env.var.ipc_data.recv_task_stack in pylib should give an
 * idea of how much stack the IPC receive task has used.
 *
 * Whilst 384 bytes currently passes our internal tests it will not run the
 * earbud app.
 *
 * Maximum IPC receive stack size seen on the earbud app was 408 bytes on the
 * master after a device disconnect and reconnect.
 */
#ifndef IPC_RECV_TASK_STACK_BYTES
#define IPC_RECV_TASK_STACK_BYTES (512)
#endif

/**
 * \brief The size of the stack for the IPC receive task in 32-bit words.
 *
 * FreeRTOS stack sizes are specified as a number of StackType_t's, not bytes.
 */
#define IPC_RECV_TASK_STACK_WORDS \
    ((IPC_RECV_TASK_STACK_BYTES + sizeof(StackType_t) - 1)/sizeof(StackType_t))
#endif /* OS_FREERTOS */

/**
 * Top-level storage for IPC internal data
 *
 * \ingroup ipc_impl
 */
typedef struct
{
    /** The send message buffer. */
    IPC_BUFFER *send;

    /** The receive message buffer. */
    IPC_BUFFER *recv;

    /** Leaves the IPC receive buffer pages mapped. */
    bool leave_pages_mapped;

    /** Record the maximum number of bytes used by the ipc_send buffer. */
    uint16 max_send_bytes_used;


#if IPC_PROTOCOL_ID < 2
    /** Linked list of pmalloced messages waiting for send buffer space. */
    IPC_MSG_QUEUE *send_queue;
    /** Internal fg/bg comms. */
    volatile bool pending;
#endif /* IPC_PROTOCOL_ID < 2 */


#ifdef OS_FREERTOS
    /** IPC receive task handle. */
    TaskHandle_t recv_task;

    /** IPC receive task data structure. */
    StaticTask_t recv_task_structure;

    /** IPC receive task stack memory. */
    StackType_t recv_task_stack[IPC_RECV_TASK_STACK_WORDS];
#endif /* OS_FREERTOS */

#ifdef CHIP_DEF_P1_SQIF_SHALLOW_SLEEP_WA_B_195036
    /** Difference between location of p0 code in flash and p1 code in flash.
        Used for translating const pointer from p1 to p0. */
    uint32 p1_pm_flash_offset_from_p0;
#endif /* CHIP_DEF_P1_SQIF_SHALLOW_SLEEP_WA_B_195036 */
} IPC_DATA;

/**
 * IPC implementation data instance
 *
 * \ingroup ipc_impl
 */
extern IPC_DATA ipc_data;

/**
 * Panic interrupt handler.
 *
 * \ingroup ipc_impl
 */
void panic_interrupt_handler(void);

/**
 * Struct used to store panic data.
 *
 * \ingroup ipc_impl
 */
typedef struct
{
    panicid p0_deathbed_confession;
    DIATRIBE_TYPE p0_diatribe;
    TIME p0_t;
    panicid p1_deathbed_confession;
    DIATRIBE_TYPE p1_diatribe;
    TIME p1_t;
} PANIC_DATA;

/**
 * Panic data instance
 *
 * \ingroup ipc_impl
 */
extern PANIC_DATA *panic_data;

/**
 * The first 32-bit value passed from P0 to P1 during IPC initialisation.
 *
 * Picked to be an invalid pointer and byte aligned so an attempt to access it
 * should fail either due to access permission or unaligned access. This is so
 * older firmware versions that don't have a signature + protocol ID will fail
 * early on during boot if P1 and P0 builds aren't matched.
 */
#define IPC_SIGNATURE ((uint32) 0xFF495043U) /* "\xffIPC" */

/**
 * Macro to encapsulate the code for sending a raw pointer in the IPC buffer,
 * rather than a proper IPC message.  Used at initialisation time.
 *
 * \ingroup ipc_send_impl
 */
#define IPC_SEND_POINTER(ptr) \
    do {\
        uint8 *to_px = ipc_buffer_map_write(ipc_data.send);\
        *((uint32 *)to_px) = (uint32)(ptr);\
        ipc_buffer_update_write(ipc_data.send, sizeof(void *));\
    } while (0)

COMPILE_TIME_ASSERT(sizeof(uint32) == sizeof(void *),
                    IPC_SEND_POINTER_relies_on_32_bit_pointers);

/**
 * Macro to encapsulate the code for synchronously receiving a raw pointer in
 * the IPC buffer, rather than via a proper IPC message.  Used at initialisation
 * time.
 *
 * \ingroup ipc_recv_impl
 */
#define IPC_RECV_POINTER(ptr, type) \
    do {\
        const uint8 *from_px = ipc_buffer_map_read(ipc_data.recv);\
        (ptr) = (type *)(*((const uint32 *)from_px));\
        ipc_buffer_update_back(ipc_data.recv, sizeof(void *));\
        ipc_buffer_update_tail_free(ipc_data.recv);\
    } while (0)

/**
 * Macro to encapsulate the code for sending a raw uint32 in the IPC buffer,
 * rather than a proper IPC message.  Used at initialisation time.
 *
 * \ingroup ipc_send_impl
 */
#define IPC_SEND_VALUE(value) \
    do {\
        uint8 *to_px = ipc_buffer_map_write(ipc_data.send);\
        *((uint32 *)to_px) = (value);\
        ipc_buffer_update_write(ipc_data.send, sizeof(uint32));\
    } while (0)

/**
 * Macro to encapsulate the code for synchronously receiving a raw uint32 in
 * the IPC buffer, rather than via a proper IPC message.  Used at initialisation
 * time.
 *
 * \ingroup ipc_recv_impl
 */
#define IPC_RECV_VALUE(value) \
    do {\
        const uint8 *from_px = ipc_buffer_map_read(ipc_data.recv);\
        (value) = *((const uint32 *)from_px);\
        ipc_buffer_update_back(ipc_data.recv, sizeof(uint32));\
        ipc_buffer_update_tail_free(ipc_data.recv);\
    } while (0)

/**
 * Process messages that were sent to this processor before ipc_init completed.
 *
 * Should be called after the IPC interrupt has been enabled.
 *
 * Processor 0 is always booted first so does not need to do anything here.
 *
 * \ingroup ipc_recv_impl
 */
void ipc_recv_messages_sent_before_init(void);

/**
 * Handler for pmalloc and smalloc related IPC signals
 * @param id Signal ID
 * @param msg Message body, including header
 *
 * \ingroup ipc_recv_impl
 */
void ipc_malloc_msg_handler(IPC_SIGNAL_ID id, const void *msg);

/**
 * Handler for test tunnel primitives
 * @param id Signal ID (= \c IPC_SIGNAL_ID_TEST_TUNNEL_PRIM)
 * @param msg Message body, including header
 * @param msg_length_bytes Total length of the message in bytes
 *
 * \ingroup ipc_recv_impl
 */
void ipc_test_tunnel_handler(IPC_SIGNAL_ID id, const void *msg,
                             uint16 msg_length_bytes);

/**
 * Handler for Bluestack primitives
 * @param id Signal ID (= \c IPC_SIGNAL_ID_BLUESTACK_PRIM)
 * @param msg Message body
 *
 * \ingroup ipc_recv_impl
 */
void ipc_bluestack_handler(IPC_SIGNAL_ID id, const void *msg);

/**
 * Handler for app standard messages
 * @param id Signal ID
 * @param msg Message body
 * @param msg_len Length of @p msg in bytes
 *
 * \ingroup ipc_recv_impl
 */
void ipc_trap_api_handler(IPC_SIGNAL_ID id, const void *msg, uint16 msg_len);

/**
 * Handler for the IPC interproc event 1.
 */
void ipc_interrupt_handler(void);

/**
 * @brief Sends the supplied message.
 *
 * The caller must check there is enough space in the buffer to send the
 * message.
 *
 * Used by both single and multitasking IPC implementations.
 *
 * \note This function must be called with interrupts blocked!
 *
 * @param header The message header
 * @param msg Pointer to message body
 * @param len_bytes Length of message body
 */
void ipc_send_no_checks(const IPC_HEADER *header, const void *msg,
                        uint16 len_bytes);

/**
 * @brief Update the IPC receive buffer tail pointer.
 *
 * Either frees the now unused buffer pages or leaves them mapped in depending
 * on the value of @c leave_pages_mapped.
 *
 * @param msg_length The length of the message being freed in bytes.
 */
void ipc_recv_message_free(uint16 msg_length);

/**
 * Send an IPC message passing the trap API version information
 */
void ipc_send_trap_api_version_info(void);

/**
 * Handler for the trap API version information message
 */
void ipc_trap_api_version_prim_handler(IPC_SIGNAL_ID id, const void *prim);

#if IPC_PROTOCOL_ID < 2
/**
 * Blocking receive: shallow sleeps until a message is seen with the supplied
 * ID.
 *
 * Note: the receive handler underlying this call will process everything else
 * it finds in the receive buffer and then return control to this function.
 * Hence non-blocking message handlers should avoid making blocking IPC
 * calls themselves, to avoid inadvertently blocking out any current blocking
 * call for a long time.
 *
 * @param msg_id IPC message to receive.
 * @param blocking_msg Pointer to pre-allocated space for the expected message.
 *                     Must be non-NULL.
 */
void ipc_recv(IPC_SIGNAL_ID msg_id, void *blocking_msg);

/**
 * Post as many messages from the "back-up" queue as possible.
 *
 * \note This function must be called with interrupts blocked!
 *
 * @return TRUE if the queue is now empty, else FALSE
 *
 * \ingroup ipc_send_impl
 */
bool ipc_clear_queue(void);
#endif /* IPC_PROTOCOL_ID < 2 */

/**
 * Handler for scheduler message primitives
 *
 * The ipc_sched_handler function has been removed for IPC_PROTOCOL_ID 2. It
 * allowed Oxygen messages to be sent to queues that were on the other
 * processor. It was used by NFC which is no longer supported and the feature
 * is unimplemented on the FreeRTOS build.
 *
 * @param id Signal ID (= \c IPC_SIGNAL_ID_SCHED_MSG_PRIM)
 * @param msg Message body
 *
 * \ingroup ipc_recv_impl
 */
#if IPC_PROTOCOL_ID == 2
#define ipc_sched_handler(id, msg) \
    do { \
        UNUSED((msg)); \
        panic_diatribe(PANIC_IPC_UNHANDLED_MESSAGE_ID, (id)); \
    } while(0)
#else
void ipc_sched_handler(IPC_SIGNAL_ID id, const void *msg);
#endif


#ifdef DESKTOP_TEST_BUILD
/**
 * Handler for seep sleep related IPC signals
 * @param id Signal ID
 * @param msg Message body, including header
 *
 * \ingroup ipc_recv_impl
 */
void ipc_deep_sleep_msg_handler(IPC_SIGNAL_ID id, const void *msg);
#endif /*  defined(PROCESSOR_P0) || defined(DESKTOP_TEST_BUILD) */

#if defined(DESKTOP_TEST_BUILD) && TRAPSET_UART
/**
 * Handler for uart related messages.
 * @param id Signal ID
 */
void ipc_uart_handler(IPC_SIGNAL_ID id);
#endif /* (defined(PROCESSOR_P0) || defined(DESKTOP_TEST_BUILD)) && TRAPSET_UART */

#if defined(DESKTOP_TEST_BUILD) && TRAPSET_SD_MMC
/**
 * Handler for SD-MMC related messages.
 * @param id Signal ID
 * @param msg Message body
 */
void ipc_sd_mmc_handler(IPC_SIGNAL_ID id, const void *msg);
#endif /* (defined(PROCESSOR_P0) || defined(DESKTOP_TEST_BUILD)) && TRAPSET_SD_MMC */

#if defined(DESKTOP_TEST_BUILD) && TRAPSET_TEST
/**
 * Handler for test trap related messages.
 * \param id Message ID
 * \param msg Message body
 */
void ipc_test_trap_handler(IPC_SIGNAL_ID id, const void *msg);
#endif /* (defined(PROCESSOR_P0) || defined(DESKTOP_TEST_BUILD)) && TRAPSET_TEST */

/**
 * Handler for stream related messages from P0 to P1.
 * @param id Signal ID
 * @param msg Message body
 */
void ipc_stream_handler(IPC_SIGNAL_ID id, const void *msg);


#ifdef ENABLE_APPCMD_TEST_ID_IPC
/**
 * Initialise the IPC Appcmd-based tests.
 *
 * \ingroup ipc_test
 */
void ipc_test_init(void);

#endif /* ENABLE_APPCMD_TEST_ID_IPC */

#ifdef OS_OXYGOS
/**
 * @brief Process a static callback message that's specific to one CPU.
 *
 * @param msg         Pointer to the message.
 * @param msg_length  The length of the message in bytes.
 *
 * @return TRUE if the message was recognised and handled, FALSE otherwise.
 */
bool ipc_recv_process_cpu_static_callback_message(const IPC_HEADER *msg,
                                                  uint16 msg_length);

/**
 * @brief Process an auto-generated message that's specific to one CPU.
 *
 * Panics if the message is not recognised.
 *
 * @param msg         Pointer to the message.
 * @param msg_length  The length of the message in bytes.
 */
void ipc_recv_process_cpu_autogen_message(const IPC_HEADER *msg,
                                          uint16 msg_length);

/**
 * @brief Process a non-blocking response message.
 *
 * @param msg         Pointer to the message.
 * @param msg_length  The length of the message in bytes.
 */
void ipc_recv_process_async_message(const IPC_HEADER *msg, uint16 msg_length);
#endif /* OS_OXYGOS */

#ifdef OS_FREERTOS
/**
 * @brief Create a task for processing received IPC messages.
 *
 * This task should be one of the highest priority tasks in the system.
 * It is higher priority than the VM task so that VM operations can't block
 * IPC. It must be at least as high priority as any task that wishes to use IPC
 * so that IPC responses can be processed.
 */
void ipc_recv_task_create(void);
#endif /* OS_FREERTOS */

#if IPC_PROTOCOL_ID == 2
/**
 * @brief Sends the header and message to the other processor.
 *
 * If there's no space in the IPC send buffer for the message this function will
 * panic with PANIC_IPC_BUFFER_OVERFLOW.
 *
 * @param [in] header  The header for the message. Must not be NULL.
 * @param [in] msg  The message to send. Must not be NULL.
 * @param [in] len_bytes  The length of @p msg in bytes.
 */
void ipc_try_send_common(const IPC_HEADER *header, const void *msg,
                         uint16 len_bytes);
#endif /* IPC_PROTOCOL_ID == 2 */

/**
 * @brief Sets the timestamp field of the IPC header to the current time.
 *
 * @param _header  A pointer to an IPC_HEADER.
 */
#ifdef IPC_ADD_TIMESTAMPS
#define ipc_header_timestamp_set(_header) \
    do \
    { \
        (_header)->timestamp_us = hal_get_time(); \
    } \
    while(0)
#else /* IPC_ADD_TIMESTAMPS */
#define ipc_header_timestamp_set(_header) do { } while(0)
#endif /* IPC_ADD_TIMESTAMPS */

#endif /* IPC_PRIVATE_H_ */

#ifndef __MESSAGE__H__
#define __MESSAGE__H__

/*! file  @brief Message types */
/*!
Message identifier type.
*/
typedef uint16 MessageId;
/*!
Message delay type.
*/
typedef uint32 Delay;
/*!
Message type.
*/
typedef const void *Message;
/*!
Task type.
*/
typedef struct TaskData *Task;
/*!
TaskData type.
*/
typedef struct TaskData { void (*handler)(Task, MessageId, Message);} TaskData;
/*!
MessageQueue type.
*/
typedef uintptr MessageQueue;
/*!
Match function type.
*/
typedef bool (*MessageMatchFn)(Task, MessageId, Message Payload);
/*!
Interpret a MessageQueue as a Task
*/
#define MessageQueueAsTask(_queue) ((Task)(_queue))


#endif

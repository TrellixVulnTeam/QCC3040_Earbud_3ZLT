#ifndef ECHO_PRIVATE_H

#define ECHO_PRIVATE_H

#include <csrtypes.h>
#include <message.h>
#include <domain_message.h>


typedef enum EchoMsgEnum
{
    ECHO_MSG_TEST = INTERNAL_MESSAGE_BASE,
    ECHO_PWM_START,
    ECHO_MSG_MAX
} EchoMsg_t;
ASSERT_INTERNAL_MESSAGES_NOT_OVERFLOWED(ECHO_MSG_MAX)

typedef struct EchoStateDataStruct
{
    TaskData		task;
} EchoStateData_t;

EchoStateData_t* Echo_Get_Data_State(void);

#endif	/* ECHO_PRIVATE_H */

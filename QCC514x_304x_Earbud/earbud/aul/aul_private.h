#ifndef AUL_PRIVATE_H

#define AUL_PRIVATE_H

#include <csrtypes.h>
#include <message.h>
#include <domain_message.h>


typedef enum
{
    AUL_MSG_TEST = INTERNAL_MESSAGE_BASE,
    AUL_PWM_START,

    AUL_MSG_MAX
} aul_msg_t;
ASSERT_INTERNAL_MESSAGES_NOT_OVERFLOWED(AUL_MSG_MAX)

typedef struct aul_state_data
{
    TaskData		task;
} aul_state_data_t;


aul_state_data_t* aul_stateDataGet(void);


#endif	/* AUL_PRIVATE_H */


#include "vmtypes.h"
#include "vm.h"

#include "aul_private.h"
#include "aul_common.h"
#include "aul_pwm.h"

#include "aul_debug.h"


#define AUL_STATE		st_aul_state


static aul_state_data_t st_aul_state;


static void st_aul_msg_test_handle(void)
{
}

static void aul_message_handler(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);

    switch (id)
    {
    case AUL_MSG_TEST:
        AUL_DEBUG_PRINT("AUL_MSG_TEST");
        st_aul_msg_test_handle();
        break;

    case AUL_PWM_START:
        AUL_DEBUG_PRINT("AUL_PWM_START");
        aul_msg_pwm_start_handle();
        break;

    default:
        AUL_DEBUG_PRINT("Unhandled AUL MSG %d", id);
        break;
    }
}


bool aul_init(Task init_task)
{
    UNUSED(init_task);

#ifdef AUL_DEBUG_PRINT_ENABLED
    {
        uint32 p0_ver, p1_ver;

        AUL_DEBUG_PRINT("aul_init()");

        p0_ver = VmGetFwVersion(FIRMWARE_ID);
        p1_ver = VmGetFwVersion(APPLICATION_ID);

        AUL_DEBUG_PRINT("P0_FW_VER: %x, P1_FW_VER: %x", p0_ver, p1_ver);
    }
#endif

    memset(&AUL_STATE, 0x00, sizeof(aul_state_data_t));

    AUL_STATE.task.handler = aul_message_handler;

    aulPwm_init();

    return TRUE;
}

aul_state_data_t* aul_stateDataGet(void)
{
    return &AUL_STATE;
}

Task aul_stateTaskGet(void)
{
    return &(AUL_STATE.task);
}

void aul_testStart(void)
{	
    MessageSendLater(aul_stateTaskGet(), AUL_MSG_TEST, NULL, D_SEC(1));
}


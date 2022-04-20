#include "vmtypes.h"
#include "vm.h"

#include "echo_private.h"
#include "echo_common.h"
#include "echo_pwm.h"

#include "echo_debug.h"

#define ECHO_STATE	st_echo_state

static EchoStateData_t st_echo_state;


static void Echo_State_Msg_Test_Handle(void)
{
}

static void Echo_Msg_Handler(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);

    switch (id)
    {
    case ECHO_MSG_TEST:
        ECHO_DEBUG_PRINT("ECHO_MSG_TEST");
        Echo_State_Msg_Test_Handle();
        break;

    case ECHO_PWM_START:
        ECHO_DEBUG_PRINT("ECHO_PWM_START");
        Echo_Pwm_Start_Handle();
        break;

    default:
        ECHO_DEBUG_PRINT("Unhandled ECHO MSG %d", id);
        break;
    }
}

bool Echo_Init(Task init_task)
{
    UNUSED(init_task);

#ifdef ECHO_DEBUG_PRINT_ENABLED
    {
        uint32 p0_ver, p1_ver;

        ECHO_DEBUG_PRINT("ECHO_Init()");

        p0_ver = VmGetFwVersion(FIRMWARE_ID);
        p1_ver = VmGetFwVersion(APPLICATION_ID);

        ECHO_DEBUG_PRINT("P0_FW_VER: %x, P1_FW_VER: %x", p0_ver, p1_ver);
    }
#endif

    memset(&ECHO_STATE, 0x00, sizeof(EchoStateData_t));

    ECHO_STATE.task.handler = Echo_Msg_Handler;

    Echo_Pwm_Init();

    return TRUE;
}

EchoStateData_t* Echo_Get_Data_State(void)
{
    return &ECHO_STATE;
}

Task ECHO_Get_Task_State(void)
{
    return &(ECHO_STATE.task);
}

void Echo_Test_Start(void)
{	
    MessageSendLater(ECHO_Get_Task_State(), ECHO_MSG_TEST, NULL, D_SEC(1));
}

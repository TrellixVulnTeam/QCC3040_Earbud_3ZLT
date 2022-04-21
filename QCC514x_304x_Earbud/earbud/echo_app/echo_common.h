#ifndef ECHO_COMMON_H

#define ECHO_COMMON_H


bool Echo_Init(Task init_task);

void Echo_Test_Start(void);

void Echo_delay_ms(void);

void Echo_delay_us(void);

void Echo_Set_PIO(void);

Task ECHO_Get_Task_State(void);

#endif	/* ECHO_COMMON_H */

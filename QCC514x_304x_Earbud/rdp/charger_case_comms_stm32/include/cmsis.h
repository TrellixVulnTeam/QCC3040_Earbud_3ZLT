/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      CMSIS.
*/

#ifndef CMSIS_H_
#define CMSIS_H_

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

#ifdef TEST
#define ENABLE_IRQ()
#define DISABLE_IRQ()
#define GET_MSP() (MEM_STACK_END + 100)
#define WFI()
#define NVIC_SYSTEM_RESET()
#else
#define ENABLE_IRQ() __enable_irq()
#define DISABLE_IRQ() __disable_irq()
#define GET_MSP() __get_MSP()
#define WFI() __WFI()
#define NVIC_SYSTEM_RESET() NVIC_SystemReset()
#endif

#endif /* CMSIS_H_ */

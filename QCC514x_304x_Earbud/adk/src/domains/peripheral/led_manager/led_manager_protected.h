/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Protected header file for LED manager

This file defines LED functionality that should only be used for test
purposes.

It provides an API to override LEDs without sending any indications to 
existing users of the LED manager.
*/

#ifndef LED_MANAGER_PROTECTED_H_
#define LED_MANAGER_PROTECTED_H_


/*! \brief Force a specific LED pattern.

    This API should be used for test purposes only. LED manager provides a prioritised
    mechanism for defining LED patterns that should be suitable for all purposes
    other than testing.

    When called, the existing LED pattern (if any) will be replaced. A set bit will 
    turn that specific LED on.

    \param led_mask A bit mask that will be used to override LEDs
*/
void LedManager_ForceLeds(uint16 led_mask);


/*! \brief Stop forcing an LED pattern

    Stop any pattern previously set by LedManager_ForceLeds().

 */
void LedManager_ForceLedsStop(void);


#endif /* LED_MANAGER_PROTECTED_H_ */

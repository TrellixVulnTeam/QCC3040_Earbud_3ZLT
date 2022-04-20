/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Defines the macro which declares a variable to hold the log level for UI module.
*/

#ifndef UI_LOG_LEVEL_H_
#define UI_LOG_LEVEL_H_

/* To enable UI module logging change the UI_USE_LOG_LEVELS to 1
 and include this header file before including any header file(s)
 into C files such as ui_prompts.c, ui_tones.c and etc. of UI module.*/
#define UI_USE_LOG_LEVELS 0

#if UI_USE_LOG_LEVELS
#define DEBUG_LOG_MODULE_NAME ui
#endif /* UI_USE_LOG_LEVELS */

#endif /* UI_LOG_LEVEL_H_ */

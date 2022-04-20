/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       voice_ui_battery.h
\brief      API of the voice UI battery handling.
*/

#ifndef VOICE_UI_BATTERY_H_
#define VOICE_UI_BATTERY_H_

#ifdef HAVE_NO_BATTERY

#define VoiceUi_BatteryInit()

#else

/*! \brief Initialisation of Voice UI battery handling
*/
void VoiceUi_BatteryInit(void);

#endif /* !HAVE_NO_BATTERY */

#endif  /* VOICE_UI_BATTERY_H_ */

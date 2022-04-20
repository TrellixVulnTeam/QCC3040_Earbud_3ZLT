/*!
\copyright  Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for Charger Detection module
*/

#ifndef CHARGER_DETECT_H_
#define CHARGER_DETECT_H_

#include <message.h>
#include <charger_data.h>

/*! Init Charger Detection */
void ChargerDetect_Init(Task task);

/*! Return maximum current that current charger can provide */
unsigned ChargerDetect_Current(void);

/*! Handle charger detected message */
void ChargerDetect_Detected(MessageChargerDetected *msg);
/*! Handle charger changed message */
void ChargerDetect_Changed(MessageChargerChanged *msg);
/*! Handle USB related messages */
void ChargerDetect_UpdateUsbStatus(MessageId id);
/*! Return TRUE if USB is suspend and detected charger respects USB events */
bool ChargerDetect_UsbIsSuspend(void);

/*! Handler for VCHG and current readings */
void ChargerDetect_VchgReading(uint16 voltage_mv, uint16 current_mA);
/*! Notify Charger Detect that fast current limit has changed */
void ChargerDetect_NotifyCurrentChanged(void);

/*! Start monitoring VCHG voltage */
void Charger_VChgMonitorStart(chargerTaskData *data);
/*! Stop monitoring VCHG voltage */
void Charger_VChgMonitorStop(chargerTaskData *data);
/*! Handler for CHARGER_INTERNAL_VCHG_MEASUREMENT message */
void Charger_VchgMonitorPeriodic(chargerTaskData *data);
/*! Handler for MESSAGE_ADC_RESULT message */
void Charger_VChgMonitorReading(chargerTaskData *data, MessageAdcResult *message);

/*! Return detected charger type
 *
 * \return detected charger type */
charger_detect_type ChargerDetect_GetChargerType(void);

#endif /* CHARGER_DETECT_H_ */

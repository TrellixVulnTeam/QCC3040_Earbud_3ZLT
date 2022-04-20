/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    hfp_profile
\brief      Reports battery level over AT+BIEV command.
 
Description of how to use that module. It may contain PlantUML sequence diagrams or state machines.

When TWS is disabled then only local battery level is reported.
When TWS is enabled then lower battery level out of two peers is reported.
 
*/

#ifndef HFP_PROFILE_BATTERY_LEVEL_H_
#define HFP_PROFILE_BATTERY_LEVEL_H_

#include "hfp_profile_typedef.h"
#include <message.h>

#ifdef HAVE_NO_BATTERY

#define HfpProfile_BatteryLevelInit()
#define HfpProfile_HandleBatteryMessages(id, message)
#define HfpProfile_EnableBatteryHfInd(instance, enable_battery_ind)

#else

/*@{*/

/*! \brief Registers hfp task to receive messages required for battery reporting */
void HfpProfile_BatteryLevelInit(void);

/*! \brief Handle messages required for the battery reporting.

    For standalone devices message of interest is:
    MESSAGE_BATTERY_LEVEL_UPDATE_PERCENT - to get updates for local battery level

    For peer devices messages of interest are:
    STATE_PROXY_EVENT - to get updates for battery level of both local and peer battery levels
    CON_MANAGER_TP_DISCONNECT_IND - to determine when remote battery level is not valid anymore

    \param id       Message id.
    \param message  Message payload.
*/
void HfpProfile_HandleBatteryMessages(MessageId id, Message message);

/*! \brief Enable/disable battery reporting.

    This function should be called on reception of HFP_HF_INDICATORS_IND
    when hf_indicator_assigned_num == hf_battery_level.
    Then hf_indicator_status should be passed as enable_battery_ind parameter.

    \param enable_battery_ind Set to true when HF battery indications should be sent,
                              false otherwise.
*/
void HfpProfile_EnableBatteryHfInd(hfpInstanceTaskData *instance, uint8 enable_battery_ind);

/*@}*/

#endif /* !HAVE_NO_BATTERY */

#endif /* HFP_PROFILE_BATTERY_LEVEL_H_ */

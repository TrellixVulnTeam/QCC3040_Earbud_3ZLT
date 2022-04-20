/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    domains
\brief      Interface to Fast Pair battery notification handling.
*/

#ifndef FAST_PAIR_BATTERY_NOTFICATIONS_H
#define FAST_PAIR_BATTERY_NOTFICATIONS_H

#if defined(INCLUDE_CASE_COMMS) || defined (INCLUDE_TWS)

/*! Size of battery notification data used in adverts and bloom filter generation. */
#define FP_BATTERY_NOTFICATION_SIZE   (4)

/*! Offsets to fields in the battery notification data. */
/*! @{ */
#define FP_BATTERY_NTF_DATA_LENGTHTYPE_OFFSET   (0)
#define FP_BATTERY_NTF_DATA_LEFT_STATE_OFFSET   (1)
#define FP_BATTERY_NTF_DATA_RIGHT_STATE_OFFSET  (2)
#define FP_BATTERY_NTF_DATA_CASE_STATE_OFFSET   (3)
/*! @} */

/*! \brief Get pointer to start of the battery notification data for adverts.
    \return uint8* Pointer to the start of the data.
*/
uint8* fastPair_BatteryGetData(void);

#else

/* No battery notification support and therefore no data size. */
#define FP_BATTERY_NOTFICATION_SIZE   (0)
#define fastPair_BatteryGetData()     (0)
#endif


#ifdef INCLUDE_CASE_COMMS

#include <cc_with_case.h>

/*! \brief Handle updated battery states from the case.
*/
void fastPair_BatteryHandleCasePowerState(const CASE_POWER_STATE_T* cps);

/*! \brief Handle updated lid status from the case.
*/
void fastPair_BatteryHandleCaseLidState(const CASE_LID_STATE_T* cls);

#endif

#ifdef INCLUDE_TWS

#include "state_proxy.h"

/*! \brief Handle state proxy events.
    \param[in] sp_event State Proxy event message.
*/
void fastPair_HandleStateProxyEvent(const STATE_PROXY_EVENT_T* sp_event);

#endif

#endif /* FAST_PAIR_BATTERY_NOTFICATIONS_H */

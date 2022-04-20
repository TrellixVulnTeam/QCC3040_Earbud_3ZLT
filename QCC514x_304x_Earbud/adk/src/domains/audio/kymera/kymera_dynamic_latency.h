/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Kymera module to adjust A2DP audio latency dynamically.
            The amount of packets relayed from primary to secondary earbud is
            monitored and the latency is increased relative to the percentage
            of relayed packets. The latency is then adjustment seemlessly by
            the kymera_latency_manager.

            Dynamic adjustment may be enabled using the function
            Kymera_DynamicLatencyEnableDynamicAdjustment().

*/

#ifndef KYMERA_DYNAMIC_LATENCY_H_
#define KYMERA_DYNAMIC_LATENCY_H_

#ifdef INCLUDE_LATENCY_MANAGER

#include <csrtypes.h>

/*! \brief Value (milliseconds) by which Audio Latency is incremented.
   Make sure this value is a factor of Latency range (MAX-MIN) */
#define KYMERA_LATENCY_UPDATE_UP_STEP_MS (50)

/*! \brief Value (milliseconds) by which Audio Latency is incremented.
   Make sure this value is a factor of Latency range (MAX-MIN) */
#define KYMERA_LATENCY_UPDATE_DOWN_STEP_MS (25)

/*! \brief Once the latency has been incremented, and acl stats shows improvement,
   we need to take these number of samples before we can start reducing the 
   latency. This parameter along with KYMERA_LATENCY_UPDATE_DOWN_STEP_US decide
   the rate of reducing latency
*/
#define KYMERA_LATENCY_REDUCTION_DELAY_COUNTER (0)

/*! \brief Periodic delay (in ms) after which Latency Manager checks whether TTP
   Latency should be updated */
#define KYMERA_LATENCY_CHECK_DELAY (500) /* 500 ms */

/*! \brief Value (milliseconds) default Audio Latency */
#define KYMERA_LATENCY_DEFAULT (TWS_STANDARD_LATENCY_MS)

/*! \brief Value (milliseconds) Audio Latency if the LQ goes below a preliminary threshold */
#define KYMERA_LATENCY_INCR_100MS (TWS_STANDARD_LATENCY_MS + 100)

/*! \brief Value (milliseconds) Audio Latency if the LQ goes below a final threshold */
#define KYMERA_LATENCY_INCR_200MS (TWS_STANDARD_LATENCY_MS + 200)


/* Link Quality thresholds to be used for dynamic latency adjustments */
#define LINK_QUALITY_AVERAGE    63500
#define LINK_QUALITY_LOW        62500
#define LINK_QUALITY_CRITICAL   62000


/******************************************************************************
 *  Relay Percentage Thresholds
 *****************************************************************************/
/*! \brief If change in relay % increases by this Threshold, 
   latency is incremented by KYMERA_LATENCY_UPDATE_UP_STEP_MS.
   Should be adjusted based on value of KYMERA_LATENCY_CHECK_DELAY. */
#define KYMERA_LATENCY_PERCENTAGE_THRESHOLD (15)

/*! \brief Minimum amount of change required in relay % before we consider
    increase/decrease of latency.
*/
#define KYMERA_LATENCY_MINIMUM_PERCENT_CHANGE (2)

void Kymera_DynamicLatencyInit(void);

/*! \brief Enables the dynamic latency adjustment feature to operate when A2DP
           is streaming.
*/
void Kymera_DynamicLatencyEnableDynamicAdjustment(void);

/*! \brief Disables the dynamic latency adjustment feature.
*/
void Kymera_DynamicLatencyDisableDynamicAdjustment(void);

/*! \brief Starts periodic latency adjustment if the feature has been enabled using
           the function Kymera_DynamicLatencyEnableDynamicAdjustment. */
void Kymera_DynamicLatencyStartDynamicAdjustment(uint16 base_latency);

/*! \brief Stops periodic latency adjustment. */
void Kymera_DynamicLatencyStopDynamicAdjustment(void);

/*! \brief Query is dynamic latency adjustment is enabled.
    \return TRUE if enabled.
*/
bool Kymera_DynamicLatencyIsEnabled(void);

/*! \brief Handle Latency Timeout. */
void Kymera_DynamicLatencyHandleLatencyTimeout(void);

/*! \brief Marshal dynamic latency data.
    \param buf The buffer to write to.
    \param length The space in the buffer in bytes.
    \return The number of bytes written. Any positive value means marshalling is
    complete.
*/
uint16 Kymera_DynamicLatencyMarshal(uint8 *buf, uint16 length);

/*! \brief Unarshal dynamic latency data.
    \param buf The buffer to read from.
    \param length The data in the buffer in bytes.
    \return The number of bytes read. Any positive value means unmarshalling is
    complete.
*/
uint16 Kymera_DynamicLatencyUnmarshal(const uint8 *buf, uint16 length);

/*! \brief Commit to the new role.
    \param TRUE if the new role is primary
*/
void Kymera_DynamicLatencyHandoverCommit(bool is_primary);

#else
#include "latency_config.h"
#define Kymera_DynamicLatencyInit()
#define Kymera_DynamicLatencyEnableDynamicAdjustment()
#define Kymera_DynamicLatencyDisableDynamicAdjustment()
#define Kymera_DynamicLatencyStartDynamicAdjustment(base_latency) UNUSED(base_latency)
#define Kymera_DynamicLatencyStopDynamicAdjustment()
#define Kymera_DynamicLatencyIsEnabled() FALSE
#define Kymera_DynamicLatencyHandleLatencyTimeout()
#define Kymera_DynamicLatencyHandoverCommit(is_primary)

#endif /* INCLUDE_LATENCY_MANAGER */

#endif /* KYMERA_DYNAMIC_LATENCY_H_ */

/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Interface for HDMA Core Algorithm.
*/

#ifdef INCLUDE_HDMA

#ifndef HDMA_CORE_H
#define HDMA_CORE_H

#ifdef DEBUG_HDMA_UT
#include <stddef.h>
#endif

#include "hdma_queue.h"
#include "hdma_private.h"

/*! Value used to reduce the chance that a random RAM value might affect a test
    system reading the initialised flag */
#define HDMA_CORE_INIT_COMPLETED_MAGIC (0x2D)

typedef uint32 hdma_timestamp; // All timestamps are in ms

/*! Enum used by event messge to invidate the event type */
typedef enum{
    /*! HDMA CORE SCO connection event */
    HDMA_CORE_SCO_CONN = (1 << 9),
    /*! HDMA CORE SCO disconnection event */
    HDMA_CORE_SCO_DISCON = (1 << 10),
    /*! HDMA CORE physical state in ear event for this bud */
    HDMA_CORE_IN_EAR = (1 << 20),
    /*! HDMA CORE physical state out of ear event for this bud */
    HDMA_CORE_OUT_OF_EAR = (1 << 21),
    /*! HDMA CORE physical state in ear event for other bud */
    HDMA_CORE_PEER_IN_EAR = (1 << 22),
    /*! HDMA CORE physical state out of ear event for other bud */
    HDMA_CORE_PEER_OUT_OF_EAR = (1 << 23),
    /*! HDMA CORE physical state in case event for this bud */
    HDMA_CORE_IN_CASE = (1 << 24),
    /*! HDMA CORE physical state out of case event for this bud */
    HDMA_CORE_OUT_OF_CASE = (1 << 25),
    /*! HDMA CORE physical state in case event for other bud */
    HDMA_CORE_PEER_IN_CASE = (1 << 26),
    /*! HDMA CORE physical state out of case event for other bud */
    HDMA_CORE_PEER_OUT_OF_CASE = (1 << 27)
}hdma_core_event_t;

/*! Structure describing rssi quality of the earbud */
typedef struct {
    /*! absolute rssi of bud (dBm)*/
    int8 rssi;
    /*! link quality of bud (0-65535)*/
    uint16 link_quality;
}hdma_core_link_quality_t;


/*! Enum of HDMA handover reasons*/
typedef enum {
    /*! HDMA CORE handover reason invalid. */
    HDMA_CORE_HANDOVER_REASON_INVALID=0,
    /*! HDMA CORE handover reason battery. */
    HDMA_CORE_HANDOVER_REASON_BATTERY_LEVEL,
    /*! HDMA CORE handover reason voice quality. */
    HDMA_CORE_HANDOVER_REASON_VOICE_QUALITY,
    /*! HDMA CORE handover reason signal quality. */
    HDMA_CORE_HANDOVER_REASON_RSSI,
    /*! HDMA CORE handover reason earbud is in case. */
    HDMA_CORE_HANDOVER_REASON_IN_CASE,
    /*! HDMA CORE handover reason earbud is out of ear */
    HDMA_CORE_HANDOVER_REASON_OUT_OF_EAR,
    /*! HDMA CORE handover reason external. */
    HDMA_CORE_HANDOVER_REASON_EXTERNAL,
    /*! HDMA CORE handover reason link quality. */
    HDMA_CORE_HANDOVER_REASON_LINK_QUALITY,
}hdma_core_handover_reason_t;

/*! Enum describing handover urgency */
typedef enum {
    /*! HDMA CORE handover urgency invalid. */
    HDMA_CORE_HANDOVER_URGENCY_INVALID=0,
    /*! HDMA CORE handover urgency low. */
    HDMA_CORE_HANDOVER_URGENCY_LOW,
    /*! HDMA CORE handover urgency high. */
    HDMA_CORE_HANDOVER_URGENCY_HIGH,
    /*! HDMA CORE handover urgency critical. */
    HDMA_CORE_HANDOVER_URGENCY_CRITICAL,
}hdma_core_handover_urgency_t;

/*! Enum Current state of the device battery. */
typedef enum
{
    /*! HDMA CORE battery level unknown. */
    HDMA_CORE_BATTERY_UNKNOWN,
    HDMA_CORE_BATTERY_UNSAFE,
    HDMA_CORE_BATTERY_CRITICAL,
    HDMA_CORE_BATTERY_OK
} hdma_core_battery_state_t;

/*! Structure to hold all information pertinent to handover */
typedef struct {
    /*! HDMA CORE handover result (true/false) */
    uint8 handover;
    /*! HDMA CORE handover reason */
    hdma_core_handover_reason_t reason;
    /*! HDMA CORE handover urgency */
    hdma_core_handover_urgency_t urgency;
}hdma_core_handover_result_t;

/*! Structure to hold all information pertinent to a single earbud */
typedef struct{
    /*! Earbud in ear or out of ear timestamp */
    hdma_timestamp inOutTransitionTime;
    /*! Earbud in ear last timestamp */
    hdma_timestamp lastTimeInEar;
    /*! TRUE when earbud is in ear and FALSE when out of ear */
    uint8 inEar;
    /*! TRUE when earbud is in case and FALSE when out of case */
    uint8 inCase;
#ifdef INCLUDE_HDMA_BATTERY_EVENT
    /*! Earbud Battery status */
    hdma_core_battery_state_t batteryStatus;
#endif
#ifdef INCLUDE_HDMA_MIC_QUALITY_EVENT
    /*! Queue contains voice quality of earbud */
    queue_t voiceQuality;
#endif
#ifdef INCLUDE_HDMA_RSSI_EVENT
    /*! Queue contains rssi quality of earbud */
    queue_t phoneRSSI;
#endif
#ifdef INCLUDE_HDMA_LINK_QUALITY_EVENT
    /*! Queue contains rssi quality of earbud */
    queue_t linkQuality;
#endif
}hdma_bud_info_t;

/*! Structure to hold all information pertinent to HDMA core state */
typedef struct{
    /*!< Flag used to indicate that the full initialisation has completed */
    uint8 initialised;
    /*!< Handover results */
    hdma_core_handover_result_t hdma_result;
    /*!< Status for active/inactive call */
    uint8 inCall;
    /*!< Time at which last handover was attempted */
    hdma_timestamp lastHandoverAttempt;
    /*!< Time at which event was processed */
    hdma_timestamp timestamp;  
    /*!< Peer earbud information */
    hdma_bud_info_t remote_bud;
    /*!< Local earbud information */
    hdma_bud_info_t local_bud;
    /*!< Lowest LQ information */
    uint16 lowestLQ;
}hdma_core_data_t;

/*! \brief Checks whether local earbud is out of ear

    \param[out] Returns True if local earbud is out of ear
*/
bool Hdma_IsOutOfEarEnabled(void);

/*! \brief Handle internal event from HDMA

    \param[in] timestamp Time at which event is received.
*/
void Hdma_CoreHandleInternalEvent( hdma_timestamp timestamp);

/*! \brief Handle the phy state and call event from HDMA

    \param[in] timestamp Time at which event is received.
    \param[in] event phy state or call event.
*/
void Hdma_CoreHandleEvent( hdma_timestamp timestamp, hdma_core_event_t event);

/*! \brief Get the lowest LQ out of 2 EBs, otherwise return 0xffff

    \return Lowest LQ value or, 0xffff
*/
uint16 Hdma_getLowestLinkQuality(void);


#ifdef INCLUDE_HDMA_BATTERY_EVENT
/*! \brief Handle the battery level status event from HDMA

    \param[in] timestamp Time at which event is received.
    \param[in] isThisBud source of the event (true:-this bud, false:- peer bud).
    \param[in] batteryStatus current battery level of battery.

*/
void Hdma_CoreHandleBatteryStatus(hdma_timestamp timestamp, uint8 isThisBud, hdma_core_battery_state_t batteryStatus);
#endif

#ifdef INCLUDE_HDMA_MIC_QUALITY_EVENT
/*! \brief Handle the voice quality event from HDMA.
           This event will be raised only during an active  HFP call.

    \param[in] timestamp Time at which event is raised.
    \param[in] isThisBud source of the event (true:-this bud, false:- peer bud).
    \param[in] voiceQuality voice quality indicator, 0 = worst, 15 = best, 0xFF unknown. 

*/
void Hdma_CoreHandleVoiceQuality(hdma_timestamp timestamp, uint8 isThisBud, uint8 voiceQuality);      
#endif

#if defined(INCLUDE_HDMA_RSSI_EVENT) || defined(INCLUDE_HDMA_LINK_QUALITY_EVENT)
/*! \brief Handle the link quality event from HDMA

    \param[in] timestamp Time at which event is received.
    \param[in] isThisBud source of the event (true:-this bud, false:- peer bud).
    \param[in] linkQuality link quality indicator (rssi, link_quality)
*/
void Hdma_CoreHandleLinkQuality(hdma_timestamp timestamp,uint8 isThisBud, hdma_core_link_quality_t linkQuality);

/*! \brief Handle the mirroring ACL link connection event

    \param[in] msg Message holds mirroring device details.
*/
void Hdma_CoreHandleMirrorAclConnectionInd(const CON_MANAGER_TP_CONNECT_IND_T* msg);

#endif
/*! \brief This function will force handover with specified urgency

    \param[in] timestamp Time at which event is received. Timestamp is in ms.
    \param[in] urgency urgency of handover.
*/
uint8 Hdma_CoreHandleExternalReq(hdma_timestamp timestamp, hdma_core_handover_urgency_t urgency)  ;

/*! \brief This function will destroy HDMA core and earbud information

*/
void Hdma_CoreDestroy(void);

/*! \brief This function will initialize HDMA core data and earbud information. Will initialize bud info with current values fetched from StateProxy

    \return Success or Fail
*/
uint8 Hdma_CoreInit(void);

#ifdef DEBUG_HDMA_UT
/*! Required only for UT testing */

typedef struct {
    int size;
    int data[BUFFER_LEN][2];  // 0: Data, 1: Timestamp
}hdma_core_result_queue_t;

typedef struct {
    hdma_core_battery_state_t batteryStatus;
    int debugLevel;
    uint8 inCase;
    int8 inEar;
    hdma_timestamp lastTimeInEar; //check for time
    hdma_core_result_queue_t peerRSSI;
    hdma_core_result_queue_t phoneRSSI;
    hdma_core_result_queue_t voiceQuality;
}hdma_core_result_bud_t;

typedef struct {
    int dbgLevel;
    uint8 inCall;
    hdma_timestamp lastHandoverAttempt;
    hdma_core_result_bud_t other_bud;
    hdma_core_handover_result_t hdma_result;
    hdma_core_result_bud_t local_bud;
    hdma_timestamp timestamp;
}hdma_core_result_data_t;

/*! \brief This function returns complete HDMA core state for UT

    \return hdma_core_result_data_t Structure having HDMA core state
*/

hdma_core_result_data_t Hdma_GetCoreHdmaData(void);

void Hdma_PopulateQueueResult(queue_t *queue, hdma_core_result_queue_t *result, queue_type_t type);

#endif /* DEBUG_HDMA_UT */
#endif /* HDMA_CORE_H */

#endif /* INCLUDE_HDMA */

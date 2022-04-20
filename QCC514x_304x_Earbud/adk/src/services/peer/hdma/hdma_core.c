/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      HDMA Core Algorithm implementation.
*/

#ifdef INCLUDE_HDMA

#include <stdlib.h>
#include <string.h>
#include "hdma_core.h"
#include "hdma_utils.h"
#include "hdma_queue.h"
#ifndef DEBUG_HDMA_UT
#include "hdma_client_msgs.h"
#include "state_proxy.h"
#include <panic.h>
#endif

#ifdef DEBUG_HDMA_UT
#include "types.h"
#endif

/*! \todo remove unused after development */
#pragma unitsuppress Unused
static int16 hdma_Filter(hdma_timestamp timestamp, queue_t *queue, int16 halfLife_ms,int16 maxAge_ms, queue_type_t type);
#ifdef INCLUDE_HDMA_MIC_QUALITY_EVENT
static void hdma_ValidateVoiceQuality(hdma_timestamp timestamp,hdma_core_handover_urgency_t *urgency, hdma_core_handover_urgency_t *suppressUrgency );
#endif
#ifdef INCLUDE_HDMA_RSSI_EVENT
static hdma_core_handover_urgency_t hdma_ValidateLink(hdma_timestamp timestamp);
#endif
#ifdef INCLUDE_HDMA_LINK_QUALITY_EVENT
static void hdma_ValidateLinkQuality(hdma_timestamp timestamp, hdma_core_handover_urgency_t *urgency, hdma_core_handover_urgency_t *suppressUrgency );
#endif
static void hdma_SetHandoverEvent(hdma_core_handover_result_t newResult);
static void hdma_StateUpdate(hdma_timestamp timestamp);
static uint8 hdma_BudInit(hdma_bud_info_t *bud_info);
static uint8 hdma_BudGetInEar(hdma_bud_info_t bud_info);
static hdma_core_handover_result_t hdma_MergeResult(hdma_core_handover_result_t resOrig, hdma_core_handover_result_t resNew);

/*! Instance of the HDMA Core. */
hdma_core_data_t *hdma_core_data;

/*! \brief Print physical state of both earbuds

*/
static void hdma_CorePrintState(void)
{
        HDMA_DEBUG_LOG_INFO("Local bud state - In Case: %d, In Ear: %d",
                   hdma_core_data->local_bud.inCase, hdma_core_data->local_bud.inEar);
        HDMA_DEBUG_LOG_INFO("Remote bud state - In Case: %d, In Ear: %d",
                   hdma_core_data->remote_bud.inCase, hdma_core_data->remote_bud.inEar);
}

/*! \brief Returns object containing the handover result based on reason and urgency

    \param[in] reason Handover Reason.
    \param[in] urgency Handover urgency.
*/
static hdma_core_handover_result_t hdma_NewResult(hdma_core_handover_reason_t reason, hdma_core_handover_urgency_t urgency)
{
    hdma_core_handover_result_t result;
    result.handover = (reason != HDMA_CORE_HANDOVER_REASON_INVALID) && (urgency != HDMA_CORE_HANDOVER_URGENCY_INVALID);
    result.reason = reason;
    result.urgency = urgency;
    return result;
}

uint8 Hdma_CoreInit(void)
{

    HDMA_DEBUG_LOG("Hdma_CoreInit");
#ifndef DEBUG_HDMA_UT
    if(hdma_core_data && hdma_core_data->initialised == HDMA_CORE_INIT_COMPLETED_MAGIC)
    {
        DEBUG_LOG_ERROR("Hdma_CoreInit: HDMA already initialised");
        return FALSE;
    }
#endif
    hdma_core_data = HDMA_MALLOC(hdma_core_data_t);
    /* Initialise local and remote bud state*/
    hdma_BudInit(&(hdma_core_data->local_bud));
    hdma_BudInit(&(hdma_core_data->remote_bud));
#ifndef DEBUG_HDMA_UT
#ifdef INCLUDE_HDMA_BATTERY_EVENT
    StateProxy_GetLocalAndRemoteBatteryStates(&(hdma_core_data->local_bud.batteryStatus),&(hdma_core_data->remote_bud.batteryStatus));
#endif
    hdma_core_data->local_bud.inCase = StateProxy_IsInCase();
    hdma_core_data->local_bud.inEar = StateProxy_IsInEar();
    hdma_core_data->remote_bud.inCase = StateProxy_IsPeerInCase();
    hdma_core_data->remote_bud.inEar = StateProxy_IsPeerInEar();
#endif
    hdma_core_data->timestamp = INVALID_TIMESTAMP;
    hdma_core_data->lastHandoverAttempt = INVALID_TIMESTAMP;
    hdma_core_data->inCall = MirrorProfile_IsEscoActive();

    hdma_core_data->hdma_result = hdma_NewResult(HDMA_CORE_HANDOVER_REASON_INVALID,HDMA_CORE_HANDOVER_URGENCY_INVALID);
    hdma_core_data->initialised = HDMA_CORE_INIT_COMPLETED_MAGIC;
    hdma_core_data->lowestLQ = 0xffff;
    hdma_CorePrintState();
    return TRUE;
}

void Hdma_CoreDestroy(void)
{
    HDMA_DEBUG_LOG("Hdma_CoreDestroy");

    if(hdma_core_data && hdma_core_data->initialised == HDMA_CORE_INIT_COMPLETED_MAGIC)
    {
        free(hdma_core_data);
        hdma_core_data = NULL;
    }
}


void Hdma_CoreHandleEvent( hdma_timestamp timestamp, hdma_core_event_t event)
{
    HDMA_DEBUG_LOG("Hdma_CoreHandleEvent: Timestamp = %u", timestamp);

    if(hdma_BudGetInEar(hdma_core_data->local_bud))
    {
        hdma_core_data->local_bud.lastTimeInEar = timestamp;
    }

    if(hdma_BudGetInEar(hdma_core_data->remote_bud))
    {
        hdma_core_data->remote_bud.lastTimeInEar = timestamp; // To check
    }

    switch(event)
    {
        case HDMA_CORE_IN_CASE:
            hdma_core_data->local_bud.inCase = TRUE;
        break;
        case HDMA_CORE_OUT_OF_CASE:
            hdma_core_data->local_bud.inCase = FALSE;
            break;
        case HDMA_CORE_PEER_IN_CASE:
            hdma_core_data->remote_bud.inCase = TRUE;
            break;
        case HDMA_CORE_PEER_OUT_OF_CASE:
            hdma_core_data->remote_bud.inCase = FALSE;
            break;
        case HDMA_CORE_IN_EAR:
            if(!hdma_BudGetInEar(hdma_core_data->local_bud))
            {
                hdma_core_data->local_bud.inOutTransitionTime = timestamp;
            }
            hdma_core_data->local_bud.inEar = TRUE;
            break;
        case HDMA_CORE_OUT_OF_EAR:
            if(hdma_BudGetInEar(hdma_core_data->local_bud))
            {
                hdma_core_data->local_bud.inOutTransitionTime = timestamp;
            }
            hdma_core_data->local_bud.inEar = FALSE;
            break;
        case HDMA_CORE_PEER_IN_EAR:
            if(!hdma_BudGetInEar(hdma_core_data->remote_bud))
            {
                hdma_core_data->remote_bud.inOutTransitionTime = timestamp;
            }
            hdma_core_data->remote_bud.inEar = TRUE;
            break;
        case HDMA_CORE_PEER_OUT_OF_EAR:
            if(hdma_BudGetInEar(hdma_core_data->remote_bud))
            {
                hdma_core_data->remote_bud.inOutTransitionTime = timestamp;
            }
            hdma_core_data->remote_bud.inEar = FALSE;
            break;
        case HDMA_CORE_SCO_CONN:
            DEBUG_LOG("HDMA_CORE_SCO_CONN [%u]", timestamp);
            hdma_core_data->inCall = TRUE;
            break;
        case HDMA_CORE_SCO_DISCON:
            DEBUG_LOG("HDMA_CORE_SCO_DISCON [%u]", timestamp);
            hdma_core_data->inCall = FALSE;
            break;
    }
    hdma_StateUpdate(timestamp);
}

uint16 Hdma_getLowestLinkQuality(void)
{
    if(hdma_core_data && hdma_core_data->lowestLQ != 0)
    {
        return  hdma_core_data->lowestLQ;
    }

    return 0xffff;
}


/*! \brief Handle internal event from HDMA

    \param[in] timestamp Time at which event is received.
*/
void Hdma_CoreHandleInternalEvent(hdma_timestamp timestamp)
{
    hdma_StateUpdate(timestamp);
}

/*! \brief Checks whether local earbud is out of ear

    \param[out] Returns True if local earbud is out of ear
*/
bool Hdma_IsOutOfEarEnabled(void)
{
    return !(hdma_BudGetInEar(hdma_core_data->local_bud));
}

/*! \brief Main update function: on the basis of the available data look at the different possible reasons for a handover.  Order of assessment is unimportant due to the merging.

    \param[in] timestamp Time at which event is received.
*/
static void hdma_StateUpdate(hdma_timestamp timestamp)
{
    hdma_core_handover_result_t result = hdma_NewResult(HDMA_CORE_HANDOVER_REASON_INVALID, HDMA_CORE_HANDOVER_URGENCY_INVALID);
    /*  Prevent update being called more often than necessary, to avoid unnecessary CPU */
    if ((hdma_core_data->timestamp != INVALID_TIMESTAMP) && (timestamp - hdma_core_data->timestamp < MIN_UPDATE_INT_MS))
    {
        hdma_SetHandoverEvent(result);
        return;
    }

    HDMA_DEBUG_LOG("hdma_StateUpdate: Timestamp [%u]", timestamp);
    hdma_CorePrintState();

    hdma_core_data->timestamp = timestamp;

    /*  Main logic for HDMA decision making */

    /*  Case (1) bud is in case */
    if((hdma_core_data->local_bud.inCase) && (!hdma_core_data->remote_bud.inCase))
    {
        result = hdma_MergeResult(result, hdma_NewResult(HDMA_CORE_HANDOVER_REASON_IN_CASE, HDMA_CORE_HANDOVER_URGENCY_CRITICAL));
    }
#ifdef INCLUDE_HDMA_BATTERY_EVENT
    /*  Case (2) battery is critical    and peer is known and not critical */
    if(   (hdma_core_data->local_bud.batteryStatus == HDMA_CORE_BATTERY_CRITICAL)
       && (hdma_core_data->remote_bud.batteryStatus != HDMA_CORE_BATTERY_CRITICAL)
       && (hdma_core_data->remote_bud.batteryStatus != HDMA_CORE_BATTERY_UNKNOWN))
    {
        /*  If the primary is in and the secondary is out then we do not change even though the battery is critical */
        if(hdma_BudGetInEar(hdma_core_data->local_bud) && (!hdma_BudGetInEar(hdma_core_data->remote_bud)))
        {
            HDMA_DEBUG_LOG("hdma_StateUpdate: Timestamp [%u]: suppress handover, critical battery but secondary is out", timestamp);
        }
        else
        {
            result = hdma_MergeResult(result, hdma_NewResult(HDMA_CORE_HANDOVER_REASON_BATTERY_LEVEL, HDMA_CORE_HANDOVER_URGENCY_HIGH));
        }
    }
#endif

    /*  If the devices are in ear then update the times  */
    if (hdma_BudGetInEar(hdma_core_data->local_bud))
    {
        hdma_core_data->local_bud.lastTimeInEar = timestamp;
    }

    if (hdma_BudGetInEar(hdma_core_data->remote_bud))
    {
        hdma_core_data->remote_bud.lastTimeInEar = timestamp;
    }

    /*  Case (3) primary is out of ear, secondary is in ear */
    if ((!hdma_BudGetInEar(hdma_core_data->local_bud)) && hdma_BudGetInEar(hdma_core_data->remote_bud))
    {
        if ((hdma_core_data->local_bud.lastTimeInEar == INVALID_TIMESTAMP)||
                ((timestamp - hdma_core_data->local_bud.lastTimeInEar) >= OUT_OF_EAR_TIME_BEFORE_HANDOVER_MS))
        {
            result = hdma_MergeResult(result, hdma_NewResult(HDMA_CORE_HANDOVER_REASON_OUT_OF_EAR, HDMA_CORE_HANDOVER_URGENCY_HIGH));
        }
    }
    hdma_core_handover_urgency_t urgency, suppressUrgency;
#if defined(INCLUDE_HDMA_RSSI_EVENT) || defined(INCLUDE_HDMA_LINK_QUALITY_EVENT)
    /*  Case (4) RSSIs/LQs: skip this if the secondary is unable to be handed over to    */
    uint8 skipRSSIChk = ((hdma_BudGetInEar(hdma_core_data->local_bud) && (!hdma_BudGetInEar(hdma_core_data->remote_bud))) || (hdma_core_data->remote_bud.inCase)
#ifdef INCLUDE_HDMA_BATTERY_EVENT
                         || (hdma_core_data->remote_bud.batteryStatus == HDMA_CORE_BATTERY_CRITICAL));
#else
                         );
#endif
#endif
#ifdef INCLUDE_HDMA_RSSI_EVENT
    if (!skipRSSIChk)
    {
        /* Check urgency and suppress urgency levels for RSSI quality based handover. */
        urgency = hdma_ValidateLink(timestamp);
        if (urgency != HDMA_CORE_HANDOVER_URGENCY_INVALID)
        {
            result = hdma_MergeResult(result, hdma_NewResult(HDMA_CORE_HANDOVER_REASON_RSSI, urgency));
        }
    }
#endif
#ifdef INCLUDE_HDMA_LINK_QUALITY_EVENT
    uint8 suppressLQ = FALSE;
    if (!skipRSSIChk)
    {
        /* Check urgency and suppress urgency levels for link quality based handover. */
        hdma_ValidateLinkQuality(timestamp, &urgency, &suppressUrgency);

        if (urgency != HDMA_CORE_HANDOVER_URGENCY_INVALID)
        {
            HDMA_DEBUG_LOG("hdma_StateUpdate: Link Quality Urgency [%d]", urgency);

            /* Check if there is any recommendations from RSSI handover */
            if(result.reason == HDMA_CORE_HANDOVER_REASON_RSSI)
            {
                if(result.urgency == HDMA_CORE_HANDOVER_URGENCY_CRITICAL && urgency > HDMA_CORE_HANDOVER_URGENCY_INVALID)
                {
                    /* Let the result be driven by RSSI , don't merge */
                }
                else if (urgency == HDMA_CORE_HANDOVER_URGENCY_CRITICAL)
                {
                    /* Even if the RSSI is not at a critical state but LQ is surpassing the limits lets do an Handover*/
                    result = hdma_MergeResult(result, hdma_NewResult(HDMA_CORE_HANDOVER_REASON_LINK_QUALITY, urgency));
                }
                else
                {
                    /* Neither RSSI nor LQ is critical, don't bother about doing handover */
                    result = hdma_NewResult(HDMA_CORE_HANDOVER_REASON_INVALID, HDMA_CORE_HANDOVER_URGENCY_INVALID);
                }
            }
            else if(urgency == HDMA_CORE_HANDOVER_URGENCY_CRITICAL)
            {
                /* Result driven from LQ */
                result = hdma_MergeResult(result, hdma_NewResult(HDMA_CORE_HANDOVER_REASON_LINK_QUALITY, urgency));
            }
            else
            {
                /* Don't Handover unless Critical, let the Handover be driver by other parameters */
            }
        }
        else if(urgency == HDMA_CORE_HANDOVER_URGENCY_INVALID)
        {
            /* LQ does not recommened Handover so, cancel RSSI recommendations too */
            if(result.reason == HDMA_CORE_HANDOVER_REASON_RSSI)
            {
                result = hdma_NewResult(HDMA_CORE_HANDOVER_REASON_INVALID, HDMA_CORE_HANDOVER_URGENCY_INVALID);
            }
        }

        suppressUrgency = HDMA_CORE_HANDOVER_URGENCY_INVALID;

        /*  If suppression is active then prevent handover unless it meets or exceeds urgency of the suppression    */
        if (suppressUrgency == HDMA_CORE_HANDOVER_URGENCY_CRITICAL)
        {
            suppressLQ = (result.urgency == HDMA_CORE_HANDOVER_URGENCY_HIGH) || (result.urgency == HDMA_CORE_HANDOVER_URGENCY_LOW);
        }
        else if (suppressUrgency == HDMA_CORE_HANDOVER_URGENCY_HIGH)
        {
            suppressLQ = (result.urgency == HDMA_CORE_HANDOVER_URGENCY_LOW);
        }

        /*  Apply suppression if necessary  */
        if (suppressLQ)
        {
            HDMA_DEBUG_LOG_INFO("Timestamp [%u]: Suppress Handover: reason [%d] urgency [%d] due to voice call", hdma_core_data->timestamp, hdma_core_data->hdma_result.reason, hdma_core_data->hdma_result.urgency);

            result = hdma_NewResult(HDMA_CORE_HANDOVER_REASON_INVALID, HDMA_CORE_HANDOVER_URGENCY_INVALID);
        }
    }
#endif
#ifdef INCLUDE_HDMA_MIC_QUALITY_EVENT
    /*  Case (6) Mic quality during a voice call    */
    uint8 skipVoiceChk = (!hdma_core_data->inCall) || (hdma_core_data->remote_bud.inCase);
    uint8 suppressMic = FALSE;

    HDMA_DEBUG_LOG("hdma_StateUpdate: inCall [%d] inCase [%d]", hdma_core_data->inCall, hdma_core_data->remote_bud.inCase);
    if (!skipVoiceChk)
    {
        /* Check urgency and suppress urgency levels for mic quality based handover. */
        hdma_ValidateVoiceQuality(timestamp, &urgency, &suppressUrgency);

        HDMA_DEBUG_LOG("hdma_StateUpdate: hdma_ValidateVoiceQuality suppressUrgency [%d] urgency [%d]",suppressUrgency, urgency);

        if (urgency != HDMA_CORE_HANDOVER_URGENCY_INVALID)
        {
            result = hdma_MergeResult(result, hdma_NewResult(HDMA_CORE_HANDOVER_REASON_VOICE_QUALITY, urgency));
        }

        /*  If suppression is active then prevent handover unless it meets or exceeds urgency of the suppression    */
        if (suppressUrgency == HDMA_CORE_HANDOVER_URGENCY_CRITICAL)
        {
            suppressMic = (result.urgency == HDMA_CORE_HANDOVER_URGENCY_HIGH) || (result.urgency == HDMA_CORE_HANDOVER_URGENCY_LOW) || (result.urgency == HDMA_CORE_HANDOVER_URGENCY_INVALID);
        }
        else if (suppressUrgency == HDMA_CORE_HANDOVER_URGENCY_HIGH)
        {
            suppressMic = (result.urgency == HDMA_CORE_HANDOVER_URGENCY_LOW) || (result.urgency == HDMA_CORE_HANDOVER_URGENCY_INVALID);
        }

        /*  Apply suppression if necessary  */
        if (suppressMic)
        {
            HDMA_DEBUG_LOG_INFO("Timestamp [%u]: Suppress Handover: reason [%d] urgency [%d] due to voice call", hdma_core_data->timestamp, hdma_core_data->hdma_result.reason, hdma_core_data->hdma_result.urgency);

            result = hdma_NewResult(HDMA_CORE_HANDOVER_REASON_INVALID, HDMA_CORE_HANDOVER_URGENCY_INVALID);
        }
    }
#endif
    /* Handle external handover */
    if(!result.handover && hdma_core_data->hdma_result.reason == HDMA_CORE_HANDOVER_REASON_EXTERNAL)
    {
        result = hdma_MergeResult(result, hdma_NewResult(hdma_core_data->hdma_result.reason, hdma_core_data->hdma_result.urgency));
    }
    hdma_SetHandoverEvent(result);
}

#ifdef INCLUDE_HDMA_BATTERY_EVENT
void Hdma_CoreHandleBatteryStatus(hdma_timestamp timestamp, uint8 isThisBud, hdma_core_battery_state_t batteryStatus)
{
    HDMA_DEBUG_LOG("Hdma_CoreHandleBatteryStatus: Timestamp [%u] batteryStatus [%d]", (uint32)timestamp, batteryStatus);

    if (isThisBud)
    {
        hdma_core_data->local_bud.batteryStatus = batteryStatus;
    }
    else
    {
        hdma_core_data->remote_bud.batteryStatus = batteryStatus;
    }
    hdma_StateUpdate(timestamp);
}
#endif

uint8 Hdma_CoreHandleExternalReq(hdma_timestamp timestamp,hdma_core_handover_urgency_t urgency)
{
    hdma_core_handover_result_t handover = hdma_NewResult(HDMA_CORE_HANDOVER_REASON_EXTERNAL, urgency);
    hdma_core_data->timestamp = timestamp;
    hdma_SetHandoverEvent(handover);
    return TRUE;
}

/*! \brief Use this function to set the result.  It will prevent excessive numbers of handover requests being generated and ensure the time of last handover is updated

    \param[in] newResult Handover new result
*/
static void hdma_SetHandoverEvent(hdma_core_handover_result_t newResult)
{
    uint16 suppressionPeriod;
    if(!newResult.handover)
    {
        DEBUG_LOG_ERROR("Handover is false [%d]\n", newResult.handover);

        if(hdma_core_data->hdma_result.handover != newResult.handover)
        {
            DEBUG_LOG_ERROR("Old Handover [%d] New handover [%d]", hdma_core_data->hdma_result.handover, newResult.handover);
        }
        #ifndef DEBUG_HDMA_UT
        hdma_NotifyHandoverClients(HDMA_CANCEL_HANDOVER_NOTIFICATION, hdma_core_data->timestamp, HDMA_HANDOVER_REASON_INVALID, HDMA_HANDOVER_URGENCY_INVALID);
        #endif

        hdma_core_data->hdma_result = newResult;
        return;
    }
    /* we are attempting handover   */
    if (newResult.urgency == HDMA_CORE_HANDOVER_URGENCY_LOW)
    {
        suppressionPeriod = MIN_HANDOVER_RETRY_TIME_LOW_MS;
    }
    else if (newResult.urgency == HDMA_CORE_HANDOVER_URGENCY_HIGH)
    {
        suppressionPeriod = MIN_HANDOVER_RETRY_TIME_HIGH_MS;
    }
    else
    {
        suppressionPeriod = MIN_HANDOVER_RETRY_TIME_CRITICAL_MS;
    }

    HDMA_DEBUG_LOG_INFO("hdma_SetHandoverEvent: hdma_core_data->lastHandoverAttempt [%u]", hdma_core_data->lastHandoverAttempt);
    /*  we want to handover but have recently generated a handover event: dont update result    */
    if (newResult.handover && (hdma_core_data->lastHandoverAttempt != INVALID_TIMESTAMP) && ((hdma_core_data->timestamp - hdma_core_data->lastHandoverAttempt) < suppressionPeriod))
    {
        DEBUG_LOG_ERROR("Timestamp [%u]: Suppress Handover: reason [%d] due to recently generated handover event, urgency [%d]", hdma_core_data->timestamp, hdma_core_data->hdma_result.reason, hdma_core_data->hdma_result.urgency);

        return ;
    }
    /*  update result, generating event and set time of last handover attempt to now    */
    hdma_core_data->hdma_result = newResult;
    hdma_core_data->lastHandoverAttempt = hdma_core_data->timestamp;

    HDMA_DEBUG_LOG_INFO("Timestamp [%u]: Handover: reason = %d , urgency = %d.", hdma_core_data->timestamp, hdma_core_data->hdma_result.reason, hdma_core_data->hdma_result.urgency);

    #ifndef DEBUG_HDMA_UT
    hdma_NotifyHandoverClients(HDMA_HANDOVER_NOTIFICATION, hdma_core_data->timestamp, hdma_core_data->hdma_result.reason, hdma_core_data->hdma_result.urgency);
    #endif
}

/*! \brief Allow more urgent handovers to overwrite any other ones.

    \param[in] resOrig Original handover result
    \param[in] resNew New handover result
    \param[out] Returns more urgent handover result among inputs
*/
static hdma_core_handover_result_t hdma_MergeResult(hdma_core_handover_result_t resOrig, hdma_core_handover_result_t resNew)
{
    /*  allow only more urgent handovers to overwrite any other ones    */
    if (resOrig.urgency >= resNew.urgency)
    {
        return resOrig;
    }
    else
    {
        return resNew;
    }
}

/*! \brief This function will initialize Earbud information. Will initialize bud info with current values fetched from StateProxy

    \return Success or Fail
*/
static uint8 hdma_BudInit(hdma_bud_info_t *bud_info)
{
    memset(bud_info, 0, sizeof(hdma_bud_info_t));
#ifdef INCLUDE_HDMA_MIC_QUALITY_EVENT
    Hdma_QueueCreate(&(bud_info->voiceQuality));
#endif
#ifdef INCLUDE_HDMA_RSSI_EVENT
    Hdma_QueueCreate(&(bud_info->phoneRSSI));
#endif
#ifdef INCLUDE_HDMA_LINK_QUALITY_EVENT
    Hdma_QueueCreate(&(bud_info->linkQuality));
#endif

#ifdef INCLUDE_HDMA_BATTERY_EVENT
    bud_info->batteryStatus = HDMA_CORE_BATTERY_UNKNOWN;
#endif
    bud_info->lastTimeInEar = INVALID_TIMESTAMP;
    bud_info->inEar = HDMA_UNKNOWN;
    bud_info->inCase = FALSE;
    return TRUE;
}

/*! \brief method through which the in/out status should always be determined as it handles fallback if sensors are missing.

    \param[in] bud_info Earbud information
    \param[out] Whether earbud is in ear or not
*/
static uint8 hdma_BudGetInEar(hdma_bud_info_t bud_info)
{
    if (bud_info.inCase == TRUE)
    {
        return FALSE;
    }
    else if (bud_info.inEar == HDMA_UNKNOWN)
    {
        return IN_EAR_FALLBACK;
    }
    else
    {
        return bud_info.inEar;
    }
}
#ifdef INCLUDE_HDMA_MIC_QUALITY_EVENT
void Hdma_CoreHandleVoiceQuality(hdma_timestamp timestamp, uint8 isThisBud, uint8 voiceQuality)
{
    HDMA_DEBUG_LOG("Hdma_CoreHandleVoiceQuality: Timestamp [%u] isThisBud [%u] voiceQuality [%u]", timestamp, isThisBud, voiceQuality);

    /*  Avoid invalid voice data being added to the buffer, it does not add info & causes older data to be deleted */
    if(voiceQuality != HDMA_UNKNOWN_QUALITY)
    {
        if(isThisBud)
        {
            Hdma_QueueInsert(&(hdma_core_data->local_bud.voiceQuality), voiceQuality, timestamp);
        }
        else
        {
            Hdma_QueueInsert(&(hdma_core_data->remote_bud.voiceQuality), voiceQuality, timestamp);
        }
    }
    hdma_StateUpdate(timestamp);
}

/*! \brief filter voice quality with the filter settings that apply for one urgency and determine if the filtered value meets the handover requirement.

    \param[in] timestamp Time at which event is received.
    \param[in] vqHalfLife Voice quality data older than half life will be down weighted
    \param[in] vqMaxAge Voice quality data older than Max life will be discarded
    \param[in] absVQ Absolute voice quality threashold
    \param[in] relVQ Relative voice quality threashold
    \param[in] otherIsBetter Output Voice quality of peer earbud is better
    \param[in] thisIsBetter Output Voice quality of this earbud is better
*/
static void hdma_CheckVoiceQuality(hdma_timestamp timestamp, int16 vqHalfLife, int16 vqMaxAge, int16 absVQ, int16 relVQ, uint8 *otherIsBetter, uint8 *thisIsBetter)
{
    int16 thisVQ = 0;
    int16 otherVQ = 0;

    thisVQ = hdma_Filter(timestamp, &(hdma_core_data->local_bud.voiceQuality), vqHalfLife, vqMaxAge, HDMA_QUEUE_MIC);
    otherVQ = hdma_Filter(timestamp, &(hdma_core_data->remote_bud.voiceQuality), vqHalfLife, vqMaxAge, HDMA_QUEUE_MIC);


    if(thisVQ >= 0 && otherVQ >= 0)
    {
        *otherIsBetter = (thisVQ < absVQ) && ((otherVQ - thisVQ) > relVQ);
        *thisIsBetter = (otherVQ < absVQ) && ((thisVQ - otherVQ) > relVQ);
    }
    else
    {
        *otherIsBetter = 0;
        *thisIsBetter = 0;
    }
    HDMA_DEBUG_LOG_INFO("Mic_Quality otherIsBetter [%u] thisIsBetter [%u]", *otherIsBetter, *thisIsBetter);
}


/*! \brief Check the voice quality to see if a handover is generated at any level of urgency.

    \param[in] timestamp Time at which event is received.
    \param[in] urgency Output gives the urgency of handover from this earbud
    \param[in] suppressUrgency Also gives the urgency of supression of other handovers
    \note This is to allow good voice to suppress other non-critical handover events
*/
static void hdma_ValidateVoiceQuality(hdma_timestamp timestamp,hdma_core_handover_urgency_t *urgency, hdma_core_handover_urgency_t *suppressUrgency )
{
    uint8 otherIsBetter, thisIsBetter;
    hdma_CheckVoiceQuality(timestamp, mic.halfLife_ms.critical, mic.maxAge_ms.critical,
                                mic.absThreshold.critical, mic.relThreshold.critical, &otherIsBetter,&thisIsBetter);
    if (otherIsBetter)
    {
        *urgency = HDMA_CORE_HANDOVER_URGENCY_CRITICAL;
        *suppressUrgency = HDMA_CORE_HANDOVER_URGENCY_INVALID;
        return;
    }

    if (thisIsBetter)
    {
        *urgency = HDMA_CORE_HANDOVER_URGENCY_INVALID;
        *suppressUrgency = HDMA_CORE_HANDOVER_URGENCY_CRITICAL;
        return;
    }

    hdma_CheckVoiceQuality(timestamp, mic.halfLife_ms.high, mic.maxAge_ms.high,
                                mic.absThreshold.high, mic.relThreshold.high, &otherIsBetter,&thisIsBetter);
    if (otherIsBetter)
    {
        *urgency = HDMA_CORE_HANDOVER_URGENCY_HIGH;
        *suppressUrgency = HDMA_CORE_HANDOVER_URGENCY_INVALID;
        return;
    }
    if (thisIsBetter)
    {
        *urgency =  HDMA_CORE_HANDOVER_URGENCY_INVALID;
        *suppressUrgency = HDMA_CORE_HANDOVER_URGENCY_HIGH;
        return;
    }

    hdma_CheckVoiceQuality(timestamp,
                                mic.halfLife_ms.low, mic.maxAge_ms.low,
                                mic.absThreshold.low, mic.relThreshold.low, &otherIsBetter,&thisIsBetter);

    if(otherIsBetter)
    {
        *urgency = HDMA_CORE_HANDOVER_URGENCY_LOW;
        *suppressUrgency = HDMA_CORE_HANDOVER_URGENCY_INVALID;
        return;
    }

    if (thisIsBetter){
        *urgency = HDMA_CORE_HANDOVER_URGENCY_INVALID;
        *suppressUrgency = HDMA_CORE_HANDOVER_URGENCY_LOW;
        return;
    }

    *urgency = HDMA_CORE_HANDOVER_URGENCY_INVALID;
    *suppressUrgency = HDMA_CORE_HANDOVER_URGENCY_INVALID;
}
#endif

#if defined(INCLUDE_HDMA_RSSI_EVENT) || defined(INCLUDE_HDMA_LINK_QUALITY_EVENT)
void Hdma_CoreHandleMirrorAclConnectionInd(const CON_MANAGER_TP_CONNECT_IND_T *msg)
{
    UNUSED(msg);
    HDMA_DEBUG_LOG_INFO("Hdma_CoreHandleMirrorAclConnectionInd");
#ifdef INCLUDE_HDMA_RSSI_EVENT
    /* Reset both local and remote buds' RSSI entries */
    Hdma_QueueCreate(&(hdma_core_data->local_bud.phoneRSSI));
    Hdma_QueueCreate(&(hdma_core_data->remote_bud.phoneRSSI));
#endif
#ifdef INCLUDE_HDMA_LINK_QUALITY_EVENT
    /* Reset both local and remote buds' Link Quality entries */
    Hdma_QueueCreate(&(hdma_core_data->local_bud.linkQuality));
    Hdma_QueueCreate(&(hdma_core_data->remote_bud.linkQuality));
#endif

    /* If HDMA had decisioned to handover based on previous mirroring device's link quality stats,
     * it's required to cancel it and new mirroring device's link quality stats shall be considered */
    if (
            (HDMA_CORE_HANDOVER_REASON_LINK_QUALITY == hdma_core_data->hdma_result.reason) ||
            (HDMA_CORE_HANDOVER_REASON_RSSI == hdma_core_data->hdma_result.reason)
        )
    {
        HDMA_DEBUG_LOG_INFO("Hdma_CoreHandleMirrorAclConnectionInd:Cancelling handover");
#ifndef DEBUG_HDMA_UT
        hdma_NotifyHandoverClients(HDMA_CANCEL_HANDOVER_NOTIFICATION, hdma_core_data->timestamp, HDMA_HANDOVER_REASON_INVALID, HDMA_HANDOVER_URGENCY_INVALID);
#endif
    }
}

void Hdma_CoreHandleLinkQuality(hdma_timestamp timestamp,uint8 isThisBud, hdma_core_link_quality_t linkQuality)
{
    HDMA_DEBUG_LOG("Hdma_CoreHandleLinkQuality: Timestamp = %u", timestamp);

    if(isThisBud)
    {
        HDMA_DEBUG_LOG("Hdma_QueueInsert: timestamp: %u, RSSI: %d, Link Quality: %d (this)", timestamp, linkQuality.rssi, linkQuality.link_quality);
#ifdef INCLUDE_HDMA_RSSI_EVENT
        Hdma_QueueInsert(&(hdma_core_data->local_bud.phoneRSSI), linkQuality.rssi, timestamp);
#endif
#ifdef INCLUDE_HDMA_LINK_QUALITY_EVENT
        Hdma_QueueInsert(&(hdma_core_data->local_bud.linkQuality), linkQuality.link_quality, timestamp);
#endif
        hdma_StateUpdate(timestamp);
    }
    else
    {
        HDMA_DEBUG_LOG("Hdma_QueueInsert: timestamp: %u, RSSI: %d Link Quality: %d (other)", timestamp, linkQuality.rssi, linkQuality.link_quality);
#ifdef INCLUDE_HDMA_RSSI_EVENT
        Hdma_QueueInsert(&(hdma_core_data->remote_bud.phoneRSSI), linkQuality.rssi, timestamp);
#endif
#ifdef INCLUDE_HDMA_LINK_QUALITY_EVENT
        Hdma_QueueInsert(&(hdma_core_data->remote_bud.linkQuality), linkQuality.link_quality, timestamp);
#endif
        hdma_StateUpdate(timestamp);
    }
}
#endif

#ifdef INCLUDE_HDMA_RSSI_EVENT
/*! \brief Filter an RSSI for a single set of urgency settings and determine if a handover is necessary.

    \param[in] timestamp Time at which event is received.
    \param[in] rssiHalfLife_ms RSSI data older than half life will be down weighted
    \param[in] rssiMaxAge_ms RSSI data older than Max life will be discarded
    \param[in] absRSSIThreshold Absolute RSSI threshold
    \param[in] relRSSIThreshold Relative RSSI threshold
    \param[out] Whether handover is required
*/
static uint8 hdma_CheckRSSILevel( hdma_timestamp timestamp, int16 rssiHalfLife_ms,int16 rssiMaxAge_ms, int16 absRSSIThreshold,int16 relRSSIThreshold)
{
    int16 thisRSSI = 0;
    int16 otherRSSI = 0;

    if(hdma_core_data->local_bud.phoneRSSI.size > 0)
        thisRSSI = hdma_Filter(timestamp, &(hdma_core_data->local_bud.phoneRSSI), rssiHalfLife_ms, rssiMaxAge_ms, HDMA_QUEUE_RSSI);
    if(hdma_core_data->remote_bud.phoneRSSI.size > 0)
        otherRSSI = hdma_Filter(timestamp, &(hdma_core_data->remote_bud.phoneRSSI), rssiHalfLife_ms, rssiMaxAge_ms, HDMA_QUEUE_RSSI);
    
    HDMA_DEBUG_LOG_INFO("otherRSSI = %d, thisRSSI = %d", otherRSSI, thisRSSI);

    if(otherRSSI < 0 && thisRSSI < 0)
        return (thisRSSI < absRSSIThreshold) && ((otherRSSI - thisRSSI) > relRSSIThreshold);
    else
        return 0;
}

/*! \brief Validate the RF link determinng if a handover is generated at any urgency level.

    \param[in] timestamp Time at which event is received.
    \param[out] urgency Output gives the urgency of handover from this earbud
*/
static hdma_core_handover_urgency_t hdma_ValidateLink(hdma_timestamp timestamp)
{
    /*  validate the RF link determinng if a handover is generated at any urgency level */
    hdma_core_handover_urgency_t urgency = HDMA_CORE_HANDOVER_URGENCY_INVALID;
    if (hdma_CheckRSSILevel(timestamp,
                                rssi.halfLife_ms.critical, rssi.maxAge_ms.critical,
                                rssi.absThreshold.critical, rssi.relThreshold.critical))
    {
        urgency = HDMA_CORE_HANDOVER_URGENCY_CRITICAL;
    }
    else if (hdma_CheckRSSILevel(timestamp,
                                 rssi.halfLife_ms.high, rssi.maxAge_ms.high,
                                 rssi.absThreshold.high, rssi.relThreshold.high))
    {
        urgency = HDMA_CORE_HANDOVER_URGENCY_HIGH;
    }
    else if (hdma_CheckRSSILevel(timestamp,
                                    rssi.halfLife_ms.low, rssi.maxAge_ms.low,
                                    rssi.absThreshold.low, rssi.relThreshold.low))
    {
        urgency = HDMA_CORE_HANDOVER_URGENCY_LOW;
    }
    return urgency;
}

#endif


#ifdef INCLUDE_HDMA_LINK_QUALITY_EVENT
/*! \brief Filter an Link Quality for a single set of urgency settings and determine if a handover is necessary.

    \param[in] timestamp Time at which event is received.
    \param[in] lqHalfLife_ms Link Quality data older than half life will be down weighted
    \param[in] lqMaxAge_ms Link Quality data older than Max life will be discarded
    \param[in] absLQ Absolute Link Quality threshold
    \param[in] relLQ Relative Link Quality threshold
    \note This is to allow good link quality to suppress other non-critical handover events
*/
static void hdma_CheckLinkQualityLevel( hdma_timestamp timestamp, int16 lqHalfLife,int16 lqMaxAge, uint16 absLQ,uint16 relLQ, uint8 *otherIsBetter)
{
    uint16 thisLQ = 0;
    uint16 otherLQ = 0;

    if(hdma_core_data->local_bud.linkQuality.size > 0)
        thisLQ = hdma_Filter(timestamp, &(hdma_core_data->local_bud.linkQuality), lqHalfLife, lqMaxAge, HDMA_QUEUE_LINK_QUALITY);
    if(hdma_core_data->remote_bud.linkQuality.size > 0)
        otherLQ = hdma_Filter(timestamp, &(hdma_core_data->remote_bud.linkQuality), lqHalfLife, lqMaxAge, HDMA_QUEUE_LINK_QUALITY);
    
    HDMA_DEBUG_LOG_INFO("otherLQ = %u, thisLQ = %u", otherLQ, thisLQ);

    hdma_core_data->lowestLQ = thisLQ < otherLQ ? thisLQ : otherLQ;

    if(otherLQ < 65535 && thisLQ < 65535)
    {
        *otherIsBetter = (thisLQ < absLQ) && ((otherLQ - thisLQ) > relLQ);
    }
    else
    {
        *otherIsBetter = 0;
    }
    HDMA_DEBUG_LOG_INFO("Link_Quality otherIsBetter [%u]", *otherIsBetter);
}


/*! \brief Validate the RF link determinng if a handover is generated at any urgency level.

    \param[in] timestamp Time at which event is received.
    \param[in] urgency Output gives the urgency of handover from this earbud
    \param[in] suppressUrgency Also gives the urgency of supression of other handovers 
    \note This is to allow good voice to suppress other non-critical handover events
*/
static void hdma_ValidateLinkQuality(hdma_timestamp timestamp, hdma_core_handover_urgency_t *urgency, hdma_core_handover_urgency_t *suppressUrgency)
{
    uint8 otherIsBetter;
    
    /*  validate the RF link determinng if a handover is generated at any urgency level */  
    hdma_CheckLinkQualityLevel(timestamp,link.halfLife_ms.critical, link.maxAge_ms.critical,
                                link.absThreshold.critical, link.relThreshold.critical, &otherIsBetter);
    if(otherIsBetter)
    {
        *urgency = HDMA_CORE_HANDOVER_URGENCY_CRITICAL;
        *suppressUrgency = HDMA_CORE_HANDOVER_URGENCY_INVALID;
        return;
    }
    
    hdma_CheckLinkQualityLevel(timestamp, link.halfLife_ms.high, link.maxAge_ms.high,
                                 link.absThreshold.high, link.relThreshold.high, &otherIsBetter);
    if(otherIsBetter)
    {
        *urgency = HDMA_CORE_HANDOVER_URGENCY_HIGH;
        *suppressUrgency = HDMA_CORE_HANDOVER_URGENCY_INVALID;
        return;
    }
    
    hdma_CheckLinkQualityLevel(timestamp, link.halfLife_ms.low, link.maxAge_ms.low,
                                    link.absThreshold.low, link.relThreshold.low, &otherIsBetter);
    if(otherIsBetter)
    {
        *urgency = HDMA_CORE_HANDOVER_URGENCY_LOW;
        *suppressUrgency = HDMA_CORE_HANDOVER_URGENCY_INVALID;
        return;
    }

    *urgency = HDMA_CORE_HANDOVER_URGENCY_INVALID;
    *suppressUrgency = HDMA_CORE_HANDOVER_URGENCY_INVALID;
}

#endif

/*! \brief Filter data according ot the specified parameters.

    \param[in] timestamp Time at which event is received.
    \param[in] queue Queue of data from oldest to newest.  Pairs of (timestamp, value)
    \param[in] halfLife_ms Older data will be down weighted according to this half life
    \param[in] maxAge_ms Data older that this will be rejected
    \param[in] type Type of queue to process data accordingly
    \param[out] Estimated data value now
*/
static int16 hdma_Filter(hdma_timestamp timestamp, queue_t *queue, int16 halfLife_ms,int16 maxAge_ms, queue_type_t type)
{
    /* filter the RSSI history to obtain a single value */
    int totVal = 0; /*  32 bit signed   */
    int totWeight = 0;  /*  32 bit signed   */
    int32 dt;
    int index;
    int16 val;
    uint16 qual;
    uint32 t;
    uint32 NHalf;
    int residual_x4;
    uint16 w;
    int i =0;

    if(!Hdma_IsQueueEmpty(queue))
    {
        HDMA_DEBUG_LOG("hdma_Filter: q_front [%d] q_rear [%d] q_size [%d] halfLife [%d] maxAge_ms [%d]", queue->front, queue->rear, queue->size, halfLife_ms, maxAge_ms);
        HDMA_DEBUG_LOG("hdma_Filter: q_base_time [%d]", queue->base_time);
        
        for(index = queue->rear; i < queue->size; i++, index =((index - 1)%(queue->capacity)))
        {
            qual = queue->quality[index].data;
            t = queue->quality[index].timestamp + queue->base_time;
            val = ((type == HDMA_QUEUE_RSSI) ? (int8)qual : qual);
            
            /*  skip any values of 0xFF & 0x00, these represent unknown voice or unreasonable RSSI or Link Quality */
            if ((type == HDMA_QUEUE_MIC && val == HDMA_UNKNOWN_QUALITY) || (type == HDMA_QUEUE_LINK_QUALITY && val == HDMA_UNKNOWN_LINK_QUALITY))
            {
                continue;
            }

            dt = MAX(timestamp - t, 0);
            if (dt > maxAge_ms)
            {
                break;
            }

            NHalf = MIN((dt / halfLife_ms), 10);

            w = (1 << (10 - NHalf));

            /*  apply filter rounding according to the residual */
            /*  When we filter we down weight older samples.
             *  We divide by an extra power of two if we are in the last quarter before the next integer (i.e. residual/half life > 3/4)
             *  and if it is between ¼ and ¾ when we divide by sqrt(2) or
             *  to do it with integers multiply by 724 and divide by 1024 (again chose to be a bit shift). */
            residual_x4 = 4 * (dt - NHalf * halfLife_ms);
            if ((residual_x4 > halfLife_ms) && (residual_x4 <= 3 * halfLife_ms))
            {
                w = ((w * 724) >> 10);
            }
            else if (residual_x4 > 3 * halfLife_ms)
            {
                w = w >> 1;
            }

            totVal += w * val;
            totWeight += w;
            if(index == 0)
            {
                index = queue->capacity;
            }
        }
    }
    if (totWeight > 0)
    {
        val = ROUND(totVal,totWeight);
    }
    else
    {
        val = HDMA_INVALID;
    }
    return val;
}

#ifdef DEBUG_HDMA_UT
/* Code only for UT */
hdma_core_result_data_t Hdma_GetCoreHdmaData(void)
{
    hdma_core_result_data_t data;
    data.dbgLevel = 0;
    data.inCall = hdma_core_data->inCall;
    data.lastHandoverAttempt = hdma_core_data->lastHandoverAttempt;
    data.hdma_result.handover = hdma_core_data->hdma_result.handover;
    data.hdma_result.reason = hdma_core_data->hdma_result.reason;
    data.hdma_result.urgency = hdma_core_data->hdma_result.urgency;
    data.timestamp = hdma_core_data->timestamp;
#ifdef INCLUDE_HDMA_BATTERY_EVENT
    data.other_bud.batteryStatus = hdma_core_data->remote_bud.batteryStatus;
    data.local_bud.batteryStatus = hdma_core_data->local_bud.batteryStatus;
#else
    data.other_bud.batteryStatus = HDMA_CORE_BATTERY_UNKNOWN;
    data.local_bud.batteryStatus = HDMA_CORE_BATTERY_UNKNOWN;
#endif
    data.other_bud.debugLevel = 0;
    data.other_bud.inCase = hdma_core_data->remote_bud.inCase;
    data.other_bud.inEar = hdma_BudGetInEar(hdma_core_data->remote_bud);
    data.other_bud.lastTimeInEar = hdma_core_data->remote_bud.lastTimeInEar;
    data.local_bud.debugLevel = 0;
    data.local_bud.inCase = hdma_core_data->local_bud.inCase;
    data.local_bud.inEar = hdma_BudGetInEar(hdma_core_data->local_bud);
    data.local_bud.lastTimeInEar = hdma_core_data->local_bud.lastTimeInEar;
#ifdef INCLUDE_HDMA_RSSI_EVENT
    Hdma_PopulateQueueResult(&(hdma_core_data->remote_bud.phoneRSSI), &data.other_bud.phoneRSSI,HDMA_QUEUE_RSSI);
    Hdma_PopulateQueueResult(&(hdma_core_data->local_bud.phoneRSSI), &data.local_bud.phoneRSSI,HDMA_QUEUE_RSSI);
#endif
#ifdef INCLUDE_HDMA_MIC_QUALITY_EVENT
    Hdma_PopulateQueueResult(&(hdma_core_data->remote_bud.voiceQuality), &data.other_bud.voiceQuality,HDMA_QUEUE_MIC);
    Hdma_PopulateQueueResult(&(hdma_core_data->local_bud.voiceQuality), &data.local_bud.voiceQuality,HDMA_QUEUE_MIC);
#endif
    return data;
}

void Hdma_PopulateQueueResult(queue_t *queue, hdma_core_result_queue_t *result, queue_type_t type)
{
    int i = 0;
    int index = 0;
    uint8 qual;
    result->size = queue->size;
    for(i =0, index = queue->front; i < result->size; i++, index =((index + 1)%(queue->capacity)))
    {
        qual = queue->quality[index].data;
        result->data[i][0] = ((type == HDMA_QUEUE_RSSI) ? (int8)qual : qual);
        result->data[i][1] = queue->quality[index].timestamp + queue->base_time;
    }
}

#endif /* DEBUG_HDMA_UT */

#endif /* INCLUDE_HDMA */


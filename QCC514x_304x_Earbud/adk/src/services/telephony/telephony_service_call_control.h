/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Telephony service call control actions
*/

#ifndef TELEPHONY_SERVICE_CALL_CONTROL_H_
#define TELEPHONY_SERVICE_CALL_CONTROL_H_

#include "telephony_service.h"

typedef enum
{
    telephony_join_calls_and_leave,
    telephony_join_calls_and_stay
} telephony_join_calls_action_t;

/*! \brief Find the highest priority voice source with an active call

    \param source Pointer to return the found voice source
    \returns TRUE if found, otherwise FALSE
 */
bool TelephonyService_FindActiveCall(voice_source_t* source);

/*! \brief Find the highest priority voice source with an incoming, outgoing or held call

    \param source Pointer to return the found voice source
    \returns TRUE if found, otherwise FALSE
 */
bool TelephonyService_FindIncomingOutgoingOrHeldCall(voice_source_t* source);

/*! \brief Find the highest priority held call and resume it
 */
void TelephonyService_ResumeHighestPriorityHeldCallRemaining(void);

/*! \brief Hang up a call

    \param source The voice source on which to hang up a call
 */
void TelephonyService_HangUpCall(voice_source_t source);

/*! \brief Answer a call

    \param source The voice source on which to answer a call
 */
void TelephonyService_AnswerCall(voice_source_t source);

/*! \brief Reject a call

    \param source The voice source on which to reject a call
 */
void TelephonyService_RejectCall(voice_source_t source);

/*! \brief Cycle to the next call, where possible putting other calls on hold

    \param source The voice source to cycle through calls on
 */
void TelephonyService_CycleToNextCall(voice_source_t source);

/*! \brief Join calls into a single multiparty call

    \param source The voice source to join calls on
    \param action Select whether to leave or stay in the multiparty call
 */
void TelephonyService_JoinCalls(voice_source_t source, telephony_join_calls_action_t action);

/*! \brief Put an active call on hold

    \param source The voice source to put on hold
 */
void TelephonyService_HoldCall(voice_source_t source);

/*! \brief Resume a held call

    \param source The voice source to resume from held
 */
void TelephonyService_ResumeCall(voice_source_t source);

#endif /* TELEPHONY_SERVICE_CALL_CONTROL_H_ */

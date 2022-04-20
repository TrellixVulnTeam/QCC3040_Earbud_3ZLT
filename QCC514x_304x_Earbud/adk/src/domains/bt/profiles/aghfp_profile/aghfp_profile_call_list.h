/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      AGHFP call list. Used to track incoming/active/outgoing/held calls
*/

#ifndef AGHFP_PROFILE_CALL_LIST_H
#define AGHFP_PROFILE_CALL_LIST_H

#include "aghfp.h"

#define for_each_call(call_list, current_call) for(current_call = call_list->head; current_call != NULL; current_call = current_call->next_call)

typedef struct call_list_element {
    aghfp_call_info call;
    struct call_list_element *next_call;
    struct call_list_element *previous_call;
} call_list_element_t;

typedef struct call_list {
    call_list_element_t *head;
    call_list_element_t *tail;
} call_list_t;

/*! \brief Initialise the call list

    \param call_list pointer to the call list pointer
 */
void AghfpProfileCallList_Initialise(call_list_t **call_list);

/*! \brief Free call list memory and any elements still existing within the list

    \param call_list pointer to the call list pointer
 */
void AghfpProfileCallList_Destroy(call_list_t **call_list);

/*! \brief Gets first element in the list

    \return Pointer to the first element or NULL if empty
 */
call_list_t *AghfpProfileCallList_GetFirstCall(call_list_t *call_list);

/*! \brief Adds an element to the list with incoming status
 */
void AghfpProfileCallList_AddIncomingCall(call_list_t *call_list);

/*! \brief Adds an element to the list with outgoing status
 */
void AghfpProfileCallList_AddOutgoingCall(call_list_t *call_list);

/*! \brief Finds element with incoming status and sets to active call
 * \note Helper function that only works with first incoming call it finds
 */
void AghfpProfileCallList_AnswerIncomingCall(call_list_t *call_list);

/*! \brief Finds element with incoming status and removes from list
 * \note Helper function that only works with first call it finds.
 *       Does not work if multiple calls with same status
 */
void AghfpProfileCallList_RejectIncomingCall(call_list_t *call_list);

/*! \brief Finds element with outgoing status and sets as active call
 * \note Helper function that only works with first call it finds.
 *       Does not work if multiple calls with same status
 */
void AghfpProfileCallList_OutgoingCallAnswered(call_list_t *call_list);

/*! \brief Finds element with outgoing status and removes from list
 * \note Helper function that only works with first call it finds.
 *       Does not work if multiple calls with same status
 */
void AghfpProfileCallList_OutgoingCallRejected(call_list_t *call_list);

/*! \brief Finds element with active status and removes from list
 * \note Helper function that only works with first call it finds.
 *       Does not work if multiple calls with same status
 */
void AghfpProfileCallList_TerminateActiveCall(call_list_t *call_list);

/*! \brief Finds element with active status and sets as held call
 * \note Helper function that only works with first call it finds.
 *       Does not work if multiple calls with same status
 */
void AghfpProfileCallList_HoldActiveCall(call_list_t *call_list);

/*! \brief Finds element with held status and sets as active call
 * \note Helper function that only works with first call it finds.
 *       Does not work if multiple calls with same status
 */
void AghfpProfileCallList_ResumeHeldCall(call_list_t *call_list);

/*! \brief Finds element with held status and removes from list
 * \note Helper function that only works with first call it finds.
 *       Does not work if multiple calls with same status
 */
void AghfpProfileCallList_TerminateHeldCall(call_list_t *call_list);

#endif // AGHFP_PROFILE_CALL_LIST_H

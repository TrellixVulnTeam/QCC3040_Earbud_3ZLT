/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      AGHFP call list. Used to track incoming/active/outgoing/held calls
*/

#include "aghfp_profile_call_list.h"

#include <logging.h>
#include <panic.h>
#include <stdlib.h>

static call_list_element_t *aghfpProfileCallList_FindLatest(call_list_element_t *call_list)
{
    if (call_list->next_call == NULL)
    {
        return call_list;
    }
    else
    {
        return aghfpProfileCallList_FindLatest(call_list->next_call);
    }
}

static void aghfpProfileCallList_PopulateCall(call_list_element_t *call_list, uint8 index, aghfp_call_state state, aghfp_call_dir dir)
{
    call_list->call.dir = dir;
    call_list->call.idx = index;
    call_list->call.size_number = 0;
    call_list->call.status = state;
    call_list->call.mode = aghfp_call_mode_voice;
    call_list->call.mpty = aghfp_call_not_mpty; /* Not a multiparty call */
}

static bool aghfpProfileCallList_IsListEmpty(call_list_t *call_list)
{
    return call_list->head == NULL;
}

static void aghfpProfileCallList_AddToList(call_list_t *call_list, aghfp_call_state state, aghfp_call_dir dir)
{
    if (aghfpProfileCallList_IsListEmpty(call_list))
    {
        call_list->head = PanicNull(malloc(sizeof(call_list_element_t)));
        call_list->tail = call_list->head;
        aghfpProfileCallList_PopulateCall(call_list->head, 1, state, dir);
        call_list->head->next_call = NULL;
        call_list->head->previous_call = NULL;
    }
    else
    {
        call_list_element_t *latest_call = call_list->tail;
        call_list_element_t *new_call = PanicNull(malloc(sizeof(call_list_element_t)));
        uint8 new_index = latest_call->call.idx + 1;

        aghfpProfileCallList_PopulateCall(new_call, new_index, state, dir);

        latest_call->next_call = new_call;

        new_call->previous_call = latest_call;
        new_call->next_call = NULL;

        call_list->tail = new_call;
    }
}

static void aghfpProfileCallList_DecrementIndexes(call_list_element_t * call)
{
    call_list_element_t *current_call = call;

    while (current_call)
    {
        current_call->call.idx--;
        current_call = current_call->next_call;
    }
}

static void aghfpProfileCallList_RemoveFromList(call_list_t * call_list, call_list_element_t *call)
{
    call_list_element_t *previous_call = call->previous_call;
    call_list_element_t *next_call = call->next_call;

    if (previous_call)
    {
        previous_call->next_call = next_call;
    }
    else
    {
        call_list->head = next_call;
    }

    if (next_call)
    {
        next_call->previous_call = previous_call;
    }
    else
    {
        call_list->tail = previous_call;
    }

    free(call);

    aghfpProfileCallList_DecrementIndexes(next_call);
}

static call_list_element_t *aghfpProfileCallList_FindCallWithStatus(call_list_t *call_list, aghfp_call_state state)
{
    call_list_element_t *current_call = call_list->head;

    while(current_call)
    {
        if (current_call->call.status == state)
        {
            return current_call;
        }
        current_call = current_call->next_call;
    };

    return NULL;
}

void AghfpProfileCallList_Initialise(call_list_t **call_list)
{
    DEBUG_LOG_FN_ENTRY("AghfpProfileCallList_Initialise");

    *call_list = PanicNull(malloc(sizeof(call_list_t)));
    (*call_list)->head = NULL;
}

void AghfpProfileCallList_Destroy(call_list_t **call_list)
{
    DEBUG_LOG_FN_ENTRY("AghfpProfileCallList_Destroy");

    PanicNull(*call_list);

    call_list_element_t *call = (*call_list)->head;
    call_list_element_t *next_call = NULL;

    while(next_call || call)
    {
        next_call = call->next_call;

        free(call);

        if (next_call)
        {
            call = next_call->next_call;
            free(next_call);
            next_call = NULL;
        }
        else
        {
            call = NULL;
        }
    }

    free(*call_list);
}

call_list_t *AghfpProfileCallList_GetFirstCall(call_list_t *call_list)
{
    DEBUG_LOG_FN_ENTRY("AghfpProfileCallList_GetFirstCall");

    if (!aghfpProfileCallList_IsListEmpty(call_list))
    {
        return call_list;
    }

    return NULL;
}

void AghfpProfileCallList_AddIncomingCall(call_list_t *call_list)
{
    DEBUG_LOG_FN_ENTRY("AghfpProfileCallList_AddIncomingCall");

    aghfpProfileCallList_AddToList(call_list, aghfp_call_state_incoming, aghfp_call_dir_incoming);
}

void AghfpProfileCallList_AddOutgoingCall(call_list_t *call_list)
{
    DEBUG_LOG_FN_ENTRY("AghfpProfileCallList_AddIncomingCall");

    aghfpProfileCallList_AddToList(call_list, aghfp_call_state_alerting, aghfp_call_dir_outgoing);
}

void AghfpProfileCallList_AnswerIncomingCall(call_list_t *call_list)
{
    DEBUG_LOG_FN_ENTRY("AghfpProfileCallList_AnswerIncomingCall");

    call_list_element_t *incoming_call = aghfpProfileCallList_FindCallWithStatus(call_list, aghfp_call_state_incoming);

    if (incoming_call)
    {
        incoming_call->call.status = aghfp_call_state_active;
    }
}

void AghfpProfileCallList_RejectIncomingCall(call_list_t *call_list)
{
    DEBUG_LOG_FN_ENTRY("AghfpProfileCallList_RejectIncomingCall");

    call_list_element_t *incoming_call = aghfpProfileCallList_FindCallWithStatus(call_list, aghfp_call_state_incoming);

    if (incoming_call)
    {
        aghfpProfileCallList_RemoveFromList(call_list, incoming_call);
    }
}

void AghfpProfileCallList_OutgoingCallAnswered(call_list_t *call_list)
{
    DEBUG_LOG_FN_ENTRY("AghfpProfileCallList_OutgoingCallAnswered");

    call_list_element_t *outgoing_call = aghfpProfileCallList_FindCallWithStatus(call_list, aghfp_call_state_alerting);

    if (outgoing_call)
    {
        outgoing_call->call.status = aghfp_call_state_active;
    }
}

void AghfpProfileCallList_OutgoingCallRejected(call_list_t *call_list)
{
    DEBUG_LOG_FN_ENTRY("AghfpProfileCallList_OutgoingCallRejected");

    call_list_element_t *outgoing_call = aghfpProfileCallList_FindCallWithStatus(call_list, aghfp_call_state_alerting);

    if (outgoing_call)
    {
        aghfpProfileCallList_RemoveFromList(call_list, outgoing_call);
    }
}


void AghfpProfileCallList_TerminateActiveCall(call_list_t *call_list)
{
    DEBUG_LOG_FN_ENTRY("AghfpProfileCallList_TerminateActiveCall");

    call_list_element_t *active_call = aghfpProfileCallList_FindCallWithStatus(call_list, aghfp_call_state_active);

    if (active_call)
    {
        aghfpProfileCallList_RemoveFromList(call_list, active_call);
    }
}

void AghfpProfileCallList_HoldActiveCall(call_list_t *call_list)
{
    DEBUG_LOG_FN_ENTRY("AghfpProfileCallList_HoldActiveCall");

    call_list_element_t *held_call = aghfpProfileCallList_FindCallWithStatus(call_list, aghfp_call_state_active);

    if (held_call)
    {
        held_call->call.status = aghfp_call_state_held;
    }
}

void AghfpProfileCallList_ResumeHeldCall(call_list_t *call_list)
{
    DEBUG_LOG_FN_ENTRY("AghfpProfileCallList_ResumeHeldCall");

    call_list_element_t *held_call = aghfpProfileCallList_FindCallWithStatus(call_list, aghfp_call_state_held);

    if (held_call)
    {
        held_call->call.status = aghfp_call_state_active;
    }
}

void AghfpProfileCallList_TerminateHeldCall(call_list_t *call_list)
{
    DEBUG_LOG_FN_ENTRY("AghfpProfileCallList_TerminateHeldCall");

    call_list_element_t *held_call = aghfpProfileCallList_FindCallWithStatus(call_list, aghfp_call_state_held);

    if (held_call)
    {
        aghfpProfileCallList_RemoveFromList(call_list, held_call);
    }
}

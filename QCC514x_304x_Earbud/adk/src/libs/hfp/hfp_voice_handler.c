/****************************************************************************
Copyright (c) 2004 - 2020 Qualcomm Technologies International, Ltd.


FILE NAME
    hfp_voice_handler.c

DESCRIPTION


NOTES

*/


/****************************************************************************
    Header files
*/
#include "hfp.h"
#include "hfp_private.h"
#include "hfp_common.h"
#include "hfp_indicators_handler.h"
#include "hfp_parse.h"
#include "hfp_send_data.h"
#include "hfp_voice_handler.h"
#include "hfp_link_manager.h"
#include "hfp_hs_handler.h"
#include "hfp_call_handler.h"

#include <panic.h>
#include <string.h>


/****************************************************************************
NAME
    hfpHandleVoiceRecognitionEnable

DESCRIPTION
    Enable/ disable voice dialling at the AG.

RETURNS
    bool    success/fail
*/
bool hfpHandleVoiceRecognitionEnable(const HFP_INTERNAL_AT_BVRA_REQ_T *req)
{
    hfp_link_data* link = req->link;

    if((hfFeatureEnabled(HFP_VOICE_RECOGNITION) && agFeatureEnabled(link, AG_VOICE_RECOGNITION)) ||
       (hfFeatureEnabled(HFP_EVRA_SUPPORTED) && agFeatureEnabled(link, AG_EVRA_SUPPORTED)))
    {
        /* Only send the cmd if the AG and local device support the voice dial or EVRA feature */
        char *bvra;

        switch(req->enable)
        {
            case hfp_evra_action_disable:
                bvra = "AT+BVRA=0\r";
            break;
            case hfp_evra_action_enable:
                bvra = "AT+BVRA=1\r";
            break;
            case hfp_evra_action_ready:
                if(hfFeatureEnabled(HFP_EVRA_SUPPORTED) && agFeatureEnabled(link, AG_EVRA_SUPPORTED))
                    bvra = "AT+BVRA=2\r";
                else
                    return FALSE;
            break;
            default:    /* Something wrong */
                return FALSE;
        }

        /* Send the AT cmd over the air */
        hfpSendAtCmd(link, (uint16)strlen(bvra), bvra, hfpBvraCmdPending);
        return TRUE;
    }

    return FALSE;
}


/****************************************************************************
NAME
    hfpHandleHspVoiceRecognitionEnable

DESCRIPTION
    Enable/ disable voice dialling at the AG.

RETURNS
    void
*/
bool hfpHandleHspVoiceRecognitionEnable(const HFP_INTERNAL_AT_BVRA_REQ_T *req)
{
    hfp_link_data* link = req->link;

    if(req->enable && link->bitfields.ag_call_state == hfp_call_state_idle)
    {
        hfpSendHsButtonPress(link, hfpBvraCmdPending);
        return TRUE;
    }

    return FALSE;
}


/****************************************************************************
NAME
    hfpHandleVoiceRecognitionStatus

DESCRIPTION
    Voice recognition status indication received from the AG.

AT INDICATION
    +BVRA

RETURNS
    void
*/
void hfpHandleVoiceRecognitionStatus(Task link_ptr, const struct hfpHandleVoiceRecognitionStatus *ind)
{
    /*
        Send a message to the application telling it the current status of the
        voice recognition engine at the AG.
    */
    hfp_link_data* link = (hfp_link_data*)link_ptr;

    MAKE_HFP_MESSAGE(HFP_VOICE_RECOGNITION_IND);
    message->priority = hfpGetLinkPriority(link);
    message->enable = ind->enable;
    MessageSend(theHfp->clientTask, HFP_VOICE_RECOGNITION_IND, message);

    /* This is necessary when dealing with HFP v0.96 AGs that do not send 
       Call Setup indicators */
    if(!ind->enable)
        hfpHandleCallVoiceRecDisabled(link);
}

/****************************************************************************
NAME
    hfpHandleVoiceRecognitionStatusState

DESCRIPTION
    Enhanced Voice recognition status indication received from the AG.

AT INDICATION
    +BVRA

RETURNS
    void
*/
void hfpHandleVoiceRecognitionStatusState(Task link_ptr, const struct hfpHandleVoiceRecognitionStatusState *ind)
{
    /* Only pass this info to the app if the HFP supports this functionality */
    if(hfFeatureEnabled(HFP_EVRA_SUPPORTED))
    {
        hfp_link_data* link = (hfp_link_data*)link_ptr;

        /*
            Send a message to the application telling it the current status and state of the
            enhanced voice recognition engine at the AG.
        */
        MAKE_HFP_MESSAGE(HFP_VOICE_RECOGNITION_EVRA_IND);
        message->priority = hfpGetLinkPriority(link);
        message->enable = ind->enable;
        message->state  = ind->vrecState;
        message->textId =  0;
        message->textType = 0;
        message->textOperation = 0;
        MessageSend(theHfp->clientTask, HFP_VOICE_RECOGNITION_EVRA_IND, message);

        /* This is necessary when dealing with HFP v0.96 AGs that do not send 
           Call Setup indicators */
        if(!ind->enable)
            hfpHandleCallVoiceRecDisabled(link);
    }
}


/****************************************************************************
NAME
    hfpHandleVoiceRecognitionStatusText

DESCRIPTION
    Enhanced Voice recognition status indication received from the AG with
    text

AT INDICATION
    +BVRA

RETURNS
    void
*/
void hfpHandleVoiceRecognitionStatusText(Task link_ptr, const struct hfpHandleVoiceRecognitionStatusText *ind)
{
    /*
        Send a message to the application telling it the current status, state and text indication from the
        enhanced voice recognition engine at the AG.
    */

        /* Only pass this info to the app if the HFP supports this functionality */
    if(hfFeatureEnabled(HFP_EVRA_SUPPORTED) && hfFeatureEnabled(HFP_EVRA_TEXT_SUPPORTED))
    {
        hfp_link_data* link = (hfp_link_data*)link_ptr;
        uint16 textLen = ind->textStr.length;

            /* Don't exceed max length */
        if(textLen >= HFP_MAX_ARRAY_LEN)
        {
            textLen = HFP_MAX_ARRAY_LEN - 1;
        }

        /* create the message */
        {
            /* Allow room for NULLs */
            MAKE_HFP_MESSAGE_WITH_LEN(HFP_VOICE_RECOGNITION_EVRA_IND, textLen + 1);
            message->priority = hfpGetLinkPriority(link);
            message->enable = ind->enable;
            message->state  = ind->vrecState;
            message->textId =  ind->textId;
            message->textType = ind->textType;
            message->textOperation = ind->textOperation;

            /* Copy data into the message */
            memmove(message->vr_text, ind->textStr.data, textLen);
            /* NULL terminate strings */
            message->vr_text[textLen]     = '\0';

            MessageSend(theHfp->clientTask, HFP_VOICE_RECOGNITION_EVRA_IND, message);

            /* This is necessary when dealing with HFP v0.96 AGs that do not send 
               Call Setup indicators */
            if(!ind->enable)
                hfpHandleCallVoiceRecDisabled(link);
        }
    }
}

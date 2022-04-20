/****************************************************************************
Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.


FILE NAME
    connection_handover.c

DESCRIPTION
    TWS Marshaling interface for the Connection Library
    
NOTES
    See handover_if.h for further interface description
    
    Builds requiring this should include CONFIG_HANDOVER in the
    makefile. e.g.
        CONFIG_FEATURES:=CONFIG_HANDOVER    
*/


/****************************************************************************
    Header files
*/
#include "connection.h"
#include "connection_private.h"
#include "init.h"
#include "handover_if.h"

#include <vm.h>
#include <string.h>
#include <logging.h>


static bool connectionVeto(void);

static bool connectionMarshal(const tp_bdaddr *tp_bd_addr,
                       uint8 *buf,
                       uint16 length,
                       uint16 *written);

static bool connectionUnmarshal(const tp_bdaddr *tp_bd_addr,
                         const uint8 *buf,
                         uint16 length,
                         uint16 *consumed);

extern const handover_interface connection_handover_if =  {
        &connectionVeto,
        &connectionMarshal,
        &connectionUnmarshal,
        NULL,
        NULL,
        NULL};

/* Return TRUE if the message is a mode change event */
static bool connectionMessageIsModeChangeEvent(Task task, MessageId id, Message message)
{
    UNUSED(task);
    if (id == MESSAGE_BLUESTACK_DM_PRIM)
    {
        const DM_UPRIM_T *prim = message;
        if (prim->type == DM_HCI_MODE_CHANGE_EVENT_IND)
        {
            return TRUE;
        }
    }
    return FALSE;
}

/* Return TRUE if the message is one that is disallowed during handover. */
static bool connectionMessageIsDisallowed(Task task, MessageId id, Message message)
{
    UNUSED(task);
    if (id == MESSAGE_BLUESTACK_DM_PRIM)
    {
        const DM_UPRIM_T *prim = message;
        if(prim->type == DM_HCI_ULP_BIGINFO_ADV_REPORT_IND)
        {
            return FALSE;
        }
        else if(prim->type == DM_ULP_PERIODIC_SCAN_SYNC_ADV_REPORT_IND)
        {
            return FALSE;
        }
    }
    else if(id == MESSAGE_MORE_DATA)
    {
        return FALSE;
    }
    return TRUE;
}

/****************************************************************************
NAME    
    connectionVeto

DESCRIPTION
    Veto check for Connection library

    Prior to handover commencing this function is called and
    the libraries internal state is checked to determine if the
    handover should proceed.

RETURNS
    bool TRUE if the Connection Library wishes to veto the handover attempt.
*/
bool connectionVeto( void )
{
    uint16 disallowed_messages;
    
    /* Check the connection library is initialized */
    if(!connectionGetInitState())
        return TRUE;

    /* Check Lock Status */
    if(connectionGetLockState())
        return TRUE;
    
    /* Check for outstanding scan requests */
    if(connectionOutstandingWriteScanEnableReqsGet() != 0)
        return TRUE;

    /* Check for pending messages on the application task */
    if(MessagesPendingForTask(connectionGetAppTask(), NULL) != 0)
    {
       return TRUE;
    }

    /* Check for disallowed messages on the Connection library task. */
    disallowed_messages = MessagePendingMatch(connectionGetCmTask(), FALSE, connectionMessageIsDisallowed);
    if (disallowed_messages)
    {
        uint16 mode_change_msgs = MessagePendingMatch(connectionGetCmTask(), FALSE, connectionMessageIsModeChangeEvent);
        if (mode_change_msgs > 1)
        {
            /* Only allow one mode change event (this will be for the peer earbud link). */
            DEBUG_LOG_INFO("connectionVeto vetoing %d mode change msgs found", mode_change_msgs);
            return TRUE;
        }
        disallowed_messages -= mode_change_msgs;
    }

    /* Veto if still there are any other messages other than allowed ones */
    if (disallowed_messages != 0)
    {
        DEBUG_LOG_INFO("connectionVeto vetoing %d disallowed messages on queue", disallowed_messages);
        return TRUE;
    }
    return FALSE;
}

/****************************************************************************
NAME    
    connectionMarshal

DESCRIPTION
    Marshal the data associated with the Connection Library

RETURNS
    bool TRUE Connection library marshalling complete, otherwise FALSE
*/
bool connectionMarshal(const tp_bdaddr *tp_bd_addr,
                       uint8 *buf,
                       uint16 length,
                       uint16 *written)
{
    UNUSED(tp_bd_addr);
    UNUSED(buf);
    UNUSED(length);
    UNUSED(written);
    /* Nothing to be done */
    return TRUE;
}

/****************************************************************************
NAME    
    connectionUnmarshal

DESCRIPTION
    Unmarshal the data associated with the Connection Library

RETURNS
    bool TRUE if Connection Library unmarshalling complete, otherwise FALSE
*/
bool connectionUnmarshal(const tp_bdaddr *tp_bd_addr,
                                   const uint8 *buf,
                                   uint16 length,
                                   uint16 *consumed)
{
    UNUSED(tp_bd_addr);
    UNUSED(buf);
    UNUSED(length);
    UNUSED(consumed);
    /* Nothing to be done */
    return TRUE;
}

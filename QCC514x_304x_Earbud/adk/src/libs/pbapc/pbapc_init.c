/****************************************************************************
Copyright (c) 2004 - 2020 Qualcomm Technologies International, Ltd.


FILE NAME
    pbapc_init.c
    
DESCRIPTION
    PBAP Client initalization routines including the handling of SDP 
    registration and SDP search.
*/
#include <connection.h>
#include <sdp_parse.h>
#include <stdlib.h>
#include <string.h>
#include <logging.h>

#include "pbapc_extern.h"
#include "pbapc_private.h"
#include "pbapc_util.h"


/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_TYPE(PbapcMessageId)
LOGGING_PRESERVE_MESSAGE_TYPE(pbapc_internal_message_t)



/* OBEX PBAP service */
static const uint8 servRequestPbapc[] =
{
    0x35, 0x03, 
    0x19, 0x11, 0x2F /* UUID16, Phone book access Server  */
};

/* Static data structures for SDP Attribute Request to PSE */
static const uint8 attrRequestPbapc[] =
{
    0x35,0x0c,
        0x09, 0x00, 0x04,         /* Protocol Descriptor List */
        0x09, 0x03, 0x14,        /* Supported Message Types */   
        0x09, 0x02, 0x00,        /* GoepL2capPsm ID */
        0x09, 0x03, 0x17

};
const uint8 serviceRecordPbapc[] =
    {
        0x09,0x00,0x01,        /* Service class ID list */
        0x35,0x03,
        0x19,0x11, 0x2E,    /* UUID = Phonebook Access Client */

        0x09,0x01,0x00,        /* Service name */
        0x25,0x0B,            /* 11 byte string - PBAP Client */
        'P','B','A','P',' ','C','l','i','e','n','t',


        0x09,0x00,0x09,        /* profile descriptor list */
        0x35,0x08,
        0x35,0x06,            /* 6 bytes in total DataElSeq */
        0x19,0x11,0x30,      /* UUID = OBEXPhonebookAccess */

        /* Profile version */
        0x09,0x01,0x02      /* 2 byte UINT, Profile Version = 0x0102 */
    };

#define LOCAL_PSM 0x1005   /* Local PSM for OBEX over L2cap */

/****************************************************************************
 *NAME    
 *  handleSDPRegisterCfm
 *
 *DESCRIPTION
 * Handle the SDP Record register confirmation 
 ***************************************************************************/
static void handleSDPRegisterCfm( pbapcState *state, 
                                  CL_SDP_REGISTER_CFM_T *msg )
{
    PbapcLibStatus status = (msg->status == success)?
                             pbapc_success: pbapc_sdp_failure_bluestack;


    if( state->currCom == pbapc_com_reg_sdp )
    {
        pbapcMsgInitCfm( state->theAppTask, msg->service_handle, status );

        /* Free the state now */
        MessageSend(&state->task,PBAPC_INT_TASK_DELETE, 0);
    }
    else
    {
        PBAPC_DEBUG(("State invalid\n"));    
    }
}



/****************************************************************************
 *NAME    
 *  handleSDPServSrchAttrCfm
 *
 *DESCRIPTION
 * Handle the SDP response and proceed with the connection
 ***************************************************************************/
static void handleSDPServSrchAttrCfm( pbapcState *state, 
                                  const CL_SDP_SERVICE_SEARCH_ATTRIBUTE_CFM_T *msg)
{

	/*PSMs if OBEX over L2CAP*/
	uint16 psm[2];
	psm[0] = LOCAL_PSM; /*local psm*/
	psm[1] = 0; /*remote PSM*/

	/*channel if OBEX over RFCOMM*/
    uint16 channel = 0;  
	uint8 chan = 0;
    uint8 *chans = &chan;
    uint8 found;

    uint8 repos;
    Region value;
    uint16 device_id = state->device_id;

    
    if( state->currCom != pbapc_com_connect ) 
    {
        MessageFlushTask((Task)state);
        /* Delete the task which is allocated during connect initiation with remote device */
        MessageSendLater(&state->task,PBAPC_INT_TASK_DELETE, 0, PBAPC_TASK_DELETE_DELAY);        
        return;
    }

	/* OBEX over L2CAP in the presence of a GoepL2capPsm attribute in the PSE's SDP records */
    if ( (msg->status==success)&&
                        (SdpParseGetGoepL2CapPsm(msg->size_attributes, msg->attributes, &psm[1])))
			
    {

        SdpParseGetPbapRepos(msg->size_attributes, msg->attributes, &repos);
		state->srvRepos = repos;
        state->L2CAP_conn = TRUE;
        if(findPBAPCSupportedFeatures(msg->size_attributes, msg->attributes,&value))
        {
            state->support_features = TRUE;
        }
		pbapcGoepConnect( state, &msg->bd_addr, &psm[0]);

       
    }

	/*OBEX over RFCOMM if above condition is not met */	
     else if ( (msg->status==success) &&
          SdpParseGetMultipleRfcommServerChannels(
            msg->size_attributes, msg->attributes,
            1, &chans, &found) )
    {
        SdpParseGetPbapRepos(msg->size_attributes, msg->attributes, &repos);
        state->srvRepos = repos;
		
        channel = (uint16)chan;
        state->L2CAP_conn = FALSE;
        /* Initiate a Connection attempt */
        pbapcGoepConnect( state, &msg->bd_addr, &channel);
    }

    else
    {
        state->currCom = pbapc_com_none;
        MessageFlushTask((Task)state);
        pbapcMsgSendConnectCfm( state->theAppTask, 
                                NULL,
                                &msg->bd_addr, 
                                pbapc_failure, 0, 0 );
        
        /* No pbapc supported, reset to NULL */
        Pbapc[device_id] = NULL;
        
        /* Delete the task after PBAPC_TASK_DELETE_DELAY, This is to handle messages directed
           to the task with in that time. the only possible message is 
           any continuation responses for this SDP attribute search. */
        MessageSendLater(&state->task,PBAPC_INT_TASK_DELETE, 0, PBAPC_TASK_DELETE_DELAY);
    }
}

/****************************************************************************
 * NAME
 *  pbapcCreateTask
 *
 * DESCRIPTION
 *  Create the PBAP task
 **************************************************************************/
Task pbapcCreateTask( Task theAppTask )
{
    pbapcState *state;

    state = malloc(sizeof(pbapcState));
    if( !state )
        return NULL;

    memset(state, 0, sizeof( pbapcState ) );
    state->task.handler = pbapcInitHandler;
    state->theAppTask = theAppTask;

    return &state->task;
}
/****************************************************************************
 * NAME
 *  pbapcRegisterSdpRecord
 *
 * DESCRIPTION
 *  Register the Client Side SDP Record with Bluestack 
 **************************************************************************/
void pbapcRegisterSdpRecord( pbapcState *state)
{
    ConnectionRegisterServiceRecord( &state->task,
                                     sizeof( serviceRecordPbapc ),
                                     serviceRecordPbapc );

    state->currCom = pbapc_com_reg_sdp;
}

/****************************************************************************
 * NAME
 *  pbapcInitConnection
 *
 * DESCRIPTION
 *  Initiate the PBAP Connection by starting an SDP search first.
 **************************************************************************/
void pbapcInitConnection( pbapcState *state, const bdaddr *bdAddr )
{
    /* search for remote channel */
    ConnectionSdpServiceSearchAttributeRequest(
        &state->task,
        bdAddr,
        30,
        sizeof(servRequestPbapc), servRequestPbapc,
        sizeof(attrRequestPbapc), attrRequestPbapc);

    state->currCom= pbapc_com_connect;
}

/****************************************************************************
 *NAME    
 *  pbapcGetSupportedRepositories
 *
 *DESCRIPTION
 *  Get the supported repositories. reurn 0xFF on error
 ***************************************************************************/
uint8 pbapcGetSupportedRepositories( pbapcState *state )
{
    return (state->handle)?(state->srvRepos): 0;
}

/****************************************************************************
 *NAME    
 *  pbapcGetApptask
 *
 *DESCRIPTION
 *  Get the application task
 ***************************************************************************/
Task pbapcGetAppTask( pbapcState *state )
{
    return state->theAppTask;
}

/****************************************************************************
 *NAME    
 *  pbapcInitHandler
 *
 *DESCRIPTION
 * Initialization Handler for messages received by the PBAPC Task.
 ***************************************************************************/
void  pbapcInitHandler( Task task, MessageId id, Message message)
{
    pbapcState *state = (pbapcState*) task;

    if( id <  PBAPC_INT_ENDOFLIST )
    {
        pbapcIntHandler( task, id, message );
		return;
    }

    switch (id)
    {
        /* Messages from the connection library */
        case CL_SDP_REGISTER_CFM:
            handleSDPRegisterCfm(state, (CL_SDP_REGISTER_CFM_T*)message);
            break;

        case CL_SDP_SERVICE_SEARCH_ATTRIBUTE_CFM:
            handleSDPServSrchAttrCfm(state, 
                            (const CL_SDP_SERVICE_SEARCH_ATTRIBUTE_CFM_T*)message);
            break;
        
        default:
           PBAPC_LOG(("Unhandled Init message : MESSAGE:0x%X\n",id));
           break;
    }
}


/****************************************************************************
Copyright (c) 2010 - 2020 Qualcomm Technologies International, Ltd.
 

FILE NAME
    obex_server.c
    
DESCRIPTION
    This file defines all API functions for a OBEX server session.
    The library allows creation of multiple OBEX sessions to multiple 
    devices. It is the applications responsibility to limit the number of 
    sessions for optimal resource usage.
*/

#include "obex_extern.h"
#include <stdlib.h>

/****************************************************************************
 * NAME
 *  ObexConnectResponse
 * 
 * DESCRIPTION
 *  API to establish a OBEX server session 
 *
 * PARAMETERS
 *  Refer obex.h
 *
 * RETURNS
 **************************************************************************/
Task ObexConnectResponse( Sink					sink,
                          uint16                *Channel,
                          bool                  accept,
                          const ObexConnParams* connParams)  
{
    ObexConnId connId;
    ObexChannel   channel;

    connId.u.sink = sink;

	channel.l2capObex = FALSE;
    channel.u.channel = (uint8)Channel[0];


    return obexConnectResponse( connId, channel, accept, connParams );     
}
   
/**************************************************************************
 * NAME
 *  ObexPutResponse
 * 
 * DESCRIPTION
 *  API to send a PUT response
 *
 * PARAMETERS
 *  Refer obex.h
 **************************************************************************/
void ObexPutResponse( Obex session, ObexResponse response )
{
    OBEX_ASSERT( session );
    obexSendResponse( session, response );
}

/**************************************************************************
 * NAME
 *  ObexGetResponse
 * 
 * DESCRIPTION
 *  API to send a GET response
 *
 * PARAMETERS
 *  Refer obex.h
 **************************************************************************/
void ObexGetResponse( Obex session, ObexResponse response )
{
    OBEX_ASSERT( session );
    obexSendResponse( session, response );
}

/**************************************************************************
 * NAME
 *  ObexSetPathResponse
 * 
 * DESCRIPTION
 *  API to send a SETPATH response
 *
 * PARAMETERS
 *  Refer obex.h
 **************************************************************************/
void ObexSetPathResponse( Obex session, ObexResponse response )
{
    OBEX_ASSERT( session );
    if(response == obex_continue ) response = obex_remote_success;

    obexSendResponse( session, response );
}

/****************************************************************************
Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.


FILE NAME
    hfp_handover.c

DESCRIPTION
    Implements HFP handover logic (Veto, Marshals/Unmarshals, Handover, etc).
 
NOTES
    See handover_if.h for further interface description
    
    Builds requiring this should include CONFIG_HANDOVER in the
    makefile. e.g.
        CONFIG_FEATURES:=CONFIG_HANDOVER    
 */

#include "hfp_marshal_desc.h"
#include "hfp_private.h"
#include "hfp_init.h"
#include "hfp_link_manager.h"
#include "hfp_handover_policy.h"

#include "marshal.h"
#include "bdaddr.h"
#include <panic.h>
#include <stdlib.h>
#include <sink.h>
#include <stream.h>
#include <source.h>
#include <logging.h>

/* This byte is set when unmarshalling is complete and cleared when committing.
   It is used in the abort to determine if the secondary need to clean up
   its state if the abort occurs between unmarshal and commit */
static uint8 unmarshalled = 0;

/* Iterate over all link priority */
#define for_all_priority(priority)     for(priority = hfp_primary_link; priority <= hfp_secondary_link; priority++)

#define RFC_INVALID_SERV_CHANNEL   0x00

static bool hfpVeto(void);

static bool hfpMarshal(const tp_bdaddr *tp_bd_addr,
                       uint8 *buf,
                       uint16 length,
                       uint16 *written);

static bool hfpUnmarshal(const tp_bdaddr *tp_bd_addr,
                         const uint8 *buf,
                         uint16 length,
                         uint16 *consumed);

static void hfpHandoverCommit(const tp_bdaddr *tp_bd_addr, const bool newRole);

static void hfpHandoverComplete( const bool newRole );

static void hfpHandoverAbort( void );

extern const handover_interface hfp_handover_if =  {
        &hfpVeto,
        &hfpMarshal,
        &hfpUnmarshal,
        &hfpHandoverCommit,
        &hfpHandoverComplete,
        &hfpHandoverAbort};

/****************************************************************************
NAME    
    hfpLinkInSteadyState

DESCRIPTION
    Returns TRUE if hfp audio state is either connected or disconnected.
    Then hfp link state is considered in steady state. In any other state,
    it is considered in transition state and returns FALSE.

RETURNS
    bool TRUE if hfp link is in steady state, otherwise FALSE
*/
static bool hfpLinkInSteadyState(hfp_link_data* link)
{
    if (link->bitfields.audio_state == hfp_audio_connected ||
        link->bitfields.audio_state == hfp_audio_disconnected)
    {
        return TRUE;
    }
    return FALSE;
}

/****************************************************************************
NAME    
    getRemoteServerChannel

DESCRIPTION
    Returns remote server channel of the specified link. If it does not exist
    or is idle or disabled then it returns RFC_INVALID_SERV_CHANNEL

RETURNS
    uint8 Remote server channel
*/
static uint8 getRemoteServerChannel(hfp_link_data *link)
{
    uint8 channel = RFC_INVALID_SERV_CHANNEL;
                            
    if (link->bitfields.ag_slc_state == hfp_slc_idle ||
        link->bitfields.ag_slc_state == hfp_slc_disabled)
    {
        /* Not connected */
    }
    else
    {
        /* Connected */
        channel = SinkGetRfcommServerChannel(link->identifier.sink);
    }

    return channel;
}

/****************************************************************************
NAME
    hfpHandoverAbort

DESCRIPTION
    Abort the HFP Handover process, free any memory
    associated with the unmarshalling process.

RETURNS
    void

*/
static void hfpHandoverAbort(void)
{
    if (unmarshalled)
    {
        hfp_link_data*  link;
        for (link = theHfp->links; link < &theHfp->links[theHfp->num_links]; link++)
        {
            hfpLinkReset(link, FALSE);
        }
        unmarshalled = 0;
    }
}

/****************************************************************************
NAME    
    hfpMarshal

DESCRIPTION
    Marshal the data associated with HFP connections

RETURNS
    bool TRUE if HFP module marshalling complete, otherwise FALSE
*/
static bool hfpMarshal(const tp_bdaddr *tp_bd_addr,
                       uint8 *buf,
                       uint16 length,
                       uint16 *written)
{
    bool validLink = TRUE;
    uint8 channel = RFC_INVALID_SERV_CHANNEL;

    /* Check we have a valid link */
    hfp_link_data *link = hfpGetLinkFromBdaddr(&tp_bd_addr->taddr.addr);

    if(link)
    {
        /* check for a valid RFC channel */
        channel = getRemoteServerChannel(link);
        if (channel == RFC_INVALID_SERV_CHANNEL)
        {
            validLink = FALSE;
        }
    }
    else
    {
        validLink = FALSE;
    }

    if(validLink)
    {
        bool marshalled;
        marshaller_t marshaller = MarshalInit(mtd_hfp, HFP_MARSHAL_OBJ_TYPE_COUNT);
        hfp_marshalled_obj obj;
        obj.link = link;
        obj.channel = channel;
        obj.bitfields = theHfp->bitfields;

        MarshalSetBuffer(marshaller, (void *) buf, length);

        marshalled = Marshal(marshaller, &obj, MARSHAL_TYPE(hfp_marshalled_obj));

        *written = marshalled ? MarshalProduced(marshaller) : 0;

        MarshalDestroy(marshaller, FALSE);
        return marshalled;
    }
    else
    {
        /* Link not valid, nothing to marshal */
        *written = 0;
        return TRUE;
    }
}

/****************************************************************************
NAME
    hfpUnmarshal

DESCRIPTION
    Unmarshal the data associated with the HFP connections

RETURNS
    bool TRUE if HFP unmarshalling complete, otherwise FALSE
*/
static bool hfpUnmarshal(const tp_bdaddr *tp_bd_addr,
                         const uint8 *buf,
                         uint16 length,
                         uint16 *consumed)
{
    marshal_type_t unmarshalled_type;
    hfp_marshalled_obj *data = NULL;

    unmarshaller_t unmarshaller = UnmarshalInit(mtd_hfp, HFP_MARSHAL_OBJ_TYPE_COUNT);
    UnmarshalSetBuffer(unmarshaller, (void *) buf, length);

    if (Unmarshal(unmarshaller, (void **) &data, &unmarshalled_type))
    {
        hfp_link_data *new_link = hfpGetIdleLink();

        PanicFalse(unmarshalled_type == MARSHAL_TYPE(hfp_marshalled_obj));
        PanicNull(data);
        PanicNull(new_link);

        theHfp->bitfields = data->bitfields;
        *new_link = *(data->link);
        new_link->identifier.bd_addr = tp_bd_addr->taddr.addr;
        /* Temporarily store the channel in the sink, convert it on commit */
        new_link->identifier.sink = (Sink)(data->channel);

        *consumed = UnmarshalConsumed(unmarshaller);
        UnmarshalDestroy(unmarshaller, TRUE);
        unmarshalled = 1;
        return TRUE;
    }
    else
    {
        *consumed = 0;
        UnmarshalDestroy(unmarshaller, TRUE);
        return FALSE;
    }
}


/****************************************************************************
NAME    
    hfpVeto

DESCRIPTION
    Veto check for HFP library

    Prior to handover commencing this function is called and
    the libraries internal state is checked to determine if the
    handover should proceed.

RETURNS
    bool TRUE if the HFP Library wishes to veto the handover attempt.
*/
bool hfpVeto( void )
{
    hfp_link_data* link;

    /* Check the HFP library is initialized */
    if ( !theHfp )
    {
        return TRUE;
    }

    hfp_link_priority priority;
    for_all_priorities(priority)
    {
        /* Check if an AT command response is pending from the AG.  */
        link = hfpGetLinkFromPriority(priority);
        if (link && ((link->bitfields.at_cmd_resp_pending != hfpNoCmdPending) ||
           (hfpLinkInSteadyState(link) == FALSE)))
        {
            return TRUE;
        }
    }

    /* Check message queue status */
    if(MessagesPendingForTask(&theHfp->task, NULL) != 0)
    {
        return TRUE;
    }

    return FALSE;
}

/****************************************************************************
NAME    
    hfpHandoverCommit

DESCRIPTION
    The HFP library performs time-critical actions to commit to the specified
    new role (primary or secondary)

RETURNS
    void
*/
static void hfpHandoverCommit(const tp_bdaddr *tp_bd_addr, const bool newRole)
{
    if (newRole)
    {
        Sink sink;
        Source src;
        uint16 conn_id;
        hfp_link_data *link = hfpGetLinkFromBdaddr(&tp_bd_addr->taddr.addr);
        if (link)
        {
            /* Convert the sink - the Bluestack instance exists at this point */
            uint8 channel = (uint8)(((uintptr)(link->identifier.sink) & 0xFF));
            sink = StreamRfcommSinkFromServerChannel(tp_bd_addr, channel);
            if(sink)
            {
                link->identifier.sink = sink;

                conn_id = SinkGetRfcommConnId(sink);
                PanicFalse(VmOverrideRfcommConnContext(conn_id, (conn_context_t)&theHfp->task));

                /* Stitch the RFCOMM sink and the task */
                MessageStreamTaskFromSink(sink, &theHfp->task);

                /* Set the handover policy on the stream */
                src = StreamSourceFromSink(sink);
                hfpSourceConfigureHandoverPolicy(src, SOURCE_HANDOVER_ALLOW_WITHOUT_DATA);
            }
            else
            {
                DEBUG_LOG("hfpHandoverCommit no sink for lap=0x%x", tp_bd_addr->taddr.addr.lap);
            }
        }
        else
        {
            DEBUG_LOG("hfpHandoverCommit no link for lap=0x%x", tp_bd_addr->taddr.addr.lap);
        }
    }
    unmarshalled = 0;
}

/****************************************************************************
NAME
    hfpHandoverComplete

DESCRIPTION
    freeing memory allocated during the unmarshalling process.

RETURNS
    void

*/
static void hfpHandoverComplete( const bool newRole )
{
    UNUSED(newRole);
}
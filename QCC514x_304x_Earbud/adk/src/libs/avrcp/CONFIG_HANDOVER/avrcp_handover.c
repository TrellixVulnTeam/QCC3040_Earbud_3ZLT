/****************************************************************************
Copyright (c) 2019-2020 Qualcomm Technologies International, Ltd.


FILE NAME
    avrcp_handover.c

DESCRIPTION
    Implements AVRCP handover logic (Veto, Marshals/Unmarshals, Handover, etc).

NOTES
    See handover_if.h for further interface description 

    Builds requiring this should include CONFIG_HANDOVER in the
    makefile. e.g.
        CONFIG_FEATURES:=CONFIG_HANDOVER    
*/


#include "avrcp_marshal_desc.h"
#include "avrcp_private.h"
#include "avrcp_init.h"
#include "avrcp_profile_handler.h"
#include "avrcp_handover_policy.h"

#include "marshal.h"
#include <sink.h>
#include <source.h>
#include <stream.h>
#include <panic.h>
#include <stdlib.h>
#include <logging.h>
#include <bdaddr.h>

typedef struct avrcp_marshal_instance_info_tag
{
    unmarshaller_t unmarshaller;
    AVRCP *avrcp;
    tp_bdaddr bd_addr;
    struct avrcp_marshal_instance_info_tag *next;
} avrcp_marshal_instance_info_t;

avrcp_marshal_instance_info_t *avrcpMarshalInstanceHead = NULL;

/* Iterate over all instance */
#define for_all_instance(instance)             for(instance = avrcpMarshalInstanceHead; instance; instance = instance->next)

static bool avrcpVeto( void );
static bool avrcpMarshal(const tp_bdaddr *tp_bd_addr,
                         uint8 *buf,
                         uint16 length,
                         uint16 *written);
static bool avrcpUnmarshal(const tp_bdaddr *tp_bd_addr,
                           const uint8 *buf,
                           uint16 length,
                           uint16 *consumed);
static void avrcpHandoverCommit(const tp_bdaddr *tp_bd_addr, const bool newRole);
static void avrcpHandoverComplete( const bool newRole );
static void avrcpHandoverAbort(void);

static avrcp_marshal_instance_info_t* avrcpCreateMarshalInstance(const tp_bdaddr *tp_bd_addr);

static avrcp_marshal_instance_info_t* avrcpGetMarshalInstance(const tp_bdaddr *tp_bd_addr);

static void avrcpDestroyAllInstance(void);

static avrcp_marshal_instance_info_t* avrcpGetOrCreateMarshalInstance(const tp_bdaddr *tp_bd_addr);


const handover_interface avrcp_handover =
{
    avrcpVeto,
    avrcpMarshal,
    avrcpUnmarshal,
    avrcpHandoverCommit,
    avrcpHandoverComplete,
    avrcpHandoverAbort
};

/****************************************************************************
NAME    
    browsingSupported

DESCRIPTION
    Finds out whether browsing is supported or not 

RETURNS
    bool TRUE if browsing supported
*/
static bool browsingSupported(void)
{
    AvrcpDeviceTask *avrcp_device_task = avrcpGetDeviceTask();

    return (isAvrcpBrowsingEnabled(avrcp_device_task) ||
            isAvrcpTargetCat1Supported(avrcp_device_task) ||
            isAvrcpTargetCat3Supported(avrcp_device_task));
}

/****************************************************************************
NAME    
    stitchAvrcp

DESCRIPTION
    Stitch an unmarshalled AVRCP connection instance 

RETURNS
    void
*/
static void stitchAvrcp(AVRCP *unmarshalled_avrcp)
{
    unmarshalled_avrcp->task.handler = avrcpProfileHandler;
    unmarshalled_avrcp->dataFreeTask.cleanUpTask.handler = avrcpDataCleanUp;

    if (browsingSupported())
    {
        AVRCP_AVBP_INIT *avrcp_avbp = (AVRCP_AVBP_INIT *) unmarshalled_avrcp;

        avrcp_avbp->avbp.task.handler = avbpProfileHandler;
        avrcp_avbp->avrcp.avbp_task = &avrcp_avbp->avbp.task;
        avrcp_avbp->avbp.avrcp_task = &avrcp_avbp->avrcp.task;
    }

    /* Initialize the connection context for the relevant connection id */
    configureL2capSinkFromMarshalledSinkCid(&unmarshalled_avrcp->sink, &unmarshalled_avrcp->task);
}

/****************************************************************************
NAME    
    avrcpHandoverAbort

DESCRIPTION
    Abort the AVRCP library Handover process, free any memory
    associated with the marshalling process.

RETURNS
    void
*/
static void avrcpHandoverAbort(void)
{
    avrcp_marshal_instance_info_t *instance;
    for_all_instance(instance)
    {
        UnmarshalDestroy(instance->unmarshaller, TRUE);
    }
    avrcpDestroyAllInstance();
}

/****************************************************************************
NAME    
    avrcpMarshal

DESCRIPTION
    Marshal the data associated with AVRCP connections

RETURNS
    bool TRUE if AVRCP module marshalling complete, otherwise FALSE
*/
static bool avrcpMarshal(const tp_bdaddr *tp_bd_addr,
                         uint8 *buf,
                         uint16 length,
                         uint16 *written)
{
    AVRCP *avrcp = NULL;
    if (AvrcpGetInstanceFromBdaddr(&tp_bd_addr->taddr.addr, &avrcp))
    {
        bool marshalled;
        marshaller_t marshaller = MarshalInit(mtd_avrcp, AVRCP_MARSHAL_OBJ_TYPE_COUNT);
        PanicNull(marshaller);

        MarshalSetBuffer(marshaller, (void *) buf, length);

        marshalled = Marshal(marshaller, avrcp,
                             browsingSupported() ? MARSHAL_TYPE(AVRCP_AVBP_INIT) :
                                                   MARSHAL_TYPE(AVRCP));

        *written = marshalled ? MarshalProduced(marshaller) : 0;

        MarshalDestroy(marshaller, FALSE);
        return marshalled;
    }
    else
    {
        *written = 0;
        return TRUE;
    }
}

/****************************************************************************
NAME    
    avrcpUnmarshal

DESCRIPTION
    Unmarshal the data associated with AVRCP connections

RETURNS
    bool TRUE if AVRCP unmarshalling complete, otherwise FALSE
*/
static bool avrcpUnmarshal(const tp_bdaddr *tp_bd_addr,
                           const uint8 *buf,
                           uint16 length,
                           uint16 *consumed)
{
    marshal_type_t unmarshalled_type;

    /* Initiating unmarshalling, initialize the instance */
    avrcp_marshal_instance_info_t* instance = avrcpGetOrCreateMarshalInstance(tp_bd_addr);

    UnmarshalSetBuffer(instance->unmarshaller, (void *) buf, length);

    if (Unmarshal(instance->unmarshaller,
                  (void**)&instance->avrcp,
                  &unmarshalled_type))
    {
        PanicFalse(unmarshalled_type == MARSHAL_TYPE(AVRCP) ||
                   unmarshalled_type == MARSHAL_TYPE(AVRCP_AVBP_INIT));

        *consumed = UnmarshalConsumed(instance->unmarshaller);

        /* Only expecting one object, so unmarshalling is complete */
        return TRUE;

    }
    else
    {
        *consumed = UnmarshalConsumed(instance->unmarshaller);
        return FALSE;
    }
}

/****************************************************************************
NAME    
    avrcpHandoverCommit

DESCRIPTION
    The AVRCP  library performs time-critical actions to commit to the specified
    new role (primary or secondary)

RETURNS
    void
*/
static void avrcpHandoverCommit(const tp_bdaddr *tp_bd_addr, const bool newRole)
{
    if (newRole)
    {
        avrcp_marshal_instance_info_t* instance = avrcpGetMarshalInstance(tp_bd_addr);
        
        /* If there is a marshalled instance handle it */
        if(instance)
        {
            /* Stitch unmarshalled AVRCP connection instance */
            stitchAvrcp(instance->avrcp);
            
            /* Add to the connection list */
            avrcpAddTaskToList(instance->avrcp, &instance->bd_addr.taddr.addr, instance->avrcp->bitfields.connection_incoming);
            
            /* Set the handover policy */
            Source src = StreamSourceFromSink(instance->avrcp->sink);
            avrcpSourceConfigureHandoverPolicy(src, SOURCE_HANDOVER_ALLOW_WITHOUT_DATA);
            
            UnmarshalDestroy(instance->unmarshaller, FALSE);
            instance->unmarshaller = NULL;
        }
    }
}

/****************************************************************************
NAME    
    avrcpHandoverComplete

DESCRIPTION
    freeing memory allocated during the unmarshalling process.

RETURNS
    void
*/
static void avrcpHandoverComplete( const bool newRole )
{
    if (newRole)
    {
        avrcpDestroyAllInstance();
    }
}

/****************************************************************************
NAME    
    avrcpVeto

DESCRIPTION
    Veto check for AVRCP library

    Prior to handover commencing this function is called and
    the libraries internal state is checked to determine if the
    handover should proceed.

RETURNS
    bool TRUE if the AVRCP Library wishes to veto the handover attempt.
*/
static bool avrcpVeto( void )
{
    avrcpList *list = avrcpListHead;
    AvrcpDeviceTask *avrcp_device_task = avrcpGetDeviceTask();
    avrcp_device_role device_role = avrcp_device_task->bitfields.device_type;

    /* If AVRCP library initialization is not complete or AvrcpInit has not been
     * called the set device role will not be set */
    if (device_role != avrcp_target &&
        device_role != avrcp_controller &&
        device_role != avrcp_target_and_controller  )
    {
        return TRUE;
    }

	
    /* Messages on the AVRCP app task are not checked during veto.
     * It is the application's responsibility to check for any messages on this
     * task that it deems require a veto */


    /* Per instance veto check */
    while (list != NULL)
    {
        AVRCP *avrcp = list->avrcp;

        /* Check whether there is a connection in progress. */
        if(avrcp->bitfields.state == avrcpConnecting)
        {
            DEBUG_LOG_INFO("avrcpVeto connecting");
            return TRUE;
        }

        if (MessagesPendingForTask(&avrcp->task, NULL))
        {
            DEBUG_LOG_INFO("avrcpVeto messages pending on avrcp->task");
            return TRUE;
        }


        list = list->next;
    }

    return FALSE;
}


/****************************************************************************
NAME
    avrcpGetOrCreateMarshalInstance

DESCRIPTION
    Check the AVRCP marshal instance 
    if the marshal instance existed return the instance
    else create a new instance associated with tp_bd_addr
RETURNS
    instance or panic
*/
static avrcp_marshal_instance_info_t* avrcpGetOrCreateMarshalInstance(const tp_bdaddr *tp_bd_addr)
{
    avrcp_marshal_instance_info_t* instance = avrcpGetMarshalInstance(tp_bd_addr);
    if (NULL == instance)
    {
        instance = avrcpCreateMarshalInstance(tp_bd_addr);
        instance->unmarshaller = UnmarshalInit(mtd_avrcp, AVRCP_MARSHAL_OBJ_TYPE_COUNT);
    }
    return instance;
}

/****************************************************************************
NAME
    avrcpGetMarshalInstance

DESCRIPTION
    Get the existed marshal instance
    associated with tp_bd_addr

RETURNS
    instance
*/
static avrcp_marshal_instance_info_t* avrcpGetMarshalInstance(const tp_bdaddr *tp_bd_addr)
{
    avrcp_marshal_instance_info_t *instance;
    for_all_instance(instance)
    {
        if (BdaddrTpIsSame(&instance->bd_addr, tp_bd_addr))
        {
            break;
        }
    }
    return instance;
}

/****************************************************************************
NAME
    avrcpCreateMarshalInstance

DESCRIPTION
    Create new marshal instance associated with tp_bd_addr

RETURNS
    instance or panic
*/
static avrcp_marshal_instance_info_t* avrcpCreateMarshalInstance(const tp_bdaddr *tp_bd_addr)
{
    avrcp_marshal_instance_info_t* instance = PanicUnlessNew(avrcp_marshal_instance_info_t);
    instance->bd_addr = *tp_bd_addr;
    instance->next = avrcpMarshalInstanceHead;
    avrcpMarshalInstanceHead = instance;
    return instance;
}

/****************************************************************************
NAME
    avrcpDestroyAllInstance

DESCRIPTION
    Destroy all AVRCP marshalled instance

RETURNS
    void
*/
static void avrcpDestroyAllInstance(void)
{
    avrcp_marshal_instance_info_t *currentinstance, *nextinstance = NULL;
    for(currentinstance = avrcpMarshalInstanceHead; currentinstance ; currentinstance = nextinstance)
    {
        nextinstance = currentinstance->next;
        free(currentinstance);
    }
    avrcpMarshalInstanceHead = NULL;
}


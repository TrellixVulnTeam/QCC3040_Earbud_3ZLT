/*
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       fast_pair_handover.c
\brief      Fast Pair Handover functions are defined
*/

#if defined(INCLUDE_MIRRORING) && defined( INCLUDE_FAST_PAIR)

#include <panic.h>
#include <stdlib.h>
#include <sink.h>
#include <stream.h>
#include <source.h>
#include "marshal.h"
#include "fast_pair_rfcomm.h"
#include "fast_pair_msg_stream.h"
#include "fast_pair_msg_stream_dev_info.h"
#include "fast_pair_marshal_desc.h"
#include "handover_if.h"
#include "logging.h"


/*! Use this flag to clean unmarshalled data, if any, during handover abort phase */
static bool unmarshalled = FALSE;

static bool FastPair_Veto(void)
{
    if(fastPair_MsgStreamIsBusy())
    {
        DEBUG_LOG("FastPair_Veto called. Veto it as busy");
        return TRUE;
    }
    return FALSE;
}


static bool FastPair_Marshal(const tp_bdaddr *tp_bd_addr,
                             uint8 *buf,
                             uint16 length,
                             uint16 *written)
{
    DEBUG_LOG("FastPair_Marshal");

    /* Marshal only the connected RFcomm connection info */
    if (fastPair_RfcommIsConnectedForAddr(&(tp_bd_addr->taddr.addr)))
    {
        bool marshalled;
        fast_pair_marshal_data obj;
        obj.rfcomm_channel = fastPair_RfcommGetRFCommChannel(&(tp_bd_addr->taddr.addr));
        obj.dev_info = fastPair_MsgStreamDevInfo_Get();

        DEBUG_LOG("FastPair_Marshal: Marashalling rfcomm info for addr[0x%06x]", tp_bd_addr->taddr.addr.lap);

        marshaller_t marshaller = MarshalInit(mtd_fast_pair, FAST_PAIR_MARSHAL_OBJ_TYPE_COUNT);
        MarshalSetBuffer(marshaller, (void*)buf, length);
        marshalled = Marshal(marshaller, &obj, MARSHAL_TYPE(fast_pair_marshal_data));
        *written = marshalled? MarshalProduced(marshaller): 0;
        MarshalDestroy(marshaller, FALSE);
        return marshalled;
    }
    *written = 0;
    return TRUE;
}


static bool FastPair_Unmarshal(const tp_bdaddr *tp_bd_addr,
                                                 const uint8 *buf,
                                                 uint16 length,
                                                 uint16 *consumed)
{
    marshal_type_t unmarshalled_type;
    fast_pair_marshal_data *data = NULL;

    DEBUG_LOG("FastPair_Unmarshal");

    unmarshaller_t unmarshaller = UnmarshalInit(mtd_fast_pair, FAST_PAIR_MARSHAL_OBJ_TYPE_COUNT);
    UnmarshalSetBuffer(unmarshaller, (void *)buf, length);

    if (Unmarshal(unmarshaller, (void **)&data, &unmarshalled_type))
    {
        PanicFalse(unmarshalled_type == MARSHAL_TYPE(fast_pair_marshal_data));
        PanicNull(data);

        fast_pair_rfcomm_data_t *theInstance =  fastPair_RfcommCreateInstance(&(tp_bd_addr->taddr.addr));
        PanicNull(theInstance);
        theInstance->server_channel = data->rfcomm_channel;

        fastPair_MsgStreamDevInfo_Set(data->dev_info);
        unmarshalled = TRUE;
        *consumed = UnmarshalConsumed(unmarshaller);
        UnmarshalDestroy(unmarshaller, TRUE);
        return TRUE;
    }
    else
    {
        *consumed = 0;
        DEBUG_LOG("FastPair_Unmarshal: failed unmarshal");
        UnmarshalDestroy(unmarshaller, TRUE);
        return FALSE;
    }
}


static void FastPair_HandoverCommit(const tp_bdaddr *tp_bd_addr, const bool is_primary)
{
    DEBUG_LOG("FastPair_HandoverCommit is_primary[%d] ", is_primary);

    if (is_primary)
    {
        /* Restore the handovered active rfcomm connections */
        if (fastPair_RfcommRestoreAfterHandover(&(tp_bd_addr->taddr.addr)))
        {
            DEBUG_LOG("FastPair_HandoverCommit: Handover Done for addr[0x%06x] ", tp_bd_addr->taddr.addr.lap);
        }
    }
}

static void FastPair_HandoverComplete(const bool is_primary )
{
    UNUSED(is_primary);
	/* mark complete of unmarshalled data*/
	unmarshalled = FALSE;
}

static void FastPair_HandoverAbort(void)
{
    DEBUG_LOG("FastPair_HandoverAbort");
    if (unmarshalled)
    {
        DEBUG_LOG("FastPair_HandoverAbort: cleaning up all unmarshalled data");
        fastPair_RfcommDestroyAllInstances();
        fast_pair_msg_stream_dev_info dev_info = {0};
        fastPair_MsgStreamDevInfo_Set(dev_info);
        unmarshalled = FALSE;
    }
}

const handover_interface fast_pair_handover_if =  {
    &FastPair_Veto,
    &FastPair_Marshal,
    &FastPair_Unmarshal,
    &FastPair_HandoverCommit,
    &FastPair_HandoverComplete,
    &FastPair_HandoverAbort
};

#endif

/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       fast_pair_pname_state.c
\brief      Fast Pair Personalized Name State Event handling
*/


#include "fast_pair_pname_state.h"
#include "fast_pair_session_data.h"

#include "cryptoalgo.h"



static bool fastpair_StatePNameProcessACLDisconnect(fast_pair_state_event_disconnect_args_t* args)
{
    bool status = FALSE;
    uint8 index;
    fastPairTaskData *theFastPair;

    DEBUG_LOG("fastpair_StatePNameProcessACLDisconnect");

    theFastPair = fastPair_GetTaskData();

    if(args->disconnect_ind->tpaddr.transport == TRANSPORT_BLE_ACL)
    {
        memset(&theFastPair->rpa_bd_addr, 0x0, sizeof(bdaddr));

        for(index = 0; index < MAX_BLE_CONNECTIONS; index++)
        {
            if(BdaddrIsSame(&theFastPair->peer_bd_addr[index], &args->disconnect_ind->tpaddr.taddr.addr))
            {
                DEBUG_LOG("fastpair_StateWaitAccountKeyProcessACLDisconnect. Reseting peer BD address and own RPA of index %x", index);
                memset(&theFastPair->peer_bd_addr[index], 0x0, sizeof(bdaddr));
                memset(&theFastPair->own_random_address[index], 0x0, sizeof(bdaddr));
            }
        }
        status = TRUE;
    }
    return status;
}

static bool fastPair_StatePNameWrite(const uint8* pname_data, uint8 pname_len)
{
    if((pname_len ==0)|| (pname_len>FAST_PAIR_PNAME_DATA_LEN))
    {
        DEBUG_LOG_ERROR("fastPair_StatePNameWrite: Wrong pname length. Value is %d ",pname_len);
        return FALSE;
    }

    DEBUG_LOG("fastPair_StorePName : Store personalized name of size %d :",pname_len);

    for(uint16 i=0; i<pname_len; i++)
    {
        DEBUG_LOG("%02x ",pname_data[i]);
    }

    /* Store the data in persistent storage */
    fastPair_StorePName(pname_data,pname_len);
    /* Marshall the account key */
    FastPair_PNameSync_Sync();

    return TRUE;
}

/* Read the personalized name from the persistent staorage*/
bool fastPair_GetPName(uint8 pname[FAST_PAIR_PNAME_DATA_LEN], uint8 *pname_len)
{
    if(FALSE == fastPair_GetPNameFromStore(pname,pname_len))
    {
        DEBUG_LOG("fastPair_GetPName: Error in getting stored PName ");
        return FALSE;
    }

    DEBUG_LOG("fastPair_GetPName : Stored personalized name of size %d ",*pname_len);

    return TRUE;
}

bool fastPair_StatePNameHandleEvent(fast_pair_state_event_t event)
{
    bool status = FALSE;
    fastPairTaskData *theFastPair;

    theFastPair = fastPair_GetTaskData();
    DEBUG_LOG("fastPair_StatePNameHandleEvent event [%d]", event.id);

    switch (event.id)
    {
        case fast_pair_state_event_disconnect:
        {
            if(event.args == NULL)
            {
                return FALSE;
            }
            status = fastpair_StatePNameProcessACLDisconnect((fast_pair_state_event_disconnect_args_t*)event.args);
        }
        break;
        
        case fast_pair_state_event_timer_expire:
        {
            fastPair_SetState(theFastPair, FAST_PAIR_STATE_IDLE);
            status = TRUE;
        }
        break;
        
        case fast_pair_state_event_pname_write:
        {
            fast_pair_state_event_pname_write_args_t *args = (fast_pair_state_event_pname_write_args_t *)event.args;
            if (args == NULL)
            {
                return FALSE;
            }
            status = fastPair_StatePNameWrite(args->pname_data,(uint8)args->size);
        }
        break;

        case fast_pair_state_event_power_off:
        {
            fastPair_SetState(theFastPair, FAST_PAIR_STATE_IDLE);
        }
        break;
        
        default:
        {
            DEBUG_LOG("Unhandled event [%d]\n", event.id);
        }
        break;
    }
    return status;
}

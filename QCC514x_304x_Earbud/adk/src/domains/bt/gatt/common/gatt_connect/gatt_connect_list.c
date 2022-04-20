/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Tracking GATT connections
*/

#include "gatt_connect.h"
#include "gatt_connect_list.h"

#include <bdaddr.h>

static gatt_connection_t connections[GATT_CONNECT_MAX_CONNECTIONS];

gatt_connection_t* GattConnect_FindConnectionFromCid(unsigned cid)
{
    gatt_connection_t* result;
    
    for(result = &connections[0]; result <= &connections[GATT_CONNECT_MAX_CONNECTIONS - 1]; result++)
    {
        if(result->cid == cid)
            return result;
    }
    
    return NULL;
}

gatt_connection_t* GattConnect_CreateConnection(unsigned cid)
{
    gatt_connection_t* connection = GattConnect_FindConnectionFromCid(0);
    tp_bdaddr tpaddr;
    
    if(connection)
    {
        connection->cid = cid;
        if (VmGetBdAddrtFromCid(gattConnect_GetCid(cid), &tpaddr))
        {
            if (tpaddr.taddr.type != TYPED_BDADDR_PUBLIC)
            {
                tp_bdaddr public_tpaddr;

                if (VmGetPublicAddress(&tpaddr, &public_tpaddr))
                {
                    tpaddr = public_tpaddr;
                }
            }

            connection->tpaddr = tpaddr;
        }
        else
        {
            BdaddrTpSetEmpty(&connection->tpaddr);
        }
    }
    
    return connection;
}

void GattConnect_DestroyConnection(unsigned cid)
{
    gatt_connection_t* connection = GattConnect_FindConnectionFromCid(cid);
    
    if(connection)
    {
        memset(connection, 0, sizeof(gatt_connection_t));
    }
}

void GattConnect_ListInit(void)
{
    memset(connections, 0, sizeof(connections));
}

gatt_connection_t* GattConnect_FindConnectionFromTpaddr(const tp_bdaddr *tpaddr_in)
{
    gatt_connection_t* result;
    tp_bdaddr tpaddr_for_cid;
    
    for(result = &connections[0]; result <= &connections[GATT_CONNECT_MAX_CONNECTIONS - 1]; result++)
    {
        if (VmGetBdAddrtFromCid(gattConnect_GetCid(result->cid), &tpaddr_for_cid))
        {
            if (BdaddrTpIsSame(tpaddr_in, &tpaddr_for_cid))
            {
                return result;
            }
        }
    }
    
    return NULL;
}

bool GattConnect_FindTpaddrFromCid(unsigned cid, tp_bdaddr * tpaddr)
{
    bool result = FALSE;
    gatt_connection_t* connection = GattConnect_FindConnectionFromCid(cid);
    
    if (connection)
    {
        if (!BdaddrTpIsEmpty(&connection->tpaddr))
        {
            *tpaddr = connection->tpaddr;
            
            result = TRUE;
        }
    }
    
    return result;
}

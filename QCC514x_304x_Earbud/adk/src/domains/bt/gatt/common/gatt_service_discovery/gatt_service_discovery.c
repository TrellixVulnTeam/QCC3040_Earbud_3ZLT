/*!
\copyright  Copyright (c) 2019 - 2020 Qualcomm Technologies International, Ltd.
All Rights Reserved.
Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief   Gatt Service Discovery implementation.
*/

#include <logging.h>
#include <panic.h>

#include "gatt_service_discovery.h"
#include "gatt_service_discovery_private.h"
#include "gatt_connect.h"

/*! \brief gatt service discovery state information */
typedef struct
{
    client_id_t current_client_id;
    service_discovery_status gsd_status;
    uint16 current_service_index;
}gatt_service_discovery_state_t;

typedef struct
{
    const client_id_t** clients;
    uint8               size;
    uint8*              num_service_instances;
}gatt_client_priority_list_t;

static void GattServiceDisovery_HandleMessage(Task task, MessageId id, Message message);
static void gattServiceDiscovery_ProcessClientPriorityList(gatt_cid_t cid);

static TaskData gsd_taskdata = {.handler = GattServiceDisovery_HandleMessage};
static gatt_service_discovery_state_t discovery_state = {0};
static gatt_client_priority_list_t client_priority_list = {NULL, 0, NULL};

static void gattServiceDiscovery_AppDiscoveryGattConnect(gatt_cid_t cid)
{
    GattServiceDiscovery_StartDiscovery(cid);
}

static void gattServiceDiscovery_AppDiscoveryGattDisconnect(gatt_cid_t cid)
{
    DEBUG_LOG("gattServiceDiscovery_AppDiscoveryGattDisconnect: cid=0x%04X", cid);
    GattServiceDiscovery_DestroyClients(cid);
}

static const gatt_connect_observer_callback_t gatt_observer_callback =
{
    .OnConnection = gattServiceDiscovery_AppDiscoveryGattConnect,
    .OnDisconnection = gattServiceDiscovery_AppDiscoveryGattDisconnect
};

static inline bool gattServiceDiscovery_CheckServiceDiscoveryContinueConditions(gatt_status_t status, bool more_to_come)
{
    bool multiple_services_discovered = (status == gatt_status_success) && more_to_come;
    bool stop_discovery_requested = GattClient_GetDiscoveryStopRequest(discovery_state.current_client_id);
    bool more_clients_to_process = (stop_discovery_requested == FALSE) && ((discovery_state.current_service_index + 1) < client_priority_list.size);

    if (!more_clients_to_process)
    {
        discovery_state.gsd_status = gsd_in_idle;
    }

    /* Ok to continue if we're NOT in the middle of a multiple service find AND there are more clients to process */
    return !multiple_services_discovered && more_clients_to_process;
}

static void gattServiceDiscovery_HandlePrimaryServiceDiscovery(const GATT_DISCOVER_PRIMARY_SERVICE_CFM_T* discovery)
{
    if(discovery->status == gatt_status_success)
    {
        /* Attach client to the discovered service */
        client_priority_list.num_service_instances[discovery_state.current_service_index]++;
        GattClient_AttachClient(discovery_state.current_client_id,discovery->cid,discovery->handle,discovery->end);
    }

    if(gattServiceDiscovery_CheckServiceDiscoveryContinueConditions(discovery->status, discovery->more_to_come))
    {
        /* Continue to iterate over client list */
        discovery_state.current_service_index++;
        gattServiceDiscovery_ProcessClientPriorityList(discovery->cid);
    }
}

static void GattServiceDisovery_HandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch (id)
    {
        /*! ---- GATT messages ---- */
        case GATT_DISCOVER_PRIMARY_SERVICE_CFM:
            gattServiceDiscovery_HandlePrimaryServiceDiscovery((const GATT_DISCOVER_PRIMARY_SERVICE_CFM_T* )message);
            break;

        default:
            break;
    }
}

static void gattServiceDiscovery_ProcessClientPriorityList(gatt_cid_t cid)
{
    gatt_client_service_t primary_service;
    client_id_t client_id;

    client_id = *client_priority_list.clients[discovery_state.current_service_index];

    /*! Update the discovery state machine with this client id */
    discovery_state.current_client_id = client_id;

    /*! Retrieve the service that needs to be discovered for this client */
    primary_service = GattClient_GetServiceForDiscovery(client_id);

    /*! Initiate the Primary Service discovery procedure */
    GattDiscoverPrimaryServiceRequest(&gsd_taskdata,
                                       cid,
                                       primary_service.uuid_type,
                                       primary_service.service_uuid);
}

void gattServiceDiscovery_Init(const client_id_t* gatt_client_prioritised_id[], uint8 num_elements)
{
    /* Init may be called once only */
    PanicNotZero(client_priority_list.clients);

    client_priority_list.clients = gatt_client_prioritised_id;
    client_priority_list.size = num_elements;
    client_priority_list.num_service_instances = PanicUnlessMalloc(sizeof(*client_priority_list.num_service_instances)*num_elements);
    discovery_state.gsd_status = gsd_in_idle;

    GattConnect_RegisterObserver(&gatt_observer_callback);
}

bool GattServiceDiscovery_StartDiscovery(gatt_cid_t cid)
{
    PanicNull(client_priority_list.clients);

    if (discovery_state.gsd_status == gsd_in_idle)
    {
        discovery_state.gsd_status = gsd_in_progress;

        /* Start processing the priority clients one by one */
        gattServiceDiscovery_ProcessClientPriorityList(cid);
    }

    return (discovery_state.gsd_status == gsd_in_progress);
}

bool GattServiceDiscovery_DestroyClients(gatt_cid_t cid)
{
    PanicNull(client_priority_list.clients);
    uint8 client_index = 0;

    if(discovery_state.gsd_status != gsd_in_progress)
    {
        /*! Iterate through all the client list */
        for (client_index = 0; client_index < client_priority_list.size; client_index++)
        {
            client_id_t client_id = *client_priority_list.clients[client_index];
            uint8 service_instance = 0;

            for (service_instance = 0; service_instance < client_priority_list.num_service_instances[client_index]; service_instance++)
            {
                /* Each service may have multiple service instances */
                GattClient_DetachClient(client_id, cid);
            }
        }
        
        discovery_state.current_service_index = 0;
    }

    return (client_index == client_priority_list.size);
}

service_discovery_status GattServiceDiscovery_GetStatus(void)
{
    return discovery_state.gsd_status;
}

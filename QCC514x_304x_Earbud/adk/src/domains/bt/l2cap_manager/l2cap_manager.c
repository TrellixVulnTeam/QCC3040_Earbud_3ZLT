/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       l2cap_manager.c
\brief	    Implementation of module providing L2CAP connections.
*/

#ifdef INCLUDE_L2CAP_MANAGER
#include "l2cap_manager_private.h"

/* Enable debug log outputs with per-module debug log levels.
 * The log output level for this module can be changed with the PyDbg command:
 *      >>> apps1.log_level("l2cap_manager", 3)
 * Where the second parameter value means:
 *      0:ERROR, 1:WARN, 2:NORMAL(= INFO), 3:VERBOSE(= DEBUG), 4:V_VERBOSE(= VERBOSE), 5:V_V_VERBOSE(= V_VERBOSE)
 * See 'logging.h' and PyDbg 'log_level()' command descriptions for details. */
#define DEBUG_LOG_MODULE_NAME l2cap_manager
#include <logging.h>
DEBUG_LOG_DEFINE_LEVEL_VAR

#include <bluestack/l2cap_prim.h>
#include <bt_device.h>
#include <connection.h>
#include <domain_message.h>
#include <panic.h>
#include <service.h>
#include <sink.h>
#include <source.h>
#include <stream.h>
#include <task_list.h>

#include <stdlib.h>



/******************************************************************************
 * General Definitions
 ******************************************************************************/
#ifdef HOSTED_TEST_ENVIRONMENT
#define STATIC_FOR_TARGET
#else
#define STATIC_FOR_TARGET   static
#endif

#define L2CAP_MANAGER_SET_TYPED_BDADDR(DEST, TRANSPORT, TYPE, ADDR) \
    do { \
        (DEST).transport  = (TRANSPORT); \
        (DEST).taddr.type = (TYPE); \
        (DEST).taddr.addr = (ADDR); \
    } while (0);


/******************************************************************************
 * Global variables
 ******************************************************************************/
l2cap_manager_task_data_t l2cap_manager_task_data;


/******************************************************************************
 * Instance handling functions
 ******************************************************************************/
/*! \brief Generate an instance ID.

    \param[IN] type     The data type of an element of the linked-list.

    \return Pointer to the new blank element.
*/
STATIC_FOR_TARGET linked_list_key l2capManager_GetNewInstanceId(l2cap_manager_linked_list_type_t type)
{
    static linked_list_key instance_id_counter_psm = 0;
    static linked_list_key instance_id_counter_l2cap_link = 0;
    linked_list_key instance_id;

    switch (type)
    {
        case l2cap_manager_linked_list_type_psm_instance:
            instance_id = L2CAP_MANAGER_INSTANCE_ID_FLAG_PSM | instance_id_counter_psm;
            instance_id_counter_psm += 1;
            instance_id_counter_psm &= L2CAP_MANAGER_INSTANCE_ID_FLAG_ID_FIELD_MASK;
            break;
        case l2cap_manager_linked_list_type_l2cap_link_instance:
            instance_id = L2CAP_MANAGER_INSTANCE_ID_FLAG_L2CAP_LINK | instance_id_counter_l2cap_link;
            instance_id_counter_l2cap_link += 1;
            instance_id_counter_l2cap_link &= L2CAP_MANAGER_INSTANCE_ID_FLAG_ID_FIELD_MASK;
            break;
        default:
            DEBUG_LOG_ERROR("l2capManager GetNewInstanceId: ERROR! Invalid linked-list type: %d", type);
            Panic();
            instance_id = L2CAP_MANAGER_INSTANCE_ID_INVALID;
    }
    return instance_id;
}


/*! \brief Create a new PSM instance and add it to the linked-list.

    \note A unique instance ID will be assigned to the new PSM instance.

    \return Pointer to the new PSM instance.
*/
STATIC_FOR_TARGET l2cap_manager_psm_instance_t* l2capManager_CreatePsmInstance(void)
{
    l2cap_manager_task_data_t *task_inst = l2capManagerGetTaskData();
    l2cap_manager_psm_instance_t *new_element;

    new_element = (l2cap_manager_psm_instance_t*) PanicUnlessMalloc(sizeof(l2cap_manager_psm_instance_t));
    if (new_element)
    {
        if (task_inst->psm_instances)
        {
            l2cap_manager_psm_instance_t *ptr = task_inst->psm_instances;

            while (ptr->next != NULL)
                ptr = ptr->next;
            ptr->next = new_element;
        }
        else
        {
            task_inst->psm_instances = new_element;     /* Add the first element to the linked-list. */
        }
        new_element->next = NULL;
        new_element->instance_id = l2capManager_GetNewInstanceId(l2cap_manager_linked_list_type_psm_instance);
    }

    return new_element;
}


STATIC_FOR_TARGET l2cap_manager_psm_instance_t* l2capManager_SearchPsmInstance(const linked_list_key instance_id)
{
    l2cap_manager_task_data_t *task_inst = l2capManagerGetTaskData();
    l2cap_manager_psm_instance_t *psm_inst = task_inst->psm_instances;

    while (psm_inst != NULL)
    {
        if (psm_inst->instance_id == instance_id)
            return psm_inst;
        psm_inst = psm_inst->next;
    }
    return NULL;
}


STATIC_FOR_TARGET l2cap_manager_psm_instance_t* l2capManager_SearchPsmInstanceByState(const uint16 state)
{
    l2cap_manager_task_data_t *task_inst = l2capManagerGetTaskData();
    l2cap_manager_psm_instance_t *psm_inst = task_inst->psm_instances;

    while (psm_inst != NULL)
    {
        if (psm_inst->state == state)
            return psm_inst;
        psm_inst = psm_inst->next;
    }
    return NULL;
}


STATIC_FOR_TARGET l2cap_manager_psm_instance_t* l2capManager_SearchPsmInstanceByLocalPsm(const uint16 local_psm)
{
    l2cap_manager_task_data_t *task_inst = l2capManagerGetTaskData();
    l2cap_manager_psm_instance_t *psm_inst = task_inst->psm_instances;

    while (psm_inst != NULL)
    {
        if (psm_inst->local_psm == local_psm)
            return psm_inst;
        psm_inst = psm_inst->next;
    }
    return NULL;
}


STATIC_FOR_TARGET bool l2capManager_GetPsmAndL2capInstanceBySink(const Sink sink, l2cap_manager_psm_instance_t **psm_inst, l2cap_manager_l2cap_link_instance_t **l2cap_inst)
{
    l2cap_manager_task_data_t *task_inst = l2capManagerGetTaskData();
    l2cap_manager_psm_instance_t *psm_ptr = task_inst->psm_instances;
    l2cap_manager_l2cap_link_instance_t *l2cap_ptr = NULL;

    while (psm_ptr != NULL)
    {
        l2cap_ptr = psm_ptr->l2cap_instances;
        while (l2cap_ptr != NULL)
        {
            if (l2cap_ptr->sink == sink)
            {
                *psm_inst = psm_ptr;
                *l2cap_inst = l2cap_ptr;
                return TRUE;
            }
            l2cap_ptr = l2cap_ptr->next;
        }
        psm_ptr = psm_ptr->next;
    }
    return FALSE;
}


STATIC_FOR_TARGET l2cap_manager_l2cap_link_instance_t* l2capManager_SearchL2capLinkInstanceByBdAddr(l2cap_manager_psm_instance_t *psm_inst, const tp_bdaddr *tpaddr)
{
    l2cap_manager_l2cap_link_instance_t *l2cap_inst = psm_inst->l2cap_instances;
    bdaddr addr = tpaddr->taddr.addr;

    PanicNull(psm_inst);
    while (l2cap_inst != NULL)
    {
        if (BdaddrIsSame(&l2cap_inst->remote_dev.taddr.addr, &addr))
            return l2cap_inst;
        l2cap_inst = l2cap_inst->next;
    }
    return NULL;
}


STATIC_FOR_TARGET bool l2capManager_CreateL2capLinkInstance(l2cap_manager_psm_instance_t *psm_inst, const tp_bdaddr *tpaddr, l2cap_manager_l2cap_link_instance_t **l2cap_inst)
{
    PanicNull(psm_inst);

    l2cap_manager_l2cap_link_instance_t *ptr = psm_inst->l2cap_instances;
    l2cap_manager_l2cap_link_instance_t *last = NULL;
    bdaddr addr = tpaddr->taddr.addr;

    while (ptr != NULL)
    {
            if (BdaddrIsSame(&ptr->remote_dev.taddr.addr, &addr))
            {
                DEBUG_LOG_DEBUG("L2capManager CreateL2capLinkInstance: ALREADY EXISTS! The link instance for %04X-%02X-%06X (0x%p)",
                                tpaddr->taddr.addr.nap, tpaddr->taddr.addr.uap, tpaddr->taddr.addr.lap, ptr);
                *l2cap_inst = ptr;       /* The link instance for the BD-ADDR already exists! */
                return FALSE;           /* A new instance is *not* created. */
            }
        last = ptr;
        ptr = ptr->next;
    }

    {
        l2cap_manager_l2cap_link_instance_t *new_inst = (l2cap_manager_l2cap_link_instance_t*) PanicUnlessMalloc(sizeof(l2cap_manager_l2cap_link_instance_t));

        new_inst->next                 = NULL;
        new_inst->link_status          = L2CAP_MANAGER_LINK_STATE_NULL;
        new_inst->local_psm            = psm_inst->local_psm;
        new_inst->remote_dev           = *tpaddr;
        new_inst->connection_id        = 0;
        new_inst->identifier           = 0;
        new_inst->mtu_remote           = 0;
        new_inst->flush_timeout_remote = 0;
        new_inst->mode                 = 0;
        new_inst->sink                 = L2CAP_MANAGER_INVALID_SINK;
        new_inst->source               = L2CAP_MANAGER_INVALID_SOURCE;
        new_inst->context              = NULL;

        if (last == NULL)
            psm_inst->l2cap_instances = new_inst;
        else
            last->next = new_inst;
        psm_inst->num_of_links += 1;

        DEBUG_LOG_DEBUG("L2capManager CreateL2capLinkInstance: CREATED: A new link instance: 0x%p", new_inst);
        *l2cap_inst = new_inst;
        return TRUE;    /* A new instance is created. */
    }
}


STATIC_FOR_TARGET bool l2capManager_DeleteL2capLinkInstanceBySink(l2cap_manager_psm_instance_t *psm_inst, const Sink sink)
{
    l2cap_manager_l2cap_link_instance_t *l2cap_inst = psm_inst->l2cap_instances;
    l2cap_manager_l2cap_link_instance_t *prev      = NULL;

    PanicNull(psm_inst);
    while (l2cap_inst != NULL)
    {
        if (l2cap_inst->sink == sink)
        {
            if (prev == NULL)
                psm_inst->l2cap_instances = l2cap_inst->next;
            else
                prev->next = l2cap_inst->next;
            free(l2cap_inst);

            if (0 < psm_inst->num_of_links)
                psm_inst->num_of_links -= 1;
            else
            {
                DEBUG_LOG_ERROR("L2capManager DeleteL2capLinkInstanceBySink: ERROR! 'num_of_links' is already zero!");
                Panic();
            }
            DEBUG_LOG_DEBUG("L2capManager DeleteL2capLinkInstanceBySink: DELETED: A link instance: 0x%p", l2cap_inst);
            return TRUE;
        }
        prev = l2cap_inst;
        l2cap_inst = l2cap_inst->next;
    }
    DEBUG_LOG_WARN("L2capManager DeleteL2capLinkInstanceBySink: WARNING! Cannot find a link instance for the sink: 0x%p", sink);
    return FALSE;
}


/******************************************************************************
 * Functions called by the message handler functions
 ******************************************************************************/
/*! \brief Ask the stack to perform SDP search. */
static void l2capManager_SdpSearchReq(const tp_bdaddr *tpaddr, const bool retry, l2cap_manager_psm_instance_t* psm_inst)
{
    l2cap_manager_sdp_search_pattern_t sdp_search_pattern;
    l2cap_manager_status_t  status;
    l2cap_manager_task_data_t *task_inst = l2capManagerGetTaskData();

    /* Read the SDP search pattern from the client's callback function. */
    status = (psm_inst->functions->get_sdp_search_pattern)(tpaddr, &sdp_search_pattern);
    PanicFalse(status == l2cap_manager_status_success);

    DEBUG_LOG_DEBUG("L2capManager SdpSearchReq: (%04X-%02X-%06X, Retry:%d)",
                    tpaddr->taddr.addr.nap, tpaddr->taddr.addr.uap, tpaddr->taddr.addr.lap, retry);
    DEBUG_LOG_VERBOSE("L2capManager SdpSearchReq: Max retires:    %d", sdp_search_pattern.max_num_of_retries);
    DEBUG_LOG_VERBOSE("L2capManager SdpSearchReq: Max attribs:    %d", sdp_search_pattern.max_attributes);
    DEBUG_LOG_VERBOSE("L2capManager SdpSearchReq: Search pattern: %p", sdp_search_pattern.search_pattern);
    DEBUG_LOG_VERBOSE("L2capManager SdpSearchReq: Search size:    %d", sdp_search_pattern.search_pattern_size);
    DEBUG_LOG_VERBOSE("L2capManager SdpSearchReq: Attrib list:    %p", sdp_search_pattern.attribute_list);
    DEBUG_LOG_VERBOSE("L2capManager SdpSearchReq: Attrib size:    %d", sdp_search_pattern.attribute_list_size);

    psm_inst->sdp_search_max_retries = sdp_search_pattern.max_num_of_retries;
    if (retry == FALSE)
    {
        /* Reset the 'attempts' counter, as this is the first try. */
        psm_inst->sdp_search_attempts = 0;
    }

    /* Perform SDP search */
    psm_inst->state = L2CAP_MANAGER_PSM_STATE_SDP_SEARCH;
    ConnectionSdpServiceSearchAttributeRequest(&task_inst->task, &tpaddr->taddr.addr,
                                                sdp_search_pattern.max_attributes,
                                                sdp_search_pattern.search_pattern_size, sdp_search_pattern.search_pattern,
                                                sdp_search_pattern.attribute_list_size, sdp_search_pattern.attribute_list);
}


/*! \brief Extract the remote PSM value from a service record returned by a SDP service search. */
static bool l2capManager_GetL2capPsm(const uint8 *begin, const uint16 size, uint16 *psm, uint16 id)
{
    ServiceDataType type;
    Region record, protocols, protocol, value;
    record.begin = begin;
    record.end   = begin + size;

    DEBUG_LOG_DEBUG("L2capManager GetL2capPsm: (Attrib:%p, Size:%d, ID:0x%04X)", begin, size, id);
    DEBUG_LOG_VERBOSE("L2capManager GetL2capPsm: %02X %02X %02X %02X  %02X %02X %02X %02X",
                      begin[0], begin[1], begin[2], begin[3], begin[4], begin[5], begin[6], begin[7]);
    DEBUG_LOG_VERBOSE("L2capManager GetL2capPsm: %02X %02X %02X %02X  %02X",
                      begin[8], begin[9], begin[10], begin[11], begin[12]);

    while (ServiceFindAttribute(&record, id, &type, &protocols))
    {
        if (type == sdtSequence)
        {
            while (ServiceGetValue(&protocols, &type, &protocol))
            {
                if (type == sdtSequence
                    && ServiceGetValue(&protocol, &type, &value)
                    && type == sdtUUID
                    && RegionMatchesUUID32(&value, (uint32)UUID16_L2CAP)
                    && ServiceGetValue(&protocol, &type, &value)
                    && type == sdtUnsignedInteger)
                {
                    *psm = (uint16) RegionReadUnsigned(&value);
                    DEBUG_LOG_DEBUG("L2capManager GetL2capPsm: PSM: 0x%04X", *psm);
                    return TRUE;
                }
            }
        }
    }
    return FALSE;
}


/*! \brief Attempt to create an L2CAP connection to the remote device. */
static void l2capManager_ConnectL2cap(const tp_bdaddr *tpaddr, l2cap_manager_psm_instance_t *psm_inst)
{
    l2cap_manager_status_t  status;
    l2cap_manager_l2cap_link_config_t config;
    l2cap_manager_task_data_t *task_inst = l2capManagerGetTaskData();

    /* Read the SDP search pattern from the client's callback function. */
    status = (psm_inst->functions->get_l2cap_link_config)(tpaddr, &config);
    PanicFalse(status == l2cap_manager_status_success);

    DEBUG_LOG_DEBUG("L2capManager ConnectL2cap: (%04X-%02X-%06X)",
                    tpaddr->taddr.addr.nap, tpaddr->taddr.addr.uap, tpaddr->taddr.addr.lap);
    DEBUG_LOG_VERBOSE("L2capManager ConnectL2cap: Local PSM:  0x%04X", psm_inst->local_psm);
    DEBUG_LOG_VERBOSE("L2capManager ConnectL2cap: Remote PSM: 0x%04X", psm_inst->remote_psm);
    DEBUG_LOG_VERBOSE("L2capManager ConnectL2cap: ConfTab Length: %d", config.conftab_length);
    DEBUG_LOG_VERBOSE("L2capManager ConnectL2cap: ConfTab:        %p", config.conftab);

    task_inst->pending_connections++;
    psm_inst->state = L2CAP_MANAGER_PSM_STATE_CONNECTING;

    ConnectionL2capConnectRequest(&task_inst->task,
                                  &tpaddr->taddr.addr,
                                  psm_inst->local_psm, psm_inst->remote_psm,
                                  config.conftab_length,
                                  config.conftab);
}

/*! \brief Convert the disconnect status of the Connection Library to of the L2CAP Manager. */
static l2cap_manager_disconnect_status_t l2capManager_ConvertDisconnectStatus(l2cap_disconnect_status in_status)
{
    switch (in_status)
    {
        case l2cap_disconnect_successful:
            return l2cap_manager_disconnect_successful;
        case l2cap_disconnect_timed_out:
            return l2cap_manager_disconnect_timed_out;
        case l2cap_disconnect_error:
            return l2cap_manager_disconnect_error;
        case l2cap_disconnect_no_connection:
            return l2cap_manager_disconnect_no_connection;
        case l2cap_disconnect_link_loss:
            return l2cap_manager_disconnect_link_loss;
        case l2cap_disconnect_transferred:
            return l2cap_manager_disconnect_transferred;
        default:
            DEBUG_LOG_WARN("L2capManager ConvertDisconnectStatus: WARNING! Disconnect status code 0x%X is not mapped!", in_status);
            return l2cap_manager_disconnect_unknown_reason;
    }
}


/*! \brief Clean up after disconnection of an L2CAP link. */
static bool l2capManager_CleanUpByDisconnection(l2cap_manager_psm_instance_t *psm_inst, const Sink sink)
{
    if (l2capManager_DeleteL2capLinkInstanceBySink(psm_inst, sink))
    {
        DEBUG_LOG_DEBUG("L2capManager CleanUpByDisconnection: Deleted the link instance for (sink: 0x%p)", sink);
        return TRUE;
    }
    else
    {
        DEBUG_LOG_ERROR("L2capManager CleanUpByDisconnection: ERROR! Failed to delete the link instance for (sink: 0x%p)", sink);
        Panic();
    }
    return FALSE;
}


/******************************************************************************
 * The message handler functions
 ******************************************************************************/
static void l2capManager_HandleL2capRegisterCfm(const CL_L2CAP_REGISTER_CFM_T *cfm)
{
    DEBUG_LOG_DEBUG("L2capManager HandleL2capRegisterCfm: (PSM:0x%04X, Status:%d)", cfm->psm, cfm->status);

    if (cfm->status == success)
    {
        l2cap_manager_sdp_record_t  sdp_record = { 0 };
        l2cap_manager_task_data_t *task_inst = l2capManagerGetTaskData();
        l2cap_manager_psm_instance_t *psm_inst = l2capManager_SearchPsmInstanceByState(L2CAP_MANAGER_PSM_STATE_PSM_REGISTRATION);
        PanicNull(psm_inst);

        /* Keep a copy of the registered L2CAP PSM. */
        psm_inst->local_psm = cfm->psm;

        /* Get the SDP service record through the client's callback function. */
        if (psm_inst->functions->get_sdp_record)
            (psm_inst->functions->get_sdp_record)(cfm->psm, &sdp_record);
        else
        {
            DEBUG_LOG_ERROR("L2capManager HandleL2capRegisterCfm: ERRRO! 'get_sdp_record' callback handler is NULL!");
            Panic();
        }
        DEBUG_LOG_VERBOSE("L2capManager HandleL2capRegisterCfm: SDP(Record:%p, Size:%d, OffsetToPsm:%d)",
                          sdp_record.service_record, sdp_record.service_record_size, sdp_record.offset_to_psm);

        if (sdp_record.service_record != NULL && sdp_record.service_record_size != 0)
        {
            /* Copy and update SDP record */
            uint8 *record = PanicUnlessMalloc(sdp_record.service_record_size);

            psm_inst->sdp_record = record;
            memcpy(record, sdp_record.service_record, sdp_record.service_record_size);

            /* Write L2CAP PSM into service record */
            record[sdp_record.offset_to_psm + 0] = (cfm->psm >> 8) & 0xFF;
            record[sdp_record.offset_to_psm + 1] = (cfm->psm) & 0xFF;

            psm_inst->state = L2CAP_MANAGER_PSM_STATE_SDP_REGISTRATION;
            /* Register service record */
            ConnectionRegisterServiceRecord(&task_inst->task, sdp_record.service_record_size, record);

            /* Do not 'free(record)' at this point.
             * Otherwise, the connection library will 'Panic()'.
             * 'psm_inst->sdp_record' must be freed later. */
        }
        else
        {
            DEBUG_LOG_WARN("L2capManager HandleL2capRegisterCfm: Valid SDP record is not supplied (Record:%p, Size:%d)", sdp_record.service_record, sdp_record.service_record_size);
        }
    }
    else
    {
        DEBUG_LOG_ERROR("L2capManager HandleL2capRegisterCfm: ERROR! Failed to register a PSM! (Status:%d)", cfm->status);
        Panic();
        return;
    }
}


static void l2capManager_HandleSdpRegisterCfm(const CL_SDP_REGISTER_CFM_T *cfm)
{
    DEBUG_LOG_DEBUG("L2capManager HandleSdpRegisterCfm: (Status:%d, Handle:0x%08X)", cfm->status, cfm->service_handle);

    if (cfm->status == sds_status_success)
    {
        l2cap_manager_psm_instance_t *psm_inst = l2capManager_SearchPsmInstanceByState(L2CAP_MANAGER_PSM_STATE_SDP_REGISTRATION);
        PanicNull(psm_inst);

        DEBUG_LOG_DEBUG("L2capManager HandleSdpRegisterCfm: SDP record registered (Handle:0x%08X)", cfm->service_handle);

        /* Save the SDP service record handle assigned by the stack. */
        psm_inst->service_handle = cfm->service_handle;
        psm_inst->state = L2CAP_MANAGER_PSM_STATE_READY;

        /* Notify the client that the PSM has been registered. */
        if (psm_inst->functions->registered_ind)
            (psm_inst->functions->registered_ind)(l2cap_manager_status_success);
    }
    else if (cfm->status == sds_status_pending)
        DEBUG_LOG_WARN("L2capManager HandleSdpRegisterCfm: Pending the SDP record registration (Status:%d)", cfm->status);
    else
    {
        DEBUG_LOG_ERROR("L2capManager HandleSdpRegisterCfm: ERROR! Failed to register an SDP record! (Status:%d)", cfm->status);
        Panic();
    }
}


static void l2capManager_NotifyFailedSdpSearch(l2cap_manager_psm_instance_t *psm_inst, l2cap_manager_l2cap_link_instance_t *l2cap_inst, const bdaddr *bd_addr)
{
    tp_bdaddr tpaddr;

    DEBUG_LOG_WARN("L2capManager NotifyFailedSdpSearch:");
    PanicNull(psm_inst);
    L2CAP_MANAGER_SET_TYPED_BDADDR(tpaddr, TRANSPORT_BREDR_ACL, TYPED_BDADDR_PUBLIC, *bd_addr);

    /* Notify the client that SDP Search is failed. */
    l2cap_manager_connect_cfm_t cfm_to_client =
    {
        .status               = l2cap_manager_connect_status_failed_sdp_search,
        .local_psm            = psm_inst->local_psm,
        .remote_psm           = L2CAP_MANAGER_PSM_INVALID,
        .tpaddr               = tpaddr,
        .sink                 = 0,
        .connection_id        = 0,
        .mtu_remote           = 0,
        .flush_timeout_remote = 0,
        .mode                 = 0,
        .qos_remote           = { 0 }
    };

    if (psm_inst->functions->handle_connect_cfm)
        (psm_inst->functions->handle_connect_cfm)(&cfm_to_client, l2cap_inst->context);
}


static void l2capManager_HandleSdpServiceSearchAttributeCfm(CL_SDP_SERVICE_SEARCH_ATTRIBUTE_CFM_T *cfm)
{
    tp_bdaddr tpaddr;
    l2cap_manager_l2cap_link_instance_t *l2cap_inst = NULL;
    l2cap_manager_psm_instance_t *psm_inst = l2capManager_SearchPsmInstanceByState(L2CAP_MANAGER_PSM_STATE_SDP_SEARCH);
    PanicNull(psm_inst);
    bool report_sdp_search_failure = TRUE;

    DEBUG_LOG_DEBUG("L2capManager HandleSdpServiceSearchAttributeCfm: (Status:0x%X, ErrCode:0x%X)", cfm->status, cfm->error_code);

    if (cfm->status == sds_status_success)
    {
        bool result = FALSE;

        DEBUG_LOG_VERBOSE("L2capManager HandleSdpServiceSearchAttributeCfm: - more_to_come: %d", cfm->more_to_come);
        DEBUG_LOG_VERBOSE("L2capManager HandleSdpServiceSearchAttributeCfm: - Remote addr:  %04X-%02X-%06X",
                          cfm->bd_addr.nap, cfm->bd_addr.uap, cfm->bd_addr.lap);
        DEBUG_LOG_VERBOSE("L2capManager HandleSdpServiceSearchAttributeCfm: - size_attributes: %d", cfm->size_attributes);
        DEBUG_LOG_VERBOSE("L2capManager HandleSdpServiceSearchAttributeCfm: - attributes[0]: 0x%02X", cfm->attributes[0]);

        /* Read the remote device's PSM from the SDP attributes. */
        result = l2capManager_GetL2capPsm(cfm->attributes, cfm->size_attributes, &psm_inst->remote_psm, saProtocolDescriptorList);
        if (result)
        {
            DEBUG_LOG_DEBUG("L2capManager HandleSdpServiceSearchAttributeCfm: OK! (size_attributes:%d)", cfm->size_attributes);

            L2CAP_MANAGER_SET_TYPED_BDADDR(tpaddr, TRANSPORT_BREDR_ACL, TYPED_BDADDR_PUBLIC, cfm->bd_addr);
            l2cap_inst = l2capManager_SearchL2capLinkInstanceByBdAddr(psm_inst, &tpaddr);
            PanicNull(l2cap_inst);

            l2cap_inst->link_status = L2CAP_MANAGER_LINK_STATE_LOCAL_INITIATED_CONNECTING;
            l2capManager_ConnectL2cap(&tpaddr, psm_inst);
            report_sdp_search_failure = FALSE;
        }
        else
            DEBUG_LOG_WARN("L2capManager HandleSdpServiceSearchAttributeCfm: WARNING! No PSM found in the remote device's SDP record!");
    }
    else if (cfm->status == sdp_no_response_data)
        DEBUG_LOG_WARN("L2capManager HandleSdpServiceSearchAttributeCfm: WARNING! SDP, No response data!");
    else
    {
        /* A SDP search attempt has failed. Let's retry! */
        psm_inst->sdp_search_attempts += 1;
        if (psm_inst->sdp_search_attempts <= psm_inst->sdp_search_max_retries)
        {
            DEBUG_LOG_DEBUG("L2capManager HandleSdpServiceSearchAttributeCfm: SDP search retry attempt: %d", psm_inst->sdp_search_attempts);

            L2CAP_MANAGER_SET_TYPED_BDADDR(tpaddr, TRANSPORT_BREDR_ACL, TYPED_BDADDR_PUBLIC, cfm->bd_addr);
            l2capManager_SdpSearchReq(&tpaddr, TRUE, psm_inst);     /* Retry the SDP search. */
            report_sdp_search_failure = FALSE;                      /* Not yet. */
        }
        else
            DEBUG_LOG_WARN("L2capManager HandleSdpServiceSearchAttributeCfm: WARNING! All the SDP search attempts failed: %d", psm_inst->sdp_search_attempts);
    }

    if (report_sdp_search_failure)
    {
        /* Let the client know that SDP search atttempt(s) have failed. */
        DEBUG_LOG_DEBUG("L2capManager HandleSdpServiceSearchAttributeCfm: Status code:         0x%X", cfm->status);
        DEBUG_LOG_DEBUG("L2capManager HandleSdpServiceSearchAttributeCfm: Error code:          0x%X", cfm->error_code);
        DEBUG_LOG_DEBUG("L2capManager HandleSdpServiceSearchAttributeCfm: Remote addr:         %04X-%02X-%06X",
                        cfm->bd_addr.nap, cfm->bd_addr.uap, cfm->bd_addr.lap);
        DEBUG_LOG_VERBOSE("L2capManager HandleSdpServiceSearchAttributeCfm: Attribute list size: 0x%04X", cfm->size_attributes);
        DEBUG_LOG_VERBOSE("L2capManager HandleSdpServiceSearchAttributeCfm: More to come:        %d", cfm->more_to_come);

        L2CAP_MANAGER_SET_TYPED_BDADDR(tpaddr, TRANSPORT_BREDR_ACL, TYPED_BDADDR_PUBLIC, cfm->bd_addr);
        l2cap_inst = l2capManager_SearchL2capLinkInstanceByBdAddr(psm_inst, &tpaddr);
        PanicNull(l2cap_inst);
        l2cap_inst->link_status = L2CAP_MANAGER_LINK_STATE_DISCONNECTED;

        l2capManager_NotifyFailedSdpSearch(psm_inst, l2cap_inst, &cfm->bd_addr);
        psm_inst->state = L2CAP_MANAGER_PSM_STATE_READY;
    }
}


static void l2capManager_HandleConnectInd(CL_L2CAP_CONNECT_IND_T *ind)
{
    l2cap_manager_connect_ind_t ind_from_remote;
    l2cap_manager_connect_rsp_t rsp_by_client = { 0 };
    l2cap_manager_l2cap_link_instance_t* l2cap_inst = NULL;
    l2cap_manager_task_data_t *task_inst = l2capManagerGetTaskData();
    l2cap_manager_psm_instance_t *psm_inst = l2capManager_SearchPsmInstanceByLocalPsm(ind->psm);
    PanicNull(psm_inst);

    DEBUG_LOG_DEBUG("L2capManager HandleConnectInd");
    DEBUG_LOG_DEBUG("L2capManager HandleConnectInd: - Remote addr: %04X-%02X-%06X",
                    ind->bd_addr.nap, ind->bd_addr.uap, ind->bd_addr.lap);
    DEBUG_LOG_DEBUG("L2capManager HandleConnectInd: - Local PSM:      0x%04X", ind->psm);
    DEBUG_LOG_DEBUG("L2capManager HandleConnectInd: - Identifier:     0x%02X", ind->identifier);
    DEBUG_LOG_DEBUG("L2capManager HandleConnectInd: - Connection ID:  0x%04X", ind->connection_id);

    L2CAP_MANAGER_SET_TYPED_BDADDR(ind_from_remote.tpaddr, TRANSPORT_BREDR_ACL, TYPED_BDADDR_PUBLIC, ind->bd_addr);
    ind_from_remote.local_psm         = ind->psm;
    ind_from_remote.remote_psm        = 0;                      /* Not known yet. */
    ind_from_remote.identifier        = ind->identifier;
    ind_from_remote.connection_id     = ind->connection_id;

    l2capManager_CreateL2capLinkInstance(psm_inst, &ind_from_remote.tpaddr, &l2cap_inst);
    if (l2cap_inst == NULL)
    {
        DEBUG_LOG_ERROR("L2capManager HandleConnectInd: ERROR! Failed to allocate a link instance!");
        Panic();
    }

    L2CAP_MANAGER_SET_TYPED_BDADDR(l2cap_inst->remote_dev, TRANSPORT_BREDR_ACL, TYPED_BDADDR_PUBLIC, ind->bd_addr);
    l2cap_inst->local_psm     = ind->psm;
    l2cap_inst->identifier    = ind->identifier;
    l2cap_inst->connection_id = ind->connection_id;
    l2cap_inst->link_status   = L2CAP_MANAGER_LINK_STATE_CONNECTING_BY_REMOTE;

    {
        /* Notify the client that a remote device attempts to connect this device.
         * The client's response is set to 'rsp_by_client'. */
        void *context = NULL;
    
        if (psm_inst->functions->respond_connect_ind)
            (psm_inst->functions->respond_connect_ind)(&ind_from_remote, &rsp_by_client, &context);
        /* Note that this 'context' pointer is used by the client.
         * We just save this for the client's use later. */
        l2cap_inst->context = context;
    }

    task_inst->pending_connections++;
    psm_inst->state = L2CAP_MANAGER_PSM_STATE_CONNECTING;

    /* Send a response accepting or rejcting the connection. */
    ConnectionL2capConnectResponse(&task_inst->task,                /* The client task. */
                                   rsp_by_client.response,          /* Accept/reject the connection. */
                                   ind->psm,                        /* The local PSM. */
                                   ind->connection_id,              /* The L2CAP connection ID.*/
                                   ind->identifier,                 /* The L2CAP signal identifier. */
                                   rsp_by_client.conftab_length,    /* The length of the configuration table. */
                                   rsp_by_client.conftab);          /* The configuration table. */
}


static void l2capManager_HandleConnectCfm(CL_L2CAP_CONNECT_CFM_T *cfm)
{
    l2cap_manager_connect_cfm_t cfm_to_client;
    l2cap_manager_l2cap_link_instance_t *l2cap_inst = NULL;
    l2cap_manager_task_data_t *task_inst = l2capManagerGetTaskData();
    l2cap_manager_psm_instance_t *psm_inst = l2capManager_SearchPsmInstanceByLocalPsm(cfm->psm_local);
    PanicNull(psm_inst);

    DEBUG_LOG_DEBUG("L2capManager HandleConnectCfm: (Status:%d)", cfm->status);

    if (cfm->status == l2cap_connect_pending)
    {
        DEBUG_LOG_DEBUG("L2capManager HandleConnectCfm: Connection pending. Wait for another message...");
        return;
    }

    /* The pending counter must be more than zero. Otherwise, something goes wrong. */
    PanicFalse(0 < task_inst->pending_connections);
    task_inst->pending_connections--;

    L2CAP_MANAGER_SET_TYPED_BDADDR(cfm_to_client.tpaddr, TRANSPORT_BREDR_ACL, TYPED_BDADDR_PUBLIC, cfm->addr);
    l2cap_inst = l2capManager_SearchL2capLinkInstanceByBdAddr(psm_inst, &cfm_to_client.tpaddr);
    PanicNull(l2cap_inst);

    if (cfm->status == l2cap_connect_success)
    {
        Source source = StreamSourceFromSink(cfm->sink);

        DEBUG_LOG_DEBUG("L2capManager HandleConnectCfm: Connected:");

        DEBUG_LOG_VERBOSE("L2capManager HandleConnectCfm: Local PSM:            0x%04X", cfm->psm_local);
        DEBUG_LOG_VERBOSE("L2capManager HandleConnectCfm: sink:                 0x%04X", cfm->sink);
        DEBUG_LOG_VERBOSE("L2capManager HandleConnectCfm: Connection ID:        0x%04X", cfm->connection_id);
        DEBUG_LOG_VERBOSE("L2capManager HandleConnectCfm: Remote addr:          %04X-%02X-%06X",
                          cfm->addr.nap, cfm->addr.uap, cfm->addr.lap);
        DEBUG_LOG_VERBOSE("L2capManager HandleConnectCfm: Remote MTU:           0x%04X", cfm->mtu_remote);
        DEBUG_LOG_VERBOSE("L2capManager HandleConnectCfm: Remote Flush Timeout: 0x%04X", cfm->flush_timeout_remote);
        DEBUG_LOG_VERBOSE("L2capManager HandleConnectCfm: Flow Mode:            0x%04X", cfm->mode);

        /* Notify the client that the connection has been established. */
        cfm_to_client.status               = l2cap_manager_connect_status_success;
        cfm_to_client.local_psm            = cfm->psm_local;
        cfm_to_client.remote_psm           = 0;
        cfm_to_client.sink                 = cfm->sink;
        cfm_to_client.connection_id        = cfm->connection_id;
        cfm_to_client.mtu_remote           = cfm->mtu_remote;
        cfm_to_client.flush_timeout_remote = cfm->flush_timeout_remote;
        cfm_to_client.mode                 = cfm->mode;
    
        /* QoS parameters (Left: L2CAP Manager struct, Right: Connection Lib struct) */
        cfm_to_client.qos_remote.service_type = cfm->qos_remote.service_type;
        cfm_to_client.qos_remote.token_rate   = cfm->qos_remote.token_rate;
        cfm_to_client.qos_remote.token_bucket = cfm->qos_remote.token_bucket;
        cfm_to_client.qos_remote.peak_bw      = cfm->qos_remote.peak_bw;
        cfm_to_client.qos_remote.latency      = cfm->qos_remote.latency;
        cfm_to_client.qos_remote.delay_var    = cfm->qos_remote.delay_var;

        /* Set the link parameters to the link instance. */
        l2cap_inst->local_psm               = cfm->psm_local;
        l2cap_inst->connection_id           = cfm->connection_id;
        l2cap_inst->mtu_remote              = cfm->mtu_remote;
        l2cap_inst->flush_timeout_remote    = cfm->flush_timeout_remote;

        l2cap_inst->qos_remote.service_type = cfm->qos_remote.service_type;
        l2cap_inst->qos_remote.token_rate   = cfm->qos_remote.token_rate;
        l2cap_inst->qos_remote.token_bucket = cfm->qos_remote.token_bucket;
        l2cap_inst->qos_remote.peak_bw      = cfm->qos_remote.peak_bw;
        l2cap_inst->qos_remote.latency      = cfm->qos_remote.latency;
        l2cap_inst->qos_remote.delay_var    = cfm->qos_remote.delay_var;

        l2cap_inst->mode                    = cfm->mode;
        l2cap_inst->sink                    = cfm->sink;
        l2cap_inst->source                  = source;
    
       /* Set the handover policy */
        PanicFalse(SourceConfigure(source, STREAM_SOURCE_HANDOVER_POLICY, SOURCE_HANDOVER_ALLOW_WITHOUT_DATA));

        l2cap_inst->link_status = L2CAP_MANAGER_LINK_STATE_CONNECTED;
        psm_inst->state = L2CAP_MANAGER_PSM_STATE_CONNECTED;
    }
    else
    {
        HYDRA_LOG_STRING(msg_failed, "Failed!");
        HYDRA_LOG_STRING(msg_r_reject, "Remote rejected!");
        HYDRA_LOG_STRING(msg_c_reject, "Config rejected!");
        HYDRA_LOG_STRING(msg_something_else, "Something else.");
        DEBUG_LOG_WARN("L2capManager HandleConnectCfm: WARNING! Failed to connect. (Status:%d = %s)", cfm->status,
                        ((cfm->status == l2cap_connect_failed) ? msg_failed :
                            ((cfm->status == l2cap_connect_failed_remote_reject) ? msg_r_reject :
                                (cfm->status == l2cap_connect_failed_config_rejected) ? msg_c_reject : msg_something_else
                            )
                        )
                      );

        /* Delete the L2CAP instance as the connection establishment attempt is failed. */
        l2capManager_DeleteL2capLinkInstanceBySink(psm_inst, L2CAP_MANAGER_INVALID_SINK);
        psm_inst->state = L2CAP_MANAGER_PSM_STATE_READY;
    }

    if (psm_inst->functions->handle_connect_cfm)
        (psm_inst->functions->handle_connect_cfm)(&cfm_to_client, l2cap_inst->context);
}


static void l2capManager_HandleDisconnectInd(CL_L2CAP_DISCONNECT_IND_T *ind)
{
    l2cap_manager_disconnect_ind_t ind_to_client;
    l2cap_manager_psm_instance_t *psm_inst = NULL;
    l2cap_manager_l2cap_link_instance_t *l2cap_inst = NULL;

    if (ind->status == l2cap_disconnect_successful)
        DEBUG_LOG_DEBUG("L2capManager HandleDisconnectInd");
    else if (ind->status == l2cap_disconnect_link_loss)
        DEBUG_LOG_DEBUG("L2capManager HandleDisconnectInd: Link loss");
    else
        DEBUG_LOG_WARN("L2capManager HandleDisconnectInd: WARNING! The status is other than successful: %d", ind->status);

    if (FALSE == l2capManager_GetPsmAndL2capInstanceBySink(ind->sink, &psm_inst, &l2cap_inst))
    {
        DEBUG_LOG_ERROR("L2capManager HandleDisconnectInd: ERROR! Failed to find the PSM/L2CAP instances for the sink (0x%p)", ind->sink);
        Panic();
    }

    ind_to_client.identifier = ind->identifier;
    ind_to_client.status     = l2capManager_ConvertDisconnectStatus(ind->status);
    ind_to_client.sink       = ind->sink;
    DEBUG_LOG_VERBOSE("L2capManager HandleDisconnectInd: (Identifier:0x%02X, Sink:0x%04X)", ind->identifier, ind->sink);

    if (psm_inst->functions->respond_disconnect_ind)
        (psm_inst->functions->respond_disconnect_ind)(&ind_to_client, l2cap_inst->context);

    ConnectionL2capDisconnectResponse(ind->identifier, ind->sink);

    l2capManager_CleanUpByDisconnection(psm_inst, ind->sink);
    psm_inst->state = L2CAP_MANAGER_PSM_STATE_READY;
}


static void l2capManager_HandleDisconnectCfm(CL_L2CAP_DISCONNECT_CFM_T *cfm)
{
    l2cap_manager_disconnect_cfm_t cfm_to_client;
    l2cap_manager_psm_instance_t *psm_inst = NULL;
    l2cap_manager_l2cap_link_instance_t *l2cap_inst = NULL;

    if (cfm->status == l2cap_disconnect_successful)
        DEBUG_LOG_DEBUG("L2capManager HandleDisconnectCfm: Success");
    else if (cfm->status == l2cap_disconnect_timed_out)
        DEBUG_LOG_DEBUG("L2capManager HandleDisconnectCfm: Timed out (No response from the peer).");    /* No response for 30 seconds results in this! */
    else
        DEBUG_LOG_WARN("L2capManager HandleDisconnectCfm: WARNING! The status is other than successful: %d", cfm->status);

    if (FALSE == l2capManager_GetPsmAndL2capInstanceBySink(cfm->sink, &psm_inst, &l2cap_inst))
    {
        DEBUG_LOG_ERROR("L2capManager HandleDisconnectCfm: ERROR! Failed to find the PSM/L2CAP instances for the sink (0x%p)", cfm->sink);
        Panic();
    }

    cfm_to_client.status = l2capManager_ConvertDisconnectStatus(cfm->status);
    cfm_to_client.sink   = cfm->sink;
    DEBUG_LOG_VERBOSE("L2capManager HandleDisconnectCfm: (Sink:0x%04X)", cfm->sink);

    if (psm_inst->functions->handle_disconnect_cfm)
        (psm_inst->functions->handle_disconnect_cfm)(&cfm_to_client, l2cap_inst->context);

    l2capManager_CleanUpByDisconnection(psm_inst, cfm->sink);
    psm_inst->state = L2CAP_MANAGER_PSM_STATE_READY;
}


static void l2capManager_HandleMessageMoreData(const MessageMoreData *msg_more_data)
{
    l2cap_manager_psm_instance_t *psm_inst = NULL;
    l2cap_manager_l2cap_link_instance_t *l2cap_inst = NULL;
    Source source = msg_more_data->source;
    Sink sink = StreamSinkFromSource(source);

    if (FALSE == l2capManager_GetPsmAndL2capInstanceBySink(sink, &psm_inst, &l2cap_inst))
    {
        DEBUG_LOG_ERROR("L2capManager HandleMessageMoreData: ERROR! Failed to find the PSM/L2CAP instances for the source (0x%p)", msg_more_data->source);
        Panic();
    }

    if (psm_inst->functions->process_more_data)
    {
        l2cap_manager_message_more_data_t more_data;
        PanicFalse(source == l2cap_inst->source);

        more_data.connection_id = l2cap_inst->connection_id;
        more_data.source        = source;
        (psm_inst->functions->process_more_data)(&more_data, l2cap_inst->context);
    }
}


static void l2capManager_HandleMessageMoreSpace(const MessageMoreSpace *msg_more_space)
{
    l2cap_manager_psm_instance_t *psm_inst = NULL;
    l2cap_manager_l2cap_link_instance_t *l2cap_inst = NULL;

    if (FALSE == l2capManager_GetPsmAndL2capInstanceBySink(msg_more_space->sink, &psm_inst, &l2cap_inst))
    {
        DEBUG_LOG_ERROR("L2capManager HandleMessageMoreSpace: ERROR! Failed to find the PSM/L2CAP instances for the sink (0x%p)", msg_more_space->sink);
        Panic();
    }

    if (psm_inst->functions->process_more_space)
    {
        l2cap_manager_message_more_space_t more_space;

        more_space.connection_id = l2cap_inst->connection_id;
        more_space.sink          = msg_more_space->sink;
        (psm_inst->functions->process_more_space)(&more_space, l2cap_inst->context);
    }
}


/******************************************************************************
 * The main message handler of the L2CAP manager
 ******************************************************************************/
/*! \brief L2CAP Manager task message handler.

    \note The connection library dependent function.
 */
static void l2capManager_HandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch (id)
    {
        /* Connection library messages */
        case MESSAGE_MORE_DATA:
            DEBUG_LOG_DEBUG("L2capManager HandleMessage: MESSAGE_MORE_DATA");
            l2capManager_HandleMessageMoreData((const MessageMoreData*) message);
            break;

        case MESSAGE_MORE_SPACE:
            DEBUG_LOG_DEBUG("L2capManager HandleMessage: MESSAGE_MORE_SPACE");
            l2capManager_HandleMessageMoreSpace((const MessageMoreSpace*) message);
            break;

        case CL_L2CAP_CONNECT_IND:
            DEBUG_LOG_DEBUG("L2capManager HandleMessage: CL_L2CAP_CONNECT_IND");
            l2capManager_HandleConnectInd((CL_L2CAP_CONNECT_IND_T*) message);
            break;

        case CL_L2CAP_CONNECT_CFM:
            DEBUG_LOG_DEBUG("L2capManager HandleMessage: CL_L2CAP_CONNECT_CFM");
            l2capManager_HandleConnectCfm((CL_L2CAP_CONNECT_CFM_T*) message);
            break;

        case CL_L2CAP_DISCONNECT_IND:
            DEBUG_LOG_DEBUG("L2capManager HandleMessage: CL_L2CAP_DISCONNECT_IND");
            l2capManager_HandleDisconnectInd((CL_L2CAP_DISCONNECT_IND_T*) message);
            break;

        case CL_L2CAP_DISCONNECT_CFM:
            DEBUG_LOG_DEBUG("L2capManager HandleMessage: CL_L2CAP_DISCONNECT_CFM");
            l2capManager_HandleDisconnectCfm((CL_L2CAP_DISCONNECT_CFM_T*) message);
            break;


        case CL_SDP_REGISTER_CFM:
            l2capManager_HandleSdpRegisterCfm((CL_SDP_REGISTER_CFM_T*) message);
            break;

        case CL_SDP_SERVICE_SEARCH_ATTRIBUTE_CFM:
            l2capManager_HandleSdpServiceSearchAttributeCfm((CL_SDP_SERVICE_SEARCH_ATTRIBUTE_CFM_T*) message);
            return;


        case CL_L2CAP_REGISTER_CFM:
            l2capManager_HandleL2capRegisterCfm((CL_L2CAP_REGISTER_CFM_T*) message);
            break;

        default:
            DEBUG_LOG_WARN("L2capManager HandleMessage: Unhandled message: 0x%04X", id);
            break;
    }
}


/******************************************************************************
 * PUBLIC API
 ******************************************************************************/
bool L2capManager_Init(Task init_task)
{
    UNUSED(init_task);

    DEBUG_LOG_DEBUG("L2capManager Init");
    l2cap_manager_task_data_t *task_inst = l2capManagerGetTaskData();
    memset(task_inst, 0, sizeof(*task_inst));

    task_inst->task.handler = l2capManager_HandleMessage;

    /* Initialise the linked-list of the PSM instances. */
    task_inst->pending_connections  = 0;
    task_inst->num_of_psm_instances = 0;
    task_inst->psm_instances        = NULL;

    return TRUE;
}


l2cap_manager_status_t L2capManager_Register(uint16 psm, const l2cap_manager_functions_t* functions, l2cap_manager_instance_id *instance_id)
{
    l2cap_manager_psm_instance_t *psm_inst;
    l2cap_manager_task_data_t *task_inst = l2capManagerGetTaskData();

    DEBUG_LOG_DEBUG("L2capManager Register");
    /* Both 'process_more_data' and 'process_more_space' handlers must be set.
     * Or, both 'process_more_data' and 'process_more_space' handlers must be NULL. */
    if (functions->process_more_data)
    {
        if (functions->process_more_space == NULL)
        {
            DEBUG_LOG_ERROR("L2capManager Register: ERROR! 'process_more_space' handler must be set if 'process_more_data' handler is set.");
            Panic();
        }
    }
    else
    {
        if (functions->process_more_space)
        {
            DEBUG_LOG_ERROR("L2capManager Register: ERROR! 'process_more_data' handler must be set if 'process_more_space' handler is set.");
            Panic();
        }
    }

    *instance_id = L2CAP_MANAGER_PSM_INSTANCE_ID_INVALID;
    psm_inst = l2capManager_CreatePsmInstance();
    if (psm_inst == NULL)
        return l2cap_manager_failed_to_allocate_an_instance;

    DEBUG_LOG_VERBOSE("L2capManager Register: (psm_inst:%p)", psm_inst);

    /* Initialise the new PSM instance. */
    psm_inst->state               = L2CAP_MANAGER_PSM_STATE_PSM_REGISTRATION;
    psm_inst->local_psm           = psm;
    psm_inst->remote_psm          = L2CAP_MANAGER_PSM_INVALID;
    psm_inst->sdp_search_attempts = 0;
    psm_inst->num_of_links        = 0;
    psm_inst->l2cap_instances     = NULL;
    psm_inst->functions           = functions;
    *instance_id = psm_inst->instance_id;

    DEBUG_LOG_VERBOSE("L2capManager Register: functions->get_sdp_record:           %p", functions->get_sdp_record);
    DEBUG_LOG_VERBOSE("L2capManager Register: psm_inst->functions:                 %p", psm_inst->functions);
    DEBUG_LOG_VERBOSE("L2capManager Register: psm_inst->functions->get_sdp_record: %p", psm_inst->functions->get_sdp_record);

    /* Register a Protocol/Service Multiplexor (PSM) that will be used for this
     * client. The remote device can use the same or a different PSM at its end. */
    ConnectionL2capRegisterRequest(&task_inst->task, psm, 0);

    return l2cap_manager_status_success;
}


l2cap_manager_status_t L2capManager_Connect(const tp_bdaddr *tpaddr, l2cap_manager_instance_id instance_id, void *context)
{
    l2cap_manager_psm_instance_t *psm_inst = NULL;
    l2cap_manager_l2cap_link_instance_t* l2cap_inst = NULL;

    DEBUG_LOG_DEBUG("L2capManager Connect");

    /* Find the PSM instance from the linked-list. */
    psm_inst = l2capManager_SearchPsmInstance(instance_id);
    PanicNull(psm_inst);    /* NB: The PSM must be registered prior to calling this function. */

    /* Create a new L2CAP link instance. */
    l2capManager_CreateL2capLinkInstance(psm_inst, tpaddr, &l2cap_inst);
    if (l2cap_inst == NULL)
        return l2cap_manager_failed_to_allocate_an_instance;

    /* Note that this 'context' pointer is used by the client.
        * We just save this for the client's use later. */
    l2cap_inst->context = context;

    /* Check if the remote PSM is known or not. */
    if (psm_inst->remote_psm == L2CAP_MANAGER_PSM_INVALID)
    {
        /* Request SDP search, as we need to get the remote PSM by SDP search. */
        l2cap_inst->link_status = L2CAP_MANAGER_LINK_STATE_LOCAL_INITIATED_SDP_SEARCH;
        l2capManager_SdpSearchReq(tpaddr, FALSE, psm_inst);      /* First attempt of the SDP search. */
    }
    else
    {
        /* The remote PSM is already known.
         * Initiate an L2CAP connection request. */
        l2cap_inst->link_status = L2CAP_MANAGER_LINK_STATE_LOCAL_INITIATED_CONNECTING;
        l2capManager_ConnectL2cap(tpaddr, psm_inst);
    }

    return l2cap_manager_status_success;
}


l2cap_manager_status_t L2capManager_Disconnect(Sink sink, l2cap_manager_instance_id instance_id)
{
    l2cap_manager_psm_instance_t *psm_inst = NULL;
    l2cap_manager_l2cap_link_instance_t *l2cap_inst = NULL;
    l2cap_manager_task_data_t *task_inst = l2capManagerGetTaskData();

    DEBUG_LOG_DEBUG("L2capManager Disconnect");
    PanicNull(sink);

    /* Make sure that the PSM instance with the 'instance_id' exists. */
    psm_inst = l2capManager_SearchPsmInstance(instance_id);
    PanicNull(psm_inst);    /* NB: The PSM must be registered prior to calling this function. */

    /* Find the PSM instance & the L2CAP instance that uses the 'sink'. */
    if (l2capManager_GetPsmAndL2capInstanceBySink(sink, &psm_inst, &l2cap_inst))
        l2cap_inst->link_status = L2CAP_MANAGER_LINK_STATE_DISCONNECTING;
    else
    {
        DEBUG_LOG_ERROR("L2capManager Disconnect: ERROR! Failed to find the PSM/L2CAP instances for the sink (0x%p)", sink);
        Panic();
    }

    /* Tell the connection library to disconnect the link. */
    ConnectionL2capDisconnectRequest(&task_inst->task, sink);

    return l2cap_manager_status_success;
}

#endif /* INCLUDE_L2CAP_MANAGER */

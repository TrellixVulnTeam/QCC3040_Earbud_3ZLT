/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\defgroup   handover_profile Handover Profile
\ingroup    profiles
\brief      Implementation of the protocol between earbuds during handover.
*/
#ifdef INCLUDE_MIRRORING

#include "handover_profile_private.h"

static void handoverProtocol_HandleVersionInd(const uint8 *data);
static handover_profile_status_t handoverProtocol_HandleStartReq(const uint8 *data);
static const uint8* handoverProtocol_WaitForMessage(Source src, handover_protocol_opcode_t opcode, uint32 timeout, uint16 *size);
static bool handoverProtocol_HandleMarshalData(Source source, const uint8 *src_addr, uint16 src_size);
static void handoverProtocol_HandleCancelInd(void);
static uint8* handoverProtocol_SinkClaimAndMap(Sink sink, uint16 size, uint16 min_slack);
static handover_profile_status_t handoverProtocol_SendMsg(handover_protocol_opcode_t opcode, void *src_addr, uint16 size);

handover_profile_status_t handoverProtocol_SendVersionInd(void)
{
    handover_profile_task_data_t *ho_inst = Handover_GetTaskData();
    HANDOVER_PROTOCOL_VERSION_IND_T ind = {0};

    ind.appsp0_version = VmGetFwVersion(FIRMWARE_ID);
    ind.appsp1_version = VmGetFwVersion(APPLICATION_ID);
    ind.btss_rom_version = ho_inst->btss_rom_version;
    ind.btss_patch_version = ho_inst->btss_patch_version;

    return handoverProtocol_SendMsg(HANDOVER_PROTOCOL_VERSION_IND, &ind, sizeof(ind));
}

handover_profile_status_t handoverProtocol_SendStartReq(void)
{
    handover_profile_task_data_t *ho_inst = Handover_GetTaskData();
    HANDOVER_PROTOCOL_START_REQ_T *start_req;
    handover_profile_status_t result;
    size_t alloc_size;

    uint8 num_devices = 0, counter = 0;

    FOR_EACH_HANDOVER_DEVICE(device)
    {
        num_devices++;
    }

    alloc_size= sizeof_HANDOVER_PROTOCOL_START_REQ_T(num_devices);

    start_req = PanicUnlessMalloc(alloc_size);

    ho_inst->session_id++;

    start_req->session_id = ho_inst->session_id;
    start_req->last_tx_seq = appPeerSigGetLastTxMsgSequenceNumber();
    start_req->last_rx_seq = appPeerSigGetLastRxMsgSequenceNumber();
    start_req->mirror_state = MirrorProfile_GetMirrorState();
    start_req->number_of_devices = num_devices;

    FOR_EACH_HANDOVER_DEVICE(device)
    {
        start_req->address[counter++] = device->addr;
    }

    result = handoverProtocol_SendMsg(HANDOVER_PROTOCOL_START_REQ, start_req, alloc_size);
    DEBUG_LOG("handoverProtocol_SendStartReq %d device %d session status enum:handover_profile_status_t:%d",
                   num_devices,
                   start_req->session_id,
                   result);

    free(start_req);
    return result;
}

handover_profile_status_t handoverProtocol_SendCancelInd(void)
{
    return handoverProtocol_SendMsg(HANDOVER_PROTOCOL_CANCEL_IND, NULL, 0);
}

handover_profile_status_t handoverProtocol_SendP1MarshalData(void)
{
    handover_profile_status_t result = HANDOVER_PROFILE_STATUS_SUCCESS;

    FOR_EACH_HANDOVER_DEVICE(device)
    {
        const handover_protocol_opcode_t opcode = HANDOVER_PROTOCOL_MARSHAL_DATA;
        const handover_protocol_marshal_tag_t tag = HANDOVER_MARSHAL_APPSP1_TAG;
        const uint16 header_size = sizeof(opcode) + sizeof(tag);

        Sink sink = Handover_GetTaskData()->link_sink;
        Source source = handoverProfile_MarshalP1Clients(&device->addr);
        uint16 data_len = SourceSize(source);
        uint16 message_size = header_size + data_len;
        uint8 *write_ptr = handoverProtocol_SinkClaimAndMap(sink, header_size, message_size);
        if (write_ptr)
        {
            *write_ptr++ = opcode;
            *write_ptr++ = tag;
            PanicZero(StreamMove(sink, source, data_len));
            SinkFlush(sink, message_size);
            SourceClose(source);
        }
        else
        {
            SourceDrop(source, data_len);
            SourceClose(source);
            result = HANDOVER_PROFILE_STATUS_HANDOVER_FAILURE;
            break;
        }
    }
    return result;
}

handover_profile_status_t handoverProtocol_SendBtStackMarshalData(bool focused)
{
    Sink sink = Handover_GetTaskData()->link_sink;

    FOR_EACH_HANDOVER_DEVICE(device)
    {
        if (device->focused == focused)
        {
            /* Note that the BT stack source data is appended with headers
            HANDOVER_PROTOCOL_MARSHAL_DATA and HANDOVER_MARSHAL_BT_STACK_TAG so
            this code does not need to append them. This saves some time as
            there is no need to map/claim the sink for writing the headers */
            uint16 len = device->u.p.btstack_data_len;
            PanicZero(StreamMove(sink, device->u.p.btstack_source, len));
            PanicFalse(SinkFlush(sink, len));
            DEBUG_LOG_INFO("handoverProtocol_SendBtStackMarshalData stream moved %d bytes", len);
        }
    }
    return HANDOVER_PROFILE_STATUS_SUCCESS;
}

static handover_profile_status_t handoverProtocol_SendStartCfm(handover_profile_status_t status, uint8 session_id)
{
    HANDOVER_PROTOCOL_START_CFM_T start_cfm = {0};

    start_cfm.session_id=session_id;
    start_cfm.status=status;

    return handoverProtocol_SendMsg(HANDOVER_PROTOCOL_START_CFM, &start_cfm, sizeof(start_cfm));
}

static handover_profile_status_t handoverProtocol_SendUnmarshalP1Cfm(void)
{
    return handoverProtocol_SendMsg(HANDOVER_PROTOCOL_UNMARSHAL_P1_CFM, NULL, 0);
}

static uint8* handoverProtocol_SinkClaimAndMap(Sink sink, uint16 size, uint16 min_slack)
{
    uint8 *mapped = NULL;

    if (!min_slack || SinkSlack(sink) >= min_slack)
    {
        uint16 offset = SinkClaim(sink, size);

        if (offset != 0xffff)
        {
            mapped = (uint8*)PanicNull(SinkMap(sink)) + offset;
        }
    }
    return mapped;
}

static handover_profile_status_t handoverProtocol_SendMsg(handover_protocol_opcode_t opcode, void *src_addr, uint16 size)
{
    handover_profile_task_data_t *ho_inst = Handover_GetTaskData();
    uint8 *dst_addr = handoverProtocol_SinkClaimAndMap(ho_inst->link_sink, size+sizeof(opcode), 0);

    if(dst_addr)
    {
        *dst_addr++ = opcode;
        if (size)
        {
            memmove(dst_addr, src_addr, size);
        }
        if (SinkFlush(ho_inst->link_sink, size+sizeof(opcode)))
        {
            DEBUG_LOG_INFO("handoverProtocol_SendMsg enum:handover_protocol_opcode_t:%d success", opcode);
            return HANDOVER_PROFILE_STATUS_SUCCESS;
        }
        else
        {
            DEBUG_LOG_INFO("handoverProtocol_SendMsg enum:handover_protocol_opcode_t:%d flush failed", opcode);
        }
    }
    else
    {
        DEBUG_LOG_INFO("handoverProtocol_SendMsg enum:handover_protocol_opcode_t:%d claim failed", opcode);
    }
    return HANDOVER_PROFILE_STATUS_HANDOVER_TIMEOUT;
}

void handoverProtocol_HandleMessage(Source source)
{
    uint16 size;
    uint32 timeout = 0;
    const uint8 *read_ptr;

    while ((read_ptr = handoverProtocol_WaitForMessage(source,
                                                       HANDOVER_PROTOCOL_ANY_MSG_ID,
                                                       timeout, &size)) != NULL)
    {
        handover_protocol_opcode_t opcode = *read_ptr;
        const uint8 *msg_ptr = read_ptr+1;
        timeout = 0;

        switch (opcode)
        {
            case HANDOVER_PROTOCOL_VERSION_IND:
                handoverProtocol_HandleVersionInd(msg_ptr);
            break;

            case HANDOVER_PROTOCOL_START_REQ:
                if (HANDOVER_PROFILE_STATUS_SUCCESS == handoverProtocol_HandleStartReq(msg_ptr))
                {
                    /* Wait for another message */
                    timeout = HANDOVER_PROFILE_PROTOCOL_MSG_TIMEOUT_MSEC;
                }
            break;

            case HANDOVER_PROTOCOL_CANCEL_IND:
                handoverProtocol_HandleCancelInd();
            break;

            case HANDOVER_PROTOCOL_MARSHAL_DATA:
            {
                if (handoverProtocol_HandleMarshalData(source, read_ptr, size))
                {
                    timeout = HANDOVER_PROFILE_PROTOCOL_MSG_TIMEOUT_MSEC;
                }
                /* Handler is responsible for dropping all marshal data */
                size = 0;
            }
            break;

            case HANDOVER_PROTOCOL_START_CFM:
            case HANDOVER_PROTOCOL_UNMARSHAL_P1_CFM:
                /* Do nothing */
            break;

            default:
                DEBUG_LOG_ERROR("handoverProtocol_HandleMessage Unexpected opcode %d", opcode);
                Panic();
            break;
        }

        if (size)
        {
            SourceDrop(source, size);
        }

        if(!timeout)
        {
            break;
        }
    }

    if (timeout)
    {
        DEBUG_LOG_ERROR("handoverProtocol_HandleMessage timedout waiting for message, cancelling");
        handoverProtocol_HandleCancelInd();
    }
}

static bool handoverProtocol_HandleMarshalData(Source source, const uint8 *src_addr, uint16 src_size)
{
    handover_profile_task_data_t *ho_inst = Handover_GetTaskData();
    handover_profile_status_t status = HANDOVER_PROFILE_STATUS_SUCCESS;
    handover_protocol_marshal_tag_t tag;
    const uint16 header_size = sizeof(handover_protocol_opcode_t) + sizeof(tag);

    PanicFalse(!ho_inst->is_primary);
    PanicFalse(src_size > header_size);

    DEBUG_LOG("HandoverProtocol_HandleMarshalData size=%d", src_size);

    switch (src_addr[1])
    {
        case HANDOVER_MARSHAL_BT_STACK_TAG:
        {
            status = handoverProfile_SecondaryHandleBtStackData(source, src_size);
        }
        break;

        case HANDOVER_MARSHAL_APPSP1_TAG:
        {
            SourceDrop(source, header_size);
            status = handoverProfile_SecondaryHandleAppsP1Data(source, src_size - header_size);
            if (handoverProfile_SecondaryIsAppsP1UnmarshalComplete())
            {
                handoverProtocol_SendUnmarshalP1Cfm();
            }
        }
        break;

        default:
            Panic();
            break;
    }

    if (status == HANDOVER_PROFILE_STATUS_SUCCESS)
    {
        /* If not yet primary, wait for more marshal data */
        return !ho_inst->is_primary;
    }
    else
    {
        return FALSE;
    }
}

static void handoverProtocol_HandleVersionInd(const uint8 *data)
{
    handover_profile_task_data_t *ho_inst = Handover_GetTaskData();
    HANDOVER_PROTOCOL_VERSION_IND_T ind;

    if(ho_inst->is_primary &&
       ho_inst->state == HANDOVER_PROFILE_STATE_CONNECTED)
    {
        uint32 appsp0_version = VmGetFwVersion(FIRMWARE_ID);
        uint32 appsp1_version = VmGetFwVersion(APPLICATION_ID);

        memmove(&ind, data, sizeof(ind));

        DEBUG_LOG("handoverProtocol_HandleVersionInd sec: 0x%x 0x%x 0x%x 0x%x",
                    ind.appsp0_version, ind.appsp1_version, ind.btss_rom_version, ind.btss_patch_version);
        DEBUG_LOG("handoverProtocol_HandleVersionInd pri: 0x%x 0x%x 0x%x 0x%x",
                    appsp0_version, appsp1_version, ho_inst->btss_rom_version, ho_inst->btss_patch_version);

        if (ind.appsp0_version == appsp0_version &&
            ind.appsp1_version == appsp1_version &&
            ind.btss_rom_version == ho_inst->btss_rom_version &&
            ind.btss_patch_version == ho_inst->btss_patch_version)
        {
            DEBUG_LOG("handoverProtocol_HandleVersionInd firmware is matched, handover will be allowed");
            ho_inst->secondary_firmware = HANDOVER_PROFILE_SECONDARY_FIRMWARE_MATCHED;
        }
        else
        {
            DEBUG_LOG("handoverProtocol_HandleVersionInd firmware is mismatched, handover will not be allowed");
            ho_inst->secondary_firmware = HANDOVER_PROFILE_SECONDARY_FIRMWARE_MISMATCHED;
        }
    }
    else
    {
        DEBUG_LOG("handoverProtocol_HandleVersionInd failed");
    }

}

static handover_profile_status_t handoverProtocol_HandleStartReq(const uint8 *data)
{
    HANDOVER_PROTOCOL_START_REQ_T *req;
    handover_profile_status_t status;
    uint8 num_devices;
    size_t req_size;

    memmove(&num_devices, data + offsetof(HANDOVER_PROTOCOL_START_REQ_T, number_of_devices), sizeof(num_devices));
    req_size = sizeof_HANDOVER_PROTOCOL_START_REQ_T(num_devices);

    req = calloc(1, req_size);
    if (req)
    {
        memmove(req, data, req_size);
        DEBUG_LOG("handoverProtocol_HandleStartReq %d device %d session", num_devices, req->session_id);
        status = handoverProfile_SecondaryStart(req);
        status = handoverProtocol_SendStartCfm(status, req->session_id);
        free(req);
        return status;
    }
    else
    {
        DEBUG_LOG("handoverProtocol_HandleStartReq allocation failed %d device", num_devices);
        return HANDOVER_PROFILE_STATUS_HANDOVER_FAILURE;
    }
}

static void handoverProtocol_HandleCancelInd(void)
{
    DEBUG_LOG("handoverProtocol_HandleCancelInd");
    handoverProfile_SecondaryCancel();
}

handover_profile_status_t handoverProtocol_WaitForStartCfm(void)
{
    handover_profile_task_data_t *ho_inst = Handover_GetTaskData();
    uint32 timeout = HANDOVER_PROFILE_PROTOCOL_MSG_TIMEOUT_MSEC;
    const uint8 *buf;

    do
    {
        uint16 size;
        buf = handoverProtocol_WaitForMessage(ho_inst->link_source, HANDOVER_PROTOCOL_START_CFM, timeout, &size);
        if (buf)
        {
            HANDOVER_PROTOCOL_START_CFM_T cfm;
            memmove(&cfm, buf+1, sizeof(cfm));
            SourceDrop(ho_inst->link_source, size);

            if (cfm.session_id == ho_inst->session_id)
            {
                DEBUG_LOG("handoverProtocol_WaitForStartCfm enum:handover_profile_status_t:%d", cfm.status);
                return cfm.status;
            }
            DEBUG_LOG_INFO("handoverProtocol_WaitForStartCfm session ID received %d, expected %d", cfm.session_id, ho_inst->session_id);

            /* Half the timeout for each subsequent attempt */
            timeout /= 2;
        }
        else
        {
            DEBUG_LOG_INFO("handoverProtocol_WaitForStartCfm timeout");
        }

    } while (buf && timeout);

    return HANDOVER_PROFILE_STATUS_HANDOVER_FAILURE;
}

handover_profile_status_t handoverProtocol_WaitForUnmarshalP1Cfm(void)
{
    handover_profile_task_data_t *ho_inst = Handover_GetTaskData();
    uint16 size;

    const uint8 *buf = handoverProtocol_WaitForMessage(ho_inst->link_source,
                                                       HANDOVER_PROTOCOL_UNMARSHAL_P1_CFM,
                                                       HANDOVER_PROFILE_PROTOCOL_MSG_TIMEOUT_MSEC, &size);
    if(buf)
    {
        SourceDrop(ho_inst->link_source, size);
        DEBUG_LOG("handoverProtocol_WaitForUnmarshalP1Cfm success");
        return HANDOVER_PROFILE_STATUS_SUCCESS;
    }
    DEBUG_LOG_INFO("handoverProtocol_WaitForUnmarshalP1Cfm timeout");
    return HANDOVER_PROFILE_STATUS_HANDOVER_FAILURE;
}

static bool handoverProtocol_IsSizeValid(handover_protocol_opcode_t opcode, uint16 size, const uint8 *data)
{
    switch(opcode)
    {
        case HANDOVER_PROTOCOL_START_REQ:
        {
            uint8 num_devices;
            memmove(&num_devices, data + sizeof(handover_protocol_opcode_t) + offsetof(HANDOVER_PROTOCOL_START_REQ_T, number_of_devices), sizeof(num_devices));
            return (size == (sizeof(handover_protocol_opcode_t) + sizeof_HANDOVER_PROTOCOL_START_REQ_T(num_devices)));
        }
        case HANDOVER_PROTOCOL_START_CFM:
            return size == sizeof(handover_protocol_opcode_t) + sizeof(HANDOVER_PROTOCOL_START_CFM_T);
        case HANDOVER_PROTOCOL_CANCEL_IND:
            return size == sizeof(handover_protocol_opcode_t);
        case HANDOVER_PROTOCOL_UNMARSHAL_P1_CFM:
            return size == sizeof(handover_protocol_opcode_t);
        case HANDOVER_PROTOCOL_VERSION_IND:
            return size == sizeof(handover_protocol_opcode_t) + sizeof(HANDOVER_PROTOCOL_VERSION_IND_T);
        case HANDOVER_PROTOCOL_MARSHAL_DATA:
            return size >= sizeof(handover_protocol_opcode_t);
        default:
            return FALSE;
    }
}

static const uint8* handoverProtocol_WaitForMessage(Source src, handover_protocol_opcode_t opcode, uint32 timeout, uint16 *size)
{
    const uint8 *buffer = SourceMap(src);
    *size = 0;
    timeout = VmGetClock() + timeout;
    do
    {
        uint16 actual_size = SourceBoundary(src);

        if (actual_size)
        {
            handover_protocol_opcode_t received_opcode = buffer[0];

            if (handoverProtocol_IsSizeValid(received_opcode, actual_size, buffer))
            {
                if (opcode == HANDOVER_PROTOCOL_ANY_MSG_ID || opcode == received_opcode)
                {
                    DEBUG_LOG_INFO("handoverProtocol_WaitForMessage received enum:handover_protocol_opcode_t:%d", received_opcode);
                    *size = actual_size;
                    break;
                }
                else
                {
                    DEBUG_LOG_INFO("handoverProtocol_WaitForMessage received enum:handover_protocol_opcode_t:%d waiting for enum:handover_protocol_opcode_t:%d",
                                        received_opcode, opcode);
                }
            }
            else
            {
                DEBUG_LOG_INFO("handoverProtocol_WaitForMessage received enum:handover_protocol_opcode_t:%d with invalid size=%d",
                                    received_opcode, actual_size);
            }

            SourceDrop(src, actual_size);
            buffer = SourceMap(src);
        }

    } while (VmGetClock() < timeout);

    return *size ? buffer : NULL;
}


#endif
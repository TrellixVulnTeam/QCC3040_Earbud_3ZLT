/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\defgroup   handover_profile Handover Profile
\ingroup    profiles
\brief      Implementation of the handover of (initial) secondary device.
*/
#ifdef INCLUDE_MIRRORING

#include "handover_profile_private.h"

typedef enum handover_profile_secondary_states
{
    HO_SECONDARY_STATE_IDLE,
    HO_SECONDARY_STATE_SETUP,
    HO_SECONDARY_STATE_RECEIVED_APPSP1_MARSHAL_DATA,
    HO_SECONDARY_STATE_APPSP1_UNMARSHAL_COMPLETE,
    HO_SECONDARY_STATE_RECEIVED_BTSTACK_MARSHAL_DATA,
} handover_profile_secondary_state_t;

static handover_profile_secondary_state_t secondary_state;

static void handoverProfile_SecondaryBecomePrimary(void);
static void handoverProfile_SecondaryCleanup(void);
static bool handoverProfile_SecondaryVeto(uint8 pri_tx_seq, uint8 pri_rx_seq, uint16 mirror_state);
static void handoverProfile_PopulateDeviceList(const HANDOVER_PROTOCOL_START_REQ_T *req);
static bool handoverProfile_ApplyBtStackData(Source source, uint16 len, bool focused);
static void handoverProfile_CommitDevice(handover_device_t *device);
static void handoverProfile_SecondaryWaitForBtStackDataAckTransfer(void);

handover_profile_status_t handoverProfile_SecondaryStart(const HANDOVER_PROTOCOL_START_REQ_T *req)
{
    handover_profile_task_data_t *ho_inst = Handover_GetTaskData();

    if(ho_inst->is_primary)
    {
        DEBUG_LOG_INFO("handoverProfile_SecondaryStart not secondary");
        return HANDOVER_PROFILE_STATUS_HANDOVER_FAILURE;
    }
    else if (ho_inst->state != HANDOVER_PROFILE_STATE_CONNECTED)
    {
        DEBUG_LOG_INFO("handoverProfile_SecondaryStart not connected");
        return HANDOVER_PROFILE_STATUS_HANDOVER_FAILURE;
    }

    handoverProfile_SecondaryCleanup();
    handoverProfile_PopulateDeviceList(req);

    if (handoverProfile_SecondaryVeto(req->last_tx_seq, req->last_rx_seq, req->mirror_state))
    {
        handoverProfile_SecondaryCleanup();
        return HANDOVER_PROFILE_STATUS_HANDOVER_VETOED;
    }

    secondary_state = HO_SECONDARY_STATE_SETUP;

    return HANDOVER_PROFILE_STATUS_SUCCESS;
}

void handoverProfile_SecondaryCancel(void)
{
    handoverProfile_SecondaryCleanup();
}

handover_profile_status_t handoverProfile_SecondaryHandleAppsP1Data(Source source, uint16 len)
{
    switch (secondary_state)
    {
        case HO_SECONDARY_STATE_SETUP:
        case HO_SECONDARY_STATE_RECEIVED_APPSP1_MARSHAL_DATA:
        {
            FOR_EACH_HANDOVER_DEVICE(device)
            {
                if (!device->u.s.appsp1_unmarshal_complete)
                {
                    while (len)
                    {
                        const uint8 *address = SourceMap(source);
                        uint16 consumed = 0;
                        PanicFalse(handoverProfile_UnmarshalP1Client(&device->addr, address, len, &consumed));
                        PanicFalse(consumed <= len);
                        SourceDrop(source, consumed);
                        len -= consumed;
                    }
                    device->u.s.appsp1_unmarshal_complete = TRUE;
                    break;
                }
            }
            /* Failed to consume all the marshal data */
            PanicNotZero(len);

            FOR_EACH_HANDOVER_DEVICE(device)
            {
                if (!device->u.s.appsp1_unmarshal_complete)
                {
                    /* Successful unmarshalling but not yet complete */
                    secondary_state = HO_SECONDARY_STATE_RECEIVED_APPSP1_MARSHAL_DATA;
                    return HANDOVER_PROFILE_STATUS_SUCCESS;
                }
            }
            /* All devices now unmarshalled */
            secondary_state = HO_SECONDARY_STATE_APPSP1_UNMARSHAL_COMPLETE;
            appPowerPerformanceProfileRequest();
        }
        return HANDOVER_PROFILE_STATUS_SUCCESS;

        default:
        {
            DEBUG_LOG_WARN("handoverProfile_SecondaryHandleAppsP1Data invalid state %d", secondary_state);
            SourceDrop(source, len);
        }
        return HANDOVER_PROFILE_STATUS_HANDOVER_FAILURE;
    }
}

handover_profile_status_t handoverProfile_SecondaryHandleBtStackData(Source source, uint16 len)
{
    DEBUG_LOG_WARN("handoverProfile_SecondaryHandleBtStackData state enum:handover_profile_secondary_state_t:%d", secondary_state);

    switch (secondary_state)
    {
        case HO_SECONDARY_STATE_RECEIVED_BTSTACK_MARSHAL_DATA:
        case HO_SECONDARY_STATE_APPSP1_UNMARSHAL_COMPLETE:
        {
            secondary_state = HO_SECONDARY_STATE_RECEIVED_BTSTACK_MARSHAL_DATA;

            /* Marshal data is either for focus device or non-focus device.
               Try unmarshalling the non-focus device first */
            bool nonfocused_done = handoverProfile_ApplyBtStackData(source, len, FALSE);
            if (!nonfocused_done)
            {
                /* If the non focused didn't complete (or it may already have completed
                   when receiving a previous packet, this must be for the focus device */
                bool focused_done = handoverProfile_ApplyBtStackData(source, len, TRUE);
                PanicFalse(focused_done);
                /* The focus device marshal data is always transmitted last,
                   now become primary */
                handoverProfile_SecondaryBecomePrimary();
                handoverProfile_SecondaryCleanup();
            }
        }
        return HANDOVER_PROFILE_STATUS_SUCCESS;

        default:
        {
            DEBUG_LOG_WARN("handoverProfile_SecondaryHandleBtStackData invalid state %d", secondary_state);
            SourceDrop(source, len);
        }
        return HANDOVER_PROFILE_STATUS_HANDOVER_FAILURE;
    }
}

bool handoverProfile_SecondaryIsAppsP1UnmarshalComplete(void)
{
    return (secondary_state == HO_SECONDARY_STATE_APPSP1_UNMARSHAL_COMPLETE);
}

static void handoverProfile_SecondaryCleanup(void)
{
    handover_profile_task_data_t *ho_inst = Handover_GetTaskData();
    handover_device_t *next = ho_inst->device_list;
    while (next)
    {
        handover_device_t *current = next;
        SinkClose(current->u.s.btstack_sink);
        next = current->next;
        current->next = NULL;
        free(current);
    }
    ho_inst->device_list = NULL;

    switch (secondary_state)
    {
        case HO_SECONDARY_STATE_RECEIVED_BTSTACK_MARSHAL_DATA:
        // fallthrough

        case HO_SECONDARY_STATE_APPSP1_UNMARSHAL_COMPLETE:
            appPowerPerformanceProfileRelinquish();
        // fallthrough

        case HO_SECONDARY_STATE_RECEIVED_APPSP1_MARSHAL_DATA:
            /* Call abort to free up any P1 unmarshalled data */
            handoverProfile_AbortP1Clients();
        // fallthrough

        case HO_SECONDARY_STATE_SETUP:
        case HO_SECONDARY_STATE_IDLE:
        break;

        default:
            Panic();
            break;
    }

    secondary_state = HO_SECONDARY_STATE_IDLE;
}

static bool handoverProfile_SecondaryVeto(uint8 pri_tx_seq, uint8 pri_rx_seq, uint16 mirror_state)
{
    uint8 sec_tx_seq = appPeerSigGetLastTxMsgSequenceNumber();
    uint8 sec_rx_seq = appPeerSigGetLastRxMsgSequenceNumber();

    /* Validate if the received and the transmitted peer signalling messages are
    same on both primary and secondary earbuds. If the same, this means there
    are no in-flight peer signalling message. If not, veto to allow time for the
    messages to be cleared. */
    if(sec_rx_seq != pri_tx_seq || pri_rx_seq != sec_tx_seq)
    {
        DEBUG_LOG_INFO("HandoverProfile_SecondaryVeto: PriTx:%x PriRx:%x SecTx:%x SecRx:%x",
                    pri_tx_seq, pri_rx_seq, sec_tx_seq, sec_rx_seq);
        return TRUE;
    }

    if (mirror_state != MirrorProfile_GetMirrorState())
    {
        DEBUG_LOG_INFO("HandoverProfile_SecondaryVeto: Mirror state mismatch: 0x%x 0x%x",
                        mirror_state, MirrorProfile_GetMirrorState());
        return TRUE;
    }

    if (MirrorProfile_Veto() || kymera_a2dp_mirror_handover_if.pFnVeto())
    {
        return TRUE;
    }

    FOR_EACH_HANDOVER_DEVICE(device)
    {
        if (appAvInstanceFindFromBdAddr(&device->addr.taddr.addr))
        {
            /* AV instance is present on Secondary. This is only possible if disconnection caused by
            previous handover is not complete yet. */
            DEBUG_LOG_INFO("HandoverProfile_SecondaryVeto: AV instance exists");
            return TRUE;
        }
    }
    return FALSE;
}

static void handoverProfile_PopulateDeviceList(const HANDOVER_PROTOCOL_START_REQ_T *req)
{
    handover_profile_task_data_t *ho_inst = Handover_GetTaskData();
    unsigned device_counter;

    /* Make a list of handover devices. The order of devices in the start
       request determines the order in which devices are prepared and
       marshalled/unmarshalled during the handover procedure. Therefore it is
       critical for the devices to be added to the device_list in the same order
       as in the start request message. */
    handover_device_t **next = &ho_inst->device_list;

    for (device_counter = 0; device_counter < req->number_of_devices; device_counter++)
    {
        handover_device_t *device = PanicUnlessMalloc(sizeof(*device));
        memset(device, 0, sizeof(*device));
        device->addr = req->address[device_counter];
        device->focused = BdaddrIsSame(MirrorProfile_GetMirroredDeviceAddress(), &device->addr.taddr.addr);

        if (device->focused)
        {
            device->u.s.btstack_sink = StreamAclMarshalSink(&device->addr);
            device->handle = MirrorProfile_GetMirrorAclHandle();
        }
        else
        {
            device->u.s.btstack_sink = StreamAclEstablishSink(&device->addr);
            device->handle = 0xffff;
        }

        *next = device;
        next = &device->next;
    }
}

static bool handoverProfile_ApplyBtStackData(Source source, uint16 len, bool focused)
{
    FOR_EACH_HANDOVER_DEVICE(device)
    {
        if (device->focused == focused)
        {
            if (!device->u.s.btstack_unmarshal_complete)
            {
                uint16 moved;
                Sink sink = device->u.s.btstack_sink;
                PanicZero(sink);
                moved = StreamMove(sink, source, len);
                PanicFalse(moved == len);
                device->u.s.btstack_unmarshal_complete = TRUE;
                device->u.s.btstack_data_len = len;
                return TRUE;
            }
        }
    }
    return FALSE;
}

static void handoverProfile_CommitDevice(handover_device_t *device)
{
    bool p1_commit_first = !device->focused || !MirrorProfile_IsA2dpActive();

    /* Flush BT stack data */
    bool flush;
    if (device->focused)
    {
        DEBUG_LOG_INFO("handoverProfile_CommitDevice flushed %d", device->u.s.btstack_data_len);
        flush = SinkFlush(device->u.s.btstack_sink, device->u.s.btstack_data_len);
    }
    else
    {
        DEBUG_LOG_INFO("handoverProfile_CommitDevice blocking flushed %d", device->u.s.btstack_data_len);
        flush = SinkFlushBlocking(device->u.s.btstack_sink, device->u.s.btstack_data_len);
    }
    PanicFalse(flush);
	
	/* If esco mirroring is active, there could be chance of delayed ACK transferred to old primary.
     * So make sure to commit ACL little delayed to avoid any link role switch which causes to change DAC(device access code)
     */
    if (device->focused && MirrorProfile_IsEscoActive())
    {
        handoverProfile_SecondaryWaitForBtStackDataAckTransfer();
    }

    /* For A2DP mirroring, the earliest the new primary bud may receive
       data from the handset is after the buds re-enter sniff mode. This
       means the P1 commit can be deferred in this mode until after the
       enter sniff command has been sent to the controller */
    if (p1_commit_first)
    {
        /* Commit P1 clients */
        handoverProfile_CommitP1Clients(&device->addr, TRUE);
    }

    /* Commit P0/BTSS clients */
    device->focused ? AclHandoverCommit(device->handle) : AclEstablishCommit(&device->addr);

    if (!p1_commit_first)
    {
        /* Need to commit the mirror profile so it knows about the change
            in role so the peer link policy can be updated correctly */
        mirror_handover_if.pFnCommit(&device->addr, TRUE);
    }

    /* The new primary re-enters sniff mode */
    if (device->focused)
    {
        MirrorProfile_HandoverRefreshSubrate();
        MirrorProfile_UpdatePeerLinkPolicy(lp_sniff);
    }

    if (!p1_commit_first)
    {
        /* Commit P1 clients */
        handoverProfile_CommitP1Clients(&device->addr, TRUE);
    }

    if (device->focused)
    {
        if (!MirrorProfile_WaitForPeerLinkMode(lp_sniff, HANDOVER_PROFILE_REENTER_SNIFF_TIMEOUT_MSEC))
        {
            DEBUG_LOG_INFO("handoverProfile_CommitDevice timeout waiting to re-enter sniff mode");
        }
    }
}

static void handoverProfile_SecondaryWaitForBtStackDataAckTransfer(void)
{
    uint32 timeout = VmGetClock() + HANDOVER_PROFILE_STACK_MARSHAL_DATA_ACK_TIMEOUT_MSEC;
    do
    {
       /*do nothing */
    }while (VmGetClock() < timeout);
}

static void handoverProfile_SecondaryBecomePrimary(void)
{
    handover_profile_task_data_t *ho_inst = Handover_GetTaskData();

    FOR_EACH_HANDOVER_DEVICE(device)
    {
        if (device->focused)
        {
            handoverProfile_CommitDevice(device);
        }
    }
    FOR_EACH_HANDOVER_DEVICE(device)
    {
        if (!device->focused)
        {
            handoverProfile_CommitDevice(device);
        }
    }
    /* Complete P1 clients*/
    handoverProfile_CompleteP1Clients(TRUE);

    /* Update the new peer address */
    PanicFalse(appDeviceGetPeerBdAddr(&ho_inst->peer_addr));
    ho_inst->is_primary = TRUE;

    secondary_state = HO_SECONDARY_STATE_IDLE;

    DEBUG_LOG_INFO("handoverProfile_SecondaryBecomePrimary: I am new Primary");
}

#endif

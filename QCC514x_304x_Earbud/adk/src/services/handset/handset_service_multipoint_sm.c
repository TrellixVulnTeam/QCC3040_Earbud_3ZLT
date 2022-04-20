/*!
\copyright  Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Handset service multipoint state machine
*/

#include <bdaddr.h>
#include <device.h>

#include <device_properties.h>
#include <focus_device.h>
#include <ui_inputs.h>

#include "handset_service_protected.h"

#define HandsetServiceMultipointSm_NotWaitingForConnectCfm() \
                                (HandsetService_GetMultipointSm().connect_cfm_wait_count == 0)

#define HandsetServiceMultipointSm_IncrementConnectCfmWaitCount() \
                                 (HandsetService_GetMultipointSm().connect_cfm_wait_count++)

#define HandsetServiceMultipointSm_DecrementConnectCfmWaitCount() \
                                 ((HandsetService_GetMultipointSm().connect_cfm_wait_count > 0) ? \
                                   HandsetService_GetMultipointSm().connect_cfm_wait_count-- : 0 )

/*! \brief Tell a handset_service multipoint state machine to go to a new state.

    Changing state always follows the same procedure:
    \li Call the Exit function of the current state (if it exists)
    \li Change the current state
    \li Call the Entry function of the new state (if it exists)

    \param state New state to go to.
*/
static void handsetServiceMultipointSm_SetState(handset_service_multipoint_state_t state);

static void handsetService_ExcludeConnectedDevicesFromReconnection(void)
{
    Focus_ResetExcludedDevices();
    
    FOR_EACH_HANDSET_SM(sm)
    {
        if(HandsetServiceSm_IsBredrAclConnected(sm))
        {
            DEBUG_LOG_VERBOSE("handsetService_ExcludeConnectedDevicesFromReconnection excluding %p", sm->handset_device);
            Focus_ExcludeDevice(sm->handset_device);
        }
    }
}

/*! \brief Stores if the Handset reconnection prodedure started or not.

    \param  reconnection_in_progress reconnection started or not.
*/
static void handsetService_SetReconnectionInProgress(bool reconnection_in_progress)
{
    HandsetService_GetMultipointSm().reconnection_in_progress = reconnection_in_progress;
}

/*! \brief Check if handset reconnection is in progress.

    \return bool TRUE when reconnection is in progress,FALSE otherwise.
*/
static bool handsetServiceMultipointSm_IsReconnectionInProgress(void)
{
    return HandsetService_GetMultipointSm().reconnection_in_progress;
}

/*! \brief Add the client task to reconnect_task_list so once reconnection completes
           MP_CONNECT_CFM cam be sent to client.

    \param task Task he MP_CONNECT_CFM will be sent to when the request is completed.
*/
static void handsetServiceMultipointSm_AddTaskToReconnectTaskList(Task task)
{
    TaskList_AddTask(&HandsetService_GetMultipointSm().reconnect_data.reconnect_task_list, task);
}

/*! \brief Set the reconnection data supplied by client.

    \param task Task the MP_CONNECT_CFM will be sent to when the request is completed.
    \param profiles Profiles to connect.
*/
static void handsetServiceMultipointSm_SetReconnectRequestData(const Task task, const uint32 profiles)
{
    HS_LOG("HandsetServiceMultipointSm_SetReconnectRequestData");

    handset_service_multipoint_state_machine_t *mp_sm = &HandsetService_GetMultipointSm();

    mp_sm->reconnect_data.profiles = profiles;
    handsetServiceMultipointSm_AddTaskToReconnectTaskList(task);
}

/*! \brief Reset Reconnection Data supplied by client. */
static void handsetServiceMultipointSm_ResetReconnectRequestData(void)
{
    HS_LOG("handsetServiceMultipointSm_ResetReconnectRequestData");

    TaskList_RemoveAllTasks(&HandsetService_GetMultipointSm().reconnect_data.reconnect_task_list);

    memset(&HandsetService_GetMultipointSm().reconnect_data, 0, 
                sizeof(handset_service_multipoint_reconnect_request_data_t));
}

/*! \brief Send HANDSET_SERVICE_MP_CONNECT_CFM to client requested for handset reconnection.
           Complete all reconnect requests with the given status.

    \param status Status code to complete the requests with. 
*/
static void handsetServiceMultipointSm_SendMpConnectCfm(handset_service_status_t status)
{
    HS_LOG("handsetServiceMultipointSm_SendMpConnectCfm");

    handset_service_multipoint_state_machine_t *mp_sm = &HandsetService_GetMultipointSm();

    if (TaskList_Size(&mp_sm->reconnect_data.reconnect_task_list))
    {
        MESSAGE_MAKE(mp_cfm, HANDSET_SERVICE_MP_CONNECT_CFM_T);
        mp_cfm->status = status;

        /* Send HANDSET_SERVICE_MP_CONNECT_CFM to all clients who made a
           connect request, then remove them from the list. */
        TaskList_MessageSend(&mp_sm->reconnect_data.reconnect_task_list, HANDSET_SERVICE_MP_CONNECT_CFM, mp_cfm);
        TaskList_RemoveAllTasks(&mp_sm->reconnect_data.reconnect_task_list);
    }
}

/*! \brief RESET the reconnection_in_progress flag and data associated with reconnection.
*/
static void handsetServiceMultipointSm_TidyUp(void)
{
    HS_LOG("handsetServiceMultipointSm_TidyUp");
    /* Reset that reconnection has been completed. */
    handsetService_SetReconnectionInProgress(FALSE);

    HandsetService_GetMultipointSm().connect_cfm_wait_count = 0;

    /* Resetting reconnection data. */
    handsetServiceMultipointSm_ResetReconnectRequestData();
}

/*! \brief Only send HANDSET_SERVICE_MP_CONNECT_CFM to client if reconnection 
           is in process and multipoint sm is not waiting for CONNECT_CFM from 
           handset_service_sm.
           Also RESET the reconnection_in_progress flag and data associated with 
           reconnection.

    \param status Status code to complete the requests with. 
*/
static void handsetServiceMultipointSm_SendMpConnectCfmAndTidyUp(handset_service_status_t status)
{
    HS_LOG("handsetServiceMultipointSm_SendMpConnectCfmAndTidyUp status enum:handset_service_status_t:%d",status);

    if (   handsetServiceMultipointSm_IsReconnectionInProgress()
        && HandsetServiceMultipointSm_NotWaitingForConnectCfm()
       )
    {
        handsetServiceMultipointSm_SendMpConnectCfm(status);

        handsetServiceMultipointSm_TidyUp();
    }
}

static bool handsetServiceMultipointSm_IsHandsetConnectInProgress(const bdaddr *addr)
{
    bool in_progress = FALSE;
    PanicNull((void *)addr);

    FOR_EACH_HANDSET_SM(sm)
    {
        if (sm->state != HANDSET_SERVICE_STATE_NULL)
        {
            HS_LOG("handsetServiceMultipointSm_IsHandsetConnectInProgress [%04x,%02x,%06lx] req [%04x,%02x,%06lx] state enum:handset_service_state_t:%d",
                                sm->handset_addr.nap,
                                sm->handset_addr.uap,
                                sm->handset_addr.lap,
                                addr->nap,
                                addr->uap,
                                addr->lap,
                                sm->state);
            if (BdaddrIsSame(&sm->handset_addr, addr))
            {
                if ( sm->state == HANDSET_SERVICE_STATE_CONNECTING_BREDR_ACL 
                  || sm->state == HANDSET_SERVICE_STATE_CONNECTING_BREDR_PROFILES )
                {
                    in_progress = TRUE;
                }
                break;
            }
        }
    }
    return in_progress;
}

static bool handsetServiceMultipointSm_GetBredrAddrOfHandsetConnectInProgress(bdaddr *addr)
{
    bool bredr_handset = FALSE;
    PanicNull(addr);

    FOR_EACH_HANDSET_SM(sm)
    {
        if (sm->state != HANDSET_SERVICE_STATE_NULL)
        {
            if ( sm->state == HANDSET_SERVICE_STATE_CONNECTING_BREDR_ACL
              || sm->state == HANDSET_SERVICE_STATE_CONNECTING_BREDR_PROFILES)
            {
                *addr = sm->handset_addr;
                HS_LOG("handsetServiceMultipointSm_GetBredrAddrOfHandsetConnectInProgress [%04x,%02x,%06lx]",
                                sm->handset_addr.nap,
                                sm->handset_addr.uap,
                                sm->handset_addr.lap);
                bredr_handset = TRUE;
                break;
            }
        }
    }
    return bredr_handset;
}

static bool handsetServiceMultipointSm_StopHandsetReconnectionInProgress(void)
{
    bool stopping = FALSE;
    bdaddr hs_addr;

    /* Get the address of the Handset currently trying to connect. */
    if(handsetServiceMultipointSm_GetBredrAddrOfHandsetConnectInProgress(&hs_addr))
    {
        HS_LOG("handsetServiceMultipointSm_StopHandsetReconnectionInProgress stopping [%04x,%02x,%06lx]",
                                hs_addr.nap,
                                hs_addr.uap,
                                hs_addr.lap);
        HandsetService_StopConnect((Task)&HandsetService_GetMultipointSm().task_data, &hs_addr);

        stopping = TRUE;
    }
    return stopping;
}

/*! \brief Only send HANDSET_SERVICE_MP_CONNECT_STOP_CFM to client.

    \param status Status code to complete the requests with. 
*/
static void handsetServiceMultipointSm_SendMpConnectStopCfm(handset_service_status_t status)
{
    HS_LOG("handsetServiceMultipointSm_SendMpConnectStopCfm status enum:handset_service_status_t:%d",status);

    handset_service_multipoint_state_machine_t *mp_sm = &HandsetService_GetMultipointSm();

    if (mp_sm->stop_reconnect_task)
    {
        MESSAGE_MAKE(mp_cfm, HANDSET_SERVICE_MP_CONNECT_STOP_CFM_T);
        mp_cfm->status = status;

        /* Send HANDSET_SERVICE_MP_CONNECT_STOP_CFM to client who made a request */
        MessageSend(mp_sm->stop_reconnect_task, HANDSET_SERVICE_MP_CONNECT_STOP_CFM, mp_cfm);
    }

    mp_sm->stop_reconnect_task = NULL;
    mp_sm->stop_reconnect_in_progress = FALSE;
}

/*! \brief Set Handset Service MP state to IDLE and reset excludelist (device_property_excludelist). */
static void handsetServiceMultipointSm_ResetMpState(void)
{
    HS_LOG("handsetServiceMultipointSm_ResetMpStateData");

    /* Set the MP state to IDLE. */
    handsetServiceMultipointSm_SetState(HANDSET_SERVICE_MP_STATE_IDLE);
}

static void handsetServiceMultipointSm_ExitGetDeviceToConnect(void)
{
    HS_LOG("handsetServiceMultipointSm_ExitGetDeviceToConnect");
}

static void handsetServiceMultipointSm_ExitGetNextDeviceToConnect(void)
{
    HS_LOG("handsetServiceMultipointSm_ExitGetNextDeviceToConnect");
}

static void handsetServiceMultipointSm_EnterGetDeviceToConnect(void)
{
    device_t handset_device;
    handset_service_multipoint_state_machine_t *mp_sm = &HandsetService_GetMultipointSm();

    HS_LOG("handsetServiceMultipointSm_EnterGetDeviceToConnect");

    /* if there is handset device to connect then go ahead with connecting to it. */
    if (Focus_GetDeviceForUiInput(ui_input_connect_handset, &handset_device))
    {
        bdaddr hs_addr = DeviceProperties_GetBdAddr(handset_device);
        uint32 requested_profiles = mp_sm->reconnect_data.profiles;

        /* Try to connect the profiles requested by topology for multiple handsets. */
        HS_LOG("handsetServiceMultipointSm_EnterGetDeviceToConnect handset_device 0x%p requested_profiles 0x%08x",
                handset_device,
                requested_profiles);

        /* If this returns TRUE suggestes we have already requested for handset connection.
           This can occur when handset connection requested for AG-A (already), and other 
           AG (AG-B) establishes ACL to device(application) which will make handset service
           to kick handset_service_multipoint_sm to look for next device for connection.
           If handset connection already in connecting state, ignore sending request. */
        if (!handsetServiceMultipointSm_IsHandsetConnectInProgress(&hs_addr))
        {
            /* Handset Service should send CONNECT_CFM to Handset Service Multipoint SM
            for following connect request. */
            HandsetService_ConnectAddressRequest(&mp_sm->task_data, &hs_addr, requested_profiles);

            /* Requested for Handset connect so increment the connect_cfm_wait_count. */
            HandsetServiceMultipointSm_IncrementConnectCfmWaitCount();
        }
    }
    /* Make sure to send HANDSET_SERVICE_MP_CONNECT_CFM if not waiting for CONNECT_CFM.
    Also tidyup and move back to IDLE state */
    else
    {
        handsetServiceMultipointSm_SendMpConnectCfmAndTidyUp(handset_service_status_success);

        handsetServiceMultipointSm_ResetMpState();
    }
}

static bool handsetServiceMultipointSm_MultipointBargeInEnabled(void)
{
#ifdef MULTIPOINT_BARGE_IN_ENABLED
    return TRUE;
#else
    return FALSE;
#endif
}

static void handsetServiceMultipointSm_EnterGetNextDeviceToConnect(void)
{
    bool max_connections_reached;
    device_t handset_device;

    HS_LOG("handsetServiceMultipointSm_EnterGetNextDeviceToConnect");
    max_connections_reached = HandsetServiceSm_MaxBredrAclConnectionsReached();

    if (!max_connections_reached &&
        Focus_GetDeviceForUiInput(ui_input_connect_handset, &handset_device) &&
        handsetServiceMultipointSm_IsReconnectionInProgress() &&
        !handsetServiceMultipointSm_MultipointBargeInEnabled())
    {
        /* Set the MP state to get the device to connect to */
        handsetServiceMultipointSm_SetState(HANDSET_SERVICE_MP_STATE_GET_DEVICE);
    }
    else
    {
        handsetServiceMultipointSm_SendMpConnectCfmAndTidyUp(handset_service_status_success);

        handsetServiceMultipointSm_ResetMpState();
    }
}

static void handsetServiceMultipointSm_SetState(handset_service_multipoint_state_t state)
{
    handset_service_multipoint_state_t old_state;
    handset_service_multipoint_state_machine_t *mp_sm = &HandsetService_GetMultipointSm();

    /* copy old state */
    old_state = mp_sm->state;

    HS_LOG("handsetServiceMultipointSm_SetState enum:handset_service_multipoint_state_t:%d -> enum:handset_service_multipoint_state_t:%d", old_state, state);

    /* Handle state exit functions */
    switch (old_state)
    {
        case HANDSET_SERVICE_MP_STATE_GET_DEVICE:
            handsetServiceMultipointSm_ExitGetDeviceToConnect();
            break;

        case HANDSET_SERVICE_MP_STATE_GET_NEXT_DEVICE:
            handsetServiceMultipointSm_ExitGetNextDeviceToConnect();
            break;

        default:
            break;
    }

    /* set new state */
    mp_sm->state = state;

    /* Handle state entry functions */
    switch (state)
    {
        /* only get the device if old state was either IDLE or GET_NEXT_DEVICE */
        case HANDSET_SERVICE_MP_STATE_GET_DEVICE:
            if (old_state == HANDSET_SERVICE_MP_STATE_IDLE ||
                old_state == HANDSET_SERVICE_MP_STATE_GET_NEXT_DEVICE)
            {
                handsetServiceMultipointSm_EnterGetDeviceToConnect();
            }
            break;

        case HANDSET_SERVICE_MP_STATE_GET_NEXT_DEVICE:
            handsetServiceMultipointSm_EnterGetNextDeviceToConnect();
            break;

        default:
            break;
    }
}

/*
    Message handler functions
*/

static void handsetServiceMultipointSm_HandleConnectCfm(const HANDSET_SERVICE_CONNECT_CFM_T *cfm)
{
    HS_LOG("handsetServiceMultipointSm_HandleConnectCfm addr %04x,%02x,%06lx status: enum:handset_service_status_t:%d",
            cfm->addr.nap,
            cfm->addr.uap,
            cfm->addr.lap,
            cfm->status);

    /* CONNECT_CFM received so decrement the connect_cfm_wait_count */
    HandsetServiceMultipointSm_DecrementConnectCfmWaitCount();

    handsetServiceMultipointSm_SendMpConnectCfmAndTidyUp(cfm->status);

    /* Not waiting for CONNECT_CFM, Set the MP state to IDLE. */
    if (HandsetServiceMultipointSm_NotWaitingForConnectCfm())
    {
        /* Set the MP state to IDLE and reset device_property_excludelist 
        for devices excluded. */
        handsetServiceMultipointSm_ResetMpState();
    }
}

static void handsetServiceMultipointSm_HandleConnectStopCfm(const HANDSET_SERVICE_CONNECT_STOP_CFM_T *cfm)
{
    HS_LOG("handsetServiceMultipointSm_HandleConnectStopCfm addr %04x,%02x,%06lx status enum:handset_service_status_t:%d",
            cfm->addr.nap,
            cfm->addr.uap,
            cfm->addr.lap,
            cfm->status);

    /* No more handset connections need to be stopped, send the MP_STOP_CONNECT_CFM. */
    if(!handsetServiceMultipointSm_StopHandsetReconnectionInProgress())
    {
        handsetServiceMultipointSm_SendMpConnectStopCfm(cfm->status);
    }
}
static void handsetServiceMultipointSm_MessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);

    HS_LOG("handsetServiceMultipointSm_MessageHandler id MESSAGE:handset_service_msg_t:0x%x", id);

    switch (id)
    {
        case HANDSET_SERVICE_CONNECT_CFM:
            handsetServiceMultipointSm_HandleConnectCfm((const HANDSET_SERVICE_CONNECT_CFM_T *)message);
            break;

        case HANDSET_SERVICE_CONNECT_STOP_CFM:
            handsetServiceMultipointSm_HandleConnectStopCfm((const HANDSET_SERVICE_CONNECT_STOP_CFM_T *)message);
            break;

        default:
            HS_LOG("handsetServiceMultipointSm_MessageHandler unhandled msg id MESSAGE:handset_service_msg_t:0x%x", id);
            break;
    }
}

void HandsetServiceMultipointSm_SetStateToGetNextDevice(void)
{
    handsetServiceMultipointSm_SetState(HANDSET_SERVICE_MP_STATE_GET_NEXT_DEVICE);
}

void HandsetServiceMultipointSm_ReconnectRequest(Task task, uint32 profiles)
{
    bool max_connections_reached = HandsetServiceSm_MaxBredrAclConnectionsReached();

    HS_LOG("HandsetServiceMultipointSm_ReconnectRequest task 0x%x profiles 0x%08x enum:handset_service_multipoint_state_t:%d max_connections_reached %d",
                task,
                profiles, 
                HandsetService_GetMultipointSm().state,
                max_connections_reached);

    if(!max_connections_reached)
    {
        /* New reconnect request while reconnection underway, just add the
        task to the list so once reconnection completes MP_CONNECT_CFM will
        be sent. */
        if (handsetServiceMultipointSm_IsReconnectionInProgress())
        {
            handsetServiceMultipointSm_AddTaskToReconnectTaskList(task);
            return;
        }

        handsetService_ExcludeConnectedDevicesFromReconnection();

        /* Store the requested data in Handset Service Multipoint SM. */
        handsetServiceMultipointSm_SetReconnectRequestData(task, profiles);

        /* Set that starting the reconnection. */
        handsetService_SetReconnectionInProgress(TRUE);

        /* Set the MP state to get the device to connect to. */
        handsetServiceMultipointSm_SetState(HANDSET_SERVICE_MP_STATE_GET_DEVICE);
    }
    else
    {
        /* max connection reached so send the MP_CONNECT_CFM straight away. */
        MESSAGE_MAKE(mp_cfm, HANDSET_SERVICE_MP_CONNECT_CFM_T);
        mp_cfm->status = handset_service_status_success;
        MessageSend(task, HANDSET_SERVICE_MP_CONNECT_CFM, mp_cfm);
    }
}

void HandsetServiceMultipointSm_StopReconnect(Task task)
{
    HS_LOG("HandsetServiceMultipointSm_StopReconnect task[%p]",task);

    /* Not expecting the stop connect from other client than one requested already.
       no chance of happening such scenario but being defensive. */
    if (HandsetService_GetMultipointSm().stop_reconnect_task)
    {
        if (HandsetService_GetMultipointSm().stop_reconnect_task != task)
        {
            DEBUG_LOG_ERROR("HandsetServiceMultipointSm_StopReconnect Called by two Tasks: task %p stop_reconnect_task %p", 
                               task,
                               HandsetService_GetMultipointSm().stop_reconnect_task);
            Panic();
         }
         else
         {
             DEBUG_LOG_WARN("HandsetServiceMultipointSm_StopReconnect called twice; task %p", task);
         }
    }
    else
    {
        /* store the client task requested to stop reconnection. */
        HandsetService_GetMultipointSm().stop_reconnect_task = task;
    }

    /* if reconnection stop is not in progress, suggests, trying to connect.
       ignore the requests for stop if already in progress. */
    if (!HandsetService_GetMultipointSm().stop_reconnect_in_progress)
    {
        /* stop reconnection is starting. */
        HandsetService_GetMultipointSm().stop_reconnect_in_progress = TRUE;

        /* Stop the Handset currently trying to connect. */
        if(!handsetServiceMultipointSm_StopHandsetReconnectionInProgress())
        {
            HS_LOG("HandsetServiceMultipointSm_StopReconnect no handset connection to stop");
            handsetServiceMultipointSm_SendMpConnectStopCfm(handset_service_status_disconnected);

            /* As nothing to do, should tidyup here. */
            handsetServiceMultipointSm_TidyUp();
        }
    }
}

void HandsetServiceMultipointSm_Init(void)
{
    HS_LOG("HandsetServiceMultipointSm_Init");

    HandsetService_GetMultipointSm().task_data.handler = handsetServiceMultipointSm_MessageHandler;

    HandsetService_GetMultipointSm().connect_cfm_wait_count = 0;

    /* Set the Handset Service MP state to IDLE. */
    handsetServiceMultipointSm_SetState(HANDSET_SERVICE_MP_STATE_IDLE);

    /* Set the stop_reconnect_task to NULL to start with. */
    HandsetService_GetMultipointSm().stop_reconnect_task = NULL;
    HandsetService_GetMultipointSm().stop_reconnect_in_progress = FALSE;

    /* Reset the reconnection_in_progress. */
    handsetService_SetReconnectionInProgress(FALSE);

    TaskList_Initialise(&HandsetService_GetMultipointSm().reconnect_data.reconnect_task_list);
    /* Reset reconnection data. */
    handsetServiceMultipointSm_ResetReconnectRequestData();

    /* Register the task to receive Handset Service messages. */
    HandsetService_ClientRegister(&HandsetService_GetMultipointSm().task_data);
}

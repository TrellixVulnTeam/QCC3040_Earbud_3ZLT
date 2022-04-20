/*!
\copyright  Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    handset_service
\brief      Handset service multipoint state machine
*/

#ifndef HANDSET_SERVICE_MULTIPOINT_SM_H_
#define HANDSET_SERVICE_MULTIPOINT_SM_H_

#include <device.h>
#include <task_list.h>

/*@{*/

/*! \brief Handset Service Multipoint states.

@startuml Handset Service Multipoint States

state HANDSET_SERVICE_MP_SM{
    state IDLE : No connection 
    state GET_DEVICE : Get handset device to connect
    state GET_NEXT_DEVICE : Get next handset device to connect

    [*] -down-> IDLE : No device

    IDLE -down--> GET_DEVICE : HandsetServiceMultipointSm_ReconnectRequest()

    state c <<choice>>
    GET_DEVICE -down-> c
    c -up-> IDLE : [DEVICE_FOUND = FALSE]

    state d <<choice>>
    GET_NEXT_DEVICE -down-> d
    d -> GET_DEVICE : [MAX_DEVICE_REACHED = FALSE && \n DEVICE_FOUND = TRUE]
    d -> IDLE : [MAX_DEVICE_REACHED = TRUE]
}
state HANDSET_SERVICE_SM {
    state DISCONNECTED : Handset not connected
    state CONNECTING : Pseudo-state for connecting sub-states.
    
    c -down--> DISCONNECTED : [DEVICE_FOUND = TRUE]\nHandsetService_ConnectAddressRequest()
    state CONNECTING {
        state CONNECTING_ACL : ACL connecting
        
        CONNECTING_ACL --> GET_NEXT_DEVICE : ACL connected
    }
}
@enduml
*/
typedef enum
{
    HANDSET_SERVICE_MP_STATE_IDLE = 0,
    HANDSET_SERVICE_MP_STATE_GET_DEVICE = 1,
    HANDSET_SERVICE_MP_STATE_GET_NEXT_DEVICE = 2,
} handset_service_multipoint_state_t;

typedef struct
{
    /*! Client task list for connect requests */
    task_list_t reconnect_task_list;

    /*! Mask of profiles that have been requested. */
    uint32 profiles;
}handset_service_multipoint_reconnect_request_data_t;

/*! \brief Context for handset service multipoint state machine. */
typedef struct
{
    /* Task for Handset Service Multipoint SM. */
    TaskData task_data;

    /*! Current state */
    handset_service_multipoint_state_t state;

    /* if Handset reconnection is in progress. */
    bool reconnection_in_progress;

    /* if Handset reconnection stop is in progress.
       TRUE when requested to stop the handset reconnection,
       FALSE otherwise. */
    bool stop_reconnect_in_progress;

    /*! Number of HANDSET_SERVICE_CONNECT_CFM to wait for.
    It will be incremented after requesting for handset connection
    (i.e. HandsetService_ConnectAddressRequest()), and decremented
    when HANDSET_SERVICE_CONNECT_CFM is received. */
    unsigned connect_cfm_wait_count;

    /*! Client task for stop connect requests */
    Task stop_reconnect_task;

    /* Reconnect data supplied by Client when requested for reconnection. */
    handset_service_multipoint_reconnect_request_data_t reconnect_data;
} handset_service_multipoint_state_machine_t;

/*! \brief Initialise a multipoint state machine.

    After this is complete the state machine will be in the
    HANDSET_SERVICE_MP_STATE_IDLE state.
*/
void HandsetServiceMultipointSm_Init(void);

/*! \brief Tell a handset_service multipoint state machine to go to 
           GET_NEXT_DEVICE state.
*/
void HandsetServiceMultipointSm_SetStateToGetNextDevice(void);

/*! \brief Connect to the previously connected  handset(s).

    Store the data supplied by client requested to reconnect to a handset.

    When reconnect completes, for whatever reason, the result is sent to the client 
    #task in a HANDSET_SERVICE_MP_CONNECT_CFM.

    \param task Task the CFM will be sent to when the request is completed.
    \param profiles Profiles to connect.

    \note Requested profiles will be tried to connect for the handset returned by Focus Device 
          (Focus_GetDeviceForUiInput()).
*/
void HandsetServiceMultipointSm_ReconnectRequest(Task task, uint32 profiles);

/*! \brief Stop the Handset Reconnection.

    When reconnect completes, for whatever reason, the result is sent to the client 
    #task in a HANDSET_SERVICE_MP_CONNECT_STOP_CFM.

    \param task Task the CFM will be sent to when the request is completed.
*/
void HandsetServiceMultipointSm_StopReconnect(Task task);

/*@}*/

#endif /* HANDSET_SERVICE_MULTIPOINT_SM_H_ */

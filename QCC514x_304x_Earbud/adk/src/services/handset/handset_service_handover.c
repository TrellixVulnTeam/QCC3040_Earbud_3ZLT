/*!
\copyright  Copyright (c) 2019-2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       handset_service_handover.c
\brief      Handset Service handover related interfaces

*/

#include <bdaddr.h>
#include <logging.h>
#include <panic.h>
#include <app_handover_if.h>
#include <device_properties.h>
#include <device_list.h>
#include <focus_device.h>
#include "handset_service_sm.h"
#include "handset_service_protected.h"
#include "handset_service_config.h"
/******************************************************************************
 * Local Function Prototypes
 ******************************************************************************/
static bool handsetService_Veto(void);
static void handsetService_Commit(bool is_primary);

/******************************************************************************
 * Global Declarations
 ******************************************************************************/
REGISTER_HANDOVER_INTERFACE_NO_MARSHALLING(HANDSET_SERVICE, handsetService_Veto, handsetService_Commit);

#define handsetService_BrEdrConnectionsAreInRange(connections) \
    (connections && (connections <= handsetService_BredrAclMaxConnections()))

#define handsetService_IsStateUnstable(state) \
    (HANDSET_SERVICE_STATE_NULL != state) && \
    (HANDSET_SERVICE_STATE_CONNECTED_BREDR != state) && \
    (HANDSET_SERVICE_STATE_DISCONNECTED != state) \

#define handsetService_IsHandsetConnected(state) \
    (HANDSET_SERVICE_STATE_CONNECTED_BREDR == state)

/******************************************************************************
 * Local Function Definitions
 ******************************************************************************/
/*! 
    \brief Handle Veto check during handover
           TRUE is returned if handset service is not in CONNECTED state.
    \return bool
*/
static bool handsetService_Veto(void)
{
    bool veto = FALSE;
    uint8 connected_handset_count = 0;

    FOR_EACH_HANDSET_SM(sm)
    {
        /* Veto if any handset state is unstable. Ignore LE connection state
         * as it gets disconnected during handover procedure */
        if (handsetService_IsStateUnstable(sm->state))
        {
            veto = TRUE;
            DEBUG_LOG_INFO("handsetService_Veto, Unstable handset state enum:handset_service_state_t:state[%d]", sm->state);
            break;
        }

        if(handsetService_IsHandsetConnected(sm->state))
        {
            connected_handset_count++;
        }
    }

    if(!veto && !handsetService_BrEdrConnectionsAreInRange(connected_handset_count))
    {
        veto = TRUE;
        DEBUG_LOG_INFO("handsetService_Veto, Number of handsets in connected state: %d", connected_handset_count);
    }

    return veto;
}

/*!
    \brief Component commits to the specified role

    The component should take any actions necessary to commit to the
    new role.

    \param[in] is_primary   TRUE if device role is primary, else secondary

*/
static void handsetService_Commit(bool is_primary)
{
    if(is_primary)
    {
        cm_connection_iterator_t iterator;
        tp_bdaddr addr;
        handset_service_state_machine_t *sm;
        /* Populate state machine for the active device connections as per connection manager records */
        if (ConManager_IterateFirstActiveConnection(&iterator, &addr))
        {
            do
            {
                /* Interested in only handset connections */
                if (appDeviceIsHandset(&addr.taddr.addr))
                {
                    /* It should create the new statemachine and set the device for the created statemachine */
                    sm = handsetService_FindOrCreateSm(&addr);
                    PanicNull((void*)sm);
                    sm->state = HANDSET_SERVICE_STATE_CONNECTED_BREDR;
                    /* Add this connected handset device to excluded list */
                    device_t device = BtDevice_GetDeviceForBdAddr(&addr.taddr.addr);
                    Focus_ExcludeDevice(device);
                }

            }while (ConManager_IterateNextActiveConnection(&iterator, &addr));
        }
        /* register for connection manager events in new primary role */
        ConManagerRegisterTpConnectionsObserver(cm_transport_all, HandsetService_GetTask());
    }
    else
    {
        FOR_EACH_HANDSET_SM(sm)
        {
            HandsetServiceSm_DeInit(sm);
        }
        Focus_ResetExcludedDevices();
    }
}


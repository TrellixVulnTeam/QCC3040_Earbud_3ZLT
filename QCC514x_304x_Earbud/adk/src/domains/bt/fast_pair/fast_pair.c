/*!
\copyright  Copyright (c) 2008 - 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       fast_pair.c
\brief      Fast Pair State Machine Task
*/


#include "fast_pair.h"
#include "fast_pair_events.h"
#include "fast_pair_gfps.h"
#include "fast_pair_pairing_if.h"
#include "fast_pair_advertising.h"
#include "fast_pair_session_data.h"
#include "fast_pair_null_state.h"
#include "fast_pair_idle_state.h"
#include "fast_pair_wait_aes_key_state.h"
#include "fast_pair_wait_pairing_request_state.h"
#include "fast_pair_wait_passkey_state.h"
#include "fast_pair_wait_account_key_state.h"
#include "fast_pair_account_key_sync.h"
#include "fast_pair_battery_notifications.h"
#include "fast_pair_msg_stream.h"
#include "fast_pair_wait_additional_data_state.h"
#include "fast_pair_pname_state.h"
#include "fast_pair_pname_sync.h"
#include "fast_pair_msg_stream_dev_action.h"
#include "fast_pair_config.h"
#ifdef FAST_PAIR_TIME_PROFILER
#include "fast_pair_time_profiler.h"
#endif /* FAST_PAIR_TIME_PROFILER */

#include "system_state.h"
#include "device_properties.h"
#include "phy_state.h"

#include <connection_manager.h>
#include <connection_message_dispatcher.h>
#include <local_addr.h>
#include <ps_key_map.h>

#include <panic.h>
#include <connection.h>
#include <ui.h>
#include <device.h>
#include <device_list.h>
#include <ps.h>
#include <string.h>
#include <cryptovm.h>
#include <stdio.h>

#include <cc_with_case.h>
#include "state_proxy.h"

LOGGING_PRESERVE_MESSAGE_TYPE(fast_pair_state_event_id)

/*!< Fast Pair task */
fastPairTaskData fast_pair_task_data;

/*! @brief Clear FP Session Information.
 */
static void fastPair_FreeSessionDataMemory(void)
{
    fastPairTaskData *theFastPair;
    theFastPair = fastPair_GetTaskData();

    if(theFastPair->session_data.private_key != NULL)
    {
        free(theFastPair->session_data.private_key);
        theFastPair->session_data.private_key= NULL;
    }
    
    if(theFastPair->session_data.public_key != NULL)
    {
        free(theFastPair->session_data.public_key);
        theFastPair->session_data.public_key= NULL;
    }

    if(theFastPair->session_data.encrypted_data != NULL)
    {
        free(theFastPair->session_data.encrypted_data);
        theFastPair->session_data.encrypted_data= NULL;
    }

    if(theFastPair->session_data.aes_key != NULL)
    {
        free(theFastPair->session_data.aes_key);
        theFastPair->session_data.aes_key= NULL;
    }

    theFastPair->session_data.kbp_action_request_data_id = 0xFF;
    theFastPair->session_data.kbp_pname_request_flag = FALSE;
}


/*! @brief Cancel FP Procedure and check for Repeated Invalid KbP Writes.
 */
static void fastPair_EnterIdle(fastPairTaskData *theFastPair)
{
    DEBUG_LOG("appFastPairEnterIdle");

    fastPair_StopTimer();
    fastPair_PairingReset();
    /* Set the pairing initiator flag to FALSE while moving to IDLE state*/
    theFastPair->provider_pairing_requested = FALSE;
    if(theFastPair->failure_count == FAST_PAIR_MAX_FAIL_ATTEMPTS)
    {
        theFastPair->failure_count = 0;

        /* Fail all new writes to KbP for next 5 minutes or till a Power Off/Power On event */ 
        fastPair_SetState(theFastPair, FAST_PAIR_STATE_NULL);
        
        fastPair_StartTimer(TRUE);
    }
    
    fastPair_FreeSessionDataMemory();
}

/*! @brief FP Procedure starting. Enter Wait state for AES key to be established.
 */
static void fastPair_EnterWaitAESKey(void)
{
    DEBUG_LOG("fastPair_EnterWaitAESKey");
}

/*! @brief Enter Wait state for Pairing request from FP seeker.
 */
static void fastPair_EnterWaitPairingRequest(void)
{
    DEBUG_LOG("fastPair_EnterWaitPairingRequest");

    fastPair_StartTimer(FALSE);
}

/*! @brief Enter Wait state for Passkey to be written by FP Seeker.
 */
static void fastPair_EnterWaitPasskey(void)
{
    DEBUG_LOG("fastPair_EnterWaitPasskey");
    
    fastPair_StartTimer(FALSE);
}

/*! @brief Enter Wait state for Account key to be written by FP Seeker.
 */
static void fastPair_EnterWaitAccountKey(void)
{
    DEBUG_LOG("fastPair_EnterWaitAccountKey");
    
    fastPair_StartTimer(FALSE);
}

/*! @brief Enter Wait state for Account key to be written by FP Seeker.
 */
static void fastPair_EnterWaitAdditionalData(void)
{
    DEBUG_LOG("fastPair_EnterWaitAdditionalData");

    fastPair_StartTimer(FALSE);
}

static void fastPair_EnterPName(void)
{
    DEBUG_LOG("fastPair_EnterPName");
}

/*! @brief Initialize Session data to NULL.
 */
static void fastPair_InitSessionData(void)
{
    fastPairTaskData *theFastPair = fastPair_GetTaskData();
    theFastPair->session_data.private_key = NULL;
    theFastPair->session_data.public_key = NULL;
    theFastPair->session_data.encrypted_data = NULL;
    theFastPair->session_data.aes_key = NULL;
}

/*! @brief Handle Power ON/OFF system state change events
 */
static void fastPair_HandleSystemStateChange(SYSTEM_STATE_STATE_CHANGE_T *msg)
{
    fastPairTaskData *theFastPair;
    theFastPair = fastPair_GetTaskData();

    DEBUG_LOG("fastPair_HandleSystemStateChange old state 0x%x, new state 0x%x", msg->old_state, msg->new_state);

    if(msg->old_state == system_state_powering_off && msg->new_state == system_state_limbo)
    {
        uint16* ring_stop = PanicUnlessMalloc(sizeof(uint16));
            
        *ring_stop = FP_STOP_RING_CURRENT;
        DEBUG_LOG("fastPair_HandleSystemStateChange. Set FP state to NULL");
        MessageSend(fpRingDevice_GetTask(), fast_pair_ring_stop_event, ring_stop);
        fastPair_SetState(theFastPair, FAST_PAIR_STATE_NULL);
    }

    else if(msg->old_state == system_state_powering_on && msg->new_state == system_state_active)
    {
        /*! Set the fast pair state to idle state only if the current state is NULL*/
        if(theFastPair->state == FAST_PAIR_STATE_NULL)
        {
            fastPair_SetState(theFastPair, FAST_PAIR_STATE_IDLE);
        }
        else
        {
            DEBUG_LOG("fastPair_HandleSystemStateChange. Already powered ON");
        }
    }
}

/*! \brief Delete the account keys.
 */
void FastPair_DeleteAccountKeys(void)
{
    fastPair_DeleteAllAccountKeys();
    /* Notify LE advertising manager regarding the change in FP adverts */
    if(fastPair_AdvNotifyDataChange())
    {
        DEBUG_LOG("FastPair_DeleteAccountKeys. Notified LE advertising manager after the data change.");
    }
    else
    {
        DEBUG_LOG("FastPair_DeleteAccountKeys. Couldn't notify LE advertising manager after the change in FP adverts.");
    }
}

/*! \brief Get Fast Pair Model ID from
     USR PS Key 5 and 6 respectively.
*/
static void fastPair_GetModelIdAspk(void)
{
    DEBUG_LOG("fastPair_GetModelIdAspk");

    uint8 fp_config_model_id[FAST_PAIR_CONFIG_MODEL_ID_LEN];
    static uint8 fp_config_aspk[FAST_PAIR_CONFIG_ASPK_LEN];

    PsRetrieve(PS_KEY_FAST_PAIR_MODEL_ID, fp_config_model_id, FAST_PAIR_CONFIG_MODEL_ID_LEN);
    PsRetrieve(PS_KEY_FAST_PAIR_SCRAMBLED_ASPK, fp_config_aspk, FAST_PAIR_CONFIG_ASPK_LEN);

    fastPair_SetPrivateKey((const uint16*)fp_config_aspk, sizeof(fp_config_aspk));
    fastPair_SetModelId(fp_config_model_id);
}

/*! \brief Message Handler to handle CL messages coming from the application
*/
bool FastPair_HandleConnectionLibraryMessages(MessageId id, Message message, bool already_handled)
{
    switch(id)
    {
        case CL_SM_AUTHENTICATE_CFM:
            DEBUG_LOG("FastPair_HandleConnectionLibraryMessages. CL_SM_AUTHENTICATE_CFM");
            fastPair_AuthenticateCfm((CL_SM_AUTHENTICATE_CFM_T *)message);
        break;

        default:
        break;
    }
    return already_handled;
}

/*! \brief Message Handler

    This function is the main message handler for the fast pair module.
*/
void FastPair_HandleMessage(Task task, MessageId id, Message message)
{

    if((id >= GATT_FAST_PAIR_SERVER_MESSAGE_BASE) && (id < GATT_FAST_PAIR_SERVER_MESSAGE_TOP))
    {
        {
            fastPair_GattFPServerMsgHandler(task, id, message);
        }
        return;
    }    

    UNUSED(task);
    
    switch (id)
    {

        case CON_MANAGER_TP_CONNECT_IND:
            fastPair_ConManagerConnectInd((CON_MANAGER_TP_CONNECT_IND_T *)message);
        break;

        case CON_MANAGER_TP_DISCONNECT_IND:
            fastPair_ConManagerDisconnectInd((CON_MANAGER_TP_DISCONNECT_IND_T *)message);
        break;

        case CON_MANAGER_HANDSET_CONNECT_ALLOW_IND:
            fastPair_ConManagerHandsetConnectAllowInd();
        break;

        case CON_MANAGER_HANDSET_CONNECT_DISALLOW_IND:
            fastPair_ConManagerHandsetConnectDisallowInd();
        break;

        case CL_SM_BLE_READ_RANDOM_ADDRESS_CFM:
            fastPair_CacheRandomAddressCfm((const CL_SM_BLE_READ_RANDOM_ADDRESS_CFM_T *)message);
        break;

        case CL_CRYPTO_GENERATE_SHARED_SECRET_KEY_CFM:
            fastPair_SharedSecretCfm((CL_CRYPTO_GENERATE_SHARED_SECRET_KEY_CFM_T *)message);
        break;

        case CL_CRYPTO_HASH_CFM:
            fastPair_HashCfm((CL_CRYPTO_HASH_CFM_T *)message);
        break;

        case CL_CRYPTO_ENCRYPT_CFM:
            fastPair_EncryptCfm((CL_CRYPTO_ENCRYPT_CFM_T *)message);
        break;

        case CL_CRYPTO_DECRYPT_CFM:
            fastPair_DecryptCfm((CL_CRYPTO_DECRYPT_CFM_T *)message);
        break;

        case fast_pair_state_event_timer_expire:
            fastPair_TimerExpired();
        break;

        case LOCAL_ADDR_CONFIGURE_BLE_GENERATION_CFM:
        {
            LOCAL_ADDR_CONFIGURE_BLE_GENERATION_CFM_T *cfm = (LOCAL_ADDR_CONFIGURE_BLE_GENERATION_CFM_T *)message;
            DEBUG_LOG("FastPair_HandleMessage. LOCAL_ADDR_CONFIGURE_BLE_GENERATION_CFM status : %d",cfm->status);
        }
        break;

        case PAIRING_ACTIVITY:
            fastPair_PairingActivity((PAIRING_ACTIVITY_T*)message);
        break;
        

        case PAIRING_STOP_CFM:
            DEBUG_LOG("FastPair_HandleMessage, PAIRING_STOP_CFM");
            if(fastPair_IsProviderPairingRequested())
            {
                fastPair_ProviderInitiatePairing();
            }
        break;

        case PHY_STATE_CHANGED_IND:
        {
            uint16* ring_stop = PanicUnlessMalloc(sizeof(uint16));
            
            *ring_stop = FP_STOP_RING_CURRENT;
            PHY_STATE_CHANGED_IND_T* msg = (PHY_STATE_CHANGED_IND_T *)message;
            if(msg->new_state == PHY_STATE_IN_CASE)
            {
                MessageSend(fpRingDevice_GetTask(), fast_pair_ring_stop_event, ring_stop);
                fastPair_PowerOff();
            }
            else if(msg->new_state == PHY_STATE_IN_EAR)
            {
                MessageSend(fpRingDevice_GetTask(), fast_pair_ring_stop_event, ring_stop);
            }
            else
            {
                /* Free the message as it won't be sent as PHY state is neither In case nor In ear */ 
                free(ring_stop);
            }
        }
        break;

        case SYSTEM_STATE_STATE_CHANGE:
            fastPair_HandleSystemStateChange((SYSTEM_STATE_STATE_CHANGE_T *)message);
        break;

#ifdef INCLUDE_CASE_COMMS
        case CASE_LID_STATE:
            fastPair_BatteryHandleCaseLidState((const CASE_LID_STATE_T *)message);
        break;
        case CASE_POWER_STATE:
            fastPair_BatteryHandleCasePowerState((const CASE_POWER_STATE_T *)message);
        break;
#endif
#ifdef INCLUDE_TWS
        case STATE_PROXY_EVENT:
            fastPair_HandleStateProxyEvent((const STATE_PROXY_EVENT_T*)message);
        break;
#endif
        default:
            DEBUG_LOG("Unhandled MessageID = MESSAGE:fast_pair_state_event_id:0x%d", id);
        break;
    }
}


/******************************************************************************/

/*! \brief Set Fast Pair FSM state */
bool fastPair_StateMachineHandleEvent(fast_pair_state_event_t event)
{
    bool ret_val = FALSE;
    fastPairTaskData *theFastPair;

    theFastPair = fastPair_GetTaskData();

#ifdef FAST_PAIR_TIME_PROFILER
    fastpair_TimeProfiler(event.id);
#endif
    switch(fastPair_GetState(theFastPair))
    {
        case FAST_PAIR_STATE_NULL:
            ret_val = fastPair_StateNullHandleEvent(event);
        break;
        
        case FAST_PAIR_STATE_IDLE:
            ret_val = fastPair_StateIdleHandleEvent(event);
        break;
        
        case FAST_PAIR_STATE_WAIT_AES_KEY:
            ret_val = fastPair_StateWaitAESKeyHandleEvent(event);
        break;

        case FAST_PAIR_STATE_WAIT_PAIRING_REQUEST:
            ret_val = fastPair_StateWaitPairingRequestHandleEvent(event);
        break;

        case FAST_PAIR_STATE_WAIT_PASSKEY:
            ret_val = fastPair_StateWaitPasskeyHandleEvent(event);
        break;
   
        case FAST_PAIR_STATE_WAIT_ACCOUNT_KEY:
            ret_val = fastPair_StateWaitAccountKeyHandleEvent(event);
        break;

        case FAST_PAIR_STATE_WAIT_ADDITIONAL_DATA:
            ret_val = fastPair_StateWaitAdditionalDataHandleEvent(event);
        break;
        case FAST_PAIR_STATE_PNAME:
            ret_val = fastPair_StatePNameHandleEvent(event);
        break;


        default:
            DEBUG_LOG("Unhandled event\n");
        break;
    }
    return ret_val;
}
/*! \brief Set Fast Pair State
    Called to change state.  Handles calling the state entry and exit
    functions for the new and old states.
*/
void fastPair_SetState(fastPairTaskData *theFastPair, fastPairState state)
{
    DEBUG_LOG("fastPair_SetState(%d)", state);

    theFastPair->prev_state = theFastPair->state;

    /* Set new state */
    theFastPair->state = state;

    /* Handle state entry functions */
    switch (state)
    {
        case FAST_PAIR_STATE_IDLE:
            fastPair_EnterIdle(theFastPair);
            break;

        case FAST_PAIR_STATE_WAIT_AES_KEY:
            fastPair_EnterWaitAESKey();
            break;

        case FAST_PAIR_STATE_WAIT_PAIRING_REQUEST:
            fastPair_EnterWaitPairingRequest();
            break;

        case FAST_PAIR_STATE_WAIT_PASSKEY:
            fastPair_EnterWaitPasskey();
            break;

        case FAST_PAIR_STATE_WAIT_ACCOUNT_KEY:
            fastPair_EnterWaitAccountKey();
            break;

        case FAST_PAIR_STATE_WAIT_ADDITIONAL_DATA:
            fastPair_EnterWaitAdditionalData();
            break;

        case FAST_PAIR_STATE_PNAME:
            fastPair_EnterPName();
            break;

        default:
            break;
    }
}


/*! \brief Get Fast Pair FSM state

Returns current state of the Fast Pair FSM.
*/
fastPairState fastPair_GetState(fastPairTaskData *theFastPair)
{
    return theFastPair->state;
}


/*! Get pointer to Fast Pair data structure */
fastPairTaskData* fastPair_GetTaskData(void)
{
    return (&fast_pair_task_data);
}

void fastPair_StartTimer(bool isQuarantine)
{
    uint16 timeout_s = 0;
    fastPairTaskData *theFastPair;

    theFastPair = fastPair_GetTaskData();
    
    timeout_s = (isQuarantine==TRUE)?FAST_PAIR_QUARANTINE_TIMEOUT:FAST_PAIR_STATE_TIMEOUT;

    DEBUG_LOG("fastPair_StartTimer timeout=[%u s]\n", timeout_s);

    /* Make sure any pending messages are cancelled */
    MessageCancelAll(&theFastPair->task, fast_pair_state_event_timer_expire);

    /* Start Fast Pair timer */
    MessageSendLater(&theFastPair->task, fast_pair_state_event_timer_expire, 0, D_SEC(timeout_s));
}

void fastPair_StopTimer(void)
{
    fastPairTaskData *theFastPair;

    theFastPair = fastPair_GetTaskData();

    DEBUG_LOG("fastPair_StopTimer \n");

    /* Make sure any pending messages are cancelled */
    MessageCancelAll(&theFastPair->task, fast_pair_state_event_timer_expire);
}

void fastPair_SetRetroactivelyWritingAccountKeyFlag(bool flag_value)
{
    fastPairTaskData *theFastPair;
    theFastPair = fastPair_GetTaskData();

    DEBUG_LOG("fastPair_SetRetroactivelyWritingAccountKeyFlag. Flag: %d", flag_value);
    theFastPair->retroactively_writing_account_key = flag_value;
}

void fastPair_StartTimerForWritingAccountKeyRetroactively(void)
{
    uint16 timeout_s = 0;
    fastPairTaskData *theFastPair;

    theFastPair = fastPair_GetTaskData();
    timeout_s = FAST_PAIR_RETROACTIVELY_WRITING_ACCOUNT_KEY_TIMEOUT;

    DEBUG_LOG("fastPair_StartTimerForWritingAccountKeyRetroactively. Timeout=[%u s]\n",timeout_s);
    /* Start the timer */
    MessageSendLater(&theFastPair->task, fast_pair_state_event_timer_expire, 0, D_SEC(timeout_s));
}

bool FastPair_Init(Task init_task)
{
    bool status = FALSE;
    
    fastPairTaskData *theFastPair = fastPair_GetTaskData();

    UNUSED(init_task);

    DEBUG_LOG("FastPair_Init");

    memset(theFastPair, 0, sizeof(*theFastPair));

    /* Set up task handler */
    theFastPair->task.handler = FastPair_HandleMessage;

    /* Initialise state */
    theFastPair->state = FAST_PAIR_STATE_NULL;

    fastPair_SetState(theFastPair, FAST_PAIR_STATE_NULL);

    /* Initialise retroactively writing account key flag to FALSE */
    fastPair_SetRetroactivelyWritingAccountKeyFlag(FALSE);

    /* Register with Connection Manager as observer to know BLE connections are made/destroyed */
    ConManagerRegisterTpConnectionsObserver(cm_transport_ble, &theFastPair->task);

    /* Register with Connection Manager as observer to know if handset connections are allowed or not */
    ConManagerRegisterAllowedConnectionsObserver(&theFastPair->task);

    /* Register with Physical state as observer to know if there are any physical state changes */
    appPhyStateRegisterClient(&theFastPair->task);

    /* Register for system state change indications */
    SystemState_RegisterForStateChanges(&theFastPair->task);

    /* Init the GATT Fast Pair Server library */
    status = fastPair_GattFPServerInitialize(&theFastPair->task);

#ifdef INCLUDE_CASE_COMMS
    /* register for case lid and battery notifications */
    CcWithCase_RegisterStateClient(&theFastPair->task);
#endif

    /* Initialize the Fast Pair Pairing Interface */
    fastPair_PairingInit();


#ifdef INCLUDE_TWS
    /* Register for state proxy battery voltage (local & remote) events */
    StateProxy_EventRegisterClient(&theFastPair->task, state_proxy_event_type_battery_voltage);
#endif

    /* Get Model ID and ASPK from PS */
    fastPair_GetModelIdAspk();

    /* Initialize the Fast Pair Advertising Interface */
    fastPair_SetUpAdvertising();

    fastPair_InitSessionData();

    /* Initialize the Fast Pair Account Key Sync Interface */
    fastPair_AccountKeySync_Init();

    /* Initialize the Fast Pair Personalized Name Sync Interface */
    fastPair_PNameSync_Init();

    /* Configure Resolvable Private Address (RPA) - conditional on whether MTP VA tests are running */
    LocalAddr_ConfigureBleGeneration(&theFastPair->task, local_addr_host_gen_resolvable, local_addr_controller_gen_none);

    /* Initialize message stream */
    fastPair_MsgStreamInit();

    /* Ready to start Fast Pair. Move to Idle state */
    theFastPair->state = FAST_PAIR_STATE_IDLE;

    return status;
}


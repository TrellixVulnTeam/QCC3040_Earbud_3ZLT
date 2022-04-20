/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       fast_pair_pairing_if.c
\brief     Fast Pair Pairing interface file.
*/
#include "fast_pair_pairing_if.h"
#include "fast_pair.h"
#include "fast_pair_advertising.h"
#include "bredr_scan_manager.h"
#include "fast_pair_events.h"
#include "fast_pair_wait_aes_key_state.h"
#include <panic.h>
#include <connection.h>
#include <stdio.h>
#include <logging.h>

fast_pair_if_t fast_pair_if_data;

/*! \brief Handle Pairing user confirmation.
 */
static pairing_user_confirmation_rsp_t fastpair_handle_user_confirmation_req(const CL_SM_USER_CONFIRMATION_REQ_IND_T* ind)
{
    uint32_t numeric_value = ind->numeric_value;
    pairing_user_confirmation_rsp_t rsp = pairing_user_confirmation_reject;
    bool isPasskeySame = FALSE;
    fast_pair_if_t *fp_if_data;
    fp_if_data = fastPair_GetIFData();

    /*if replay, send accept/reject to Pairing Manager and call State Manager API that these match*/
    if(fp_if_data->seeker_passkey)
    {
        if(fp_if_data->seeker_passkey == numeric_value)
        {
            rsp = pairing_user_confirmation_accept;
            isPasskeySame = TRUE;
        }
        fp_if_data->seeker_passkey = 0;/*reset the stored passkey*/

        fastPair_ProviderPasskey(isPasskeySame, numeric_value);
    }
    else
    {
        /*seeker value not yet received over fast pair process, wait!*/
        rsp = pairing_user_confirmation_wait;
    }

    DEBUG_LOG("fastpair_handle_user_confirmation_req - response %d", rsp);

    return rsp;
}

/*! \brief Handle information on IO capability of remote device.
 */
static void fastpair_handle_remote_io_capability(const CL_SM_REMOTE_IO_CAPABILITY_IND_T *ind)
{
    fast_pair_if_t *fp_if_data;
    fp_if_data = fastPair_GetIFData();

    DEBUG_LOG("fastpair_handle_remote_io_capability");

    if(ind->io_capability == cl_sm_io_cap_no_input_no_output)
    {
         /*to be rejected when io_cap_req_ind is received*/
         fp_if_data->isAccept = FALSE;
    }
    else
    {
        fp_if_data->isAccept = TRUE;
    }
}

/*! \brief Provide information on IO capability of local device.
 */
static pairing_io_capability_rsp_t fastpair_handle_io_capability_req(const CL_SM_IO_CAPABILITY_REQ_IND_T* ind)
{
    pairing_io_capability_rsp_t rsp;
    fast_pair_if_t *fp_if_data;
    fastPairTaskData *theFastPair;
    
    theFastPair = fastPair_GetTaskData();
    fp_if_data = fastPair_GetIFData();

    DEBUG_LOG("fastpair_handle_io_capability_req");
    UNUSED(ind);

	
    /* Check if we are performing initial pairing or subsequent pairing by checking if public key is available */
    if(theFastPair->session_data.public_key != NULL)
    {
        /* Accept pairing request for initial pairing only in Br/Edr discoverable mode */
        if(fastPair_AdvIsBrEdrDiscoverable() == FALSE)
        {
            /* If we are not in discoverable Mode, then set the bonding bit to False*/
            fp_if_data->isAccept = FALSE;
        }
    }

    if(theFastPair->provider_pairing_requested == TRUE)
    {
         /*Always accept pairing request if we are initiating pairing*/
         fp_if_data->isAccept = TRUE;
    }

    rsp.io_capability = cl_sm_io_cap_display_yes_no;
    rsp.mitm =  mitm_required ;
    rsp.bonding = fp_if_data->isAccept;
    rsp.key_distribution = KEY_DIST_RESPONDER_ENC_CENTRAL | KEY_DIST_RESPONDER_ID | KEY_DIST_INITIATOR_ENC_CENTRAL | KEY_DIST_INITIATOR_ID;
    rsp.oob_data = oob_data_none;
    rsp.oob_hash_c = NULL;
    rsp.oob_rand_r = NULL;

    /*Intimate state manager that pairing has started*/
    fastPair_ReceivedPairingRequest(fp_if_data->isAccept);

    return rsp;
}

/*! \brief Retrieve FastPair pairing interface data.
 */
fast_pair_if_t * fastPair_GetIFData(void)
{
    return &fast_pair_if_data;
}

/*! \brief FastPair pairing interface initialisation.
 */
void fastPair_PairingInit(void)
{
    /*Initialize FP Pairing Interface*/
    fast_pair_if_t * fp_if_data;    
    fastPairTaskData *theFastPair;
    
    theFastPair = fastPair_GetTaskData();

    fp_if_data = fastPair_GetIFData();

    DEBUG_LOG("fastPair_PairingInit");

    fp_if_data->isAccept = FALSE;
    fp_if_data->seeker_passkey = 0;

    /* Register with pairing module to know when device is Br/Edr discoverable */
    Pairing_ActivityClientRegister(&(theFastPair->task));
}

/*! \brief Fast pair process has started. State manager can call this.
 */
void fastPair_StartPairing(void)
{
    pairing_plugin_t fastPair_exchange;

    fastPair_exchange.handle_io_capability_req = fastpair_handle_io_capability_req;
    fastPair_exchange.handle_remote_io_capability = fastpair_handle_remote_io_capability;
    fastPair_exchange.handle_user_confirmation_req = fastpair_handle_user_confirmation_req;

    DEBUG_LOG("fastPair_StartPairing");
    /*FP has started .Intimate Pairing Manager*/
    Pairing_PluginRegister(fastPair_exchange);
}

/*! \brief Initiate Pairing to seeker.
 */
void fastPair_InitiatePairing(const bdaddr *bd_addr)
{
    fastPairTaskData *theFastPair;
    theFastPair = fastPair_GetTaskData();
    DEBUG_LOG("fastPair_InitiatePairing");
    /*FP has started .Start needed process with Pairing Manager*/
    if(bd_addr)
    {
        Pairing_PairAddress(&theFastPair->task,(bdaddr *)bd_addr);
    }
}

/*! \brief Initiate Pairing with seeker_addr.
 */
void fastPair_InitiateFastPairing(void)
{
    fastPairTaskData *theFastPair;
    theFastPair = fastPair_GetTaskData();
    bdaddr *bd_addr = &theFastPair->seeker_addr;

    DEBUG_LOG("fastPair_InitiateFastPairing: Provider Initiates Pairing");
    DEBUG_LOG("pairing_bd addr %04x:%02x:%06x", bd_addr->nap, bd_addr->uap, bd_addr->lap);
    /*FP has started .Start needed process with Pairing Manager*/
    if(bd_addr)
    {
        Pairing_PairAddress(&theFastPair->task,(bdaddr *)bd_addr);
        fastPair_StartPairing();
    }
    /* Restart the fast pair timer which was stopped before pairing stop */
    fastPair_StartTimer(FALSE);
}

/*! \brief Seeker passkey is received. State manager can call this.
 */
void fastPair_PairingPasskeyReceived(uint32 passkey)
{
    fast_pair_if_t *fp_if_data;
    fp_if_data = fastPair_GetIFData();

   /*FP has Passkey received. Compare with generated passkey*/
   DEBUG_LOG("fastPair_PairingPasskeyReceived - response %d", passkey);
   fp_if_data->seeker_passkey = passkey;
   Pairing_PluginRetryUserConfirmation();
}

/*! \brief  To reset the pairing interface. On a fast pair process completion(success/failure/timeout), state manager can call this.
 */
void fastPair_PairingReset(void)
{
    fast_pair_if_t *fp_if_data;

    fp_if_data = fastPair_GetIFData();
    DEBUG_LOG("fastPair_PairingReset");

    fp_if_data->seeker_passkey = 0;/*reset the stored passkey*/
    fp_if_data->isAccept = FALSE;

    if(Pairing_PluginIsRegistered())
    {
        pairing_plugin_t fastPair_exchange;


        fastPair_exchange.handle_io_capability_req = fastpair_handle_io_capability_req;
        fastPair_exchange.handle_remote_io_capability = fastpair_handle_remote_io_capability;
        fastPair_exchange.handle_user_confirmation_req = fastpair_handle_user_confirmation_req;
        /*Reset for other fastpair sub modules*/
        DEBUG_LOG("Unregister from pairing plugin");
        Pairing_PluginUnregister(fastPair_exchange);
    }
}

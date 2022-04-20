/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_connect_state.c
\brief  Implementation of the connect state for Amazon AVS
*/

#include "ama_connect_state.h"

#ifdef INCLUDE_ACCESSORY
#include "ama_accessory.h"
#endif
#include "ama_audio.h"
#include "ama_ble.h"
#include "ama_config.h"
#include "ama_data.h"
#include "ama_rfcomm.h"
#include "ama_speech.h"
#include "ama_profile.h"

#include "bt_device.h"

static bdaddr ama_bd_addr;

const bdaddr * Ama_GetBtAddress(void)
{
    return &ama_bd_addr;
}

bool Ama_IsConnected(void)
{
#ifdef INCLUDE_ACCESSORY
    return AmaRfcomm_IsConnected() || AmaAccessory_IsIapConnected();
#else
    return AmaRfcomm_IsConnected();
#endif
}

bool Ama_IsRegistered(void)
{
    return VoiceUi_GetDeviceFlag(device_va_flag_ama_setup_done);
}

void Ama_CompleteSetup(void)
{
    DEBUG_LOG_FN_ENTRY("Ama_CompleteSetup");
    VoiceUi_SetDeviceFlag(device_va_flag_ama_setup_done, TRUE);
    AmaBle_UpdateAdvertising();
}

ama_transport_t Ama_GetActiveTransport(void)
{
    return AmaData_GetActiveTransport();
}

void Ama_TransportSwitched(ama_transport_t transport)
{
    DEBUG_LOG_FN_ENTRY("Ama_TransportSwitched enum:ama_transport_t:%d", transport);

    AmaData_SetActiveTransport(transport);

    switch(transport)
    {
        case ama_transport_rfcomm:
            DEBUG_LOG("Ama_SwitchedTransport ama_transport_rfcomm with (%u) codec (1->mSBC, 2->OPUS)", (ama_codec_t)AMA_DEFAULT_CODEC_OVER_RFCOMM);
            Ama_ConfigureCodec((ama_codec_t)AMA_DEFAULT_CODEC_OVER_RFCOMM);
            break;

        case ama_transport_iap:
            DEBUG_LOG("Ama_SwitchedTransport ama_transport_iap with (%u) codec (1->mSBC, 2->OPUS)", (ama_codec_t)AMA_DEFAULT_CODEC_OVER_IAP2);
            Ama_ConfigureCodec((ama_codec_t)AMA_DEFAULT_CODEC_OVER_IAP2);
            break;

        default:
            DEBUG_LOG("Ama_SwitchedTransport UNKNOWN transport");
            break;
    }
}

void Ama_TransportConnected(ama_transport_t transport, const bdaddr * bd_addr)
{
    DEBUG_LOG_FN_ENTRY("Ama_TransportConnected");

    if(AmaData_GetActiveTransport() == ama_transport_none)
    {
        DEBUG_LOG("Ama_TransportConnected enum:ama_transport_t:%d, [%lx, %x, %x]", transport, bd_addr->lap, bd_addr->uap, bd_addr->nap);

        Ama_TransportSwitched(transport);
        memcpy(&ama_bd_addr, bd_addr, sizeof(bdaddr));
        AmaProtocol_TransportConnCfm();
        AmaProfile_SendConnectedInd(&ama_bd_addr);
    }
    else
    {
        DEBUG_LOG_WARN("Ama_TransportConnected IGNORED enum:ama_transport_t:%d, [%lx, %x, %x]", transport, bd_addr->lap, bd_addr->uap, bd_addr->nap);
    }
}

void Ama_TransportDisconnected(ama_transport_t transport_to_disconnect)
{
    DEBUG_LOG_FN_ENTRY("Ama_TransportDisconnected");

    ama_transport_t active_transport = AmaData_GetActiveTransport();

    if(active_transport == transport_to_disconnect)
    {
        Ama_TransportSwitched(ama_transport_none);
        AmaProtocol_ResetParser();
        AmaData_SetState(ama_state_initialized);
        AmaSpeech_SetToDefault();
        AmaAudio_Stop();

        if(VoiceUi_IsWakeUpWordFeatureIncluded())
        {
            AmaAudio_StopWakeWordDetection();
        }
        if (active_transport != ama_transport_none)
        {
            if(!BdaddrIsZero(&ama_bd_addr))
            {
                DEBUG_LOG("Ama_TransportDisconnected enum:ama_transport_t:%d, [%lx, %x, %x]", active_transport, ama_bd_addr.lap, ama_bd_addr.uap, ama_bd_addr.nap);
                AmaProfile_SendDisconnectedInd(&ama_bd_addr);
                BdaddrSetZero(&ama_bd_addr);

#ifdef INCLUDE_ACCESSORY
                if(transport_to_disconnect != ama_transport_iap)
                {
                    bdaddr iap_bd_addr;
                    if(AmaAccessory_GetBdaddrForActiveLink(&iap_bd_addr) && active_transport != ama_transport_iap)
                    {
                        /* There is an active iAP session running, connect it. */
                        Ama_TransportConnected(ama_transport_iap, &iap_bd_addr);
                    }
                }
#endif /* INCLUDE_ACCESSORY */
            }
            else
            {
                DEBUG_LOG_WARN("Ama_TransportDisconnected bdaddr is zero");
            }
        }
    }
}

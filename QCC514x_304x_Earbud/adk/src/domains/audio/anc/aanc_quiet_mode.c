/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       aanc_quiet_mode.c
\brief      Quiet mode implementation for Adaptive Active Noise Cancellation (ANC).
*/
#ifdef ENABLE_ADAPTIVE_ANC
#include "aanc_quiet_mode.h"
#include "anc_state_manager.h"
#include "anc_state_manager_private.h"
#include "kymera_adaptive_anc.h"
#include "system_clock.h"
#include "state_proxy.h"
#include <logging.h>

#include <sink.h>

#define QUIET_MODE_TIME_DELAY_MS  (200U)
#define QUIET_MODE_TIME_DELAY_US  (US_PER_MS * QUIET_MODE_TIME_DELAY_MS)
#define US_TO_MS(us) ((us) / US_PER_MS)

AANC_UPDATE_QUIETMODE_IND_T remote_aanc_data, local_aanc_data;
Sink wall_clock_sink;

#define GetLocalPeerAancQuietModeData()  (&local_aanc_data)
#define GetRemotePeerAancQuietModeData() (&remote_aanc_data)

static bool aancQuietMode_IsRemoteQuietModeDetected(void)
{
    AANC_UPDATE_QUIETMODE_IND_T *quiet_mode_data =GetRemotePeerAancQuietModeData();
    return quiet_mode_data->aanc_quiet_mode_detected;
}

static bool aancQuietMode_IsRemoteQuietModeEnabled(void)
{
    AANC_UPDATE_QUIETMODE_IND_T *quiet_mode_data = GetRemotePeerAancQuietModeData();
    return quiet_mode_data->aanc_quiet_mode_enabled;
}

static bool aancQuietMode_IsRemoteQuietModeEnableRequested(void)
{
    AANC_UPDATE_QUIETMODE_IND_T *quiet_mode_data = GetRemotePeerAancQuietModeData();
    return quiet_mode_data->aanc_quiet_mode_enable_requested;
}

static bool aancQuietMode_IsRemoteQuietModeDisableRequested(void)
{
    AANC_UPDATE_QUIETMODE_IND_T *quiet_mode_data = GetRemotePeerAancQuietModeData();
    return quiet_mode_data->aanc_quiet_mode_disable_requested;
}

static marshal_rtime_t aancQuietMode_GetRemoteTimestamp(void)
{
    AANC_UPDATE_QUIETMODE_IND_T *quiet_mode_data = GetRemotePeerAancQuietModeData();
    return quiet_mode_data->timestamp;
}

static void aancQuietMode_UpdateQuietModeDetected(bool status)
{
    AANC_UPDATE_QUIETMODE_IND_T *quiet_mode_data = GetLocalPeerAancQuietModeData();
    quiet_mode_data->aanc_quiet_mode_detected = status;
}

static void aancQuietMode_UpdateQuietModeEnabled(bool status)
{
    AANC_UPDATE_QUIETMODE_IND_T *quiet_mode_data = GetLocalPeerAancQuietModeData();
    quiet_mode_data->aanc_quiet_mode_enabled = status;
}

static void aancQuietMode_UpdateQuietModeEnableRequest(bool status)
{
    AANC_UPDATE_QUIETMODE_IND_T *quiet_mode_data = GetLocalPeerAancQuietModeData();
    quiet_mode_data->aanc_quiet_mode_enable_requested = status;
}

static void aancQuietMode_UpdateQuietModeDisableRequest(bool status)
{
    AANC_UPDATE_QUIETMODE_IND_T *quiet_mode_data = GetLocalPeerAancQuietModeData();
    quiet_mode_data->aanc_quiet_mode_disable_requested = status;
}

static void aancQuietMode_UpdateQuietModeTimestamp(marshal_rtime_t time_instance)
{
    AANC_UPDATE_QUIETMODE_IND_T *quiet_mode_data = GetLocalPeerAancQuietModeData();
    quiet_mode_data->timestamp = time_instance;
}

static void aancQuietMode_UpdateRemoteQuietModeDetected(bool status)
{
    AANC_UPDATE_QUIETMODE_IND_T *quiet_mode_data = GetRemotePeerAancQuietModeData();
    quiet_mode_data->aanc_quiet_mode_detected = status;
}

static void aancQuietMode_UpdateRemoteQuietModeEnabled(bool status)
{
    AANC_UPDATE_QUIETMODE_IND_T *quiet_mode_data = GetRemotePeerAancQuietModeData();
    quiet_mode_data->aanc_quiet_mode_enabled = status;
}

static void aancQuietMode_UpdateRemoteQuietModeEnableRequest(bool status)
{
    AANC_UPDATE_QUIETMODE_IND_T *quiet_mode_data = GetRemotePeerAancQuietModeData();
    quiet_mode_data->aanc_quiet_mode_enable_requested = status;
}

static void aancQuietMode_UpdateRemoteQuietModeDisableRequest(bool status)
{
    AANC_UPDATE_QUIETMODE_IND_T *quiet_mode_data = GetRemotePeerAancQuietModeData();
    quiet_mode_data->aanc_quiet_mode_disable_requested = status;
}

static void aancQuietMode_UpdateRemoteQuietModeTimestamp(marshal_rtime_t time_instance)
{
    AANC_UPDATE_QUIETMODE_IND_T *quiet_mode_data = GetRemotePeerAancQuietModeData();
    quiet_mode_data->timestamp = time_instance;
}

static bool aancQuietMode_IsQuietModeDetectedOnBothPeer(void)
{
    if(AancQuietMode_IsQuietModeDetected() && aancQuietMode_IsRemoteQuietModeDetected())
    {
        return TRUE;
    }
    return FALSE;
}

static Sink aancQuietMode_GetwallClockSink(void)
{
    return wall_clock_sink;
}

static marshal_rtime_t aancQuietMode_GetWallClock(marshal_rtime_t time)
{
    marshal_rtime_t wallclock=0;
    wallclock_state_t wc_state;

    if(SinkIsValid(aancQuietMode_GetwallClockSink()))
    {
        RtimeWallClockGetStateForSink(&wc_state, aancQuietMode_GetwallClockSink());

        if(RtimeLocalToWallClock(&wc_state, time, &wallclock))
        {
            DEBUG_LOG_VERBOSE("Wallclock returned with value of %u",wallclock);
        }
    }
    return wallclock;
}

static void aancQuietMode_UpdateQuietModeTransitionTime(void)
{
    /* get the current system time */
    marshal_rtime_t now = SystemClockGetTimerTime();
    marshal_rtime_t w_time;
    w_time=aancQuietMode_GetWallClock(now);

    marshal_rtime_t timestamp = rtime_add(w_time,QUIET_MODE_TIME_DELAY_US);
    aancQuietMode_UpdateQuietModeTimestamp(timestamp);
}

static bool aancQuietMode_IsLocalDeviceQuietModeInitiator(void)
{
    if(AancQuietMode_IsQuietModeEnableRequested() || aancQuietMode_IsRemoteQuietModeEnableRequested())
    {
        return FALSE;
    }
    else if(aancQuietMode_IsQuietModeDetectedOnBothPeer())
    {
        return TRUE;
    }

    return FALSE;
}

static bool aancQuietMode_IsLocalDeviceQuietModeDisableInitiator(void)
{
    if(AancQuietMode_IsQuietModeDisableRequested() || aancQuietMode_IsRemoteQuietModeDisableRequested())
    {
        return FALSE;
    }
    else if(!AancQuietMode_IsQuietModeDetected())
    {
        return TRUE;
    }
    return FALSE;
}

static bool aancQuietMode_CheckPendingRequest(void)
{
    if(AancQuietMode_IsQuietModeEnableRequested() || AancQuietMode_IsQuietModeDisableRequested())
    {
        return TRUE;
    }
    return FALSE;
}

static bool aancQuietMode_CheckPendingEnableRequest(void)
{
    if(AancQuietMode_IsQuietModeEnableRequested() || aancQuietMode_IsRemoteQuietModeEnableRequested())
    {
        return TRUE;
    }
    return FALSE;
}

static bool aancQuietMode_CheckPendingDisableRequest(void)
{
    if(AancQuietMode_IsQuietModeDisableRequested() || aancQuietMode_IsRemoteQuietModeDisableRequested())
    {
        return TRUE;
    }
    return FALSE;
}

static void aancQuietMode_MsgRegisteredClientsOnQuietModeUpdate(void)
{
    if(AncStateManager_GetClientTask())
    {
        MESSAGE_MAKE(ind, AANC_UPDATE_QUIETMODE_IND_T);
        ind->aanc_quiet_mode_detected = AancQuietMode_IsQuietModeDetected();
        ind->aanc_quiet_mode_enabled  = AancQuietMode_IsQuietModeEnabled();
        ind->aanc_quiet_mode_disable_requested = AancQuietMode_IsQuietModeDisableRequested();
        ind->aanc_quiet_mode_enable_requested  = AancQuietMode_IsQuietModeEnableRequested();
        ind->timestamp = AancQuietMode_GetTimestamp();
        TaskList_MessageSend(AncStateManager_GetClientTask(),AANC_UPDATE_QUIETMODE_IND,ind);
    }
}

static void aancQuietMode_HandleQuietModeEnableTx(void)
{
    DEBUG_LOG_FN_ENTRY("aancQuietMode_HandleQuietModeEnableTx");

    if(!AancQuietMode_IsQuietModeEnabled() && !aancQuietMode_CheckPendingRequest())
    {
        if(aancQuietMode_IsLocalDeviceQuietModeInitiator())
        {
            DEBUG_LOG_ALWAYS("Initiator of quiet mode enable");
            aancQuietMode_UpdateQuietModeTransitionTime();
            MessageSendLater(AncStateManager_GetTask(), anc_state_manager_event_aanc_quiet_mode_enable,
                             NULL,QUIET_MODE_TIME_DELAY_MS);
            aancQuietMode_UpdateQuietModeEnableRequest(TRUE);
            aancQuietMode_MsgRegisteredClientsOnQuietModeUpdate();
        }
        else if(AancQuietMode_IsQuietModeDetected())
        {
            aancQuietMode_MsgRegisteredClientsOnQuietModeUpdate();
        }
    }
}

static void aancQuietMode_HandleQuietModeDisableTx(void)
{
    DEBUG_LOG_FN_ENTRY("aancQuietMode_HandleQuietModeDisableTx");
    if(AancQuietMode_IsQuietModeEnabled() && !aancQuietMode_CheckPendingRequest())
    {
        if(aancQuietMode_IsLocalDeviceQuietModeDisableInitiator())
        {
            DEBUG_LOG_ALWAYS("Initiator of quiet mode disable");
            aancQuietMode_UpdateQuietModeTransitionTime();
            MessageSendLater(AncStateManager_GetTask(), anc_state_manager_event_aanc_quiet_mode_disable,
                             NULL,QUIET_MODE_TIME_DELAY_MS);
            aancQuietMode_UpdateQuietModeDisableRequest(TRUE);
            aancQuietMode_MsgRegisteredClientsOnQuietModeUpdate();
        }
    }
}


static bool aancQuietMode_CheckRemoteQuietModeRequest(void)
{
    if(aancQuietMode_IsRemoteQuietModeEnableRequested() || aancQuietMode_IsRemoteQuietModeDisableRequested())
    {
        return TRUE;
    }
    return FALSE;
}

static marshal_rtime_t aancQuietMode_GetQuietModeTimeDelta(void)
{
    marshal_rtime_t delta;
    rtime_t now = SystemClockGetTimerTime();
    rtime_t w_time;
    marshal_rtime_t timestamp = aancQuietMode_GetRemoteTimestamp();
    w_time=aancQuietMode_GetWallClock(now);

    delta = rtime_sub(timestamp, w_time);

    return delta;
}

static void aancQuietMode_UpdateRemoteQuietModeData(STATE_PROXY_AANC_DATA_T* new_aanc_data)
{
    aancQuietMode_UpdateRemoteQuietModeDetected(new_aanc_data->aanc_quiet_mode_detected);
    aancQuietMode_UpdateRemoteQuietModeEnabled(new_aanc_data->aanc_quiet_mode_enabled);
    aancQuietMode_UpdateRemoteQuietModeEnableRequest(new_aanc_data->aanc_quiet_mode_enable_requested);
    aancQuietMode_UpdateRemoteQuietModeDisableRequest(new_aanc_data->aanc_quiet_mode_disable_requested);
    aancQuietMode_UpdateRemoteQuietModeTimestamp(new_aanc_data->timestamp);

    DEBUG_LOG_VERBOSE("Remote data updated as %d %d %d %d %d",aancQuietMode_IsRemoteQuietModeDetected(),
                     aancQuietMode_IsRemoteQuietModeEnabled(),aancQuietMode_IsRemoteQuietModeEnableRequested(),
                     aancQuietMode_IsRemoteQuietModeDisableRequested(),aancQuietMode_GetRemoteTimestamp());
}

static void aancQuietMode_HandleQuietModeRxRequest(void)
{
    if(aancQuietMode_CheckRemoteQuietModeRequest())
    {
        anc_state_manager_event_id_t event;
        event = aancQuietMode_IsRemoteQuietModeEnableRequested()?anc_state_manager_event_aanc_quiet_mode_enable:
                                                       anc_state_manager_event_aanc_quiet_mode_disable;
        marshal_rtime_t delta_time=aancQuietMode_GetQuietModeTimeDelta();
        if(rtime_gt(delta_time,0))
        {
            MessageSendLater(AncStateManager_GetTask(), event,
                             NULL,US_TO_MS(delta_time));
        }
        else
        {
            MessageSend(AncStateManager_GetTask(),event,NULL);
        }
    }
}

static void aancQuietMode_HandleQuietModeCollision(void)
{
    DEBUG_LOG_FN_ENTRY("aancQuietMode_HandleQuietModeCollision");
    if(!aancQuietMode_IsRemoteQuietModeEnableRequested() || !aancQuietMode_IsRemoteQuietModeDisableRequested())
    {
        /* The remote device have detected quiet mode on peer device, but the remote device is
         * not the initiator of the quiet mode- in this situation check for local quiet mode status
         * and try to become the initiator of the quiet mode if quiet mode is detected locally as well.
         * This situation can very well happen if both the earbuds detects quiet mode within very
         * short period of time in between them.
        */
        aancQuietMode_HandleQuietModeEnableTx();
        aancQuietMode_HandleQuietModeDisableTx();
    }
}

bool AancQuietMode_IsQuietModeDetected(void)
{
    AANC_UPDATE_QUIETMODE_IND_T *quiet_mode_data =GetLocalPeerAancQuietModeData();
    return quiet_mode_data->aanc_quiet_mode_detected;
}

bool AancQuietMode_IsQuietModeEnabled(void)
{
    AANC_UPDATE_QUIETMODE_IND_T *quiet_mode_data = GetLocalPeerAancQuietModeData();
    return quiet_mode_data->aanc_quiet_mode_enabled;
}

bool AancQuietMode_IsQuietModeEnableRequested(void)
{
    AANC_UPDATE_QUIETMODE_IND_T *quiet_mode_data = GetLocalPeerAancQuietModeData();
    return quiet_mode_data->aanc_quiet_mode_enable_requested;
}

bool AancQuietMode_IsQuietModeDisableRequested(void)
{
    AANC_UPDATE_QUIETMODE_IND_T *quiet_mode_data = GetLocalPeerAancQuietModeData();
    return quiet_mode_data->aanc_quiet_mode_disable_requested;
}

marshal_rtime_t AancQuietMode_GetTimestamp(void)
{
    AANC_UPDATE_QUIETMODE_IND_T *quiet_mode_data = GetLocalPeerAancQuietModeData();
    return quiet_mode_data->timestamp;
}

void AancQuietMode_HandleQuietModeDetected(void)
{
    DEBUG_LOG_FN_ENTRY("AancQuietMode_HandleQuietModeDetected");
    aancQuietMode_UpdateQuietModeDetected(TRUE);
    aancQuietMode_HandleQuietModeEnableTx();
}

void AancQuietMode_HandleQuietModeCleared(void)
{
    DEBUG_LOG_FN_ENTRY("AancQuietMode_HandleQuietModeCleared");
    aancQuietMode_UpdateQuietModeDetected(FALSE);
    aancQuietMode_HandleQuietModeDisableTx();
}

void AancQuietMode_HandleQuietModeEnable(void)
{
    if(aancQuietMode_CheckPendingEnableRequest())
    {
         aancQuietMode_UpdateQuietModeEnableRequest(FALSE);
         aancQuietMode_UpdateRemoteQuietModeEnableRequest(FALSE);
         aancQuietMode_UpdateQuietModeEnabled(TRUE);
         aancQuietMode_UpdateQuietModeTimestamp(0);
         KymeraAdaptiveAnc_EnableQuietMode();
         TaskList_MessageSendId(AncStateManager_GetClientTask(),ANC_UPDATE_QUIETMODE_ON_IND);
    }
}

void AancQuietMode_HandleQuietModeDisable(void)
{
    if(aancQuietMode_CheckPendingDisableRequest())
    {
         KymeraAdaptiveAnc_DisableQuietMode();
         AancQuietMode_ResetQuietModeData();
         TaskList_MessageSendId(AncStateManager_GetClientTask(),ANC_UPDATE_QUIETMODE_OFF_IND);
    }
}

void AancQuietMode_HandleQuietModeRx(STATE_PROXY_AANC_DATA_T* new_aanc_data)
{
    aancQuietMode_UpdateRemoteQuietModeData(new_aanc_data);
    aancQuietMode_HandleQuietModeRxRequest();
    aancQuietMode_HandleQuietModeCollision();
}

void AancQuietMode_SetWallClock(Sink sink)
{
    wall_clock_sink = sink;
}

void AancQuietMode_ResetQuietModeData(void)
{
    aancQuietMode_UpdateQuietModeDetected(FALSE);
    aancQuietMode_UpdateQuietModeDisableRequest(FALSE);
    aancQuietMode_UpdateQuietModeEnableRequest(FALSE);
    aancQuietMode_UpdateQuietModeEnabled(FALSE);
    aancQuietMode_UpdateQuietModeTimestamp(0);
    aancQuietMode_UpdateRemoteQuietModeDetected(FALSE);
    aancQuietMode_UpdateRemoteQuietModeDisableRequest(FALSE);
    aancQuietMode_UpdateRemoteQuietModeEnableRequest(FALSE);
    aancQuietMode_UpdateRemoteQuietModeEnabled(FALSE);
    aancQuietMode_UpdateRemoteQuietModeTimestamp(0);
}


#endif /*ENABLE_ADAPTIVE_ANC */

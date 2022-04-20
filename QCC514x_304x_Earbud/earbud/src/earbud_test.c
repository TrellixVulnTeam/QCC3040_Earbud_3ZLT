/*!
\copyright  Copyright (c) 2017 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of specifc application testing functions
*/
#ifndef DISABLE_TEST_API

#include "earbud_test.h"

#include "system_state.h"
#include "adk_log.h"
#include "earbud_config.h"
#include "pairing.h"
#include "power_manager.h"
#include "earbud_sm.h"
#include "earbud_sm_private.h"
#include "battery_monitor_config.h"
#include "charger_monitor_config.h"
#include "kymera_config.h"
#include "temperature_config.h"
#include "peripherals/thermistor.h"
#include "ui.h"
#include "ui_user_config.h"
#include "peer_link_keys.h"
#include "microphones.h"
#include "volume_messages.h"
#include "device_db_serialiser.h"
#include "audio_sources.h"
#include "voice_sources.h"
#include "gaia_framework_feature.h"
#include "gaia_features.h"
#include "anc_gaia_plugin.h"
#include "system_clock.h"
#include "earbud_tones.h"
#include "earbud_tones_config_table.h"
#include "temperature.h"
#include "state_of_charge.h"

#include "anc_state_manager.h"
#include "aanc_quiet_mode.h"

#include "kymera_adaptive_anc.h"
#include "kymera_latency_manager.h"
#include "kymera_dynamic_latency.h"
#include "kymera_data.h"
#include "gaming_mode.h"

#ifdef ENABLE_EARBUD_FIT_TEST
#include "fit_test.h"
#endif

#include "audio_plugin_if.h"
#include "audio_clock.h"
#include "audio_power.h"
#include "volume_utils.h"
#ifdef INCLUDE_FAST_PAIR
#include "fast_pair_config.h"
#include <ps_key_map.h>
#endif

#include "earbud_buttons.h"
#include "upgrade.h"

#include <device_properties.h>
#include <device_list.h>
#include <device.h>
#include <dfu.h>
#include <hfp_profile.h>
#include <hfp_profile_instance.h>
#include <handover_profile.h>
#include <battery_monitor.h>
#include "battery_region.h"
#include <av.h>
#include <av_config.h>
#include <logical_input_switch.h>
#include <profile_manager.h>
#include <peer_find_role_private.h>
#include <peer_pair_le.h>
#include <peer_signalling.h>
#include <handset_service.h>
#include <connection_manager.h>
#include <connection.h>
#include <mirror_profile.h>
#include <volume_service.h>
#include <focus_audio_source.h>
#include <focus_voice_source.h>

#include <kymera.h>
#include <bredr_scan_manager.h>
#include <device_test_service_test.h>
#include <device_test_service.h>
#include <bredr_scan_manager.h>
#include <tws_topology_private.h>
#include <hdma_utils.h>

#include <charger_monitor.h>
#include <kymera.h>
#include <earbud_tones.h>
#include <system_reboot.h>

#include <cryptovm.h>
#include <ps.h>
#include <boot.h>
#include <feature.h>
#include <panic.h>
#include <stdio.h>
#include <ctype.h>
#include <bdaddr.h>
#include <util.h>

#ifdef INCLUDE_ACCESSORY
#include "accessory.h"
#include "request_app_launch.h"
#endif

#include <cc_with_case.h>

#ifdef GC_SECTIONS
/* Move all functions in KEEP_PM section to ensure they are not removed during
 * garbage collection */
#pragma unitcodesection KEEP_PM
#endif

typedef enum
{
    WAIT_FOR_IDLE_THEN_BRING_OUT_OF_CASE,
} delayed_test_event_t;

static void delayedInternalTestEventHandler(Task task, MessageId id, Message msg);
static const TaskData delayedInternalTestEventHandlerTask = { delayedInternalTestEventHandler };

static void testTaskHandler(Task task, MessageId id, Message message);
TaskData testTask = {testTaskHandler};

static struct
{
    uint16 testcase;
    uint16 iteration;
    uint16 last_marker;
} test_support = {0};

static void delayedInternalTestEventHandler(Task task, MessageId id, Message msg)
{
    UNUSED(task);
    UNUSED(msg);
    switch(id)
    {
        case WAIT_FOR_IDLE_THEN_BRING_OUT_OF_CASE:
            if(!appTestIsTopologyIdle())
            {
                MessageSendLater((Task) &delayedInternalTestEventHandlerTask, WAIT_FOR_IDLE_THEN_BRING_OUT_OF_CASE,
                                                     NULL, 100);
            }
            else
            {
                appTestPhyStateOutOfCaseEvent();
            }
            break;
        default:
            break;
    }
}

/*! \brief Handle ACL Mode Changed Event

    This message is received when the link mode of an ACL has changed from sniff
    to active or vice-versa.  Currently the application ignores this message.
*/
static void appTestHandleClDmModeChangeEvent(CL_DM_MODE_CHANGE_EVENT_T *evt)
{
    DEBUG_LOG_VERBOSE("appHandleClDmModeChangeEvent, event %d, %x,%x,%lx", evt->mode, evt->bd_addr.nap, evt->bd_addr.uap, evt->bd_addr.lap);
}

bool appTestHandleConnectionLibraryMessages(MessageId id, Message message,
                                            bool already_handled)
{
    switch(id)
    {
        case CL_DM_MODE_CHANGE_EVENT:
            appTestHandleClDmModeChangeEvent((CL_DM_MODE_CHANGE_EVENT_T *)message);
            return TRUE;
    }

    return already_handled;
}

/*! \brief Returns the current battery voltage
 */
uint16 appTestGetBatteryVoltage(void)
{
    uint16 voltage = appBatteryGetVoltageInstantaneous();
    DEBUG_LOG_ALWAYS("appTestGetBatteryVoltage, %u", voltage);
    return voltage;
}

void appTestSetBatteryVoltage(uint16 new_level)
{
    DEBUG_LOG_ALWAYS("appTestSetBatteryVoltage, %u", new_level);
    appBatteryTestSetFakeVoltage(new_level);
}

void appTestUnsetBatteryVoltage(void)
{
    DEBUG_LOG_ALWAYS("appTestUnsetBatteryVoltage");
    appBatteryTestUnsetFakeVoltage();
}

void appTestInjectBatteryTestVoltage(uint16 new_level)
{
    DEBUG_LOG_ALWAYS("appTestInjectBatteryTestVoltage, %u", new_level);
    appBatteryTestInjectFakeLevel(new_level);
}

void appTestRestartBatteryVoltageMeasurements(void)
{
    DEBUG_LOG_ALWAYS("appTestRestartBatteryVoltageMeasurements");
    appBatteryTestResumeAdcMeasurements();
}

uint8 appTestConvertBatteryVoltageToPercentage(uint16 voltage_mv)
{
    uint8 percent = Soc_ConvertLevelToPercentage(voltage_mv);
    DEBUG_LOG_ALWAYS("appTestConvertBatteryVoltageToPercentage %d mV -> %d%", voltage_mv, percent);

    return percent;
}

bool appTestBatteryStateIsOk(void)
{
    return (battery_region_ok == BatteryRegion_GetState());
}

bool appTestBatteryStateIsCritical(void)
{
    return (battery_region_critical == BatteryRegion_GetState());
}

bool appTestBatteryStateIsUnsafe(void)
{
    return (battery_region_unsafe == BatteryRegion_GetState());
}

uint32 appTestBatteryFilterResponseTimeMs(void)
{
    return appConfigBatteryReadingPeriodMs();
}

#ifdef HAVE_THERMISTOR
uint16 appTestThermistorDegreesCelsiusToMillivolts(int8 temperature)
{
    return appThermistorDegreesCelsiusToMillivolts(temperature);
}

void appTestSetBatteryTemperature(int8 new_temp)
{
    DEBUG_LOG_ALWAYS("appTestSetBatteryTemperature, %u", new_temp);
    appTemperatureSetFakeValue(new_temp);
}

void appTestUnsetBatteryTemperature(void)
{
    DEBUG_LOG_ALWAYS("appTestUnsetBatteryTemperature");
    appTemperatureUnsetFakeValue();
}

void appTestInjectBatteryTestTemperature(int8 new_temp)
{
    DEBUG_LOG_ALWAYS("appTestInjectBatteryTestTemperature, %d", new_temp);
    appTemperatureTestInjectFakeLevel(new_temp);
}

void appTestRestartTemperatureMeasurements(void)
{
    DEBUG_LOG_ALWAYS("appTestRestartTemperatureMeasurements");
    appTemperatureResumeAdcMeasurements();
}

int8 appTestGetBatteryTemperature(void)
{
    int8 temp = appTemperatureGetInstantaneous();
    DEBUG_LOG_ALWAYS("appTestGetBatteryTemperature, %d", temp);
    return temp;
}

#endif

/*! \brief Put Earbud into Handset Pairing mode
*/
void appTestPairHandset(void)
{
    DEBUG_LOG_ALWAYS("appTestPairHandset");
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_sm_pair_handset);
}

/*! \brief Delete all Handset pairing
*/
void appTestDeleteHandset(void)
{
    DEBUG_LOG_ALWAYS("appTestDeleteHandset");
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_sm_delete_handsets);
}

/*! \brief Delete Earbud peer pairing
*/
bool appTestDeletePeer(void)
{
    bdaddr bd_addr;

    DEBUG_LOG_ALWAYS("appTestDeletePeer");
    /* Check if we have previously paired with an earbud */
    if (appDeviceGetPeerBdAddr(&bd_addr))
    {
        return appDeviceDelete(&bd_addr);
    }
    else
    {
        DEBUG_LOG_WARN("appTestDeletePeer: NO PEER TO DELETE");
        return FALSE;
    }
}

bool appTestGetPeerAddr(bdaddr *peer_address)
{
    DEBUG_LOG_ALWAYS("appTestGetPeerAddr");
    if (appDeviceGetPeerBdAddr(peer_address))
        return TRUE;
    else
        return FALSE;
}


static bool appTestReadLocalAddr_completed = FALSE;
void appTestReadLocalAddr(void)
{
    DEBUG_LOG_ALWAYS("appTestReadLocalAddr started");

    appTestReadLocalAddr_completed = FALSE;
    ConnectionReadLocalAddr(&testTask);
}


static bdaddr appTestGetLocalAddr_address = {0};
bool appTestGetLocalAddr(bdaddr *addr)
{
    DEBUG_LOG_ALWAYS("appTestGetLocalAddr. Read:%d %04x:%02x:%06x",
                     appTestReadLocalAddr_completed,
                     appTestGetLocalAddr_address.nap, appTestGetLocalAddr_address.uap, appTestGetLocalAddr_address.lap);

    *addr = appTestGetLocalAddr_address;
    return appTestReadLocalAddr_completed;
}


/*! \brief Return if Earbud is in a Pairing mode
*/
bool appTestIsPairingInProgress(void)
{
    bool pip = !appPairingIsIdle();

    DEBUG_LOG_ALWAYS("appTestIsPairingInProgress:%d", pip);

    return pip;
}

/*! \brief Initiate Earbud A2DP connection to the Handset
*/
bool appTestHandsetA2dpConnect(void)
{
    bdaddr bd_addr;

    DEBUG_LOG_ALWAYS("appTestHandsetA2dpConnect");

    if (appDeviceGetHandsetBdAddr(&bd_addr))
    {
        return appAvA2dpConnectRequest(&bd_addr, appTestIsPtsMode() ? A2DP_CONNECT_MEDIA : A2DP_CONNECT_NOFLAGS);
    }
    else
    {
        return FALSE;
    }
}

/*! Used to block the rules for handset pairing */
bool appTestHandsetPairingBlocked = FALSE;

/*! \brief Stop the earbud pairing with a handset
 */
void appTestBlockAutoHandsetPairing(bool block)
{
    bool was_blocked = appTestHandsetPairingBlocked;
    bool paired_already = appTestIsHandsetPaired();

    appTestHandsetPairingBlocked = block;

    DEBUG_LOG_ALWAYS("appTestBlockAutoHandsetPairing(%d) was %d (paired:%d)",
                    block, was_blocked, paired_already);
}

/*! \brief Return if Earbud has a handset pairing
 */
bool appTestIsHandsetPaired(void)
{
    bool paired = BtDevice_IsPairedWithHandset();

    DEBUG_LOG_ALWAYS("appTestIsHandsetPaired:%d",paired);

    return paired;
}

/*! \brief Return if Earbud has an Handset A2DP connection
*/
bool appTestIsHandsetA2dpConnected(void)
{
    bool connected = FALSE;
    bdaddr bd_addr;

    if (appDeviceGetHandsetBdAddr(&bd_addr))
    {
        /* Find handset AV instance */
        avInstanceTaskData *theInst = appAvInstanceFindFromBdAddr(&bd_addr);
        connected = theInst && appA2dpIsConnected(theInst);
    }

    DEBUG_LOG_ALWAYS("appTestIsHandsetA2dpConnected:%d", connected);

    /* If we get here then there's no A2DP connected for handset */
    return connected;
}

bool appTestIsHandsetA2dpMediaConnected(void)
{
    bool connected = FALSE;
    bdaddr bd_addr;

    if (appDeviceGetHandsetBdAddr(&bd_addr))
    {
        /* Find handset AV instance */
        avInstanceTaskData *theInst = appAvInstanceFindFromBdAddr(&bd_addr);
        connected = theInst && appA2dpIsConnectedMedia(theInst);
    }

    DEBUG_LOG_ALWAYS("appTestIsHandsetA2dpMediaConnected:%d", connected);

    /* If we get here then there's no A2DP connected for handset */
    return connected;
}

/*! \brief Return if Earbud is in A2DP streaming mode with the handset
*/
bool appTestIsHandsetA2dpStreaming(void)
{
    bool streaming = FALSE;
    bdaddr bd_addr;

    if (appDeviceGetHandsetBdAddr(&bd_addr))
    {
        /* Find handset AV instance */
        avInstanceTaskData *theInst = appAvInstanceFindFromBdAddr(&bd_addr);
        streaming = theInst && appA2dpIsStreaming(theInst);
    }

    DEBUG_LOG_ALWAYS("appTestIsHandsetA2dpStreaming:%d", streaming);

    /* If we get here then there's no A2DP connected for handset */
    return streaming;
}

bool appTestIsA2dpPlaying(void)
{
    bool playing = Av_IsPlaying();

    DEBUG_LOG_ALWAYS("appTestIsA2dpPlaying=%d", playing);

    return playing;
}

/*! \brief Initiate Earbud AVRCP connection to the Handset
*/
bool appTestHandsetAvrcpConnect(void)
{
    bdaddr bd_addr;

    DEBUG_LOG_ALWAYS("appTestHandsetAvrcpConnect");
    if (appDeviceGetHandsetBdAddr(&bd_addr))
        return  appAvAvrcpConnectRequest(NULL, &bd_addr);
    return FALSE;
}

/*! \brief Return if Earbud has an Handset AVRCP connection
*/
bool appTestIsHandsetAvrcpConnected(void)
{
    bool connected = FALSE;
    bdaddr bd_addr;

    if (appDeviceGetHandsetBdAddr(&bd_addr))
    {
        /* Find handset AV instance */
        avInstanceTaskData *theInst = appAvInstanceFindFromBdAddr(&bd_addr);
        connected = theInst && appAvrcpIsConnected(theInst);
    }

    DEBUG_LOG_ALWAYS("appTestIsHandsetAvrcpConnected = %d", connected);

    /* If we get here then there's no AVRCP connected for handset */
    return connected;
}

/*! \brief Return if Earbud has an Handset HFP connection
*/
bool appTestIsHandsetHfpConnected(void)
{
    bool connected = appHfpIsConnected();

    DEBUG_LOG_ALWAYS("appTestIsHandsetHfpConnected= %d", connected);

    return connected;
}

/*! \brief Return if Earbud has an Handset HFP SCO connection
*/
bool appTestIsHandsetHfpScoActive(void)
{
    bool active = HfpProfile_IsScoActive();

    DEBUG_LOG_ALWAYS("appTestIsHandsetHfpScoActive:%d", active);

    return active;
}

/*! \brief Initiate Earbud HFP Voice Dial request to the Handset
*/
void appTestHandsetHfpVoiceDial(void)
{
    DEBUG_LOG_ALWAYS("appTestHandsetHfpVoiceDial");
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_voice_dial);
}

void appTestHandsetHfpMuteToggle(void)
{
    DEBUG_LOG_ALWAYS("appTestHandsetHfpMute");
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_mic_mute_toggle);
}

void appTestHandsetHfpVoiceTransferToAg(void)
{
    DEBUG_LOG_ALWAYS("appTestHandsetHfpVoiceTransferToAg");
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_voice_transfer_to_ag);
}

void appTestHandsetHfpVoiceTransferToHeadset(void)
{
    DEBUG_LOG_ALWAYS("appTestHandsetHfpVoiceTransferToHeadset");
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_voice_transfer_to_headset);
}

void appTestHandsetHfpCallAccept(void)
{
    DEBUG_LOG_ALWAYS("appTestHandsetHfpCallAccept");
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_voice_call_accept);
}

void appTestHandsetHfpCallReject(void)
{
    DEBUG_LOG_ALWAYS("appTestHandsetHfpCallReject");
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_voice_call_reject);
}

void appTestHandsetHfpCallHangup(void)
{
    DEBUG_LOG_ALWAYS("appTestHandsetHfpCallHangup");
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_voice_call_hang_up);
}

void appTestHandsetVoiceCallCycle(void)
{
    DEBUG_LOG_ALWAYS("appTestHandsetVoiceCallCycle");
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_voice_call_cycle);
}

void appTestHandsetVoiceCallJoinCalls(void)
{
    DEBUG_LOG_ALWAYS("appTestHandsetVoiceCallJoinCalls");
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_voice_call_join_calls);
}

void appTestHandsetVoiceCallJoinCallsAndHangUp(void)
{
    DEBUG_LOG_ALWAYS("appTestHandsetVoiceCallJoinCallsAndHangUp");
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_voice_call_join_calls_and_hang_up);
}

bool appTestHandsetHfpCallLastDialed(void)
{
    DEBUG_LOG_ALWAYS("appTestHandsetHfpCallLastDialed");
    hfpInstanceTaskData * instance = HfpProfile_GetInstanceForVoiceSourceWithUiFocus();
    if (instance != NULL && appHfpIsConnectedForInstance(instance))
    {
        Ui_InjectUiInput(ui_input_voice_call_last_dialed);
        return TRUE;
    }
    else
        return FALSE;
}

void appTestHandsetHfpVolumeDownStart(void)
{
    DEBUG_LOG_ALWAYS("appTestHandsetHfpVolumeDownStart");
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_volume_down_start);
}

void appTestHandsetHfpVolumeUpStart(void)
{
    DEBUG_LOG_ALWAYS("appTestHandsetHfpVolumeUpStart");
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_volume_up_start);
}

void appTestHandsetHfpVolumeStop(void)
{
    DEBUG_LOG_ALWAYS("appTestHandsetHfpVolumeStop");
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_volume_stop);
}

bool appTestHandsetHfpSetScoVolume(uint8 volume)
{
    voice_source_t focused_source = voice_source_none;
    bool volume_set = FALSE;

    if (TwsTopology_IsSecondary())
    {
        // This Test API should never be called on the secondary Earbud
        DEBUG_LOG_ALWAYS("appTestHandsetHfpScoVolume call on Secondary EB. Volume change won't be applied");
        return volume_set;
    }

    hfpInstanceTaskData * instance = HfpProfile_GetInstanceForVoiceSourceWithUiFocus();
    if (instance != NULL && appHfpIsCallForInstance(instance))
    {
        focused_source = HfpProfileInstance_GetVoiceSourceForInstance(instance);
        if (VoiceSource_IsHfp(focused_source))
        {
            Volume_SendVoiceSourceVolumeUpdateRequest(focused_source, event_origin_local, volume);
            volume_set = TRUE;
        }
    }

    DEBUG_LOG_ALWAYS("appTestHandsetHfpScoVolume enum:voice_source_t:%d", focused_source);

    return volume_set;
}

void appTestHandsetVoiceCallAccept(void)
{
    DEBUG_LOG_ALWAYS("appTestHandsetVoiceCallAccept");
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_voice_call_accept);
}

void appTestHandsetVoiceCallReject(void)
{
    DEBUG_LOG_ALWAYS("appTestHandsetVoiceCallReject");
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_voice_call_reject);
}

void appTestHandsetVoiceCallHangup(void)
{
    DEBUG_LOG_ALWAYS("appTestHandsetVoiceCallHangup");
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_voice_call_hang_up);
}

bool appTestIsHandsetHfpMuted(void)
{
    bool muted = FALSE;
    hfpInstanceTaskData * instance = HfpProfile_GetInstanceForVoiceSourceWithUiFocus();
    if (instance != NULL)
    {
        muted = HfpProfile_IsMicrophoneMuted(instance);
    }

    DEBUG_LOG_ALWAYS("appTestIsHandsetHfpMuted;%d", muted);

    return muted;
}

bool appTestIsHandsetHfpCall(void)
{
    bool is_call = appHfpIsCall();

    DEBUG_LOG_ALWAYS("appTestIsHandsetHfpCall:%d", is_call);

    return is_call;
}

bool appTestIsHandsetHfpCallIncoming(void)
{
    bool incoming = appHfpIsCallIncoming();

    DEBUG_LOG_ALWAYS("appTestIsHandsetHfpCallIncoming:%d", incoming);

    return incoming;
}

bool appTestIsHandsetHfpCallOutgoing(void)
{
    bool outgoing = appHfpIsCallOutgoing();

    DEBUG_LOG_ALWAYS("appTestIsHandsetHfpCallOutgoing:%d", outgoing);

    return outgoing;
}

/*! \brief Return if Earbud has a connection to the Handset
*/
bool appTestIsHandsetConnected(void)
{
    bool connected = appTestIsHandsetA2dpConnected() ||
                     appTestIsHandsetAvrcpConnected() ||
                     appTestIsHandsetHfpConnected();

    DEBUG_LOG_ALWAYS("appTestIsHandsetConnected = %d", connected);

    return connected;
}

/*! \brief Initiate Earbud A2DP connection to the the Peer
*/
bool appTestPeerA2dpConnect(void)
{
    DEBUG_LOG_ALWAYS("appTestPeerA2dpConnect ***DEPRECATED***");

    return FALSE;
}

/*! \brief Return if Earbud has a Peer A2DP connection
*/
bool appTestIsPeerA2dpConnected(void)
{
    bool connected = FALSE;

    /* Since TWM implemented, test if A2DP mirroring is connected instead. */
    connected = MirrorProfile_IsConnected();

    DEBUG_LOG_ALWAYS("appTestIsPeerA2dpConnected:%d", connected);

    return connected;
}


/*! \brief Check if Earbud is in A2DP streaming mode with peer Earbud
 */
bool appTestIsPeerA2dpStreaming(void)
{
    bool streaming = FALSE;

    /* Since TWM implemented, test if A2DP mirroring is streaming. */
    streaming = MirrorProfile_IsA2dpActive();

    DEBUG_LOG_ALWAYS("appTestIsPeerA2dpStreaming:%d", streaming);

    return streaming;
}


/*! \brief Initiate Earbud AVRCP connection to the the Peer
*/
bool appTestPeerAvrcpConnect(void)
{
    DEBUG_LOG_ALWAYS("appTestPeerAvrcpConnect ***DEPRECATED***");

    return FALSE;
}

/*! \brief Return if Earbud has a Peer AVRCP connection
*/
bool appTestIsPeerAvrcpConnected(void)
{
    bool connected = FALSE;

    /* Since TWM implemented, consider a peer signalling connection as the equivalent connection. */
    connected = appPeerSigIsConnected();

    DEBUG_LOG_ALWAYS("appTestIsPeerAvrcpConnected:%d", connected);

    return connected;
}

#ifdef INCLUDE_EXTRA_TESTS
void appTestPeerDisconnect(void)
{
    bdaddr bd_addr;

    if(appDeviceGetPeerBdAddr(&bd_addr))
    {
        DEBUG_LOG_ALWAYS("appTestPeerDisconnect peer lap 0x%x", bd_addr.lap);

        ConManagerSendCloseAclRequest(&bd_addr, TRUE);
        appTestTransmitEnable(0);
    }
    else
    {
        DEBUG_LOG_ALWAYS("appTestPeerDisconnect NO PEER");
    }
}
#endif

/*! \brief Send the AV play/pause toggle command
*/
void appTestAvTogglePlayPause(void)
{
    DEBUG_LOG_ALWAYS("appTestAvTogglePlayPause");
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_toggle_play_pause);
}

#ifdef INCLUDE_HDMA
#include "hdma.h"
/*! \brief Start dynamic handover procedure */
bool earbudTest_DynamicHandover(void)
{
    DEBUG_LOG_ALWAYS("earbudTest_DynamicHandover");
    return Hdma_ExternalHandoverRequest();
}
#endif

/*! \brief Send the Avrcp pause command to the Handset
*/
void appTestAvPause(void)
{
    DEBUG_LOG_ALWAYS("appTestAvPause");
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_pause_all);
}

/*! \brief Send the Avrcp play command to the Handset
*/
void appTestAvPlay(void)
{
    DEBUG_LOG_ALWAYS("appTestAvPlay");
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_play);
}

/*! \brief Send the Avrcp stop command to the Handset
*/
void appTestAvStop(void)
{
    DEBUG_LOG_ALWAYS("appTestAvStop");
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_stop_av_connection);
}

/*! \brief Send the Avrcp forward command to the Handset
*/
void appTestAvForward(void)
{
    DEBUG_LOG_ALWAYS("appTestAvForward");
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_av_forward);
}

/*! \brief Send the Avrcp backward command to the Handset
*/
void appTestAvBackward(void)
{
    DEBUG_LOG_ALWAYS("appTestAvBackward");
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_av_backward);
}

/*! \brief Send the Avrcp fast forward state command to the Handset
*/
void appTestAvFastForwardStart(void)
{
    DEBUG_LOG_ALWAYS("appTestAvFastForwardStart");
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_av_fast_forward_start);
}

/*! \brief Send the Avrcp fast forward stop command to the Handset
*/
void appTestAvFastForwardStop(void)
{
    DEBUG_LOG_ALWAYS("appTestAvFastForwardStop");
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_fast_forward_stop);
}

/*! \brief Send the Avrcp rewind start command to the Handset
*/
void appTestAvRewindStart(void)
{
    DEBUG_LOG_ALWAYS("appTestAvRewindStart");
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_av_rewind_start);
}

/*! \brief Send the Avrcp rewind stop command to the Handset
*/
void appTestAvRewindStop(void)
{
    DEBUG_LOG_ALWAYS("appTestAvRewindStop");
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_rewind_stop);
}

/*! \brief Send the Avrcp volume change command to the Handset
*/
bool appTestAvVolumeChange(int8 step)
{
    bool volume_change_applied = FALSE;

    DEBUG_LOG_ALWAYS("appTestAvVolumeChange %d", step);

    if (step != 0)
    {
        audio_source_t focused_source = audio_source_none;
        bool is_focused_source_found = Focus_GetAudioSourceForContext(&focused_source);

        if (!is_focused_source_found && MirrorProfile_IsA2dpActive())
        {
            // If there is no focused source, but A2DP mirroring is active, then
            // use a2dp1 audio source (which is hard-coded for mirroring scenario).
            focused_source = audio_source_a2dp_1;
        }

        if (focused_source == audio_source_a2dp_1 || focused_source == audio_source_a2dp_2)
        {
            int step_size = VolumeUtils_GetStepSize(AudioSources_GetVolume(focused_source).config);
            if ((step % step_size) == 0)
            {
                int ui_inputs_to_raise = ABS(step) / step_size;
                ui_input_t ui_input = (step > 0) ? ui_input_volume_up : ui_input_volume_down;
                for (int ui_input_counter = 0; ui_input_counter < ui_inputs_to_raise; ui_input_counter++)
                {
                    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input);
                }
                DEBUG_LOG_VERBOSE("appTestAvVolumeChange applied %d volume steps", ui_inputs_to_raise);

                volume_change_applied = TRUE;
            }
            else
            {
                DEBUG_LOG_ALWAYS("step should be in multiple of step size %d", step_size);
            }
        }
        else
        {
            DEBUG_LOG_ALWAYS("focused audio source is not A2dp enum:audio_source_t:%d", focused_source);
        }
    }

    return volume_change_applied;
}

void appTestAvVolumeSet(uint8 volume)
{
    DEBUG_LOG_ALWAYS("appTestAvVolumeSet %d", volume);

    if (TwsTopology_IsSecondary())
    {
        // This Test API should never be called on the secondary Earbud
        Panic();
    }

    audio_source_t focused_source = audio_source_none;
    if (!Focus_GetAudioSourceForContext(&focused_source))
    {
        DEBUG_LOG_ALWAYS("no focused audio source");
        return;
    }

    if (focused_source != audio_source_a2dp_1 && focused_source != audio_source_a2dp_2)
    {
        DEBUG_LOG_ALWAYS("focused audio source is not A2dp");
        return;
    }

    Volume_SendAudioSourceVolumeUpdateRequest(focused_source, event_origin_local, volume);
}

/*! \brief Send the Media Control pause command to the Handset
*/
void appTestMediaControlPause(void)
{
    DEBUG_LOG_ALWAYS("appTestMediaControlPause");
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_pause);
}

/*! \brief Send the Media Control pause all command to the Handset
*/
void appTestMediaControlPauseAll(void)
{
    DEBUG_LOG_ALWAYS("appTestMediaControlPauseAll");
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_pause_all);
}

/*! \brief Send the Media Control play command to the Handset
*/
void appTestMediaControlPlay(void)
{
    DEBUG_LOG_ALWAYS("appTestMediaControlPlay");
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_play);
}

/*! \brief Send the Media Control stop command to the Handset
*/
void appTestMediaControlStop(void)
{
    DEBUG_LOG_ALWAYS("appTestMediaControlStop");
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_stop_av_connection);
}

/*! \brief Send the Media Control forward command to the Handset
*/
void appTestMediaControlForward(void)
{
    DEBUG_LOG_ALWAYS("appTestMediaControlForward");
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_av_forward);
}

/*! \brief Send the Media Control backward command to the Handset
*/
void appTestMediaControlBackward(void)
{
    DEBUG_LOG_ALWAYS("appTestMediaControlBackward");
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_av_backward);
}

/*! \brief Send the Media Control play/pause toggle command
*/
void appTestMediaControlTogglePlayPause(void)
{
    DEBUG_LOG_ALWAYS("appTestMediaControlTogglePlayPause");
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_toggle_play_pause);
}

void appTestPowerAllowDormant(bool enable)
{
    powerTaskData *thePower = PowerGetTaskData();

    DEBUG_LOG_ALWAYS("appTestPowerAllowDormant %d", enable);
    thePower->allow_dormant = enable;
}

/*! \brief Returns the DSP clock speed in Mhz during the active mode.
*/
uint32 EarbudTest_DspClockSpeedInActiveMode(void)
{
    audio_dsp_clock kclocks;
    audio_power_save_mode mode = AUDIO_POWER_SAVE_MODE_3;

    PanicFalse(AudioDspGetClock(&kclocks));
    mode = AudioPowerSaveModeGet();
    DEBUG_LOG("EarbudTest_DspClockSpeedInActiveMode, current mode of operation of kymera dsp is %d, Active mode: %d Mhz, Low Power mode: %d Mhz, Trigger mode: %d Mhz", mode, kclocks.active_mode, kclocks.low_power_mode, kclocks.trigger_mode);
    return kclocks.active_mode;
}

#ifdef INCLUDE_EXTRA_TESTS

/*! \brief Test the generation of link kets */
extern void TestLinkkeyGen(void)
{
    bdaddr bd_addr;
    uint16 lk[8];
    uint16 lk_out[8];

    bd_addr.nap = 0x0002;
    bd_addr.uap = 0x5B;
    bd_addr.lap = 0x00FF02;

    lk[0] = 0x9541;
    lk[1] = 0xe6b4;
    lk[2] = 0x6859;
    lk[3] = 0x0791;
    lk[4] = 0x9df9;
    lk[5] = 0x95cd;
    lk[6] = 0x9570;
    lk[7] = 0x814b;

    DEBUG_LOG_ALWAYS("appTestPowerAllowDormant");
    PeerLinkKeys_GenerateKey(&bd_addr, lk, 0x74777332UL, lk_out);

#if 0
    bd_addr.nap = 0x0000;
    bd_addr.uap = 0x74;
    bd_addr.lap = 0x6D7031;

    lk[0] = 0xec02;
    lk[1] = 0x34a3;
    lk[2] = 0x57c8;
    lk[3] = 0xad05;
    lk[4] = 0x3410;
    lk[5] = 0x10a6;
    lk[6] = 0x0a39;
    lk[7] = 0x7d9b;
#endif

    PeerLinkKeys_GenerateKey(&bd_addr, lk, 0x6c656272UL, lk_out);

}

/*! \brief Test the cryptographic key conversion function, producing an H6 key */
extern void TestH6(void)
{
    uint8 key_h7[16] = {0xec,0x02,0x34,0xa3,0x57,0xc8,0xad,0x05,0x34,0x10,0x10,0xa6,0x0a,0x39,0x7d,0x9b};
    //uint32 key_id = 0x6c656272;
    uint32 key_id = 0x7262656c;
    uint8 key_h6[16];

    DEBUG_LOG_ALWAYS("appTestPowerAllowDormant");
    CryptoVmH6(key_h7, key_id, key_h6);
    printf("H6: ");
    for (int h6_i = 0; h6_i < 16; h6_i++)
        printf("%02x ", key_h6[h6_i]);
    printf("\n");
}

#endif /* INCLUDE_EXTRA_TESTS */

/*! \brief report the saved handset information */
extern void appTestHandsetInfo(void)
{
    bdaddr bd_addr;
    DEBUG_LOG_ALWAYS("appTestHandsetInfo");
    if (appDeviceGetHandsetBdAddr(&bd_addr))
    {
        DEBUG_LOG_VERBOSE("appTestHandsetInfo, bdaddr %04x,%02x,%06lx",
                   bd_addr.nap, bd_addr.uap, bd_addr.lap);
    }
}

#include "adk_log.h"

/*! \brief Simple test function to make sure that the DEBUG_LOG macros
        work */
extern void TestDebug(void)
{
    DEBUG_LOG_ALWAYS("TestDebug (Always): test %d %d", 1, 2);
    DEBUG_LOG_ERROR("TestDebug (Error): test");
}

/*! \brief Generate event that Earbud is now in the case. */
void appTestPhyStateInCaseEvent(void)
{
    DEBUG_LOG_ALWAYS("appTestPhyStateInCaseEvent");
    appPhyStateInCaseEvent();
}

/*! \brief Generate event that Earbud is now out of the case. */
void appTestPhyStateOutOfCaseEvent(void)
{
    DEBUG_LOG_ALWAYS("appTestPhyStateOutOfCaseEvent");
    appPhyStateOutOfCaseEvent();
}

/*! \brief Generate event that Earbud is now in ear. */
void appTestPhyStateInEarEvent(void)
{
    DEBUG_LOG_ALWAYS("appTestPhyStateInEarEvent");
    appPhyStateInEarEvent();
}

/*! \brief Generate event that Earbud is now out of the ear. */
void appTestPhyStateOutOfEarEvent(void)
{
    DEBUG_LOG_ALWAYS("appTestPhyStateOutOfEarEvent");
    appPhyStateOutOfEarEvent();
}

/*! \brief Generate event that Earbud is now moving */
void appTestPhyStateMotionEvent(void)
{
    DEBUG_LOG_ALWAYS("appTestPhyStateMotionEvent");
    appPhyStateMotionEvent();
}

/*! \brief Generate event that Earbud is now not moving. */
void appTestPhyStateNotInMotionEvent(void)
{
    DEBUG_LOG_ALWAYS("appTestPhyStateNotInMotionEvent");
    appPhyStateNotInMotionEvent();
}

bool appTestPhyStateIsInEar(void)
{
    return (PHY_STATE_IN_EAR == appPhyStateGetState());
}


bool appTestPhyStateOutOfEar(void)
{
    return     PHY_STATE_OUT_OF_EAR == appPhyStateGetState()
            || PHY_STATE_OUT_OF_EAR_AT_REST == appPhyStateGetState();
}


bool appTestPhyStateIsInCase(void)
{
    return (PHY_STATE_IN_CASE == appPhyStateGetState());
}

static bool reset_happened = TRUE;

/*! \brief Reset an Earbud to factory defaults.
    Will drop any connections, delete all pairing and reboot.
*/
void appTestFactoryReset(void)
{
    DEBUG_LOG_ALWAYS("appTestFactoryReset");
    reset_happened = FALSE;
    LogicalInputSwitch_SendPassthroughDeviceSpecificLogicalInput(ui_input_factory_reset_request);
}

/*! \brief Determine if a reset has happened
    Will return TRUE only once after a reset.
    All subsequent calls will return FALSE
*/
bool appTestResetHappened(void)
{
    bool result = reset_happened;

    DEBUG_LOG_ALWAYS("appTestResetHappened: %d", result);

    if(reset_happened) {
        reset_happened = FALSE;
    }

    return result;
}

/*! \brief Determine if the earbud has a paired peer earbud.
*/
bool appTestIsPeerPaired(void)
{
    bool paired = BtDevice_IsPairedWithPeer();

    DEBUG_LOG_ALWAYS("appTestIsPeerPaired:%d",paired);

    return paired;
}

void appTestConnectHandset(void)
{
    DEBUG_LOG_ALWAYS("appTestConnectHandset");
    appSmConnectHandset();
}

bool appTestConnectHandsetA2dpMedia(void)
{
    DEBUG_LOG_ALWAYS("appTestConnectHandsetA2dpMedia ** DEPRECATED **");
    return FALSE;
}

/*! \brief Send A2dp Media Start request to remote
     Added to support PTS qualification TCs.
*/
bool appTestHandsetA2dpMediaStart(void)
{
    bdaddr bd_addr;

    DEBUG_LOG_ALWAYS("appTestHandsetA2dpMediaStart");

    if (appDeviceGetHandsetBdAddr(&bd_addr))
    {
        /* Find handset AV instance */
        avInstanceTaskData *theInst = appAvInstanceFindFromBdAddr(&bd_addr);

        if (theInst)
        {
            return A2dpMediaStartRequest(theInst->a2dp.device_id, theInst->a2dp.stream_id);
        }
    }

    return FALSE;
}

/*! \brief Send A2dp Media Suspend request to remote
    Added to support PTS qualification TCs.
*/
bool appTestHandsetA2dpMediaSuspend(void)
{
    DEBUG_LOG_ALWAYS("appTestHandsetA2dpMediaSuspend");

    bdaddr bd_addr;

    if (appDeviceGetHandsetBdAddr(&bd_addr))
    {
        /* Find handset AV instance */
        avInstanceTaskData *theInst = appAvInstanceFindFromBdAddr(&bd_addr);

        if (theInst)
        {
            return A2dpMediaSuspendRequest(theInst->a2dp.device_id, theInst->a2dp.stream_id);
        }
    }

    return FALSE;
}

/*! \brief Send Media Close to remote
    Added to support PTS qualification TCs.
*/
bool appTestHandsetA2dpMediaClose(void)
{
    bdaddr bd_addr;

    DEBUG_LOG_ALWAYS("appTestHandsetA2dpMediaClose");

    if (appDeviceGetHandsetBdAddr(&bd_addr))
    {
        /* Find handset AV instance */
        avInstanceTaskData *theInst = appAvInstanceFindFromBdAddr(&bd_addr);

        if (theInst)
        {
            return A2dpMediaCloseRequest(theInst->a2dp.device_id, theInst->a2dp.stream_id);
        }
    }

    return FALSE;
}

/*! \brief Send A2dp Abort stream request to remote
    Added to support PTS qualification TCs.
*/
bool appTestHandsetA2dpAbort(void)
{
    bdaddr bd_addr;

    DEBUG_LOG_ALWAYS("appTestHandsetA2dpAbort");

    if (appDeviceGetHandsetBdAddr(&bd_addr))
    {
        return FALSE;
    }
    return FALSE;
}

/*! \brief Send A2dp Reconfigure stream request to remote
    Added to support PTS qualification TCs.
*/
bool appTestHandsetA2dpMediaReconfigure(void)
{
    bdaddr bd_addr;

    DEBUG_LOG_ALWAYS("appTestHandsetA2dpMediaReconfigure");

    if (appDeviceGetHandsetBdAddr(&bd_addr))
    {
        avInstanceTaskData *theInst = appAvInstanceFindFromBdAddr(&bd_addr);

        if (theInst)
        {
            uint8 *caps = (uint8 *)malloc(sizeof(uint8) * 8);

            /* Just hardcode the values */
            caps[0] = 7;
            caps[1] = 6;
            caps[2] = 0;
            caps[3] = 0;
            caps[4] = 17;
            caps[5] = 21;
            caps[6] = 30;
            caps[7] = 50;
    
            return A2dpMediaReconfigureRequest(theInst->a2dp.device_id,
                                               theInst->a2dp.stream_id,
                                               8,
                                               caps);
        }
    }
    return FALSE;
}

bool appTestIsPeerSyncComplete(void)
{
    DEBUG_LOG_ALWAYS("appTestIsPeerSyncComplete:1 ** DEPRECATED **");
    /* test scripts may rely on this, for now just return TRUE so
     * they keep running. */
    return TRUE;
}


bool power_off_tone_have_been_played = FALSE;
bool power_off_prompt_have_been_played = FALSE;

static void testTaskHandler(Task task, MessageId id, Message message)
{
    static lp_power_mode   powermode = 42;
    static uint16          interval = (uint16)-1;

    UNUSED(task);

    switch (id)
    {
        case CL_DM_ROLE_CFM:
            {
                const CL_DM_ROLE_CFM_T *cfm = (const CL_DM_ROLE_CFM_T *)message;
                bdaddr  peer;
                tp_bdaddr sink_addr;

                if (  !SinkGetBdAddr(cfm->sink, &sink_addr)
                   || !appDeviceGetPeerBdAddr(&peer))
                {
                    return;
                }

                if (   BdaddrIsSame(&peer, &sink_addr.taddr.addr)
                    && hci_success == cfm->status)
                {
                    if (hci_role_master == cfm->role)
                    {
                        DEBUG_LOG_VERBOSE("SCO FORWARDING LINK IS: MASTER");
                    }
                    else if (hci_role_slave == cfm->role)
                    {
                        DEBUG_LOG_VERBOSE("SCO FORWARDING LINK IS: SLAVE");
                    }
                    else
                    {
                        DEBUG_LOG_VERBOSE("SCO FORWARDING LINK STATE IS A MYSTERY: %d",cfm->role);
                    }
                    DEBUG_LOG_VERBOSE("SCO FORWARDING POWER MODE (cached) IS %d (sniff = %d)",powermode,lp_sniff);
                    DEBUG_LOG_VERBOSE("SCO FORWARDING INTERVAL (cached) IS %d",interval);
                }
            }
            break;

        case CL_DM_MODE_CHANGE_EVENT:
            {
                bdaddr peer;
                const CL_DM_MODE_CHANGE_EVENT_T *mode = (const CL_DM_MODE_CHANGE_EVENT_T *)message;

                if (!appDeviceGetPeerBdAddr(&peer))
                    return;

                if (BdaddrIsSame(&mode->bd_addr, &peer))
                {
                    powermode = mode->mode;
                    interval = mode->interval;
                }
            }
            break;

        case CL_DM_LOCAL_BD_ADDR_CFM:
            {
                const CL_DM_LOCAL_BD_ADDR_CFM_T *addr_cfm = (const CL_DM_LOCAL_BD_ADDR_CFM_T *)message;
                if (addr_cfm->status == hci_success)
                {
                    appTestReadLocalAddr_completed = TRUE;
                    appTestGetLocalAddr_address = addr_cfm->bd_addr;

                    DEBUG_LOG_ALWAYS("appTestReadLocalAddr completed. %04x:%02x:%06x",
                                     appTestGetLocalAddr_address.nap,
                                     appTestGetLocalAddr_address.uap,
                                     appTestGetLocalAddr_address.lap);
                }
                else
                {
                    DEBUG_LOG_WARN("appTestReadLocalAddr failed. Status:%d",
                                   addr_cfm->status);
                }
            }
            break;

        case KYMERA_NOTIFICATION_TONE_STARTED:
            {
                const KYMERA_NOTIFICATION_TONE_STARTED_T *tone_msg = (KYMERA_NOTIFICATION_TONE_STARTED_T *)message;

                DEBUG_LOG("testTaskHandler KYMERA_NOTIFICATION_TONE_STARTED received tone %p", tone_msg->tone);

                if(tone_msg->tone == app_tone_battery_empty)
                {
                    DEBUG_LOG_VERBOSE("testTaskHandler Received tone is app_tone_battery_empty");
                    power_off_tone_have_been_played = TRUE;
                }
                else if(tone_msg->tone == app_tone_av_connected)
                {
                    DEBUG_LOG_VERBOSE("testTaskHandler Received tone is app_tone_av_connected");
                }
                else
                {
                    DEBUG_LOG_VERBOSE("testTaskHandler Received tone is not yet recognised");
                }
            }
            break;

        case KYMERA_NOTIFICATION_PROMPT_STARTED:
            {
                const KYMERA_NOTIFICATION_PROMPT_STARTED_T *prompt_msg = (KYMERA_NOTIFICATION_PROMPT_STARTED_T *)message;

                DEBUG_LOG_VERBOSE("testTaskHandler KYMERA_NOTIFICATION_PROMPT_STARTED received prompt id 0x%x", prompt_msg->id);

                FILE_INDEX power_off_id = FileFind(FILE_ROOT, "power_off.sbc", strlen("power_off.sbc"));

                if (prompt_msg->id == power_off_id)
                {
                    DEBUG_LOG_VERBOSE("testTaskHandler Received prompt is POWER OFF");
                    power_off_prompt_have_been_played = TRUE;
                }
                else
                {
                    DEBUG_LOG_VERBOSE("testTaskHandler Received prompt is not yet recognised");
                }
            }
            break;

        case PEER_PAIR_LE_PAIR_CFM:
            {
                const PEER_PAIR_LE_PAIR_CFM_T *pair_cfm = (const PEER_PAIR_LE_PAIR_CFM_T *)message;

                DEBUG_LOG_ALWAYS("testTaskHandler Peer Pair LE completed. status enum:peer_pair_le_status_t:%d", pair_cfm->status);
            }
            break;
    }
}

void appTestScoFwdLinkStatus(void)
{
}

bool appTestScoFwdConnect(void)
{
    return FALSE;
}

bool appTestScoFwdDisconnect(void)
{
    return TRUE;
}

bool appTestPowerOff(void)
{
    DEBUG_LOG_ALWAYS("appTestPowerOff");
    return appPowerOffRequest();
}

/* Remove these once feature_if.h has been updated */
#define APTX_ADAPTIVE (40)
#define APTX_ADAPTIVE_MONO (41)

bool appTestLicenseCheck(void)
{
    const uint8 license_table[7] =
    {
        APTX_CLASSIC, APTX_CLASSIC_MONO,
        CVC_RECV, CVC_SEND_HS_1MIC, CVC_SEND_HS_2MIC_MO,
        APTX_ADAPTIVE_MONO
    };
    const bool license_enabled[7] =
    {
        appConfigAptxEnabled(), appConfigAptxEnabled(),
        TRUE, TRUE, TRUE,
        appConfigAptxAdaptiveEnabled()
    };

    bool licenses_ok = TRUE;

    DEBUG_LOG_ALWAYS("appTestLicenseCheck");
    for (int i = 0; i < ARRAY_DIM(license_table); i++)
    {
        if (license_enabled[i])
        {
            if (!FeatureVerifyLicense(license_table[i]))
            {
                DEBUG_LOG_ALWAYS("appTestLicenseCheck: License for feature %d not valid", license_table[i]);
                licenses_ok = FALSE;
            }
            else
                DEBUG_LOG_ALWAYS("appTestLicenseCheck: License for feature %d valid", license_table[i]);
        }
    }

    return licenses_ok;
}

void appTestConnectAfterPairing(bool enable)
{
    UNUSED(enable);
    DEBUG_LOG_WARN("appTestConnectAfterPairing ** DEPRECATED **");
}

bool appTestScoFwdForceDroppedPackets(unsigned percentage_to_drop, int multiple_packets)
{
    UNUSED(percentage_to_drop);
    UNUSED(multiple_packets);
    return FALSE;
}

bool appTestMicFwdLocalMic(void)
{
    return FALSE;
}

bool appTestMicFwdRemoteMic(void)
{
    return FALSE;
}

bool appTestIsPtsMode(void)
{
    return A2dpProfile_IsPtsMode();
}

bool appTestSetPtsMode(bool enable)
{
    DEBUG_LOG_ALWAYS("appTestSetPtsMode");

    A2dpProfile_SetPtsMode(enable);

    return TRUE;
}

bool appTestPtsSetAsPeer(void)
{
    bdaddr handset_addr;
    bdaddr peer_addr;
    device_t device;
    uint16 flags = 0;
    deviceType type = DEVICE_TYPE_EARBUD;

    if (appDeviceGetHandsetBdAddr(&handset_addr))
    {
        device = BtDevice_GetDeviceForBdAddr(&handset_addr);
        if (device)
        {
            if (appDeviceGetPeerBdAddr(&peer_addr))
            {
                ConnectionAuthSetPriorityDevice(&peer_addr, FALSE);
                ConnectionSmDeleteAuthDevice(&peer_addr);
            }
            Device_SetProperty(device, device_property_type, &type, sizeof(deviceType));
            Device_GetPropertyU16(device, device_property_flags, &flags);
            flags |= DEVICE_FLAGS_SECONDARY_ADDR;
            flags |= DEVICE_FLAGS_IS_PTS;
            Device_SetPropertyU16(device, device_property_flags, flags);
            Device_SetPropertyU32(device, device_property_supported_profiles, DEVICE_PROFILE_A2DP);

            DeviceDbSerialiser_Serialise();

            SystemReboot_Reboot();
            return TRUE;
        }
    }

    DEBUG_LOG_ALWAYS("appTestPtsSetAsPeer, failed");
    return FALSE;
}

/*! \brief Determine if appConfigScoForwardingEnabled is TRUE
*/
bool appTestIsScoFwdIncluded(void)
{
    return FALSE;
}

/*! \brief Determine if appConfigMicForwardingEnabled is TRUE.
*/
bool appTestIsMicFwdIncluded(void)
{
    return FALSE;
}

/*! \brief Macro to create function declaration/definition that returns the value
    of the specified configuration.
    \param returntype The type returned by the function.
    \param name Given a configuration named appConfigXYZ(), XYZ should be passed
    to the name argument and a function will be created called appTestConfigXYZ().*/
#define MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(returntype, name) \
returntype appTestConfig##name(void);                   \
returntype appTestConfig##name(void)                    \
{                                                       \
    return appConfig##name();                           \
}

/*! \name Config Accessor functions.
    \note These don't have a public declaration as they are not expected to be
    called within the earbud code - the appConfig functions/macros should be used
    instead.
*/
//!@{
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(uint16, BatteryFullyCharged)
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(uint16, BatteryVoltageOk)
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(uint16, BatteryVoltageLow)
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(uint16, BatteryVoltageCritical)
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(int8, BatteryChargingTemperatureMax)
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(int8, BatteryChargingTemperatureMin)
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(int8, BatteryDischargingTemperatureMax)
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(int8, BatteryDischargingTemperatureMin)
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(uint32, BatteryReadingPeriodMs)
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(uint16, ChargerTrickleCurrent)
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(uint16, ChargerPreCurrent)
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(uint16, ChargerPreFastThresholdVoltage)
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(uint16, ChargerFastCurrent)
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(uint16, ChargerTerminationCurrent)
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(uint16, ChargerTerminationVoltage)
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(uint32, ChargerPreChargeTimeoutMs)
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(uint32, ChargerFastChargeTimeoutMs)
#ifdef INCLUDE_TEMPERATURE
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(uint32, TemperatureReadingPeriodMs)
#endif
//!@}

#ifdef INCLUDE_EXTRA_TESTS

const uint16 FFA_coefficients[15] = {0xFD66, 0x00C4, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0101, 0xFFE6, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000};
const uint16 FFB_coefficients[15] = {0xFE66, 0xFF5F, 0xFFC8, 0x0028, 0x0071, 0x001C, 0xFFC9, 0x00E1, 0xFFEC, 0xFF18, 0xFFF2, 0x0028, 0x0015, 0x0027, 0xFFE6};
const uint16 FB_coefficients[15] =  {0x004C, 0xFCE0, 0xFF5E, 0x0118, 0x0094, 0x0016, 0xFFCB, 0x00C6, 0x0100, 0xFDD4, 0xFF4C, 0x0161, 0xFF64, 0xFFFD, 0x0057};
const uint16 ANC0_FLP_A_SHIFT_1 = 8; //LPF
const uint16 ANC0_FLP_A_SHIFT_2 = 9;
const uint16 ANC0_FLP_B_SHIFT_1 = 8;
const uint16 ANC0_FLP_B_SHIFT_2 = 8;
const uint16 ANC1_FLP_A_SHIFT_1 = 5;
const uint16 ANC1_FLP_A_SHIFT_2 = 5;
const uint16 ANC1_FLP_B_SHIFT_1 = 5;
const uint16 ANC1_FLP_B_SHIFT_2 = 5;
const uint16 DC_FILTER_SHIFT = 7;

#include <operator.h>
#include <vmal.h>
#include <micbias.h>
#include <audio_anc.h>
#include <cap_id_prim.h>
#include <opmsg_prim.h>
#include "kymera.h"

extern void appKymeraExternalAmpSetup(void);
extern void appKymeraExternalAmpControl(bool enable);

extern void appTestAncSetFilters(void);
void appTestAncSetFilters(void)
{
    //Source ADC_A = (Source)PanicFalse(StreamAudioSource(AUDIO_HARDWARE_CODEC, AUDIO_INSTANCE_0, AUDIO_CHANNEL_A));
    Source ADC_A = (Source)PanicFalse(StreamAudioSource(AUDIO_HARDWARE_DIGITAL_MIC, AUDIO_INSTANCE_0, AUDIO_CHANNEL_A));

    /* lLad IIRs for both ANC0 and ANC1 */
    PanicFalse(AudioAncFilterIirSet(AUDIO_ANC_INSTANCE_0, AUDIO_ANC_PATH_ID_FFA, 15, FFA_coefficients));
    PanicFalse(AudioAncFilterIirSet(AUDIO_ANC_INSTANCE_0, AUDIO_ANC_PATH_ID_FFB, 15, FFB_coefficients));
    PanicFalse(AudioAncFilterIirSet(AUDIO_ANC_INSTANCE_0, AUDIO_ANC_PATH_ID_FB, 15, FB_coefficients));
    //PanicFalse(AudioAncFilterIirSet(AUDIO_ANC_INSTANCE_1, AUDIO_ANC_PATH_ID_FFA, 15, FFA_coefficients));
    //PanicFalse(AudioAncFilterIirSet(AUDIO_ANC_INSTANCE_1, AUDIO_ANC_PATH_ID_FFB, 15, FFB_coefficients));
    //PanicFalse(AudioAncFilterIirSet(AUDIO_ANC_INSTANCE_1, AUDIO_ANC_PATH_ID_FB, 15, FB_coefficients));

    /* Set LPF filter shifts */
    PanicFalse(AudioAncFilterLpfSet(AUDIO_ANC_INSTANCE_0, AUDIO_ANC_PATH_ID_FFA, ANC0_FLP_A_SHIFT_1, ANC0_FLP_A_SHIFT_2));
    //PanicFalse(AudioAncFilterLpfSet(AUDIO_ANC_INSTANCE_1, AUDIO_ANC_PATH_ID_FFA, ANC1_FLP_A_SHIFT_1, ANC1_FLP_A_SHIFT_2));

    /* Set DC filters */
    PanicFalse(SourceConfigure(ADC_A, STREAM_ANC_FFA_DC_FILTER_ENABLE, 1));
    PanicFalse(SourceConfigure(ADC_A, STREAM_ANC_FFA_DC_FILTER_SHIFT, DC_FILTER_SHIFT));
    //PanicFalse(SourceConfigure(ADC_B, STREAM_ANC_FFA_DC_FILTER_ENABLE, 1));
    //PanicFalse(SourceConfigure(ADC_B, STREAM_ANC_FFA_DC_FILTER_SHIFT, DC_FILTER_SHIFT));

    /* Set LPF gains */
    PanicFalse(SourceConfigure(ADC_A, STREAM_ANC_FFA_GAIN, 128));
    PanicFalse(SourceConfigure(ADC_A, STREAM_ANC_FFA_GAIN_SHIFT, 0));
    //PanicFalse(SourceConfigure(ADC_B, STREAM_ANC_FFA_GAIN, 128));
    //PanicFalse(SourceConfigure(ADC_B, STREAM_ANC_FFA_GAIN_SHIFT, 0));
}

extern void appTestAncSetup(void);
void appTestAncSetup(void)
{
    const uint16 sample_rate = 48000;
    Source adc_a = NULL, adc_b = NULL;
    Sink dac_l = NULL, dac_r = NULL;

    OperatorFrameworkEnable(1);

    if(appConfigAncFeedForwardMic())
    {
        adc_a = Microphones_TurnOnMicrophone(appConfigAncFeedForwardMic(), sample_rate, high_priority_user);
    }
    if(appConfigAncFeedBackMic())
    {
        adc_b = Microphones_TurnOnMicrophone(appConfigAncFeedBackMic(), sample_rate, high_priority_user);
    }
    SourceSynchronise(adc_a, adc_b);

    /* Get the DAC output sinks */
    dac_l = (Sink)PanicFalse(StreamAudioSink(AUDIO_HARDWARE_CODEC, AUDIO_INSTANCE_0, AUDIO_CHANNEL_A));
    PanicFalse(SinkConfigure(dac_l, STREAM_CODEC_OUTPUT_RATE, sample_rate));
    PanicFalse(SinkConfigure(dac_l, STREAM_CODEC_OUTPUT_GAIN, 12));
    dac_r = (Sink)StreamAudioSink(AUDIO_HARDWARE_CODEC, AUDIO_INSTANCE_0, AUDIO_CHANNEL_B);
    if(dac_r)
    {
        PanicFalse(SinkConfigure(dac_r, STREAM_CODEC_OUTPUT_RATE, sample_rate));
        PanicFalse(SinkConfigure(dac_r, STREAM_CODEC_OUTPUT_GAIN, 12));
    }

    /* Set DAC gains */
    if(dac_l)
    {
        PanicFalse(SinkConfigure(dac_r, STREAM_CODEC_RAW_OUTPUT_GAIN, 7));
    }
    if(dac_r)
    {
        PanicFalse(SinkConfigure(dac_r, STREAM_CODEC_RAW_OUTPUT_GAIN, 7));
    }

    /* feedforward or feedback with analog mics */
    if(dac_l && adc_a)
    {
        PanicFalse(SourceConfigure(adc_a, STREAM_ANC_INSTANCE, AUDIO_ANC_INSTANCE_0));
    }
    if(dac_r && adc_b)
    {
        PanicFalse(SourceConfigure(adc_b, STREAM_ANC_INSTANCE, AUDIO_ANC_INSTANCE_1));
    }
    if(dac_l && adc_a)
    {
        PanicFalse(SourceConfigure(adc_a, STREAM_ANC_INPUT, AUDIO_ANC_PATH_ID_FFA));
    }
    if(dac_r && adc_b)
    {
        PanicFalse(SourceConfigure(adc_b, STREAM_ANC_INPUT, AUDIO_ANC_PATH_ID_FFA));
    }

    /* Associate DACS */
    if(dac_l && adc_a)
    {
        PanicFalse(SinkConfigure(dac_l, STREAM_ANC_INSTANCE, AUDIO_ANC_INSTANCE_0));
    }
    if(dac_r && adc_b)
    {
        PanicFalse(SinkConfigure(dac_r, STREAM_ANC_INSTANCE, AUDIO_ANC_INSTANCE_1));
    }

    /* Setup ANC filters */
    appTestAncSetFilters();

    /* Turn on ANC */
    PanicFalse(AudioAncStreamEnable(0x9, dac_r && adc_b ? 0x09 : 0x0));

    appKymeraExternalAmpSetup();
    appKymeraExternalAmpControl(TRUE);
}

#define GAIN_DB_TO_Q6N_SF (11146541)
#define GAIN_DB(x)      ((int32)(GAIN_DB_TO_Q6N_SF * (x)))

extern void appTestAudioPassthrough(void);
void appTestAudioPassthrough(void)
{
    const uint16 sample_rate = 48000;
    const int32 initial_gain = GAIN_DB(0);

    OperatorFrameworkEnable(1);

    /* Set up MICs */
    Source ADC_A = Microphones_TurnOnMicrophone(appConfigAncFeedForwardMic(), sample_rate, high_priority_user);
    Source ADC_B = Microphones_TurnOnMicrophone(appConfigAncFeedBackMic(), sample_rate, high_priority_user);
    SourceSynchronise(ADC_A, ADC_B);

    /* Get the DAC output sinks */
    Sink DAC_L = (Sink)PanicFalse(StreamAudioSink(AUDIO_HARDWARE_CODEC, AUDIO_INSTANCE_0, AUDIO_CHANNEL_A));
    PanicFalse(SinkConfigure(DAC_L, STREAM_CODEC_OUTPUT_RATE, sample_rate));
    PanicFalse(SinkConfigure(DAC_L, STREAM_CODEC_OUTPUT_GAIN, 12));
    Sink DAC_R = (Sink)StreamAudioSink(AUDIO_HARDWARE_CODEC, AUDIO_INSTANCE_0, AUDIO_CHANNEL_B);
    if (DAC_R)
    {
        PanicFalse(SinkConfigure(DAC_R, STREAM_CODEC_OUTPUT_RATE, sample_rate));
        PanicFalse(SinkConfigure(DAC_R, STREAM_CODEC_OUTPUT_GAIN, 12));
    }

    /* Set DAC gains */
    PanicFalse(SinkConfigure(DAC_L, STREAM_CODEC_RAW_OUTPUT_GAIN, 7));
    if (DAC_R)
        PanicFalse(SinkConfigure(DAC_R, STREAM_CODEC_RAW_OUTPUT_GAIN, 7));

    /* Now create the operator for routing the audio */
    Operator passthrough = (Operator)PanicFalse(VmalOperatorCreate(CAP_ID_BASIC_PASS));

    /* Configure the operators
     * Using the information from CS-312572-UG, Kymera Capability
     * Library User Guide
     */
    uint16 set_gain[] = { OPMSG_COMMON_ID_SET_PARAMS, 1, 1, 1, UINT32_MSW(initial_gain), UINT32_LSW(initial_gain) };
    PanicFalse(VmalOperatorMessage(passthrough, set_gain, 6, NULL, 0));

    /* And connect everything */
    /* ...line_in to the passthrough */
    PanicFalse(StreamConnect(ADC_A, StreamSinkFromOperatorTerminal(passthrough, 0)));
    if (ADC_B && DAC_R)
        PanicFalse(StreamConnect(ADC_B, StreamSinkFromOperatorTerminal(passthrough, 1)));

    /* ...and passthrough to line out */
    PanicFalse(StreamConnect(StreamSourceFromOperatorTerminal(passthrough, 0), DAC_L));
    if (DAC_R)
        PanicFalse(StreamConnect(StreamSourceFromOperatorTerminal(passthrough, 1), DAC_R));

    /* Finally start the operator */
    PanicFalse(OperatorStartMultiple(1, &passthrough, NULL));

    appKymeraExternalAmpSetup();
    appKymeraExternalAmpControl(TRUE);
}



#include <usb_hub.h>

extern void appTestUsbAudioPassthrough(void);
void appTestUsbAudioPassthrough(void)
{
    const uint16 sample_rate = 48000;

    OperatorFrameworkEnable(1);

    /* Attach USB */
    //UsbHubAttach();

    /* Get the DAC output sinks */
    Sink DAC_L = (Sink)PanicFalse(StreamAudioSink(AUDIO_HARDWARE_CODEC, AUDIO_INSTANCE_0, AUDIO_CHANNEL_A));
    PanicFalse(SinkConfigure(DAC_L, STREAM_CODEC_OUTPUT_RATE, sample_rate));
    PanicFalse(SinkConfigure(DAC_L, STREAM_CODEC_OUTPUT_GAIN, 12));
    Sink DAC_R = (Sink)StreamAudioSink(AUDIO_HARDWARE_CODEC, AUDIO_INSTANCE_0, AUDIO_CHANNEL_B);
    if (DAC_R)
    {
        PanicFalse(SinkConfigure(DAC_R, STREAM_CODEC_OUTPUT_RATE, sample_rate));
        PanicFalse(SinkConfigure(DAC_R, STREAM_CODEC_OUTPUT_GAIN, 12));
    }

    /* Set DAC gains */
    PanicFalse(SinkConfigure(DAC_L, STREAM_CODEC_RAW_OUTPUT_GAIN, 7));
    if (DAC_R)
        PanicFalse(SinkConfigure(DAC_R, STREAM_CODEC_RAW_OUTPUT_GAIN, 7));

    /* Now create the operator for routing the audio */
    Operator usb_rx = (Operator)PanicFalse(VmalOperatorCreate(CAP_ID_USB_AUDIO_RX));

    /* Configure the operators
     * Using the information from CS-312572-UG, Kymera Capability
     * Library User Guide
     */
    uint16 set_config[] =
    {
        OPMSG_USB_AUDIO_ID_SET_CONNECTION_CONFIG,
        0,                  // data_format
        sample_rate / 25,   // sample_rate
        1,                  // number_of_channels
        2 * 8,              // subframe_size
        2 * 8               // subframe_resolution
    };

    PanicFalse(VmalOperatorMessage(usb_rx, set_config, 6, NULL, 0));

    /* And connect everything */
    /* ...USB sournce to USB Rx operator */
    PanicFalse(StreamConnect(StreamUsbEndPointSource(end_point_iso_in), StreamSinkFromOperatorTerminal(usb_rx, 0)));

    /* ...and USB Rx operator to line out */
    PanicFalse(StreamConnect(StreamSourceFromOperatorTerminal(usb_rx, 0), DAC_L));
    if (DAC_R)
        PanicFalse(StreamConnect(StreamSourceFromOperatorTerminal(usb_rx, 1), DAC_R));

    /* Finally start the operator */
    PanicFalse(OperatorStartMultiple(1, &usb_rx, NULL));

    appKymeraExternalAmpSetup();
    appKymeraExternalAmpControl(TRUE);
}

#define ANC_TUNING_SINK_USB_LEFT      0 /*can be any other backend device. PCM used in this tuning graph*/
#define ANC_TUNING_SINK_MIC1_LEFT     4 /* must be connected to internal ADC. Analog or digital */

extern void appTestAncTuningSetSource(void);
void appTestAncTuningSetSource(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    uint16 set_source_control[8] =
    {
        0x2002, 2,
        6, 0, ANC_TUNING_SINK_MIC1_LEFT,
        8, 0, ANC_TUNING_SINK_USB_LEFT,
    };
    PanicFalse(VmalOperatorMessage(theKymera->anc_tuning,
                                   &set_source_control, SIZEOF_OPERATOR_MESSAGE(set_source_control),
                                   NULL, 0));
}

#endif /* INCLUDE_EXTRA_TESTS */

bool appTestEnterDfuWhenEnteringCase(bool unused)
{
    bool enter_dfu_logical_input_sent = FALSE;

    DEBUG_LOG_ALWAYS("appTestEnterDfuWhenEnteringCase");

    UNUSED(unused);

    if (appPeerSigIsConnected() && appSmIsOutOfCase())
    {
        MessageSend(LogicalInputSwitch_GetTask(), LI_MFB_BUTTON_RELEASE_DFU, NULL);
        enter_dfu_logical_input_sent = TRUE;
    }

    DEBUG_LOG_ALWAYS("appTestEnterDfuWhenEnteringCase cmd_sent=%d", enter_dfu_logical_input_sent);

    return enter_dfu_logical_input_sent;
}

bool appTestIsInitialisationCompleted(void)
{
    bool completed = SystemState_GetState() > system_state_initialised;

    DEBUG_LOG_ALWAYS("appTestIsInitialisationCompleted:%d", completed);

    return completed;
}

bool appTestIsPrimary(void)
{
    bool prim = appSmIsPrimary();

    DEBUG_LOG_ALWAYS("appTestIsPrimary: %d",prim);

    return prim;
}

bool appTestIsRight(void)
{
    bool right = appConfigIsRight();

    DEBUG_LOG_ALWAYS("appTestIsRight: %d", right);

    return right;
}

bool appTestIsTopologyRole(app_test_topology_role_enum_t checked_role)
{
    bool role_matches = FALSE;
    tws_topology_role role = TwsTopology_GetRole();

    switch (checked_role)
    {
        case app_test_topology_role_none:
            role_matches = (role == tws_topology_role_none);
            DEBUG_LOG_ALWAYS("appTestIsTopologyRole. No role:%d", role_matches);
            break;

        case app_test_topology_role_any_primary:
            role_matches = (role == tws_topology_role_primary);
            DEBUG_LOG_ALWAYS("appTestIsTopologyRole. Primary:%d (Acting:%d)",
                            role_matches, TwsTopology_IsActingPrimary());
            break;

        case app_test_topology_role_primary:
            role_matches = TwsTopology_IsFullPrimary();
            DEBUG_LOG_ALWAYS("appTestIsTopologyRole. Primary(Full):%d", role_matches);
            break;

        case app_test_topology_role_acting_primary:
            role_matches = TwsTopology_IsActingPrimary();
            DEBUG_LOG_ALWAYS("appTestIsTopologyRole. Acting Primary:%d", role_matches);
            break;

        case app_test_topology_role_secondary:
            role_matches = (role == tws_topology_role_secondary);
            DEBUG_LOG_ALWAYS("appTestIsTopologyRole. Secondary:%d", role_matches);
            break;

        default:
            DEBUG_LOG_ALWAYS("appTestIsTopologyRole. Unsupported role:%d",checked_role);
            Panic();
            break;
    }

    return role_matches;
}


bool appTestIsTopologyIdle(void)
{
    bool idle = (TwsTopology_GetRole() == app_test_topology_role_none);

    DEBUG_LOG_ALWAYS("appTestIsTopologyIdle:%d", idle);

    return idle;
}


bool appTestIsTopologyRunning(void)
{
    bool running = twsTopology_IsRunning();

    DEBUG_LOG_ALWAYS("appTestIsTopologyRunning:%d", running);

    return running;
}


/*! List of application states to be included in specific debug.

    Macros are used as it allows us to define a string for log
    purposes, and create an entry in a switch statement while
    keeping the two synchronised.
 */
#define NAMED_STATES(APP_S) \
    APP_S(APP_STATE_STARTUP) \
    APP_S(APP_STATE_HANDSET_PAIRING) \
    APP_S(APP_STATE_IN_CASE_IDLE) \
    APP_S(APP_STATE_IN_CASE_DFU) \
    APP_S(APP_STATE_OUT_OF_CASE_IDLE) \
    APP_S(APP_STATE_IN_EAR_IDLE)

/*! Macro to create a hydra logging string for each state */
#define HYD_STRING(_state) HYDRA_LOG_STRING(HLS_STATE_NAME_ ## _state, #_state);
/*! Macro to create a hydra logging string for each state */
#define STATE_CASE(_state) case _state: state_name = HLS_STATE_NAME_ ## _state; break;

bool appTestIsApplicationState(appState checked_state)
{
    bool state_matches;
    appState state = appSmGetState();
    const char *state_name;

    NAMED_STATES(HYD_STRING)

    state_matches = state == checked_state;

    switch (checked_state)
    {
        NAMED_STATES(STATE_CASE)

        default:
            DEBUG_LOG_ALWAYS("appTestIsApplicationState. State:x%x:%d (state is x%x)",
                            checked_state, state_matches, state);
            return state_matches;
    }

    if (state_matches)
    {
        DEBUG_LOG_ALWAYS("appTestIsApplicationState. %s:%d", state_name, state_matches);
    }
    else
    {
        DEBUG_LOG_ALWAYS("appTestIsApplicationState. %s:%d (state is x%x)",
                            state_name, state_matches, state);
    }
    return state_matches;
}


bool appTestIsPeerFindRoleRunning(void)
{
    bool running = peer_find_role_is_running();

    DEBUG_LOG_ALWAYS("appTestIsPeerFindRoleRunning:%d", running);

    return running;
}


bool appTestIsPeerPairLeRunning(void)
{
    bool running = PeerPairLeIsRunning();

    DEBUG_LOG_ALWAYS("appTestIsPeerPairLeRunning:%d", running);

    return running;
}


#ifdef INCLUDE_EXTRA_TESTS
static void earbudTest_ReportProperty(device_t device, device_property_t property)
{
    if (Device_IsPropertySet(device,property))
    {
        uint8 value = 0;
        uint16 u16_value = 0;
        uint32 u32_value = 0;
        switch (property)
        {
        case device_property_bdaddr:
            {
                bdaddr bd_addr = DeviceProperties_GetBdAddr(device);
                DEBUG_LOG_VERBOSE("\tbdaddr nap %x uap %x lap %x", bd_addr.nap, bd_addr.uap, bd_addr.lap);
            }
            break;
        case device_property_audio_volume:
            Device_GetPropertyU8(device, property, &value);
            DEBUG_LOG_VERBOSE("\taudio_volume %02x", value);
            break;
        case device_property_audio_source:
            Device_GetPropertyU8(device, property, &value);
            DEBUG_LOG_VERBOSE("\taudio_source %02x", value);
            break;
        case device_property_av_instance:
            DEBUG_LOG_VERBOSE("\tav_instance %08x", Av_InstanceFindFromDevice(device));
            break;
        case device_property_hfp_profile:
            Device_GetPropertyU8(device, property, &value);
            DEBUG_LOG_VERBOSE("\thfp_profile %02x", value);
            break;
        case device_property_type:
            Device_GetPropertyU8(device, property, &value);
            DEBUG_LOG_VERBOSE("\ttype %02x", value);
            break;
        case device_property_link_mode:
            Device_GetPropertyU8(device, property, &value);
            DEBUG_LOG_VERBOSE("\tlink_mode %02x", value);
            break;
        case device_property_supported_profiles:
            Device_GetPropertyU32(device, property, &u32_value);
            DEBUG_LOG_VERBOSE("\tsupported_profiles %08x", u32_value);
            break;
        case device_property_connected_profiles:
            Device_GetPropertyU32(device, property, &u32_value);
            DEBUG_LOG_VERBOSE("\tcurrently connected_profiles %08x", u32_value);
            break;
        case device_property_flags:
            Device_GetPropertyU16(device, property, &u16_value);
            DEBUG_LOG_VERBOSE("\tflags %04x", u16_value);
            break;
        case device_property_sco_fwd_features:
            Device_GetPropertyU16(device, property, &u16_value);
            DEBUG_LOG_VERBOSE("\tsco_fwd_features %04x", u16_value);
            break;
        case device_property_mru:
            Device_GetPropertyU8(device, property, &value);
            DEBUG_LOG_VERBOSE("\tmru %1x", value);
            break;
        case device_property_profile_request_index:
            Device_GetPropertyU8(device, property, &value);
            DEBUG_LOG_VERBOSE("\tprofile_request_index %1x", value);
            break;
        case device_property_profiles_connect_order:
            {
                profile_t *ptr_profile_order = NULL;
                size_t size;
                PanicFalse(Device_GetProperty(device, property, (void *)&ptr_profile_order, &size));
                profile_t array[5] = {profile_manager_bad_profile};
                memcpy(array, ptr_profile_order, MIN(size,5));
                DEBUG_LOG_VERBOSE("\tprofiles_connect_order %d,%d,%d,%d,%d", array[0], array[1], array[2], array[3], array[4]);
            }
            break;
        case `:
            Device_GetPropertyU8(device, property, &value);
            DEBUG_LOG_VERBOSE("\tvoice_volume %02x", value);
            break;
        case device_property_va_flags:
            Device_GetPropertyU8(device, property, &value);
            DEBUG_LOG_VERBOSE("\tva_flags %02x", value);
            break;
        case device_property_va_locale:
            {
                uint8 va_locale[4] = { '#','#','#','#' };
                void *ptr;
                size_t size;
                if( Device_GetProperty(device, property, &ptr, &size) )
                {
                    memcpy(va_locale, ptr, MIN(size,sizeof(va_locale)));
                    for ( size_t i = 0 ; i < sizeof(va_locale) ; i++ )
                    {
                        if ( va_locale[i] == '\0' ) va_locale[i] = '.';
                        if ( !isprint(va_locale[i]) ) va_locale[i] = '?';
                    }
                }
                DEBUG_LOG_VERBOSE("\tva_locale \"%c%c%c%c\"",
                                  va_locale[0], va_locale[1], va_locale[2], va_locale[3]);
            }
            break;
        case device_property_voice_assistant:
            Device_GetPropertyU8(device, property, &value);
            DEBUG_LOG_VERBOSE("\tvoice_assistant %02x", value);
            break;
        default:
            break;
        }
    }
}

static void earbudTest_ReportDeviceData(device_t device, void * data)
{
    UNUSED(data);

    DEBUG_LOG_ALWAYS("Device %x", device);
    for (int property=0; property < device_property_max_num; property++)
    {
        if (Device_IsPropertySet(device, property))
        {
            earbudTest_ReportProperty(device, property);
        }
    }
}

void EarbudTest_DeviceDatabaseReport(void)
{
    DEBUG_LOG_ALWAYS("DeviceDatabase");
    DeviceList_Iterate(earbudTest_ReportDeviceData, NULL);
}
#endif

extern uint8 profile_list[4];

static device_t earbudTest_GetHandset(void)
{
    bool is_mru_handset = TRUE;
    device_t handset_device = DeviceList_GetFirstDeviceWithPropertyValue(device_property_mru, &is_mru_handset, sizeof(uint8));
    if (!handset_device)
    {
        bdaddr handset_address = {0,0,0};
        if (appDeviceGetHandsetBdAddr(&handset_address))
        {
            handset_device = BtDevice_GetDeviceForBdAddr(&handset_address);
        }
    }
    return handset_device;
}

void EarbudTest_ConnectHandset(void)
{
    DEBUG_LOG_ALWAYS("EarbudTest_ConnectHandset");
    appSmConnectHandset();
}

void EarbudTest_DisconnectHandset(void)
{
    DEBUG_LOG_ALWAYS("EarbudTest_DisconnectHandset");
    EarbudTest_DisconnectHandsetExcept(0);
}

void EarbudTest_DisconnectHandsetExcept(uint32 profiles)
{
    bdaddr handset_addr = {0};

    if (appDeviceGetHandsetBdAddr(&handset_addr))
    {
        DEBUG_LOG_ALWAYS("EarbudTest_DisconnectHandsetExcept lap=%6x, except 0x%08x", handset_addr.lap, profiles);

        HandsetService_DisconnectRequest(SmGetTask(), &handset_addr, profiles);
    }
}

bool appTestIsHandsetFullyConnected(void)
{
    bool connected = FALSE;
    device_t handset_device = earbudTest_GetHandset();

    if (handset_device)
    {
        connected = HandsetService_Connected(handset_device);
        DEBUG_LOG_ALWAYS("appTestIsHandsetFullyConnected:%d", connected);
    }
    else
    {
        DEBUG_LOG_ALWAYS("appTestIsHandsetFullyConnected:0  No handset device");
    }
    return connected;
}

bool appTestIsHandsetAclConnected(void)
{
    bool connected = FALSE;
    device_t handset_device = earbudTest_GetHandset();

    if (handset_device)
    {
        bdaddr addr = DeviceProperties_GetBdAddr(handset_device);
        connected = ConManagerIsConnected(&addr);
        DEBUG_LOG_ALWAYS("appTestIsHandsetAclConnected:%d 0x%06x", connected, addr.lap);
    }
    else
    {
        DEBUG_LOG_ALWAYS("appTestIsHandsetAclConnected:0 No handset device/addr");
    }
    return connected;
}


bool appTestPrimaryAddressIsFromThisBoard(void)
{
    bdaddr self;
    uint16 flags;
    bool is_primary = FALSE;

    HYDRA_LOG_STRING(mine, "MINE");
    HYDRA_LOG_STRING(peers, "PEERS");


    appDeviceGetMyBdAddr(&self);

    if (appDeviceGetFlags(&self, &flags))
    {
        if (flags & DEVICE_FLAGS_MIRRORING_C_ROLE)
        {
            is_primary = TRUE;
        }
    }

    DEBUG_LOG_ALWAYS("appTestPrimaryAddressIsFromThisBoard %04x%02x%06x %s",
            self.nap, self.uap, self.lap,
            is_primary ? mine : peers);

    return is_primary;
}

static void earbudTest_RestartFindRole(void)
{
    appTestPhyStateInCaseEvent();
    MessageCancelAll((Task) &delayedInternalTestEventHandlerTask, WAIT_FOR_IDLE_THEN_BRING_OUT_OF_CASE);
    MessageSendLater((Task) &delayedInternalTestEventHandlerTask, WAIT_FOR_IDLE_THEN_BRING_OUT_OF_CASE,
                                         NULL, 100);
}

void EarbudTest_PeerFindRoleOverrideScore(uint16 score)
{
    DEBUG_LOG_ALWAYS("EarbudTest_PeerFindRoleOverrideScore 0x%x", score);
    PeerFindRole_OverrideScore((grss_figure_of_merit_t)score);
    if(!appTestIsTopologyIdle())
    {
        earbudTest_RestartFindRole();
    }
}

bool EarbudTest_IsPeerSignallingConnected(void)
{
    return appPeerSigIsConnected();
}

bool EarbudTest_IsPeerSignallingDisconnected(void)
{
    return appPeerSigIsDisconnected();
}

#ifdef INCLUDE_HDMA_RSSI_EVENT
const hdma_thresholds_t *appTestGetRSSIthreshold(void)
{
    DEBUG_LOG_VERBOSE("halfLife: critical [%d] high [%d], low [%d]", rssi.halfLife_ms.critical, rssi.halfLife_ms.high, rssi.halfLife_ms.low);
    DEBUG_LOG_VERBOSE("maxAge: critical [%d] high [%d], low [%d]", rssi.maxAge_ms.critical, rssi.maxAge_ms.high, rssi.maxAge_ms.low);
    DEBUG_LOG_VERBOSE("absThreshold: critical [%d] high [%d], low [%d]", rssi.absThreshold.critical, rssi.absThreshold.high, rssi.absThreshold.low);
    DEBUG_LOG_VERBOSE("relThreshold: critical [%d] high [%d], low [%d]", rssi.relThreshold.critical, rssi.relThreshold.high, rssi.relThreshold.low);
    return &rssi;
}
#endif

#ifdef INCLUDE_HDMA_MIC_QUALITY_EVENT
const hdma_thresholds_t *appTestGetMICthreshold(void)
{
    DEBUG_LOG_VERBOSE("halfLife: critical [%d] high [%d], low [%d]", mic.halfLife_ms.critical, mic.halfLife_ms.high, mic.halfLife_ms.low);
    DEBUG_LOG_VERBOSE("maxAge: critical [%d] high [%d], low [%d]", mic.maxAge_ms.critical, mic.maxAge_ms.high, mic.maxAge_ms.low);
    DEBUG_LOG_VERBOSE("absThreshold: critical [%d] high [%d], low [%d]", mic.absThreshold.critical, mic.absThreshold.high, mic.absThreshold.low);
    DEBUG_LOG_VERBOSE("relThreshold: critical [%d] high [%d], low [%d]", mic.relThreshold.critical, mic.relThreshold.high, mic.relThreshold.low);
    return &mic;
}
#endif


/* private API added to upgrade library */
bool appTestUpgradeResetState(void)
{
    DEBUG_LOG_ALWAYS("appTestUpgradeResetState");

    return Dfu_ClearPsStore();
}

#ifdef INCLUDE_DFU
void EarbudTest_EnterInCaseDfu(void)
{
    DEBUG_LOG_ALWAYS("EarbudTest_EnterInCaseDfu");

    appSmEnterDfuModeInCase(TRUE, TRUE);
    appTestPhyStateInCaseEvent();
}
#endif

uint16 appTestSetTestNumber(uint16 test_number)
{
    test_support.testcase = test_number;

    return test_number;
}

uint16 appTestSetTestIteration(uint16 test_iteration)
{
    test_support.iteration = test_iteration;

    return test_iteration;
}

uint16 appTestWriteMarker(uint16 marker)
{
    static unsigned testcase = 0;
    test_support.last_marker = marker;

    if (   test_support.testcase
        && (  test_support.testcase != testcase
           || marker == 0))
    {
        testcase = test_support.testcase;

        if (test_support.iteration)
        {
            DEBUG_LOG_VERBOSE("@@@Testcase:%d  Iteration:%d -------------------------------", testcase, test_support.iteration);
        }
        else
        {
            DEBUG_LOG_VERBOSE("@@@Testcase:%d  ------------------------------------------", testcase);
        }
    }

    if (marker)
    {
        if (test_support.testcase && test_support.iteration)
        {
            DEBUG_LOG_VERBOSE("@@@Testcase marker: TC%d Iteration:%d Step:%d *************************",
                    testcase, test_support.iteration, marker);
        }
        else if (test_support.testcase)
        {
            DEBUG_LOG_VERBOSE("@@@Testcase marker: TC%d Step:%d *************************",
                    testcase, marker);
        }
        else
        {
            DEBUG_LOG_VERBOSE("@@@Testcase marker: Iteration:%d Step:%d *************************",
                    test_support.iteration, marker);
        }
    }

    return marker;
}


void appTestCvcPassthrough(void)
{
    DEBUG_LOG_ALWAYS("appTestCvcPassthrough");
    Kymera_ScoSetCvcPassthroughMode(KYMERA_CVC_RECEIVE_PASSTHROUGH | KYMERA_CVC_SEND_PASSTHROUGH, 0);
}

void EarbudTest_SetAncEnable(void)
{
    if (appPhyStateIsOutOfCase())
    {
        DEBUG_LOG_ALWAYS("EarbudTest_SetAncEnable");
        Ui_InjectUiInput(ui_input_anc_on);
    }
}

void EarbudTest_SetAncDisable(void)
{
    if (appPhyStateIsOutOfCase())
    {
        DEBUG_LOG_ALWAYS("EarbudTest_SetAncDisable");
        Ui_InjectUiInput(ui_input_anc_off);
    }
}

void EarbudTest_SetAncToggleOnOff(void)
{
    if (appPhyStateIsOutOfCase())
    {
        DEBUG_LOG_ALWAYS("EarbudTest_SetAncToggleOnOff");
        Ui_InjectUiInput(ui_input_anc_toggle_on_off);
    }
}


void EarbudTest_AncToggleDiagnostic(void)
{
    if (appPhyStateIsOutOfCase())
    {
        DEBUG_LOG_ALWAYS("EarbudTest_AncToggleDiagnostic");
        Ui_InjectUiInput(ui_input_anc_toggle_diagnostic);
    }
}

bool EarbudTest_AncWriteFineGain(audio_anc_path_id gain_path, uint8 gain)
{
    bool status = FALSE;

    if(appPhyStateIsOutOfCase())
    {
        DEBUG_LOG_ALWAYS("EarbudTest_AncWriteFineGain");
        status = AncWriteFineGain(gain_path, gain);
    }
    return status;
}

void EarbudTest_SetAncMode(anc_mode_t mode)
{
    if (appPhyStateIsOutOfCase())
    {
        DEBUG_LOG_ALWAYS("EarbudTest_SetAncMode");
        switch(mode)
        {
            case anc_mode_1:
                Ui_InjectUiInput(ui_input_anc_set_mode_1);
                break;
            case anc_mode_2:
                Ui_InjectUiInput(ui_input_anc_set_mode_2);
                break;
            case anc_mode_3:
                Ui_InjectUiInput(ui_input_anc_set_mode_3);
                break;
            case anc_mode_4:
                Ui_InjectUiInput(ui_input_anc_set_mode_4);
                break;
            case anc_mode_5:
                Ui_InjectUiInput(ui_input_anc_set_mode_5);
                break;
            case anc_mode_6:
                Ui_InjectUiInput(ui_input_anc_set_mode_6);
                break;
            case anc_mode_7:
                Ui_InjectUiInput(ui_input_anc_set_mode_7);
                break;
            case anc_mode_8:
                Ui_InjectUiInput(ui_input_anc_set_mode_8);
                break;
            case anc_mode_9:
                Ui_InjectUiInput(ui_input_anc_set_mode_9);
                break;
            case anc_mode_10:
                Ui_InjectUiInput(ui_input_anc_set_mode_10);
                break;
            default:
                Ui_InjectUiInput(ui_input_anc_set_mode_1);
                break;
        }
    }
}

void EarbudTest_SetNextAncMode(void)
{
    if (appPhyStateIsOutOfCase())
    {
        DEBUG_LOG("EarbudTest_SetNextAncMode");
        Ui_InjectUiInput(ui_input_anc_set_next_mode);
    }
}

void EarbudTest_SetAncLeakthroughGain(uint8 gain)
{
    if(appPhyStateIsOutOfCase())
    {
        DEBUG_LOG("EarbudTest_SetAncLeakthroughGain %d", gain);
        AncStateManager_StoreAncLeakthroughGain(gain);
        Ui_InjectUiInput(ui_input_anc_set_leakthrough_gain);
    }
}

void EarbudTest_SetAncToggleConfig1(uint8 config)
{
    DEBUG_LOG("EarbudTest_SetAncToggleConfig1");
    AncStateManager_SetAncToggleConfiguration(anc_toggle_way_config_id_1, config);
}

void EarbudTest_SetAncToggleConfig2(uint8 config)
{
    DEBUG_LOG("EarbudTest_SetAncToggleConfig2");
    AncStateManager_SetAncToggleConfiguration(anc_toggle_way_config_id_2, config);
}

void EarbudTest_SetAncToggleConfig3(uint8 config)
{
    DEBUG_LOG("EarbudTest_SetAncToggleConfig3");
    AncStateManager_SetAncToggleConfiguration(anc_toggle_way_config_id_3, config);
}

void EarbudTest_SetAncConfigDuringStandalone(uint8 config)
{
    DEBUG_LOG("EarbudTest_SetAncConfigDuringStandalone");
    AncStateManager_SetAncScenarioConfiguration(anc_scenario_config_id_standalone, config);
}

void EarbudTest_SetAncConfigDuringPlayback(uint8 config)
{
    DEBUG_LOG("EarbudTest_SetAncConfigDuringPlayback");
    AncStateManager_SetAncScenarioConfiguration(anc_scenario_config_id_playback, config);
}

void EarbudTest_SetAncConfigDuringSco(uint8 config)
{
    DEBUG_LOG("EarbudTest_SetAncConfigDuringSco");
    AncStateManager_SetAncScenarioConfiguration(anc_scenario_config_id_sco, config);
}

void EarbudTest_SetAncConfigDuringVa(uint8 config)
{
    DEBUG_LOG("EarbudTest_SetAncConfigDuringVa");
    AncStateManager_SetAncScenarioConfiguration(anc_scenario_config_id_va, config);
}

void EarbudTest_SetAncDemoState(bool demo_state)
{
    DEBUG_LOG("EarbudTest_SetAncDemoState");
    AncStateManager_SetDemoState(demo_state);
}

void EarbudTest_ToggleAncAdaptivity(void)
{
    if (appPhyStateIsOutOfCase())
    {
        DEBUG_LOG("EarbudTest_ToggleAncAdaptivity");
        Ui_InjectUiInput(ui_input_anc_adaptivity_toggle_on_off);
    }
}

void EarbudTest_ToggleAncConfig(void)
{
    if (appPhyStateIsOutOfCase())
    {
        DEBUG_LOG("EarbudTest_ToggleAncConfig");
        Ui_InjectUiInput(ui_input_anc_toggle_way);
    }
}

bool EarbudTest_GetAncstate(void)
{
     DEBUG_LOG_ALWAYS("EarbudTest_GetAncstate");
     return AncStateManager_IsEnabled();

}

anc_mode_t EarbudTest_GetAncMode(void)
{
     DEBUG_LOG_ALWAYS("EarbudTest_GetAncMode");
     return (AncStateManager_GetMode());
}

uint8 EarbudTest_GetAncLeakthroughGain(void)
{
    DEBUG_LOG("EarbudTest_GetAncLeakthroughGain");
    return AncStateManager_GetAncGain();
}

uint8 EarbudTest_GetAncToggleConfig1(void)
{
     DEBUG_LOG("EarbudTest_GetAncToggleConfig1");
     return AncStateManager_GetAncToggleConfiguration(anc_toggle_way_config_id_1);
}

uint8 EarbudTest_GetAncToggleConfig2(void)
{
     DEBUG_LOG("EarbudTest_GetAncToggleConfig2");
     return AncStateManager_GetAncToggleConfiguration(anc_toggle_way_config_id_2);
}

uint8 EarbudTest_GetAncToggleConfig3(void)
{
     DEBUG_LOG("EarbudTest_GetAncToggleConfig3");
     return AncStateManager_GetAncToggleConfiguration(anc_toggle_way_config_id_3);
}

uint8 EarbudTest_GetAncConfigDuringStandalone(void)
{
     DEBUG_LOG("EarbudTest_GetAncConfigDuringStandalone");
     return AncStateManager_GetAncScenarioConfiguration(anc_scenario_config_id_standalone);
}

uint8 EarbudTest_GetAncConfigDuringPlayback(void)
{
     DEBUG_LOG("EarbudTest_GetAncConfigDuringPlayback");
     return AncStateManager_GetAncScenarioConfiguration(anc_scenario_config_id_playback);
}

uint8 EarbudTest_GetAncConfigDuringSco(void)
{
     DEBUG_LOG("EarbudTest_GetAncConfigDuringSco");
     return AncStateManager_GetAncScenarioConfiguration(anc_scenario_config_id_sco);
}

uint8 EarbudTest_GetAncConfigDuringVa(void)
{
     DEBUG_LOG("EarbudTest_GetAncConfigDuringVa");
     return AncStateManager_GetAncScenarioConfiguration(anc_scenario_config_id_va);
}

bool EarbudTest_GetAncDemoState(void)
{
     DEBUG_LOG("EarbudTest_GetAncDemoState");
     return AncStateManager_IsDemoStateActive();
}

bool EarbudTest_GetAncAdaptivity(void)
{
     DEBUG_LOG("EarbudTest_GetAncAdaptivity");
     return AncStateManager_GetAdaptiveAncAdaptivity();
}

void EarbudTest_StartAncTuning(void)
{
    /* Check if Static ANC tuning is already running */
    if(AncStateManager_IsTuningModeActive())
    {
        DEBUG_LOG_INFO("Static ANC tuning is already in progress");
    }
    else
    {
        /* Check if Adaptive ANC tuning is running. If yes, Stop it before starting Static ANC tuning */
        if(AncStateManager_IsAdaptiveAncTuningModeActive())
        {
            DEBUG_LOG_INFO("Destroying Adaptive ANC tuning chain as Adaptive ANC tuning is in progress");
            AncStateManager_ExitAdaptiveAncTuningMode();
        }

        /* Start ANC tuing only if appConfigAncTuningEnabled() is set to TRUE and device is incase */
        if(appConfigAncTuningEnabled() && appPhyStateGetState() == PHY_STATE_IN_CASE)
        {
            DEBUG_LOG_DEBUG("EarbudTest_StartAncTuning");
            Ui_InjectUiInput(ui_input_anc_enter_tuning_mode);
        }
    }
}

void EarbudTest_StopAncTuning(void)
{
    DEBUG_LOG_FN_ENTRY("EarbudTest_StopAncTuning");
    Ui_InjectUiInput(ui_input_anc_exit_tuning_mode);
}

void EarbudTest_StartAdaptiveAncTuning(void)
{
    /* Check if Adaptive ANC tuning is already running */
    if(AncStateManager_IsAdaptiveAncTuningModeActive())
    {
        DEBUG_LOG_INFO("Adaptive ANC tuning is already in progress");
    }
    else
    {
        /* Check if Static ANC tuning is running. If yes, Stop it before starting Adaptive ANC tuning */
        if(AncStateManager_IsTuningModeActive())
        {
            DEBUG_LOG_INFO("Destroying Static ANC tuning chain as Static ANC tuning is in progress");
            AncStateManager_ExitAncTuningMode();
        }

        /* Start AANC tuing only if appConfigAncTuningEnabled() is set to TRUE and device is incase */
        if(appConfigAncTuningEnabled() && appPhyStateGetState() == PHY_STATE_IN_CASE)
        {
            DEBUG_LOG_DEBUG("EarbudTest_StartAdaptiveAncTuning");
            Ui_InjectUiInput(ui_input_anc_enter_adaptive_anc_tuning_mode);
        }
    }
}

void EarbudTest_StopAdaptiveAncTuning(void)
{
    DEBUG_LOG_FN_ENTRY("EarbudTest_StopAdaptiveAncTuning");
    Ui_InjectUiInput(ui_input_anc_exit_adaptive_anc_tuning_mode);
}

bool EarbudTest_OverrideA2dpLatency(uint32 latency_ms)
{
    return Kymera_LatencyManagerSetOverrideLatency(latency_ms);
}

/* void EarbudTest_GAIACommandGetAncState(void)
{
     DEBUG_LOG_ALWAYS("EarbudTest_GAIACommandGetAncState");
     earbudTest_SendGaiaCommandToAnc(anc_gaia_get_anc_state, 1, 0);
}

void EarbudTest_GAIACommandSetAncEnable(void)
{
     DEBUG_LOG_ALWAYS("EarbudTest_GAIACommandSetAncEnable");
     earbudTest_SendGaiaCommandToAnc(anc_gaia_set_anc_state, ANC_GAIA_SET_ANC_STATE_PAYLOAD_LENGTH, ANC_GAIA_SET_ANC_STATE_ENABLE);
}

void EarbudTest_GAIACommandSetAncDisable(void)
{
     DEBUG_LOG_ALWAYS("EarbudTest_GAIACommandSetAncDisable");
     earbudTest_SendGaiaCommandToAnc(anc_gaia_set_anc_state, ANC_GAIA_SET_ANC_STATE_PAYLOAD_LENGTH, ANC_GAIA_SET_ANC_STATE_DISABLE);
}

void EarbudTest_GAIACommandGetAncNumOfModes(void)
{
     DEBUG_LOG_ALWAYS("EarbudTest_GAIACommandGetAncNumOfModes");
     earbudTest_SendGaiaCommandToAnc(anc_gaia_get_num_anc_modes, 1, 0);
}

void EarbudTest_GAIACommandSetAncMode(uint8 mode)
{
     DEBUG_LOG_ALWAYS("EarbudTest_GAIACommandSetAncMode");
     earbudTest_SendGaiaCommandToAnc(anc_gaia_set_anc_mode, ANC_GAIA_SET_ANC_MODE_PAYLOAD_LENGTH, mode);
}

void EarbudTest_GAIACommandGetAncLeakthroughGain(void)
{
     DEBUG_LOG_ALWAYS("EarbudTest_GAIACommandGetAncLeakthroughGain");
     earbudTest_SendGaiaCommandToAnc(anc_gaia_get_configured_leakthrough_gain, 1, 0);
}

void EarbudTest_GAIACommandSetAncLeakthroughGain(uint8 gain)
{
     DEBUG_LOG_ALWAYS("EarbudTest_GAIACommandSetAncLeakthroughGain");
     earbudTest_SendGaiaCommandToAnc(anc_gaia_set_configured_leakthrough_gain, ANC_GAIA_SET_ANC_LEAKTHROUGH_GAIN_PAYLOAD_LENGTH, gain);
} */

void EarbudTest_EnableLeakthrough(void)
{
    if(appPhyStateIsOutOfCase())
    {
        DEBUG_LOG("EarbudTest_EnableLeakthrough");
        Ui_InjectUiInput(ui_input_leakthrough_on);
    }
}

void EarbudTest_DisableLeakthrough(void)
{
    if(appPhyStateIsOutOfCase())
    {
        DEBUG_LOG("EarbudTest_DisableLeakthrough");
        Ui_InjectUiInput(ui_input_leakthrough_off);
    }
}

void EarbudTest_ToggleLeakthroughOnOff(void)
{
    if(appPhyStateIsOutOfCase())
    {
        DEBUG_LOG_ALWAYS("EarbudTest_LeakthroughToggleOnOff");
        Ui_InjectUiInput(ui_input_leakthrough_toggle_on_off);
    }
}

void EarbudTest_SetLeakthroughMode(leakthrough_mode_t leakthrough_mode)
{
    if(appPhyStateIsOutOfCase())
    {
        DEBUG_LOG("EarbudTest_SetLeakthroughMode");
        switch(leakthrough_mode)
        {
            case LEAKTHROUGH_MODE_1:
                Ui_InjectUiInput(ui_input_leakthrough_set_mode_1);
                break;
            case LEAKTHROUGH_MODE_2:
                Ui_InjectUiInput(ui_input_leakthrough_set_mode_2);
                break;
            case LEAKTHROUGH_MODE_3:
                Ui_InjectUiInput(ui_input_leakthrough_set_mode_3);
                break;
            default:
                DEBUG_LOG("Invalid value of leakthrough mode is passed");
                break;
        }
    }
}

void EarbudTest_SetNextLeakthroughMode(void)
{
    if(appPhyStateIsOutOfCase())
    {
        Ui_InjectUiInput(ui_input_leakthrough_set_next_mode);
    }
}

leakthrough_mode_t EarbudTest_GetLeakthroughMode(void)
{
    DEBUG_LOG("EarbudTest_GetLeakthroughMode");
    return AecLeakthrough_GetMode();
}

bool EarbudTest_IsLeakthroughEnabled(void)
{
    DEBUG_LOG("EarbudTest_IsLeakthroughEnabled");
    return AecLeakthrough_IsLeakthroughEnabled();
}

void appTestSetFixedRole(peer_find_role_fixed_role_t role)
{
    DEBUG_LOG_ALWAYS("appTestSetFixedRole role=%d", role);
    PeerFindRole_SetFixedRole(role);
}

#ifdef INCLUDE_FAST_PAIR
/* Refer comments given in the function prototype to configure fast pair model ID.
*/
void appTestSetFPModelID(uint32 fast_pair_model_id)
{
    DEBUG_LOG("appTestSetFPModelID");

    uint32 buffer = fast_pair_model_id;

    /* Size of Model ID to be passed to PsStore API should be in words (2 words) */
    PanicFalse(PsStore(PS_KEY_FAST_PAIR_MODEL_ID, &buffer, FAST_PAIR_CONFIG_MODEL_ID_LEN/2));
}

/* Refer comments given in the function prototype to configure scrambled aspk.
*/
void appTestSetFPASPK(uint32 dword1, uint32 dword2, uint32 dword3, uint32 dword4,
                          uint32 dword5, uint32 dword6, uint32 dword7, uint32 dword8)
{
    DEBUG_LOG("appTestSetFPASPK");
    uint32 aspk [8];

    aspk[0] = dword1;
    aspk[1] = dword2;
    aspk[2] = dword3;
    aspk[3] = dword4;
    aspk[4] = dword5;
    aspk[5] = dword6;
    aspk[6] = dword7;
    aspk[7] = dword8;

    /* Size of ASPK(32 bytes) to be passed to PsStore API should be in words (16 words) */
    PanicFalse(PsStore(PS_KEY_FAST_PAIR_SCRAMBLED_ASPK, (void *)aspk, FAST_PAIR_CONFIG_ASPK_LEN/2));
}
#endif /* INCLUDE_FAST_PAIR */

appKymeraScoMode appTestGetHfpCodec(void)
{
    const appKymeraScoChainInfo *sco_info = KymeraGetTaskData()->sco_info;
    appKymeraScoMode sco_mode = NO_SCO;
    if (sco_info)
    {
        sco_mode = sco_info->mode;
    }
    DEBUG_LOG("appTestGetHfpCodec enum:appKymeraScoMode:%d", sco_mode);
    return sco_mode;
}

#ifdef INCLUDE_ACCESSORY

void appTestRequestAppLaunch(const char * app_name)
{
    DEBUG_LOG_ALWAYS("appTestRequestRttAppLaunch");
    bdaddr handset_address = {0,0,0};
    if(appDeviceGetHandsetBdAddr(&handset_address))
    {
        AccessoryFeature_RequestAppLaunch(handset_address, app_name, launch_with_user_alert);
    }
}

bool appTestIsAccessoryConnected(void)
{
    DEBUG_LOG_ALWAYS("appTestIsAccessoryConnected");
    bool is_connected = FALSE;
    bdaddr handset_address = {0,0,0};
    if(appDeviceGetHandsetBdAddr(&handset_address))
    {
        is_connected = Accessory_IsConnected(handset_address);
    }
    return is_connected;
}

#endif

bool appTestAnyBredrConnection(void)
{
    bool bredr_connection = ConManagerAnyTpLinkConnected(cm_transport_bredr);

    DEBUG_LOG_ALWAYS("appTestAnyBredrConnection: %d", bredr_connection);

    return bredr_connection;
}

int appTestGetCurrentAudioVolume(void)
{
    int volume = 0;

    audio_source_t focused_source = audio_source_none;
    if (Focus_GetAudioSourceForContext(&focused_source))
    {
        volume = AudioSources_GetVolume(focused_source).value;
    }
    if (focused_source == audio_source_none && MirrorProfile_IsA2dpActive())
    {
        // If there is no focused source, but A2DP mirroring is active, then
        // use a2dp1 audio source (which is hard-coded for mirroring scenario).
        focused_source = audio_source_a2dp_1;
        volume = AudioSources_GetVolume(focused_source).value;
    }

    DEBUG_LOG_ALWAYS("appTestGetCurrentAudioVolume enum:audio_source_t:%d volume %d",
                     focused_source, volume);

    return volume;
}

int appTestGetCurrentVoiceVolume(void)
{
    int volume = 0;

    voice_source_t focused_source = voice_source_none;
    if (Focus_GetVoiceSourceForContext(ui_provider_telephony, &focused_source))
    {
        volume = VoiceSources_GetVolume(focused_source).value;
    }
    if (focused_source == voice_source_none && MirrorProfile_GetScoSink())
    {
        // If there is no focused source, but mirroring is active with SCO, then
        // use hfp1 voice source (which is hard-coded for mirroring scenario).
        focused_source = voice_source_hfp_1;
        volume = VoiceSources_GetVolume(focused_source).value;
    }

    DEBUG_LOG_ALWAYS("appTestGetCurrentVoiceVolume enum:voice_source_t:%d volume %d",
                     focused_source, volume);

    return volume;
}

uint8 appTestGetAdaptiveAncFeedForwardGain(void)
{
    uint8 gain = 0;
    DEBUG_LOG("appTestGetAdaptiveAncFeedForwardGain");
    KymeraAdaptiveAnc_GetFFGain(&gain);
    return gain;
}

adaptive_anc_mode_t appTestGetCurrentAdaptiveAncMode(void)
{
    adaptive_anc_mode_t aanc_mode;
    if(Kymera_ObtainCurrentApdativeAncMode(&aanc_mode))
    {
        DEBUG_LOG("appTestGetCurrentAdaptiveAncMode, aanc_mode = %d", aanc_mode);
        return aanc_mode;
    }
    DEBUG_LOG("appTestGetCurrentAdaptiveAncMode, failed!");
    return 0;
}

bool appTestAdaptiveAncIsNoiseLevelBelowQuietModeThreshold(void)
{
    DEBUG_LOG("appTestAdaptiveAncIsNoiseLevelBelowQuietModeThreshold");
    return Kymera_AdaptiveAncIsNoiseLevelBelowQuietModeThreshold();
}

bool appTestAdaptiveAncIsQuietModeDetectedOnLocalDevice(void)
{
    DEBUG_LOG("appTestAdaptiveAncIsQuietModeDetectedOnLocalDevice");
    return AancQuietMode_IsQuietModeDetected();
}

void appTestSetAdaptiveAncGainParameters(uint32 mantissa, uint32 exponent)
{
    DEBUG_LOG("appTestSetAdaptiveAncGainParameters");

    /* Just to keep compiler happy */
    UNUSED(mantissa);
    UNUSED(exponent);

    KymeraAdaptiveAnc_SetGainValues(mantissa, exponent);
}

bool appTestIsDeviceTestServiceEnabled(void)
{
    bool enabled = DeviceTestService_TestMode();

    DEBUG_LOG_ALWAYS("appTestIsDeviceTestServiceEnabled:%d", enabled);
    return enabled;
}


bool appTestIsDeviceTestServiceActive(void)
{
    bool active = FALSE;

#ifdef INCLUDE_DEVICE_TEST_SERVICE
    active = DeviceTestServiceIsActive();
#endif

    DEBUG_LOG_ALWAYS("appTestIsDeviceTestServiceActive. Active:%d", active);

    return active;
}


bool appTestIsDeviceTestServiceAuthenticated(void)
{
    bool authenticated = FALSE;

#ifdef INCLUDE_DEVICE_TEST_SERVICE
    authenticated = DeviceTestServiceIsAuthenticated();
#endif

    DEBUG_LOG_ALWAYS("appTestIsDeviceTestServiceAuthenticated. Authenticated:%d", authenticated);

    return authenticated;
}


bool appTestDeviceTestServiceEnable(device_test_service_mode_t mode)
{
    bool saved = FALSE;

    DeviceTestService_SaveTestMode(mode);

    /* The SaveTestMode API does not check if the PSKEY write was
       successful. Check now by reading the value back.
     */
    saved = (mode == DeviceTestService_TestModeType());

    DEBUG_LOG_ALWAYS("appTestDeviceTestServiceEnable. Saved:%d", saved);

    return saved;
}

unsigned appTestDeviceTestServiceDevices(device_t **devices)
{
    unsigned num_found = deviceTestService_GetDtsDevices(devices);

    DEBUG_LOG_ALWAYS("appTestDeviceTestServiceDevices. Devices:%u",
                      num_found);

    return num_found;
}


bool appTestIsBredrScanEnabled(void)
{
    bool scan_enabled = !BredrScanManager_IsScanDisabled();

    DEBUG_LOG_ALWAYS("appTestIsBredrScanEnabled Enabled:%d", scan_enabled);

    return scan_enabled;
}

bool appTestIsPreparingToSleep(void)
{
    appState state = appSmGetState();
    return (state & APP_SUBSTATE_SOPORIFIC_TERMINATING) != 0;
}

#ifdef INCLUDE_GAMING_MODE
bool appTestIsGamingModeOn(void)
{
    return GamingMode_IsGamingModeEnabled();
}

bool appTestSetGamingMode(bool new_gaming_state)
{
    bool current_gaming_state = appTestIsGamingModeOn();
    if(current_gaming_state != new_gaming_state)
    {
        LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_gaming_mode_toggle);
        return TRUE;
    }

    return FALSE;
}
#endif
#ifdef INCLUDE_LATENCY_MANAGER
bool appTestIsDynamicLatencyAdjustmentEnabled(void)
{
    return Kymera_DynamicLatencyIsEnabled();
}

uint16 appTestGetCurrentA2dpLatency(void)
{
    return Kymera_LatencyManagerGetLatency();
}

void appTestSetDynamicLatencyAdjustment(bool enable)
{
    if (enable)
    {
        Kymera_DynamicLatencyEnableDynamicAdjustment();
    }
    else
    {
        Kymera_DynamicLatencyDisableDynamicAdjustment();
    }
}
#endif

bool appTestIsKymeraIdle(void)
{
    bool idle = Kymera_IsIdle();

    DEBUG_LOG_ALWAYS("appTestIsKymeraIdle. Idle:%d", idle);

    return idle;
}


bool appTestIsPeerQhsConnected(void)
{
    bool qhs_connected_status = FALSE;
    bdaddr bd_addr;

    if (appDeviceGetPeerBdAddr(&bd_addr))
    {
        qhs_connected_status = ConManagerGetQhsConnectStatus(&bd_addr);
    }

    DEBUG_LOG_ALWAYS("appTestIsPeerQhsConnected:%d", qhs_connected_status);

    return qhs_connected_status;
}

bool appTestIsHandsetQhsConnected(void)
{
    bool qhs_connected_status = FALSE;
    bdaddr bd_addr;

    if (appDeviceGetHandsetBdAddr(&bd_addr))
    {
        qhs_connected_status = ConManagerGetQhsConnectStatus(&bd_addr);
    }

    DEBUG_LOG_ALWAYS("appTestIsHandsetQhsConnected:%d", qhs_connected_status);

    return qhs_connected_status;
}

bool appTestIsTimeToSleep(void)
{
    bool time_to_sleep = (appSmGetState() == APP_STATE_OUT_OF_CASE_IDLE);

    DEBUG_LOG_ALWAYS("appTestIsTimeToSleep %d", time_to_sleep);
    return time_to_sleep;
}

void appTestTriggerSleep(void)
{
    DEBUG_LOG_ALWAYS("appTestTriggerSleep");
    MessageCancelFirst(SmGetTask(), SM_INTERNAL_TIMEOUT_IDLE);
    MessageSend(SmGetTask(), SM_INTERNAL_TIMEOUT_IDLE, NULL);
}

void appTestForceAllowSleep(void)
{
    DEBUG_LOG_ALWAYS("appTestForceAllowSleep");
    Charger_ForceAllowPowerOff(TRUE);
}

void appTestDisallowDormant(void)
{
    DEBUG_LOG_ALWAYS("appTestDisallowDormant");
    Charger_DisallowDormant(TRUE);
}

void appTestRegisterForKymeraNotifications(void)
{
    DEBUG_LOG_ALWAYS("appTestRegisterForKymeraNotifications");
    Kymera_RegisterNotificationListener(&testTask);
}

/*! \brief Get A2DP Codec
     This can only be called from primary earbud
     because we don't have av instance for mirroring in secondary earbud
*/
app_test_a2dp_codec_t EarbudTest_GetA2dpCodec(void)
{

    DEBUG_LOG_ALWAYS("earbudTest_getA2dpCodec");
    bdaddr bd_addr;
    if (appDeviceGetHandsetBdAddr(&bd_addr))
    {
        /* Find handset AV instance */
        avInstanceTaskData *theInst = appAvInstanceFindFromBdAddr(&bd_addr);
        if(theInst)
        {
            switch(theInst->a2dp.current_seid)
            {
                case AV_SEID_SBC_SRC:
                case AV_SEID_SBC_SNK:
                   case AV_SEID_SBC_MONO_TWS_SNK:
                    return a2dp_codec_sbc;

                case AV_SEID_AAC_SNK:
                case AV_SEID_AAC_STEREO_TWS_SNK:
                    return a2dp_codec_aac;

                case AV_SEID_APTX_SNK:
                case AV_SEID_APTX_MONO_TWS_SNK:
                    return a2dp_codec_aptx;

                case AV_SEID_APTX_ADAPTIVE_SNK:
                case AV_SEID_APTX_ADAPTIVE_TWS_SNK:
                    return a2dp_codec_aptx_adaptive;
                default:
                    break;
            }
        }
    }

    return a2dp_codec_unknown;
}

#ifdef INCLUDE_GAA
uint8 EarbudTest_GetGaaOtaState(void)
{
    if (!appSmIsPrimary())
    {
        DEBUG_LOG_ALWAYS("EarbudTest_GetGaaOtaState: Warning: earbud is not primary");
    }
    return Gaa_OtaGetState();
}

uint32 EarbudTest_GetGaaModelId(void)
{
    uint32 model_id = 0;
    if (Gaa_GetModelId(&model_id))
    {
        DEBUG_LOG_ALWAYS("EarbudTest_GetGaaModelId: 0x%08lx", model_id);
    }
    else
    {
        DEBUG_LOG_ERROR("EarbudTest_GetGaaModelId: Gaa_GetModelId failed");
    }
    return model_id;
}

bool EarbudTest_SetGaaModelId(uint32 model_id)
{
    bool result = Gaa_SetModelId(model_id);
    if (result)
    {
        DEBUG_LOG_ALWAYS("EarbudTest_SetGaaModelId(0x%08lx) successful", model_id);
    }
    else
    {
        DEBUG_LOG_ERROR("EarbudTest_SetGaaModelId(0x%08lx) failed", model_id);
    }
    return result;
}

uint32 appTestGetGaaOtaControl(void)
{
    uint32 result = 0xFFFFFFFFLU;
    uint16 GaaOtaControl = 0;
    bool   present = Gaa_OtaGetControlPsKey(&GaaOtaControl);
    if (present)
    {
        result = GaaOtaControl;
    }
    return result;
}

void appTestSetGaaOtaControl(uint16 GaaOtaControl)
{
    Gaa_OtaSetControlPsKey(GaaOtaControl);
}

#endif  /* INCLUDE_GAA */

#ifdef GC_SECTIONS
/**
 * @brief appTestShowKeptSymbols
 *
 * A list of ADK and library functions and/or variables that are referenced
 * directly via pydbg test scripts, but which are otherwise unused given the
 * current software configuration.
 *
 * This is an alternative mechanism to scattering #pragma unitcodesection KEEP_PM
 * over the library code.  Doing that would unnecessarily include code in production
 * builds even if such code was not actually used.
 *
 * So here we fake our interest by making pointers to things of interest.
 * Nothing is called or dereferenced, but it's enough to stop the linker
 * garbage collecting otherwise unused resources.
 *
 * It's not necessary to call this function, but it's harmess if you do.
 */
static const void *tableOfSymbolsToKeep[] = {
    (void*)ConnectionReadTxPower,
    (void*)Kymera_RegisterNotificationListener,
    (void*)SinkGetRssi,
};
void appTestShowKeptSymbols(void);
void appTestShowKeptSymbols(void)
{
    for( size_t i = 0 ; i < sizeof(tableOfSymbolsToKeep)/sizeof(tableOfSymbolsToKeep[0]) ; i++ )
    {
        if( tableOfSymbolsToKeep[i] != NULL )
        {
            DEBUG_LOG_ALWAYS("Have %p",tableOfSymbolsToKeep[i]);
        }
    }
}
#endif

#ifdef INCLUDE_TEST_TONES
static rtime_t getTestToneStartTime(void)
{
    return rtime_add(SystemClockGetTimerTime(), 300*US_PER_MS);
}

void appTestToneStartContinuous(void)
{
    appKymeraTonePlay(app_tone_test_continuous, getTestToneStartTime(), TRUE, NULL, 0);
}

void appTestToneStartIndexed(void)
{
    appKymeraTonePlay(app_tone_test_indexed, getTestToneStartTime(), TRUE, NULL, 0);
}

void appTestToneStartByEvent(uint16 event_id)
{
    int i;
    for (i = EarbudTonesConfigTable_SingleGetSize() - 1; i >= 0; i -= 1)
    {
        if (earbud_ui_tones_table[i].sys_event == event_id)
        {
            appKymeraTonePlay(earbud_ui_tones_table[i].data.tone.tone, getTestToneStartTime(), TRUE, NULL, 0);
        }
    }
}

void appTestToneStop(void)
{
    appKymeraTonePromptCancel();
}
#endif

void appTestCaseLidOpenEvent(void)
{
    DEBUG_LOG_ALWAYS("appTestCaseLidOpenEvent");
    CcWithCase_LidEvent(CASE_LID_STATE_OPEN);
}

void appTestCaseLidClosedEvent(void)
{
    DEBUG_LOG_ALWAYS("appTestCaseLidClosedEvent");
    CcWithCase_LidEvent(CASE_LID_STATE_CLOSED);
}

void appTestCasePowerMsgEvent(uint8 case_battery_state,
                              uint8 peer_battery_state, uint8 local_battery_state,
                              bool case_charger_connected)
{
    DEBUG_LOG_ALWAYS("appTestCasePowerMsgEvent 0x%x 0x%x 0x%x %d", case_battery_state, peer_battery_state, local_battery_state, case_charger_connected);
    CcWithCase_PowerEvent(case_battery_state, peer_battery_state, local_battery_state, case_charger_connected);
}

bool appTestCaseEventsSupported(void)
{
    bool case_events_supported = CcWithCase_EventsEnabled();
    DEBUG_LOG_ALWAYS("appTestCaseEventsSupported %d", case_events_supported);
    return case_events_supported;
}

bool appTestTransmitEnable(bool val)
{
    bool successful = VmTransmitEnable(val);

    DEBUG_LOG_ALWAYS("appTestTransmitEnable(%d)  success=%d", val, successful);

    return successful;
}

void appTestDisconnectHandsets(void)
{
    TwsTopology_DisconnectAllHandsets();
}

bool earbudTestPeerPairWithAddress(bdaddr *address)
{
    bool result = PeerPairing_PeerPairToAddress(&testTask, address);

    DEBUG_LOG_ALWAYS("earbudTestPeerPairWithAddress %04x%02x%06lx Failed:%d",
                        address->nap, address->uap, address->lap,
                        !result);

    return result;
}


#ifdef INCLUDE_MUSIC_PROCESSING
#include "music_processing.h"

void appTest_MusicProcessingGetAvailablePresets(void)
{
    DEBUG_LOG_ALWAYS("appTest_MusicProcessingGetAvailablePresets");
}

void appTest_MusicProcessingSetPreset(uint8 preset)
{
    DEBUG_LOG_ALWAYS("appTest_MusicProcessingSetPreset Preset:%d", preset);
    MusicProcessing_SetPreset(preset);
}

void appTest_MusicProcessingSetUserEqBand(uint8 band, int16 gain)
{
    int16 band_gain = gain;
    DEBUG_LOG_ALWAYS("appTest_MusicProcessingSetUserEqBands Band:%d Gain: %d", band, gain);
    MusicProcessing_SetUserEqBands(band, band, &band_gain);
}

#endif /* INCLUDE_MUSIC_PROCESSING */

bool earbudTestAddPeerPairing(const bdaddr *primary,
                              const bdaddr *secondary,
                              bool this_is_primary,
                              const cl_root_keys *randomised_keys,
                              PEER_PAIRING_LONG_TERM_KEY_T *bredr,
                              PEER_PAIRING_LONG_TERM_KEY_T *le)
{
    DEBUG_LOG_ALWAYS("earbudTestAddPeerPairing. Adding earbud pairing record. Primary:%d",
                      this_is_primary);

    bool successful = PeerPairing_AddPeerPairing(primary, secondary, this_is_primary,
                                                 randomised_keys,
                                                 bredr, le);

    if (!successful)
    {
        DEBUG_LOG_ALWAYS("earbudTestAddPeerPairing. Adding earbud pairing record. FAILED");
    }

    return successful;
}

#ifdef INCLUDE_CHARGER

void appTestChargerStatus(charger_status chg_status)
{
    Charger_TestModeControl(TRUE);
    Charger_TestChargerStatus(chg_status);
}

void appTestChargerDetected(usb_attached_status attached_status,
                                       uint16 charger_dp_millivolts,
                                       uint16 charger_dm_millivolts,
                                       usb_type_c_advertisement cc_status)
{
    Charger_TestModeControl(TRUE);
    Charger_TestChargerDetected(attached_status,
                                charger_dp_millivolts,
                                charger_dm_millivolts,
                                cc_status);
}

void appTestChargerConnected(bool is_connected)
{
    Charger_TestModeControl(TRUE);
    Charger_TestChargerConnected(is_connected);
}

void appTestChargerRestore(void)
{
    Charger_TestModeControl(FALSE);
}

#endif /* INCLUDE_CHARGER */

uint32 appTestUpgradeGetVersion(void)
{
    uint32 result = 0;
    uint16 major, minor, config;
    if (UpgradeGetVersion(&major, &minor, &config))
    {
        DEBUG_LOG_ALWAYS("appTestUpgradeGetVersion: %d.%d.%d", major, minor, config);
        result = (major << 24);
        result |= (minor << 8);
        result |= (config & 0xFF);
    }
    else
    {
        DEBUG_LOG_ALWAYS("appTestUpgradeGetVersion: UpgradeGetVersion failed");
    }

    return result;
}

#ifdef ENABLE_EARBUD_FIT_TEST
void EarbudTest_StartFitTest(void)
{
    DEBUG_LOG_ALWAYS("EarbudTest_StartFitTest");
    Ui_InjectUiInput(ui_input_fit_test_start);
}

void EarbudTest_StopFitTest(void)
{
    DEBUG_LOG_ALWAYS("EarbudTest_StopFitTest");
    Ui_InjectUiInput(ui_input_fit_test_abort);
}

uint32 EarbudTest_GetLocalDeviceFitTestStatus(void)
{
    fit_test_result_t result = FitTest_GetLocalDeviceTestResult();
    DEBUG_LOG_ALWAYS("EarbudTest_GetLocalDeviceFitTestStatus: enum:fit_test_result_t:%d", result);
    return (uint32)result;
}

uint32 EarbudTest_GetRemoteDeviceFitTestStatus(void)
{
    fit_test_result_t result = FitTest_GetRemoteDeviceTestResult();
    DEBUG_LOG_ALWAYS("EarbudTest_GetRemoteDeviceFitTestStatus: enum:fit_test_result_t:%d", result);
    return (uint32)result;
}

void EarbudTest_StartFitTestTuning(void)
{
    /* Check if Fit Test tuning is already running */
    if(FitTest_IsTuningModeActive())
    {
        DEBUG_LOG_INFO("Fit Test tuning is already in progress");
    }
    else
    {
        /* Start Fit test tuning only if device is not incase */
        if(appPhyStateIsOutOfCase())
        {
            DEBUG_LOG_DEBUG("EarbudTest_StartFitTestTuning");
            Ui_InjectUiInput(ui_input_fit_test_enter_tuning_mode);
        }
    }
}

void EarbudTest_StopFitTestTuning(void)
{
    DEBUG_LOG_FN_ENTRY("EarbudTest_StopFitTestTuning");
    Ui_InjectUiInput(ui_input_fit_test_exit_tuning_mode);
}

#endif /* ENABLE_EARBUD_FIT_TEST */


ui_user_gesture_table_content_t test_ui_config_table[] =
{
    { .gesture_id = ui_gesture_tap, .originating_touchpad = touchpad_left, .context_id = ui_context_passthrough, .action_id = ui_action_volume_down },
    { .gesture_id = ui_gesture_tap, .originating_touchpad = touchpad_right, .context_id = ui_context_passthrough, .action_id = ui_action_volume_up },
};

void EarbudTest_ApplyUiUserConfigTestVector(void)
{
    UiUserConfig_SetUserGestureConfiguration(
                test_ui_config_table,
                ARRAY_DIM(test_ui_config_table)*sizeof(ui_user_gesture_table_content_t));
}

#endif // DISABLE_TEST_API

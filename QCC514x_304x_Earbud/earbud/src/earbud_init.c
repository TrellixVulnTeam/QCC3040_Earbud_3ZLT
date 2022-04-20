/*!
\copyright  Copyright (c) 2008 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Initialisation module for the earbud application
*/

#include "app_task.h"
#include "earbud_init.h"
#include "earbud_common_rules.h"
#include "earbud_config.h"

#include "..\aul\aul_common.h"
//#include "C:\Qualcomm_Prog\qcc514x-qcc304x-src-1-0_qtil_standard_oem_earbud-ADK-21.1-CS2-MR1\earbud\aul\aul_common.h"
//#incldue "../aul/aul_common.h"
//#include "./aul_common.h"
//#include "aul_common.h"

#include "earbud_led.h"
#include "earbud_ui_config.h"
#ifdef INCLUDE_FAST_PAIR
#include "earbud_config.h"
#endif
#include "earbud_temperature_config.h"
#include "earbud_region_config.h"
#include "earbud_soc_config.h"
#include "authentication.h"
#include "adk_log.h"
#include "unexpected_message.h"
#include "earbud_test.h"
#include "earbud_hardware.h"
#include "proximity.h"
#include "temperature.h"
#include "battery_region.h"
#include "state_of_charge.h"
#include "handset_service.h"
#include "pairing.h"
#include "power_manager.h"
#include "earbud_ui.h"
#include "earbud_sm.h"
#include "earbud_handover.h"
#include "voice_ui.h"
#include "voice_ui_eq.h"
#ifdef ENABLE_AUDIO_TUNING_MODE
#include "voice_audio_tuning_mode.h"
#endif
#include "gaia_framework.h"
#include "earbud_gaia_plugin.h"
#include "earbud_gaia_tws.h"
#include "handset_service_gaia_plugin.h"
#ifdef ENABLE_GAIA_USER_FEATURE_LIST_DATA
#include "earbud_gaia_user_feature_config.h"
#endif
#include "upgrade_gaia_plugin.h"
#if defined(INCLUDE_GAIA_PYDBG_REMOTE_DEBUG) || defined(INCLUDE_GAIA_PANIC_LOG_TRANSFER)
#include "gaia_debug_plugin.h"
#endif
#if defined(INCLUDE_MUSIC_PROCESSING)
#include "music_processing.h"
#include "music_processing_gaia_plugin.h"
#endif
#if defined(INCLUDE_CVC_DEMO)
#include "voice_enhancement_gaia_plugin.h"
#endif
#include "gatt_handler.h"
#include "gatt_connect.h"
#include "gatt_server_battery.h"
#include "gatt_server_dis.h"
#include "gatt_server_gatt.h"
#include "gatt_server_gap.h"
#include "dfu.h"
#ifdef INCLUDE_DFU_PEER
#include "dfu_peer.h"
#endif
#include "gaia.h"
#include "usb_device.h"
#include "connection_manager_config.h"
#include "device_db_serialiser.h"
#include "bt_device_class.h"
#include "le_advertising_manager.h"
#include "le_scan_manager.h"
#include "link_policy.h"
#include "logical_input_switch.h"
#include "input_event_manager.h"
#include "pio_monitor.h"
#include "local_addr.h"
#include "local_name.h"
#include "connection_message_dispatcher.h"
#include "ui.h"
#include "ui_user_config.h"
#include "volume_messages.h"
#include "volume_service.h"
#include "media_player.h"
#include "telephony_service.h"
#include "audio_sources.h"
#include "voice_sources.h"
#include "peer_link_keys.h"
#include "peer_ui.h"
#include "telephony_messages.h"
#include "anc_state_manager.h"
#include "aec_leakthrough.h"
#include "audio_curation.h"
#include "anc_gaia_plugin.h"
#include "single_entity.h"
#include "audio_router.h"
#include "focus_select.h"
#include "gaming_mode.h"
#include "earbud_production_test.h"
#include "earbud_feature_manager_priority_list.h"

#ifdef ENABLE_EARBUD_FIT_TEST
#include "fit_test.h"
#endif

#ifdef INCLUDE_AMA
#include "ama.h"
#endif

#include "earbud_buttons.h"

#include <system_state.h>
#include <bandwidth_manager.h>
#include <bredr_scan_manager.h>
#include <connection_manager.h>
#include <device_list.h>
#include <feature_manager.h>
#include <hfp_profile.h>
#include <hfp_profile_battery_level.h>
#include <handover_profile.h>
#include <mirror_profile.h>
#include <charger_monitor.h>
#include <message_broker.h>
#include <profile_manager.h>
#include <state_proxy.h>
#include <peer_signalling.h>
#include <tws_topology.h>
#include <peer_find_role.h>
#include <peer_pair_le.h>
#include <key_sync.h>
#include <ui_indicator_prompts.h>
#include <ui_indicator_tones.h>
#include <ui_indicator_leds.h>
#include <fast_pair.h>
#include <tx_power.h>
#include <multidevice.h>
#include <device_sync.h>
#ifdef INCLUDE_SWIFT_PAIR
#include <swift_pair.h>
#endif
#ifdef INCLUDE_REMOTE_NAME
#include <device_sync_pskey.h>
#include <device_pskey.h>
#include <remote_name.h>
#endif
#ifdef INCLUDE_L2CAP_MANAGER
#include <l2cap_manager.h>
#endif
#if defined HAVE_RDP_HW_YE134 || defined HAVE_RDP_HW_18689
#include <pio_common.h>
#include <microphones_config.h>
#endif

#include <power_manager_action.h>

#ifdef INCLUDE_WATCHDOG
#include <watchdog.h>
#endif

#ifdef INCLUDE_QCOM_CON_MANAGER
#include <qualcomm_connection_manager.h>
#endif

#include <panic.h>
#include <pio.h>
#include <stdio.h>
#include <feature.h>

#ifdef INCLUDE_GAA
#include <gaa.h>
#include <gaa_ota.h>
#endif


#ifdef INCLUDE_GAA_LE
#include <gatt_server_gaa_comm.h>
#include <gatt_server_ams_proxy.h>
#include <gatt_server_ancs_proxy.h>
#include <gatt_server_gaa_media.h>
#endif

#ifdef INCLUDE_GATT_SERVICE_DISCOVERY
#include <gatt_client.h>
#include <gatt_service_discovery.h>
#endif
#include <device_test_service.h>

#ifdef INCLUDE_ACCESSORY
#include "accessory.h"
#include "accessory_tws.h"
#include "request_app_launch.h"
#include "rtt.h"
#endif
#include "earbud_setup_audio.h"
#include "earbud_setup_unexpected_message.h"

#include "earbud_init_bt.h"

#include <device_properties.h>

#include <cc_with_case.h>
#include <app/bluestack/dm_prim.h>

#ifdef INCLUDE_MUSIC_PROCESSING
    voice_ui_eq_if_t voice_ui_eq_if =
    {
        MusicProcessing_IsEqActive,
        MusicProcessing_GetNumberOfActiveBands,
        MusicProcessing_SetUserEqBands,
        MusicProcessing_SetPreset
    };
#endif /* INCLUDE_MUSIC_PROCESSING */

#ifdef ENABLE_EARBUD_FIT_TEST
#include "fit_test_gaia_plugin.h"
#endif

static bool appPioInit(Task init_task)
{
#ifndef USE_BDADDR_FOR_LEFT_RIGHT
    /* Make PIO2 an input with pull-up */
    PioSetMapPins32Bank(0, 1UL << appConfigHandednessPio(), 1UL << appConfigHandednessPio());
    PioSetDir32Bank(0, 1UL << appConfigHandednessPio(), 0);
    PioSet32Bank(0, 1UL << appConfigHandednessPio(), 1UL << appConfigHandednessPio());
    DEBUG_LOG_INFO("appPioInit, left %d, right %d", appConfigIsLeft(), appConfigIsRight());

    Multidevice_SetType(multidevice_type_pair);
    Multidevice_SetSide(appConfigIsLeft() ? multidevice_side_left : multidevice_side_right);
#endif

#if defined HAVE_RDP_HW_YE134 || defined HAVE_RDP_HW_18689
    /* Config BIAS Pins active low*/
    PioSetActiveLevel(appConfigMic2Pio(), FALSE);
    PioSetActiveLevel(appConfigMic3Pio(), FALSE);
#endif

    /* Start off with external power supplies enabled. This is needed for the proximity sensor */
    EarbudHardware_SetSensorPowerSupplies(all_supplies_mask, all_supplies_mask);
    UNUSED(init_task);

    return TRUE;
}

static bool appLicenseCheck(Task init_task)
{
    if (FeatureVerifyLicense(APTX_CLASSIC))
        DEBUG_LOG_VERBOSE("appLicenseCheck: aptX Classic is licensed, aptX A2DP CODEC is enabled");
    else
        DEBUG_LOG_WARN("appLicenseCheck: aptX Classic not licensed, aptX A2DP CODEC is disabled");

    if (FeatureVerifyLicense(APTX_CLASSIC_MONO))
        DEBUG_LOG_VERBOSE("appLicenseCheck: aptX Classic Mono is licensed, aptX TWS+ A2DP CODEC is enabled");
    else
        DEBUG_LOG_WARN("appLicenseCheck: aptX Classic Mono not licensed, aptX TWS+ A2DP CODEC is disabled");

    if (FeatureVerifyLicense(CVC_RECV))
        DEBUG_LOG_VERBOSE("appLicenseCheck: cVc Receive is licensed");
    else
        DEBUG_LOG_WARN("appLicenseCheck: cVc Receive not licensed");

    if (FeatureVerifyLicense(CVC_SEND_HS_1MIC))
        DEBUG_LOG_VERBOSE("appLicenseCheck: cVc Send 1-MIC is licensed");
    else
        DEBUG_LOG_WARN("appLicenseCheck: cVc Send 1-MIC not licensed");

    if (FeatureVerifyLicense(CVC_SEND_HS_2MIC_MO))
        DEBUG_LOG_VERBOSE("appLicenseCheck: cVc Send 2-MIC is licensed");
    else
        DEBUG_LOG_WARN("appLicenseCheck: cVc Send 2-MIC not licensed");

    UNUSED(init_task);
    return TRUE;
}

#ifdef UNMAP_AFH_CH78

/*! Unmap AFH channel 78

    It is need to meet regulatory requirements when QHS is used.
*/
static bool earbud_RemapAfh78(Task init_task)
{
    static const uint8_t afh_map[10] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x3f};

    UNUSED(init_task);

    MESSAGE_MAKE(prim, DM_HCI_SET_AFH_CHANNEL_CLASS_REQ_T);
    prim->common.op_code = DM_HCI_SET_AFH_CHANNEL_CLASS_REQ;
    prim->common.length = sizeof(DM_HCI_SET_AFH_CHANNEL_CLASS_REQ_T);
    memcpy(prim->map, afh_map, sizeof(afh_map));

    VmSendDmPrim(prim);

    return TRUE;
}
#endif

static bool appInitTransportManagerInitFixup(Task init_task)
{
    UNUSED(init_task);

    TransportMgrInit();

    return TRUE;
}

static bool appMessageDispatcherRegister(Task init_task)
{
    Task client = &app_init.task;

    UNUSED(init_task);

    ConnectionMessageDispatcher_RegisterInquiryClient(client);
    ConnectionMessageDispatcher_RegisterCryptoClient(client);
    ConnectionMessageDispatcher_RegisterCsbClient(client);
    ConnectionMessageDispatcher_RegisterLeClient(client);
    ConnectionMessageDispatcher_RegisterTdlClient(client);
    ConnectionMessageDispatcher_RegisterL2capClient(client);
    ConnectionMessageDispatcher_RegisterLocalDeviceClient(client);
    ConnectionMessageDispatcher_RegisterPairingClient(client);
    ConnectionMessageDispatcher_RegisterLinkPolicyClient(client);
    ConnectionMessageDispatcher_RegisterTestClient(client);
    ConnectionMessageDispatcher_RegisterRemoteConnectionClient(client);
    ConnectionMessageDispatcher_RegisterRfcommClient(client);
    ConnectionMessageDispatcher_RegisterScoClient(client);
    ConnectionMessageDispatcher_RegisterSdpClient(client);
    ConnectionMessageDispatcher_RegisterLeIsoClient(client);

    return TRUE;
}

static const bt_device_default_value_callback_t property_default_values[] =
{
        {device_property_headset_service_config, HandsetService_SetDefaultConfig}
};

static const bt_device_default_value_callback_list_t default_value_callback_list = {property_default_values, ARRAY_DIM(property_default_values)};

static bool appInitDeviceDbSerialiser(Task init_task)
{
    UNUSED(init_task);

    DeviceDbSerialiser_Init();

    BtDevice_RegisterPropertyDefaults(&default_value_callback_list);

    /* Register persistent device data users */
    BtDevice_RegisterPddu();

#ifdef INCLUDE_FAST_PAIR
    FastPair_RegisterPersistentDeviceDataUser();
#endif

#ifdef INCLUDE_REMOTE_NAME
    DevicePsKey_RegisterPddu();
#endif

    UiUserConfig_RegisterPddu();

    /* Allow space in device list to store all paired devices + connected handsets not yet paired */
    DeviceList_Init(appConfigEarbudMaxDevicesSupported() + appConfigMaxNumOfHandsetsCanConnect());

    DeviceDbSerialiser_Deserialise();

    return TRUE;
}

static const InputActionMessage_t* appInitGetInputActions(uint16* input_actions_dim)
{
    const InputActionMessage_t* input_actions = NULL;
#if defined(HAVE_1_BUTTON) || defined(HAVE_6_BUTTONS)
#if defined(INCLUDE_GAA) || defined(INCLUDE_AMA)
    if (appConfigIsRight())
    {
        DEBUG_LOG_VERBOSE("appInitGetInputActions voice_assistant_message_group");
        *input_actions_dim = ARRAY_DIM(voice_assistant_message_group);
        input_actions = voice_assistant_message_group;
    }
    else
#endif /* INCLUDE_GAA || INCLUDE_AMA */
    {
        DEBUG_LOG_VERBOSE("appInitGetInputActions media_message_group");
        *input_actions_dim = ARRAY_DIM(media_message_group);
        input_actions = media_message_group;
    }
#else /* (HAVE_1_BUTTON) || (HAVE_6_BUTTONS) */
    *input_actions_dim = ARRAY_DIM(default_message_group);
    input_actions = default_message_group;
#endif /* (HAVE_1_BUTTON) || (HAVE_6_BUTTONS) */
    return input_actions;
}

static bool appInputEventMangerInit(Task init_task)
{
    const InputActionMessage_t* input_actions = NULL;
    uint16 input_actions_dim = 0;
    UNUSED(init_task);
    input_actions = appInitGetInputActions(&input_actions_dim);
    PanicNull((void*)input_actions);

    /* Initialise input event manager with auto-generated tables for
     * the target platform. Connect to the logical Input Switch component */
    InputEventManagerInit(LogicalInputSwitch_GetTask(), input_actions,
                          input_actions_dim, &input_event_config);
    return TRUE;
}

#ifdef INIT_DEBUG
/*! Debug function blocks execution until appInitDebugWait is cleared:
    apps1.fw.env.vars['appInitDebugWait'].set_value(0) */
static bool appInitDebug(Task init_task)
{
    volatile static bool appInitDebugWait = TRUE;
    while(appInitDebugWait);

    UNUSED(init_task);
    return TRUE;
}
#endif

#ifdef INCLUDE_FAST_PAIR
static bool appTxPowerInit(Task init_task)
{
    bool result = TxPower_Init(init_task);
    TxPower_SetTxPowerPathLoss(appConfigBoardTxPowerPathLoss);
    return result;
}
#endif

#ifdef INCLUDE_GAIA
static bool earbud_EarbudGaiaPluginRegister(Task init_task)
{
    DEBUG_LOG_VERBOSE("earbud_EarbudGaiaPluginRegister");

    EarbudGaiaPlugin_Init();
    EarbudGaiaTws_Init(init_task);

    return TRUE;
}
#endif

#ifdef INCLUDE_DFU

static bool earbud_DfuAppRegister(Task init_task)
{
    UNUSED(init_task);

    DEBUG_LOG_VERBOSE("earbud_DfuAppRegister");

    Dfu_ClientRegister(SmGetTask());

    Dfu_SetVersionInfo(UPGRADE_INIT_VERSION_MAJOR, UPGRADE_INIT_VERSION_MINOR, UPGRADE_INIT_CONFIG_VERSION);

    return TRUE;
}

static bool earbud_UpgradeGaiaPluginRegister(Task init_task)
{
    UNUSED(init_task);

    DEBUG_LOG("earbud_UpgradeGaiaPluginRegister");

    UpgradeGaiaPlugin_Init();

    return TRUE;
}
#endif

#ifdef INCLUDE_DFU_PEER
static bool earbud_PeerDfuAppRegister(Task init_task)
{
    UNUSED(init_task);

    DEBUG_LOG_VERBOSE("earbud_PeerDfuAppRegister");

    DfuPeer_ClientRegister(SmGetTask());

    return TRUE;
}
#endif

#if defined(ENABLE_ANC) && defined(INCLUDE_GAIA)
static bool earbud_AncGaiaPluginRegister(Task init_task)
{
    UNUSED(init_task);

    DEBUG_LOG_VERBOSE("earbud_AncGaiaPluginRegister");

    AncGaiaPlugin_Init();

    return TRUE;
}
#endif


#if defined(ENABLE_EARBUD_FIT_TEST) && defined(INCLUDE_GAIA)
static bool earbud_FitTestGaiaPluginRegister(Task init_task)
{
    UNUSED(init_task);

    DEBUG_LOG_VERBOSE("earbud_FitTestGaiaPluginRegister");

    FitTestGaiaPlugin_Init();

    return TRUE;
}

#endif


#ifdef INCLUDE_TEMPERATURE
/*! \brief Utility function to Initialise Temperature component   */
static bool earbud_TemperatureInit(Task init_task)
{
    const temperature_lookup_t* config_table;
    unsigned config_table_size;

    config_table = EarbudTemperature_GetConfigTable(&config_table_size);

    /* set voltage->temperature config table */
    Temperature_SetConfigurationTable(config_table, config_table_size);

    appTemperatureInit(init_task);
    return TRUE;
}
#endif

/*! \brief Utility function to Initialise Battery Region component   */
static bool earbud_BatteryRegionInit(Task init_task)
{
    const charge_region_t* config_table;
    unsigned config_table_size;
    const battery_region_handlers_t* handlers_list;

    UNUSED(init_task);

    config_table = EarbudRegion_GetChargeModeConfigTable(&config_table_size);

    /* set charge mode config table */
    BatteryRegion_SetChargeRegionConfigTable(CHARGE_MODE, config_table, config_table_size);

    config_table = EarbudRegion_GetDischargeModeConfigTable(&config_table_size);

    /* set discharge mode config table */
    BatteryRegion_SetChargeRegionConfigTable(DISCHARGE_MODE, config_table, config_table_size);

    /* get handler functions list */
    handlers_list = EarbudRegion_GetRegionHandlers();

    /* set region state handler functions list */
    BatteryRegion_SetHandlerStructure(handlers_list);

    BatteryRegion_Init();
    return TRUE;
}

/*! \brief Utility function to Initialise SoC component   */
static bool earbud_SoCInit(Task init_task)
{
    const soc_lookup_t* config_table;
    unsigned config_table_size;

    UNUSED(init_task);
    config_table = EarbudSoC_GetConfigTable(&config_table_size);

    /* set voltage->percentage config table */
    Soc_SetConfigurationTable(config_table, config_table_size);

    Soc_Init();
    return TRUE;
}

static bool earbud_FeatureManagerInit(Task init_task)
{
    UNUSED(init_task);
    FeatureManager_SetPriorities(Earbud_GetFeatureManagerPriorityList());
    return TRUE;
}

#ifdef INCLUDE_WATCHDOG
static bool appWatchdogInit(Task init_task)
{
    UNUSED(init_task);
    Watchdog_Init();
    return TRUE;
}
#endif

/*! \brief Table of initialisation functions */
static const system_state_step_t appInitTable[] =
{
#ifdef INIT_DEBUG
    {appInitDebug,          0, NULL},
#endif
#ifdef INCLUDE_WATCHDOG
    {appWatchdogInit,       0, NULL},
#endif
    {appPioInit,            0, NULL},
    {PioMonitorInit,        0, NULL},
    {Ui_Init,               0, NULL},
    {appLicenseCheck,       0, NULL},
    {earbud_FeatureManagerInit, 0, NULL},
#ifdef INCLUDE_TEMPERATURE
    {earbud_TemperatureInit,    0, NULL},
#endif
    {appBatteryInit,        MESSAGE_BATTERY_INIT_CFM, NULL},
#ifdef INCLUDE_CHARGER
    {Charger_Init,          0, NULL},
#endif
#ifdef INCLUDE_CAPSENSE
    {TouchSensor_Init,      0, NULL},
#endif
    //{LedManager_Init,       0, NULL},
    {earbud_BatteryRegionInit,        0, NULL},
    {appPowerInit,          APP_POWER_INIT_CFM, NULL},
    {earbud_SoCInit,        0, NULL},
    {appConnectionInit,     INIT_CL_CFM, NULL},
    {aul_init,       0, NULL},
#ifdef UNMAP_AFH_CH78
    {earbud_RemapAfh78,       0, NULL},
#endif
    {appMessageDispatcherRegister, 0, NULL},
#ifdef USE_BDADDR_FOR_LEFT_RIGHT
    {appConfigInit,         INIT_READ_LOCAL_NAME_CFM, appInitHandleReadLocalBdAddrCfm},
#endif
    {appInputEventMangerInit, 0, NULL},
    {appPhyStateInit,       PHY_STATE_INIT_CFM, NULL},
    {LocalAddr_Init,        0, NULL},
    {ConManagerInit,        0, NULL},
    {appLinkPolicyInit,     0, NULL},
    {CommonRules_Init,      0, NULL},
    {PrimaryRules_Init,     0, NULL},
    {SecondaryRules_Init,   0, NULL},
    {appInitDeviceDbSerialiser, 0, NULL},
    {UiUserConfig_Init,     0, NULL},
    {appDeviceInit,         INIT_READ_LOCAL_BD_ADDR_CFM, appDeviceHandleClDmLocalBdAddrCfm},
    {BandwidthManager_Init, 0, NULL},
    {BredrScanManager_Init, BREDR_SCAN_MANAGER_INIT_CFM, NULL},
    {LocalName_Init, LOCAL_NAME_INIT_CFM, NULL},
    {LeAdvertisingManager_Init,     0, NULL},
    {LeScanManager_Init,     0, NULL},
    {AudioSources_Init,      0, NULL},
    {VoiceSources_Init,      0, NULL},
    {Volume_InitMessages,   0, NULL},
    {VolumeService_Init,    0, NULL},
    {appAvInit,             AV_INIT_CFM, NULL},
    {appPeerSigInit,        PEER_SIG_INIT_CFM, NULL},
    {LogicalInputSwitch_Init,     0, NULL},
    {Pairing_Init,          PAIRING_INIT_CFM, NULL},
    {FocusSelect_Init,   0, NULL},
    {Telephony_InitMessages, 0, NULL},
    {TelephonyService_Init, 0, NULL},
    {HfpProfile_Init,            APP_HFP_INIT_CFM, NULL},
    {SingleEntity_Init,     0, NULL},
#ifdef INCLUDE_QCOM_CON_MANAGER
    {QcomConManagerInit,QCOM_CON_MANAGER_INIT_CFM,NULL},
#endif
    {KeySync_Init,          0, NULL},
#ifdef INCLUDE_MIRRORING
    {HandoverProfile_Init,  HANDOVER_PROFILE_INIT_CFM, NULL},
    {MirrorProfile_Init,    MIRROR_PROFILE_INIT_CFM, NULL},
#endif
#ifdef INCLUDE_USB_DEVICE
    {UsbDevice_Init,        0, NULL},
#endif
    {appKymeraInit,         0, NULL},
#ifdef ENABLE_ANC
    {AncStateManager_Init, 0, NULL},
#endif

#ifdef ENABLE_AEC_LEAKTHROUGH
    {AecLeakthrough_Init, 0, NULL},
#endif
    {StateProxy_Init,       0, NULL},
    {MediaPlayer_Init,       0, NULL},
    {appInitTransportManagerInitFixup, 0, NULL},        //! \todo TransportManager does not meet expected init interface
    {GattConnect_Init,      0, NULL},   // GATT functionality is initialised by calling GattConnect_Init then GattConnect_ServerInitComplete.
    {GattHandlerInit,       0, NULL},
    // All GATT Servers MUST be initialised after GattConnect_Init and before GattConnect_ServerInitComplete.
    {PeerPairLe_Init, INIT_PEER_PAIR_LE_CFM, NULL},
    {DeviceSync_Init,       0, NULL},
#ifdef INCLUDE_REMOTE_NAME
    {DeviceSyncPsKey_Init,  0, NULL},
#endif
    {ProfileManager_Init,   0, NULL},
    {HandsetService_Init,   0, NULL},
#ifdef INCLUDE_CASE_COMMS
    {CcWithCase_Init,             0, NULL},
#endif
    {PeerFindRole_Init,     INIT_PEER_FIND_ROLE_CFM, NULL},
    {TwsTopology_Init,      TWS_TOPOLOGY_INIT_CFM, NULL},
    {PeerLinkKeys_Init,     0, NULL},
#ifdef INCLUDE_GATT_BATTERY_SERVER
    {GattServerBattery_Init,0, NULL},
#endif
#ifdef INCLUDE_GATT_DEVICE_INFO_SERVER
    {GattServerDeviceInfo_Init, 0, NULL},
#endif
    {GattServerGatt_Init,   0, NULL},
    {GattServerGap_Init,    0, NULL},

#ifdef INCLUDE_ACCESSORY
    {Accessory_Init, 0, NULL},
    {Accessory_tws_Init, 0, NULL},
    {AccessoryFeature_RequestAppLaunchInit, 0, NULL},
    {Rtt_Init, 0, NULL},
#endif
#if defined(INCLUDE_MUSIC_PROCESSING)
    {MusicProcessing_Init,             0, NULL},
#endif /* INCLUDE_MUSIC_PROCESSING */
#ifdef INCLUDE_L2CAP_MANAGER
    {L2capManager_Init, 0, NULL},
#endif

#ifdef ENABLE_EARBUD_FIT_TEST
    {FitTest_init,0,NULL},
#endif

#ifdef INCLUDE_GAIA
    {GaiaFramework_Init,           APP_GAIA_INIT_CFM, NULL},   // Gatt needs GAIA
    {HandsetServicegGaiaPlugin_Init,  0, NULL},
#if defined(INCLUDE_DFU)
    {earbud_UpgradeGaiaPluginRegister, 0, NULL},
#endif
#if (defined(INCLUDE_GAIA_PYDBG_REMOTE_DEBUG) || defined(INCLUDE_GAIA_PANIC_LOG_TRANSFER))
    {GaiaDebugPlugin_Init,  0, NULL},
#endif
    {earbud_EarbudGaiaPluginRegister, 0, NULL},
#if defined(ENABLE_ANC)
    {earbud_AncGaiaPluginRegister, 0, NULL},
#endif
#if defined(INCLUDE_MUSIC_PROCESSING) && defined (INCLUDE_GAIA)
    {MusicProcessingGaiaPlugin_Init,             0, NULL},
#endif
#if defined(INCLUDE_CVC_DEMO) && defined (INCLUDE_GAIA)
    {VoiceEnhancementGaiaPlugin_Init,            0, NULL},
#endif
#ifdef ENABLE_GAIA_USER_FEATURE_LIST_DATA
    {EarbudGaiaUserFeature_RegisterUserFeatureData,  0, NULL},
#endif
#ifdef ENABLE_EARBUD_FIT_TEST
    {earbud_FitTestGaiaPluginRegister,0,NULL},
#endif
#endif /* INCLUDE_GAIA */
#ifdef INCLUDE_GAA_LE
    {GattServerGaaMedia_Init, 0, NULL},
    {GattServerGaaComm_Init, 0, NULL},
    {GattServerAmsProxy_Init, 0, NULL},
    {GattServerAncsProxy_Init, 0, NULL},
#endif
    {appSmInit,             0, NULL},
#ifdef INCLUDE_DFU
    {Dfu_EarlyInit, 0, NULL},
    {earbud_DfuAppRegister, 0, NULL},
    {Dfu_Init,        UPGRADE_INIT_CFM, NULL},
#endif
#ifdef INCLUDE_DFU_PEER
    {DfuPeer_EarlyInit, 0, NULL},
    {earbud_PeerDfuAppRegister, 0, NULL},
    {DfuPeer_Init,  DFU_PEER_INIT_CFM, NULL},
#endif
    {VoiceUi_Init,   0, NULL},
#ifdef ENABLE_AUDIO_TUNING_MODE
    {VoiceAudioTuningMode_Init, 0, NULL},
#endif
    {AudioCuration_Init, 0, NULL},
    {UiPrompts_Init,     0, NULL},
    {UiTones_Init,       0, NULL},
    {UiLeds_Init,        0, NULL},
    {PeerUi_Init,        0, NULL},
    {EarbudUi_Init,      0, NULL},
#ifdef INCLUDE_REMOTE_NAME
    {RemoteName_Init,    0, NULL},
#endif

#ifdef INCLUDE_MIRRORING
    {EarbudHandover_Init, 0, NULL},
#ifdef INCLUDE_GAMING_MODE
    {GamingMode_init, 0, NULL},
#endif
#endif
#ifdef INCLUDE_FAST_PAIR
    {appTxPowerInit, 0 , NULL},
    {FastPair_Init, 0, NULL},

#endif




#ifdef INCLUDE_GATT_SERVICE_DISCOVERY
    {GattServiceDiscovery_Init, 0, NULL},
#endif
    // All GATT Servers MUST be initialised before GATT initialisation is complete.
    {GattConnect_ServerInitComplete, GATT_CONNECT_SERVER_INIT_COMPLETE_CFM, NULL},
#ifdef INCLUDE_GAA
    {Gaa_Init, 0, NULL},
#endif

#ifdef INCLUDE_DEVICE_TEST_SERVICE
    {DeviceTestService_Init, 0, NULL},
#endif

#ifdef INCLUDE_AMA
    {Ama_Init, 0, NULL},
#endif

#ifdef INCLUDE_SWIFT_PAIR
    {SwiftPair_Init, 0, NULL},
#endif


    {Earbud_RegisterForBtMessages, 0, NULL},
};

static bool finalSleepStep(Task task)
{
#if defined HAVE_RDP_HW_YE134 || defined HAVE_RDP_HW_18689
bool enable_sensor_power = appPhyStateIsOutOfCase();  /* Keep touchpad powered when out of the case */
#endif
    UNUSED(task);

#if defined HAVE_RDP_HW_YE134 || defined HAVE_RDP_HW_18689
    /* When in case, we can turn off the sensor power supplies to save power.
     * If we're out of the case, we need to keep the power supplies enabled
     * so that the touchpad can wake us.
     *
     * In the case, we will only be woken on VCHG changing, or the hall
     * effect sensor on SYS_CTRL. These are non-maskable wake sources.
     */
    EarbudHardware_SetSensorPowerSupplies(all_supplies_mask, enable_sensor_power ? all_supplies_mask : 0);
    /* If we are out of the case, we want to be wakeable from the touchpad */
    appPowerEnterDormantMode(enable_sensor_power);
#else
    appPowerEnterDormantMode(TRUE);
#endif
    return TRUE;
}

static bool finalShutdownStep(Task task)
{
    UNUSED(task);

    appPowerDoPowerOff();

    return TRUE;
}

static const system_state_step_t sleep_table[] =
{
    {finalSleepStep, 0, NULL}
};

static const system_state_step_t shutdown_table[] =
{
    {finalShutdownStep, 0, NULL}
};

static void earbudInit_SetMessageBrokerRegistrations(void)
{
    unsigned registrations_array_dim = (unsigned)message_broker_group_registrations_end -
                              (unsigned)message_broker_group_registrations_begin;
    PanicFalse((registrations_array_dim % sizeof(message_broker_group_registration_t)) == 0);
    registrations_array_dim /= sizeof(message_broker_group_registration_t);

    MessageBroker_Init(message_broker_group_registrations_begin,
                       registrations_array_dim);
}

static void earbudInit_CompleteUiInitialisation(void)
{
    const ui_config_table_content_t* config_table;
    unsigned config_table_size;

    config_table = EarbudUi_GetConfigTable(&config_table_size);
    Ui_SetConfigurationTable(config_table, config_table_size);

    LogicalInputSwitch_SetLogicalInputIdRange(MIN_INPUT_ACTION_MESSAGE_ID,
                                              MAX_INPUT_ACTION_MESSAGE_ID);
#ifdef INCLUDE_CAPSENSE
    /* initialize touch UI event table */
    const touch_event_config_t *touch_events;
    touch_events = EarbudUi_GetCapsenseEventTable(&config_table_size);
    TouchSensorClientRegister(LogicalInputSwitch_GetTask(), config_table_size, touch_events);
#endif

    EarbudUi_ConfigureFocusSelection();

    PioMonitorEnable();

    /* Reset the touchpad so that it can handle user inputs.
     * We need to reset it after calling PioMonitorEnable in
     * order to handle any interrupts as soon as possible.
     * If the earbud is in the case, keep the touchpad held in
     * reset to save power. The touchpad will be powered on by
     * the phy state machine when the user takes it out of the
     * case.
     */
    bool hold_touchpad_in_reset = appPhyStateGetState() == PHY_STATE_IN_CASE;

    DEBUG_LOG_ALWAYS("hold_touchpad_in_reset = %d, appPhyStateGetState() = %d", hold_touchpad_in_reset, appPhyStateGetState());

#ifdef PRODUCTION_TEST_MODE
    /* If we are running in production test mode, we want to enable
     * the touchpad to allow it to be accessed by test operators or
     * even reprogrammed on the production line.
     */
    if (sm_boot_production_test_mode == appSmTestService_BootMode()) {
        hold_touchpad_in_reset = FALSE;
    }
#endif
    TouchSensor_Reset(hold_touchpad_in_reset);

    /* UI and App is completely initialized, system is ready for inputs */
}

void EarbudInit_StartInitialisation(void)
{
    Earbud_StartBtInit();

    earbudInit_SetMessageBrokerRegistrations();

    LedManager_SetHwConfig(&earbud_led_config);

    Earbud_SetBundlesConfig();

    SystemState_Init();
    SystemState_RemoveLimboState();

    SystemState_RegisterForStateChanges(appGetAppTask());
    SystemState_RegisterTableForInitialise(appInitTable, ARRAY_DIM(appInitTable));
    SystemState_RegisterTableForSleep(sleep_table, ARRAY_DIM(sleep_table));
    SystemState_RegisterTableForShutdown(shutdown_table, ARRAY_DIM(shutdown_table));
    SystemState_RegisterTableForEmergencyShutdown(shutdown_table, ARRAY_DIM(shutdown_table));

#ifdef INCLUDE_GAA
    Gaa_OtaSetSilentCommitSupported(UPGRADE_SILENT_COMMIT_SUPPORTED);
#endif
    SystemState_Initialise();
}

void EarbudInit_CompleteInitialisation(void)
{
    earbudInit_CompleteUiInitialisation();

    /* complete power manager initialisation to enable sleep */
    appPowerInitComplete();

    Earbud_SetupUnexpectedMessage();

    Earbud_SetupAudio();

#ifdef INCLUDE_GAA
    Gaa_InitComplete();
#endif

#ifdef INCLUDE_MUSIC_PROCESSING
    VoiceUi_SetEqInterface(&voice_ui_eq_if);
#endif

#if defined(INCLUDE_GAIA) && defined(INCLUDE_DFU)
    DEBUG_LOG_VERBOSE("Registration of SmGetTask() with GAIA");

    GaiaFrameworkInternal_ClientRegister(SmGetTask());
#endif

#ifdef INCLUDE_DFU
    Dfu_SetSilentCommitSupported(UPGRADE_SILENT_COMMIT_SUPPORTED);
#endif

#ifdef PLAY_PROMPTS_IN_CASE
    UiTones_SetTonePlaybackEnabled(TRUE);
    UiPrompts_SetPromptPlaybackEnabled(TRUE);
#endif

#ifdef ENABLE_LE_ADVERTISING_NO_RESTART_ON_DATA_UPDATE
    LeAdvertisingManager_ConfigureAdvertisingOnNotifyDataChange(le_adv_config_notify_keep_advertising);
#endif

    HfpProfile_BatteryLevelInit();

    SystemState_StartUp();
}

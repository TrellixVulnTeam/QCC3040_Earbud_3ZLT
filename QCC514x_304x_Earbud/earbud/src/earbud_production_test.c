/*!
\copyright  Copyright (c) 2005 - 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief	    Production test mode
*/

#ifdef PRODUCTION_TEST_MODE

/* local includes */
#include "app_task.h"
#include "earbud_sm.h"
#include "system_state.h"
#include "earbud_sm_private.h"
#include "earbud_led.h"
#include "adk_log.h"
#include "earbud_config.h"
#include "earbud_tones.h"
#include "earbud_production_test.h"
#include "anc_state_manager.h"

/* framework includes */
#include <connection_manager.h>
#include <kymera.h>
#include "kymera_sco_private.h"
#include "kymera_latency_manager.h"
#include <device_test_service_config.h>
#include <ps.h>
#include <pairing.h>
#include <bt_device.h>
#include <handset_service.h>
#include <ui.h>
#include <pio_common.h>
#include <microphones_config.h>


#ifdef FCC_TEST
#if TRAPSET_TEST2
#include <test2.h>
#else
#include <test.h>
#endif
#endif
/* dut modes - tx and rx */
typedef enum
{
    TEST_TX, 
    TEST_RX,
    TEST_DUT,
    TEST_DUT_AUDIO,
    TEST_PERIPHERALS,
    TEST_MIC,
    TEST_MIC_BCM,
    TEST_RF,
    TEST_CHARGING_CASE,
} TestMode;

typedef struct
{
    TestMode    mode;
    uint16      channel;
    uint16      level;
    uint16      modFreq;
    uint16      packetType;
    uint16      length;
} DutControl;

#if TRAPSET_TEST2
#define FCC_POWER_LEVEL 6
// Channel frequencies
#define CHANNEL0  0
#define CHANNEL39 39
#define CHANNEL40 40
#define CHANNEL78 78
// Packet types
#define TDH5  15
#define T2DH5 46
#define T3DH5 47
#else
#define FCC_POWER_LEVEL 10
// Channel frequencies
#define CHANNEL0  2402
#define CHANNEL39 2441
#define CHANNEL40 2442
#define CHANNEL78 2480

/* Packet types */
#define TDH5  15
#define T2DH5 30
#define T3DH5 31
#endif
// Packet lengths
#define LDH5  339
#define L2DH5 679
#define L3DH5 1021

#ifdef FCC_TEST
#ifdef ALL_FCC_LEVELS
#define MAX_DUT_MODES 15
#else
#define MAX_DUT_MODES 2
#endif
#else
#ifdef INCLUDE_BCM
#define MAX_DUT_MODES 4
#else
#define MAX_DUT_MODES 3
#endif
#endif
DutControl DutModes [MAX_DUT_MODES] = {
#ifdef FCC_TEST
    {TEST_DUT,0,0,0,0,0},
    {TEST_DUT_AUDIO,0,0,0,0,0},
#ifdef ALL_FCC_LEVELS
    {TEST_TX,CHANNEL0 ,FCC_POWER_LEVEL,0,TDH5 ,LDH5},
    {TEST_TX,CHANNEL0 ,FCC_POWER_LEVEL,0,T2DH5,L2DH5},
    {TEST_TX,CHANNEL0 ,FCC_POWER_LEVEL,0,T3DH5,L3DH5},
    {TEST_TX,CHANNEL39,FCC_POWER_LEVEL,0,TDH5 ,LDH5},
    {TEST_TX,CHANNEL39,FCC_POWER_LEVEL,0,T2DH5,L2DH5},
    {TEST_TX,CHANNEL39,FCC_POWER_LEVEL,0,T3DH5,L3DH5},
    {TEST_TX,CHANNEL78,FCC_POWER_LEVEL,0,TDH5 ,LDH5},
    {TEST_TX,CHANNEL78,FCC_POWER_LEVEL,0,T2DH5,L2DH5},
    {TEST_TX,CHANNEL78,FCC_POWER_LEVEL,0,T3DH5,L3DH5},
#endif
#else
    //{TEST_PERIPHERALS,0,0,0,0,0},
    {TEST_MIC,0,0,0,0,0},
#ifdef INCLUDE_BCM
    {TEST_MIC_BCM,0,0,0,0,0},
#endif
    {TEST_RF,CHANNEL40 ,FCC_POWER_LEVEL,0,TDH5 ,LDH5},
    {TEST_CHARGING_CASE,0,0,0,0,0},
#endif

};

static uint8 dutMode = 0;
static audio_mic_params mic_voice_config = {0};

void appSmTestService_SetTestStep(uint8 step)
{
    dutMode = step;
    /* Enable ANC for production tuning */
	Ui_InjectUiInput(ui_input_anc_on);

}

/*! The PSKEY to use to determine if Device Test Mode is enabled
 */
sm_boot_mode_t appSmTestService_BootMode(void)
{
    sm_boot_mode_t boot_mode = sm_boot_normal_mode;
    uint16 current_size_words;

    current_size_words = PsRetrieve(DeviceTestService_EnablingPskey(), NULL, 0);

    if (current_size_words)
    {
        uint16 *key_storage = PanicUnlessMalloc(current_size_words * sizeof(uint16));

        if (PsRetrieve(DeviceTestService_EnablingPskey(),
                       key_storage,
                       current_size_words) >= 1)
        {
            boot_mode = key_storage[0];
        }
        free(key_storage);
    }

    DEBUG_LOG("appSmTestService_BootMode : %d", boot_mode);

    return boot_mode;
}

/*! \brief Write Device Test Service mode PS Key 
    1 : enter test mode after reboot
    0 : leave test mode after next reboot
*/
void appSmTestService_SaveBootMode(sm_boot_mode_t mode)
{
    uint16 already_exists;
    uint16 current_size_words;
    uint16 written_words;
    uint16 *key_storage;

    already_exists = PsRetrieve(DeviceTestService_EnablingPskey(), NULL, 0);
    current_size_words = MAX(1, already_exists);
    key_storage = PanicUnlessMalloc(current_size_words * sizeof(uint16));

    if (already_exists)
    {
        if (!PsRetrieve(DeviceTestService_EnablingPskey(),
                        key_storage,
                        current_size_words) >= 1)
        {
            Panic();
        }
    }
    key_storage[0] = mode;

    written_words = PsStore(DeviceTestService_EnablingPskey(),
                            key_storage, current_size_words);
    free(key_storage);

    if (written_words != current_size_words)
    {
        DEBUG_LOG("appSmTestService_SaveBootMode Unable to save mode. %d words written", 
                    written_words);
    }
    else
    {
        DEBUG_LOG("appSmTestService_SaveBootMode Saved mode:%d. %d words written", 
                    mode, written_words);
    }
}


/*! \brief Handle request to enter DUT test mode. */
static void appSmDynamicConfigureTalkMic(microphone_number_t mic_id)
{
    /* load voice mic with original mic config before changing anything */
    Microphones_SetMicrophoneConfig(appConfigMicVoice(), &mic_voice_config);
    /* update voice mic config with the wanted mic */
    Microphones_SetMicrophoneConfig(appConfigMicVoice(), Microphones_GetMicrophoneConfig(mic_id));
#ifdef KYMERA_SCO_USE_2MIC
#if defined HAVE_RDP_HW_YE134 || defined HAVE_RDP_HW_18689
    /* make the 2nd mic off by configuring active high */
    PioSetActiveLevel(appConfigMic2Pio(), TRUE);
#endif
#endif
}

/*! \brief Handle request to enter DUT test mode. */
void appSmHandleInternalEnterDUTTestMode(void)
{
    static uint8 audio = 0;

    DEBUG_LOG_VERBOSE("DUT Mode test audio %d", audio);

    if (audio == 0)
    {
        appSmClearUserPairing();
        ConManagerAllowHandsetConnect(TRUE);
        ConnectionEnterDutMode();
        audio = 1;
    }
    else
    {
        appKymeraTonePlay(dut_mode_tone, 0, TRUE, NULL, 0);
        MessageSendLater(SmGetTask(), SM_INTERNAL_ENTER_DUT_TEST_MODE, NULL, 1000);
    }
}

/*! \brief Handle request to enter FCC test mode. */
void appSmHandleInternalEnterProductionTestMode(void)
{
    DEBUG_LOG_VERBOSE("appSmHandleInternalEnterProductionTestMode");
#ifdef INCLUDE_BCM
    static bool restore_mic = FALSE;
#endif
#ifdef PRODUCTION_TEST_MODE
#ifdef FCC_TEST
#if TRAPSET_TEST2
     HopChannels channels = {CHANNEL0, CHANNEL0, CHANNEL0, CHANNEL0, CHANNEL0};
     bdaddr addr =  {0x000000,0x00,0x00};
     uint8 i = 0;
#endif /* TRAPSET_TEST2 */
#endif /* FCC_TEST */
if (sm_boot_production_test_mode == appSmTestService_BootMode())
{ 
    /* Ensure that ANC is in a static mode for the production line tuning */
    /* Make sure toggling does nothing */
    AncStateManager_SetAncToggleConfiguration(0, anc_toggle_config_not_configured);
    AncStateManager_SetAncToggleConfiguration(1, anc_toggle_config_not_configured);
    /* Make sure the audo ANC mode switch does nothing */
    AncStateManager_SetAncScenarioConfiguration(anc_scenario_config_id_standalone, anc_toggle_config_is_same_as_current);
    AncStateManager_SetAncScenarioConfiguration(anc_scenario_config_id_sco,        anc_toggle_config_is_same_as_current);
    AncStateManager_SetAncScenarioConfiguration(anc_scenario_config_id_playback,   anc_toggle_config_is_same_as_current);
    AncStateManager_SetAncScenarioConfiguration(anc_scenario_config_id_va,         anc_toggle_config_is_same_as_current);
    /* We want mode 2, static */
	/* Note, this must match a suitable ANC configuration from the table anc_config_data, in anc_config.c */
    AncStateManager_SetMode(anc_mode_2);
    Kymera_ScoSetCvcPassthroughMode(KYMERA_CVC_RECEIVE_PASSTHROUGH | KYMERA_CVC_SEND_PASSTHROUGH, 0);

    if(dutMode == 1)
    {
		/* save voice mic config for recovery */
        memcpy(&mic_voice_config, Microphones_GetMicrophoneConfig(appConfigMicVoice()), sizeof(audio_mic_params));
    }

    if (dutMode > MAX_DUT_MODES) 
    {
        dutMode = 0;
        /* update the PS key to boot into mute mode*/
        appSmTestService_SaveBootMode(sm_boot_normal_mode);
        /* exit device undet test mode */
        Panic();
        return;
    }

    DutControl * mode = &DutModes[dutMode-1];

#ifdef INCLUDE_BCM
	if (restore_mic)
	{
        /* load voice mic with original mic config after BCM TEST */
		Microphones_SetMicrophoneConfig(appConfigMicVoice(), &mic_voice_config);
        restore_mic = FALSE;
	}
#endif

    switch (mode->mode) {
    case TEST_PERIPHERALS:
        /* Flash LED indication for Operator to verify LED and Touch works */
        appUiFCCDH5();
        break;
    case TEST_MIC:
        appUiFCC2DH5();
        Ui_InjectUiInput(ui_input_anc_off);
        appSmClearUserPairing();
        ConManagerAllowHandsetConnect(TRUE);
        Pairing_Pair(SmGetTask(), FALSE);
        HandsetService_ConnectableRequest(SmGetTask());
        /* Configure voice mic as talk mic*/
		appSmDynamicConfigureTalkMic(appConfigMicVoice());
        break;
#ifdef INCLUDE_BCM
	case TEST_MIC_BCM:
		/* configure BCM as talk Mic*/
		appSmDynamicConfigureTalkMic(appConfigMicBCM());
        restore_mic = TRUE;
		break;
#endif
    case TEST_RF:
        Pairing_PairStop(SmGetTask());
        Ui_InjectUiInput(ui_input_anc_on);
        ConManagerAllowHandsetConnect(TRUE);
        ConnectionEnterDutMode();
        appUiFCC3DH5();
        break;
    case TEST_CHARGING_CASE:
        appIdleProduction();
        break;
#ifdef FCC_TEST
    case TEST_DUT:
        DEBUG_LOG("Going to DUT mode");
        MessageSendLater(SmGetTask(), SM_INTERNAL_ENTER_DUT_TEST_MODE, NULL, 10);
        break;

    case TEST_DUT_AUDIO:
        DEBUG_LOG("Going to DUT mode with audio");
        MessageSendLater(SmGetTask(), SM_INTERNAL_ENTER_DUT_TEST_MODE, NULL, 10);
    break;
    case TEST_TX:
        DEBUG_LOG("Going to FCC mode");
        /* stop audio */
        MessageCancelAll(SmGetTask(), SM_INTERNAL_ENTER_DUT_TEST_MODE);
        switch(mode->packetType) {
            case TDH5: /* One fast flash */
               appUiFCCDH5();
               DEBUG_LOG("DH5 Tx test Channel %d", mode->channel);
               break;
            case T2DH5: /* Two fast flashes */
                appUiFCC2DH5();
                DEBUG_LOG("2-DH5 Tx test Channel %d", mode->channel);
                break;
            case T3DH5:
                appUiFCC3DH5();
                DEBUG_LOG("3-DH5 Tx test Channel %d", mode->channel);
                break;
            default: /* 3 fast flashes */
                break;
            }
#if TRAPSET_TEST2
            for (i=0; i<5; i++)
            {
                channels[i] = mode->channel;
            }
            if(!appDeviceGetMyBdAddr((bdaddr*)&addr))
            {
                DEBUG_LOG("Can't retrieve BT address");
            }
            else
            {
                DEBUG_LOG("BT addressaddr %04x,%02x,%06lx", addr.nap, addr.uap, addr.lap);
            }
    
            Test2TxData(&channels, mode->level, 0, 0x04, mode->packetType, mode->length, (const bdaddr*)&addr, 1);
#else /* TRAPSET_TEST2 */
            TestCfgPkt(mode->packetType, mode->length);
            TestTxPower(mode->level);
            TestTxStart(mode->channel, 0, 0);
            TestTxData1(mode->channel, mode->level);
            TestCfgPkt(mode->packetType, mode->length);
#endif /* TRAPSET_TEST2 */
            DEBUG_LOG_VERBOSE("Finish TEST_TX", mode->channel);
            break;
#endif /* FCC_TEST */
        default:
            break;
        }
        /* go to the next mode */
        dutMode++;
    }
#endif /* PRODUCTION_TEST_MODE */
}


/*! \brief Request To enter Production Test mode */
void appSmEnterProductionTestMode(void)
{
    MessageSend(SmGetTask(), SM_INTERNAL_ENTER_PRODUCTION_TEST_MODE, NULL);
}
#endif /*PRODUCTION_TEST_MODE*/

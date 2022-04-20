/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       adk_test_va.c
\brief      Implementation of specifc application testing functions
*/

#ifndef DISABLE_TEST_API

#include "adk_test_va.h"
#include <ama.h>
#include <ama_audio.h>
#include <connection_manager.h>
#include <logical_input_switch.h>
#include <multidevice.h>
#include <voice_ui_audio.h>
#include <voice_ui_container.h>
#include <voice_ui_va_client_if.h>
#include <logging.h>

#ifdef GC_SECTIONS
/* Move all functions in KEEP_PM section to ensure they are not removed during
 * garbage collection */
#pragma unitcodesection KEEP_PM
static const void *tableOfAdkSymbolsToKeep[] = {
    (void*)appTestVaTap,
    (void*)appTestVaDoubleTap,
    (void*)appTestVaPressAndHold,
    (void*)appTestVaRelease,
    (void*)appTestVaHeldRelease,
    (void*)appTest_VaGetSelectedAssistant,
    (void*)appTestSetActiveVa2GAA,
    (void*)appTestSetActiveVa2AMA,
    (void*)appTestIsVaAudioActive,
#ifdef INCLUDE_AMA
    (void*)appTestPrintAmaLocale,
#endif
};
void appTestShowKeptAdkSymbols(void);
void appTestShowKeptAdkSymbols(void)
{
    for( size_t i = 0 ; i < sizeof(tableOfAdkSymbolsToKeep)/sizeof(tableOfAdkSymbolsToKeep[0]) ; i++ )
    {
        if( tableOfAdkSymbolsToKeep[i] != NULL )
        {
            DEBUG_LOG_ALWAYS("Have %p",tableOfAdkSymbolsToKeep[i]);
        }
    }
}

#endif

void appTestVaTap(void)
{
    DEBUG_LOG_ALWAYS("appTestVaTap");
    /* Simulates a "Button Down -> Button Up -> Single Press Detected" sequence
    for the default configuration of a dedicated VA button */
    if (Multidevice_IsPair())
    {
        LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_va_1);
        LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_va_6);
        LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_va_3);
    }
    else
    {
        Ui_InjectUiInput(ui_input_va_1);
        Ui_InjectUiInput(ui_input_va_6);
        Ui_InjectUiInput(ui_input_va_3);
    }
}

void appTestVaDoubleTap(void)
{
    DEBUG_LOG_ALWAYS("appTestVaDoubleTap");
    /* Simulates a "Button Down -> Button Up -> Button Down -> Button Up -> Double Press Detected"
    sequence for the default configuration of a dedicated VA button */
    if (Multidevice_IsPair())
    {
        LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_va_1);
        LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_va_6);
        LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_va_1);
        LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_va_6);
        LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_va_4);
    }
    else
    {
        Ui_InjectUiInput(ui_input_va_1);
        Ui_InjectUiInput(ui_input_va_6);
        Ui_InjectUiInput(ui_input_va_1);
        Ui_InjectUiInput(ui_input_va_6);
        Ui_InjectUiInput(ui_input_va_4);
    }
}

void appTestVaPressAndHold(void)
{
    DEBUG_LOG_ALWAYS("appTestVaPressAndHold");
    /* Simulates a "Button Down -> Hold" sequence for the default configuration
    of a dedicated VA button */
    if (Multidevice_IsPair())
    {
        LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_va_1);
        LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_va_5);
    }
    else
    {
        Ui_InjectUiInput(ui_input_va_1);
        Ui_InjectUiInput(ui_input_va_5);
    }
}

void appTestVaRelease(void)
{
    DEBUG_LOG_ALWAYS("appTestVaRelease");
    /* Simulates a "Button Up" event for the default configuration
    of a dedicated VA button */
    if (Multidevice_IsPair())
    {
        LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_va_6);
    }
    else
    {
        Ui_InjectUiInput(ui_input_va_6);
    }
}

void appTestVaHeldRelease(void)
{
    DEBUG_LOG_ALWAYS("appTestVaHeldRelease");
    /* Simulates a "Long Press" event for the default configuration
    of a dedicated VA button */
    if (Multidevice_IsPair())
    {
        LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_va_1);
        LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_va_2);
    }
    else
    {
        Ui_InjectUiInput(ui_input_va_1);
        Ui_InjectUiInput(ui_input_va_2);
    }

}

unsigned appTest_VaGetSelectedAssistant(void)
{
    voice_ui_provider_t va = VoiceUi_GetSelectedAssistant();

    DEBUG_LOG_DEBUG("appTestGetActiveVa: enum:voice_ui_provider_t:%u", va);
    return va;
}

void appTestSetActiveVa2GAA(void)
{
#ifdef INCLUDE_GAA
    VoiceUi_SelectVoiceAssistant(voice_ui_provider_gaa, voice_ui_reboot_allowed);
#else
    DEBUG_LOG_ALWAYS("Gaa not included in the build");
#endif
}


void appTestSetActiveVa2AMA(void)
{
#ifdef INCLUDE_AMA
    VoiceUi_SelectVoiceAssistant(voice_ui_provider_ama, voice_ui_reboot_allowed);
#else
    DEBUG_LOG_ALWAYS("AMA not included in the build");
#endif
}

bool appTestIsVaAudioActive(void)
{
    return VoiceUi_IsVaActive();
}
#ifdef INCLUDE_AMA
void appTestPrintAmaLocale(void)
{
    char locale[AMA_LOCALE_STR_SIZE];

    if (AmaAudio_GetDeviceLocale(locale, sizeof(locale)))
    {
        DEBUG_LOG_ALWAYS("appTestPrintAmaLocale: \"%c%c%c%c%c\"",
                            locale[0], locale[1], locale[2], locale[3], locale[4]);
    }
    else
    {
        DEBUG_LOG_ALWAYS("appTestPrintAmaLocale: Failed to get locale");
    }
}
void appTestSetAmaLocale(const char *locale)
{
    DEBUG_LOG_ALWAYS("appTestSetAmaLocale: \"%c%c%c%c\"",
                        locale[0], locale[1], locale[2], locale[3]);
    VoiceUi_SetPackedLocale((uint8*)locale);
}
#endif

bool appTestIsVaDeviceInSniff(void)
{
    const voice_ui_handle_t * va = VoiceUi_GetActiveVa();
    lp_power_mode mode = lp_active;

    if (va && va->voice_assistant && va->voice_assistant->GetBtAddress)
    {
        const bdaddr * va_address = va->voice_assistant->GetBtAddress();
        tp_bdaddr tpaddr;
        BdaddrTpFromBredrBdaddr(&tpaddr, va_address);

        if(ConManagerGetPowerMode(&tpaddr,&mode))
        {
            DEBUG_LOG("appTestIsVaDeviceInSniff %d", mode);
        }
        else
        {
            DEBUG_LOG_WARN("appTestIsVaDeviceInSniff not able to get power mode");
        }
    }
    else
    {
        DEBUG_LOG_WARN("appTestIsVaDeviceInSniff no active VA");
    }

    return (mode==lp_sniff);
}

#endif /* DISABLE_TEST_API */

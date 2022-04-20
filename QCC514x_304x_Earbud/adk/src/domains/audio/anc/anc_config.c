/*!
\copyright  Copyright (c) 2019 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       anc_config.c
\brief      Configuration for microphones and stubs for Active Noise Cancellation (ANC).
            Static conifguration for ANC FF, FB, HY modes.
*/


#ifdef ENABLE_ANC
#include <logging.h>
#include "anc_config.h"
#include "kymera_config.h"
#include "anc_state_manager.h"

/*! \brief Max ANC Modes that can be configured */
#define ANC_CONFIG_MAX_MODE 10

/*! \brief ANC Configuration data */
typedef struct
{
    bool is_adaptive;        /*If the ANC mode is configured as adaptive ANC, Else Static*/
    bool is_leakthrough;     /*If the ANC mode is configured as LKT/Transparent mode, Else Noise Cancellation*/
} anc_config_data_t;

/*! \brief ANC Config Data 
    This table gives example configuration for use of different ANC modes. 
    The actual mode configuration for a product needs to be defined at the customer end.

    IMPORTANT: IT IS MANDATORY TO UPDATE THIS TABLE IN SYNC WITH THE TUNING OF ANC
    RECOMMENDATION: It is recommended that the modes are configured in sequence.
    appConfigNumOfAncModes() to be updated to the configured number of modes.
*/
#ifdef ENABLE_ADAPTIVE_ANC

#ifdef HAVE_RDP_UI 
/*Change the config according to the tuning file for RDP*/
const anc_config_data_t anc_config_data[ANC_CONFIG_MAX_MODE] = 
{
    {ANC_CONFIG_MODE_ADAPTIVE, ANC_CONFIG_MODE_NOISE_CANCELLATION}, /*anc_mode_1*/ /* Balanced Adaptive ANC (Deep Flat tuning) EANC*/
    {ANC_CONFIG_MODE_STATIC, ANC_CONFIG_MODE_NOISE_CANCELLATION},   /*anc_mode_2*/ /* Balanced Static ANC (Deep Flat tuning) EANC */
    {ANC_CONFIG_MODE_STATIC, ANC_CONFIG_MODE_NOISE_CANCELLATION},   /*anc_mode_3*/ /* Static ANC (Deep/Peak Performance tuning) EANC*/
    {ANC_CONFIG_MODE_STATIC, ANC_CONFIG_MODE_NOISE_CANCELLATION},   /*anc_mode_4*/ /* Static ANC (Wide tuning) EANC */
    {ANC_CONFIG_MODE_STATIC, ANC_CONFIG_MODE_LEAKTHROUGH},          /*anc_mode_5*/ /* Static ANC (Transparency tuning) EANC */
    {ANC_CONFIG_MODE_STATIC, ANC_CONFIG_MODE_NOISE_CANCELLATION},   /*anc_mode_6*/ /* Not configured */
    {ANC_CONFIG_MODE_STATIC, ANC_CONFIG_MODE_NOISE_CANCELLATION},   /*anc_mode_7*/ /* Custom Preset A */
    {ANC_CONFIG_MODE_STATIC, ANC_CONFIG_MODE_NOISE_CANCELLATION},   /*anc_mode_8*/ /* Custom Preset B */
    {ANC_CONFIG_MODE_STATIC, ANC_CONFIG_MODE_NOISE_CANCELLATION},   /*anc_mode_9*/ /* Custom Preset C */
    {ANC_CONFIG_MODE_STATIC, ANC_CONFIG_MODE_NOISE_CANCELLATION},   /*anc_mode_10*//* Custom Preset D */
};
#elif defined(CORVUS_YD300)
/*Change the config according to the tuning file for Corvus*/
const anc_config_data_t anc_config_data[ANC_CONFIG_MAX_MODE] = 
{
    {ANC_CONFIG_MODE_ADAPTIVE, ANC_CONFIG_MODE_NOISE_CANCELLATION}, /*anc_mode_1*/ 
    {ANC_CONFIG_MODE_STATIC, ANC_CONFIG_MODE_LEAKTHROUGH},          /*anc_mode_2*/ 
    {ANC_CONFIG_MODE_STATIC, ANC_CONFIG_MODE_NOISE_CANCELLATION},   /*anc_mode_3*/ 
    {ANC_CONFIG_MODE_STATIC, ANC_CONFIG_MODE_NOISE_CANCELLATION},   /*anc_mode_4*/ 
    {ANC_CONFIG_MODE_STATIC, ANC_CONFIG_MODE_NOISE_CANCELLATION},   /*anc_mode_5*/ 
    {ANC_CONFIG_MODE_STATIC, ANC_CONFIG_MODE_NOISE_CANCELLATION},   /*anc_mode_6*/ 
    {ANC_CONFIG_MODE_STATIC, ANC_CONFIG_MODE_NOISE_CANCELLATION},   /*anc_mode_7*/ 
    {ANC_CONFIG_MODE_STATIC, ANC_CONFIG_MODE_NOISE_CANCELLATION},   /*anc_mode_8*/
    {ANC_CONFIG_MODE_STATIC, ANC_CONFIG_MODE_NOISE_CANCELLATION},   /*anc_mode_9*/
    {ANC_CONFIG_MODE_STATIC, ANC_CONFIG_MODE_NOISE_CANCELLATION},   /*anc_mode_10*/
};
#else
/*Change the config according to the tuning file for the selected platform*/
const anc_config_data_t anc_config_data[ANC_CONFIG_MAX_MODE] = 
{
    {ANC_CONFIG_MODE_ADAPTIVE, ANC_CONFIG_MODE_NOISE_CANCELLATION}, /*anc_mode_1*/  /*Adaptive ANC*/
    {ANC_CONFIG_MODE_STATIC, ANC_CONFIG_MODE_NOISE_CANCELLATION}, /*anc_mode_2*/  /*Static hybrid mode*/
    {ANC_CONFIG_MODE_STATIC, ANC_CONFIG_MODE_NOISE_CANCELLATION}, /*anc_mode_3*/  /*Static hybrid mode*/
    {ANC_CONFIG_MODE_STATIC, ANC_CONFIG_MODE_NOISE_CANCELLATION}, /*anc_mode_4*/  /*Static hybrid mode*/
    {ANC_CONFIG_MODE_STATIC, ANC_CONFIG_MODE_NOISE_CANCELLATION}, /*anc_mode_5*/  /*Static hybrid mode*/
    {ANC_CONFIG_MODE_STATIC, ANC_CONFIG_MODE_NOISE_CANCELLATION}, /*anc_mode_6*/  /*Static hybrid mode*/
    {ANC_CONFIG_MODE_STATIC, ANC_CONFIG_MODE_NOISE_CANCELLATION}, /*anc_mode_7*/  /*Static hybrid mode*/
    {ANC_CONFIG_MODE_STATIC, ANC_CONFIG_MODE_NOISE_CANCELLATION}, /*anc_mode_8*/  /*Static hybrid mode*/
    {ANC_CONFIG_MODE_STATIC, ANC_CONFIG_MODE_NOISE_CANCELLATION}, /*anc_mode_9*/  /*Static hybrid mode*/
    {ANC_CONFIG_MODE_STATIC, ANC_CONFIG_MODE_NOISE_CANCELLATION}, /*anc_mode_10*/ /*Static hybrid mode*/
};
#endif

#else /*Static ANC build*/
/*Change the config according to the tuning file for static ANC*/
const anc_config_data_t anc_config_data[ANC_CONFIG_MAX_MODE] = 
{
    {ANC_CONFIG_MODE_STATIC, ANC_CONFIG_MODE_NOISE_CANCELLATION}, /*anc_mode_1*/  /*Static hybrid mode, passthrough configuration*/
    {ANC_CONFIG_MODE_STATIC, ANC_CONFIG_MODE_NOISE_CANCELLATION}, /*anc_mode_2*/  /*Static hybrid mode, passthrough configuration*/
    {ANC_CONFIG_MODE_STATIC, ANC_CONFIG_MODE_NOISE_CANCELLATION}, /*anc_mode_3*/  /*Static hybrid mode, passthrough configuration*/
    {ANC_CONFIG_MODE_STATIC, ANC_CONFIG_MODE_NOISE_CANCELLATION}, /*anc_mode_4*/  /*Static hybrid mode, passthrough configuration*/
    {ANC_CONFIG_MODE_STATIC, ANC_CONFIG_MODE_NOISE_CANCELLATION}, /*anc_mode_5*/  /*Static hybrid mode, passthrough configuration*/
    {ANC_CONFIG_MODE_STATIC, ANC_CONFIG_MODE_NOISE_CANCELLATION}, /*anc_mode_6*/  /*Static hybrid mode, passthrough configuration*/
    {ANC_CONFIG_MODE_STATIC, ANC_CONFIG_MODE_NOISE_CANCELLATION}, /*anc_mode_7*/  /*Static hybrid mode, passthrough configuration*/
    {ANC_CONFIG_MODE_STATIC, ANC_CONFIG_MODE_NOISE_CANCELLATION}, /*anc_mode_8*/  /*Static hybrid mode, passthrough configuration*/
    {ANC_CONFIG_MODE_STATIC, ANC_CONFIG_MODE_NOISE_CANCELLATION}, /*anc_mode_9*/  /*Static hybrid mode, passthrough configuration*/
    {ANC_CONFIG_MODE_STATIC, ANC_CONFIG_MODE_NOISE_CANCELLATION}, /*anc_mode_10*/ /*Static hybrid mode, passthrough configuration*/
};
#endif


/*
 * There is no config manager setup yet, so hard-code the default value as Feed Forward Mode on Analog Mic from
 * Kymera_config for reference.
 */
#ifdef INCLUDE_STEREO
#define getFeedForwardLeftMicConfig()   (appConfigAncFeedForwardLeftMic())
#define getFeedForwardRightMicConfig()  (appConfigAncFeedForwardRightMic())
#define getFeedBackLeftMicConfig()      (appConfigAncFeedBackLeftMic())
#define getFeedBackRightMicConfig()     (appConfigAncFeedBackRightMic())
#else
#define getFeedForwardLeftMicConfig()   (appConfigAncFeedForwardMic())
#define getFeedForwardRightMicConfig()  (microphone_none)
#define getFeedBackLeftMicConfig()      (appConfigAncFeedBackMic())
#define getFeedBackRightMicConfig()     (microphone_none)
#endif

anc_readonly_config_def_t anc_readonly_config =
{
    .anc_mic_params_r_config = {
        .feed_forward_left_mic = getFeedForwardLeftMicConfig(),
        .feed_forward_right_mic = getFeedForwardRightMicConfig(),
        .feed_back_left_mic = getFeedBackLeftMicConfig(),
        .feed_back_right_mic = getFeedBackRightMicConfig(),
     },
     .num_anc_modes = appConfigNumOfAncModes(),
};

/* Write to persistance is not enabled for now and set to defaults */
static anc_writeable_config_def_t anc_writeable_config = {

    .persist_initial_mode = appConfigAncMode(),
    .persist_initial_state = anc_state_manager_uninitialised,
    .initial_anc_state = anc_state_manager_uninitialised,
    .initial_anc_mode = appConfigAncMode(),
};


uint16 ancConfigManagerGetReadOnlyConfig(uint16 config_id, const void **data)
{
    UNUSED(config_id);
    *data = &anc_readonly_config;
    DEBUG_LOG("ancConfigManagerGetReadOnlyConfig\n");
    return (uint16) sizeof(anc_readonly_config);
}

void ancConfigManagerReleaseConfig(uint16 config_id)
{
    UNUSED(config_id);
    DEBUG_LOG("ancConfigManagerReleaseConfig\n");
}

uint16 ancConfigManagerGetWriteableConfig(uint16 config_id, void **data, uint16 size)
{
    UNUSED(config_id);
    UNUSED(size);
    *data = &anc_writeable_config;
    DEBUG_LOG("ancConfigManagerGetWriteableConfig\n");
    return (uint16) sizeof(anc_writeable_config);
}

void ancConfigManagerUpdateWriteableConfig(uint16 config_id)
{
    UNUSED(config_id);
    DEBUG_LOG("ancConfigManagerUpdateWriteableConfig\n");
}

bool AncConfig_IsAncModeAdaptive(anc_mode_t anc_mode)
{
    if (anc_mode >= appConfigNumOfAncModes())
    {
        return FALSE;
    }
    return anc_config_data[anc_mode].is_adaptive;
}

bool AncConfig_IsAncModeLeakThrough(anc_mode_t anc_mode)
{
    if (anc_mode >= appConfigNumOfAncModes())
    {
        return FALSE;
    }
    return anc_config_data[anc_mode].is_leakthrough;
}

bool AncConfig_IsAncModeStatic(anc_mode_t anc_mode)
{
    if (anc_mode >= appConfigNumOfAncModes())
    {
        return FALSE;
    }
    return (!anc_config_data[anc_mode].is_adaptive);
}


#endif /* ENABLE_ANC */

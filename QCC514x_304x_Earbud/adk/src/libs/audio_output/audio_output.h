/******************************************************************************
Copyright (c) 2014 - 2015 Qualcomm Technologies International, Ltd.


FILE NAME
    audio_output.h

DESCRIPTION
    Plugin that implements multichannel audio by utilising multiple hardware
    audio outputs (onboard DAC, I2S, etc...).
*/


/*!
@file    audio_output.h
@brief   Implements multichannel audio by utilising multiple hardware audio
         outputs (onboard DAC, I2S, etc...).

         This file provides documentation for the audio_output API.

         Plugin should be initialised with a suitable output mapping before
         being used, defined by the structures declared in this header file.
*/


#ifndef __AUDIO_OUTPUT_H__
#define __AUDIO_OUTPUT_H__

/*============================================================================*
 *  SDK Header Files
 *============================================================================*/

#include <csrtypes.h>
#include <stream.h>

/*============================================================================*
 *  Public Defintions
 *============================================================================*/

/* Turn audio output into a bitmask */
#define AudioOutputGetMask(audio_out) ((uint16)1 << audio_out)

/*============================================================================*
 *  Public Data Types
 *============================================================================*/

/*!
    @brief Audio Output Channel

    Allows identification of and looping through each output channel. The first
    output should always start at 0, and audio_output_max should always be last
    as it is also used to define arrays and loop bounds.
*/
typedef enum __audio_output
{
    audio_output_primary_left = 0,   /*! Left primary (filtered) output */
    audio_output_primary_right,      /*! Right primary (filtered) output */
    audio_output_secondary_left,     /*! Left secondary (filtered) output */
    audio_output_secondary_right,    /*! Right secondary (filtered) output */
    audio_output_wired_sub,          /*! Wired subwoofer (LP filter) output */
    audio_output_aux_left,           /*! Left auxiliary (full range) output */
    audio_output_aux_right,          /*! Right auxiliary (full range) output */
    audio_output_max                 /*! NOT A VALID OUTPUT: For looping/arrays */
} audio_output_t;

/*!
    @brief Audio Output Hardware Type

    Defines the possible audio output hardware types supported by the plugin.
*/
typedef enum __audio_output_hardware_type
{
    audio_output_type_none = 0, /*! NONE: This output should be disabled */
    audio_output_type_dac,      /*! Internal hardware DAC (codec) */
    audio_output_type_i2s,      /*! Digital output to external DAC */
    audio_output_type_spdif     /*! SPDIF output */
} audio_output_hardware_type_t;

/*!
    @brief Audio Output Hardware Instance

    Instance of the audio output hardware type. Some platforms have multiple
    DAC or I2S outputs, for example.
*/
typedef enum __audio_output_hardware_instance
{
    audio_output_hardware_instance_0 = 0,   /*! First instance */
    audio_output_hardware_instance_1,       /*! Second instance */
    audio_output_hardware_instance_2,       /*! Third instance */
    audio_output_hardware_instance_max      /*! NOT A VALID INSTANCE */
} audio_output_hardware_instance_t;

/*!
    @brief Audio Output Channel

    Channels for each audio output hardware instance. Each instance usually has
    more than one channel, left and right for example.
*/
typedef enum __audio_output_channel
{
    audio_output_channel_a = 0,  /*! Left or Mono */
    audio_output_channel_b       /*! Right */
} audio_output_channel_t;

/*!
    @brief Audio Output Resolution

    Audio output resolution,16 or 24 bit resolution mode.
*/
typedef enum __audio_output_resolution
{
    audio_output_16_bit = 0,
    audio_output_24_bit
}audio_output_resolution_t;

/*!
    @brief Audio Endpoint

    Defines a hardware audio output. Used in pskey mapping to determine which
    audio sink to connect each output stream to.
*/
typedef struct __audio_output_endpoint
{
    audio_output_hardware_type_t hardware_type:4;
    audio_output_hardware_instance_t hardware_instance:2;
    audio_output_channel_t channel:2;
} audio_output_endpoint_t;

/*!
    @brief Audio Output Group

    Describes the logical groupings of outputs in audio_output_t used
    to handle volume control and mute. Each grouping of outputs has an
    independent system, master and tone gain; the individual outputs have
    their own gain trims as given in audio_output_mapping_t.
    Muting a group mutes all outputs within that group.

    audio_output_group_main contains:
    - audio_output_primary_left
    - audio_output_primary_right
    - audio_output_secondary_left
    - audio_output_secondary_right
    - audio_output_wired_sub

    audio_output_group_aux contains:
    - audio_output_aux_left
    - audio_output_aux_right
*/
typedef enum
{
    audio_output_group_main,
    audio_output_group_aux,
    audio_output_group_all
} audio_output_group_t;

/*!
    @brief Multi Channel Output Group Gain Type

    Describes gain type for an output group, this may be hardware
    controlled, digital or hybrid mode (a combination of hardware
    for coarse gain adjustments and digital for fine gain adjustments)
*/
typedef enum
{
    audio_output_gain_none,
    audio_output_gain_hardware,
    audio_output_gain_digital,
    audio_output_gain_invalid
} audio_output_gain_type_t;

/*! Trims for the main output group */
typedef struct
{
    int16 primary_left;     /* Volume trim for primary left in dB/60 */
    int16 primary_right;    /* Volume trim for primary right in dB/60 */
    int16 secondary_left;   /* Volume trim for secondary left in dB/60 */
    int16 secondary_right;  /* Volume trim for secondary right in dB/60 */
    int16 wired_sub;        /* Volume trim for sub-woofer in dB/60 */
} audio_output_trims_main_t;

/*! Trims for the aux output group */
typedef struct
{
    int16 left;         /* Volume trim for aux left in dB/60 */
    int16 right;        /* Volume trim for aux right in dB/60 */
} audio_output_trims_aux_t;

/*!
    @brief Multi Channel Output Group Gain

    Describes system, master and tone gain for an output group
*/
typedef union
{
    audio_output_trims_main_t main;
    audio_output_trims_aux_t  aux;
} audio_output_trims_t;

/*!
    @brief Multi Channel Configuration

    Configuration type for loading pskey config into. Simply a wrapper for an
    array of hardware mappings, one for each output.
*/
typedef struct __audio_output_config
{
    audio_output_endpoint_t mapping[audio_output_max];
    audio_output_gain_type_t gain_type[audio_output_group_all];
    audio_output_resolution_t output_resolution_mode;
    int16 fixed_hw_gain;
} audio_output_config_t;

/*!
    @brief Multi Channel Transform

    Determines how sources are connected to audio outputs
*/
typedef enum
{
    audio_output_tansform_connect,            /*! Use StreamConnect */
    audio_output_tansform_adpcm,              /*! Use TransformAdpcmDecode */
    audio_output_tansform_connect_and_dispose,/*! Use StreamConnectAndDispose */
    audio_output_tansform_max                 /*! Invalid */
} audio_output_transform_t;

/*!
    @brief Multi Channel Parameters

    Passed to the plugin to carry any additional information required for
    setting up connections at runtime. This information might be different on
    each call to the plugin, unlike the mapping config which stays constant.

    Optional parameters should be expected to be FALSE or 0 by default, so that
    memset may be used for initialisation (to 0).
*/
typedef struct __audio_output_params
{
    uint32 sample_rate;                     /*! Sample rate to run hardware outputs at */
    audio_output_transform_t transform;     /*! Source is ADPCM (doesn't apply to DSP) */
} audio_output_params_t;

/*============================================================================*
 *  Public Function Prototypes
 *============================================================================*/

/******************************************************************************
NAME:
    AudioOutputInit

DESCRIPTION:
    This function gets a pointer to the application malloc'd slot containing
    the multichannel audio configuration data.

PARAMETERS:
    conf    Pointer to malloc'd slot.

RETURNS:
    Whether initialisation was successful. TRUE = success, FALSE = error.
*/
bool AudioOutputInit(const audio_output_config_t* const conf);

/******************************************************************************
NAME:
    AudioOutputAddSource

DESCRIPTION:
    Add mapping from a source to an audio output

PARAMETERS:
    source    The source to map from
    output    The output to map to

RETURNS:
    TRUE if successful. FALSE if the output already has a mapping.
*/
bool AudioOutputAddSource(Source source, audio_output_t output);

#define AudioOutputAddSourceOrPanic(source, output) \
    PanicFalse(AudioOutputAddSource(source, output))

/******************************************************************************
NAME:
    AudioOutputConnect

DESCRIPTION:
    Connect all mapped sources. Mappings are reset once this function has been
    called

PARAMETERS:
    params      Captures all additional parameters in a single structure.

RETURNS:
    TRUE if successful. FALSE if connections could not be established.
*/
bool AudioOutputConnect(audio_output_params_t* params);

#define AudioOutputConnectOrPanic(params) \
    PanicFalse(AudioOutputConnect(params))

/******************************************************************************
NAME:
    AudioOutputDisconnect

DESCRIPTION:
    Disconnects all connected hardware audio outputs from their input sources.

RETURNS:
    Whether disconnection was successful. TRUE = success, FALSE = error.
*/
bool AudioOutputDisconnect(void);

/******************************************************************************
NAME:
    AudioOutputConfigRequiresI2s

DESCRIPTION:
    Returns whether the output mapping contains usage of any I2S hardware, and
    therefore whether the I2S plugin needs to be loaded and initialised,
    regardless of the state of any external I2S devices.

RETURNS:
    Whether I2S is required. TRUE = yes, FALSE = no.
*/
bool AudioOutputConfigRequiresI2s(void);

/******************************************************************************
NAME:
    AudioOutputI2sActive

DESCRIPTION:
    Returns whether any external I2S devices are currently connected and active.

    I2S devices are only activated if and when they are required. This function
    can therefore be used as a reliable indication that a device is connected
    and actually going to be used.

RETURNS:
    Whether an I2S device is active. TRUE = yes, FALSE = no.
*/
bool AudioOutputI2sActive(void);

/******************************************************************************
NAME:
    AudioOutputGetHardwareType

DESCRIPTION:
    Returns the hardware type mapped to an output.

PARAMETERS:
    audio_out     Output to get the hardware type for.

RETURNS:
    The hardware type.
*/
audio_output_hardware_type_t AudioOutputGetHardwareType(audio_output_t audio_out);

/******************************************************************************
NAME:
    AudioOutputIsOutputMapped

DESCRIPTION:
    Returns whether an output is actually mapped to any physical hardware.

PARAMETERS:
    audio_out Output to get the mapping state for

RETURNS:
    Whether an output is mapped (TRUE = mapped, FALSE = not mapped)
*/
bool AudioOutputIsOutputMapped(audio_output_t audio_out);

/******************************************************************************
NAME:
    AudioOutputGainGetFixedHardwareLevel

DESCRIPTION:
    Returns the configured fixed hardware gain level. This is the hardware gain
    that will be applied when digital volume control is used.
*/
int16 AudioOutputGainGetFixedHardwareLevel(void);

/******************************************************************************
NAME:
    AudioOutputGainApplyConfiguredLevels

DESCRIPTION:
    Apply hardware gains as appropriate for the gain type used.

    When the group is configured to use digital gain this function will set the
    hardware gain to the fixed level specified in configuration.

    When the group is configured to use hardware gain then the hardware gain
    will be set to the requested value master_gain.

    A set of optional trims can be provided (or left as NULL), which allows
    per-channel trims to be applied at the hardware level, if desired.

PARAMETERS:
    group The output group to apply the volume change to
    master_gain Desired output gain for the group (in dB/60)
    gain_info Optional trims to apply to each channel (can be NULL)
*/
void AudioOutputGainApplyConfiguredLevels(const audio_output_group_t group,
                                          const int16 master_gain,
                                          const audio_output_trims_t* trims);

/******************************************************************************
NAME:
    AudioOutputGainSetLevel

DESCRIPTION:
    Apply hardware gains as requested by caller.

    This function will always set the hardware gain to the value requested in
    master_gain, regardless of the gain type configured for the group.

PARAMETERS:
    group The output group to apply the volume change to
    master_gain Desired output gain for the group (in dB/60)
*/
void AudioOutputGainSetLevel(const audio_output_group_t group,
                             const int16 master_gain);

/******************************************************************************
NAME:
    AudioOutputGainGetType

DESCRIPTION:
    Get the gain type for a group (as specified in the multi-channel
    configuration).

PARAMETERS:
    group The output group for which to get the gain type.

RETURNS:
    The gain type configured for the group
*/
audio_output_gain_type_t AudioOutputGainGetType(const audio_output_group_t group);

/******************************************************************************
NAME:
    AudioOutput24BitOutputEnabled

DESCRIPTION:
    Returns whether 24 bit audio output is enabled.

RETURNS:
    Whether an 24 Bit audio output is enabled. TRUE = yes, FALSE = no.
*/
bool AudioOutput24BitOutputEnabled(void);
#endif /* __AUDIO_OUTPUT_H__ */

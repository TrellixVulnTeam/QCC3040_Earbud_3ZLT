/*******************************************************************************
Copyright (c) 2017-2020 Qualcomm Technologies International, Ltd.


FILE NAME
    anc_data.h

DESCRIPTION
    Encapsulation of the ANC VM Library data.
*/

#ifndef ANC_DATA_H_
#define ANC_DATA_H_

#include "anc.h"
#include "anc_sm.h"
#include "anc_config_data.h"

#include <csrtypes.h>

/*! @brief The ANC fine tune gain step value that can be set using the library.

    Used by the ANC library to cycle through the fine tune gain setting.
 */
typedef struct
{
    anc_user_gain_config_t *left;
    anc_user_gain_config_t *right;
} anc_user_config_t;

typedef enum
{
    ANC_INSTANCE_EP_BASED,
    ANC_INSTANCE_0_MASK,
    ANC_INSTANCE_1_MASK,
    ANC_INSTANCE_01_MASK
}anc_instance_mask_t;


#define INSTANCE_0_MASK  (1 << 16)
#define INSTANCE_1_MASK  (1 << 17)
#define INSTANCE_01_MASK (INSTANCE_0_MASK | INSTANCE_1_MASK)

/******************************************************************************
NAME
    ancDataInitialise

DESCRIPTION
    This function is responsible allocating any required memory for the library
    data and must be called before any other library data function is called.

RETURNS
    bool indicating if the library data was successfully initialised.
*/
bool ancDataInitialise(void);

/******************************************************************************
NAME
    ancDataDeinitialise

DESCRIPTION
    This function is responsible for de-initialising the library
    data.

RETURNS
    bool indicating if the memory for library data was successfully deinitialised.
*/
bool ancDataDeinitialise(void);

/******************************************************************************
NAME
    ancDataSetState/ancDataGetState

DESCRIPTION
    Simple setter and getter functions for the ANC VM library state.
*/
void ancDataSetState(anc_state state);
anc_state ancDataGetState(void);


/******************************************************************************
NAME
    ancDataSetMicParams/ancDataGetMicParams

DESCRIPTION
    Simple setter and getter functions for the ANC Microphone configuration.
*/
void ancDataSetMicParams(anc_mic_params_t *mic_params);
anc_mic_params_t* ancDataGetMicParams(void);

/******************************************************************************
NAME
    ancDataSetMode/ancDataGetMode

DESCRIPTION
    Simple setter and getter functions for the ANC Mode.
*/
bool ancDataSetMode(anc_mode_t mode);
anc_mode_t ancDataGetMode(void);

/******************************************************************************
NAME
    ancDataSetTopology/ancDatagetTopology

DESCRIPTION
    Simple setter and getter functions for the ANC topology
*/
void ancDataSetTopology(anc_filter_topology_t topology);
anc_filter_topology_t ancDataGetTopology(void);

/******************************************************************************
NAME
    ancDataGetConfigData

DESCRIPTION
    Simple getter function for the ANC config data.
*/
anc_config_t * ancDataGetConfigData(void);

/******************************************************************************
NAME
    ancDataGetCurrentModeConfig

DESCRIPTION
    Simple getter function for the config of the current ANC mode.
*/
anc_mode_config_t * ancDataGetCurrentModeConfig(void);

/******************************************************************************
NAME
    ancDataRetrieveAndPopulateTuningData

DESCRIPTION
    Function responsible for reading tuning tool params into ANC config data
*/
bool ancDataRetrieveAndPopulateTuningData(anc_mode_t set_mode);

/******************************************************************************
NAME
    ancDataIsLeftChannelConfigurable
    ancDataIsRightChannelConfigurable

DESCRIPTION
    Function to determine whether left/right channels are to be configured
*/
bool ancDataIsLeftChannelConfigurable(void);
bool ancDataIsRightChannelConfigurable(void);

/******************************************************************************
NAME
    ancConfigDataGetMicForMicPath

DESCRIPTION
    Function responsible for returning the mic config for a particular ANC mic
*/
audio_mic_params * ancConfigDataGetMicForMicPath(anc_path_enable mic_path);

/******************************************************************************
NAME
    ancConfigDataGetHardwareGainForMicPath

DESCRIPTION
    Function responsible for returning the hardware gain of a particular ANC mic
*/
uint32 ancConfigDataGetHardwareGainForMicPath(anc_path_enable mic_path);


/******************************************************************************
NAME
    ancDataSetUserGainConfig

DESCRIPTION
    Simple setter function for ANC filter path user specific gain configuration.
*/
void ancDataSetUserGainConfig(anc_user_gain_config_t *left, anc_user_gain_config_t *right);
/******************************************************************************
NAME
    ancDataSetUserGainConfig

DESCRIPTION
    Simple getter function for ANC filter path user specific gain configuration.
*/
anc_user_config_t* ancDataGetUserGainConfig(void);
/******************************************************************************
NAME
    ancDataResetUserGainConfig

DESCRIPTION
    Function to reset user specific gain configuration in place holder.
*/
void ancDataResetUserGainConfig(void);

#endif

/*******************************************************************************
Copyright (c) 2019 Qualcomm Technologies International, Ltd.


FILE NAME
    anc_config_write.h

DESCRIPTION
    Wirte the gain parameter to Audio PS key
*/

#ifndef ANC_CONFIG_WRITE_H_
#define ANC_CONFIG_WRITE_H_

#include "anc.h"

/*!
    @brief Write fine gain to PS store for the gain path and mode specified

    @param mode              ANC mode in use
    @param gain_path         ANC gain path
    @param gain              ANC gain value

    @return TRUE indicating PS update was successful else FALSE
*/
bool ancWriteFineGain(anc_mode_t mode, audio_anc_path_id gain_path, uint16 gain);


/*!
    @brief Write fine gain to PS store for the gain path and mode specified for both instances

    @param mode               ANC mode in use
    @param gain_path          ANC gain path
    @param instance_0_gain    ANC instance 0 gain value
    @param instance_1_gain    ANC instance 1 gain value

    @return TRUE indicating PS update was successful else FALSE
*/
bool ancWriteFineGainParallelFilter(anc_mode_t mode, audio_anc_path_id gain_path, uint16 instance_0_gain, uint16 instance_1_gain);

#endif

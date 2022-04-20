/*!
\copyright  Copyright (c) 2019 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       anc_config.h
\brief      Configuration defintions and stubs for Active Noise Cancellation (ANC). 
*/

#ifndef ANC_CONFIG_H_
#define ANC_CONFIG_H_

#ifdef ENABLE_ANC

#include <anc.h>

#ifdef ENABLE_ENHANCED_ANC
#define ancConfigFilterTopology()                  (anc_parallel_filter_topology)
#else
#define ancConfigFilterTopology()                  (anc_single_filter_topology)
#endif

#define ANC_CONFIG_MODE_ADAPTIVE 1
#define ANC_CONFIG_MODE_STATIC 0

#define ANC_CONFIG_MODE_LEAKTHROUGH 1
#define ANC_CONFIG_MODE_NOISE_CANCELLATION 0

/*
 * There is no config manager setup yet, so hard-code the default value
 * by referring ..module_configurations/anc_def.xml
 */

typedef struct {
    unsigned short feed_back_right_mic:4;
    unsigned short feed_back_left_mic:4;
    unsigned short feed_forward_right_mic:4;
    unsigned short feed_forward_left_mic:4;
} anc_mic_params_r_config_t;

#define ANC_READONLY_CONFIG_BLK_ID 1155

typedef struct {
    anc_mic_params_r_config_t anc_mic_params_r_config;
    unsigned short num_anc_modes:8;
} anc_readonly_config_def_t;

#define ANC_WRITEABLE_CONFIG_BLK_ID 1162

typedef struct {
    unsigned short persist_initial_mode:1;
    unsigned short persist_initial_state:1;
    unsigned short initial_anc_state:1;
    unsigned short initial_anc_mode:4;
} anc_writeable_config_def_t;

#define EVENTS_USR_MESSAGE_BASE (0x4000)

extern anc_readonly_config_def_t anc_readonly_config;
uint16 ancConfigManagerGetReadOnlyConfig(uint16 config_id, const void **data);
void ancConfigManagerReleaseConfig(uint16 config_id);
uint16 ancConfigManagerGetWriteableConfig(uint16 config_id, void **data, uint16 size);
void ancConfigManagerUpdateWriteableConfig(uint16 config_id);

#endif /* ENABLE_ANC */


#endif /* ANC_CONFIG_H_ */

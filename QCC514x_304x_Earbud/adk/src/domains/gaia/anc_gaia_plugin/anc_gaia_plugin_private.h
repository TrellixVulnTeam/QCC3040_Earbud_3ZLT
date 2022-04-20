/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       anc_gaia_plugin_private.h
\brief      Internal header for gaia anc framework plugin
*/

#ifndef ANC_GAIA_PLUGIN_PRIVATE_H_
#define ANC_GAIA_PLUGIN_PRIVATE_H_

/*! \brief ANC GAIA plugin data. */
typedef struct
{
    /*! State Proxy task */
    TaskData task;

    /*! transport for set command received by ANC GAIA Plugin */
    GAIA_TRANSPORT *command_received_transport;

    /*! Latest set command received */
    uint8 received_command;
} anc_gaia_plugin_task_data_t;

anc_gaia_plugin_task_data_t anc_gaia_plugin_data;
#define ancGaiaPlugin_GetTaskData()                  (&anc_gaia_plugin_data)
#define ancGaiaPlugin_GetTask()                      (&anc_gaia_plugin_data.task)

#endif /* ANC_GAIA_PLUGIN_PRIVATE_H_ */

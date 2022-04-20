/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       connection_manager_qos.h
\brief      Header file for Connection Manager QoS
*/

#ifndef __CON_MANAGER_QOS_H
#define __CON_MANAGER_QOS_H

#include <connection_manager.h>
#include <connection_manager_list.h>

/*! \brief Send updated parameters to the controller

    Sends a request to the controller to update the current link 
    settings.

    \note The current requested qos is compared with the current link settings 
    and if matched, no update will be sent 

    \param connection The connection to update
    */
void conManagerSendParameterUpdate(cm_connection_t* connection);

/*! \brief Apply parameters on connection */
void ConManagerApplyQosOnConnect(cm_connection_t* connection);

/*! \brief Apply parameters before connection */
void ConManagerApplyQosPreConnect(cm_connection_t* connection);

/*! \brief Initialise connection parameters */
void ConnectionManagerQosInit(void);

/*! \brief Request parameter update if new parameters for a link are not compatible with those expected */
void conManagerQosCheckNewConnParams(cm_connection_t *connection);

#endif

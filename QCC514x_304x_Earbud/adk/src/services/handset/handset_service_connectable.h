/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Handset service connectable
*/

#ifndef HANDSET_SERVICE_CONNECTABLE_H_
#define HANDSET_SERVICE_CONNECTABLE_H_

#include "handset_service.h"

/*@{*/

/*! \brief Register as observer for connect/disconnect indications from connection_manager */
void handsetService_ObserveConnections(void);

/*! \brief Initialise connectable module */
void handsetService_ConnectableInit(void);


/*! \brief Enable BR/EDR connections */
void handsetService_ConnectableEnableBredr(bool enable);

/*! \brief Allow BR/EDR connections */
void handsetService_ConnectableAllowBredr(bool allow);

/*@}*/

#endif /* HANDSET_SERVICE_CONNECTABLE_H_ */

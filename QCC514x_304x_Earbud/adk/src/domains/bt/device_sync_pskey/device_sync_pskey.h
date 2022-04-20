/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    bt_domain
\brief      Extensions of device_sync for synchronisation of device ps keys.
 
It registers as a device_sync client and handles synchronisation of device ps keys.
 
*/

#ifndef DEVICE_SYNC_PSKEY_H_
#define DEVICE_SYNC_PSKEY_H_

#include <message.h>

/*@{*/

/*! \brief Init function

    \param init_task unused

    \return always TRUE
*/
bool DeviceSyncPsKey_Init(Task init_task);

/*@}*/

#endif /* DEVICE_SYNC_PSKEY_H_ */

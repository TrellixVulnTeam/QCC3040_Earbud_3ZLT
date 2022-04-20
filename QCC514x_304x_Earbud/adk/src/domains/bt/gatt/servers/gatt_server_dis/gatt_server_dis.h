/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   gatt_server_dis Device Information Service
\ingroup    bt_domain
\brief      Header file for the GATT Device Information Service Server module.
            Component that deals with initialising the gatt_device_info_server library.

@{
*/

#ifndef GATT_SERVER_DIS_H_
#define GATT_SERVER_DIS_H_

#include <gatt_device_info_server.h>

/*! Structure holding information for the application handling of GATT Device Information Server */
typedef struct
{
    /*! Task for handling battery related messages */
    TaskData gatt_device_info_task;

    /*! Device Information server library data */
    gdiss_t gdiss;
}gattServerDeviceInfoData;

/*!< App GATT component task */
extern gattServerDeviceInfoData gatt_server_device_info;

/*! Get pointer to the main device information service Task */
#define GetGattServerDeviceInfoTask() (&gatt_server_device_info.gatt_device_info_task)

/*! Get pointer to the device information server data passed to the library */
#define GetGattServerDeviceInfoGdis() (&gatt_server_device_info.gdiss)

/*! @brief Initialise the GATT Device Information Service Server.

    \param init_task    Task to send init completion message to

    \returns TRUE
*/
bool GattServerDeviceInfo_Init(Task init_task);

#endif /* GATT_SERVER_DIS_H_ */

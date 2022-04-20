/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      GATT Connect configuration
*/

#ifndef GATT_CONNECT_CONFIG_H_
#define GATT_CONNECT_CONFIG_H_

enum
{
    max_gatt_connect_observers_base = 10,
    max_gatt_connect_observers
};

#define MAX_NUMBER_GATT_CONNECT_OBSERVERS   (max_gatt_connect_observers - 1)

#endif
/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    bt_domain
\brief      Validation of device db.
 
This component is a collection of two independent functionalities.
 1. Validation of correctness of device db data stored in PDL attributes.
 2. Recovering corrupted device.
 
It is meant to be used from within device_db_serialiser only.
Its functionality can be disabled by defining DISABLE_DEVICE_DB_ASSERT.

*/

#ifndef DEVICE_DB_SERIALISER_ASSERT_H_
#define DEVICE_DB_SERIALISER_ASSERT_H_

/*@{*/

#include <csrtypes.h>

/*! \brief Validate correctness of device db data stored in PDL attributes.

    This is based on checking that there should be only one device of type 1 and only one of type 3 as well.
    Validation makes sense for earbud, it will always come out positive for other applications like headset.
*/
bool DeviceDbSerialiser_IsPsStoreValid(void);

/*! \brief Attempt to recover device from the corrupted state.

    It will delete PDL and panic.
    That is to prevent device falling into panic loop,
    which is likely to happen when PDL is corrupted.

    To restore earbuds to operational state
    the other earbud must be factory reset to trigger peer pairing process.
*/
void DeviceDbSerialiser_HandleCorruption(void);

/*@}*/

#endif /* DEVICE_DB_SERIALISER_ASSERT_H_ */

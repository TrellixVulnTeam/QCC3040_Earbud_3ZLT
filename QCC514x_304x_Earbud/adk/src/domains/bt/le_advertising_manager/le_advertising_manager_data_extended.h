/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Manage execution of callbacks to construct adverts and scan response
*/

#ifndef LE_ADVERTSING_MANAGER_DATA_EXTENDED_H_
#define LE_ADVERTSING_MANAGER_DATA_EXTENDED_H_

#include "le_advertising_manager.h"
#include "le_advertising_manager_private.h"


#ifdef INCLUDE_ADVERTISING_EXTENSIONS

/*! Used to register and use the first application advertising set */
#define ADV_HANDLE_APP_SET_1            1


void leAdvertisingManager_RegisterExtendedDataIf(void);

#else

#define leAdvertisingManager_RegisterExtendedDataIf() ((void)(0))
    
#endif /* INCLUDE_ADVERTISING_EXTENSIONS*/


#endif /* LE_ADVERTSING_MANAGER_DATA_EXTENDED_H_ */

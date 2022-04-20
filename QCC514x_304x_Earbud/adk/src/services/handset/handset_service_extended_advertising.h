/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Manage the handset extended advertising set.
*/

#ifndef HANDSET_SERVICE_EXTENDED_ADVERTISING_H_
#define HANDSET_SERVICE_EXTENDED_ADVERTISING_H_

#if defined(INCLUDE_ADVERTISING_EXTENSIONS)

/*! \brief Initialise the handset extended advertising module.
*/
void HandsetServiceExtAdv_Init(void);

/*! \brief Update the handset extended advertising state

    This function will select or release the handset extended advertising set
    based on the state of the main handset service.

    \return TRUE if the state of the handset extended advertising will change,
            FALSE otherwise.
*/
bool HandsetServiceExtAdv_UpdateAdvertisingData(void);

#else

#define HandsetServiceExtAdv_Init(init_task) ((void)0)

#define HandsetServiceExtAdv_UpdateAdvertisingData() (TRUE)

#endif /* defined(INCLUDE_ADVERTISING_EXTENSIONS) */

#endif /* HANDSET_SERVICE_EXTENDED_ADVERTISING_H_ */

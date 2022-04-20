/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Interface to implementation of VA profile
*/

#ifndef VA_PROFILE_H_
#define VA_PROFILE_H_

#include <bdaddr.h>

/*! Function pointer used to check if VA is active at a given BT address. */
typedef bool (*vapActiveAtBdaddr)(const bdaddr *);

/*! \brief Set callback to check if VA feature is active at the given address.
    \param vapActiveAtBdaddr function pointer that takes a const bdaddr *
           and returns a bool.
*/
void VaProfile_RegisterClient(vapActiveAtBdaddr callback);

/*! \brief Check if VA is active at a given BT address.
    \param const bdaddr * BT address
    \return bool TRUE if VA is active at BT address, otherwise FALSE.
*/
bool VaProfile_IsVaActiveAtBdaddr(const bdaddr * bd_addr);

#endif /* VA_PROFILE_H_ */

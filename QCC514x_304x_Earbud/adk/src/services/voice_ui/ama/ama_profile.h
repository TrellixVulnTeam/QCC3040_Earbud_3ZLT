/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_profile.h
\brief  Interfaces defination of the profile interface for Amazon AVS
*/

#ifndef AMA_PROFILE_H
#define AMA_PROFILE_H

#include <bdaddr.h>

/*! \brief Initialise the AMA profile handling
*/
void AmaProfile_Init(void);

/*! \brief Send a connected indication for the profile
 *  \param const bdaddr * pointer to BT address
*/
void AmaProfile_SendConnectedInd(const bdaddr * bd_addr);

/*! \brief Send a disconnected indication for the profile
 *  \param const bdaddr * pointer to BT address
*/
void AmaProfile_SendDisconnectedInd(const bdaddr * bd_addr);

#endif /* AMA_PROFILE_H */

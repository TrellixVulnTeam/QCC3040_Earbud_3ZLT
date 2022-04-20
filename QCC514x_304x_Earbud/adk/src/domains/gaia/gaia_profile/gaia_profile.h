/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       gaia_profile.h
\brief  Interfaces defination of the profile interface for GAIA
*/

#ifndef GAIA_PROFILE_H
#define GAIA_PROFILE_H

/*! \brief This function prototype needs to be moved to an appropriate location in GAIA.
*/
bool Gaia_DisconnectIfRequired(const bdaddr *bd_addr);

/*! \brief Initialise the GAIA profile handling
*/
void GaiaProfile_Init(void);

/*! \brief Send a connected indication for the profile
*/
void GaiaProfile_SendConnectedInd(const bdaddr *bd_addr);

/*! \brief Send a disconnected indication for the profile
*/
void GaiaProfile_SendDisconnectedInd(const bdaddr *bd_addr);

/*! \brief Inform profile manager of connect confirmation
*/
void GaiaProfile_HandleConnectCfm(const bdaddr *bd_addr, bool success);

#endif /* GAIA_PROFILE_H */

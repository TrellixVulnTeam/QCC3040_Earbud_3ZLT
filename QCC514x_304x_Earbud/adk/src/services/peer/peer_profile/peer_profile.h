/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       peer_profile.h
\brief  Interfaces defination of the profile interface for PEER
*/

#ifndef PEER_PROFILE_H
#define PEER_PROFILE_H

/*! \brief This function prototype needs to be moved to an appropriate location in PEER.
*/
bool Peer_ConnectIfRequired(void);

/*! \brief This function prototype needs to be moved to an appropriate location in PEER.
*/
bool Peer_DisconnectIfRequired(void);

/*! \brief Initialise the PEER profile handling
*/
void PeerProfile_Init(void);

/*! \brief Send a connected indication for the profile
*/
void PeerProfile_SendConnectedInd(void);

/*! \brief Send a disconnected indication for the profile
*/
void PeerProfile_SendDisconnectedInd(void);

#endif /* PEER_PROFILE_H */

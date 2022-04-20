/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header for AVRCP Browsing functionality. This file is mainly used to provide functionality
            that is required for PTS testing and not general use cases.
*/

#ifndef AVRCP_PROFILE_BROWSING_H_
#define AVRCP_PROFILE_BROWSING_H_

#ifdef INCLUDE_AVRCP_BROWSING

#include <avrcp.h>
#include <av_instance.h>

void AvrcpBrowsing_HandleBrowseConnectInd(const AVRCP_BROWSE_CONNECT_IND_T *ind);
void AvrcpBrowsing_HandleSetAddressedPlayerInd(avInstanceTaskData *the_inst, const AVRCP_SET_ADDRESSED_PLAYER_IND_T *ind);
void AvrcpBrowsing_HandleGetFolderItemsInd(avInstanceTaskData *the_inst, const AVRCP_BROWSE_GET_FOLDER_ITEMS_IND_T *ind);
void AvrcpBrowsing_HandleGetNumberOfItemsInd(avInstanceTaskData *the_inst, const AVRCP_BROWSE_GET_NUMBER_OF_ITEMS_IND_T *ind);
void AvrcpBrowsing_HandleEventAddressedPlayerChanged(AVRCP_REGISTER_NOTIFICATION_IND_T *ind, avrcp_response_type response);
void AvrcpBrowsing_HandleConnectCfm(avInstanceTaskData *the_inst, const AVRCP_BROWSE_CONNECT_CFM_T *cfm);
void AvrcpBrowsing_HandleDisconnectInd(avInstanceTaskData *the_inst, const AVRCP_BROWSE_DISCONNECT_IND_T *cfm);

#else

#define AvrcpBrowsing_HandleBrowseConnectInd(ind)
#define AvrcpBrowsing_HandleSetAddressedPlayerInd(the_inst, ind)
#define AvrcpBrowsing_HandleGetFolderItemsInd(the_inst, ind)
#define AvrcpBrowsing_HandleGetNumberOfItemsInd(the_inst, ind)
#define AvrcpBrowsing_HandleEventAddressedPlayerChanged(ind, response)
#define AvrcpBrowsing_HandleConnectCfm(the_inst, cfm)
#define AvrcpBrowsing_HandleDisconnectInd(the_inst, cfm)


#endif /* INCLUDE_AVRCP_BROWSING */
#endif /* AVRCP_PROFILE_BROWSING_H_ */

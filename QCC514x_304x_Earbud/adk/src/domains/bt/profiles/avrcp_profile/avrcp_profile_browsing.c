/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of browsing specific avrcp functionality

*/

/* Only compile if INCLUDE_AVRCP_BROWSING defined */
#ifdef INCLUDE_AVRCP_BROWSING

#include "avrcp_profile_browsing.h"
#include <av.h>
#include <stream.h>
#include <panic.h>
#include <logging.h>

#define GET_FOLDER_ITEMS_RESPONSE_LENGTH 35
#define GET_FOLDER_ITEMS_NUMBER_OF_ITEMS 1

typedef struct
{
    uint8* cleanup_data;
    bool is_browsing_connected;
} avrcp_browsing_data_t;

avrcp_browsing_data_t avrcp_browsing_data =
{
    .cleanup_data = NULL,
};

#define AvrcpBrowsing_GetTaskData() (&avrcp_browsing_data)


static void avrcpBrowsing_CleanUp(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);
    switch (id)
    {
    case MESSAGE_SOURCE_EMPTY:
    {
        /* Free the previously stored data ptr. */
        if (AvrcpBrowsing_GetTaskData()->cleanup_data)
        {
            free(AvrcpBrowsing_GetTaskData()->cleanup_data);
        }
        AvrcpBrowsing_GetTaskData()->cleanup_data = NULL;
    }
        break;
    default:
        break;
    }
}

TaskData browsing_cleanup_task = {avrcpBrowsing_CleanUp};


void AvrcpBrowsing_HandleBrowseConnectInd(const AVRCP_BROWSE_CONNECT_IND_T *ind)
{
    DEBUG_LOG_VERBOSE("AvrcpBrowsing_HandleBrowseConnectInd: bd_addr: 0x%04x, 0x%02x, 0x%06lx",
                      ind->bd_addr.nap,
                      ind->bd_addr.uap,
                      ind->bd_addr.lap);

    avInstanceTaskData * av_instance = appAvInstanceFindFromBdAddr(&ind->bd_addr);

    if (av_instance == NULL)
    {
        DEBUG_LOG_ERROR("AvrcpBrowsing_HandleBrowseConnectInd: No AV instance for bd_addr: 0x%04x, 0x%02x, 0x%06lx",
                        ind->bd_addr.nap,
                        ind->bd_addr.uap,
                        ind->bd_addr.lap);
        return;
    }

    AvrcpBrowseConnectResponse(av_instance->avrcp.avrcp, ind->connection_id, ind->signal_id, TRUE);
}

void AvrcpBrowsing_HandleSetAddressedPlayerInd(avInstanceTaskData *the_inst, const AVRCP_SET_ADDRESSED_PLAYER_IND_T *ind)
{
    DEBUG_LOG_VERBOSE("AvrcpBrowsing_HandleSetAddressedPlayerInd: player_id:%u", ind->player_id);

    UNUSED(the_inst);
    avrcp_response_type res_type = avrcp_response_rejected_invalid_player_id;
    if(ind->player_id != 0xffffu)
    {
        DEBUG_LOG("AvrcpBrowsing_HandleSetAddressedPlayerInd: Request for AVRCP Addressed Played id %d\n", ind->player_id);
        res_type = avctp_response_accepted;
    }
    AvrcpSetAddressedPlayerResponse(ind->avrcp, res_type);
}

void AvrcpBrowsing_HandleGetFolderItemsInd(avInstanceTaskData *the_inst, const AVRCP_BROWSE_GET_FOLDER_ITEMS_IND_T *ind)
{
    DEBUG_LOG_VERBOSE("AvrcpBrowsing_HandleGetFolderItemsInd");

    UNUSED(the_inst);
    Source src_pdu=0;
    avrcp_response_type res_type = avrcp_response_rejected_invalid_scope;
    uint8 *item=NULL;

    if(ind->scope == avrcp_media_player_scope)
    {
        res_type = avrcp_response_browsing_success;

        item = PanicUnlessMalloc(GET_FOLDER_ITEMS_RESPONSE_LENGTH);

        /* Set the data to 0 */

        int i = 0;
        for (i=0; i<GET_FOLDER_ITEMS_RESPONSE_LENGTH; i++)
        {
            item[i] = 0;
        }

        /* Set the data for the response. Fields are in MSB-first*/

        item[0] = 0x01; /* Media Player Item */
        item[1] = 0x00; /* Item length */
        item[2] = 0x20; /* 32 */
        item[3] = 0x00; /* Player ID */
        item[4] = 0x01;
        item[5] = 0x01; /* Player Type */
        item[6] = 0x00; /* Sub-type */
        item[7] = 0x00;
        item[8] = 0x00;
        item[9] = 0x00;
        item[10] = 0x00; /* Play Status */
        item[11] = 0x00; /* Feature Bit mask */
        item[12] = 0x00;
        item[13] = 0x00;
        item[14] = 0x00;
        item[15] = 0x60; /* VOL UP, VOL DOWN */
        item[16] = 0x05; /* Play, Pause */

        item[27] = 0x00; /* Character Set ID */
        item[28] = 0x6A; /* UTF-8 */

        item[29] = 0x00; /* Displayable Name Length */
        item[30] = 0x04;

        item[31] = 0x51; /* Q */
        item[32] = 0x43; /* C */
        item[33] = 0x4F; /* O */
        item[34] = 0x4D; /* M */

        /* Create a source from the data */
        src_pdu = StreamRegionSource(item, GET_FOLDER_ITEMS_RESPONSE_LENGTH);

        /* The clean-up task clears this so should be NULL at this point */
        PanicNotNull(AvrcpBrowsing_GetTaskData()->cleanup_data);

        /* Register a task for freeing the data and store a ptr to it */
        AvrcpBrowsing_GetTaskData()->cleanup_data = item;

        MessageStreamTaskFromSink(StreamSinkFromSource(src_pdu), &browsing_cleanup_task);

        AvrcpBrowseGetFolderItemsResponse(ind->avrcp, res_type, 0, GET_FOLDER_ITEMS_NUMBER_OF_ITEMS, GET_FOLDER_ITEMS_RESPONSE_LENGTH, src_pdu);
    }
}

void AvrcpBrowsing_HandleGetNumberOfItemsInd(avInstanceTaskData *the_inst, const AVRCP_BROWSE_GET_NUMBER_OF_ITEMS_IND_T *ind)
{
    DEBUG_LOG_VERBOSE("AvrcpBrowsing_HandleGetNumberOfItemsInd");

    UNUSED(the_inst);
    avrcp_response_type res_type = avrcp_response_rejected_invalid_scope;
    uint16 num_items=0;

    if(ind->scope == avrcp_media_player_scope)
    {
        res_type = avrcp_response_browsing_success;
        num_items = 1;
    }

    AvrcpBrowseGetNumberOfItemsResponse(ind->avrcp, res_type, 0, num_items);
}

void AvrcpBrowsing_HandleConnectCfm(avInstanceTaskData *the_inst, const AVRCP_BROWSE_CONNECT_CFM_T *cfm)
{
    DEBUG_LOG_VERBOSE("AvrcpBrowsing_HandleConnectCfm: Status:enum:avrcp_status_code:%d", cfm->status);

    UNUSED(the_inst);
    AvrcpBrowsing_GetTaskData()->is_browsing_connected = (cfm->status == avrcp_success);
}

void AvrcpBrowsing_HandleDisconnectInd(avInstanceTaskData *the_inst, const AVRCP_BROWSE_DISCONNECT_IND_T *ind)
{
    DEBUG_LOG_VERBOSE("AvrcpBrowsing_HandleDisconnectInd: Status:enum:avrcp_status_code:%d", ind->status);

    UNUSED(the_inst);
    AvrcpBrowsing_GetTaskData()->is_browsing_connected = (ind->status == avrcp_success);
}

void AvrcpBrowsing_HandleEventAddressedPlayerChanged(AVRCP_REGISTER_NOTIFICATION_IND_T *ind, avrcp_response_type response)
{
    DEBUG_LOG_VERBOSE("AvrcpBrowsing_HandleEventAddressedPlayerChanged: EventId:enum:avrcp_supported_events:%d", ind->event_id);

    AvrcpEventAddressedPlayerChangedResponse(ind->avrcp, response, 1, 0);
}

#endif /* INCLUDE_AVRCP_BROWSING */

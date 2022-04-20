/* Copyright (c) 2019 - 2020 Qualcomm Technologies International, Ltd. */
/*  */

#include "gatt_root_key_server_private.h"

#include "gatt_root_key_server_msg_handler.h"
#include "gatt_root_key_server_challenge.h"
#include "gatt_root_key_server_indicate.h"

#include "gatt_root_key_server_access.h"


/****************************************************************************/
void rootKeyServerMsgHandler(Task task, MessageId id, Message payload)
{
    GATT_ROOT_KEY_SERVER *instance= (GATT_ROOT_KEY_SERVER*)task;

    GATT_ROOT_KEY_SERVER_DEBUG("rootKeyServerMsgHandler: MESSAGE:root_key_server_internal_msg_t:0x%x", id);

    switch (id)
    {
        case GATT_MANAGER_SERVER_ACCESS_IND:
            /* Read/write access to characteristic */
            handleRootKeyAccess(instance, (const GATT_MANAGER_SERVER_ACCESS_IND_T *)payload);
            break;

        case GATT_MANAGER_REMOTE_CLIENT_INDICATION_CFM:
            handleRootKeyIndicationCfm(instance, 
                                       (const GATT_MANAGER_REMOTE_CLIENT_INDICATION_CFM_T *)payload);
            break;

        case ROOT_KEY_SERVER_INTERNAL_CHALLENGE_WRITE:
            handleInternalChallengeWrite((const ROOT_KEY_SERVER_INTERNAL_CHALLENGE_WRITE_T *)payload);
            break;

        case ROOT_KEY_SERVER_INTERNAL_KEYS_WRITE:
            handleInternalKeysWrite((const ROOT_KEY_SERVER_INTERNAL_KEYS_WRITE_T *)payload);
            break;

        case ROOT_KEY_SERVER_INTERNAL_KEYS_COMMIT:
            handleInternalKeysCommit((const ROOT_KEY_SERVER_INTERNAL_KEYS_COMMIT_T *)payload);
            break;

        default:
            /* Unrecognised GATT Manager message */
            GATT_ROOT_KEY_SERVER_DEBUG("rootKeyServerMsgHandler: Msg MESSAGE:root_key_server_internal_msg_t:0x%xnot handled", 
                                       id);
            /*! \todo This panic may be a step too far */
            GATT_ROOT_KEY_SERVER_DEBUG_PANIC();
            break;
    }
}


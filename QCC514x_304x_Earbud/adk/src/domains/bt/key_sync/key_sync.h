/*!
\copyright  Copyright (c) 2005 - 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   key_sync Key Sync
\ingroup    bt_domain
\brief      Key synchronisation component.
*/

#ifndef KEY_SYNC_H
#define KEY_SYNC_H

#include <domain_message.h>

#include <bdaddr.h>
#include <device.h>

#include <message.h>

/*\{*/

typedef struct
{
    /* Address of device that have been synchronised */
    bdaddr bd_addr;
} KEY_SYNC_DEVICE_COMPLETE_IND_T;

/*! \brief Message that may be sent by this component. */
enum key_sync_messages
{
    /* Key synchronisation is complete.
     * NOT IMPLEMENTED YET */
    KEY_SYNC_COMPLETE = KEY_SYNC_MESSAGE_BASE,
    /* Indicates that keys for given device have been synchronised */
    KEY_SYNC_DEVICE_COMPLETE_IND,

    /*! This must be the final message */
    KEY_SYNC_MESSAGE_END
};


/*! \brief Initialise the key sync component. */
bool KeySync_Init(Task init_task);

/*! \brief Synchronise link keys with peer. */
void KeySync_Sync(void);

/*! \brief Check if device keys have been sent and received on the other earbud

    \param device Device to be checked

    \return TRUE if device keys are in sync
*/
bool KeySync_IsDeviceInSync(device_t device);

/*! \brief Register task that will receive key_sync_messages

    \note Only one task is supported

    \param listener Task to receive notifications
*/
void KeySync_RegisterListener(Task listener);

/*\}*/

#endif /* KEY_SYNC_H */

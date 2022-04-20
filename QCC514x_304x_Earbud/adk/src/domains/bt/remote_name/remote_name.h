/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    bt_device
\defgroup   remote_name Remote name
\brief      Access to remote device friendly name.
 
 
*/

#ifndef REMOTE_NAME_H_
#define REMOTE_NAME_H_

#include <domain_message.h>
#include <device.h>
#include <message.h>

/*@{*/

typedef struct
{
  device_t device;
} REMOTE_NAME_AVAILABLE_IND_T;

typedef enum
{
    REMOTE_NAME_AVAILABLE_IND = REMOTE_NAME_MESSAGE_BASE,
} remote_name_message_t;

/*! \brief Init function

    \param init_task unused

    \return always TRUE
*/
bool RemoteName_Init(Task init_task);

/*! \brief Checks if name for a device exists

    \param device Device to be queried.

    \return TRUE if name can be read immediately.
*/
bool RemoteName_IsAvailable(device_t device);

/*! \brief Register for REMOTE_NAME_AVAILABLE_IND message

    Task task will receive message REMOTE_NAME_AVAILABLE_IND
    when the name for device will become available.

    The message will be sent only once,
    after that task will be automatically removed from the list.

    \param task   Listener task.
    \param device Device for which notification should be sent.
*/
void RemoteName_NotifyWhenAvailable(Task task, device_t device);

/*! \brief Get name of remote device

    \param      device Device which name should be retrieved.
    \param[out] size   Size of name will be written here.

    \return Remote device name. Memory is allocated by RemoteName_Get(),
            and it must be freed by a caller.
*/
const char *RemoteName_Get(device_t device, uint16 *size);


/*@}*/

#endif /* REMOTE_NAME_H_ */

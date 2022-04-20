/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   device_sync Device Sync
\ingroup    bt_domain
\brief      Main component responsible for device data synchronisation.
 
It synchronises selected device properties stored device database.
Only supported direction of synchronisation is from the primary to the secondary.
It supports the following triggers:
 - Write to the device property - only that property is synchronised
 - Peer signalling connect - all supported properties for all handsets are synchronised
 - Key sync complete for given device - all supported properties from given device are synchronised
 
*/

#ifndef DEVICE_SYNC_H_
#define DEVICE_SYNC_H_

#include <domain_message.h>

#include <message.h>
#include <device.h>
#include <app/marshal/marshal_if.h>

/*@{*/

/*! List of possible clients */
typedef enum
{
    device_sync_client_core = 0,
    device_sync_client_device_pskey
} device_sync_client_id_t;

/*! Structure used to synchronise device properties */
typedef struct
{
    /*! The device whose property changed */
    bdaddr addr;

    /* Id of client which should handle the data */
    uint8 client_id;

    /*! The property id */
    uint8 id;

    /*! The size of the property */
    uint8 size;

    /*! Dynamic array containing the property data */
    uint8 data[1];
} device_property_sync_t;

typedef struct
{
    /*! \brief Called on sync message reception on the secondary
        \return TRUE if confirmations should be send
    */
    bool (*SyncRxIndCallback)(void *message);
    /*! \brief Confirmation that secondary have received the sync message */
    void (*SyncCfmCallback)(device_t device, uint8 id);
    /*! \brief Called on primary when peer signalling gets connected */
    void (*PeerConnectedCallback)(void);
    /*! \brief Called on primary when completion of key sync for a device is confirmed */
    void (*DeviceAddedToPeerCallback)(device_t device);
} device_sync_callback_t;

typedef struct
{
    device_t device;
    uint8 property_id;
} DEVICE_SYNC_PROPERTY_UPDATE_IND_T;

typedef enum
{
    /*! Sent when a property is updated on the secondary earbud */
    DEVICE_SYNC_PROPERTY_UPDATE_IND = DEVICE_SYNC_MESSAGE_BASE,

    DEVICE_SYNC_MESSAGE_END
} device_sync_messages_t;

/*! \brief Init function

    \param init_task unused

    \return always TRUE
*/
bool DeviceSync_Init(Task init_task);

/*! \brief Register set of callback to participate in synchronisation

    Only one client is supported.

    \param client_id Id of client associated with callbacks.
    \param callback  Pointer to a structure with callbacks.
*/
void DeviceSync_RegisterCallback(device_sync_client_id_t client_id, const device_sync_callback_t *callback);

/*! \brief Register to receive notification messages

    Listener will receive messages like DEVICE_SYNC_PROPERTY_UPDATE_IND.

    \param listener Listener task
*/
void DeviceSync_RegisterForNotification(Task listener);

/*! \brief Send a synchronisation message

    To be used by clients to send over their data.

    \param msg   Synchronisation data.
*/
void DeviceSync_SyncData(void *msg);

/*@}*/

#endif /* DEVICE_SYNC_H_ */

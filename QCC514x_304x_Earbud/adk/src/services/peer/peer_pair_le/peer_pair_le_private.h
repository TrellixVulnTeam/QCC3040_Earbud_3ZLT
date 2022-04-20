/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Private header file for the peer service providing LE based pairing
*/

#ifndef PEER_PAIR_LE_PRIVATE_H_
#define PEER_PAIR_LE_PRIVATE_H_

#include <stdlib.h>

#include <message.h>

#include <le_scan_manager.h>
#include <le_advertising_manager.h>

#include "peer_pair_le_sm.h"
#include "peer_pair_le_config.h"

#include <gatt_root_key_client.h>
#include <gatt_root_key_server.h>
#include "bt_types.h"
#include <local_addr_protected.h>
#include <multidevice.h>


/*! Structure used internally by the LE peer pairing service to record details
    of potential peer devices that have been found. */
typedef struct
{
    typed_bdaddr taddr;   /*!< Address of the found device */
    int rssi;   /*!< Signal strength recorded for this device */
} peerPairLeFoundDevice;


/*! Structure used to hold data needed when the LE peer pairing service is running

    This structure is allocated when peer pairing is started, and freed when paired
    avoiding long term memory usage.

    Accessed from the structure peerPairLeTaskData
 */
typedef struct 
{
    /*! Single task representing client */
    Task                    client;

    /*! Temporary storage of local address
        \todo Remove as will use private addresses AND not fixed */
    typed_bdaddr            local_addr;

    /*! Address of the discovered peer device */
    typed_bdaddr            peer;

    /*! Connection ID for the GATT connection */
    gatt_cid_t                  gatt_cid;

    /*! Devices found while scanning for a peer. We track the 2 with highest RSSI */
    peerPairLeFoundDevice   scanned_devices[2];

    /*! Details of the current LE advertising data set */
    le_adv_data_set_handle  advert_handle;

    /*! Flag used to indicate advertising handle is allocated
        MessageSendConditionally needs a uint16 */
    uint16                  advertising_active;

    /*! The find command has been deferred as not in a valid state */
    bool                    find_pending;

    /*! Expected peer address. If zero then pairs with the device
        with the strongest RSSI. */
    bdaddr                  expected_device;

    /*! Data needed when acting as a client for the root key service */
    GATT_ROOT_KEY_CLIENT    root_key_client;

    /*! Context used if overriding local address */
    local_address_context_t local_addr_context;
} peerPairLeRunTimeData;


/*! Structure to hold the data used by for the LE peer pairing service */
typedef struct 
{
    /*! Task for handling messages */
    TaskData                    task;

    /*! Internal state of peer pairing module */
    PEER_PAIR_LE_STATE          state;
    
    /*! Instance of the GATT Root Key Server */
    GATT_ROOT_KEY_SERVER        root_key_server;

    /*! The bulk of the services data.
        This is only allocated when the service is running. */
    peerPairLeRunTimeData       *data;
} peerPairLeTaskData;


/*! Global storage for the LE peer pairing service data */
extern peerPairLeTaskData peer_pair_le;

/*! \name Accessor functions

    \brief Accessor functions into the LE peer pair service data 

    \note These functions must be used rather than direct access. This
    allows the service data to be allocated rather than global.
 */
/*! \{ */

/*! Accessor to get the LE peer pair service task data */
#define PeerPairLeGetTaskData()     (&peer_pair_le)

/*! Accessor to get the LE peer pair service run time data */
#define PeerPairLeGetData()         (peer_pair_le.data)

/*! Accessor to get the Task for the LE peer pair service */
#define PeerPairLeGetTask()         (&peer_pair_le.task)

/*! Accessor to get the LE peer pair server instance */
#define PeerPairLeGetRootKeyServer()    (peer_pair_le.root_key_server)

/*! Set the client of the LE peer pairing service */
#define PeerPairLeSetClient(_client) \
                            do {\
                                PeerPairLeGetData()->client = (_client);\
                            } while(0)

/*! Retrieve the client of the LE peer pairing service */
#define PeerPairLeGetClient()       (PeerPairLeGetData()->client)

/*! See if this device is for left ear */
#define PeerPairLeIsLeft()  (Multidevice_IsLeft())

/*! See if this device is for right ear */
#define PeerPairLeIsRight() (!PeerPairLeIsLeft())

/*! Bluetooth address of only allowed device for pairing */
#define PeerPairLeExpectedDevice() (PeerPairLeGetData()->expected_device)

/*! See if allowed device for pairing has been set */
#define PeerPairLeExpectedDeviceIsSet() (!BdaddrIsZero(&PeerPairLeExpectedDevice()))
/*! \} */


/*! Helper function to create messages to be despatched by MessageSend */
#define MAKE_PEER_PAIR_LE_MESSAGE(TYPE) TYPE##_T *message = (TYPE##_T*)PanicNull(calloc(1,sizeof(TYPE##_T)))


/*! Message identifiers used for internal messages */
typedef enum
{
        /*! Process a find peer request */
    PEER_PAIR_LE_INTERNAL_FIND_PEER = INTERNAL_MESSAGE_BASE,

        /*! Last request has completed. Clean up the module */
    PEER_PAIR_LE_INTERNAL_COMPLETED,

        /*! If advertising could not be started originally, start it now */
    PEER_PAIR_LE_INTERNAL_ENABLE_ADVERTISING,

        /* Timeout messages grouped together */

        /*! Timeout started on the first scan response has expired */
    PEER_PAIR_LE_TIMEOUT_FROM_FIRST_SCAN,
        /*!Timeout started to restart the discovery in case a device is found with low RSSI */
    PEER_PAIR_LE_TIMEOUT_REDISCOVERY,

        /*! This must be the final message */
    PEER_PAIR_LE_INTERNAL_MESSAGE_END
} peer_pair_le_internal_message_t;
ASSERT_INTERNAL_MESSAGES_NOT_OVERFLOWED(PEER_PAIR_LE_INTERNAL_MESSAGE_END)


#endif /* PEER_PAIR_LE_PRIVATE_H_ */

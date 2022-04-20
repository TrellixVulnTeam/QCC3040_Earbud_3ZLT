/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       
\brief      Header file for the peer service providing LE based pairing
*/

#ifndef PEER_PAIR_LE_H_
#define PEER_PAIR_LE_H_

#include <message.h>
#include "domain_message.h"
#include "le_scan_manager.h"

/*! \ingroup le_peer_pairing_service
    \{
*/

/*! Messages that may be sent externally by the LE peer pairing service */
typedef enum
{
    /*! Message sent on completion of pairing */
    PEER_PAIR_LE_PAIR_CFM = PEER_PAIR_LE_MESSAGE_BASE,
    /*! Message sent on completion of peer pair module initialisation */
    PEER_PAIR_LE_INIT_CFM,

    /*! This must be the final message */
    PEER_PAIR_LE_MESSAGE_END
} peer_pair_le_message_t;

/*! Status code included in LE peering pairing service messages */
typedef enum
{
    peer_pair_le_status_success,    /*!< The operation was successful */
    peer_pair_le_status_failed,     /*!< The operation failed */
} peer_pair_le_status_t;


/*! Message sent upon completion of a pairing operation requested by PeerPairLe_FindPeer */
typedef struct
{
    peer_pair_le_status_t status;   /*!< Status of the pairing operation */
} PEER_PAIR_LE_PAIR_CFM_T;

/*! \brief Peer Pairing service UI Provider contexts */
typedef enum
{
    context_peer_pairing_idle,
    context_peer_pairing_active,

} peer_pairing_provider_context_t;

/*! Initialise the pairing service.

    This function MUST be called first before using any other functionality of the pairing service.

    \param init_task    Task to send init completion message (if any) to

    \return TRUE
 */
bool PeerPairLe_Init(Task init_task);


/*! Ask the peer pairing service to find a peer.

    \param  task        Task to send message to on completion
    
 */
void PeerPairLe_FindPeer(Task task);

/*! Ask the peer pairing service to pair with a specific device

    Other than the check for address, the peer pairing mechanism will
    proceed as normal.

    \param  task        Task to send message to on completion
    \param  peer        Public address of the device to pair with
 */
void PeerPairLe_PairPeerWithAddress(Task task, const bdaddr *peer);

/*! Return if peer pairing is active 

    \return TRUE if peer pair LE is running, FALSE otherwise
 */
bool PeerPairLeIsRunning(void);

/*! Accepts the LE Advertisements from Scan Manager.

    \param  CL_DM_BLE_ADVERTISING_REPORT_IND_T  CL Structure for LE Adverts

 */
void PeerPairLe_HandleFoundDeviceScan(const LE_SCAN_MANAGER_ADV_REPORT_IND_T* scan);
/*! \}
*/

/*! \defgroup   le_peer_pairing_service Peer Pairing Service
    \ingroup    peer_service

    The LE Peer Pairing Service takes responsibility for using 
    Bluetooth Low Energy (BLE) to pair with another device, record the
    addresses of both devices, select which address is primary, and
    update the keys of both devices.

    The keys are updated so that in future each device can safely 
    share details of connections to other devices, such as handsets.
    This is part of Bluetooth Address Management.

    \note The primary address selected will always be that from the left
          earbud.

    <H2>What devices are found ?</H2>

    Only compatible devices will be found, compatibility being defined
    by 
    \li support for the LE peer pairing service. The service will attempt
        to connect with these devices.
    \li having the same 'secret'

    To reduce the chance of pairing with an unexpected device, two 
    additional criteria are used 
    \li The device must be close. This is determined by the received signal
        strength indication (RSSI), that gives the strength of the 
        advert received. This is controlled by \ref appConfigPeerPairLeMinRssi
    \li Only one device very close. The service can see several devices but
        will only keep track of the strongest. If the RSSI of the two 
        strongest devices are too similar, both will be rejected. This is 
        controlled by \ref appConfigPeerPairLeMinRssiDelta

    <H2>Using the service</H2>

    Just call \ref PeerPairLe_FindPeer on each device, and the operation is 
    fully autonomous.

    The following message is sent to indicate the final status.
    \ref PEER_PAIR_LE_PAIR_CFM

    If the status in the message is #peer_pair_le_status_success then the 
    service has paired successfully. Each device will have recorded the
    original Bluetooth device address of themselves and their peer. These
    addresses will be shared in future.

    The service also determines which of these addresses is the Primary 
    address that will be used in all communication with the handset, and can . 
    be accessed by \ref appDeviceGetPrimaryBdAddr.

    <H2>Basics of operation</H2>

    <H3>Find device</H3>

    The service works by advertising support for the peer pairing service.
    At the same time it scans for devices that support the service.
    Once an advertisement is seen a timeout is started to make sure that
    the device seen is the closest device. See 
    \ref appConfigPeerPairLeTimeoutPeerSelect.

    The timeout is based on a comparison of the Bluetooth device addresses
    of the devices. In typical usage both devices will recognise each other 
    at about the same time. The different timeouts ensure that one device 
    will connect first. 

    <H3>Connect</H3>

    When advertising the service is always trying to connect to a server.

    When an advert has been seen the service stops advertising and 
    attempt to make a connection to a client.

    When a connection is established the gatt root key service is started
    at each end of the link. As a server by one device and as a client by
    the other.

    <H3>Verify device and transfer keys</H3>

    The GATT service for root key transfer verifies that the
    devices are compatible, and transfers the keys between them 
    autonomously.

    A link key is also securely generated so that a standard Bluetooth
    Basic Rate / Enhandced Data Rate (BR/EDR) connection can be 
    made in future without the need to pair separately.

    <H3>Disconnect</H3>

    The connection between the devices is disconnected once the devices
    are paired. Although this has the disadvantage of removing a connection
    that will normally be required immediately, the need to reconnect
    ensures that connections between the devices can be made reliably after
    the pairing process.

*/

#endif /* PEER_PAIR_LE_H_ */

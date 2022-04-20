/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\defgroup   peer_pairing Peer pairing
\ingroup    bt_domain
\brief      Bluetooth domain functionality for creating paired peer devices 
            by command.

            Module contains API and functionality to add peer pairing records.
            This is normally done using a Bluetooth link in the \ref peer_pair_le 
            code.

            It is recommended that this module is only used from test code.
*/

/*! \addtogroup peer_pairing
 @{
*/

#ifndef PEER_PAIRING_H
#define PEER_PAIRING_H

#include <bdaddr.h>
#include <connection.h>

/*! \brief Type used to contain a 128 bit key for adding pairing for peer devices

    Similar types exist, but are applicable to specific libraries. Define
    a type unique to this API. */
typedef struct
{
    uint16 key[8];  /*!< Key value */
} PEER_PAIRING_LONG_TERM_KEY_T;

/*! \brief Update or replace pairing to a peer with pairing to a new address

    This function is for use to pair two devices without the need of any radio 
    connection. The anticipated use is in a factory setting using pydbg
    or \ref device_test_service, or from a charger case.

    \note It does not matter if the devices already have earbud pairing. 

    \note The earbuds cannot be connected to an earbud. They should be 
        disconnected first.

    \param primary          Pointer to the primary device address in the pairing
    \param secondary        Pointer to the secondary device address in the pairing
    \param is_primary       Whether this is the device that owns the primary address
    \param randomised_keys  New root keys to be set. This is the encryption key
                            and identity root. These must be the same on each device
                            in an earbud pair.
    \param bredr_key        Pointer to the BREDR link key.
    \param le_key           Pointer to the Bluetooth Low Energy long term key.

    \return TRUE if the pairing was successful and records were created. FALSE if 
            any errors were detected.
 */
bool PeerPairing_AddPeerPairing(const bdaddr *primary,
                                const bdaddr *secondary,
                                bool this_is_primary,
                                const cl_root_keys *randomised_keys,
                                PEER_PAIRING_LONG_TERM_KEY_T *bredr_key,
                                PEER_PAIRING_LONG_TERM_KEY_T *ble_key);


/*! \brief Remove existing peer pairing and request pairing to a specific device

    The anticipated use is in a factory setting using pydbg or
    the \ref device_test_service, or from a charger case.

    Pairing is performed asynchronously. Use BtDevice_IsPairedWithPeer()
    to check for completion of pairing. PeerPairLeIsRunning() may be used to
    see if pairing is in progress.

    \note Existing peer pairing will be removed, but the device should not be 
    active when called. Similar to PeerPairing_AddPeerPairing() the earbuds 
    cannot be connected to an earbud. They should be disconnected first.

    \param task     Task that will receive a confirm message when peering has
                    been added PEER_PAIR_LE_PAIR_CFM
    \param target   Pointer to the bluetooth device address with which to pair

    \return FALSE if existing pairing could not be removed. TRUE if any existing 
            pairing has been removed and pairing has been requested.
 */
bool PeerPairing_PeerPairToAddress(Task task, const bdaddr *target);

#endif /* PEER_PAIRING_H */

/*! @} End group documentation in Doxygen */


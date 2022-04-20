/*!
\copyright  Copyright (c) 2017 - 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of specifc application testing functions
*/

#include "peer_pairing.h"

#include "device_db_serialiser.h"
#include <device.h>
#include <device_properties.h>
#include <device_list.h>
#include <connection.h>
#include <connection_manager.h>
#include <peer_pair_le.h>

#include <logging.h>
#include <panic.h>

#include "..\..\adk\src\libs\connection\connection_private.h"
#include "..\..\adk\src\libs\connection\connection_tdl.h"


/*! \brief Remove the two device entries for earbuds

    This function will only be successful if the earbuds are not connected.

    \note Only earbud entries are removed.

    \return TRUE if entries were removed successfully, FALSE in case of a failure
 */
static bool peerPairing_RemoveEarbudDevices(void)
{
    bool removed = FALSE;
    bdaddr local;
    bdaddr remote;

    if (appDeviceGetPeerBdAddr(&remote))
    {
        removed = TRUE;

        if (!appDeviceDelete(&remote))
        {
            return FALSE;
        }
    }
    if (appDeviceGetMyBdAddr(&local))
    {
        removed = TRUE;

        if (!appDeviceDelete(&local))
        {
            return FALSE;
        }
    }

    if (removed)
    {
        DEBUG_LOG("peerPairing_RemoveEarbudDevices. Was a pre-existing pairing record.");
    }

    return TRUE;
}


static bool peerPairing_AddAuthDevice(const bdaddr *addr,
                                   const PEER_PAIRING_LONG_TERM_KEY_T *bredr)
{
    CL_INTERNAL_SM_ADD_AUTH_DEVICE_REQ_T req= {0};

    req.bd_addr = *addr;
    req.bonded = TRUE;
    /* Key is supplied out of band, so mark as trusted and authenticated.
       With over the air pairing, neither of these is true */
    req.trusted = TRUE;
    req.enc_bredr.link_key_type = DM_SM_LINK_KEY_AUTHENTICATED_P256;
    memcpy(req.enc_bredr.link_key, bredr->key, sizeof(req.enc_bredr.link_key));

    return connectionAuthAddDevice(&req);
}

/*! Add/update the LE keys held in the connection library records

    Replace/Add Central and IRK KEY entries.. The IRK is retrieved using
    connection library APIs.

    \note This uses internal connection library APIs, which takes an array of
       key entries to update. Make use of this to make a single call
       updating the long term (LTK, Central) and resolving (IRK) keys 
       at the same time

    \param addr The public address to update
    \param le   The Central (long term key) for the link

    \return TRUE if successful, otherwise FALSE
*/
static bool peerPairing_UpdateAuthKeys(const bdaddr *addr, 
                                       const PEER_PAIRING_LONG_TERM_KEY_T *le)
{
    DM_SM_UKEY_T key_entry;
    DM_SM_KEYS_T smk = {0};
    cl_irk irk;
        /* LE Key. Rand and diversifier initialised to 0 */
    DM_SM_KEY_ENC_CENTRAL_T le_key = {0};
    typed_bdaddr vm_typed_addr = { .type = TYPED_BDADDR_PUBLIC, .addr = *addr};
    TYPED_BD_ADDR_T typed_addr;

    BdaddrConvertTypedVmToBluestack(&typed_addr, &vm_typed_addr);

    /* Populate the LE key entry in the update list */
    memcpy(le_key.ltk,le->key,sizeof(le_key.ltk));
    key_entry.enc_central = &le_key;
    smk.u[0] = key_entry;

    /* Retrieve and populate the IRK */
    if (!ConnectionSmGetLocalIrk(&irk))
    {
        return FALSE;
    }
    key_entry.id = (DM_SM_KEY_ID_T*)&irk;
    smk.u[1] = key_entry;

    /* Now set the parameters for updating the key, letting the connection
       library know we are supplying two keys.
       No return value to check */
    smk.security_requirements = 5;
    smk.encryption_key_size = sizeof(le_key.ltk);
    smk.present =   DM_SM_KEY_ENC_CENTRAL << (DM_SM_NUM_KEY_BITS * 0)
                  | DM_SM_KEY_ID          << (DM_SM_NUM_KEY_BITS * 1)
                  | DM_SM_KEYS_UPDATE_EXISTING;

    connectionAuthUpdateTdl(&typed_addr, &smk);

    return TRUE;
}

/*! \brief Create an entry for one of the two peer devices

    Create a connection library pairing record for the requested device
    making sure it has the needed BREDR and LE keys. Then create a 
    matching entry in the device list.

    Flags are set based on parameters passed.

    \param[in] addr         Public address of device to add
    \param     device_type  Type of device (SELF or EARBUD)
    \param     device_flags Flags to set for device
    \param[in] bredr        Link key for BREDR
    \param[in] le           Link key for LE

    \return FALSE if an error detected
*/
static bool peerPairing_AddPeerPairDevice(const bdaddr *addr,
                                         deviceType device_type,
                                         uint16 device_flags,
                                         const PEER_PAIRING_LONG_TERM_KEY_T *bredr,
                                         const PEER_PAIRING_LONG_TERM_KEY_T *le)
{
    device_t device;

    if (!peerPairing_AddAuthDevice(addr, bredr))
    {
        DEBUG_LOG("peerPairing_AddPeerPairDevice Failed creating connection library device entry");

        return FALSE;
    }

    device = BtDevice_GetDeviceCreateIfNew(addr, device_type);
    if (   !Device_SetPropertyU16(device, device_property_flags, device_flags)
        || !BtDevice_SetDefaultProperties(device)
        || !ConnectionAuthSetPriorityDevice(addr, TRUE)
        || !peerPairing_UpdateAuthKeys(addr, le))
    {
        DEBUG_LOG("peerPairing_AddPeerPairDevice Failed creating BT device");

        return FALSE;
    }

    return TRUE;
}

bool PeerPairing_AddPeerPairing(const bdaddr *primary,
                                const bdaddr *secondary,
                                bool this_is_primary,
                                const cl_root_keys *randomised_keys,
                                PEER_PAIRING_LONG_TERM_KEY_T *bredr,
                                PEER_PAIRING_LONG_TERM_KEY_T *le)
{
    const bdaddr *self;
    const bdaddr *peer;
    uint16 flags = DEVICE_FLAGS_MIRRORING_ME;

    DEBUG_LOG("PeerPairing_AddPeerPairing. Adding earbud pairing record. Primary:%d",
                    this_is_primary);

    /* Get ready to create the stuff we need
            Remove any existing device entries
            Update the root keys. */
    if (   !peerPairing_RemoveEarbudDevices()
        || !ConnectionSetRootKeys((cl_root_keys *)randomised_keys))
    {
        DEBUG_LOG("PeerPairing_AddPeerPairing Failed clearing devices / setting root");
        return FALSE;
    }

    if (this_is_primary)
    {
        self = primary;
        peer = secondary;
        flags |= DEVICE_FLAGS_MIRRORING_C_ROLE | DEVICE_FLAGS_PRIMARY_ADDR;
    }
    else
    {
        self = secondary;
        peer = primary;
        flags |= DEVICE_FLAGS_SECONDARY_ADDR;
    }

    if (!peerPairing_AddPeerPairDevice(self, DEVICE_TYPE_SELF, flags, bredr, le))
    {
        DEBUG_LOG("PeerPairing_AddPeerPairing Failed creating entries for self");

        return FALSE;
    }

    /* Swap the flags in line with the usage from peer_pair_le */
    flags = flags ^ (  DEVICE_FLAGS_MIRRORING_C_ROLE
                     | DEVICE_FLAGS_MIRRORING_ME
                     | DEVICE_FLAGS_PRIMARY_ADDR | DEVICE_FLAGS_SECONDARY_ADDR);
    if (!peerPairing_AddPeerPairDevice(peer, DEVICE_TYPE_EARBUD, flags, bredr, le))
    {
        DEBUG_LOG("PeerPairing_AddPeerPairing Failed creating entries for peer");

        return FALSE;
    }

    /* Make sure the keys are updated */
    DeviceDbSerialiser_Serialise();

    return TRUE;
}


bool PeerPairing_PeerPairToAddress(Task task, const bdaddr *target)
{
    if (!peerPairing_RemoveEarbudDevices())
    {
        DEBUG_LOG("PeerPairing_PeerPairToAddress Failed clearing devices");
        return FALSE;
    }

    PeerPairLe_PairPeerWithAddress(task, target);
    DEBUG_LOG("PeerPairing_PeerPairToAddress Pairing requested");
    return TRUE;
}


/*!
\copyright  Copyright (c) 2005 - 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       sdp.h
\brief      Header file for the SDP definitions
*/

#ifndef SDP_H_
#define SDP_H_


#include <bdaddr.h>

/*! Alternative UUIDs for TWS+ Sink service and profile when TWS+ is disabled, service
 * and profile are still used between Earbuds*/
#define UUID_TWS_SINK_SERVICE       0x00,0x00,0xeb,0x01,0xd1,0x02,0x11,0xe1,0x9b,0x23,0x00,0x02,0x5b,0x00,0xa5,0xa5
#define UUID_TWS_PROFILE_SERVICE    0x00,0x00,0xeb,0x02,0xd1,0x02,0x11,0xe1,0x9b,0x23,0x00,0x02,0x5b,0x00,0xa5,0xa5

/*! UUID for SCO forwarding service */
#define UUID_SCO_FWD_SERVICE        0x00,0x00,0xeb,0x03,0xd1,0x02,0x11,0xe1,0x9b,0x23,0x00,0x02,0x5b,0x00,0xa5,0xa5

/*! UUID for Qualcomm Peer Sync Service */
#define UUID_PEER_SYNC_SERVICE      0x00,0x00,0xeb,0x04,0xd1,0x02,0x11,0xe1,0x9b,0x23,0x00,0x02,0x5b,0x00,0xa5,0xa5

/*! UUID for Qualcomm Device Upgrade Peer Service */
#define UUID_DFU_PEER_SERVICE      0x00,0x00,0xeb,0x05,0xd1,0x02,0x11,0xe1,0x9b,0x23,0x00,0x02,0x5b,0x00,0xa5,0xa5

/*! UUID for Qualcomm Handover Profile Service */
#define UUID_HANDOVER_PROFILE_SERVICE      0x00,0x00,0xeb,0x06,0xd1,0x02,0x11,0xe1,0x9b,0x23,0x00,0x02,0x5b,0x00,0xa5,0xa5
/*! UUID for Qualcomm Mirror Profile Service */
#define UUID_MIRROR_PROFILE_SERVICE      0x00,0x00,0xeb,0x07,0xd1,0x02,0x11,0xe1,0x9b,0x23,0x00,0x02,0x5b,0x00,0xa5,0xa5

#ifdef INCLUDE_GAIA_PYDBG_REMOTE_DEBUG
/*! UUID for Qualcomm GAIA Debug Peer Link Service */
#define UUID_GAIA_DEBUG_PEER_LINK_SERVICE   0x00, 0x00, 0x11, 0x09, 0xd1, 0x02, 0x11, 0xe1, 0x9b, 0x23, 0x00, 0x02, 0x5b, 0x00, 0xa5, 0xa5
#endif /* INCLUDE_GAIA_PYDBG_REMOTE_DEBUG */

/*! SDP Service Class Attribute UUID */
#define UUID_SERVICE_CLASS_ID_LIST (0x0001)

#define UUID_PROTOCOL_DESCRIPTOR_LIST (0x0004)

/*! SDP Bluetooth Profile Descriptor List Attribute UUID */
#define UUID_BT_PROFILE_DESCRIPTOR_LIST (0x0009)

#define UUID_BT_SUPPORTED_FEATURES (0x0311)


/*! SDP Peer Bluetooth Address Attribute UUID */
#define UUID_PEER_BDADDR (0x0200)

/* Unsigned Integer with Size Index 1 (implicit, 2 bytes) */
/*! SDP Data Element for uint8 */
#define SDP_DATA_EL_UINT8(value)  (0x08 + 0), ((value) & 0xFF)

/*! SDP Data Element for uint16 */
#define SDP_DATA_EL_UINT16(value) (0x08 + 1), (((value) >> 8) & 0xFF), ((value) & 0xFF)

/*! SDP Data Element for uint32 */
#define SDP_DATA_EL_UINT32(value) (0x08 + 2), (((value) >> 24) & 0xFF), (((value) >> 16) & 0xFF), (((value) >> 8) & 0xFF), ((value) & 0xFF)

#define SDP_DATA_EL_UUID16(value) 0x19, (((value) >> 8) & 0xFF), ((value) & 0xFF)

/*! Data Element Sequence with (count) uint16s, Size Index 5 (explicit) */
#define SDP_DATA_EL_UINT_16_LIST(count) (0x35),(3 * (count))

/*! UUID with Size Index 4 (implicit, 16 bytes) */
#define SDP_DATA_EL_UUID128_2(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p)  (0x1C),a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p
#define SDP_DATA_EL_UUID128(uuid)  SDP_DATA_EL_UUID128_2(uuid)

/*! Data Element Sequence with (count) UUID128s, Size Index 5 (explicit) */
#define SDP_DATA_EL_UUID_128_LIST(count) (0x35),(17 * (count))

/*! Marco for SDP Attribute ID */
#define SDP_ATTR_ID(id) SDP_DATA_EL_UINT16(id)

/*! Marco for SDP Data Element Sequence */
#define SDP_DATA_EL_SEQ(size)   (0x35), (size)


/*! \brief Extract the TWS version from SDP records supplied

    Used to extract the TWS version from a full SDP record, delineated
    by pointers to start (ptr) and end of the SDP.

    \param  ptr         Pointer to the start of the SDP records
    \param  end         Pointer to the end of the SDP records
    \param  tws_version Address of a value to take the TWS version. This cannot be NULL.

    \returns TRUE if a TWS version was found in the SDP records, FALSE otherwise.
 */
bool appSdpFindTwsVersion(const uint8 *ptr, const uint8 *end, uint16 *tws_version);

/*! \brief Get a pointer to the service record of the TWS Sink

	\returns A pointer to the service record of the TWS Sink
*/
const uint8 *appSdpGetTwsSinkServiceRecord(void);

/*! \brief Get the size of the TWS Sink service record

	\returns the size of the TWS Sink service record
*/
uint16 appSdpGetTwsSinkServiceRecordSize(void);

/*! \brief Get a pointer to an SDP search record that can be used to find a TWS sink

    The pointer returned can be passed to ConnectionSdpServiceSearchAttributeRequest
	as the search_pattern parameter.

	\returns A pointer to an SDP search record that can be used to find a TWS sink
 */
const uint8 *appSdpGetTwsSinkServiceSearchRequest(void);

/*! \brief Get the size of the TEST Sink search request record

	\returns The size of the TWS Sink search request record
*/
uint16 appSdpGetTwsSinkServiceSearchRequestSize(void);

/*! \brief Get pointer to an attribute search record that can be used to find a TWS sink

	The pointer returned can be passed to ConnectionSdpServiceSearchAttributeRequest
	as the search_pattern parameter.

	\returns A pointer to an attribute search record that can be used to find a TWS sink
*/
const uint8 *appSdpGetTwsSinkAttributeSearchRequest(void);

/*! \brief Get the size of the attribute search request record

	\returns The size of the TWS Sink attribute search request record
*/
uint16 appSdpGetTwsSinkAttributeSearchRequestSize(void);

/*! \brief Get a pointer to an SDP search record that can be used to find a TWS source

    The pointer returned can be passed to ConnectionSdpServiceSearchAttributeRequest
	as the search_pattern parameter.

	\returns A pointer to an SDP search record that can be used to find a TWS source
 */
const uint8 *appSdpGetTwsSourceServiceSearchRequest(void);

/*! \brief Get the size of the TWS Source search request record

	\returns The size of the TWS Source search request record
*/
uint16 appSdpGetTwsSourceServiceSearchRequestSize(void);

/*! \brief Get a pointer to an attribute search record that can be used to find a TWS Source

    The pointer returned can be passed to ConnectionSdpServiceSearchAttributeRequest
	as the search_pattern parameter.

	\returns A pointer to an attribute search record that can be used to find a TWS sink
*/
const uint8 *appSdpGetTwsSourceAttributeSearchRequest(void);

/*! \brief Get the size of the TWS Source attribute search request record

	\returns The size of the TWS Source attribute search request record
*/
uint16 appSdpGetTwsSourceAttributeSearchRequestSize(void);

/*! \brief Populate a TWS Sink Service Record with Bluetooth address

    This function overwrites the space for Bluetooth address in an
    existing SDP record.

    \param  record    Pointer to the TWS Sink service record
    \param  bd_addr   Pointer to the Peer Bluetooth address
 */
void appSdpSetTwsSinkServiceRecordPeerBdAddr(uint8 *record, const bdaddr *bd_addr);

/* Peer Signalling */
void appSdpSetPeerSigPsm(uint8 *record, uint16 psm);

const uint8 *appSdpGetPeerSigServiceRecord(void);
uint16 appSdpGetPeerSigServiceRecordSize(void);
const uint8 *appSdpGetPeerSigServiceSearchRequest(void);
uint16 appSdpGetPeerSigServiceSearchRequestSize(void);

const uint8 *appSdpGetPeerSigAttributeSearchRequest(void);
uint16 appSdpGetPeerSigAttributeSearchRequestSize(void);
/* Mirror Profile */
void appSdpSetMirrorProfilePsm(uint8 *record, uint16 psm);

const uint8 *appSdpGetMirrorProfileServiceRecord(void);
uint16 appSdpGetMirrorProfileServiceRecordSize(void);
const uint8 *appSdpGetMirrorProfileServiceSearchRequest(void);
uint16 appSdpGetMirrorProfileServiceSearchRequestSize(void);

const uint8 *appSdpGetMirrorProfileAttributeSearchRequest(void);
uint16 appSdpGetMirrorProfileAttributeSearchRequestSize(void);
#ifdef INCLUDE_DFU_PEER
/* Upgrade Peer */

/*! \brief Write L2CAP PSM into service record

    \returns None
*/
void appSdpSetDFUPeerPsm(uint8 *record, uint16 psm);

/*! \brief Get the size of the Device Upgrade Peer service record

    \returns the size of the Device Upgrade Peer service record
*/
uint16 appSdpGetDFUPeerServiceRecordSize(void);

/*! \brief Get a pointer to the service record of the Device Upgrade Peer

    \returns A pointer to the service record of the Device Upgrade Peer
*/
const uint8 *appSdpGetDFUPeerServiceRecord(void);
/*! \brief Get the size of the Device Upgrade Peer search request record

    \returns The size of the Device Upgrade Peer search request record
*/
uint16 appSdpGetDFUPeerServiceSearchRequestSize(void);

/*! \brief Get a pointer to an SDP search record that can be used to find a Device Upgrade Peer

    The pointer returned can be passed to ConnectionSdpServiceSearchAttributeRequest
    as the search_pattern parameter.

    \returns A pointer to an SDP search record that can be used to find a Device Upgrade Peer
 */
const uint8 *appSdpGetDFUPeerServiceSearchRequest(void);

/*! \brief Get the size of the attribute search request record

    \returns The size of the Device Upgrade Peer attribute search request record
*/
uint16 appSdpGetDFUPeerAttributeSearchRequestSize(void);

/*! \brief Get pointer to an attribute search record that can be used to find a Device Upgrade Peer

    The pointer returned can be passed to ConnectionSdpServiceSearchAttributeRequest
    as the search_pattern parameter.

    \returns A pointer to an attribute search record that can be used to find a Device Upgrade Peer
*/
const uint8 *appSdpGetDFUPeerAttributeSearchRequest(void);
#endif /* INCLUDE_DFU_PEER */

/* Handover Profile */
/*! \brief Set Handover Profile L2CAP PSM into service record

    \param[in] record       Pointer to SDP service record
    \param[in] psm          PSM of Handover profile
*/
void sdp_SetHandoverProfilePsm(uint8 *record, uint16 psm);

/*! \brief Get a pointer to the service record of Handover Profile

    \returns A pointer to the service record of Handover Profile
*/
const uint8 *sdp_GetHandoverProfileServiceRecord(void);

/*! \brief Get the size of the Handover Profile service record

    \returns Size of Handover Profile service record
*/
uint16 sdp_GetHandoverProfileServiceRecordSize(void);

/*! \brief Get a pointer to an SDP search record that can be used to find Handover Profile

    The pointer returned can be passed to ConnectionSdpServiceSearchAttributeRequest
    as the search_pattern parameter.

    \returns A pointer to an SDP search record that can be used to find a Handover Profile
 */
const uint8 *sdp_GetHandoverProfileServiceSearchRequest(void);

/*! \brief Get the size of Handover Profile search request record

    \returns The size of Handover Profile search request record
*/
uint16 sdp_GetHandoverProfileServiceSearchRequestSize(void);

/*! \brief Get pointer to an attribute search record that can be used to find Handover Profile

    The pointer returned can be passed to ConnectionSdpServiceSearchAttributeRequest
    as the search_pattern parameter.

    \returns A pointer to an attribute search record that can be used to find Handover Profile
*/
const uint8 *sdp_GetHandoverProfileAttributeSearchRequest(void);

/*! \brief Get the size of the attribute search request record

    \returns The size of Handover Profile attribute search request record
*/
uint16 sdp_GetHandoverProfileAttributeSearchRequestSize(void);


/*! Accessor function to get the SDP record for the device test service

    The Device Test Service should have an entry so that it can
    be identified using the Service Discovery Protocol (SDP).

    This function gives the service access, and allows for the
    record to be moved in future.

    \param[out] length Pointer to value to take the record length

    \return pointer to the service record
 */
const uint8 *sdp_GetDeviceTestServiceServiceRecord(uint16 *length);


#ifdef INCLUDE_GAIA_PYDBG_REMOTE_DEBUG
/*! Accessor function to get the SDP record for the GAIA Debug Peer Link service.

    This function returns the pointer to the service record for the GAIA Debug
    Peer Link service.

    The GAIA Debug (also known as the 'Wireless Debug' over BT) uses a link
    between the Primary and the Secondary for Pydbg Remote Debug, which sends
    Pydbg commands to the Secondary and receives responses from it.

    \param[out] Length Pointer to value to take the record length (in bytes).

    \return Pointer to the service record.
 */
const uint8 *sdp_GetGaiaDebugPeerLinkServiceRecord(uint16 *length);

/*! \brief Get the offset to the PSM in the service record for the GAIA Debug
           Peer Link.
           
    This function returns the position of the PSM, which will be dynamically
    allocated by the stack.

    \return Pointer to the service search record.
 */
uint16 appSdpGetGaiaDebugPeerLinkServiceRecordPsmOffset(void);


/*! \brief Get a pointer to the service search pattern for the GAIA Debug Peer Link.

    \param[out] Length Pointer to value to take the length of the service
                search pattern (in bytes).

    \return Pointer to the service search pattern.
 */
const uint8 *appSdpGetGaiaDebugPeerLinkServiceSearchRequest(uint16 *length);

/*! \brief Get a pointer to the attribute search pattern for the GAIA Debug Peer Link.

    \param[out] Length Pointer to value to take the length of the attribute
                search pattern (in bytes).

    \return Pointer to the attribute search pattern.
 */
const uint8 *appSdpGetGaiaDebugPeerLinkAttributeSearchRequest(uint16 *length);
#endif /* INCLUDE_GAIA_PYDBG_REMOTE_DEBUG */

#endif /* SDP_H_ */

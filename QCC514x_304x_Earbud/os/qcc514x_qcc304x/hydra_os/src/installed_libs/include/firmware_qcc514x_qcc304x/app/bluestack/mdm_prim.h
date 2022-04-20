/*!

Copyright (c) 2019 - 2021 Qualcomm Technologies International, Ltd.
  %%version

\file   mdm_prim.h

\brief  Bluestack Mirror Device Manager

        The functionalities exposed in this file are Qualcomm proprietary.

        Bluestack Mirror Device Manager provides application interface to perform mirroring functionalities.
        Currently the functionalities are only applicable for BR/EDR and 
        eSCO links.
*/
#ifndef _MDM_PRIM_H_
#define _MDM_PRIM_H_

#include "hci.h"
#include "dm_prim.h"
#include "l2cap_prim.h"

#ifdef __cplusplus
extern "C" {
#endif

/*!
    \name Response/result error and status codes

    \{
*/
/*! Operation was successful */
#define MDM_RESULT_SUCCESS                      0x0000
#define MDM_RESULT_INVALID_PARAM                0x0001
#define MDM_RESULT_INPROGRESS                   0x0002
#define MDM_RESULT_FAIL                         0x0003
#define MDM_RESULT_TIMEOUT                      0x0004


/*! \name Bluestack primitive segmentation and numbering

    \brief MDM primitives occupy the number space from
    MDM_PRIM_BASE to (MDM_PRIM_BASE | 0x00FF).

    \{ */
#define MDM_PRIM_DOWN           (MDM_PRIM_BASE)
#define MDM_PRIM_UP             (MDM_PRIM_BASE | 0x0080)
#define MDM_PRIM_MAX            (MDM_PRIM_BASE | 0x00FF)

typedef enum mdm_prim_tag
{
    /* downstream primitives */
    ENUM_MDM_REGISTER_REQ = MDM_PRIM_DOWN,
    ENUM_MDM_SET_BREDR_SLAVE_ADDRESS_REQ,
    ENUM_MDM_LINK_CREATE_REQ,
    ENUM_MDM_LINK_DISCONNECT_REQ,
    ENUM_MDM_L2CAP_CREATE_REQ,
    ENUM_MDM_L2CAP_CREATE_RSP,
    ENUM_MDM_L2CAP_DISCONNECT_REQ,
    ENUM_MDM_L2CAP_DISCONNECT_RSP,
    ENUM_MDM_LE_CIS_CREATE_REQ,
    ENUM_MDM_LE_CIS_DISCONNECT_REQ,

    ENUM_MDM_DEBUG_TRAP_API_REQ = MDM_PRIM_UP - 1,

    /* upstream primitives */
    ENUM_MDM_REGISTER_CFM = MDM_PRIM_UP,
    ENUM_MDM_SET_BREDR_SLAVE_ADDRESS_CFM,
    ENUM_MDM_SET_BREDR_SLAVE_ADDRESS_IND,
    ENUM_MDM_ACL_LINK_CREATE_CFM,
    ENUM_MDM_ACL_LINK_CREATE_IND,
    ENUM_MDM_LINK_DISCONNECT_CFM,
    ENUM_MDM_LINK_DISCONNECT_IND,
    ENUM_MDM_ESCO_LINK_CREATE_CFM,
    ENUM_MDM_ESCO_LINK_CREATE_IND,
    ENUM_MDM_ESCO_RENEGOTIATED_IND,
    ENUM_MDM_L2CAP_CREATE_IND,
    ENUM_MDM_L2CAP_CREATE_CFM,
    ENUM_MDM_L2CAP_DISCONNECT_IND,
    ENUM_MDM_L2CAP_DISCONNECT_CFM,
    ENUM_MDM_L2CAP_DATA_SYNC_IND,
    ENUM_MDM_MIRROR_MODE_CHANGE_EVENT_IND,
    ENUM_MDM_LE_CIS_CREATE_CFM,
    ENUM_MDM_LE_CIS_CREATE_IND,
    ENUM_MDM_LE_CIS_DISCONNECT_CFM,
    ENUM_MDM_LE_CIS_DISCONNECT_IND,

    ENUM_MDM_DEBUG_IND = MDM_PRIM_MAX - 2,
    ENUM_MDM_DEBUG_TRAP_API_CFM = MDM_PRIM_MAX - 1

} MDM_PRIM_T;


/**
 * Type definition used to specify mirror return and reason code.
 * The return and reason code corresponds to HCI and Qualcomm proprietary code.
 *
 * 0x0000 - 0x00FF  Defined by Bluetooth Core Specification, Volume 2, Part D.
 * 0x0100 - 0x01FF  Reserved for Future Qualcomm Use (RFQU).
 */
typedef uint16_t mdm_return_t;
typedef uint16_t mdm_reason_t;

/**
 * Type definition used to specify mirror link type.
 */
typedef uint8_t link_type_t;

#define LINK_TYPE_ACL       ((link_type_t)0x01)
#define LINK_TYPE_ESCO      ((link_type_t)0x02)

/**
 * Type definition used to specify mirror role type.
 */
typedef uint8_t mirror_role_t;
 
#define ROLE_TYPE_PRIMARY       ((mirror_role_t)0x00)
#define ROLE_TYPE_SECONDARY     ((mirror_role_t)0x01)

/**
 * Type definition used to specify CIS ownership type.
 */
typedef uint8_t cis_ownership_t;

#define CIS_NO_DELEGATE_NO_MIRRORING_SECONDARY ((cis_ownership_t) 0x00)
#define CIS_DELEGATE_SECONDARY  ((cis_ownership_t) 0x01)
#define CIS_MIRRORING_SECONDARY ((cis_ownership_t) 0x02)

/* downstream primitives */
#define MDM_REGISTER_REQ                ((mdm_prim_t)ENUM_MDM_REGISTER_REQ)
#define MDM_SET_BREDR_SLAVE_ADDRESS_REQ ((mdm_prim_t)ENUM_MDM_SET_BREDR_SLAVE_ADDRESS_REQ)
#define MDM_LINK_CREATE_REQ      ((mdm_prim_t)ENUM_MDM_LINK_CREATE_REQ)
#define MDM_LINK_DISCONNECT_REQ  ((mdm_prim_t)ENUM_MDM_LINK_DISCONNECT_REQ)
#define MDM_L2CAP_CREATE_REQ     ((mdm_prim_t)ENUM_MDM_L2CAP_CREATE_REQ)
#define MDM_L2CAP_CREATE_RSP     ((mdm_prim_t)ENUM_MDM_L2CAP_CREATE_RSP)
#define MDM_L2CAP_DISCONNECT_REQ ((mdm_prim_t)ENUM_MDM_L2CAP_DISCONNECT_REQ)
#define MDM_L2CAP_DISCONNECT_RSP ((mdm_prim_t)ENUM_MDM_L2CAP_DISCONNECT_RSP)
#define MDM_LE_CIS_CREATE_REQ    ((mdm_prim_t)ENUM_MDM_LE_CIS_CREATE_REQ)
#define MDM_LE_CIS_DISCONNECT_REQ ((mdm_prim_t)ENUM_MDM_LE_CIS_DISCONNECT_REQ)


/* upstream primitives */
#define MDM_REGISTER_CFM                ((mdm_prim_t)ENUM_MDM_REGISTER_CFM)
#define MDM_SET_BREDR_SLAVE_ADDRESS_CFM ((mdm_prim_t)ENUM_MDM_SET_BREDR_SLAVE_ADDRESS_CFM)
#define MDM_SET_BREDR_SLAVE_ADDRESS_IND ((mdm_prim_t)ENUM_MDM_SET_BREDR_SLAVE_ADDRESS_IND)
#define MDM_ACL_LINK_CREATE_CFM  ((mdm_prim_t)ENUM_MDM_ACL_LINK_CREATE_CFM)
#define MDM_ACL_LINK_CREATE_IND  ((mdm_prim_t)ENUM_MDM_ACL_LINK_CREATE_IND)
#define MDM_LINK_DISCONNECT_CFM  ((mdm_prim_t)ENUM_MDM_LINK_DISCONNECT_CFM)
#define MDM_LINK_DISCONNECT_IND  ((mdm_prim_t)ENUM_MDM_LINK_DISCONNECT_IND)
#define MDM_ESCO_LINK_CREATE_CFM ((mdm_prim_t)ENUM_MDM_ESCO_LINK_CREATE_CFM)
#define MDM_ESCO_LINK_CREATE_IND ((mdm_prim_t)ENUM_MDM_ESCO_LINK_CREATE_IND)
#define MDM_ESCO_RENEGOTIATED_IND ((mdm_prim_t)ENUM_MDM_ESCO_RENEGOTIATED_IND)
#define MDM_L2CAP_CREATE_IND     ((mdm_prim_t)ENUM_MDM_L2CAP_CREATE_IND)
#define MDM_L2CAP_CREATE_CFM     ((mdm_prim_t)ENUM_MDM_L2CAP_CREATE_CFM)
#define MDM_L2CAP_DISCONNECT_IND ((mdm_prim_t)ENUM_MDM_L2CAP_DISCONNECT_IND)
#define MDM_L2CAP_DISCONNECT_CFM ((mdm_prim_t)ENUM_MDM_L2CAP_DISCONNECT_CFM)
#define MDM_L2CAP_DATA_SYNC_IND ((mdm_prim_t)ENUM_MDM_L2CAP_DATA_SYNC_IND)
#define MDM_MIRROR_MODE_CHANGE_EVENT_IND ((mdm_prim_t)ENUM_MDM_MIRROR_MODE_CHANGE_EVENT_IND)
#define MDM_LE_CIS_CREATE_CFM    ((mdm_prim_t)ENUM_MDM_LE_CIS_CREATE_CFM)
#define MDM_LE_CIS_CREATE_IND    ((mdm_prim_t)ENUM_MDM_LE_CIS_CREATE_IND)
#define MDM_LE_CIS_DISCONNECT_CFM ((mdm_prim_t)ENUM_MDM_LE_CIS_DISCONNECT_CFM)
#define MDM_LE_CIS_DISCONNECT_IND ((mdm_prim_t)ENUM_MDM_LE_CIS_DISCONNECT_IND)

#define MDM_DEBUG_IND                   ((mdm_prim_t)ENUM_MDM_DEBUG_IND)
/*! \} */

/*! \brief Types for MDM */
typedef uint16_t                mdm_prim_t;
typedef uint16_t                mdm_result_t;

/*! \brief Register the MDM subsystem request

    Before any MDM operations can be performed the MDM subsystem shall
    be registered and a destination phandle for upstream application
    primitives shall also be registered.
*/
typedef struct
{
    mdm_prim_t          type;           /*!< Always MDM_REGISTER_REQ */
    phandle_t           phandle;        /*!< Destination phandle */
} MDM_REGISTER_REQ_T;


typedef struct
{
    mdm_prim_t          type;           /*!< Always MDM_REGISTER_CFM */
    phandle_t           phandle;        /*!< Destination phandle */
    mdm_result_t        result;         /*!< Result code - uses MDM_RESULT range */
} MDM_REGISTER_CFM_T;


/*! \brief Set slave address to a requested bluetooth address

    Change the address of slave device of a given link
    The initiating device will receive MDM_SET_BREDR_SLAVE_ADDRESS_CFM
    when the request is processed. The other device will receive
    MDM_SET_BREDR_SLAVE_ADDRESS_IND once the request is processed.

    If the status of the request is successful then 
    MDM_SET_BREDR_SLAVE_ADDRESS_CFM/MDM_SET_BREDR_SLAVE_ADDRESS_IND will
    contain old bluetooth address and new bluetooth address set on the 
    slave device.

    Flags field in the confirmation or indication message will identify
    if the slave device is local or remote.See Flag definition below.
*/
typedef struct
{
    mdm_prim_t          type;           /*!< Always MDM_SET_BREDR_SLAVE_ADDRESS_REQ */
    phandle_t           phandle;        /*!< Destination phandle */
    BD_ADDR_T           remote_bd_addr; /*!< Bluetooth address of remote device */
    BD_ADDR_T           new_bd_addr;    /*!< New Bluetooth address requested on slave device */
} MDM_SET_BREDR_SLAVE_ADDRESS_REQ_T;

/* Flags identifying local or remote device for
 * address change
 */
#define LOCAL_ADDRESS_CHANGED   0x00
#define REMOTE_ADDRESS_CHANGED  0x01

/*! \brief Confirmation of set BREDR slave address request

    This confirmation provides status of execution of
    MDM_SET_BREDR_SLAVE_ADDRESS_REQ. If status is success then confirmation
    also provides new address along with the old address of the slave device.
    Flags field identifies if the slave device is local or remote.
    See Flag definition above.

    Note:
    Flag definition can be ignored for failure status.
 */
typedef struct
{
    mdm_prim_t          type;           /*!< Always MDM_SET_BREDR_SLAVE_ADDRESS_CFM */
    phandle_t           phandle;        /*!< destination phandle */
    hci_return_t        status;         /*!< Anything other than HCI_SUCCESS is a failure */
    uint16_t            flags;          /*!< See flag definition above */
    BD_ADDR_T           old_bd_addr;    /*!< Old bluetooth address of slave device */
    BD_ADDR_T           new_bd_addr;    /*!< New bluetooth address of slave device
                                             if status is HCI_SUCCESS*/
} MDM_SET_BREDR_SLAVE_ADDRESS_CFM_T;

/*! \brief Indication of set BREDR slave address request on slave device

    This indication is sent when bluetooth address of slave device is
    changed asychronously, because of remote device initiated procedure.

    Note:
    Flag definition can be ignored for failure status.
 */
typedef struct
{
    mdm_prim_t          type;           /* Always  MDM_SET_BREDR_SLAVE_ADDRESS_IND */
    phandle_t           phandle;        /* destination phandle */
    uint16_t            flags;          /* See flag definition above */
    BD_ADDR_T           old_bd_addr;    /*!< Old bluetooth address of slave device */
    BD_ADDR_T           new_bd_addr;    /*!< New bluetooth address of slave device
                                             if status is HCI_SUCCESS*/
} MDM_SET_BREDR_SLAVE_ADDRESS_IND_T;

/*! \brief Setup a mirror ACL or eSCO link on a primary device

    Primary device uses this prim to establish a mirror ACL or eSCO link 
    between the secondary and the remote device based on "link_type" field.

    The mirror ACL allows the secondary device to synchronize to the link 
    between the primary and the remote device. It allows handing over of 
    primary role between primary and secondary device. 

    The mirror eSCO allows secondary device to sniff eSCO data sent from remote
    device to the primary device.

    Mirror ACL link creation is indicated using MDM_ACL_LINK_CREATE_CFM
    and mirror eSCO link creation is indicated using 
    MDM_ESCO_LINK_CREATE_CFM. When mirror link is disconnected 
    it will be indicated using MDM_LINK_DISCONNECT_CFM_T/IND_T.
   
    Note:
    a. The primary and secondary should be connected on an ACL link before the
    creation of mirror link.
    b. The mirror ACL link creation needs to be done before the creation of 
    mirror eSCO link.
*/
typedef struct
{
    mdm_prim_t          type;              /*!< Always MDM_LINK_CREATE_REQ */
    phandle_t           phandle;           /*!< Destination phandle */
    TP_BD_ADDR_T        mirror_bd_addr;    /*!< Bluetooth address of remote device which is being mirrored */
    TP_BD_ADDR_T        secondary_bd_addr; /*!< Bluetooth address of secondary device */
    link_type_t         link_type;         /*!< Type of mirror link, either ACL or eSCO */
} MDM_LINK_CREATE_REQ_T;

/*! \brief Confirmation of mirror ACL link creation

    This confirmation provides status for mirror ACL link creation request 
   (MDM_LINK_CREATE_REQ) on a primary device.

    The status value anything other than HCI_SUCCESS indicates failure of 
    mirror ACL link creation procedure. When the mirror ACL link creation is 
    successful, the secondary device will be mirroring the ACL link between the
    primary and remote device.

    The "connection_handle" uniquely identifies different mirroring links. Its 
    used as an identifier to subsequently perform other operations related to 
    mirroring.
*/
typedef struct
{
    mdm_prim_t              type;              /*!< Always MDM_ACL_LINK_CREATE_CFM */
    phandle_t               phandle;           /*!< Destination phandle */
    mdm_return_t            status;            /*!< Anything other than HCI_SUCCESS is a failure */
    hci_connection_handle_t connection_handle; /*!< Connection handle for the mirror ACL */
    TP_BD_ADDR_T            buddy_bd_addr;     /*!< Secondary device's address */
    TP_BD_ADDR_T            mirror_bd_addr;    /*!< Bluetooth address of Peer device which is being mirrored */
    mirror_role_t           role;              /*!< Role of the local device for the mirror link, primary. */
} MDM_ACL_LINK_CREATE_CFM_T;

/*! \brief Indication of mirror ACL link creation

    This event indicates mirror ACL link creation on secondary device.
    The mirror ACL link is created when primary device initiates the procedure.
    When the mirror ACL link creation is successful, the secondary device will 
    be mirroring the ACL link between the primary and remote device.

    The "connection_handle" uniquely identifies different mirroring links. It 
    is used as an identifier to subsequently perform other operations related 
    to mirroring.

    Note:
    No data is expected to be received on this mirror ACL link.
*/
typedef struct
{
    mdm_prim_t              type;              /*!< Always MDM_ACL_LINK_CREATE_IND */
    phandle_t               phandle;           /*!< Destination phandle */
    mdm_return_t            status;            /*!< Anything other than HCI_SUCCESS is a failure */
    hci_connection_handle_t connection_handle; /*!< Connection handle for the mirror ACL */
    TP_BD_ADDR_T            buddy_bd_addr;     /*!< Primary device's address */
    TP_BD_ADDR_T            mirror_bd_addr;    /*!< Bluetooth address of Peer device which is being mirrored */
    mirror_role_t           role;              /*!< Role of the local device for the mirror link, secondary */
} MDM_ACL_LINK_CREATE_IND_T;

/*! \brief Disconnect mirror ACL or eSCO link

    This prim is used to disconnect a mirror ACL or eSCO link between
    the secondary and remote device. Upon disconnection of mirror links 
    MDM_LINK_DISCONNECT_CFM is sent to the application.

    Note:
    The eSCO mirror link needs to be disconnected before disconnecting the
    ACL mirror link.
*/
typedef struct
{
    mdm_prim_t              type;        /*!< Always MDM_LINK_DISCONNECT_REQ */
    phandle_t               phandle;     /*!< Destination phandle */
    hci_connection_handle_t conn_handle; /*!< Connection handle of a eSCO/ACL mirror link */
    hci_reason_t            reason;      /*!< Reason to be reported in MDM_LINK_DISCONNECT_IND */
} MDM_LINK_DISCONNECT_REQ_T;

/*! \brief Confirmation of mirror ACL or eSCO link disconnection

    This confirmation provides status of mirror link disconnection request from
    the application. HCI_SUCCESS status indicates that disconnection of the 
    link is successful.
*/
typedef struct
{
    mdm_prim_t              type;        /*!< Always MDM_LINK_DISCONNECT_CFM */
    phandle_t               phandle;     /*!< Destination phandle */
    hci_connection_handle_t conn_handle; /*!< Connection handle of a eSCO/ACL mirror link */
    mdm_return_t            status;      /*!< Disconnection status, success or failure */
    link_type_t             link_type;   /*!< Type of mirror link, eSCO/ACL */
    mirror_role_t           role;        /*!< Role of the local device for the mirror link, Primary/Secondary */
} MDM_LINK_DISCONNECT_CFM_T;

/*! \brief Indication of mirror ACL or eSCO link disconnection

    This indication is sent to the application when a mirror ACL/eSCO 
    link is disconnected.

    The disconnect indication can be received if 
    a. Remote primary/secondary device has disconnected the mirror link.
    b. ACL or eSCO between the primary and the phone is disconnected.
    c. ACL between the primary and secondary is disconnected.
*/
typedef struct
{
    mdm_prim_t              type;        /*!< Always MDM_LINK_DISCONNECT_IND */
    phandle_t               phandle;     /*!< Destination phandle */
    hci_connection_handle_t conn_handle; /*!< Connection handle of a eSCO/ACL mirror link */
    mdm_reason_t            reason;      /*!< Disconnection reason received from the remote device */
    link_type_t             link_type;   /*!< Type of mirror link, eSCO/ACL */
    mirror_role_t           role;        /*!< Role of the local device for the mirror link, Primary/Secondary */
} MDM_LINK_DISCONNECT_IND_T;

/*! \brief Confirmation of mirror eSCO link creation

    This confirmation provides status for mirror eSCO link creation request
    (MDM_LINK_CREATE_REQ) on a primary device.

    The status value anything other than HCI_SUCCESS indicates failure of 
    mirror eSCO link creation procedure. When the mirror eSCO link creation is 
    successful, the secondary device will be mirroring the eSCO link between the
    primary and remote device.

    The "connection_handle" uniquely identifies different mirroring links. Its 
    used as an identifier to subsequently perform other operations related to 
    mirroring.

    Note:
    a. ESCO parameters for the mirror eSCO link are same as eSCO link with     
       remote device.
    b. When primary role is handed over to secondary device, 
       the original (bidirectional) eSCO link to the remote device becomes a 
       receive-only mirrored eSCO link. The connection_handle remains valid 
       throughout.
*/
typedef struct
{
    mdm_prim_t              type;              /*!< Always MDM_ESCO_LINK_CREATE_CFM */
    phandle_t               phandle;           /*!< Destination phandle */
    mdm_return_t            status;            /*!< Anything other than HCI_SUCCESS is a failure */
    hci_connection_handle_t connection_handle; /*!< Connection handle for the mirror eSCO link */
    TP_BD_ADDR_T            buddy_bd_addr;     /*!< Secondary device's address */
    TP_BD_ADDR_T            mirror_bd_addr;    /*!< Bluetooth address of Peer device which is being mirrored */
    mirror_role_t           role;              /*!< Role of the local device for the mirror link, primary. */  
} MDM_ESCO_LINK_CREATE_CFM_T;

/* \brief Indication of mirror eSCO link creation

    This event indicates mirror eSCO link creation on secondary device. Mirror 
    eSCO link is created when primary device initiates the procedure.

    The mirror eSCO allows secondary device to sniff eSCO data sent from remote
    device to the primary device.

    The "connection_handle" uniquely identifies different mirroring links. Its 
    used as an identifier to subsequently perform other operations related to 
    mirroring.
    
    Note: 
    a. When secondary device becomes primary device after handover procedure,
       receive-only mirrored eSCO link becomes the bidirectional eSCO link to 
       the remote device. The connection_handle remains valid throughout.
*/
typedef struct
{
    mdm_prim_t              type;              /*!< Always MDM_ESCO_LINK_CREATE_IND */
    phandle_t               phandle;           /*!< Destination phandle */
    mdm_return_t            status;            /*!< Anything other than HCI_SUCCESS is a failure */
    hci_connection_handle_t connection_handle; /*!< Connection handle for the mirror eSCO link */
    TP_BD_ADDR_T            buddy_bd_addr;     /*!< Secondary device's address */
    TP_BD_ADDR_T            mirror_bd_addr;    /*!< Bluetooth address of remote device which is being mirrored */
    mirror_role_t           role;              /*!< Role of the local device for the mirror link, secondary. */     
    link_type_t             link_type;         /*!< Type of mirror link, eSCO */
    uint8_t                 tx_interval;       /*!< transmission interval in slots */
    uint8_t                 wesco;             /*!< retransmission window in slots */
    uint16_t                rx_packet_length;  /*!< Rx payload length in bytes */
    uint16_t                tx_packet_length;  /*!< Tx payload length in bytes */
    uint8_t                 air_mode;          /*!< Coding format of eSCO packets */
} MDM_ESCO_LINK_CREATE_IND_T;

/* \brief Indication of new parameters for mirror eSCO link

    This event indicates new negotiated parameters of the mirror eSCO link on 
    secondary device, this event is generated when primary and remote devices 
    successfully renegotiates new eSCO parameters for the link. 

    The "connection_handle" uniquely identifies different mirroring links. Its 
    used as an identifier to subsequently perform other operations related to 
    mirroring.
*/
typedef struct
{
    mdm_prim_t              type;              /*!< Always MDM_ESCO_RENEGOTIATED_IND */
    phandle_t               phandle;           /*!< Destination phandle */
    hci_connection_handle_t connection_handle; /*!< Connection handle for the mirror eSCO link */
    uint8_t                 tx_interval;       /*!< transmission interval in slots */
    uint8_t                 wesco;             /*!< retransmission window in slots */
    uint16_t                rx_packet_length;  /*!< Rx payload length in bytes */
    uint16_t                tx_packet_length;  /*!< Tx payload length in bytes */
} MDM_ESCO_RENEGOTIATED_IND_T;

/* Initiate mirroring of an L2CAP channel (Basic mode) on Primary device. 
 *
 * L2CAP mirroring creates L2CAP instance on Primary and Secondary device 
 * to mirror a particular L2CAP channel between Primary and Remote device.
 * Status of L2CAP mirroring is indicated using MDM_L2CAP_CREATE_CFM
 *  
 * When reliable ACL sniffing is enabled, the mirror L2CAP processes L2CAP
 * packets sent by the remote device to the primary device on the mirrored 
 * L2CAP channel. Processed L2CAP packets are sent to the associated mirror 
 * L2CAP stream. The application can access the mirrored L2CAP packets via
 * the mirror L2CAP stream.
 * 
 * L2CAP mirroring is completely driven by Primary device just like ACL and eSCO
 * mirroring. 
 *
 * Note: 
 *   1. L2CAP mirroring doesn't guarantee reliable L2CAP data sniffing, 
 *      Secondary device can miss one or more L2CAP packets sent by remote 
 *      device to Primary device.
 *   2. This primitive shall be used only for L2CAP basic mode, i.e mode shall 
 *      be set to L2CA_FLOW_MODE_BASIC. Streaming mode and ERTM are not 
 *      supported.
 */
typedef struct
{
    mdm_prim_t               type;              /*!< Always MDM_L2CAP_CREATE_REQ */
    phandle_t                phandle;           /*!< Destination phandle */ 
    hci_connection_handle_t  connection_handle; /*!< Connection handle of the mirror ACL */
    l2ca_cid_t               cid;               /*!< CID of L2CAP channel to be mirrored */
    uint16_t                 flags;             /*!< Reserved for future use, shall be set to 0 */
} MDM_L2CAP_CREATE_REQ_T;


/* Indication of request to create Mirror L2CAP channel on Secondary device.
 * 
 * This event indicates that Primary device has initiated Mirroring of an L2CAP 
 * channel between Primary and Remote device. Application shall respond to this
 * event with MDM_L2CAP_CREATE_RSP.
 * 
 */
typedef struct
{
    mdm_prim_t               type;              /*!< Always MDM_L2CAP_CREATE_IND */
    phandle_t                phandle;           /*!< Destination phandle */ 
    hci_connection_handle_t  connection_handle; /*!< Connection handle of the mirror ACL */
    l2ca_cid_t               cid;               /*!< CID of L2CAP channel to be mirrored */
    uint16_t                 flags;                  /*!< Reserved for future use, shall be set to 0 */
} MDM_L2CAP_CREATE_IND_T;


/* Response to create Mirror L2CAP channel on Secondary device. 
 * 
 * When L2CAP mirroring is accepted, L2CAP instance will be created on Secondary
 * device to mirror L2CAP channel between Primary and Remote device. Application
 * shall provide configuration of the mirror L2CAP channel using conftab, usage 
 * of conftab is same as documented in L2CAP auto (i.e L2CA_AUTO_TP_CONNECT_RSP/
 * L2CA_AUTO_CONNECT_RSP). For L2CAP basic mode only L2CA_AUTOPT_MTU_IN is the 
 * valid configuration. Result of Mirror L2CAP channel creation is indicated 
 * in MDM_L2CAP_CREATE_CFM.
 * 
 * Note: L2CAP mirroring is supported only for Basic L2CAP channels. 
 */
typedef struct
{
    mdm_prim_t               type;              /*!< Always MDM_L2CAP_CREATE_RSP */
    phandle_t                phandle;           /*!< Destination phandle */ 
    hci_connection_handle_t  connection_handle; /*!< Connection handle of the mirror ACL */
    l2ca_conn_result_t       response;          /*!< Result code - uses L2CA_CONNECT range */
    l2ca_cid_t               cid;               /*!< CID of L2CAP channel to be mirrored */
    uint16_t                 conftab_length;    /*!< Number of uint16_t's in the 'conftab' table */
    uint16_t                *conftab;           /*!< Configuration table (key,value pairs) */
} MDM_L2CAP_CREATE_RSP_T;


/* Confirmation for Mirror L2CAP Create Request.
 *
 * This event is sent on both Primary and Secondary device to confirm mirror
 * L2CAP connection creation status, on success a receive only L2CAP stream is 
 * created on Secondary device.
 */
typedef struct
{
    mdm_prim_t               type;              /*!< Always MDM_L2CAP_CREATE_CFM */
    phandle_t                phandle;           /*!< Destination phandle */
    hci_connection_handle_t  connection_handle; /*!< Connection handle of the mirror ACL */
    l2ca_cid_t               cid;               /*!< CID of L2CAP channel being mirrored */
    uint16_t                 flags;             /*!< Reserved for future use, shall be set to 0 */
    l2ca_conn_result_t       result;            /*!< Result code - uses L2CA_CONNECT range */
} MDM_L2CAP_CREATE_CFM_T;

/*
 * Disconnect Mirror L2CAP connection on Primary device. 
 * 
 * Mirroring of L2CAP channel on Primary and Secondary device is aborted when
 * mirror ACL of the L2CAP channel is disconnected.
 * Returns MDM_L2CAP_DISCONNECT_CFM when disconnection 
 * is complete. 
 */ 
typedef struct
{
    mdm_prim_t          type;              /*!< Always MDM_L2CAP_DISCONNECT_REQ */
    phandle_t           phandle;           /*!< Destination phandle */
    l2ca_cid_t          cid;               /*!< cid of L2CAP channel being mirrored */
} MDM_L2CAP_DISCONNECT_REQ_T;

/* 
 * Indication of Mirror L2CAP disconnection. 
 * 
 * Mirroring of L2CAP channel on Primary and Secondary device is aborted when 
 * mirror ACL of the L2CAP channel is disconnected. Application shall respond 
 * to disconnect indication with disconnection response. 
 *
 * L2CAP stream associated with this CID on Secondary is destroyed after the
 * application responds to disconnection. 
 */
typedef struct
{
    mdm_prim_t          type;              /*!< Always MDM_L2CAP_DISCONNECT_IND */
    phandle_t           phandle;           /*!< Destination phandle */
    l2ca_cid_t          cid;               /*!< cid of L2CAP channel being mirrored */
    l2ca_disc_result_t  reason;            /*!< Reason code - uses L2CA_DISCONNECT range */ 
} MDM_L2CAP_DISCONNECT_IND_T;

/*
 * Response to Mirror L2CAP disconnection indication event
 */
typedef struct
{
    mdm_prim_t          type;             /*!< Always MDM_L2CAP_DISCONNECT_RSP */
    l2ca_cid_t          cid;              /*!< Local channel ID */
} MDM_L2CAP_DISCONNECT_RSP_T;

/* 
 *  Confirmation of Mirror L2CAP disconnection request.
 *
 *  HCI_SUCCESS status indicates that disconnection of mirror L2CAP is 
 *  successful.
 *
 */
typedef struct
{
    mdm_prim_t          type;              /*!< Always MDM_L2CAP_DISCONNECT_CFM */
    phandle_t           phandle;           /*!< Destination phandle */
    l2ca_cid_t          cid;               /*!< cid of L2CAP channel being mirrored */
    mdm_return_t        status;            /*!< Success or failure to disconnect */
} MDM_L2CAP_DISCONNECT_CFM_T;

/*! \brief Indication of data synchronisation on a mirror L2CAP link.

   This event indicates both primary and secondary devices have synchronised
   data receive on a mirror L2CAP link. This event is generated on both
   primary and secondary devices, and indicates the time of arrival of first
   mirrored data packet on an L2CAP CID at the primary device. The event will
   also be generated after each resync between primary and secondary devices.
*/
typedef struct
{
    mdm_prim_t          type;                   /*!< Always MDM_L2CAP_DATA_SYNC_IND */
    phandle_t           phandle;                /*!< Destination phandle */
    hci_connection_handle_t connection_handle;  /*!< Connection handle of the mirror ACL */
    l2ca_cid_t          cid;                    /*!< Cid of L2CAP channel being mirrored */
    uint32_t            clock;                  /*!< Clock synchronisation time instant */
} MDM_L2CAP_DATA_SYNC_IND_T;

/*! \brief Indication of mirror mode change event on secondary.

   This event on secondary indicates a mode change between primary and
   mirrored device
*/

typedef struct
{
    mdm_prim_t          type;              /*!< Always MDM_MIRROR_MODE_CHANGE_EVENT_IND */
    phandle_t           phandle;           /*!< Destination phandle */
    mdm_return_t        status;            /*!< Anything other than MDM_RESULT_SUCCESS is a failure */
    BD_ADDR_T           mirror_bd_addr;    /*!< Bluetooth address of Peer device which is being mirrored */
    uint8_t             mode;               /*!< Current mode */
    uint16_t            length;             /*!< Indicates the sniff interval when mode is sniff */

} MDM_MIRROR_MODE_CHANGE_EVENT_IND_T;

/*! \brief Create delegated or mirror CIS link

    This prim is used by the primary earbud to synchronize a CIS on the 
    secondary earbud from a peer device. The primary earbud can delegate the 
    ownership of a CIS or request the secondary earbud to start mirroring a 
    CIS using this command.

    Primary device shall receive MDM_LE_CIS_CREATE_CFM once the CIS is 
    delegated or mirrored successfully or not.

    Secondary device shall receive MDM_LE_CIS_CREATE_IND on successful 
    creation of CIS on secondary side.
*/
typedef struct
{
    mdm_prim_t          type;              /*!< Always MDM_LE_CIS_CREATE_REQ */
    phandle_t           phandle;           /*!< Destination phandle */
    TP_BD_ADDR_T        secondary_bd_addr; /*!< Bluetooth address of secondary device */
    hci_connection_handle_t cis_handle;    /*!< cis connection that primary needs to synchronize
                                                the secondary with */
    cis_ownership_t     cis_ownership;     /*!< Indicates whether the ownership of a CIS needs to 
                                                be delegated to the secondary or whether the CIS
                                                needs to be mirrored by the secondary */
} MDM_LE_CIS_CREATE_REQ_T;

/*  
    This structure corresponds to DM_CIS_PARAM_T in dm_prim
*/
typedef struct mdm_cis_params
{
    uint24_t cig_sync_delay;
    uint24_t cis_sync_delay;
    uint24_t transport_latency_c_to_p;
    uint24_t transport_latency_p_to_c;
    uint16_t max_pdu_c_to_p;
    uint16_t max_pdu_p_to_c;
    uint16_t iso_interval;
    uint8_t  phy_c_to_p;
    uint8_t  phy_p_to_c;
    uint8_t  nse;
    uint8_t  bn_c_to_p;
    uint8_t  bn_p_to_c;
    uint8_t  ft_c_to_p;
    uint8_t  ft_p_to_c;
} MDM_CIS_PARAM_T;

/*! \brief Confirm that a CIS mirror or delegated connection has been established

    This confirmation provides status for mirror or delegated CIS link creation 
    request (MDM_LE_CIS_CREATE_REQ) on a primary device.
*/
typedef struct 
{
    mdm_prim_t          type;              /*!< Always MDM_LE_CIS_CREATE_CFM */
    phandle_t           phandle;           /*!< Destination phandle */
    mdm_return_t        status;            /*!< status */
    hci_connection_handle_t cis_handle;    /*!< CIS handle for cis establishment On the Primary
                                                (Connected Peripheral), this corresponds to the 
                                                CIS that  is being delegated or mirrored i.e.
                                                   the CIS to the Phone (Connected Central). */
    TP_BD_ADDR_T        buddy_bd_addr;     /*!< Secondary device's address */
    TP_BD_ADDR_T        mirror_bd_addr;    /*!< Bluetooth address of remote device which is being mirrored */
    cis_ownership_t     cis_ownership;     /*!< Indicates whether the ownership of a CIS needs to 
                                                be delegated to the secondary or whether the CIS
                                                needs to be mirrored by the secondary */
    MDM_CIS_PARAM_T     cis_params;        /*!< CIS parameters agreed during cis establishment */
    uint16_t            supervision_timeout; /*!< Connection supervision timeout. */
    mirror_role_t       role;              /*!< Buddy role */
} MDM_LE_CIS_CREATE_CFM_T;

/*! \brief Indication of creation of delegation or mirroring of CIS link.

    This event indicates CIS link has been delegated or mirror CIS link created 
    on secondary device.
*/
typedef struct 
{
    mdm_prim_t          type;              /*!< Always MDM_LE_CIS_CREATE_IND */
    phandle_t           phandle;           /*!< Destination phandle */
    mdm_return_t        status;            /*!< status */
    hci_connection_handle_t cis_handle;    /*!< CIS handle for cis establishment. On the Secondary
                                                (Shadowing Peripheral), this corresponds to the 
                                                delegated or mirrored CIS itself */
    TP_BD_ADDR_T        buddy_bd_addr;     /*!< Secondary device's address */
    TP_BD_ADDR_T        mirror_bd_addr;    /*!< Bluetooth address of remote device which is being mirrored */
    cis_ownership_t     cis_ownership;     /*!< Indicates whether the ownership of a CIS needs to 
                                                be delegated to the secondary or whether the CIS
                                                needs to be mirrored by the secondary */
    MDM_CIS_PARAM_T     cis_params;        /*!< CIS parameters agreed during cis establishment */
    uint16_t            supervision_timeout; /*!< Connection supervision timeout. */
    mirror_role_t       role;              /*!< Buddy role */
} MDM_LE_CIS_CREATE_IND_T;

/*! \brief Disconnect mirror CIS link

    This prim is used to disconnect a mirror CIS link between
    the secondary and remote device. Upon disconnection of CIS mirror link 
    MDM_LINK_DISCONNECT_CFM is sent to the application.

*/
typedef struct
{
    mdm_prim_t              type;         /*!< Always MDM_LE_CIS_DISCONNECT_REQ */
    phandle_t               phandle;      /*!< Destination phandle */
    hci_connection_handle_t conn_handle;  /*!< Connection handle of a CIS mirror link */
    hci_reason_t            reason;       /*!< Reason to be reported in MDM_LE_CIS_DISCONNECT_IND */
} MDM_LE_CIS_DISCONNECT_REQ_T;

/*! \brief Confirmation of mirror CIS link disconnection

    This confirmation provides status of mirror CIS link disconnection request from
    the application. HCI_SUCCESS status indicates that disconnection of the 
    link is successful.
*/
typedef struct
{
    mdm_prim_t              type;         /*!< Always MDM_LE_CIS_DISCONNECT_CFM */
    phandle_t               phandle;      /*!< Destination phandle */
    hci_connection_handle_t conn_handle;  /*!< Connection handle of a CIS mirror link */
    mdm_return_t            status;       /*!< Disconnection status, success or failure */
    mirror_role_t           role;         /*!< Role of the local device for the mirror link, 
                                               Primary/Secondary */
} MDM_LE_CIS_DISCONNECT_CFM_T;

/*! \brief Indication of mirror CIS link disconnection

    This indication is sent to the application when a mirror CIS 
    link is disconnected.
*/
typedef struct
{
    mdm_prim_t              type;        /*!< Always MDM_LE_CIS_DISCONNECT_IND */
    phandle_t               phandle;     /*!< Destination phandle */
    hci_connection_handle_t conn_handle; /*!< Connection handle of a CIS mirror link */
    mdm_reason_t            reason;      /*!< Disconnection reason received from the remote device */
    mirror_role_t           role;        /*!< Role of the local device for the mirror link,
                                              Primary/Secondary */
} MDM_LE_CIS_DISCONNECT_IND_T;

typedef struct
{
    mdm_prim_t          type;           /*!< Always MDM_DEBUG_IND */
    uint16_t            size_debug;
    uint8_t             *debug;
} MDM_DEBUG_IND_T;


/*! \brief Union of the primitives */
typedef union
{
    /* Shared */
    mdm_prim_t                        type;

    /* Downstream */
    MDM_REGISTER_REQ_T                mdm_register_req;
    MDM_SET_BREDR_SLAVE_ADDRESS_REQ_T mdm_set_bredr_slave_address_req;
    MDM_LINK_CREATE_REQ_T             mdm_link_create_req;
    MDM_LINK_DISCONNECT_REQ_T         mdm_link_disconnect_req;
    MDM_L2CAP_CREATE_REQ_T            mdm_l2cap_create_req;
    MDM_L2CAP_CREATE_RSP_T            mdm_l2cap_create_rsp;
    MDM_L2CAP_DISCONNECT_REQ_T        mdm_l2cap_disconnect_req;
    MDM_L2CAP_DISCONNECT_RSP_T        mdm_l2cap_disconnect_rsp;
    MDM_LE_CIS_DISCONNECT_REQ_T       mdm_le_cis_disconnect_req;
    MDM_LE_CIS_CREATE_REQ_T           mdm_le_cis_create_req;


    /* Upstream */
    MDM_REGISTER_CFM_T                mdm_register_cfm;
    MDM_SET_BREDR_SLAVE_ADDRESS_CFM_T mdm_set_bredr_slave_address_cfm;
    MDM_SET_BREDR_SLAVE_ADDRESS_IND_T mdm_set_bredr_slave_address_ind;
    MDM_ACL_LINK_CREATE_CFM_T         mdm_acl_link_create_cfm;
    MDM_ACL_LINK_CREATE_IND_T         mdm_acl_link_create_ind;
    MDM_LINK_DISCONNECT_CFM_T         mdm_link_disconnect_cfm;
    MDM_LINK_DISCONNECT_IND_T         mdm_link_disconnect_ind;
    MDM_ESCO_LINK_CREATE_CFM_T        mdm_esco_link_create_cfm;
    MDM_ESCO_LINK_CREATE_IND_T        mdm_esco_link_create_ind;
    MDM_ESCO_RENEGOTIATED_IND_T       mdm_esco_renegotiated_ind;
    MDM_L2CAP_CREATE_IND_T            mdm_l2cap_create_ind;
    MDM_L2CAP_CREATE_CFM_T            mdm_l2cap_create_cfm;
    MDM_L2CAP_DISCONNECT_IND_T        mdm_l2cap_disconnect_ind;
    MDM_L2CAP_DISCONNECT_CFM_T        mdm_l2cap_disconnect_cfm;
    MDM_MIRROR_MODE_CHANGE_EVENT_IND_T   mdm_mirror_mode_change_event_ind;
    MDM_LE_CIS_DISCONNECT_CFM_T       mdm_le_cis_disconnect_cfm;
    MDM_LE_CIS_DISCONNECT_IND_T       mdm_le_cis_disconnect_ind;
    MDM_LE_CIS_CREATE_CFM_T           mdm_le_cis_create_cfm;
    MDM_LE_CIS_CREATE_IND_T           mdm_le_cis_create_ind;

    MDM_DEBUG_IND_T                   mdm_debug_ind;

} MDM_UPRIM_T;

#ifdef __cplusplus
}
#endif

#endif /* _MDM_PRIM_H_ */

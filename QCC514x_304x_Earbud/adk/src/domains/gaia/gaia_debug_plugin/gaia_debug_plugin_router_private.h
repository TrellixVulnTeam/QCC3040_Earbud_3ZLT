/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       gaia_debug_plugin_router_private.h
\brief      Private header file for the router that manages PyDbg Remote Debug commands addressed to the Secondary device.
            This header file defines the interface used by the lower layer, Pydbg Remote Debug 'Debug-Type' command handler.
*/

#ifndef GAIA_DEBUG_PLUGIN_ROUTER_PRIVATE_H_
#define GAIA_DEBUG_PLUGIN_ROUTER_PRIVATE_H_

#include <gaia.h>
#include "remote_debug_prim.h"


/*! \brief The header size of Gaia 'Debug_Tunnel_To_Chip' command/response. */
#define GAIA_DEBUG_TUNNEL_TO_CHIP_CMD_RSP_PARAMETER_HEADER_SIZE     (2)

/*! \brief The size of 'IP Protocol' Type field in the Pydbg Remote Debug header. */
#define PYDBG_REMOTE_DEBUG_IP_PROTOCOL_TYPE_FIELD_SIZE              (1)

/*! \brief The header size of the PyDbg Remote Debug PDU.
           (This size does not include any Gaia headers.) */
#define PYDBG_REMOTE_DEBUG_PDU_DEBUG_TYPE_HEADER_SIZE               (PYDBG_REMOTE_DEBUG_IP_PROTOCOL_TYPE_FIELD_SIZE + REMOTE_DEBUG_DEBUG_CMD_PAYLOAD_PAYLOAD_BYTE_OFFSET)

/*! \brief The header size of the PyDbg Remote Debug Routed-Type PDU.
           (This size does not include any Gaia headers.) */
#define PYDBG_REMOTE_DEBUG_PDU_ROUTED_TYPE_HEADER_SIZE              (PYDBG_REMOTE_DEBUG_IP_PROTOCOL_TYPE_FIELD_SIZE + REMOTE_DEBUG_ROUTED_CMD_PAYLOAD_PAYLOAD_BYTE_OFFSET)

/*! \brief Pydbg Remote Debug Max PDU size.
           Note that this value is referred almost every Pydbg commands (i.e.
           'MemoryReadReq'). So, this code does not rely on the official way
           to inquire the transport's optimum Tx/Rx packet sizes given by the
           funcitons below for the performance:
            - Gaia_TransportGetInfo(t, GAIA_TRANSPORT_OPTIMUM_TX_PACKET, &value)
            - Gaia_TransportGetInfo(t, GAIA_TRANSPORT_OPTIMUM_RX_PACKET, &value)

           This is based on the the calculation below:
                GAIA_TRANSPORT_RFCOMM_V3_MAX_TX_PKT_SIZE                  254 bytes 
                Gaia header size(SOF~LEN,VendorID,CommandID):               8 bytes
                Tailing CS:                                                 1 bytes
                'DebugTunnelToChip' Command header ('Client ID', 'Tag'):    2 bytes */
#define PYDBG_REMOTE_DEBUG_MAX_PDU_SIZE                             (254 - 8 - 1 - GAIA_DEBUG_TUNNEL_TO_CHIP_CMD_RSP_PARAMETER_HEADER_SIZE)

/*! \brief Pydbg Remote Debug (IP Protocol 'Debug' Type) payload size. */
#define PYDBG_REMOTE_DEBUG_PAYLOAD_SIZE_DEBUG_TYPE                  (PYDBG_REMOTE_DEBUG_MAX_PDU_SIZE - PYDBG_REMOTE_DEBUG_PDU_DEBUG_TYPE_HEADER_SIZE)

/*! \brief Pydbg Remote Debug (IP Protocol 'Routed' Type) payload size. */
#define PYDBG_REMOTE_DEBUG_PAYLOAD_SIZE_ROUTED_TYPE                 (PYDBG_REMOTE_DEBUG_MAX_PDU_SIZE - PYDBG_REMOTE_DEBUG_PDU_ROUTED_TYPE_HEADER_SIZE)


/*! \brief Formatted array data debug print macro. */
#define GAIA_DEBUG_DEBUG_LOG_FORMATTED_ARRAY(size, array, type_str) \
    { \
        uint16 macro_i; \
        uint16 macro_offset = 0; \
        uint16 macro_remaining = size; \
      \
        if (8 <= size) \
        { \
            for (macro_i = 0; macro_i < (size / 8); macro_i++) \
            { \
                macro_offset = (macro_i * 8); \
                DEBUG_LOG_##type_str(" +%03d:  %02X %02X %02X %02X  %02X %02X %02X %02X", macro_offset, \
                                     array[macro_offset + 0], array[macro_offset + 1], array[macro_offset + 2], array[macro_offset + 3], \
                                     array[macro_offset + 4], array[macro_offset + 5], array[macro_offset + 6], array[macro_offset + 7]); \
            } \
            macro_remaining -= (macro_i * 8); \
            macro_offset     = (macro_i * 8); \
        } \
     \
        switch (macro_remaining) \
        { \
            case 1: \
                DEBUG_LOG_##type_str(" +%03d:  %02X", macro_offset, array[macro_offset + 0]); \
                break; \
            case 2: \
                DEBUG_LOG_##type_str(" +%03d:  %02X %02X", macro_offset, array[macro_offset + 0], array[macro_offset + 1]); \
                break; \
            case 3: \
                DEBUG_LOG_##type_str(" +%03d:  %02X %02X %02X", macro_offset, \
                                     array[macro_offset + 0], array[macro_offset + 1], array[macro_offset + 2]); \
                break; \
            case 4: \
                DEBUG_LOG_##type_str(" +%03d:  %02X %02X %02X %02X", macro_offset, \
                                     array[macro_offset + 0], array[macro_offset + 1], array[macro_offset + 2], array[macro_offset + 3]); \
                break; \
            case 5: \
                DEBUG_LOG_##type_str(" +%03d:  %02X %02X %02X %02X  %02X", macro_offset, \
                                     array[macro_offset + 0], array[macro_offset + 1], array[macro_offset + 2], array[macro_offset + 3], array[macro_offset + 4]); \
                break; \
            case 6: \
                DEBUG_LOG_##type_str(" +%03d:  %02X %02X %02X %02X  %02X %02X", macro_offset, \
                                     array[macro_offset + 0], array[macro_offset + 1], array[macro_offset + 2], array[macro_offset + 3], array[macro_offset + 4], array[macro_offset + 5]); \
                break; \
            case 7: \
                DEBUG_LOG_##type_str(" +%03d:  %02X %02X %02X %02X  %02X %02X %02X", macro_offset, \
                                     array[macro_offset + 0], array[macro_offset + 1], array[macro_offset + 2], array[macro_offset + 3], array[macro_offset + 4], array[macro_offset + 5], array[macro_offset + 6]); \
                break; \
            default: \
                break; \
        } \
    }


/*! \brief GAIA Transport and Pydbg PDU header parameters which are required to send back responses. */
typedef struct
{
    GAIA_TRANSPORT                      *gaia_transport;    /*!< Pointer to GAIA transport instance. */

    uint8                               gaia_client_id;     /*!< An identifier assigned and used by the host (the mobile app). */
    uint8                               gaia_tag;           /*!< Another identifier used by the host (the mobile app). */

    REMOTE_DEBUG_CMD_TYPE               pdu_type;           /*!< Pydbg PDU Type. */
    REMOTE_DEBUG_CMD                    cmd_id;             /*!< Pydbg command ID. */
    uint16                              pydbg_seq_no;       /*!< Pydbg Tag (Sequence Number). */

    /* The following parameters are available only when 'pdu_type' == REMOTE_DEBUG_CMD_TYPE_ROUTED_CMD. */
    REMOTE_DEBUG_ROUTE_CMD              routed_cmd_id;      /*!< Pydbg (Routed Type): Routed Command ID. */
    REMOTE_DEBUG_ROUTED_REQ_ROUTE_T     routed_req_to;      /*!< Pydbg (Routed Type): Request routing (Request sent to). */
    REMOTE_DEBUG_ROUTED_RESP_ROUTE_T    routed_rsp_from;    /*!< Pydbg (Routed Type): Response routing (Response sent from). */
    REMOTE_DEBUG_CMD_TYPE               routed_pdu_type;    /*!< Pydbg (Routed Type): Actual PDU Type, routed by this PDU type. */

} pydbg_remote_debug_pdu_info_t;


/*! \brief Response PDU memory allocated from the heap. */
typedef struct
{
    /* Example of (Type = 1:IP Protocol 'Debug' Type):
     *      0        1        2        3        4        5        6        7        8       ...       N    (Byte)
     *  +--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+
     *  |ClientID|   Tag  | Type=1 | Cmd ID |  Payload Length |  Tag (Seq No.)  |     Payload (if any)     |
     *  +--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+
     *  |<-  Tunnelling ->|<-----   PyDbg Remote Debug Protocol Header    ----->|<---- PyDbg Payload  ---->|
     *  |<--------------------------- header_size ----------------------------->|<------- payload -------->|
     *  |                          |<-------------------------- cmd_rsp_message -------------------------->|
     *  |<----------------------------------------- PDU -------------------------------------------------->|
     * 
     *      - '*pdu'                Pointer to the allocated memory.
     *      - 'pdu_size':           = Header Size + Payload Size.
     *      - 'cmd_rsp_message':    The command/response message.
     *      - '*payload':           The command payload. This points to memory[8] in this example but it depends on the PDU Type.
     */
    uint8 *pdu;             /*!< The head of the allocated memory. 'free()' must be called with this. */
    uint16 pdu_size;        /*!< The size of the PDU. */
    uint8 *payload;         /*!< The start of the payload space. */
    uint8 *cmd_rsp_message; /*!< The start of the command/response message. */

} allocated_pydbg_rsp_pdu_t;


/*! \brief Type of this device (Headset or Earbud, and Left or Right if this is an Earbud). */
typedef enum
{
    gaia_debug_device_type_invalid = 0,
    gaia_debug_device_type_earbud_left_primary,
    gaia_debug_device_type_earbud_left_secondary,
    gaia_debug_device_type_earbud_right_primary,
    gaia_debug_device_type_earbud_right_secondary,
    gaia_debug_device_type_headset,

    gaia_debug_device_type_earbud_num_of_types
} gaia_debug_device_type_t;



/*! \brief Send a response to the mobile through the router, which manages
           forwarding the response back to the Primary device.

    \param result       Pointers to the memory allocated on success, otherwise they are NULL.

    \param pdu_type     The type of PDU that specifies the size of header space.

    \param payload_size The size of the payload requested.
*/
void GaiaDebugPlugin_PydbgRoutingMallocRspPDU(allocated_pydbg_rsp_pdu_t *result, const REMOTE_DEBUG_CMD_TYPE pdu_type, const uint16 payload_size);


/*! \brief Send a response to the mobile through the router, which manages
           forwarding the response back to the Primary device.

    \param rsp_pdu      Response PDU message.

    \param rsp_cmd_id   Response command ID.

    \param payload_size The size of the response payload.
*/
void GaiaDebugPlugin_PydbgRoutingSendResponse(allocated_pydbg_rsp_pdu_t *rsp_pdu, uint8 rsp_cmd_id, uint16 payload_size);


/*! \brief Send an error response to the mobile through the router, which
           manages forwarding the response back to the Primary device.

    \param status_code  Error status code.
*/
void GaiaDebugPlugin_PydbgRoutingSendError(uint8 status_code);


/*! \brief Return this device is the Primary/Secondary and the Left/Right.

    \return Type of this device (Primary/Secondary and Left/Right, or Headset).
*/
gaia_debug_device_type_t GaiaDebugPlugin_GetDeviceType(void);

#endif /* GAIA_DEBUG_PLUGIN_ROUTER_PRIVATE_H_ */
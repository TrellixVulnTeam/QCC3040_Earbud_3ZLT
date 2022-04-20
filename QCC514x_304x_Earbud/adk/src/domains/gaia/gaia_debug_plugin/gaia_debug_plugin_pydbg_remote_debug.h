/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       gaia_debug_plugin_pydbg_remote_debug.h
\brief      Header file for PyDbg remote debug, a part of Gaia Debug Feature.
*/

#ifndef GAIA_DEBUG_PLUGIN_PYDBG_REMOTE_DEBUG_H_
#define GAIA_DEBUG_PLUGIN_PYDBG_REMOTE_DEBUG_H_

#include <csrtypes.h>
#include <gaia.h>
#include "gaia_debug_plugin_router_private.h"
#include "remote_debug_prim.h"


/*! \brief The number of PyDbg Remote Debug packets that can be transferred over
           Gaia with a single 'DebugTunnelToChip' command/response payload.
           For example, if the maximum PDU size is set to 64 bytes and Gaia transport
           allows PyDbg to use 192 bytes of the command/response payload, this value
           can be set to 3 (= 192 / 64). */
#define PYDBG_REMOTE_DEBUG_NUMBER_OF_OUTSTANDING_PACKETS            (1)

/*! \brief The 'Capabilities' bitmap of the Protocol Version Response defines
           the capabilities supported by this device. Currently only the LSB
           is assigned. */
#define PYDBG_REMOTE_DEBUG_PROTOCOL_VERSION_SUPPORTED_CAPABILITIES_DEFAULT      (0x00000000UL)
#define PYDBG_REMOTE_DEBUG_PROTOCOL_VERSION_SUPPORTED_CAPABILITIES_ROUTING      (0x00000001UL)

/*! \brief The response header length of PyDbg remote Debug 'Memory_Read_Rsp'. */
#define PYDBG_REMOTE_DEBUG_MEMORY_READ_RSP_HEADER_SIZE              (REMOTE_DEBUG_MEMORY_READ_RSP_T_BYTE_SIZE - 1)

/*! \brief The command header length of PyDbg remote Debug 'Memory_Write_Req'. */
#define PYDBG_REMOTE_DEBUG_MEMORY_WRITE_REQ_CMD_HEADER_SIZE         (REMOTE_DEBUG_MEMORY_WRITE_REQ_T_BYTE_SIZE - 1)



/*! \brief 'Debug Tunnel To Chip' command parameter positions. */
typedef enum
{
    debug_plugin_debug_tunnel_to_chip_cmd_client_id             = 0,
    debug_plugin_debug_tunnel_to_chip_cmd_tag                   = 1,
    debug_plugin_debug_tunnel_to_chip_cmd_payload_0             = 2,

    number_of_debug_plugin_debug_tunnel_to_chip_cmd_bytes       = 2
} debug_plugin_debug_tunnel_to_chip_cmd_t;

/*! \brief 'Debug Tunnel To Chip' response parameter positions. */
typedef enum
{
    debug_plugin_debug_tunnel_to_chip_rsp_client_id             = 0,
    debug_plugin_debug_tunnel_to_chip_rsp_tag                   = 1,
    debug_plugin_debug_tunnel_to_chip_rsp_payload_0             = 2,

    number_of_debug_plugin_debug_tunnel_to_chip_rsp_bytes       = 2
} debug_plugin_debug_tunnel_to_chip_rsp_t;


/*! \brief Subsystem IDs. */
typedef enum
{
    subsystem_id_curator        = 0,
    subsystem_id_bt             = 2,
    subsystem_id_audio          = 3,
    subsystem_id_app            = 4,
} subsystem_id_t;

/*! \brief Subsystem Block IDs for Apps-SS. */
typedef enum
{
    apps_ss_block_id_apps_data_mem          = 0,
    apps_ss_block_id_apps_p0_data_mem       = 8,
    apps_ss_block_id_apps_p0_program_mem    = 9,
    apps_ss_block_id_apps_p1_data_mem       = 10,
    apps_ss_block_id_apps_p1_program_mem    = 11 
} apps_ss_block_id_t;

/*! Macro to check if a subsystem block ID (for Apps-SS) is valid one or not. */
#define IS_VALID_APPS_SS_BLOCK_ID(id) \
    (id == apps_ss_block_id_apps_data_mem || \
     id == apps_ss_block_id_apps_p0_data_mem || \
     id == apps_ss_block_id_apps_p0_program_mem || \
     id == apps_ss_block_id_apps_p1_data_mem || \
     id == apps_ss_block_id_apps_p1_program_mem \
    )

/*! Macro to check if a subsystem block ID (for Apps-SS) is Apps-P1 or not. */
#define IS_APPS_P1_BLOCK_ID(id) \
    (id == apps_ss_block_id_apps_p1_data_mem || \
     id == apps_ss_block_id_apps_p1_program_mem \
    )




/*! \brief Handle PyDbg 'Debug-Type' command and execute the request.

    \param pdu_type         The type of PDU that specifies the size of header space.

    \param cmd_id           Pydbg Remote Debug 'Debug-Type' command ID.

    \param payload_length   Payload size of the PyDbg command payload (in bytes).

    \param payload          PyDbg command payload data.
*/
void GaiaDebugPlugin_PyDbgDebugCommandHandler(const REMOTE_DEBUG_CMD_TYPE pdu_type, REMOTE_DEBUG_CMD cmd_id, uint16 payload_length, const uint8 *payload);

#endif /* GAIA_DEBUG_PLUGIN_PYDBG_REMOTE_DEBUG_H_ */

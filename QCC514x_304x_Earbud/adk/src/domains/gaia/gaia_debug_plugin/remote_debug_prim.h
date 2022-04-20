/*****************************************************************************

            Copyright (c) 2020 Qualcomm Technologies International, Ltd.

            WARNING: This is an auto-generated file!
                     DO NOT EDIT!

*****************************************************************************/
#ifndef REMOTE_DEBUG_PRIM_H__
#define REMOTE_DEBUG_PRIM_H__

/*
 * This file is tentatively placed in 'gaia/gaia_debug_plugin/' directory, but
 * this should be replaced by the file below:
 *    /os/src/common/interface/gen/k32/remote_debug_prim.h
 * 
 * Also note that the #include line below is manually removed.
 * References:
 *  - B-306011 Add a field to the remote debug response route type
 *  - B-304485 Implement wireless debug protocol for routing to secondary earbud
 *  - B-300097 Create xml and header files for pydbg remote debug protocol
 */
/* #include "hydra/hydra_types.h" */


/*******************************************************************************

  NAME
    Remote_Debug_Cmd

  DESCRIPTION

 VALUES
    protocol_version_req -
    protocol_version_rsp -
    chip_reset_req       -
    chip_reset_rsp       -
    max_pdu_size_req     -
    max_pdu_size_rsp     -
    memory_read_req      -
    memory_read_rsp      -
    memory_write_req     -
    memory_write_rsp     -
    appcmd_req           -
    appcmd_rsp           -

*******************************************************************************/
typedef enum
{
    REMOTE_DEBUG_CMD_PROTOCOL_VERSION_REQ = 0,
    REMOTE_DEBUG_CMD_PROTOCOL_VERSION_RSP = 1,
    REMOTE_DEBUG_CMD_MAX_PDU_SIZE_REQ = 2,
    REMOTE_DEBUG_CMD_MAX_PDU_SIZE_RSP = 3,
    REMOTE_DEBUG_CMD_MEMORY_READ_REQ = 4,
    REMOTE_DEBUG_CMD_MEMORY_READ_RSP = 5,
    REMOTE_DEBUG_CMD_MEMORY_WRITE_REQ = 6,
    REMOTE_DEBUG_CMD_MEMORY_WRITE_RSP = 7,
    REMOTE_DEBUG_CMD_APPCMD_REQ = 8,
    REMOTE_DEBUG_CMD_APPCMD_RSP = 9,
    REMOTE_DEBUG_CMD_CHIP_RESET_REQ = 10,
    REMOTE_DEBUG_CMD_CHIP_RESET_RSP = 11
} REMOTE_DEBUG_CMD;
/*******************************************************************************

  NAME
    Remote_Debug_Cmd_Type

  DESCRIPTION

 VALUES
    transport_cmd -
    debug_cmd     -
    routed_cmd    -

*******************************************************************************/
typedef enum
{
    REMOTE_DEBUG_CMD_TYPE_TRANSPORT_CMD = 0,
    REMOTE_DEBUG_CMD_TYPE_DEBUG_CMD = 1,
    REMOTE_DEBUG_CMD_TYPE_ROUTED_CMD = 2
} REMOTE_DEBUG_CMD_TYPE;
/*******************************************************************************

  NAME
    Remote_Debug_DEVICE_ID_T

  DESCRIPTION

 VALUES
    single_device -
    left          -
    right         -

*******************************************************************************/
typedef enum
{
    REMOTE_DEBUG_DEVICE_ID_T_SINGLE_DEVICE = 0,
    REMOTE_DEBUG_DEVICE_ID_T_LEFT = 1,
    REMOTE_DEBUG_DEVICE_ID_T_RIGHT = 2
} REMOTE_DEBUG_DEVICE_ID_T;
/*******************************************************************************

  NAME
    Remote_Debug_PROTOCOL_VERSION_T

  DESCRIPTION

 VALUES
    PROTOCOL_VERSION_PHASE_1 -
    PROTOCOL_VERSION         -

*******************************************************************************/
typedef enum
{
    REMOTE_DEBUG_PROTOCOL_VERSION_T_PROTOCOL_VERSION_PHASE_1 = 1,
    REMOTE_DEBUG_PROTOCOL_VERSION_T_PROTOCOL_VERSION = 2
} REMOTE_DEBUG_PROTOCOL_VERSION_T;
/*******************************************************************************

  NAME
    Remote_Debug_ROUTED_REASON

  DESCRIPTION

 VALUES
    handover      -
    link_lost     -
    link_closed   -
    not_supported -

*******************************************************************************/
typedef enum
{
    REMOTE_DEBUG_ROUTED_REASON_HANDOVER = 0,
    REMOTE_DEBUG_ROUTED_REASON_LINK_LOST = 1,
    REMOTE_DEBUG_ROUTED_REASON_LINK_CLOSED = 2,
    REMOTE_DEBUG_ROUTED_REASON_NOT_SUPPORTED = 3
} REMOTE_DEBUG_ROUTED_REASON;
/*******************************************************************************

  NAME
    Remote_Debug_ROUTED_REQ_ROUTE_T

  DESCRIPTION

 VALUES
    primary   -
    secondary -
    left      -
    right     -

*******************************************************************************/
typedef enum
{
    REMOTE_DEBUG_ROUTED_REQ_ROUTE_T_PRIMARY = 0,
    REMOTE_DEBUG_ROUTED_REQ_ROUTE_T_SECONDARY = 1,
    REMOTE_DEBUG_ROUTED_REQ_ROUTE_T_LEFT = 16,
    REMOTE_DEBUG_ROUTED_REQ_ROUTE_T_RIGHT = 17
} REMOTE_DEBUG_ROUTED_REQ_ROUTE_T;
/*******************************************************************************

  NAME
    Remote_Debug_ROUTED_RESP_ROUTE_T

  DESCRIPTION

 VALUES
    left_primary            -
    secondary_not_supported -
    left_secondary          -
    right_primary           -
    right_secondary         -

*******************************************************************************/
typedef enum
{
    REMOTE_DEBUG_ROUTED_RESP_ROUTE_T_LEFT_PRIMARY = 0,
    REMOTE_DEBUG_ROUTED_RESP_ROUTE_T_SECONDARY_NOT_SUPPORTED = 0,
    REMOTE_DEBUG_ROUTED_RESP_ROUTE_T_LEFT_SECONDARY = 1,
    REMOTE_DEBUG_ROUTED_RESP_ROUTE_T_RIGHT_PRIMARY = 16,
    REMOTE_DEBUG_ROUTED_RESP_ROUTE_T_RIGHT_SECONDARY = 17
} REMOTE_DEBUG_ROUTED_RESP_ROUTE_T;
/*******************************************************************************

  NAME
    Remote_Debug_Route_Cmd

  DESCRIPTION

 VALUES
    routed_request      -
    routed_response     -
    unroutable_response -

*******************************************************************************/
typedef enum
{
    REMOTE_DEBUG_ROUTE_CMD_ROUTED_REQUEST = 0,
    REMOTE_DEBUG_ROUTE_CMD_ROUTED_RESPONSE = 1,
    REMOTE_DEBUG_ROUTE_CMD_UNROUTABLE_RESPONSE = 2
} REMOTE_DEBUG_ROUTE_CMD;
/*******************************************************************************

  NAME
    Remote_Debug_Tr_Cmd

  DESCRIPTION

 VALUES
    transport_version_req       -
    connect_rsp                 -
    disconnect_req              -
    disconnect_rsp              -
    undeliverable_debug_cmd_rsp -
    transport_version_rsp       -
    max_pdu_size_req            -
    max_pdu_size_rsp            -
    connection_info_req         -
    connection_info_rsp         -
    available_devices_req       -
    available_devices_rsp       -
    connect_req                 -

*******************************************************************************/
typedef enum
{
    REMOTE_DEBUG_TR_CMD_TRANSPORT_VERSION_REQ = 1,
    REMOTE_DEBUG_TR_CMD_TRANSPORT_VERSION_RSP = 2,
    REMOTE_DEBUG_TR_CMD_MAX_PDU_SIZE_REQ = 3,
    REMOTE_DEBUG_TR_CMD_MAX_PDU_SIZE_RSP = 4,
    REMOTE_DEBUG_TR_CMD_CONNECTION_INFO_REQ = 5,
    REMOTE_DEBUG_TR_CMD_CONNECTION_INFO_RSP = 6,
    REMOTE_DEBUG_TR_CMD_AVAILABLE_DEVICES_REQ = 7,
    REMOTE_DEBUG_TR_CMD_AVAILABLE_DEVICES_RSP = 8,
    REMOTE_DEBUG_TR_CMD_CONNECT_REQ = 9,
    REMOTE_DEBUG_TR_CMD_CONNECT_RSP = 10,
    REMOTE_DEBUG_TR_CMD_DISCONNECT_REQ = 11,
    REMOTE_DEBUG_TR_CMD_DISCONNECT_RSP = 12,
    REMOTE_DEBUG_TR_CMD_UNDELIVERABLE_DEBUG_CMD_RSP = 13
} REMOTE_DEBUG_TR_CMD;
/*******************************************************************************

  NAME
    Remote_Debug_Transport_Type_Code

  DESCRIPTION

 VALUES
    GAIA         -
    IP_FORWARDER -

*******************************************************************************/
typedef enum
{
    REMOTE_DEBUG_TRANSPORT_TYPE_CODE_GAIA = 0,
    REMOTE_DEBUG_TRANSPORT_TYPE_CODE_IP_FORWARDER = 1
} REMOTE_DEBUG_TRANSPORT_TYPE_CODE;
/*******************************************************************************

  NAME
    Remote_Debug_connection_status

  DESCRIPTION

 VALUES
    success -
    refused -
    timeout -

*******************************************************************************/
typedef enum
{
    REMOTE_DEBUG_CONNECTION_STATUS_SUCCESS = 0,
    REMOTE_DEBUG_CONNECTION_STATUS_REFUSED = 1,
    REMOTE_DEBUG_CONNECTION_STATUS_TIMEOUT = 2
} REMOTE_DEBUG_CONNECTION_STATUS;
/*******************************************************************************

  NAME
    Remote_Debug_disconnection_status

  DESCRIPTION

 VALUES
    success       -
    not_connected -
    timeout       -

*******************************************************************************/
typedef enum
{
    REMOTE_DEBUG_DISCONNECTION_STATUS_SUCCESS = 0,
    REMOTE_DEBUG_DISCONNECTION_STATUS_NOT_CONNECTED = 1,
    REMOTE_DEBUG_DISCONNECTION_STATUS_TIMEOUT = 2
} REMOTE_DEBUG_DISCONNECTION_STATUS;
/*******************************************************************************

  NAME
    Remote_Debug_tr_type

  DESCRIPTION

 VALUES
    debug  -
    memory -

*******************************************************************************/
typedef enum
{
    REMOTE_DEBUG_TR_TYPE_DEBUG = 0,
    REMOTE_DEBUG_TR_TYPE_MEMORY = 1
} REMOTE_DEBUG_TR_TYPE;
/*******************************************************************************

  NAME
    Remote_Debug_trb

  DESCRIPTION

 VALUES
    NO_ERROR            -
    SUBSYSTEM_POWER_OFF -
    DEBUG_TIMEOUT       -
    ACCESS_PROTECTION   -
    NO_MEMORY_HERE      -
    WRONG_LENGTH        -
    NOT_WRITABLE        -
    BAD_ALIGNMENT       -
    SUBSYSTEM_ASLEEP    -
    ROUTING_ERROR       -
    LOCK_ERROR          -

*******************************************************************************/
typedef enum
{
    REMOTE_DEBUG_TRB_NO_ERROR = 0,
    REMOTE_DEBUG_TRB_SUBSYSTEM_POWER_OFF = 1,
    REMOTE_DEBUG_TRB_SUBSYSTEM_ASLEEP = 2,
    REMOTE_DEBUG_TRB_ROUTING_ERROR = 3,
    REMOTE_DEBUG_TRB_LOCK_ERROR = 4,
    REMOTE_DEBUG_TRB_DEBUG_TIMEOUT = 10,
    REMOTE_DEBUG_TRB_ACCESS_PROTECTION = 11,
    REMOTE_DEBUG_TRB_NO_MEMORY_HERE = 12,
    REMOTE_DEBUG_TRB_WRONG_LENGTH = 13,
    REMOTE_DEBUG_TRB_NOT_WRITABLE = 14,
    REMOTE_DEBUG_TRB_BAD_ALIGNMENT = 15
} REMOTE_DEBUG_TRB;
/*******************************************************************************

  NAME
    Remote_Debug_undeliverable_status

  DESCRIPTION

 VALUES
    not_connected               -
    link_timeout                -
    link_disconnected_by_device -
    feature_not_supported       -

*******************************************************************************/
typedef enum
{
    REMOTE_DEBUG_UNDELIVERABLE_STATUS_NOT_CONNECTED = 1,
    REMOTE_DEBUG_UNDELIVERABLE_STATUS_LINK_TIMEOUT = 2,
    REMOTE_DEBUG_UNDELIVERABLE_STATUS_LINK_DISCONNECTED_BY_DEVICE = 3,
    REMOTE_DEBUG_UNDELIVERABLE_STATUS_FEATURE_NOT_SUPPORTED = 4
} REMOTE_DEBUG_UNDELIVERABLE_STATUS;


#define REMOTE_DEBUG_PRIM_ANY_SIZE 1

/*******************************************************************************

  NAME
    Remote_Debug_APPCMD_REQ_T

  DESCRIPTION

  MEMBERS
    timeout_seconds -
    command         -
    parameters      -

*******************************************************************************/
typedef struct
{
    uint8 _data[12];
} REMOTE_DEBUG_APPCMD_REQ_T;

/* The following macros take REMOTE_DEBUG_APPCMD_REQ_T *remote_debug_appcmd_req_t_ptr */
#define REMOTE_DEBUG_APPCMD_REQ_T_TIMEOUT_SECONDS_BYTE_OFFSET (0)
#define REMOTE_DEBUG_APPCMD_REQ_T_TIMEOUT_SECONDS_GET(remote_debug_appcmd_req_t_ptr)  \
    (((uint32)((remote_debug_appcmd_req_t_ptr)->_data[0]) | \
      ((uint32)((remote_debug_appcmd_req_t_ptr)->_data[1]) << 8) | \
      ((uint32)((remote_debug_appcmd_req_t_ptr)->_data[2]) << (2 * 8)) | \
      ((uint32)((remote_debug_appcmd_req_t_ptr)->_data[3]) << (3 * 8))))
#define REMOTE_DEBUG_APPCMD_REQ_T_TIMEOUT_SECONDS_SET(remote_debug_appcmd_req_t_ptr, timeout_seconds) do { \
        (remote_debug_appcmd_req_t_ptr)->_data[0] = (uint8)((timeout_seconds) & 0xff); \
        (remote_debug_appcmd_req_t_ptr)->_data[1] = (uint8)(((timeout_seconds) >> 8) & 0xff); \
        (remote_debug_appcmd_req_t_ptr)->_data[2] = (uint8)(((timeout_seconds) >> (2 * 8)) & 0xff); \
        (remote_debug_appcmd_req_t_ptr)->_data[3] = (uint8)(((timeout_seconds) >> (3 * 8)) & 0xff); } while (0)
#define REMOTE_DEBUG_APPCMD_REQ_T_COMMAND_BYTE_OFFSET (4)
#define REMOTE_DEBUG_APPCMD_REQ_T_COMMAND_GET(remote_debug_appcmd_req_t_ptr)  \
    (((uint32)((remote_debug_appcmd_req_t_ptr)->_data[4]) | \
      ((uint32)((remote_debug_appcmd_req_t_ptr)->_data[5]) << 8) | \
      ((uint32)((remote_debug_appcmd_req_t_ptr)->_data[4 + 2]) << (2 * 8)) | \
      ((uint32)((remote_debug_appcmd_req_t_ptr)->_data[4 + 3]) << (3 * 8))))
#define REMOTE_DEBUG_APPCMD_REQ_T_COMMAND_SET(remote_debug_appcmd_req_t_ptr, command) do { \
        (remote_debug_appcmd_req_t_ptr)->_data[4] = (uint8)((command) & 0xff); \
        (remote_debug_appcmd_req_t_ptr)->_data[5] = (uint8)(((command) >> 8) & 0xff); \
        (remote_debug_appcmd_req_t_ptr)->_data[4 + 2] = (uint8)(((command) >> (2 * 8)) & 0xff); \
        (remote_debug_appcmd_req_t_ptr)->_data[4 + 3] = (uint8)(((command) >> (3 * 8)) & 0xff); } while (0)
#define REMOTE_DEBUG_APPCMD_REQ_T_PARAMETERS_BYTE_OFFSET (8)
#define REMOTE_DEBUG_APPCMD_REQ_T_PARAMETERS_GET(remote_debug_appcmd_req_t_ptr)  \
    (((uint32)((remote_debug_appcmd_req_t_ptr)->_data[8]) | \
      ((uint32)((remote_debug_appcmd_req_t_ptr)->_data[8 + 1]) << 8) | \
      ((uint32)((remote_debug_appcmd_req_t_ptr)->_data[8 + 2]) << (2 * 8)) | \
      ((uint32)((remote_debug_appcmd_req_t_ptr)->_data[8 + 3]) << (3 * 8))))
#define REMOTE_DEBUG_APPCMD_REQ_T_PARAMETERS_SET(remote_debug_appcmd_req_t_ptr, parameters) do { \
        (remote_debug_appcmd_req_t_ptr)->_data[8] = (uint8)((parameters) & 0xff); \
        (remote_debug_appcmd_req_t_ptr)->_data[8 + 1] = (uint8)(((parameters) >> 8) & 0xff); \
        (remote_debug_appcmd_req_t_ptr)->_data[8 + 2] = (uint8)(((parameters) >> (2 * 8)) & 0xff); \
        (remote_debug_appcmd_req_t_ptr)->_data[8 + 3] = (uint8)(((parameters) >> (3 * 8)) & 0xff); } while (0)
#define REMOTE_DEBUG_APPCMD_REQ_T_BYTE_SIZE (12)
/*lint -e(773) allow unparenthesized*/
#define REMOTE_DEBUG_APPCMD_REQ_T_CREATE(timeout_seconds, command, parameters) \
    (uint8)((timeout_seconds) & 0xff), \
    (uint8)(((timeout_seconds) >> 8) & 0xff), \
    (uint8)(((timeout_seconds) >> 2 * 8) & 0xff), \
    (uint8)(((timeout_seconds) >> 3 * 8) & 0xff), \
    (uint8)((command) & 0xff), \
    (uint8)(((command) >> 8) & 0xff), \
    (uint8)(((command) >> 2 * 8) & 0xff), \
    (uint8)(((command) >> 3 * 8) & 0xff), \
    (uint8)((parameters) & 0xff), \
    (uint8)(((parameters) >> 8) & 0xff), \
    (uint8)(((parameters) >> 2 * 8) & 0xff), \
    (uint8)(((parameters) >> 3 * 8) & 0xff)
#define REMOTE_DEBUG_APPCMD_REQ_T_PACK(remote_debug_appcmd_req_t_ptr, timeout_seconds, command, parameters) \
    do { \
        (remote_debug_appcmd_req_t_ptr)->_data[0] = (uint8)((uint8)((timeout_seconds) & 0xff)); \
        (remote_debug_appcmd_req_t_ptr)->_data[1] = (uint8)(((timeout_seconds) >> 8) & 0xff); \
        (remote_debug_appcmd_req_t_ptr)->_data[2] = (uint8)(((timeout_seconds) >> (2 * 8)) & 0xff); \
        (remote_debug_appcmd_req_t_ptr)->_data[3] = (uint8)((((timeout_seconds) >> (3 * 8)) & 0xff)); \
        (remote_debug_appcmd_req_t_ptr)->_data[4] = (uint8)((uint8)((command) & 0xff)); \
        (remote_debug_appcmd_req_t_ptr)->_data[5] = (uint8)(((command) >> 8) & 0xff); \
        (remote_debug_appcmd_req_t_ptr)->_data[4 + 2] = (uint8)(((command) >> (2 * 8)) & 0xff); \
        (remote_debug_appcmd_req_t_ptr)->_data[4 + 3] = (uint8)((((command) >> (3 * 8)) & 0xff)); \
        (remote_debug_appcmd_req_t_ptr)->_data[8] = (uint8)((uint8)((parameters) & 0xff)); \
        (remote_debug_appcmd_req_t_ptr)->_data[8 + 1] = (uint8)(((parameters) >> 8) & 0xff); \
        (remote_debug_appcmd_req_t_ptr)->_data[8 + 2] = (uint8)(((parameters) >> (2 * 8)) & 0xff); \
        (remote_debug_appcmd_req_t_ptr)->_data[8 + 3] = (uint8)((((parameters) >> (3 * 8)) & 0xff)); \
    } while (0)


/*******************************************************************************

  NAME
    Remote_Debug_APPCMD_RSP_T

  DESCRIPTION

  MEMBERS
    reponse -
    result  -

*******************************************************************************/
typedef struct
{
    uint8 _data[8];
} REMOTE_DEBUG_APPCMD_RSP_T;

/* The following macros take REMOTE_DEBUG_APPCMD_RSP_T *remote_debug_appcmd_rsp_t_ptr */
#define REMOTE_DEBUG_APPCMD_RSP_T_REPONSE_BYTE_OFFSET (0)
#define REMOTE_DEBUG_APPCMD_RSP_T_REPONSE_GET(remote_debug_appcmd_rsp_t_ptr)  \
    (((uint32)((remote_debug_appcmd_rsp_t_ptr)->_data[0]) | \
      ((uint32)((remote_debug_appcmd_rsp_t_ptr)->_data[1]) << 8) | \
      ((uint32)((remote_debug_appcmd_rsp_t_ptr)->_data[2]) << (2 * 8)) | \
      ((uint32)((remote_debug_appcmd_rsp_t_ptr)->_data[3]) << (3 * 8))))
#define REMOTE_DEBUG_APPCMD_RSP_T_REPONSE_SET(remote_debug_appcmd_rsp_t_ptr, reponse) do { \
        (remote_debug_appcmd_rsp_t_ptr)->_data[0] = (uint8)((reponse) & 0xff); \
        (remote_debug_appcmd_rsp_t_ptr)->_data[1] = (uint8)(((reponse) >> 8) & 0xff); \
        (remote_debug_appcmd_rsp_t_ptr)->_data[2] = (uint8)(((reponse) >> (2 * 8)) & 0xff); \
        (remote_debug_appcmd_rsp_t_ptr)->_data[3] = (uint8)(((reponse) >> (3 * 8)) & 0xff); } while (0)
#define REMOTE_DEBUG_APPCMD_RSP_T_RESULT_BYTE_OFFSET (4)
#define REMOTE_DEBUG_APPCMD_RSP_T_RESULT_GET(remote_debug_appcmd_rsp_t_ptr)  \
    (((uint32)((remote_debug_appcmd_rsp_t_ptr)->_data[4]) | \
      ((uint32)((remote_debug_appcmd_rsp_t_ptr)->_data[5]) << 8) | \
      ((uint32)((remote_debug_appcmd_rsp_t_ptr)->_data[4 + 2]) << (2 * 8)) | \
      ((uint32)((remote_debug_appcmd_rsp_t_ptr)->_data[4 + 3]) << (3 * 8))))
#define REMOTE_DEBUG_APPCMD_RSP_T_RESULT_SET(remote_debug_appcmd_rsp_t_ptr, result) do { \
        (remote_debug_appcmd_rsp_t_ptr)->_data[4] = (uint8)((result) & 0xff); \
        (remote_debug_appcmd_rsp_t_ptr)->_data[5] = (uint8)(((result) >> 8) & 0xff); \
        (remote_debug_appcmd_rsp_t_ptr)->_data[4 + 2] = (uint8)(((result) >> (2 * 8)) & 0xff); \
        (remote_debug_appcmd_rsp_t_ptr)->_data[4 + 3] = (uint8)(((result) >> (3 * 8)) & 0xff); } while (0)
#define REMOTE_DEBUG_APPCMD_RSP_T_BYTE_SIZE (8)
/*lint -e(773) allow unparenthesized*/
#define REMOTE_DEBUG_APPCMD_RSP_T_CREATE(reponse, result) \
    (uint8)((reponse) & 0xff), \
    (uint8)(((reponse) >> 8) & 0xff), \
    (uint8)(((reponse) >> 2 * 8) & 0xff), \
    (uint8)(((reponse) >> 3 * 8) & 0xff), \
    (uint8)((result) & 0xff), \
    (uint8)(((result) >> 8) & 0xff), \
    (uint8)(((result) >> 2 * 8) & 0xff), \
    (uint8)(((result) >> 3 * 8) & 0xff)
#define REMOTE_DEBUG_APPCMD_RSP_T_PACK(remote_debug_appcmd_rsp_t_ptr, reponse, result) \
    do { \
        (remote_debug_appcmd_rsp_t_ptr)->_data[0] = (uint8)((uint8)((reponse) & 0xff)); \
        (remote_debug_appcmd_rsp_t_ptr)->_data[1] = (uint8)(((reponse) >> 8) & 0xff); \
        (remote_debug_appcmd_rsp_t_ptr)->_data[2] = (uint8)(((reponse) >> (2 * 8)) & 0xff); \
        (remote_debug_appcmd_rsp_t_ptr)->_data[3] = (uint8)((((reponse) >> (3 * 8)) & 0xff)); \
        (remote_debug_appcmd_rsp_t_ptr)->_data[4] = (uint8)((uint8)((result) & 0xff)); \
        (remote_debug_appcmd_rsp_t_ptr)->_data[5] = (uint8)(((result) >> 8) & 0xff); \
        (remote_debug_appcmd_rsp_t_ptr)->_data[4 + 2] = (uint8)(((result) >> (2 * 8)) & 0xff); \
        (remote_debug_appcmd_rsp_t_ptr)->_data[4 + 3] = (uint8)((((result) >> (3 * 8)) & 0xff)); \
    } while (0)


/*******************************************************************************

  NAME
    Remote_Debug_CAPABILITIES_T

  DESCRIPTION

  MEMBERS
    routing  -
    reserved -

*******************************************************************************/
typedef struct
{
    uint8 _data[4];
} REMOTE_DEBUG_CAPABILITIES_T;

/* The following macros take REMOTE_DEBUG_CAPABILITIES_T *remote_debug_capabilities_t_ptr */
#define REMOTE_DEBUG_CAPABILITIES_T_ROUTING_BYTE_OFFSET (0)
#define REMOTE_DEBUG_CAPABILITIES_T_ROUTING_GET(remote_debug_capabilities_t_ptr) ((((remote_debug_capabilities_t_ptr)->_data[0] & 0x1)))
#define REMOTE_DEBUG_CAPABILITIES_T_ROUTING_SET(remote_debug_capabilities_t_ptr, routing) ((remote_debug_capabilities_t_ptr)->_data[0] =  \
                                                                                               (uint8)(((remote_debug_capabilities_t_ptr)->_data[0] & ~0x1) | (((routing)) & 0x1)))
#define REMOTE_DEBUG_CAPABILITIES_T_BYTE_SIZE (4)
/*lint -e(773) allow unparenthesized*/
#define REMOTE_DEBUG_CAPABILITIES_T_CREATE(routing) \
    (uint8)(((routing)) & 0x1)
#define REMOTE_DEBUG_CAPABILITIES_T_PACK(remote_debug_capabilities_t_ptr, routing) \
    do { \
        (remote_debug_capabilities_t_ptr)->_data[0] = (uint8)((uint8)(((routing)) & 0x1)); \
    } while (0)


/*******************************************************************************

  NAME
    Remote_Debug_CHIP_RESET_REQ_T

  DESCRIPTION

  MEMBERS
    reset_type -

*******************************************************************************/
typedef struct
{
    uint8 _data[1];
} REMOTE_DEBUG_CHIP_RESET_REQ_T;

/* The following macros take REMOTE_DEBUG_CHIP_RESET_REQ_T *remote_debug_chip_reset_req_t_ptr */
#define REMOTE_DEBUG_CHIP_RESET_REQ_T_RESET_TYPE_BYTE_OFFSET (0)
#define REMOTE_DEBUG_CHIP_RESET_REQ_T_RESET_TYPE_GET(remote_debug_chip_reset_req_t_ptr) ((remote_debug_chip_reset_req_t_ptr)->_data[0])
#define REMOTE_DEBUG_CHIP_RESET_REQ_T_RESET_TYPE_SET(remote_debug_chip_reset_req_t_ptr, reset_type) ((remote_debug_chip_reset_req_t_ptr)->_data[0] = (uint8)(reset_type))
#define REMOTE_DEBUG_CHIP_RESET_REQ_T_BYTE_SIZE (1)
/*lint -e(773) allow unparenthesized*/
#define REMOTE_DEBUG_CHIP_RESET_REQ_T_CREATE(reset_type) \
    (uint8)(reset_type)
#define REMOTE_DEBUG_CHIP_RESET_REQ_T_PACK(remote_debug_chip_reset_req_t_ptr, reset_type) \
    do { \
        (remote_debug_chip_reset_req_t_ptr)->_data[0] = (uint8)((uint8)(reset_type)); \
    } while (0)


/*******************************************************************************

  NAME
    Remote_Debug_CHIP_RESET_RSP_T

  DESCRIPTION

  MEMBERS
    reset_status -

*******************************************************************************/
typedef struct
{
    uint8 _data[1];
} REMOTE_DEBUG_CHIP_RESET_RSP_T;

/* The following macros take REMOTE_DEBUG_CHIP_RESET_RSP_T *remote_debug_chip_reset_rsp_t_ptr */
#define REMOTE_DEBUG_CHIP_RESET_RSP_T_RESET_STATUS_BYTE_OFFSET (0)
#define REMOTE_DEBUG_CHIP_RESET_RSP_T_RESET_STATUS_GET(remote_debug_chip_reset_rsp_t_ptr) ((remote_debug_chip_reset_rsp_t_ptr)->_data[0])
#define REMOTE_DEBUG_CHIP_RESET_RSP_T_RESET_STATUS_SET(remote_debug_chip_reset_rsp_t_ptr, reset_status) ((remote_debug_chip_reset_rsp_t_ptr)->_data[0] = (uint8)(reset_status))
#define REMOTE_DEBUG_CHIP_RESET_RSP_T_BYTE_SIZE (1)
/*lint -e(773) allow unparenthesized*/
#define REMOTE_DEBUG_CHIP_RESET_RSP_T_CREATE(reset_status) \
    (uint8)(reset_status)
#define REMOTE_DEBUG_CHIP_RESET_RSP_T_PACK(remote_debug_chip_reset_rsp_t_ptr, reset_status) \
    do { \
        (remote_debug_chip_reset_rsp_t_ptr)->_data[0] = (uint8)((uint8)(reset_status)); \
    } while (0)


/*******************************************************************************

  NAME
    Remote_Debug_CONNECT_RSP_T

  DESCRIPTION

  MEMBERS
    status -

*******************************************************************************/
typedef struct
{
    uint8 _data[4];
} REMOTE_DEBUG_CONNECT_RSP_T;

/* The following macros take REMOTE_DEBUG_CONNECT_RSP_T *remote_debug_connect_rsp_t_ptr */
#define REMOTE_DEBUG_CONNECT_RSP_T_STATUS_BYTE_OFFSET (0)
#define REMOTE_DEBUG_CONNECT_RSP_T_STATUS_GET(remote_debug_connect_rsp_t_ptr)  \
    ((REMOTE_DEBUG_CONNECTION_STATUS)((uint32)((remote_debug_connect_rsp_t_ptr)->_data[0]) | \
                                      ((uint32)((remote_debug_connect_rsp_t_ptr)->_data[1]) << 8) | \
                                      ((uint32)((remote_debug_connect_rsp_t_ptr)->_data[2]) << (2 * 8)) | \
                                      ((uint32)((remote_debug_connect_rsp_t_ptr)->_data[3]) << (3 * 8))))
#define REMOTE_DEBUG_CONNECT_RSP_T_STATUS_SET(remote_debug_connect_rsp_t_ptr, status) do { \
        (remote_debug_connect_rsp_t_ptr)->_data[0] = (uint8)((status) & 0xff); \
        (remote_debug_connect_rsp_t_ptr)->_data[1] = (uint8)(((status) >> 8) & 0xff); \
        (remote_debug_connect_rsp_t_ptr)->_data[2] = (uint8)(((status) >> (2 * 8)) & 0xff); \
        (remote_debug_connect_rsp_t_ptr)->_data[3] = (uint8)(((status) >> (3 * 8)) & 0xff); } while (0)
#define REMOTE_DEBUG_CONNECT_RSP_T_BYTE_SIZE (4)
/*lint -e(773) allow unparenthesized*/
#define REMOTE_DEBUG_CONNECT_RSP_T_CREATE(status) \
    (uint8)((status) & 0xff), \
    (uint8)(((status) >> 8) & 0xff), \
    (uint8)(((status) >> 2 * 8) & 0xff), \
    (uint8)(((status) >> 3 * 8) & 0xff)
#define REMOTE_DEBUG_CONNECT_RSP_T_PACK(remote_debug_connect_rsp_t_ptr, status) \
    do { \
        (remote_debug_connect_rsp_t_ptr)->_data[0] = (uint8)((uint8)((status) & 0xff)); \
        (remote_debug_connect_rsp_t_ptr)->_data[1] = (uint8)(((status) >> 8) & 0xff); \
        (remote_debug_connect_rsp_t_ptr)->_data[2] = (uint8)(((status) >> (2 * 8)) & 0xff); \
        (remote_debug_connect_rsp_t_ptr)->_data[3] = (uint8)((((status) >> (3 * 8)) & 0xff)); \
    } while (0)


/*******************************************************************************

  NAME
    Remote_Debug_DISCONNECT_RSP_T

  DESCRIPTION

  MEMBERS
    status -

*******************************************************************************/
typedef struct
{
    uint8 _data[2];
} REMOTE_DEBUG_DISCONNECT_RSP_T;

/* The following macros take REMOTE_DEBUG_DISCONNECT_RSP_T *remote_debug_disconnect_rsp_t_ptr */
#define REMOTE_DEBUG_DISCONNECT_RSP_T_STATUS_BYTE_OFFSET (0)
#define REMOTE_DEBUG_DISCONNECT_RSP_T_STATUS_GET(remote_debug_disconnect_rsp_t_ptr)  \
    ((REMOTE_DEBUG_DISCONNECTION_STATUS)((uint16)((remote_debug_disconnect_rsp_t_ptr)->_data[0]) | \
                                         ((uint16)((remote_debug_disconnect_rsp_t_ptr)->_data[1]) << 8)))
#define REMOTE_DEBUG_DISCONNECT_RSP_T_STATUS_SET(remote_debug_disconnect_rsp_t_ptr, status) do { \
        (remote_debug_disconnect_rsp_t_ptr)->_data[0] = (uint8)((status) & 0xff); \
        (remote_debug_disconnect_rsp_t_ptr)->_data[1] = (uint8)((status) >> 8); } while (0)
#define REMOTE_DEBUG_DISCONNECT_RSP_T_BYTE_SIZE (2)
/*lint -e(773) allow unparenthesized*/
#define REMOTE_DEBUG_DISCONNECT_RSP_T_CREATE(status) \
    (uint8)((status) & 0xff), \
    (uint8)((status) >> 8)
#define REMOTE_DEBUG_DISCONNECT_RSP_T_PACK(remote_debug_disconnect_rsp_t_ptr, status) \
    do { \
        (remote_debug_disconnect_rsp_t_ptr)->_data[0] = (uint8)((uint8)((status) & 0xff)); \
        (remote_debug_disconnect_rsp_t_ptr)->_data[1] = (uint8)(((status) >> 8)); \
    } while (0)


/*******************************************************************************

  NAME
    Remote_Debug_MAX_PDU_SIZE_RSP_T

  DESCRIPTION

  MEMBERS
    pdu_size_bytes                -
    number_of_outstanding_packets -

*******************************************************************************/
typedef struct
{
    uint8 _data[8];
} REMOTE_DEBUG_MAX_PDU_SIZE_RSP_T;

/* The following macros take REMOTE_DEBUG_MAX_PDU_SIZE_RSP_T *remote_debug_max_pdu_size_rsp_t_ptr */
#define REMOTE_DEBUG_MAX_PDU_SIZE_RSP_T_PDU_SIZE_BYTES_BYTE_OFFSET (0)
#define REMOTE_DEBUG_MAX_PDU_SIZE_RSP_T_PDU_SIZE_BYTES_GET(remote_debug_max_pdu_size_rsp_t_ptr)  \
    (((uint32)((remote_debug_max_pdu_size_rsp_t_ptr)->_data[0]) | \
      ((uint32)((remote_debug_max_pdu_size_rsp_t_ptr)->_data[1]) << 8) | \
      ((uint32)((remote_debug_max_pdu_size_rsp_t_ptr)->_data[2]) << (2 * 8)) | \
      ((uint32)((remote_debug_max_pdu_size_rsp_t_ptr)->_data[3]) << (3 * 8))))
#define REMOTE_DEBUG_MAX_PDU_SIZE_RSP_T_PDU_SIZE_BYTES_SET(remote_debug_max_pdu_size_rsp_t_ptr, pdu_size_bytes) do { \
        (remote_debug_max_pdu_size_rsp_t_ptr)->_data[0] = (uint8)((pdu_size_bytes) & 0xff); \
        (remote_debug_max_pdu_size_rsp_t_ptr)->_data[1] = (uint8)(((pdu_size_bytes) >> 8) & 0xff); \
        (remote_debug_max_pdu_size_rsp_t_ptr)->_data[2] = (uint8)(((pdu_size_bytes) >> (2 * 8)) & 0xff); \
        (remote_debug_max_pdu_size_rsp_t_ptr)->_data[3] = (uint8)(((pdu_size_bytes) >> (3 * 8)) & 0xff); } while (0)
#define REMOTE_DEBUG_MAX_PDU_SIZE_RSP_T_NUMBER_OF_OUTSTANDING_PACKETS_BYTE_OFFSET (4)
#define REMOTE_DEBUG_MAX_PDU_SIZE_RSP_T_NUMBER_OF_OUTSTANDING_PACKETS_GET(remote_debug_max_pdu_size_rsp_t_ptr)  \
    (((uint32)((remote_debug_max_pdu_size_rsp_t_ptr)->_data[4]) | \
      ((uint32)((remote_debug_max_pdu_size_rsp_t_ptr)->_data[5]) << 8) | \
      ((uint32)((remote_debug_max_pdu_size_rsp_t_ptr)->_data[4 + 2]) << (2 * 8)) | \
      ((uint32)((remote_debug_max_pdu_size_rsp_t_ptr)->_data[4 + 3]) << (3 * 8))))
#define REMOTE_DEBUG_MAX_PDU_SIZE_RSP_T_NUMBER_OF_OUTSTANDING_PACKETS_SET(remote_debug_max_pdu_size_rsp_t_ptr, number_of_outstanding_packets) do { \
        (remote_debug_max_pdu_size_rsp_t_ptr)->_data[4] = (uint8)((number_of_outstanding_packets) & 0xff); \
        (remote_debug_max_pdu_size_rsp_t_ptr)->_data[5] = (uint8)(((number_of_outstanding_packets) >> 8) & 0xff); \
        (remote_debug_max_pdu_size_rsp_t_ptr)->_data[4 + 2] = (uint8)(((number_of_outstanding_packets) >> (2 * 8)) & 0xff); \
        (remote_debug_max_pdu_size_rsp_t_ptr)->_data[4 + 3] = (uint8)(((number_of_outstanding_packets) >> (3 * 8)) & 0xff); } while (0)
#define REMOTE_DEBUG_MAX_PDU_SIZE_RSP_T_BYTE_SIZE (8)
/*lint -e(773) allow unparenthesized*/
#define REMOTE_DEBUG_MAX_PDU_SIZE_RSP_T_CREATE(pdu_size_bytes, number_of_outstanding_packets) \
    (uint8)((pdu_size_bytes) & 0xff), \
    (uint8)(((pdu_size_bytes) >> 8) & 0xff), \
    (uint8)(((pdu_size_bytes) >> 2 * 8) & 0xff), \
    (uint8)(((pdu_size_bytes) >> 3 * 8) & 0xff), \
    (uint8)((number_of_outstanding_packets) & 0xff), \
    (uint8)(((number_of_outstanding_packets) >> 8) & 0xff), \
    (uint8)(((number_of_outstanding_packets) >> 2 * 8) & 0xff), \
    (uint8)(((number_of_outstanding_packets) >> 3 * 8) & 0xff)
#define REMOTE_DEBUG_MAX_PDU_SIZE_RSP_T_PACK(remote_debug_max_pdu_size_rsp_t_ptr, pdu_size_bytes, number_of_outstanding_packets) \
    do { \
        (remote_debug_max_pdu_size_rsp_t_ptr)->_data[0] = (uint8)((uint8)((pdu_size_bytes) & 0xff)); \
        (remote_debug_max_pdu_size_rsp_t_ptr)->_data[1] = (uint8)(((pdu_size_bytes) >> 8) & 0xff); \
        (remote_debug_max_pdu_size_rsp_t_ptr)->_data[2] = (uint8)(((pdu_size_bytes) >> (2 * 8)) & 0xff); \
        (remote_debug_max_pdu_size_rsp_t_ptr)->_data[3] = (uint8)((((pdu_size_bytes) >> (3 * 8)) & 0xff)); \
        (remote_debug_max_pdu_size_rsp_t_ptr)->_data[4] = (uint8)((uint8)((number_of_outstanding_packets) & 0xff)); \
        (remote_debug_max_pdu_size_rsp_t_ptr)->_data[5] = (uint8)(((number_of_outstanding_packets) >> 8) & 0xff); \
        (remote_debug_max_pdu_size_rsp_t_ptr)->_data[4 + 2] = (uint8)(((number_of_outstanding_packets) >> (2 * 8)) & 0xff); \
        (remote_debug_max_pdu_size_rsp_t_ptr)->_data[4 + 3] = (uint8)((((number_of_outstanding_packets) >> (3 * 8)) & 0xff)); \
    } while (0)


/*******************************************************************************

  NAME
    Remote_Debug_MEMORY_READ_REQ_T

  DESCRIPTION

  MEMBERS
    subsystem_id          -
    block_id              -
    bytes_per_transaction -
    transaction_type      -
    address               -
    read_length_bytes     -

*******************************************************************************/
typedef struct
{
    uint8 _data[12];
} REMOTE_DEBUG_MEMORY_READ_REQ_T;

/* The following macros take REMOTE_DEBUG_MEMORY_READ_REQ_T *remote_debug_memory_read_req_t_ptr */
#define REMOTE_DEBUG_MEMORY_READ_REQ_T_SUBSYSTEM_ID_BYTE_OFFSET (0)
#define REMOTE_DEBUG_MEMORY_READ_REQ_T_SUBSYSTEM_ID_GET(remote_debug_memory_read_req_t_ptr) ((remote_debug_memory_read_req_t_ptr)->_data[0])
#define REMOTE_DEBUG_MEMORY_READ_REQ_T_SUBSYSTEM_ID_SET(remote_debug_memory_read_req_t_ptr, subsystem_id) ((remote_debug_memory_read_req_t_ptr)->_data[0] = (uint8)(subsystem_id))
#define REMOTE_DEBUG_MEMORY_READ_REQ_T_BLOCK_ID_BYTE_OFFSET (1)
#define REMOTE_DEBUG_MEMORY_READ_REQ_T_BLOCK_ID_GET(remote_debug_memory_read_req_t_ptr) ((remote_debug_memory_read_req_t_ptr)->_data[1])
#define REMOTE_DEBUG_MEMORY_READ_REQ_T_BLOCK_ID_SET(remote_debug_memory_read_req_t_ptr, block_id) ((remote_debug_memory_read_req_t_ptr)->_data[1] = (uint8)(block_id))
#define REMOTE_DEBUG_MEMORY_READ_REQ_T_BYTES_PER_TRANSACTION_BYTE_OFFSET (2)
#define REMOTE_DEBUG_MEMORY_READ_REQ_T_BYTES_PER_TRANSACTION_GET(remote_debug_memory_read_req_t_ptr) ((remote_debug_memory_read_req_t_ptr)->_data[2])
#define REMOTE_DEBUG_MEMORY_READ_REQ_T_BYTES_PER_TRANSACTION_SET(remote_debug_memory_read_req_t_ptr, bytes_per_transaction) ((remote_debug_memory_read_req_t_ptr)->_data[2] = (uint8)(bytes_per_transaction))
#define REMOTE_DEBUG_MEMORY_READ_REQ_T_TRANSACTION_TYPE_BYTE_OFFSET (3)
#define REMOTE_DEBUG_MEMORY_READ_REQ_T_TRANSACTION_TYPE_GET(remote_debug_memory_read_req_t_ptr) ((REMOTE_DEBUG_TR_TYPE)(remote_debug_memory_read_req_t_ptr)->_data[3])
#define REMOTE_DEBUG_MEMORY_READ_REQ_T_TRANSACTION_TYPE_SET(remote_debug_memory_read_req_t_ptr, transaction_type) ((remote_debug_memory_read_req_t_ptr)->_data[3] = (uint8)(transaction_type))
#define REMOTE_DEBUG_MEMORY_READ_REQ_T_ADDRESS_BYTE_OFFSET (4)
#define REMOTE_DEBUG_MEMORY_READ_REQ_T_ADDRESS_GET(remote_debug_memory_read_req_t_ptr)  \
    (((uint32)((remote_debug_memory_read_req_t_ptr)->_data[4]) | \
      ((uint32)((remote_debug_memory_read_req_t_ptr)->_data[5]) << 8) | \
      ((uint32)((remote_debug_memory_read_req_t_ptr)->_data[4 + 2]) << (2 * 8)) | \
      ((uint32)((remote_debug_memory_read_req_t_ptr)->_data[4 + 3]) << (3 * 8))))
#define REMOTE_DEBUG_MEMORY_READ_REQ_T_ADDRESS_SET(remote_debug_memory_read_req_t_ptr, address) do { \
        (remote_debug_memory_read_req_t_ptr)->_data[4] = (uint8)((address) & 0xff); \
        (remote_debug_memory_read_req_t_ptr)->_data[5] = (uint8)(((address) >> 8) & 0xff); \
        (remote_debug_memory_read_req_t_ptr)->_data[4 + 2] = (uint8)(((address) >> (2 * 8)) & 0xff); \
        (remote_debug_memory_read_req_t_ptr)->_data[4 + 3] = (uint8)(((address) >> (3 * 8)) & 0xff); } while (0)
#define REMOTE_DEBUG_MEMORY_READ_REQ_T_READ_LENGTH_BYTES_BYTE_OFFSET (8)
#define REMOTE_DEBUG_MEMORY_READ_REQ_T_READ_LENGTH_BYTES_GET(remote_debug_memory_read_req_t_ptr)  \
    (((uint32)((remote_debug_memory_read_req_t_ptr)->_data[8]) | \
      ((uint32)((remote_debug_memory_read_req_t_ptr)->_data[8 + 1]) << 8) | \
      ((uint32)((remote_debug_memory_read_req_t_ptr)->_data[8 + 2]) << (2 * 8)) | \
      ((uint32)((remote_debug_memory_read_req_t_ptr)->_data[8 + 3]) << (3 * 8))))
#define REMOTE_DEBUG_MEMORY_READ_REQ_T_READ_LENGTH_BYTES_SET(remote_debug_memory_read_req_t_ptr, read_length_bytes) do { \
        (remote_debug_memory_read_req_t_ptr)->_data[8] = (uint8)((read_length_bytes) & 0xff); \
        (remote_debug_memory_read_req_t_ptr)->_data[8 + 1] = (uint8)(((read_length_bytes) >> 8) & 0xff); \
        (remote_debug_memory_read_req_t_ptr)->_data[8 + 2] = (uint8)(((read_length_bytes) >> (2 * 8)) & 0xff); \
        (remote_debug_memory_read_req_t_ptr)->_data[8 + 3] = (uint8)(((read_length_bytes) >> (3 * 8)) & 0xff); } while (0)
#define REMOTE_DEBUG_MEMORY_READ_REQ_T_BYTE_SIZE (12)
/*lint -e(773) allow unparenthesized*/
#define REMOTE_DEBUG_MEMORY_READ_REQ_T_CREATE(subsystem_id, block_id, bytes_per_transaction, transaction_type, address, read_length_bytes) \
    (uint8)(subsystem_id), \
    (uint8)(block_id), \
    (uint8)(bytes_per_transaction), \
    (uint8)(transaction_type), \
    (uint8)((address) & 0xff), \
    (uint8)(((address) >> 8) & 0xff), \
    (uint8)(((address) >> 2 * 8) & 0xff), \
    (uint8)(((address) >> 3 * 8) & 0xff), \
    (uint8)((read_length_bytes) & 0xff), \
    (uint8)(((read_length_bytes) >> 8) & 0xff), \
    (uint8)(((read_length_bytes) >> 2 * 8) & 0xff), \
    (uint8)(((read_length_bytes) >> 3 * 8) & 0xff)
#define REMOTE_DEBUG_MEMORY_READ_REQ_T_PACK(remote_debug_memory_read_req_t_ptr, subsystem_id, block_id, bytes_per_transaction, transaction_type, address, read_length_bytes) \
    do { \
        (remote_debug_memory_read_req_t_ptr)->_data[0] = (uint8)((uint8)(subsystem_id)); \
        (remote_debug_memory_read_req_t_ptr)->_data[1] = (uint8)((uint8)(block_id)); \
        (remote_debug_memory_read_req_t_ptr)->_data[2] = (uint8)((uint8)(bytes_per_transaction)); \
        (remote_debug_memory_read_req_t_ptr)->_data[3] = (uint8)((uint8)(transaction_type)); \
        (remote_debug_memory_read_req_t_ptr)->_data[4] = (uint8)((uint8)((address) & 0xff)); \
        (remote_debug_memory_read_req_t_ptr)->_data[5] = (uint8)(((address) >> 8) & 0xff); \
        (remote_debug_memory_read_req_t_ptr)->_data[4 + 2] = (uint8)(((address) >> (2 * 8)) & 0xff); \
        (remote_debug_memory_read_req_t_ptr)->_data[4 + 3] = (uint8)((((address) >> (3 * 8)) & 0xff)); \
        (remote_debug_memory_read_req_t_ptr)->_data[8] = (uint8)((uint8)((read_length_bytes) & 0xff)); \
        (remote_debug_memory_read_req_t_ptr)->_data[8 + 1] = (uint8)(((read_length_bytes) >> 8) & 0xff); \
        (remote_debug_memory_read_req_t_ptr)->_data[8 + 2] = (uint8)(((read_length_bytes) >> (2 * 8)) & 0xff); \
        (remote_debug_memory_read_req_t_ptr)->_data[8 + 3] = (uint8)((((read_length_bytes) >> (3 * 8)) & 0xff)); \
    } while (0)


/*******************************************************************************

  NAME
    Remote_Debug_MEMORY_READ_RSP_T

  DESCRIPTION

  MEMBERS
    status    -
    device_id -
    data      -

*******************************************************************************/
typedef struct
{
    uint8 _data[3];
} REMOTE_DEBUG_MEMORY_READ_RSP_T;

/* The following macros take REMOTE_DEBUG_MEMORY_READ_RSP_T *remote_debug_memory_read_rsp_t_ptr */
#define REMOTE_DEBUG_MEMORY_READ_RSP_T_STATUS_BYTE_OFFSET (0)
#define REMOTE_DEBUG_MEMORY_READ_RSP_T_STATUS_GET(remote_debug_memory_read_rsp_t_ptr) ((REMOTE_DEBUG_TRB)(remote_debug_memory_read_rsp_t_ptr)->_data[0])
#define REMOTE_DEBUG_MEMORY_READ_RSP_T_STATUS_SET(remote_debug_memory_read_rsp_t_ptr, status) ((remote_debug_memory_read_rsp_t_ptr)->_data[0] = (uint8)(status))
#define REMOTE_DEBUG_MEMORY_READ_RSP_T_DEVICE_ID_BYTE_OFFSET (1)
#define REMOTE_DEBUG_MEMORY_READ_RSP_T_DEVICE_ID_GET(remote_debug_memory_read_rsp_t_ptr) ((REMOTE_DEBUG_DEVICE_ID_T)(remote_debug_memory_read_rsp_t_ptr)->_data[1])
#define REMOTE_DEBUG_MEMORY_READ_RSP_T_DEVICE_ID_SET(remote_debug_memory_read_rsp_t_ptr, device_id) ((remote_debug_memory_read_rsp_t_ptr)->_data[1] = (uint8)(device_id))
#define REMOTE_DEBUG_MEMORY_READ_RSP_T_DATA_BYTE_OFFSET (2)
#define REMOTE_DEBUG_MEMORY_READ_RSP_T_DATA_GET(remote_debug_memory_read_rsp_t_ptr) ((remote_debug_memory_read_rsp_t_ptr)->_data[2])
#define REMOTE_DEBUG_MEMORY_READ_RSP_T_DATA_SET(remote_debug_memory_read_rsp_t_ptr, data) ((remote_debug_memory_read_rsp_t_ptr)->_data[2] = (uint8)(data))
#define REMOTE_DEBUG_MEMORY_READ_RSP_T_BYTE_SIZE (3)
/*lint -e(773) allow unparenthesized*/
#define REMOTE_DEBUG_MEMORY_READ_RSP_T_CREATE(status, device_id, data) \
    (uint8)(status), \
    (uint8)(device_id), \
    (uint8)(data)
#define REMOTE_DEBUG_MEMORY_READ_RSP_T_PACK(remote_debug_memory_read_rsp_t_ptr, status, device_id, data) \
    do { \
        (remote_debug_memory_read_rsp_t_ptr)->_data[0] = (uint8)((uint8)(status)); \
        (remote_debug_memory_read_rsp_t_ptr)->_data[1] = (uint8)((uint8)(device_id)); \
        (remote_debug_memory_read_rsp_t_ptr)->_data[2] = (uint8)((uint8)(data)); \
    } while (0)


/*******************************************************************************

  NAME
    Remote_Debug_MEMORY_WRITE_REQ_T

  DESCRIPTION

  MEMBERS
    subsystem_id          -
    block_id              -
    bytes_per_transaction -
    transaction_type      -
    address               -
    data                  -

*******************************************************************************/
typedef struct
{
    uint8 _data[9];
} REMOTE_DEBUG_MEMORY_WRITE_REQ_T;

/* The following macros take REMOTE_DEBUG_MEMORY_WRITE_REQ_T *remote_debug_memory_write_req_t_ptr */
#define REMOTE_DEBUG_MEMORY_WRITE_REQ_T_SUBSYSTEM_ID_BYTE_OFFSET (0)
#define REMOTE_DEBUG_MEMORY_WRITE_REQ_T_SUBSYSTEM_ID_GET(remote_debug_memory_write_req_t_ptr) ((remote_debug_memory_write_req_t_ptr)->_data[0])
#define REMOTE_DEBUG_MEMORY_WRITE_REQ_T_SUBSYSTEM_ID_SET(remote_debug_memory_write_req_t_ptr, subsystem_id) ((remote_debug_memory_write_req_t_ptr)->_data[0] = (uint8)(subsystem_id))
#define REMOTE_DEBUG_MEMORY_WRITE_REQ_T_BLOCK_ID_BYTE_OFFSET (1)
#define REMOTE_DEBUG_MEMORY_WRITE_REQ_T_BLOCK_ID_GET(remote_debug_memory_write_req_t_ptr) ((remote_debug_memory_write_req_t_ptr)->_data[1])
#define REMOTE_DEBUG_MEMORY_WRITE_REQ_T_BLOCK_ID_SET(remote_debug_memory_write_req_t_ptr, block_id) ((remote_debug_memory_write_req_t_ptr)->_data[1] = (uint8)(block_id))
#define REMOTE_DEBUG_MEMORY_WRITE_REQ_T_BYTES_PER_TRANSACTION_BYTE_OFFSET (2)
#define REMOTE_DEBUG_MEMORY_WRITE_REQ_T_BYTES_PER_TRANSACTION_GET(remote_debug_memory_write_req_t_ptr) ((remote_debug_memory_write_req_t_ptr)->_data[2])
#define REMOTE_DEBUG_MEMORY_WRITE_REQ_T_BYTES_PER_TRANSACTION_SET(remote_debug_memory_write_req_t_ptr, bytes_per_transaction) ((remote_debug_memory_write_req_t_ptr)->_data[2] = (uint8)(bytes_per_transaction))
#define REMOTE_DEBUG_MEMORY_WRITE_REQ_T_TRANSACTION_TYPE_BYTE_OFFSET (3)
#define REMOTE_DEBUG_MEMORY_WRITE_REQ_T_TRANSACTION_TYPE_GET(remote_debug_memory_write_req_t_ptr) ((REMOTE_DEBUG_TR_TYPE)(remote_debug_memory_write_req_t_ptr)->_data[3])
#define REMOTE_DEBUG_MEMORY_WRITE_REQ_T_TRANSACTION_TYPE_SET(remote_debug_memory_write_req_t_ptr, transaction_type) ((remote_debug_memory_write_req_t_ptr)->_data[3] = (uint8)(transaction_type))
#define REMOTE_DEBUG_MEMORY_WRITE_REQ_T_ADDRESS_BYTE_OFFSET (4)
#define REMOTE_DEBUG_MEMORY_WRITE_REQ_T_ADDRESS_GET(remote_debug_memory_write_req_t_ptr)  \
    (((uint32)((remote_debug_memory_write_req_t_ptr)->_data[4]) | \
      ((uint32)((remote_debug_memory_write_req_t_ptr)->_data[5]) << 8) | \
      ((uint32)((remote_debug_memory_write_req_t_ptr)->_data[4 + 2]) << (2 * 8)) | \
      ((uint32)((remote_debug_memory_write_req_t_ptr)->_data[4 + 3]) << (3 * 8))))
#define REMOTE_DEBUG_MEMORY_WRITE_REQ_T_ADDRESS_SET(remote_debug_memory_write_req_t_ptr, address) do { \
        (remote_debug_memory_write_req_t_ptr)->_data[4] = (uint8)((address) & 0xff); \
        (remote_debug_memory_write_req_t_ptr)->_data[5] = (uint8)(((address) >> 8) & 0xff); \
        (remote_debug_memory_write_req_t_ptr)->_data[4 + 2] = (uint8)(((address) >> (2 * 8)) & 0xff); \
        (remote_debug_memory_write_req_t_ptr)->_data[4 + 3] = (uint8)(((address) >> (3 * 8)) & 0xff); } while (0)
#define REMOTE_DEBUG_MEMORY_WRITE_REQ_T_DATA_BYTE_OFFSET (8)
#define REMOTE_DEBUG_MEMORY_WRITE_REQ_T_DATA_GET(remote_debug_memory_write_req_t_ptr) ((remote_debug_memory_write_req_t_ptr)->_data[8])
#define REMOTE_DEBUG_MEMORY_WRITE_REQ_T_DATA_SET(remote_debug_memory_write_req_t_ptr, data) ((remote_debug_memory_write_req_t_ptr)->_data[8] = (uint8)(data))
#define REMOTE_DEBUG_MEMORY_WRITE_REQ_T_BYTE_SIZE (9)
/*lint -e(773) allow unparenthesized*/
#define REMOTE_DEBUG_MEMORY_WRITE_REQ_T_CREATE(subsystem_id, block_id, bytes_per_transaction, transaction_type, address, data) \
    (uint8)(subsystem_id), \
    (uint8)(block_id), \
    (uint8)(bytes_per_transaction), \
    (uint8)(transaction_type), \
    (uint8)((address) & 0xff), \
    (uint8)(((address) >> 8) & 0xff), \
    (uint8)(((address) >> 2 * 8) & 0xff), \
    (uint8)(((address) >> 3 * 8) & 0xff), \
    (uint8)(data)
#define REMOTE_DEBUG_MEMORY_WRITE_REQ_T_PACK(remote_debug_memory_write_req_t_ptr, subsystem_id, block_id, bytes_per_transaction, transaction_type, address, data) \
    do { \
        (remote_debug_memory_write_req_t_ptr)->_data[0] = (uint8)((uint8)(subsystem_id)); \
        (remote_debug_memory_write_req_t_ptr)->_data[1] = (uint8)((uint8)(block_id)); \
        (remote_debug_memory_write_req_t_ptr)->_data[2] = (uint8)((uint8)(bytes_per_transaction)); \
        (remote_debug_memory_write_req_t_ptr)->_data[3] = (uint8)((uint8)(transaction_type)); \
        (remote_debug_memory_write_req_t_ptr)->_data[4] = (uint8)((uint8)((address) & 0xff)); \
        (remote_debug_memory_write_req_t_ptr)->_data[5] = (uint8)(((address) >> 8) & 0xff); \
        (remote_debug_memory_write_req_t_ptr)->_data[4 + 2] = (uint8)(((address) >> (2 * 8)) & 0xff); \
        (remote_debug_memory_write_req_t_ptr)->_data[4 + 3] = (uint8)((((address) >> (3 * 8)) & 0xff)); \
        (remote_debug_memory_write_req_t_ptr)->_data[8] = (uint8)((uint8)(data)); \
    } while (0)


/*******************************************************************************

  NAME
    Remote_Debug_MEMORY_WRITE_RSP_T

  DESCRIPTION

  MEMBERS
    status    -
    device_id -

*******************************************************************************/
typedef struct
{
    uint8 _data[2];
} REMOTE_DEBUG_MEMORY_WRITE_RSP_T;

/* The following macros take REMOTE_DEBUG_MEMORY_WRITE_RSP_T *remote_debug_memory_write_rsp_t_ptr */
#define REMOTE_DEBUG_MEMORY_WRITE_RSP_T_STATUS_BYTE_OFFSET (0)
#define REMOTE_DEBUG_MEMORY_WRITE_RSP_T_STATUS_GET(remote_debug_memory_write_rsp_t_ptr) ((REMOTE_DEBUG_TRB)(remote_debug_memory_write_rsp_t_ptr)->_data[0])
#define REMOTE_DEBUG_MEMORY_WRITE_RSP_T_STATUS_SET(remote_debug_memory_write_rsp_t_ptr, status) ((remote_debug_memory_write_rsp_t_ptr)->_data[0] = (uint8)(status))
#define REMOTE_DEBUG_MEMORY_WRITE_RSP_T_DEVICE_ID_BYTE_OFFSET (1)
#define REMOTE_DEBUG_MEMORY_WRITE_RSP_T_DEVICE_ID_GET(remote_debug_memory_write_rsp_t_ptr) ((REMOTE_DEBUG_DEVICE_ID_T)(remote_debug_memory_write_rsp_t_ptr)->_data[1])
#define REMOTE_DEBUG_MEMORY_WRITE_RSP_T_DEVICE_ID_SET(remote_debug_memory_write_rsp_t_ptr, device_id) ((remote_debug_memory_write_rsp_t_ptr)->_data[1] = (uint8)(device_id))
#define REMOTE_DEBUG_MEMORY_WRITE_RSP_T_BYTE_SIZE (2)
/*lint -e(773) allow unparenthesized*/
#define REMOTE_DEBUG_MEMORY_WRITE_RSP_T_CREATE(status, device_id) \
    (uint8)(status), \
    (uint8)(device_id)
#define REMOTE_DEBUG_MEMORY_WRITE_RSP_T_PACK(remote_debug_memory_write_rsp_t_ptr, status, device_id) \
    do { \
        (remote_debug_memory_write_rsp_t_ptr)->_data[0] = (uint8)((uint8)(status)); \
        (remote_debug_memory_write_rsp_t_ptr)->_data[1] = (uint8)((uint8)(device_id)); \
    } while (0)


/*******************************************************************************

  NAME
    Remote_Debug_PROTOCOL_VERSION_RSP_T

  DESCRIPTION

  MEMBERS
    protocol_version -
    capabilities     -
    device_id        -
    padding          -

*******************************************************************************/
typedef struct
{
    uint8 _data[10];
} REMOTE_DEBUG_PROTOCOL_VERSION_RSP_T;

/* The following macros take REMOTE_DEBUG_PROTOCOL_VERSION_RSP_T *remote_debug_protocol_version_rsp_t_ptr */
#define REMOTE_DEBUG_PROTOCOL_VERSION_RSP_T_PROTOCOL_VERSION_BYTE_OFFSET (0)
#define REMOTE_DEBUG_PROTOCOL_VERSION_RSP_T_PROTOCOL_VERSION_GET(remote_debug_protocol_version_rsp_t_ptr)  \
    ((REMOTE_DEBUG_PROTOCOL_VERSION_T)((uint32)((remote_debug_protocol_version_rsp_t_ptr)->_data[0]) | \
                                       ((uint32)((remote_debug_protocol_version_rsp_t_ptr)->_data[1]) << 8) | \
                                       ((uint32)((remote_debug_protocol_version_rsp_t_ptr)->_data[2]) << (2 * 8)) | \
                                       ((uint32)((remote_debug_protocol_version_rsp_t_ptr)->_data[3]) << (3 * 8))))
#define REMOTE_DEBUG_PROTOCOL_VERSION_RSP_T_PROTOCOL_VERSION_SET(remote_debug_protocol_version_rsp_t_ptr, protocol_version) do { \
        (remote_debug_protocol_version_rsp_t_ptr)->_data[0] = (uint8)((protocol_version) & 0xff); \
        (remote_debug_protocol_version_rsp_t_ptr)->_data[1] = (uint8)(((protocol_version) >> 8) & 0xff); \
        (remote_debug_protocol_version_rsp_t_ptr)->_data[2] = (uint8)(((protocol_version) >> (2 * 8)) & 0xff); \
        (remote_debug_protocol_version_rsp_t_ptr)->_data[3] = (uint8)(((protocol_version) >> (3 * 8)) & 0xff); } while (0)
#define REMOTE_DEBUG_PROTOCOL_VERSION_RSP_T_CAPABILITIES_BYTE_OFFSET (4)
#define REMOTE_DEBUG_PROTOCOL_VERSION_RSP_T_CAPABILITIES_GET(remote_debug_protocol_version_rsp_t_ptr, capabilities_ptr) do {  \
        (capabilities_ptr)->_data[0] = (remote_debug_protocol_version_rsp_t_ptr)->_data[4]; \
        (capabilities_ptr)->_data[1] = (remote_debug_protocol_version_rsp_t_ptr)->_data[5]; \
        (capabilities_ptr)->_data[2] = (remote_debug_protocol_version_rsp_t_ptr)->_data[4 + 2]; \
        (capabilities_ptr)->_data[3] = (remote_debug_protocol_version_rsp_t_ptr)->_data[4 + 3]; } while (0)
#define REMOTE_DEBUG_PROTOCOL_VERSION_RSP_T_CAPABILITIES_SET(remote_debug_protocol_version_rsp_t_ptr, capabilities_ptr) do {  \
        (remote_debug_protocol_version_rsp_t_ptr)->_data[4] = (capabilities_ptr)->_data[0]; \
        (remote_debug_protocol_version_rsp_t_ptr)->_data[5] = (capabilities_ptr)->_data[1]; \
        (remote_debug_protocol_version_rsp_t_ptr)->_data[4 + 2] = (capabilities_ptr)->_data[2]; \
        (remote_debug_protocol_version_rsp_t_ptr)->_data[4 + 3] = (capabilities_ptr)->_data[3]; } while (0)
#define REMOTE_DEBUG_PROTOCOL_VERSION_RSP_T_DEVICE_ID_BYTE_OFFSET (8)
#define REMOTE_DEBUG_PROTOCOL_VERSION_RSP_T_DEVICE_ID_GET(remote_debug_protocol_version_rsp_t_ptr) ((REMOTE_DEBUG_DEVICE_ID_T)(remote_debug_protocol_version_rsp_t_ptr)->_data[8])
#define REMOTE_DEBUG_PROTOCOL_VERSION_RSP_T_DEVICE_ID_SET(remote_debug_protocol_version_rsp_t_ptr, device_id) ((remote_debug_protocol_version_rsp_t_ptr)->_data[8] = (uint8)(device_id))
#define REMOTE_DEBUG_PROTOCOL_VERSION_RSP_T_PADDING_BYTE_OFFSET (9)
#define REMOTE_DEBUG_PROTOCOL_VERSION_RSP_T_PADDING_GET(remote_debug_protocol_version_rsp_t_ptr) ((remote_debug_protocol_version_rsp_t_ptr)->_data[9])
#define REMOTE_DEBUG_PROTOCOL_VERSION_RSP_T_PADDING_SET(remote_debug_protocol_version_rsp_t_ptr, padding) ((remote_debug_protocol_version_rsp_t_ptr)->_data[9] = (uint8)(padding))
#define REMOTE_DEBUG_PROTOCOL_VERSION_RSP_T_BYTE_SIZE (10)
/*lint -e(773) allow unparenthesized*/
#define REMOTE_DEBUG_PROTOCOL_VERSION_RSP_T_CREATE(protocol_version, capabilities, device_id, padding) \
    (uint8)((protocol_version) & 0xff), \
    (uint8)(((protocol_version) >> 8) & 0xff), \
    (uint8)(((protocol_version) >> 2 * 8) & 0xff), \
    (uint8)(((protocol_version) >> 3 * 8) & 0xff), \
    (uint8), \
    (uint8)(device_id), \
    (uint8)(padding)
#define REMOTE_DEBUG_PROTOCOL_VERSION_RSP_T_PACK(remote_debug_protocol_version_rsp_t_ptr, protocol_version, capabilities_ptr, device_id, padding) \
    do { \
        (remote_debug_protocol_version_rsp_t_ptr)->_data[0] = (uint8)((uint8)((protocol_version) & 0xff)); \
        (remote_debug_protocol_version_rsp_t_ptr)->_data[1] = (uint8)(((protocol_version) >> 8) & 0xff); \
        (remote_debug_protocol_version_rsp_t_ptr)->_data[2] = (uint8)(((protocol_version) >> (2 * 8)) & 0xff); \
        (remote_debug_protocol_version_rsp_t_ptr)->_data[3] = (uint8)((((protocol_version) >> (3 * 8)) & 0xff)); \
        (remote_debug_protocol_version_rsp_t_ptr)->_data[4] = (uint8)((capabilities_ptr)->_data[0]); \
        (remote_debug_protocol_version_rsp_t_ptr)->_data[5] = (uint8)((capabilities_ptr)->_data[1]); \
        (remote_debug_protocol_version_rsp_t_ptr)->_data[4 + 2] = (uint8)((capabilities_ptr)->_data[2]); \
        (remote_debug_protocol_version_rsp_t_ptr)->_data[4 + 3] = (uint8)((capabilities_ptr)->_data[3]); \
        (remote_debug_protocol_version_rsp_t_ptr)->_data[8] = (uint8)((uint8)(device_id)); \
        (remote_debug_protocol_version_rsp_t_ptr)->_data[9] = (uint8)((uint8)(padding)); \
    } while (0)


/*******************************************************************************

  NAME
    Remote_Debug_ROUTED_REQ_T

  DESCRIPTION

  MEMBERS
    request_routing -
    routed_type     -
    routed_cmd_id   -
    payload         -

*******************************************************************************/
typedef struct
{
    uint8 _data[16];
} REMOTE_DEBUG_ROUTED_REQ_T;

/* The following macros take REMOTE_DEBUG_ROUTED_REQ_T *remote_debug_routed_req_t_ptr */
#define REMOTE_DEBUG_ROUTED_REQ_T_REQUEST_ROUTING_BYTE_OFFSET (0)
#define REMOTE_DEBUG_ROUTED_REQ_T_REQUEST_ROUTING_GET(remote_debug_routed_req_t_ptr)  \
    ((REMOTE_DEBUG_ROUTED_REQ_ROUTE_T)((uint16)((remote_debug_routed_req_t_ptr)->_data[0]) | \
                                       ((uint16)((remote_debug_routed_req_t_ptr)->_data[1]) << 8)))
#define REMOTE_DEBUG_ROUTED_REQ_T_REQUEST_ROUTING_SET(remote_debug_routed_req_t_ptr, request_routing) do { \
        (remote_debug_routed_req_t_ptr)->_data[0] = (uint8)((request_routing) & 0xff); \
        (remote_debug_routed_req_t_ptr)->_data[1] = (uint8)((request_routing) >> 8); } while (0)
#define REMOTE_DEBUG_ROUTED_REQ_T_ROUTED_TYPE_BYTE_OFFSET (2)
#define REMOTE_DEBUG_ROUTED_REQ_T_ROUTED_TYPE_GET(remote_debug_routed_req_t_ptr) ((REMOTE_DEBUG_CMD_TYPE)(remote_debug_routed_req_t_ptr)->_data[2])
#define REMOTE_DEBUG_ROUTED_REQ_T_ROUTED_TYPE_SET(remote_debug_routed_req_t_ptr, routed_type) ((remote_debug_routed_req_t_ptr)->_data[2] = (uint8)(routed_type))
#define REMOTE_DEBUG_ROUTED_REQ_T_ROUTED_CMD_ID_BYTE_OFFSET (3)
#define REMOTE_DEBUG_ROUTED_REQ_T_ROUTED_CMD_ID_GET(remote_debug_routed_req_t_ptr) ((REMOTE_DEBUG_CMD)(remote_debug_routed_req_t_ptr)->_data[3])
#define REMOTE_DEBUG_ROUTED_REQ_T_ROUTED_CMD_ID_SET(remote_debug_routed_req_t_ptr, routed_cmd_id) ((remote_debug_routed_req_t_ptr)->_data[3] = (uint8)(routed_cmd_id))
#define REMOTE_DEBUG_ROUTED_REQ_T_PAYLOAD_BYTE_OFFSET (4)
#define REMOTE_DEBUG_ROUTED_REQ_T_BYTE_SIZE (16)
/*lint -e(773) allow unparenthesized*/
#define REMOTE_DEBUG_ROUTED_REQ_T_CREATE(request_routing, routed_type, routed_cmd_id) \
    (uint8)((request_routing) & 0xff), \
    (uint8)((request_routing) >> 8), \
    (uint8)(routed_type), \
    (uint8)(routed_cmd_id)
#define REMOTE_DEBUG_ROUTED_REQ_T_PACK(remote_debug_routed_req_t_ptr, request_routing, routed_type, routed_cmd_id) \
    do { \
        (remote_debug_routed_req_t_ptr)->_data[0] = (uint8)((uint8)((request_routing) & 0xff)); \
        (remote_debug_routed_req_t_ptr)->_data[1] = (uint8)(((request_routing) >> 8)); \
        (remote_debug_routed_req_t_ptr)->_data[2] = (uint8)((uint8)(routed_type)); \
        (remote_debug_routed_req_t_ptr)->_data[3] = (uint8)((uint8)(routed_cmd_id)); \
    } while (0)


/*******************************************************************************

  NAME
    Remote_Debug_ROUTED_RESP_T

  DESCRIPTION

  MEMBERS
    response_routing -
    routed_type      -
    routed_cmd_id    -
    payload          -

*******************************************************************************/
typedef struct
{
    uint8 _data[16];
} REMOTE_DEBUG_ROUTED_RESP_T;

/* The following macros take REMOTE_DEBUG_ROUTED_RESP_T *remote_debug_routed_resp_t_ptr */
#define REMOTE_DEBUG_ROUTED_RESP_T_RESPONSE_ROUTING_BYTE_OFFSET (0)
#define REMOTE_DEBUG_ROUTED_RESP_T_RESPONSE_ROUTING_GET(remote_debug_routed_resp_t_ptr)  \
    ((REMOTE_DEBUG_ROUTED_RESP_ROUTE_T)((uint16)((remote_debug_routed_resp_t_ptr)->_data[0]) | \
                                        ((uint16)((remote_debug_routed_resp_t_ptr)->_data[1]) << 8)))
#define REMOTE_DEBUG_ROUTED_RESP_T_RESPONSE_ROUTING_SET(remote_debug_routed_resp_t_ptr, response_routing) do { \
        (remote_debug_routed_resp_t_ptr)->_data[0] = (uint8)((response_routing) & 0xff); \
        (remote_debug_routed_resp_t_ptr)->_data[1] = (uint8)((response_routing) >> 8); } while (0)
#define REMOTE_DEBUG_ROUTED_RESP_T_ROUTED_TYPE_BYTE_OFFSET (2)
#define REMOTE_DEBUG_ROUTED_RESP_T_ROUTED_TYPE_GET(remote_debug_routed_resp_t_ptr) ((REMOTE_DEBUG_CMD_TYPE)(remote_debug_routed_resp_t_ptr)->_data[2])
#define REMOTE_DEBUG_ROUTED_RESP_T_ROUTED_TYPE_SET(remote_debug_routed_resp_t_ptr, routed_type) ((remote_debug_routed_resp_t_ptr)->_data[2] = (uint8)(routed_type))
#define REMOTE_DEBUG_ROUTED_RESP_T_ROUTED_CMD_ID_BYTE_OFFSET (3)
#define REMOTE_DEBUG_ROUTED_RESP_T_ROUTED_CMD_ID_GET(remote_debug_routed_resp_t_ptr) ((REMOTE_DEBUG_CMD)(remote_debug_routed_resp_t_ptr)->_data[3])
#define REMOTE_DEBUG_ROUTED_RESP_T_ROUTED_CMD_ID_SET(remote_debug_routed_resp_t_ptr, routed_cmd_id) ((remote_debug_routed_resp_t_ptr)->_data[3] = (uint8)(routed_cmd_id))
#define REMOTE_DEBUG_ROUTED_RESP_T_PAYLOAD_BYTE_OFFSET (4)
#define REMOTE_DEBUG_ROUTED_RESP_T_BYTE_SIZE (16)
/*lint -e(773) allow unparenthesized*/
#define REMOTE_DEBUG_ROUTED_RESP_T_CREATE(response_routing, routed_type, routed_cmd_id) \
    (uint8)((response_routing) & 0xff), \
    (uint8)((response_routing) >> 8), \
    (uint8)(routed_type), \
    (uint8)(routed_cmd_id)
#define REMOTE_DEBUG_ROUTED_RESP_T_PACK(remote_debug_routed_resp_t_ptr, response_routing, routed_type, routed_cmd_id) \
    do { \
        (remote_debug_routed_resp_t_ptr)->_data[0] = (uint8)((uint8)((response_routing) & 0xff)); \
        (remote_debug_routed_resp_t_ptr)->_data[1] = (uint8)(((response_routing) >> 8)); \
        (remote_debug_routed_resp_t_ptr)->_data[2] = (uint8)((uint8)(routed_type)); \
        (remote_debug_routed_resp_t_ptr)->_data[3] = (uint8)((uint8)(routed_cmd_id)); \
    } while (0)


/*******************************************************************************

  NAME
    Remote_Debug_UNROUTABLE_RESP_T

  DESCRIPTION

  MEMBERS
    response_routing -
    routed_type      -
    routed_cmd_id    -
    payload          -

*******************************************************************************/
typedef struct
{
    uint8 _data[8];
} REMOTE_DEBUG_UNROUTABLE_RESP_T;

/* The following macros take REMOTE_DEBUG_UNROUTABLE_RESP_T *remote_debug_unroutable_resp_t_ptr */
#define REMOTE_DEBUG_UNROUTABLE_RESP_T_REQUEST_ROUTING_BYTE_OFFSET (0)
#define REMOTE_DEBUG_UNROUTABLE_RESP_T_REQUEST_ROUTING_GET(remote_debug_unroutable_resp_t_ptr)  \
    ((REMOTE_DEBUG_UNROUTABLE_RESP_ROUTE_T)((uint16)((remote_debug_unroutable_resp_t_ptr)->_data[0]) | \
                                        ((uint16)((remote_debug_unroutable_resp_t_ptr)->_data[1]) << 8)))
#define REMOTE_DEBUG_UNROUTABLE_RESP_T_REQUEST_ROUTING_SET(remote_debug_unroutable_resp_t_ptr, request_routing) do { \
        (remote_debug_unroutable_resp_t_ptr)->_data[0] = (uint8)((request_routing) & 0xff); \
        (remote_debug_unroutable_resp_t_ptr)->_data[1] = (uint8)((request_routing) >> 8); } while (0)
#define REMOTE_DEBUG_UNROUTABLE_RESP_T_ROUTED_TYPE_BYTE_OFFSET (2)
#define REMOTE_DEBUG_UNROUTABLE_RESP_T_ROUTED_TYPE_GET(remote_debug_unroutable_resp_t_ptr) ((REMOTE_DEBUG_CMD_TYPE)(remote_debug_unroutable_resp_t_ptr)->_data[2])
#define REMOTE_DEBUG_UNROUTABLE_RESP_T_ROUTED_TYPE_SET(remote_debug_unroutable_resp_t_ptr, routed_type) ((remote_debug_unroutable_resp_t_ptr)->_data[2] = (uint8)(routed_type))
#define REMOTE_DEBUG_UNROUTABLE_RESP_T_ROUTED_CMD_ID_BYTE_OFFSET (3)
#define REMOTE_DEBUG_UNROUTABLE_RESP_T_ROUTED_CMD_ID_GET(remote_debug_unroutable_resp_t_ptr) ((REMOTE_DEBUG_CMD)(remote_debug_unroutable_resp_t_ptr)->_data[3])
#define REMOTE_DEBUG_UNROUTABLE_RESP_T_ROUTED_CMD_ID_SET(remote_debug_unroutable_resp_t_ptr, routed_cmd_id) ((remote_debug_unroutable_resp_t_ptr)->_data[3] = (uint8)(routed_cmd_id))
#define REMOTE_DEBUG_UNROUTABLE_RESP_T_RESPONSE_ROUTING_BYTE_OFFSET (4)
#define REMOTE_DEBUG_UNROUTABLE_RESP_T_RESPONSE_ROUTING_GET(remote_debug_unroutable_resp_t_ptr)  \
    ((REMOTE_DEBUG_UNROUTABLE_RESP_ROUTE_T)((uint16)((remote_debug_unroutable_resp_t_ptr)->_data[4]) | \
                                        ((uint16)((remote_debug_unroutable_resp_t_ptr)->_data[5]) << 8)))
#define REMOTE_DEBUG_UNROUTABLE_RESP_T_RESPONSE_ROUTING_SET(remote_debug_unroutable_resp_t_ptr, response_routing) do { \
        (remote_debug_unroutable_resp_t_ptr)->_data[4] = (uint8)((response_routing) & 0xff); \
        (remote_debug_unroutable_resp_t_ptr)->_data[5] = (uint8)((response_routing) >> 8); } while (0)
#define REMOTE_DEBUG_UNROUTABLE_RESP_T_ROUTED_REASON_BYTE_OFFSET (6)
#define REMOTE_DEBUG_UNROUTABLE_RESP_T_ROUTED_REASON_GET(remote_debug_unroutable_resp_t_ptr) ((REMOTE_DEBUG_CMD)(remote_debug_unroutable_resp_t_ptr)->_data[6])
#define REMOTE_DEBUG_UNROUTABLE_RESP_T_ROUTED_REASON_SET(remote_debug_unroutable_resp_t_ptr, routed_cmd_id) ((remote_debug_unroutable_resp_t_ptr)->_data[6] = (uint8)(routed_cmd_id))
#define REMOTE_DEBUG_UNROUTABLE_RESP_T_PAYLOAD_BYTE_OFFSET (7)
#define REMOTE_DEBUG_UNROUTABLE_RESP_T_BYTE_SIZE (8)
/*lint -e(773) allow unparenthesized*/
#define REMOTE_DEBUG_UNROUTABLE_RESP_T_CREATE(request_routing, routed_type, routed_cmd_id, response_routing, reason) \
    (uint8)((request_routing) & 0xff), \
    (uint8)((request_routing) >> 8), \
    (uint8)(routed_type), \
    (uint8)(routed_cmd_id), \
    (uint8)((response_routing) & 0xff), \
    (uint8)((response_routing) >> 8), \
    (uint8)(reason)
#define REMOTE_DEBUG_UNROUTABLE_RESP_T_PACK(remote_debug_unroutable_resp_t_ptr, request_routing, routed_type, routed_cmd_id, response_routing, reason) \
    do { \
        (remote_debug_unroutable_resp_t_ptr)->_data[0] = (uint8)((uint8)((request_routing) & 0xff)); \
        (remote_debug_unroutable_resp_t_ptr)->_data[1] = (uint8)(((request_routing) >> 8)); \
        (remote_debug_unroutable_resp_t_ptr)->_data[2] = (uint8)((uint8)(routed_type)); \
        (remote_debug_unroutable_resp_t_ptr)->_data[3] = (uint8)((uint8)(routed_cmd_id)); \
        (remote_debug_unroutable_resp_t_ptr)->_data[4] = (uint8)((uint8)((response_routing) & 0xff)); \
        (remote_debug_unroutable_resp_t_ptr)->_data[5] = (uint8)(((response_routing) >> 8)); \
        (remote_debug_unroutable_resp_t_ptr)->_data[6] = (uint8)((uint8)(reason)); \
    } while (0)
 

/*******************************************************************************

  NAME
    Remote_Debug_TRANSPORT_VERSION_RSP_T

  DESCRIPTION

  MEMBERS
    transport_type   -
    major_version    -
    minor_version    -
    tertiary_version -

*******************************************************************************/
typedef struct
{
    uint8 _data[16];
} REMOTE_DEBUG_TRANSPORT_VERSION_RSP_T;

/* The following macros take REMOTE_DEBUG_TRANSPORT_VERSION_RSP_T *remote_debug_transport_version_rsp_t_ptr */
#define REMOTE_DEBUG_TRANSPORT_VERSION_RSP_T_TRANSPORT_TYPE_BYTE_OFFSET (0)
#define REMOTE_DEBUG_TRANSPORT_VERSION_RSP_T_TRANSPORT_TYPE_GET(remote_debug_transport_version_rsp_t_ptr)  \
    ((REMOTE_DEBUG_TRANSPORT_TYPE_CODE)((uint32)((remote_debug_transport_version_rsp_t_ptr)->_data[0]) | \
                                        ((uint32)((remote_debug_transport_version_rsp_t_ptr)->_data[1]) << 8) | \
                                        ((uint32)((remote_debug_transport_version_rsp_t_ptr)->_data[2]) << (2 * 8)) | \
                                        ((uint32)((remote_debug_transport_version_rsp_t_ptr)->_data[3]) << (3 * 8))))
#define REMOTE_DEBUG_TRANSPORT_VERSION_RSP_T_TRANSPORT_TYPE_SET(remote_debug_transport_version_rsp_t_ptr, transport_type) do { \
        (remote_debug_transport_version_rsp_t_ptr)->_data[0] = (uint8)((transport_type) & 0xff); \
        (remote_debug_transport_version_rsp_t_ptr)->_data[1] = (uint8)(((transport_type) >> 8) & 0xff); \
        (remote_debug_transport_version_rsp_t_ptr)->_data[2] = (uint8)(((transport_type) >> (2 * 8)) & 0xff); \
        (remote_debug_transport_version_rsp_t_ptr)->_data[3] = (uint8)(((transport_type) >> (3 * 8)) & 0xff); } while (0)
#define REMOTE_DEBUG_TRANSPORT_VERSION_RSP_T_MAJOR_VERSION_BYTE_OFFSET (4)
#define REMOTE_DEBUG_TRANSPORT_VERSION_RSP_T_MAJOR_VERSION_GET(remote_debug_transport_version_rsp_t_ptr)  \
    (((uint32)((remote_debug_transport_version_rsp_t_ptr)->_data[4]) | \
      ((uint32)((remote_debug_transport_version_rsp_t_ptr)->_data[5]) << 8) | \
      ((uint32)((remote_debug_transport_version_rsp_t_ptr)->_data[4 + 2]) << (2 * 8)) | \
      ((uint32)((remote_debug_transport_version_rsp_t_ptr)->_data[4 + 3]) << (3 * 8))))
#define REMOTE_DEBUG_TRANSPORT_VERSION_RSP_T_MAJOR_VERSION_SET(remote_debug_transport_version_rsp_t_ptr, major_version) do { \
        (remote_debug_transport_version_rsp_t_ptr)->_data[4] = (uint8)((major_version) & 0xff); \
        (remote_debug_transport_version_rsp_t_ptr)->_data[5] = (uint8)(((major_version) >> 8) & 0xff); \
        (remote_debug_transport_version_rsp_t_ptr)->_data[4 + 2] = (uint8)(((major_version) >> (2 * 8)) & 0xff); \
        (remote_debug_transport_version_rsp_t_ptr)->_data[4 + 3] = (uint8)(((major_version) >> (3 * 8)) & 0xff); } while (0)
#define REMOTE_DEBUG_TRANSPORT_VERSION_RSP_T_MINOR_VERSION_BYTE_OFFSET (8)
#define REMOTE_DEBUG_TRANSPORT_VERSION_RSP_T_MINOR_VERSION_GET(remote_debug_transport_version_rsp_t_ptr)  \
    (((uint32)((remote_debug_transport_version_rsp_t_ptr)->_data[8]) | \
      ((uint32)((remote_debug_transport_version_rsp_t_ptr)->_data[8 + 1]) << 8) | \
      ((uint32)((remote_debug_transport_version_rsp_t_ptr)->_data[8 + 2]) << (2 * 8)) | \
      ((uint32)((remote_debug_transport_version_rsp_t_ptr)->_data[8 + 3]) << (3 * 8))))
#define REMOTE_DEBUG_TRANSPORT_VERSION_RSP_T_MINOR_VERSION_SET(remote_debug_transport_version_rsp_t_ptr, minor_version) do { \
        (remote_debug_transport_version_rsp_t_ptr)->_data[8] = (uint8)((minor_version) & 0xff); \
        (remote_debug_transport_version_rsp_t_ptr)->_data[8 + 1] = (uint8)(((minor_version) >> 8) & 0xff); \
        (remote_debug_transport_version_rsp_t_ptr)->_data[8 + 2] = (uint8)(((minor_version) >> (2 * 8)) & 0xff); \
        (remote_debug_transport_version_rsp_t_ptr)->_data[8 + 3] = (uint8)(((minor_version) >> (3 * 8)) & 0xff); } while (0)
#define REMOTE_DEBUG_TRANSPORT_VERSION_RSP_T_TERTIARY_VERSION_BYTE_OFFSET (12)
#define REMOTE_DEBUG_TRANSPORT_VERSION_RSP_T_TERTIARY_VERSION_GET(remote_debug_transport_version_rsp_t_ptr)  \
    (((uint32)((remote_debug_transport_version_rsp_t_ptr)->_data[12]) | \
      ((uint32)((remote_debug_transport_version_rsp_t_ptr)->_data[13]) << 8) | \
      ((uint32)((remote_debug_transport_version_rsp_t_ptr)->_data[12 + 2]) << (2 * 8)) | \
      ((uint32)((remote_debug_transport_version_rsp_t_ptr)->_data[12 + 3]) << (3 * 8))))
#define REMOTE_DEBUG_TRANSPORT_VERSION_RSP_T_TERTIARY_VERSION_SET(remote_debug_transport_version_rsp_t_ptr, tertiary_version) do { \
        (remote_debug_transport_version_rsp_t_ptr)->_data[12] = (uint8)((tertiary_version) & 0xff); \
        (remote_debug_transport_version_rsp_t_ptr)->_data[13] = (uint8)(((tertiary_version) >> 8) & 0xff); \
        (remote_debug_transport_version_rsp_t_ptr)->_data[12 + 2] = (uint8)(((tertiary_version) >> (2 * 8)) & 0xff); \
        (remote_debug_transport_version_rsp_t_ptr)->_data[12 + 3] = (uint8)(((tertiary_version) >> (3 * 8)) & 0xff); } while (0)
#define REMOTE_DEBUG_TRANSPORT_VERSION_RSP_T_BYTE_SIZE (16)
/*lint -e(773) allow unparenthesized*/
#define REMOTE_DEBUG_TRANSPORT_VERSION_RSP_T_CREATE(transport_type, major_version, minor_version, tertiary_version) \
    (uint8)((transport_type) & 0xff), \
    (uint8)(((transport_type) >> 8) & 0xff), \
    (uint8)(((transport_type) >> 2 * 8) & 0xff), \
    (uint8)(((transport_type) >> 3 * 8) & 0xff), \
    (uint8)((major_version) & 0xff), \
    (uint8)(((major_version) >> 8) & 0xff), \
    (uint8)(((major_version) >> 2 * 8) & 0xff), \
    (uint8)(((major_version) >> 3 * 8) & 0xff), \
    (uint8)((minor_version) & 0xff), \
    (uint8)(((minor_version) >> 8) & 0xff), \
    (uint8)(((minor_version) >> 2 * 8) & 0xff), \
    (uint8)(((minor_version) >> 3 * 8) & 0xff), \
    (uint8)((tertiary_version) & 0xff), \
    (uint8)(((tertiary_version) >> 8) & 0xff), \
    (uint8)(((tertiary_version) >> 2 * 8) & 0xff), \
    (uint8)(((tertiary_version) >> 3 * 8) & 0xff)
#define REMOTE_DEBUG_TRANSPORT_VERSION_RSP_T_PACK(remote_debug_transport_version_rsp_t_ptr, transport_type, major_version, minor_version, tertiary_version) \
    do { \
        (remote_debug_transport_version_rsp_t_ptr)->_data[0] = (uint8)((uint8)((transport_type) & 0xff)); \
        (remote_debug_transport_version_rsp_t_ptr)->_data[1] = (uint8)(((transport_type) >> 8) & 0xff); \
        (remote_debug_transport_version_rsp_t_ptr)->_data[2] = (uint8)(((transport_type) >> (2 * 8)) & 0xff); \
        (remote_debug_transport_version_rsp_t_ptr)->_data[3] = (uint8)((((transport_type) >> (3 * 8)) & 0xff)); \
        (remote_debug_transport_version_rsp_t_ptr)->_data[4] = (uint8)((uint8)((major_version) & 0xff)); \
        (remote_debug_transport_version_rsp_t_ptr)->_data[5] = (uint8)(((major_version) >> 8) & 0xff); \
        (remote_debug_transport_version_rsp_t_ptr)->_data[4 + 2] = (uint8)(((major_version) >> (2 * 8)) & 0xff); \
        (remote_debug_transport_version_rsp_t_ptr)->_data[4 + 3] = (uint8)((((major_version) >> (3 * 8)) & 0xff)); \
        (remote_debug_transport_version_rsp_t_ptr)->_data[8] = (uint8)((uint8)((minor_version) & 0xff)); \
        (remote_debug_transport_version_rsp_t_ptr)->_data[8 + 1] = (uint8)(((minor_version) >> 8) & 0xff); \
        (remote_debug_transport_version_rsp_t_ptr)->_data[8 + 2] = (uint8)(((minor_version) >> (2 * 8)) & 0xff); \
        (remote_debug_transport_version_rsp_t_ptr)->_data[8 + 3] = (uint8)((((minor_version) >> (3 * 8)) & 0xff)); \
        (remote_debug_transport_version_rsp_t_ptr)->_data[12] = (uint8)((uint8)((tertiary_version) & 0xff)); \
        (remote_debug_transport_version_rsp_t_ptr)->_data[13] = (uint8)(((tertiary_version) >> 8) & 0xff); \
        (remote_debug_transport_version_rsp_t_ptr)->_data[12 + 2] = (uint8)(((tertiary_version) >> (2 * 8)) & 0xff); \
        (remote_debug_transport_version_rsp_t_ptr)->_data[12 + 3] = (uint8)((((tertiary_version) >> (3 * 8)) & 0xff)); \
    } while (0)


/*******************************************************************************

  NAME
    Remote_Debug_UNDELIVERABLE_DEBUG_CMD_RSP_T

  DESCRIPTION

  MEMBERS
    status     -
    type       -
    command_id -
    tag        -

*******************************************************************************/
typedef struct
{
    uint8 _data[8];
} REMOTE_DEBUG_UNDELIVERABLE_DEBUG_CMD_RSP_T;

/* The following macros take REMOTE_DEBUG_UNDELIVERABLE_DEBUG_CMD_RSP_T *remote_debug_undeliverable_debug_cmd_rsp_t_ptr */
#define REMOTE_DEBUG_UNDELIVERABLE_DEBUG_CMD_RSP_T_STATUS_BYTE_OFFSET (0)
#define REMOTE_DEBUG_UNDELIVERABLE_DEBUG_CMD_RSP_T_STATUS_GET(remote_debug_undeliverable_debug_cmd_rsp_t_ptr)  \
    ((REMOTE_DEBUG_UNDELIVERABLE_STATUS)((uint32)((remote_debug_undeliverable_debug_cmd_rsp_t_ptr)->_data[0]) | \
                                         ((uint32)((remote_debug_undeliverable_debug_cmd_rsp_t_ptr)->_data[1]) << 8) | \
                                         ((uint32)((remote_debug_undeliverable_debug_cmd_rsp_t_ptr)->_data[2]) << (2 * 8)) | \
                                         ((uint32)((remote_debug_undeliverable_debug_cmd_rsp_t_ptr)->_data[3]) << (3 * 8))))
#define REMOTE_DEBUG_UNDELIVERABLE_DEBUG_CMD_RSP_T_STATUS_SET(remote_debug_undeliverable_debug_cmd_rsp_t_ptr, status) do { \
        (remote_debug_undeliverable_debug_cmd_rsp_t_ptr)->_data[0] = (uint8)((status) & 0xff); \
        (remote_debug_undeliverable_debug_cmd_rsp_t_ptr)->_data[1] = (uint8)(((status) >> 8) & 0xff); \
        (remote_debug_undeliverable_debug_cmd_rsp_t_ptr)->_data[2] = (uint8)(((status) >> (2 * 8)) & 0xff); \
        (remote_debug_undeliverable_debug_cmd_rsp_t_ptr)->_data[3] = (uint8)(((status) >> (3 * 8)) & 0xff); } while (0)
#define REMOTE_DEBUG_UNDELIVERABLE_DEBUG_CMD_RSP_T_TYPE_BYTE_OFFSET (4)
#define REMOTE_DEBUG_UNDELIVERABLE_DEBUG_CMD_RSP_T_TYPE_GET(remote_debug_undeliverable_debug_cmd_rsp_t_ptr) ((REMOTE_DEBUG_CMD_TYPE)(remote_debug_undeliverable_debug_cmd_rsp_t_ptr)->_data[4])
#define REMOTE_DEBUG_UNDELIVERABLE_DEBUG_CMD_RSP_T_TYPE_SET(remote_debug_undeliverable_debug_cmd_rsp_t_ptr, type) ((remote_debug_undeliverable_debug_cmd_rsp_t_ptr)->_data[4] = (uint8)(type))
#define REMOTE_DEBUG_UNDELIVERABLE_DEBUG_CMD_RSP_T_COMMAND_ID_BYTE_OFFSET (5)
#define REMOTE_DEBUG_UNDELIVERABLE_DEBUG_CMD_RSP_T_COMMAND_ID_GET(remote_debug_undeliverable_debug_cmd_rsp_t_ptr) ((REMOTE_DEBUG_CMD)(remote_debug_undeliverable_debug_cmd_rsp_t_ptr)->_data[5])
#define REMOTE_DEBUG_UNDELIVERABLE_DEBUG_CMD_RSP_T_COMMAND_ID_SET(remote_debug_undeliverable_debug_cmd_rsp_t_ptr, command_id) ((remote_debug_undeliverable_debug_cmd_rsp_t_ptr)->_data[5] = (uint8)(command_id))
#define REMOTE_DEBUG_UNDELIVERABLE_DEBUG_CMD_RSP_T_TAG_BYTE_OFFSET (6)
#define REMOTE_DEBUG_UNDELIVERABLE_DEBUG_CMD_RSP_T_TAG_GET(remote_debug_undeliverable_debug_cmd_rsp_t_ptr)  \
    (((uint16)((remote_debug_undeliverable_debug_cmd_rsp_t_ptr)->_data[6]) | \
      ((uint16)((remote_debug_undeliverable_debug_cmd_rsp_t_ptr)->_data[7]) << 8)))
#define REMOTE_DEBUG_UNDELIVERABLE_DEBUG_CMD_RSP_T_TAG_SET(remote_debug_undeliverable_debug_cmd_rsp_t_ptr, tag) do { \
        (remote_debug_undeliverable_debug_cmd_rsp_t_ptr)->_data[6] = (uint8)((tag) & 0xff); \
        (remote_debug_undeliverable_debug_cmd_rsp_t_ptr)->_data[7] = (uint8)((tag) >> 8); } while (0)
#define REMOTE_DEBUG_UNDELIVERABLE_DEBUG_CMD_RSP_T_BYTE_SIZE (8)
/*lint -e(773) allow unparenthesized*/
#define REMOTE_DEBUG_UNDELIVERABLE_DEBUG_CMD_RSP_T_CREATE(status, type, command_id, tag) \
    (uint8)((status) & 0xff), \
    (uint8)(((status) >> 8) & 0xff), \
    (uint8)(((status) >> 2 * 8) & 0xff), \
    (uint8)(((status) >> 3 * 8) & 0xff), \
    (uint8)(type), \
    (uint8)(command_id), \
    (uint8)((tag) & 0xff), \
    (uint8)((tag) >> 8)
#define REMOTE_DEBUG_UNDELIVERABLE_DEBUG_CMD_RSP_T_PACK(remote_debug_undeliverable_debug_cmd_rsp_t_ptr, status, type, command_id, tag) \
    do { \
        (remote_debug_undeliverable_debug_cmd_rsp_t_ptr)->_data[0] = (uint8)((uint8)((status) & 0xff)); \
        (remote_debug_undeliverable_debug_cmd_rsp_t_ptr)->_data[1] = (uint8)(((status) >> 8) & 0xff); \
        (remote_debug_undeliverable_debug_cmd_rsp_t_ptr)->_data[2] = (uint8)(((status) >> (2 * 8)) & 0xff); \
        (remote_debug_undeliverable_debug_cmd_rsp_t_ptr)->_data[3] = (uint8)((((status) >> (3 * 8)) & 0xff)); \
        (remote_debug_undeliverable_debug_cmd_rsp_t_ptr)->_data[4] = (uint8)((uint8)(type)); \
        (remote_debug_undeliverable_debug_cmd_rsp_t_ptr)->_data[5] = (uint8)((uint8)(command_id)); \
        (remote_debug_undeliverable_debug_cmd_rsp_t_ptr)->_data[6] = (uint8)((uint8)((tag) & 0xff)); \
        (remote_debug_undeliverable_debug_cmd_rsp_t_ptr)->_data[7] = (uint8)(((tag) >> 8)); \
    } while (0)


/*******************************************************************************

  NAME
    Remote_Debug_device_address_t

  DESCRIPTION

  MEMBERS
    address_length -
    address_string -

*******************************************************************************/
typedef struct
{
    uint8 _data[2];
} REMOTE_DEBUG_DEVICE_ADDRESS_T;

/* The following macros take REMOTE_DEBUG_DEVICE_ADDRESS_T *remote_debug_device_address_t_ptr */
#define REMOTE_DEBUG_DEVICE_ADDRESS_T_ADDRESS_LENGTH_BYTE_OFFSET (0)
#define REMOTE_DEBUG_DEVICE_ADDRESS_T_ADDRESS_LENGTH_GET(remote_debug_device_address_t_ptr) ((remote_debug_device_address_t_ptr)->_data[0])
#define REMOTE_DEBUG_DEVICE_ADDRESS_T_ADDRESS_LENGTH_SET(remote_debug_device_address_t_ptr, address_length) ((remote_debug_device_address_t_ptr)->_data[0] = (uint8)(address_length))
#define REMOTE_DEBUG_DEVICE_ADDRESS_T_ADDRESS_STRING_BYTE_OFFSET (1)
#define REMOTE_DEBUG_DEVICE_ADDRESS_T_ADDRESS_STRING_GET(remote_debug_device_address_t_ptr) ((remote_debug_device_address_t_ptr)->_data[1])
#define REMOTE_DEBUG_DEVICE_ADDRESS_T_ADDRESS_STRING_SET(remote_debug_device_address_t_ptr, address_string) ((remote_debug_device_address_t_ptr)->_data[1] = (uint8)(address_string))
#define REMOTE_DEBUG_DEVICE_ADDRESS_T_BYTE_SIZE (2)
/*lint -e(773) allow unparenthesized*/
#define REMOTE_DEBUG_DEVICE_ADDRESS_T_CREATE(address_length, address_string) \
    (uint8)(address_length), \
    (uint8)(address_string)
#define REMOTE_DEBUG_DEVICE_ADDRESS_T_PACK(remote_debug_device_address_t_ptr, address_length, address_string) \
    do { \
        (remote_debug_device_address_t_ptr)->_data[0] = (uint8)((uint8)(address_length)); \
        (remote_debug_device_address_t_ptr)->_data[1] = (uint8)((uint8)(address_string)); \
    } while (0)


/*******************************************************************************

  NAME
    Remote_Debug_AVAILABLE_DEVICES_RSP_T

  DESCRIPTION

  MEMBERS
    devices -

*******************************************************************************/
typedef struct
{
    uint8 _data[2];
} REMOTE_DEBUG_AVAILABLE_DEVICES_RSP_T;

/* The following macros take REMOTE_DEBUG_AVAILABLE_DEVICES_RSP_T *remote_debug_available_devices_rsp_t_ptr */
#define REMOTE_DEBUG_AVAILABLE_DEVICES_RSP_T_DEVICES_BYTE_OFFSET (0)
#define REMOTE_DEBUG_AVAILABLE_DEVICES_RSP_T_DEVICES_GET(remote_debug_available_devices_rsp_t_ptr, devices_ptr) do {  \
        (devices_ptr)->_data[0] = (remote_debug_available_devices_rsp_t_ptr)->_data[0]; \
        (devices_ptr)->_data[1] = (remote_debug_available_devices_rsp_t_ptr)->_data[1]; } while (0)
#define REMOTE_DEBUG_AVAILABLE_DEVICES_RSP_T_DEVICES_SET(remote_debug_available_devices_rsp_t_ptr, devices_ptr) do {  \
        (remote_debug_available_devices_rsp_t_ptr)->_data[0] = (devices_ptr)->_data[0]; \
        (remote_debug_available_devices_rsp_t_ptr)->_data[1] = (devices_ptr)->_data[1]; } while (0)
#define REMOTE_DEBUG_AVAILABLE_DEVICES_RSP_T_BYTE_SIZE (2)
/*lint -e(773) allow unparenthesized*/
#define REMOTE_DEBUG_AVAILABLE_DEVICES_RSP_T_CREATE(devices) \
    (uint8)
#define REMOTE_DEBUG_AVAILABLE_DEVICES_RSP_T_PACK(remote_debug_available_devices_rsp_t_ptr, devices_ptr) \
    do { \
        (remote_debug_available_devices_rsp_t_ptr)->_data[0] = (uint8)((devices_ptr)->_data[0]); \
        (remote_debug_available_devices_rsp_t_ptr)->_data[1] = (uint8)((devices_ptr)->_data[1]); \
    } while (0)


/*******************************************************************************

  NAME
    Remote_Debug_CONNECTION_INFO_RSP_T

  DESCRIPTION

  MEMBERS
    status                     -
    connection_up_time_seconds -
    address                    -

*******************************************************************************/
typedef struct
{
    uint8 _data[10];
} REMOTE_DEBUG_CONNECTION_INFO_RSP_T;

/* The following macros take REMOTE_DEBUG_CONNECTION_INFO_RSP_T *remote_debug_connection_info_rsp_t_ptr */
#define REMOTE_DEBUG_CONNECTION_INFO_RSP_T_STATUS_BYTE_OFFSET (0)
#define REMOTE_DEBUG_CONNECTION_INFO_RSP_T_STATUS_GET(remote_debug_connection_info_rsp_t_ptr)  \
    ((REMOTE_DEBUG_CONNECTION_STATUS)((uint32)((remote_debug_connection_info_rsp_t_ptr)->_data[0]) | \
                                      ((uint32)((remote_debug_connection_info_rsp_t_ptr)->_data[1]) << 8) | \
                                      ((uint32)((remote_debug_connection_info_rsp_t_ptr)->_data[2]) << (2 * 8)) | \
                                      ((uint32)((remote_debug_connection_info_rsp_t_ptr)->_data[3]) << (3 * 8))))
#define REMOTE_DEBUG_CONNECTION_INFO_RSP_T_STATUS_SET(remote_debug_connection_info_rsp_t_ptr, status) do { \
        (remote_debug_connection_info_rsp_t_ptr)->_data[0] = (uint8)((status) & 0xff); \
        (remote_debug_connection_info_rsp_t_ptr)->_data[1] = (uint8)(((status) >> 8) & 0xff); \
        (remote_debug_connection_info_rsp_t_ptr)->_data[2] = (uint8)(((status) >> (2 * 8)) & 0xff); \
        (remote_debug_connection_info_rsp_t_ptr)->_data[3] = (uint8)(((status) >> (3 * 8)) & 0xff); } while (0)
#define REMOTE_DEBUG_CONNECTION_INFO_RSP_T_CONNECTION_UP_TIME_SECONDS_BYTE_OFFSET (4)
#define REMOTE_DEBUG_CONNECTION_INFO_RSP_T_CONNECTION_UP_TIME_SECONDS_GET(remote_debug_connection_info_rsp_t_ptr)  \
    (((uint32)((remote_debug_connection_info_rsp_t_ptr)->_data[4]) | \
      ((uint32)((remote_debug_connection_info_rsp_t_ptr)->_data[5]) << 8) | \
      ((uint32)((remote_debug_connection_info_rsp_t_ptr)->_data[4 + 2]) << (2 * 8)) | \
      ((uint32)((remote_debug_connection_info_rsp_t_ptr)->_data[4 + 3]) << (3 * 8))))
#define REMOTE_DEBUG_CONNECTION_INFO_RSP_T_CONNECTION_UP_TIME_SECONDS_SET(remote_debug_connection_info_rsp_t_ptr, connection_up_time_seconds) do { \
        (remote_debug_connection_info_rsp_t_ptr)->_data[4] = (uint8)((connection_up_time_seconds) & 0xff); \
        (remote_debug_connection_info_rsp_t_ptr)->_data[5] = (uint8)(((connection_up_time_seconds) >> 8) & 0xff); \
        (remote_debug_connection_info_rsp_t_ptr)->_data[4 + 2] = (uint8)(((connection_up_time_seconds) >> (2 * 8)) & 0xff); \
        (remote_debug_connection_info_rsp_t_ptr)->_data[4 + 3] = (uint8)(((connection_up_time_seconds) >> (3 * 8)) & 0xff); } while (0)
#define REMOTE_DEBUG_CONNECTION_INFO_RSP_T_ADDRESS_BYTE_OFFSET (8)
#define REMOTE_DEBUG_CONNECTION_INFO_RSP_T_ADDRESS_GET(remote_debug_connection_info_rsp_t_ptr, address_ptr) do {  \
        (address_ptr)->_data[0] = (remote_debug_connection_info_rsp_t_ptr)->_data[8]; \
        (address_ptr)->_data[1] = (remote_debug_connection_info_rsp_t_ptr)->_data[8 + 1]; } while (0)
#define REMOTE_DEBUG_CONNECTION_INFO_RSP_T_ADDRESS_SET(remote_debug_connection_info_rsp_t_ptr, address_ptr) do {  \
        (remote_debug_connection_info_rsp_t_ptr)->_data[8] = (address_ptr)->_data[0]; \
        (remote_debug_connection_info_rsp_t_ptr)->_data[8 + 1] = (address_ptr)->_data[1]; } while (0)
#define REMOTE_DEBUG_CONNECTION_INFO_RSP_T_BYTE_SIZE (10)
/*lint -e(773) allow unparenthesized*/
#define REMOTE_DEBUG_CONNECTION_INFO_RSP_T_CREATE(status, connection_up_time_seconds, address) \
    (uint8)((status) & 0xff), \
    (uint8)(((status) >> 8) & 0xff), \
    (uint8)(((status) >> 2 * 8) & 0xff), \
    (uint8)(((status) >> 3 * 8) & 0xff), \
    (uint8)((connection_up_time_seconds) & 0xff), \
    (uint8)(((connection_up_time_seconds) >> 8) & 0xff), \
    (uint8)(((connection_up_time_seconds) >> 2 * 8) & 0xff), \
    (uint8)(((connection_up_time_seconds) >> 3 * 8) & 0xff), \
    (uint8)
#define REMOTE_DEBUG_CONNECTION_INFO_RSP_T_PACK(remote_debug_connection_info_rsp_t_ptr, status, connection_up_time_seconds, address_ptr) \
    do { \
        (remote_debug_connection_info_rsp_t_ptr)->_data[0] = (uint8)((uint8)((status) & 0xff)); \
        (remote_debug_connection_info_rsp_t_ptr)->_data[1] = (uint8)(((status) >> 8) & 0xff); \
        (remote_debug_connection_info_rsp_t_ptr)->_data[2] = (uint8)(((status) >> (2 * 8)) & 0xff); \
        (remote_debug_connection_info_rsp_t_ptr)->_data[3] = (uint8)((((status) >> (3 * 8)) & 0xff)); \
        (remote_debug_connection_info_rsp_t_ptr)->_data[4] = (uint8)((uint8)((connection_up_time_seconds) & 0xff)); \
        (remote_debug_connection_info_rsp_t_ptr)->_data[5] = (uint8)(((connection_up_time_seconds) >> 8) & 0xff); \
        (remote_debug_connection_info_rsp_t_ptr)->_data[4 + 2] = (uint8)(((connection_up_time_seconds) >> (2 * 8)) & 0xff); \
        (remote_debug_connection_info_rsp_t_ptr)->_data[4 + 3] = (uint8)((((connection_up_time_seconds) >> (3 * 8)) & 0xff)); \
        (remote_debug_connection_info_rsp_t_ptr)->_data[8] = (uint8)((address_ptr)->_data[0]); \
        (remote_debug_connection_info_rsp_t_ptr)->_data[8 + 1] = (uint8)((address_ptr)->_data[1]); \
    } while (0)


/*******************************************************************************

  NAME
    Remote_Debug_CONNECT_REQ_T

  DESCRIPTION

  MEMBERS
    device -

*******************************************************************************/
typedef struct
{
    uint8 _data[2];
} REMOTE_DEBUG_CONNECT_REQ_T;

/* The following macros take REMOTE_DEBUG_CONNECT_REQ_T *remote_debug_connect_req_t_ptr */
#define REMOTE_DEBUG_CONNECT_REQ_T_DEVICE_BYTE_OFFSET (0)
#define REMOTE_DEBUG_CONNECT_REQ_T_DEVICE_GET(remote_debug_connect_req_t_ptr, device_ptr) do {  \
        (device_ptr)->_data[0] = (remote_debug_connect_req_t_ptr)->_data[0]; \
        (device_ptr)->_data[1] = (remote_debug_connect_req_t_ptr)->_data[1]; } while (0)
#define REMOTE_DEBUG_CONNECT_REQ_T_DEVICE_SET(remote_debug_connect_req_t_ptr, device_ptr) do {  \
        (remote_debug_connect_req_t_ptr)->_data[0] = (device_ptr)->_data[0]; \
        (remote_debug_connect_req_t_ptr)->_data[1] = (device_ptr)->_data[1]; } while (0)
#define REMOTE_DEBUG_CONNECT_REQ_T_BYTE_SIZE (2)
/*lint -e(773) allow unparenthesized*/
#define REMOTE_DEBUG_CONNECT_REQ_T_CREATE(device) \
    (uint8)
#define REMOTE_DEBUG_CONNECT_REQ_T_PACK(remote_debug_connect_req_t_ptr, device_ptr) \
    do { \
        (remote_debug_connect_req_t_ptr)->_data[0] = (uint8)((device_ptr)->_data[0]); \
        (remote_debug_connect_req_t_ptr)->_data[1] = (uint8)((device_ptr)->_data[1]); \
    } while (0)


/*******************************************************************************

  NAME
    Remote_Debug_DEBUG_CMD_PAYLOAD

  DESCRIPTION

  MEMBERS
    Debug_Command  -
    Payload_Length -
    Tag            -
    payload        -

*******************************************************************************/
typedef struct
{
    uint8 _data[17];
} REMOTE_DEBUG_DEBUG_CMD_PAYLOAD;

/* The following macros take REMOTE_DEBUG_DEBUG_CMD_PAYLOAD *remote_debug_debug_cmd_payload_ptr */
#define REMOTE_DEBUG_DEBUG_CMD_PAYLOAD_DEBUG_COMMAND_BYTE_OFFSET (0)
#define REMOTE_DEBUG_DEBUG_CMD_PAYLOAD_DEBUG_COMMAND_GET(remote_debug_debug_cmd_payload_ptr) ((REMOTE_DEBUG_CMD)(remote_debug_debug_cmd_payload_ptr)->_data[0])
#define REMOTE_DEBUG_DEBUG_CMD_PAYLOAD_DEBUG_COMMAND_SET(remote_debug_debug_cmd_payload_ptr, debug_command) ((remote_debug_debug_cmd_payload_ptr)->_data[0] = (uint8)(debug_command))
#define REMOTE_DEBUG_DEBUG_CMD_PAYLOAD_PAYLOAD_LENGTH_BYTE_OFFSET (1)
#define REMOTE_DEBUG_DEBUG_CMD_PAYLOAD_PAYLOAD_LENGTH_GET(remote_debug_debug_cmd_payload_ptr)  \
    (((uint16)((remote_debug_debug_cmd_payload_ptr)->_data[1]) | \
      ((uint16)((remote_debug_debug_cmd_payload_ptr)->_data[2]) << 8)))
#define REMOTE_DEBUG_DEBUG_CMD_PAYLOAD_PAYLOAD_LENGTH_SET(remote_debug_debug_cmd_payload_ptr, payload_length) do { \
        (remote_debug_debug_cmd_payload_ptr)->_data[1] = (uint8)((payload_length) & 0xff); \
        (remote_debug_debug_cmd_payload_ptr)->_data[2] = (uint8)((payload_length) >> 8); } while (0)
#define REMOTE_DEBUG_DEBUG_CMD_PAYLOAD_TAG_BYTE_OFFSET (3)
#define REMOTE_DEBUG_DEBUG_CMD_PAYLOAD_TAG_GET(remote_debug_debug_cmd_payload_ptr)  \
    (((uint16)((remote_debug_debug_cmd_payload_ptr)->_data[3]) | \
      ((uint16)((remote_debug_debug_cmd_payload_ptr)->_data[4]) << 8)))
#define REMOTE_DEBUG_DEBUG_CMD_PAYLOAD_TAG_SET(remote_debug_debug_cmd_payload_ptr, tag) do { \
        (remote_debug_debug_cmd_payload_ptr)->_data[3] = (uint8)((tag) & 0xff); \
        (remote_debug_debug_cmd_payload_ptr)->_data[4] = (uint8)((tag) >> 8); } while (0)
#define REMOTE_DEBUG_DEBUG_CMD_PAYLOAD_PAYLOAD_BYTE_OFFSET (5)
#define REMOTE_DEBUG_DEBUG_CMD_PAYLOAD_BYTE_SIZE (17)
/*lint -e(773) allow unparenthesized*/
#define REMOTE_DEBUG_DEBUG_CMD_PAYLOAD_CREATE(Debug_Command, Payload_Length, Tag) \
    (uint8)(Debug_Command), \
    (uint8)((Payload_Length) & 0xff), \
    (uint8)((Payload_Length) >> 8), \
    (uint8)((Tag) & 0xff), \
    (uint8)((Tag) >> 8)
#define REMOTE_DEBUG_DEBUG_CMD_PAYLOAD_PACK(remote_debug_debug_cmd_payload_ptr, Debug_Command, Payload_Length, Tag) \
    do { \
        (remote_debug_debug_cmd_payload_ptr)->_data[0] = (uint8)((uint8)(Debug_Command)); \
        (remote_debug_debug_cmd_payload_ptr)->_data[1] = (uint8)((uint8)((Payload_Length) & 0xff)); \
        (remote_debug_debug_cmd_payload_ptr)->_data[2] = (uint8)(((Payload_Length) >> 8)); \
        (remote_debug_debug_cmd_payload_ptr)->_data[3] = (uint8)((uint8)((Tag) & 0xff)); \
        (remote_debug_debug_cmd_payload_ptr)->_data[4] = (uint8)(((Tag) >> 8)); \
    } while (0)


/*******************************************************************************

  NAME
    Remote_Debug_ROUTED_CMD_PAYLOAD

  DESCRIPTION

  MEMBERS
    Route_Command  -
    Payload_Length -
    Tag            -
    payload        -

*******************************************************************************/
typedef struct
{
    uint8 _data[21];
} REMOTE_DEBUG_ROUTED_CMD_PAYLOAD;

/* The following macros take REMOTE_DEBUG_ROUTED_CMD_PAYLOAD *remote_debug_routed_cmd_payload_ptr */
#define REMOTE_DEBUG_ROUTED_CMD_PAYLOAD_ROUTE_COMMAND_BYTE_OFFSET (0)
#define REMOTE_DEBUG_ROUTED_CMD_PAYLOAD_ROUTE_COMMAND_GET(remote_debug_routed_cmd_payload_ptr) ((REMOTE_DEBUG_ROUTE_CMD)(remote_debug_routed_cmd_payload_ptr)->_data[0])
#define REMOTE_DEBUG_ROUTED_CMD_PAYLOAD_ROUTE_COMMAND_SET(remote_debug_routed_cmd_payload_ptr, route_command) ((remote_debug_routed_cmd_payload_ptr)->_data[0] = (uint8)(route_command))
#define REMOTE_DEBUG_ROUTED_CMD_PAYLOAD_PAYLOAD_LENGTH_BYTE_OFFSET (1)
#define REMOTE_DEBUG_ROUTED_CMD_PAYLOAD_PAYLOAD_LENGTH_GET(remote_debug_routed_cmd_payload_ptr)  \
    (((uint16)((remote_debug_routed_cmd_payload_ptr)->_data[1]) | \
      ((uint16)((remote_debug_routed_cmd_payload_ptr)->_data[2]) << 8)))
#define REMOTE_DEBUG_ROUTED_CMD_PAYLOAD_PAYLOAD_LENGTH_SET(remote_debug_routed_cmd_payload_ptr, payload_length) do { \
        (remote_debug_routed_cmd_payload_ptr)->_data[1] = (uint8)((payload_length) & 0xff); \
        (remote_debug_routed_cmd_payload_ptr)->_data[2] = (uint8)((payload_length) >> 8); } while (0)
#define REMOTE_DEBUG_ROUTED_CMD_PAYLOAD_TAG_BYTE_OFFSET (3)
#define REMOTE_DEBUG_ROUTED_CMD_PAYLOAD_TAG_GET(remote_debug_routed_cmd_payload_ptr)  \
    (((uint16)((remote_debug_routed_cmd_payload_ptr)->_data[3]) | \
      ((uint16)((remote_debug_routed_cmd_payload_ptr)->_data[4]) << 8)))
#define REMOTE_DEBUG_ROUTED_CMD_PAYLOAD_TAG_SET(remote_debug_routed_cmd_payload_ptr, tag) do { \
        (remote_debug_routed_cmd_payload_ptr)->_data[3] = (uint8)((tag) & 0xff); \
        (remote_debug_routed_cmd_payload_ptr)->_data[4] = (uint8)((tag) >> 8); } while (0)
#define REMOTE_DEBUG_ROUTED_CMD_PAYLOAD_PAYLOAD_BYTE_OFFSET (5)
#define REMOTE_DEBUG_ROUTED_CMD_PAYLOAD_BYTE_SIZE (21)
/*lint -e(773) allow unparenthesized*/
#define REMOTE_DEBUG_ROUTED_CMD_PAYLOAD_CREATE(Route_Command, Payload_Length, Tag) \
    (uint8)(Route_Command), \
    (uint8)((Payload_Length) & 0xff), \
    (uint8)((Payload_Length) >> 8), \
    (uint8)((Tag) & 0xff), \
    (uint8)((Tag) >> 8)
#define REMOTE_DEBUG_ROUTED_CMD_PAYLOAD_PACK(remote_debug_routed_cmd_payload_ptr, Route_Command, Payload_Length, Tag) \
    do { \
        (remote_debug_routed_cmd_payload_ptr)->_data[0] = (uint8)((uint8)(Route_Command)); \
        (remote_debug_routed_cmd_payload_ptr)->_data[1] = (uint8)((uint8)((Payload_Length) & 0xff)); \
        (remote_debug_routed_cmd_payload_ptr)->_data[2] = (uint8)(((Payload_Length) >> 8)); \
        (remote_debug_routed_cmd_payload_ptr)->_data[3] = (uint8)((uint8)((Tag) & 0xff)); \
        (remote_debug_routed_cmd_payload_ptr)->_data[4] = (uint8)(((Tag) >> 8)); \
    } while (0)


/*******************************************************************************

  NAME
    Remote_Debug_TRANSPORT_CMD_PAYLOAD

  DESCRIPTION

  MEMBERS
    Transport_Command -
    Payload_Length    -
    Tag               -
    payload           -

*******************************************************************************/
typedef struct
{
    uint8 _data[21];
} REMOTE_DEBUG_TRANSPORT_CMD_PAYLOAD;

/* The following macros take REMOTE_DEBUG_TRANSPORT_CMD_PAYLOAD *remote_debug_transport_cmd_payload_ptr */
#define REMOTE_DEBUG_TRANSPORT_CMD_PAYLOAD_TRANSPORT_COMMAND_BYTE_OFFSET (0)
#define REMOTE_DEBUG_TRANSPORT_CMD_PAYLOAD_TRANSPORT_COMMAND_GET(remote_debug_transport_cmd_payload_ptr) ((REMOTE_DEBUG_TR_CMD)(remote_debug_transport_cmd_payload_ptr)->_data[0])
#define REMOTE_DEBUG_TRANSPORT_CMD_PAYLOAD_TRANSPORT_COMMAND_SET(remote_debug_transport_cmd_payload_ptr, transport_command) ((remote_debug_transport_cmd_payload_ptr)->_data[0] = (uint8)(transport_command))
#define REMOTE_DEBUG_TRANSPORT_CMD_PAYLOAD_PAYLOAD_LENGTH_BYTE_OFFSET (1)
#define REMOTE_DEBUG_TRANSPORT_CMD_PAYLOAD_PAYLOAD_LENGTH_GET(remote_debug_transport_cmd_payload_ptr)  \
    (((uint16)((remote_debug_transport_cmd_payload_ptr)->_data[1]) | \
      ((uint16)((remote_debug_transport_cmd_payload_ptr)->_data[2]) << 8)))
#define REMOTE_DEBUG_TRANSPORT_CMD_PAYLOAD_PAYLOAD_LENGTH_SET(remote_debug_transport_cmd_payload_ptr, payload_length) do { \
        (remote_debug_transport_cmd_payload_ptr)->_data[1] = (uint8)((payload_length) & 0xff); \
        (remote_debug_transport_cmd_payload_ptr)->_data[2] = (uint8)((payload_length) >> 8); } while (0)
#define REMOTE_DEBUG_TRANSPORT_CMD_PAYLOAD_TAG_BYTE_OFFSET (3)
#define REMOTE_DEBUG_TRANSPORT_CMD_PAYLOAD_TAG_GET(remote_debug_transport_cmd_payload_ptr)  \
    (((uint16)((remote_debug_transport_cmd_payload_ptr)->_data[3]) | \
      ((uint16)((remote_debug_transport_cmd_payload_ptr)->_data[4]) << 8)))
#define REMOTE_DEBUG_TRANSPORT_CMD_PAYLOAD_TAG_SET(remote_debug_transport_cmd_payload_ptr, tag) do { \
        (remote_debug_transport_cmd_payload_ptr)->_data[3] = (uint8)((tag) & 0xff); \
        (remote_debug_transport_cmd_payload_ptr)->_data[4] = (uint8)((tag) >> 8); } while (0)
#define REMOTE_DEBUG_TRANSPORT_CMD_PAYLOAD_PAYLOAD_BYTE_OFFSET (5)
#define REMOTE_DEBUG_TRANSPORT_CMD_PAYLOAD_BYTE_SIZE (21)
/*lint -e(773) allow unparenthesized*/
#define REMOTE_DEBUG_TRANSPORT_CMD_PAYLOAD_CREATE(Transport_Command, Payload_Length, Tag) \
    (uint8)(Transport_Command), \
    (uint8)((Payload_Length) & 0xff), \
    (uint8)((Payload_Length) >> 8), \
    (uint8)((Tag) & 0xff), \
    (uint8)((Tag) >> 8)
#define REMOTE_DEBUG_TRANSPORT_CMD_PAYLOAD_PACK(remote_debug_transport_cmd_payload_ptr, Transport_Command, Payload_Length, Tag) \
    do { \
        (remote_debug_transport_cmd_payload_ptr)->_data[0] = (uint8)((uint8)(Transport_Command)); \
        (remote_debug_transport_cmd_payload_ptr)->_data[1] = (uint8)((uint8)((Payload_Length) & 0xff)); \
        (remote_debug_transport_cmd_payload_ptr)->_data[2] = (uint8)(((Payload_Length) >> 8)); \
        (remote_debug_transport_cmd_payload_ptr)->_data[3] = (uint8)((uint8)((Tag) & 0xff)); \
        (remote_debug_transport_cmd_payload_ptr)->_data[4] = (uint8)(((Tag) >> 8)); \
    } while (0)


#endif /* REMOTE_DEBUG_PRIM_H__ */


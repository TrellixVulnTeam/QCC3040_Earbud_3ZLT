/*****************************************************************
Copyright (c) 2011 - 2020 Qualcomm Technologies International, Ltd.
*/

#ifndef _GAIA_PRIVATE_H_
#define _GAIA_PRIVATE_H_

#include <panic.h>
#include "gaia.h"

#define MESSAGE_PMAKE(NAME,TYPE) TYPE * const NAME = PanicUnlessNew(TYPE)

#ifdef DEBUG_GAIA
#include <panic.h>
#include <logging.h>
#define GAIA_DEBUG(x) DEBUG_LOG x
#define GAIA_PANIC() Panic()
#else
#define GAIA_DEBUG(x)
#define GAIA_PANIC()
#endif


#define ULBIT(x) (1UL << (x))


#define GAIA_VERSION_01 (1)
#define GAIA_VERSION_03 (3)
#define GAIA_VERSION    (GAIA_VERSION_03)

#define GAIA_INVALID_ID (0xFFFF)

#define GAIA_API_VERSION_MINOR_MAX (15)





/*  Helper macros  */
/*************************************************************************
NAME
    send_simple_command

DESCRIPTION
    Build and send a Gaia command packet
*/
#define send_simple_command(transport, command, size_payload, payload) \
    GaiaSendPacket((GAIA_TRANSPORT *)(transport), \
                   GAIA_VENDOR_QTIL, command, GAIA_STATUS_NONE, \
                   size_payload, (uint8 *) payload)


#define send_response(transport, vendor, command, status, length, payload) \
    GaiaSendPacket((GAIA_TRANSPORT *)(transport), vendor, command, status, length, payload)


/*************************************************************************
NAME
    send_simple_response

DESCRIPTION
    Build and send a Gaia acknowledgement packet
*/
#define send_simple_response(transport, command, status) \
    send_ack(transport, GAIA_VENDOR_QTIL, command, status, 0, NULL)


/*************************************************************************
NAME
    send_success

DESCRIPTION
    Send a successful response to the given command
*/
#define send_success(transport, command) \
    send_simple_response(transport, command, GAIA_STATUS_SUCCESS)


/*************************************************************************
NAME
    send_success_payload

DESCRIPTION
    Send a successful response incorporating the given payload
*/
#define send_success_payload(transport, command, length, payload) \
    send_ack(transport, GAIA_VENDOR_QTIL, command, GAIA_STATUS_SUCCESS, length, payload)


/*************************************************************************
NAME
    send_notification

DESCRIPTION
    Send a notification incorporating the given payload
*/
#define send_notification(transport, event, length, payload) \
    send_response(transport, GAIA_VENDOR_QTIL, GAIA_EVENT_NOTIFICATION, \
                  event, length, payload)


/*************************************************************************
NAME
    send_insufficient_resources

DESCRIPTION
    Send an INSUFFICIENT_RESOURCES response to the given command
*/
#define send_insufficient_resources(transport, command) \
    send_simple_response(transport, command, GAIA_STATUS_INSUFFICIENT_RESOURCES)


/*************************************************************************
NAME
    send_invalid_parameter

DESCRIPTION
    Send an INVALID_PARAMETER response to the given command
*/
#define send_invalid_parameter(transport, command) \
    send_simple_response(transport, command, GAIA_STATUS_INVALID_PARAMETER)


/*************************************************************************
NAME
    send_incorrect_state

DESCRIPTION
    Send an INCORRECT_STATE response to the given command
*/
#define send_incorrect_state(transport, command) \
    send_simple_response(transport, command, GAIA_STATUS_INCORRECT_STATE)


/*************************************************************************
NAME
    send_ack

DESCRIPTION
    Build and send a Gaia acknowledgement packet
*/
#define send_ack(transport, vendor, command, status, length, payload) \
    GaiaSendPacket((GAIA_TRANSPORT *) (transport), vendor, (command) | GAIA_ACK_MASK, status, length, payload)


/*  Internal message ids  */
typedef enum
{
    GAIA_INTERNAL_INIT = 1,
    GAIA_INTERNAL_REBOOT_REQ,
} gaia_internal_message;

typedef struct _GAIA_T GAIA_T;

/*! @brief Gaia library main task and state structure. */
struct _GAIA_T
{
    Task app_task;
    uint32 command_locus_bits;
    gaia_v3_command_handler_fn_t gaia_v3_command_handler;

    unsigned api_minor:4;
    gaia_app_version_t app_version;
};

extern GAIA_T gaia;


/*! @brief Utility function to send a GAIA_CONNECT_CFM message to client task.
 *
 *  @param transport The gaia transport on which the event occurred.
 *  @param success Boolean indicating success (TRUE) or failure (FALSE) of connection attempt.
 */
void gaia_TransportSendGaiaConnectCfm(gaia_transport *transport, bool success, const tp_bdaddr *tp_bd_addr);

/*! @brief Utility function to send a GAIA_CONNECT_IND message to client task.
 *
 *  @param transport The gaia transport on which the event occurred.
 *  @param success Boolean indicating success (TRUE) or failure (FALSE) of connection attempt.
 */
void gaia_TransportSendGaiaConnectInd(gaia_transport *transport, bool success, const tp_bdaddr *tp_bd_addr);

/*! @brief Utility function to send a GAIA_DISCONNECT_IND message to client task.
 *
 *  @param transport The gaia transport on which the event occurred.
 */
void gaia_TransportSendGaiaDisconnectInd(gaia_transport *transport);

/*! @brief Utility function to send a GAIA_DISCONNECT_CFM message to client task.
 *
 *  @param transport The gaia transport on which the event occurred.
 */
void gaia_TransportSendGaiaDisconnectCfm(gaia_transport *transport);

/*! @brief Utility function to send a GAIA_START_SERVICE_CFM message to client task.
 *
 *  @param transport_type The transport type over which the service runs
 *  @param transport The transport instance over which the service runs. Can be NULL if the 'success' parameter indicates a failure to start the service.
 *  @param success Boolean indicating success or failure of the GaiaStartService request
 */
void gaia_TransportSendGaiaStartServiceCfm(gaia_transport_type transport_type, gaia_transport *transport, bool success);

/*! @brief Utility function to send a GAIA_STOP_SERVICE_CFM message to client task.
 *
 *  @param transport_type The transport type over which the service runs
 *  @param transport The transport instance over which the service runs. Can be NULL if the 'success' parameter indicates a failure to stop the service.
 *  @param success Boolean indicating success or failure of the GaiaSttopService request
 */
void gaia_TransportSendGaiaStopServiceCfm(gaia_transport_type transport_type, gaia_transport *transport, bool success);

void gaia_TransportSendGaiaHandoverInd(gaia_transport *transport, bool success, bool is_primary);


#endif /* ifdef _GAIA_PRIVATE_H_ */

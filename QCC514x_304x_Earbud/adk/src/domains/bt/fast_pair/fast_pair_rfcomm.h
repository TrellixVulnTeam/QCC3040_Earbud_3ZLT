/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       fast_pair_rfcomm.h
\brief      File consists of function declaration for Fast Pair Service's RFCOMM transport.
*/
#ifndef FASTPAIR_RFCOMM_H
#define FASTPAIR_RFCOMM_H

#include "bdaddr.h"

#define FASTPAIR_RFCOMM_CONNECTIONS_MAX (2)

/* RFComm message definitions */
typedef enum
{
    FASTPAIR_RFCOMM_MESSAGE_TYPE_CONNECT_IND,
    FASTPAIR_RFCOMM_MESSAGE_TYPE_SERVER_CONNECT_CFM,
    FASTPAIR_RFCOMM_MESSAGE_TYPE_INCOMING_DATA,
    FASTPAIR_RFCOMM_MESSAGE_TYPE_DISCONNECT_IND,
    FASTPAIR_RFCOMM_MESSAGE_TYPE_DISCONNECT_CFM
} FASTPAIR_RFCOMM_MESSAGE_TYPE;

typedef enum
{
    RFCOMM_CONN_STATE_DISCONNECTED,
    RFCOMM_CONN_STATE_CONNECTED
}rfcomm_conn_state_t;

/* local data structure for RFCOMM transport */
typedef struct
{
    Sink data_sink;
    bdaddr device_addr;
    unsigned server_channel:8;
    unsigned connections_allowed:1;
    rfcomm_conn_state_t conn_state:1;
}fast_pair_rfcomm_data_t;

extern uint8 ack_msg_to_fp_seeker_number;
extern uint8 send_data_to_fp_seeker_number;


/*! \brief Initialize the FastPair RFCOMM module.
*/
void fastPair_RfcommInit(void);

/*! \brief Return if FastPair RFCOMM channel is connected.

    \return TRUE if RFComm channel is connected, FALSE if not connected
*/
bool fastPair_RfcommIsConnected(void);

/*! \brief Get RFCOMM connected instances.

    \return Return number of RFCOMM connected instances.
*/
uint8 fastPair_RfcommGetRFCommConnectedInstances(void);


/*! \brief Send FastPair protocol data to the handset using RFCOMM.

    \param data Pointer to uint8 data to send
    \param length Number of octets to send
    \return TRUE if the data was successfully sent, otherwise FALSE.
*/
bool fastPair_RfcommSendData(uint8* data, uint16 length);


/*! \brief  Callback that is called when fast pair message is received over RFComm.

    \param msg_type RFComm message type
    \param msg_data Message data. NULL if no data exists.
    \param len      Message data length

    \return processed data length in case msg_type is FASTPAIR_RFCOMM_MESSAGE_TYPE_INCOMING_DATA. 0 otherwise.
*/
typedef uint16 (*fastPair_RfcommMsgCallBack)(FASTPAIR_RFCOMM_MESSAGE_TYPE msg_type,const uint8 *msg_data, uint16 len);


/*! \brief Register for fast pair RFComm incoming message callback.

    \param msgCallBack Callback that is called when fast pair message is received over RFComm.
*/
void fastPair_RfcommRegisterMessage(fastPair_RfcommMsgCallBack msgCallBack);


/*! Get Fast Pair RFCommm channel connected with Bluetoot device.
    \param addr Bluetooth address of device connected over rfcomm channel.

    \returns Fast Pair RFCommm channel.
 */
uint8 fastPair_RfcommGetRFCommChannel(bdaddr *addr);

/*! Set the Fast Pair RFCommm channel.
    \param addr Bluetooth address of device connected with RFComm channel
    \param channel fast Pair RFComm channel number
 */
void fastPair_RfcommSetRFCommChannel(bdaddr *addr, uint8 channel);

/*! Restore Fast Pair RFComm after handover
    \param addr Bluetooth address of device whose RFComm connection to be restored.

    \returns TRUE if the RFComm is restored succesfully, FALSE if it fails
 */
bool fastPair_RfcommRestoreAfterHandover(bdaddr *addr);

/*! \brief Return if FastPair RFCOMM channel is connected with requested device addr
    \param addr Bluetooth address of remote device

    \return TRUE if RFComm channel is connected, FALSE if not connected
*/
bool fastPair_RfcommIsConnectedForAddr(bdaddr *addr);

/*! \brief Get RFCOMM connection instance using bluetooth address
    \param addr Bluetooth address of remote device

    \return Instance of RFCOMM connection for the given BD Address
*/
fast_pair_rfcomm_data_t* fastPair_RfcommGetInstance(bdaddr *addr);

/*! \brief Create RFComm connection instance
    \param addr Bluetooth address of device connected over RFcomm.

    \return Instance of RFComm connection which is either created or existing one
*/
fast_pair_rfcomm_data_t* fastPair_RfcommCreateInstance(bdaddr *addr);

/*! \brief Destroy RFComm connection instance
    \param instance Instance of rfcomm connection created in fastPair_RfcommCreateInstance
*/
void fastPair_RfcommDestroyInstance(fast_pair_rfcomm_data_t *instance);

/*! \brief Destroy all RFComm connection instances */
void fastPair_RfcommDestroyAllInstances(void);

/*! \brief Disconnect RFCOMM connection for the given instance
    \param instance Instance of RFCOMM connection

    \return Returns TRUE if instance is found and disconnection request is given, FALSE otherwise
*/
bool fastPair_RfcommDisconnectInstance(fast_pair_rfcomm_data_t *instance);


#endif /* FASTPAIR_RFCOMM_H */


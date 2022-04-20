/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       fast_pair_msg_stream.h
\brief      File consists of function declaration for Fast Pair Message Stream.
*/
#ifndef FASTPAIR_MSG_STREAM_H
#define FASTPAIR_MSG_STREAM_H

/* Message Group definition */
typedef enum
{
    FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_UNKNOWN = 0x00,
    FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_BLUETOOTH_EVENT = 0x01,
    FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_COMPANION_APP_EVENT = 0x02,
    FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_DEVICE_INFORMATION_EVENT = 0x03,
    FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_DEVCIE_ACTION_EVENT = 0x04,
    FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_MAX = 0x05
} FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP;


/*! \brief  Initialize fast pair message stream.
*/
void fastPair_MsgStreamInit(void);

/*! \brief Return if FastPair message stream channel is connected.

    \return TRUE if channel is connected, FALSE if not connected
*/
bool fastPair_MsgStreamIsConnected(void);

/*! \brief Return if FastPair message stream is busy.

    \return TRUE if busy, FALSE if not busy
*/
bool fastPair_MsgStreamIsBusy(void);

/*! \brief  Send data to seeker.

    \param msg_group Message group
    \param msg_code  Message code
    \param add_data  Additional data. NULL if no data exists.
    \param add_data_len Additional data length
*/
void fastPair_MsgStreamSendData(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP msg_group, uint8 msg_code, uint8 *add_data, uint16 add_data_len);

/*! \brief  Send Acknowledgement(ACK) to seeker.

    \param msg_group Message group
    \param msg_code  Message code
*/
void fastPair_MsgStreamSendACK(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP msg_group, uint8 msg_code);

/* Reason for NAK */
typedef enum
{
    FASTPAIR_MESSAGESTREAM_NAK_REASON_NOT_SUPORTED = 0x00,
    FASTPAIR_MESSAGESTREAM_NAK_REASON_DEVICE_BUSY = 0x01,
    FASTPAIR_MESSAGESTREAM_NAK_REASON_NOT_ALLOWED_DUE_TO_STATE = 0x02
} FASTPAIR_MESSAGESTREAM_NAK_REASON;

/*! \brief  Send Negative-Acknowledgement(NAK) to seeker.

    \param msg_group Message group
    \param msg_code  Message code
    \param nak_reason NAK reason.
*/
void fastPair_MsgStreamSendNAK(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP msg_group, uint8 msg_code,FASTPAIR_MESSAGESTREAM_NAK_REASON nak_reason);

/*! \brief  Send Rsp to seeker.

    \param msg_group Message group
    \param msg_code  Message code
    \param add_data  Additional data. NULL if no data exists.
    \param add_data_len Additional data length
*/
void fastPair_MsgStreamSendRsp(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP msg_group, uint8 msg_code, uint8 *data, uint8 data_len);

/* Message definitions of Message stream  */
typedef enum
{
    FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_CONNECT_IND,
    FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_SERVER_CONNECT_CFM,
    FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_INCOMING_DATA,
    FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_DISCONNECT_IND,
    FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_DISCONNECT_CFM
} FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE;

/*! \brief  Message callback. In case of FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_INCOMING_DATA, it is related to the message group.

    \param msg_code  Message type
    \param data      Data. NULL if no data exists.
    \param data_len  Data length
*/
typedef void (*fastPair_MsgStreamMsgCallBack)(FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE msg_type, const uint8 *data, uint16 data_len);


/*! \brief Register for messages. In case of FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_INCOMING_DATA, it is related to the message group.
    \details Each message group implementation has to register a callback for the messages. Only incase of
             FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_INCOMING_DATA message type, it is related to the message group.
             Other cases are generic and is sent to all the registered clients.

    \note In case of FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_INCOMING_DATA, data contains message code (1 byte),
          additional data length(2 bytes) and additional data(if any). Message group is not sent.

    \param msg_group   Message group
    \param msgCallBack Message callback. In case of FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_INCOMING_DATA, it is related to the message group.
    \return TRUE, if the callback for the message group has succesfully registered, FALSE if it fails.
*/
bool fastPair_MsgStreamRegisterGroupMessages(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP msg_group, fastPair_MsgStreamMsgCallBack msgCallBack);

#endif /* FASTPAIR_MSG_STREAM_H */


/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       fast_pair_msg_stream_dev_info.h
\brief      File consists of function declaration for Fast Pair Device Information Message Stream.
*/
#ifndef FASTPAIR_MSG_STREAM_DEV_INFO_H
#define FASTPAIR_MSG_STREAM_DEV_INFO_H

/* Device Info data structure*/
typedef struct{
    uint8 dev_info_capabilities;
}fast_pair_msg_stream_dev_info;

/*! \brief  Initialize fast pair Device info message stream.
*/
void fastPair_MsgStreamDevInfo_Init(void);

/*! \brief  Inform that an battery update is available.
*/
void fastPair_MsgStreamDevInfo_BatteryUpdateAvailable(void);

/* Device capaility Bits */
#define FASTPAIR_MESSAGESTREAM_DEVINFO_CAPABILITIES_SILENCE_MODE_SUPPORTED (0x01)
#define FASTPAIR_MESSAGESTREAM_DEVINFO_CAPABILITIES_COMPANION_APP_INSTALLED (0x02)

/*! \brief  Get the device information

  \return Returns the device info
*/
fast_pair_msg_stream_dev_info fastPair_MsgStreamDevInfo_Get(void);

/*! \brief  Set the device information

    \param dev_info Device Information
*/
void fastPair_MsgStreamDevInfo_Set(fast_pair_msg_stream_dev_info dev_info);

#endif /* FASTPAIR_MSG_STREAM_DEV_INFO_H */


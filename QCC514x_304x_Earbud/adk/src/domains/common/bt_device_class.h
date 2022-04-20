/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       bt_device_class.h
\brief      Utility definitions.
*/

#ifndef BT_DEVICE_CLASS_H_
#define BT_DEVICE_CLASS_H_

/*!@{ \name BT class of device field defines. */

/* Major Service Classes */
#define LIMITED_DISCOVERY_MODE_MAJOR_SERV_CLASS 0x00002000
#define LE_AUDIO_MAJOR_SERV_CLASS               0x00004000
/* Reserved                                     0x00008000 */
#define POSITIONING_MAJOR_SERV_CLASS            0x00010000
#define NETWORKING_MAJOR_SERV_CLASS             0x00020000
#define RENDER_MAJOR_SERV_CLASS                 0x00040000
#define CAPTURING_MAJOR_SERV_CLASS              0x00080000
#define OBJECT_TRANSFER_MAJOR_SERV_CLASS        0x00100000
#define AUDIO_MAJOR_SERV_CLASS                  0x00200000
#define TELEPHONY_MAJOR_SERV_CLASS              0x00400000
#define INFORMATION_MAJOR_SERV_CLASS            0x00800000

/* Major Device Classes */
#define MISC_MAJOR_DEVICE_CLASS                 0x00000000
#define COMPUTER_MAJOR_DEVICE_CLASS             0x00000100
#define PHONE_MAJOR_DEVICE_CLASS                0x00000200
#define LAN_MAJOR_DEVICE_CLASS                  0x00000300
#define AV_MAJOR_DEVICE_CLASS                   0x00000400
#define PERIPHERAL_MAJOR_DEVICE_CLASS           0x00000500
#define IMAGING_MAJOR_DEVICE_CLASS              0x00000600
#define WEARABLE_MAJOR_DEVICE_CLASS             0x00000700
#define TOY_MAJOR_DEVICE_CLASS                  0x00000800
#define HEALTH_MAJOR_DEVICE_CLASS               0x00000900
#define UNCATEGORIZED_MAJOR_DEVICE_CLASS        0x00001F00

/* Minor Device Classes */
#define UNCATEGORIZED_MINOR_DEVICE_CLASS		0x00000000

/* Minor Device Classes for AV_MAJOR_DEVICE_CLASS */
#define HEADSET_MINOR_DEVICE_CLASS              0x00000004
#define HANDS_FREE_MINOR_DEVICE_CLASS           0x00000008
/* reserved                                     0x0000000c */
#define MICROPHONE_MINOR_DEVICE_CLASS           0x00000010
#define LOUDSPEAKER_MINOR_DEVICE_CLASS          0x00000014
#define HEADPHONES_MINOR_DEVICE_CLASS           0x00000018
#define PORTABLE_AUDIO_MINOR_DEVICE_CLASS       0x0000001c
#define CAR_AUDIO_MINOR_DEVICE_CLASS            0x00000020
#define STB_MINOR_DEVICE_CLASS                  0x00000024
#define HIFI_AUDIO_MINOR_DEVICE_CLASS           0x00000028
#define VCR_MINOR_DEVICE_CLASS                  0x0000002c
#define VIDEO_CAMERA_MINOR_DEVICE_CLASS         0x00000030
#define CAMCORDER_MINOR_DEVICE_CLASS            0x00000034
#define VIDEO_MONITOR_MINOR_DEVICE_CLASS        0x00000038
#define VIDEO_AND_SPEAKER_MINOR_DEVICE_CLASS    0x0000003c
#define VIDEO_CONFERENCING_MINOR_DEVICE_CLASS   0x00000040
/* reserved                                     0x00000044*/
#define GAMING_TOY_MINOR_DEVICE_CLASS           0x00000048


/*!@}*/

#endif /* BT_DEVICE_CLASS_H_ */

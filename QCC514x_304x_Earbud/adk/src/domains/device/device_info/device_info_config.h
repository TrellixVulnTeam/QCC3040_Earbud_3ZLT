/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup device_info
\brief      File containing values that can be used to configure the device info.

@{
*/
#ifndef DEVICE_INFO_CONFIG_H_
#define DEVICE_INFO_CONFIG_H_

#ifndef DEVICE_HARDWARE_VERSION
/* Default version */
#define DEVICE_HARDWARE_VERSION "1.0.0"
#endif

#ifndef DEVICE_BUILDID
#define DEVICE_BUILDID "(unreleased)"
#endif

#define _DIC_STRINGIFY_(str) _DIC_AUX_(str)
#define _DIC_AUX_(str) #str

#ifndef DEVICE_FIRMWARE_VERSION
/* Default version */
#define DEVICE_FIRMWARE_VERSION "1.0.0."
#endif

#ifndef DEVICE_CURRENT_LANGUAGE
/* Default version */
#define DEVICE_CURRENT_LANGUAGE "en"
#endif

#ifndef DEVICE_SOFTWARE_VERSION
/* Default version */
#define DEVICE_SOFTWARE_VERSION "20.3"
#endif

#endif /* DEVICE_INFO_CONFIG_H_ */

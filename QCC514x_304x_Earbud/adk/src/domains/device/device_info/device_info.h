/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Device Info component
*/

#ifndef DEVICE_INFO_H_
#define DEVICE_INFO_H_

/*! \brief Gets the device name.

    \return The name of the local device.
*/
const char * DeviceInfo_GetName(void);

/*! \brief Gets the manufacturer info.

    \return The manufacturer string of the local device.
*/
const char * DeviceInfo_GetManufacturer(void);

/*! \brief Gets the model ID.

    \return The model ID of the local device.
*/
const char * DeviceInfo_GetModelId(void);

/*! \brief Gets the hardware version.

    \note the returned pointer needs to be valid for ever.

    \return The hardware version of the local device.
*/
const char * DeviceInfo_GetHardwareVersion(void);

/*! \brief Gets the firmware version.

    \note the returned pointer needs to be valid for ever.

    \return The firmware version of the local device.
*/
const char * DeviceInfo_GetFirmwareVersion(void);

/*! \brief Gets the serial number.

    \return The serial number of the local device.
*/
const char * DeviceInfo_GetSerialNumber(void);

/*! \brief Gets the current language.

    \note the returned pointer needs to be valid for ever.

    \return The current language of the local device, in ISO 639-1 format
*/
const char * DeviceInfo_GetCurrentLanguage(void);

/*! \brief Gets the software version.

    \note the returned pointer needs to be valid for ever.

    \return The software version of the local device.
*/
const char * DeviceInfo_GetSoftwareVersion(void);

#endif /* DEVICE_INFO_H_ */

/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup bt_domain
\brief   Provides access to ps keys associated with a device.
 
Device Ps Key had its own PDDU.
Number of a ps key associated with a device is stored in device database.

*/

#ifndef DEVICE_PSKEY_H_
#define DEVICE_PSKEY_H_

#include <device.h>

/*@{*/

/*! Data id is used to differentiate between features using device ps keys. */
typedef enum
{
    device_pskey_remote_device_name
} device_pskey_data_id_t;

typedef enum
{
    device_ps_key_flag_contains_data = 1 << 0,
    device_ps_key_flag_new_data_pending = 1 << 1,
    device_ps_key_flag_needs_sync = 1 << 2,
} device_pskey_flags_t;

typedef struct
{
    void (*Write)(device_t device, device_pskey_data_id_t data_id, uint8 *data, uint16 data_size);
} device_pskey_callback_t;

/*! \brief Register device ps key PDDU

    Register device ps key PDDU to be a part of serialisation and deserialisation of device database
*/
void DevicePsKey_RegisterPddu(void);

/*! \brief Register write callback

    \param callback Function to be called on write.
*/
void DevicePsKey_RegisterCallback(const device_pskey_callback_t *callback);

/*! \brief Write a ps key associated with a device

    \param device    Device with which data is supposed to be associated.
    \param data_id   Id of feature owning data.
    \param data      Pointer to a data buffer.
    \param data_size Length of data in bytes.

    \return Number of bytes actually written. Note that the number of actually written bytes
            may be one greater than data_size. This is because only even sized data can be written.
*/
uint16 DevicePsKey_Write(device_t device, device_pskey_data_id_t data_id, uint8 *data, uint16 data_size);

/*! \brief Read a ps key associated with a device

    \param device         Device with which data is supposed to be associated.
    \param data_id        Id of feature owning data.
    \param[out] data_size Length of read data in bytes.

    \return Buffer with read data. Memory is allocated by DevicePsKey_Read(),
            and it must be freed by a caller.
*/
uint8 *DevicePsKey_Read(device_t device, device_pskey_data_id_t data_id, uint16 *data_size);

/*! \brief Set a flag for ps key associated with a device

    Note that this will only update flag in device database (RAM).
    If flag requires persistence then caller should call
    DeviceDbSerialiser_SerialiseDevice() or DeviceDbSerialiser_Serialise()
    when updates are done.

    \param device   Device with which data is supposed to be associated.
    \param data_id  Id of feature owning data.
    \param flag     Flag to be set.
*/
void DevicePsKey_SetFlag(device_t device, device_pskey_data_id_t data_id, device_pskey_flags_t flag);

/*! \brief Clear a flag for ps key associated with a device

    Note that this will only update flag in device database (RAM).
    If flag requires persistence then caller should call
    DeviceDbSerialiser_SerialiseDevice() or DeviceDbSerialiser_Serialise()
    when updates are done.

    \param device   Device with which data is supposed to be associated.
    \param data_id  Id of feature owning data.
    \param flag     Flag to be cleared.
*/
void DevicePsKey_ClearFlag(device_t device, device_pskey_data_id_t data_id, device_pskey_flags_t flag);

/*! \brief Checks if flag, for ps key associated with a device, is set.

    Note that this will only update flag in device database (RAM).
    If flag requires persistence then caller should call
    DeviceDbSerialiser_SerialiseDevice() or DeviceDbSerialiser_Serialise()
    when updates are done.

    \param device   Device with which data is supposed to be associated.
    \param data_id  Id of feature owning data.
    \param flag     Flag to be tested.

    \return TRUE if flag is set.
*/
bool DevicePsKey_IsFlagSet(device_t device, device_pskey_data_id_t data_id, device_pskey_flags_t flag);

/*! \brief Clear a flag for all devices with associated ps key

    Note that this will only update flag in device database (RAM).
    If flag requires persistence then caller should call
    DeviceDbSerialiser_SerialiseDevice() or DeviceDbSerialiser_Serialise()
    when updates are done.

    \param device   Device with which data is supposed to be associated.
    \param data_id  Id of feature owning data.
    \param flag     Flag to be cleared.
*/
void DevicePsKey_ClearFlagInAllDevices(device_pskey_data_id_t data_id, device_pskey_flags_t flag);

/*@}*/

#endif /* REMOTE_DEVICE_PSKEY_H_ */

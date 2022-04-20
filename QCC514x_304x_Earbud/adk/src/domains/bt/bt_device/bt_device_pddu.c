/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Serialisation and deserialisation of BT device.

*/

#include <bt_device.h>
#include <device_db_serialiser.h>
#include <device_properties.h>
#include <pddu_map.h>

#include <logging.h>
#include <panic.h>

#define SIZE_OF_PAYLOAD_TYPE    0x1
#define PAYLOAD_DATA_OFFSET     (SIZE_OF_PAYLOAD_TYPE)

static const bt_device_default_value_callback_list_t *default_value_callback_list;

static void btDevice_SerialiseVariable(uint32 data, size_t data_size, uint8 *buf, uint16 *index)
{
    if(buf)
    {
        memcpy(buf + *index, &data, data_size);
    }
    *index += data_size;
}

static void btDevice_SerialiseData(void *data, size_t data_size, uint8 *buf, uint16 *index)
{
    if(buf)
    {
        memcpy(buf + *index, data, data_size);
    }
    *index += data_size;
}

/*! \brief Serialise pdd data to bufptr

    \note When bufptr is NULL then it doesn't write anything
    but effectively calculates size of buffer needed to serialise the data.

    \param pdd Data structure to be serialised.
    \param buf Buffer for serialised data, it needs to be big enough to hold data.

    \return Size in bytes required to hold serialised data.
*/
static uint16 BtDevice_SerialisePddu(bt_device_pdd_t *pdd, uint8* buf)
{
    uint16 index = 0;

    uint8 data_type_tag = 0xb;
    uint8 end_tag = 0xff;

    btDevice_SerialiseVariable(data_type_tag, sizeof(data_type_tag), buf, &index);

    btDevice_SerialiseVariable(pdd->a2dp_volume, sizeof(pdd->a2dp_volume), buf, &index);
    btDevice_SerialiseVariable(pdd->hfp_profile, sizeof(pdd->hfp_profile), buf, &index);
    btDevice_SerialiseVariable(pdd->type, sizeof(pdd->type), buf, &index);
    btDevice_SerialiseVariable(pdd->link_mode, sizeof(pdd->link_mode), buf, &index);
    btDevice_SerialiseVariable(pdd->reserved_1, sizeof(pdd->reserved_1), buf, &index);
    btDevice_SerialiseVariable(pdd->reserved_2, sizeof(pdd->reserved_2), buf, &index);

    btDevice_SerialiseVariable(pdd->padding, sizeof(pdd->padding), buf, &index);
    btDevice_SerialiseVariable(pdd->flags, sizeof(pdd->flags), buf, &index);
    btDevice_SerialiseVariable(pdd->sco_fwd_features, sizeof(pdd->sco_fwd_features), buf, &index);
    btDevice_SerialiseVariable(pdd->battery_server_config_l, sizeof(pdd->battery_server_config_l), buf, &index);
    btDevice_SerialiseVariable(pdd->battery_server_config_r, sizeof(pdd->battery_server_config_r), buf, &index);
    btDevice_SerialiseVariable(pdd->gatt_server_config, sizeof(pdd->gatt_server_config), buf, &index);

    btDevice_SerialiseVariable(pdd->gatt_server_services_changed, sizeof(pdd->gatt_server_services_changed), buf, &index);
    btDevice_SerialiseVariable(pdd->voice_assistant, sizeof(pdd->voice_assistant), buf, &index);
    btDevice_SerialiseVariable(pdd->dts, sizeof(pdd->dts), buf, &index);
    btDevice_SerialiseVariable(pdd->supported_profiles, sizeof(pdd->supported_profiles), buf, &index);
    btDevice_SerialiseVariable(pdd->last_connected_profiles, sizeof(pdd->last_connected_profiles), buf, &index);
    btDevice_SerialiseVariable(pdd->hfp_volume, sizeof(pdd->hfp_volume), buf, &index);
    btDevice_SerialiseVariable(pdd->hfp_mic_gain, sizeof(pdd->hfp_mic_gain), buf, &index);

    btDevice_SerialiseVariable(pdd->va_flags, sizeof(pdd->va_flags), buf, &index);
    btDevice_SerialiseData(pdd->va_locale, sizeof pdd->va_locale, buf, &index);

    btDevice_SerialiseVariable(pdd->headset_service_config, sizeof(pdd->headset_service_config), buf, &index);
    btDevice_SerialiseVariable(pdd->analog_audio_volume, sizeof(pdd->analog_audio_volume), buf, &index);

    btDevice_SerialiseVariable(end_tag, sizeof(end_tag), buf, &index);

    return index;
}

static uint8 btDevice_calculateLengthPdd(void)
{
    bt_device_pdd_t dummy_pdd = {0};
    return BtDevice_SerialisePddu(&dummy_pdd, NULL);
}

static uint8 btDevice_GetDeviceDataLen(device_t device)
{
    UNUSED(device);
    deviceTaskData *theDevice = DeviceGetTaskData();
    return theDevice->pdd_len;
}

static void btDevice_SerialisePersistentDeviceData(device_t device, void *buf, uint8 offset)
{

    uint8* bufptr = (uint8 *)buf;
    bt_device_pdd_t data_to_marshal_from_device_database = {0};

    UNUSED(offset);

    BtDevice_GetDeviceData(device, &data_to_marshal_from_device_database);

    BtDevice_SerialisePddu(&data_to_marshal_from_device_database, bufptr);
}

static void btDevice_DeserialisePddAndIncrementIndex(void *data, uint8 *buf, uint16 *index, size_t size)
{
    memcpy(data, buf + *index, size);
    *index += size;
}

static void btDevice_DeserialisePddAndIncrementIndexU8(uint8 *data, uint8 *buf, uint16 *index)
{
    *data = buf[*index];
    *index += sizeof(uint8);
}

static void btDevice_DeserialisePddAndIncrementIndexU16(uint16 *data, uint8 *buf, uint16 *index)
{
    *data = buf[*index + 1] << 8 | buf[*index];
    *index += sizeof(uint16);
}

static void btDevice_DeserialisePddAndIncrementIndexU32(uint32 *data, uint8 *buf, uint16 *index)
{
    *data = buf[*index + 3] << 24 | buf[*index + 2] << 16 | buf[*index + 1] << 8 | buf[*index];
    *index += sizeof(uint32);
}


static bool btDevice_IsEnoughDataFor202Properties(bt_device_pdd_t *device_data, uint16 index, uint8 data_length)
{
    uint8 size_of_properties = sizeof(device_data->supported_profiles) + sizeof(device_data->last_connected_profiles);
    return index + size_of_properties <= data_length ? TRUE : FALSE;
}

static void btDevice_Load202Properties(bt_device_pdd_t *device_data, uint8 *buf, uint16 *index)
{
    btDevice_DeserialisePddAndIncrementIndexU32(&device_data->supported_profiles, buf, index);
    btDevice_DeserialisePddAndIncrementIndexU32(&device_data->last_connected_profiles, buf, index);
    /* Don't add anything here */
}

static void btDevice_Set202Defaults(bt_device_pdd_t *device_data)
{
    DEBUG_LOG_ALWAYS("btDevice_Set202Defaults");
    device_data->supported_profiles = device_data->reserved_1;
    device_data->last_connected_profiles = device_data->reserved_2;
}

static bool btDevice_IsEnoughDataFor203Properties(bt_device_pdd_t *device_data, uint16 index, uint8 data_length)
{
    uint8 size_of_properties = sizeof(device_data->hfp_volume) + sizeof(device_data->hfp_mic_gain);
    return index + size_of_properties <= data_length ? TRUE : FALSE;
}

static void btDevice_Load203Properties(bt_device_pdd_t *device_data, uint8 *buf, uint16 *index)
{
    btDevice_DeserialisePddAndIncrementIndexU8(&device_data->hfp_volume, buf, index);
    btDevice_DeserialisePddAndIncrementIndexU8(&device_data->hfp_mic_gain, buf, index);
    /* Don't add anything here */
}

static void btDevice_Set203Defaults(bt_device_pdd_t *device_data)
{
    DEBUG_LOG_ALWAYS("btDevice_Set203Defaults");
    device_data->hfp_volume = 10;
    device_data->hfp_mic_gain = 15;
}

static bool btDevice_IsEnoughDataFor2031Properties(bt_device_pdd_t *device_data, uint16 index, uint8 data_length)
{
    uint8 size_of_properties = sizeof(device_data->va_flags) + sizeof(device_data->va_locale);
    return index + size_of_properties <= data_length ? TRUE : FALSE;
}

static void btDevice_Load2031Properties(bt_device_pdd_t *device_data, uint8 *buf, uint16 *index)
{
    btDevice_DeserialisePddAndIncrementIndexU8(&device_data->va_flags, buf, index);
    btDevice_DeserialisePddAndIncrementIndex(device_data->va_locale, buf, index, sizeof device_data->va_locale);
}

static void btDevice_Set2031Defaults(bt_device_pdd_t *device_data)
{
    DEBUG_LOG_ALWAYS("btDevice_Set2031Defaults");
    device_data->va_flags = device_va_flag_wuw_enabled;
    memset(device_data->va_locale, '\0', ARRAY_DIM(device_data->va_locale));
}

static bool btDevice_IsEnoughDataFor211Properties(bt_device_pdd_t *device_data, uint16 index, uint8 data_length)
{
    uint8 size_of_properties = sizeof(device_data->headset_service_config) + sizeof(device_data->analog_audio_volume);
    return index + size_of_properties <= data_length ? TRUE : FALSE;
}

static void btDevice_Load211Properties(bt_device_pdd_t *device_data, uint8 *buf, uint16 *index)
{
    btDevice_DeserialisePddAndIncrementIndexU32(&device_data->headset_service_config, buf, index);
    btDevice_DeserialisePddAndIncrementIndexU8(&device_data->analog_audio_volume, buf, index);
}

static void btDevice_Set211Defaults(bt_device_pdd_t *device_data)
{
    DEBUG_LOG_ALWAYS("btDevice_Set211Defaults");

    if(default_value_callback_list)
    {
        uint8 i;

        for(i = 0; i < default_value_callback_list->num_of_callbacks; ++i)
        {
            if(device_property_headset_service_config == default_value_callback_list->callback[i].property)
            {
                default_value_callback_list->callback[i].DefaultValueCallback(&device_data->headset_service_config, sizeof(device_data->headset_service_config));
                break;
            }
        }
    }
    device_data->analog_audio_volume = 10;
}

typedef struct
{
    bool (*IsEnoughDataForProperties)(bt_device_pdd_t *device_data, uint16 index, uint8 data_length);
    void (*LoadProperties)(bt_device_pdd_t *device_data, uint8 *buf, uint16 *index);
    void (*SetDefaults)(bt_device_pdd_t *device_data);
    uint16 version_id;
} bt_device_deser_callbacks_t;

bt_device_deser_callbacks_t deser_fns[] = {
    {
        .IsEnoughDataForProperties = btDevice_IsEnoughDataFor202Properties,
        .LoadProperties = btDevice_Load202Properties,
        .SetDefaults = btDevice_Set202Defaults,
        .version_id = 0x2020
    },
    {
        .IsEnoughDataForProperties = btDevice_IsEnoughDataFor203Properties,
        .LoadProperties = btDevice_Load203Properties,
        .SetDefaults = btDevice_Set203Defaults,
        .version_id = 0x2030

    },
    {
        .IsEnoughDataForProperties = btDevice_IsEnoughDataFor2031Properties,
        .LoadProperties = btDevice_Load2031Properties,
        .SetDefaults = btDevice_Set2031Defaults,
        .version_id = 0x2031
    },
    {
        .IsEnoughDataForProperties = btDevice_IsEnoughDataFor211Properties,
        .LoadProperties = btDevice_Load211Properties,
        .SetDefaults = btDevice_Set211Defaults,
        .version_id = 0x2110
    },
};

static void btDevice_DeserialisePersistentDeviceData(device_t device, void *buffer, uint8 data_length, uint8 offset)
{
    bt_device_pdd_t device_data = {0};
    uint8 *buf = buffer;
    uint16 index = PAYLOAD_DATA_OFFSET;

    UNUSED(offset);

    /* This is needed only in BtDevice PDDU as it was originally using marshalling
       which was adding an extra 0xFF at the end. That 0xFF should not be interpreted as data.
     */
    data_length -= 1;

    btDevice_DeserialisePddAndIncrementIndexU8(&device_data.a2dp_volume, buf, &index);
    btDevice_DeserialisePddAndIncrementIndexU8(&device_data.hfp_profile, buf, &index);
    btDevice_DeserialisePddAndIncrementIndexU8(&device_data.type, buf, &index);
    btDevice_DeserialisePddAndIncrementIndexU8(&device_data.link_mode, buf, &index);
    btDevice_DeserialisePddAndIncrementIndexU8(&device_data.reserved_1, buf, &index);
    btDevice_DeserialisePddAndIncrementIndexU8(&device_data.reserved_2, buf, &index);

    btDevice_DeserialisePddAndIncrementIndexU16(&device_data.padding, buf, &index);
    btDevice_DeserialisePddAndIncrementIndexU16(&device_data.flags, buf, &index);
    btDevice_DeserialisePddAndIncrementIndexU16(&device_data.sco_fwd_features, buf, &index);
    btDevice_DeserialisePddAndIncrementIndexU16(&device_data.battery_server_config_l, buf, &index);
    btDevice_DeserialisePddAndIncrementIndexU16(&device_data.battery_server_config_r, buf, &index);
    btDevice_DeserialisePddAndIncrementIndexU16(&device_data.gatt_server_config, buf, &index);

    btDevice_DeserialisePddAndIncrementIndexU8(&device_data.gatt_server_services_changed, buf, &index);
    btDevice_DeserialisePddAndIncrementIndexU8(&device_data.voice_assistant, buf, &index);
    btDevice_DeserialisePddAndIncrementIndexU8(&device_data.dts, buf, &index);

    DEBUG_LOG_VERBOSE("btDevice_DeserialisePersistentDeviceData end of 2010 index %d, data_length %d", index, data_length);


    /* Load properties for which data is available */
    uint8 i;
    for(i = 0; i < ARRAY_DIM(deser_fns); ++i)
    {
        if(deser_fns[i].IsEnoughDataForProperties(&device_data, index, data_length))
        {
            deser_fns[i].LoadProperties(&device_data, buf, &index);
            DEBUG_LOG_VERBOSE("btDevice_DeserialisePersistentDeviceData end of %04X index %d, data_length %d", deser_fns[i].version_id, index, data_length);
        }
        else
        {
            break;
        }
    }

    /* Use defaults for remaining properties i.e. properties that are not stored in the persistent store.
       This situation will occur during upgrade from the previous versions of the software which didn't have
       those properties.
     */
    for(; i < ARRAY_DIM(deser_fns); ++i)
    {
        deser_fns[i].SetDefaults(&device_data);
    }

    BtDevice_SetDeviceData(device, &device_data);
}

void BtDevice_RegisterPddu(void)
{
    deviceTaskData *theDevice = DeviceGetTaskData();
    theDevice->pdd_len = btDevice_calculateLengthPdd();

    DeviceDbSerialiser_RegisterPersistentDeviceDataUser(
        PDDU_ID_BT_DEVICE,
        btDevice_GetDeviceDataLen,
        btDevice_SerialisePersistentDeviceData,
        btDevice_DeserialisePersistentDeviceData);
}

void BtDevice_RegisterPropertyDefaults(bt_device_default_value_callback_list_t const *callback_list)
{
    default_value_callback_list = callback_list;
}

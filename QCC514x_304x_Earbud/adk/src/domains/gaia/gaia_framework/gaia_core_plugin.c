/*!
\copyright  Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for the gaia framework core plugin
*/

#define DEBUG_LOG_MODULE_NAME gaia_core
#include <logging.h>
DEBUG_LOG_DEFINE_LEVEL_VAR

#include <panic.h>
#include <byte_utils.h>
#include <stdlib.h>
#include <bt_device.h>

#include "gaia_core_plugin.h"
#include "gaia_framework_feature.h"
#include "gaia_transport.h"
#include "device_info.h"
#include "power_manager.h"
#include "charger_monitor.h"
#include "gaia_framework_data_channel.h"
#include "system_reboot.h"

#define NUM_OF_BYTES_PER_FEATURE 2
#define MORE_TO_COME_PAYLOAD_LENGTH 1

#define SIZE_DEVICE_BD_ADDR (6)

/*! \brief Function pointer definition for the command handler

    \param pdu_id      PDU specific ID for the message

    \param length      Length of the payload

    \param payload     Payload data
*/
static gaia_framework_command_status_t gaiaCorePlugin_MainHandler(GAIA_TRANSPORT *t, uint8 pdu_id, uint16 payload_length, const uint8 *payload);

static void gaiaCorePlugin_GetApiVersion(GAIA_TRANSPORT *t);
static void gaiaCorePlugin_GetSupportedFeatures(GAIA_TRANSPORT *t);
static void gaiaCorePlugin_GetSupportedFeaturesNext(GAIA_TRANSPORT *t);
static void gaiaCorePlugin_GetSerialNumber(GAIA_TRANSPORT *t);
static void gaiaCorePlugin_GetVariant(GAIA_TRANSPORT *t);
static void gaiaCorePlugin_GetApplicationVersion(GAIA_TRANSPORT *t);
static void gaiaCorePlugin_DeviceReset(GAIA_TRANSPORT *t);
static void gaiaCorePlugin_RegisterNotification(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload);
static void gaiaCorePlugin_UnregisterNotification(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload);
static void gaiaCorePlugin_GetTransportInfo(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload);
static void gaiaCorePlugin_SetTransportParameter(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload);
static void gaiaCorePlugin_GetUserFeature(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload);
static void gaiaCorePlugin_GetUserFeatureNext(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload);
static void gaiaCorePlugin_GetDeviceBluetoothAddress(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload);

/*! \brief Function that hendles 'Data Transfer Setup' command.
*/
static void gaiaCorePlugin_DataTransferSetup(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload);

/*! \brief Function that handles 'Data Transfer Get' command.
*/
static void gaiaCorePlugin_DataTransferGet(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload);

/*! \brief Function that handles 'Data Transfer Set' command.
*/
static void gaiaCorePlugin_DataTransferSet(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload);

/*! \brief Copies the data bytes of the Application Feature List from the offset specified.

    \param[IN/OUT] buf      The buffer that the data bytes will be copied to.
    \param[IN] buf_size     The size of the buffer.
    \param[IN] list         Pointer to a struct that stores the Feature-Type,
                            the number of the strings, and a pointer to the
                            string list.
    \param[IN] offset       Offset from the begining of the data bytes.
    \param[OUT] buf_used    The number of the data bytes copied to the buffer.

    \return The offset from which the contiguous data can be read.
            This value is set to zero if there are no more data.
*/
static uint16 gaiaCorePlugin_GetFormattedDataBytesFromStringList(uint8 *buf, const uint16 buf_size, gaia_user_defined_feature_data_t *list, const uint16 offset, uint16 *buf_used);


/*! \brief Copies the user-defined feature list data (starting from the 'offset')
           to the buffer.

    \param[IN] buf          Pointer to the buffer where the data is copied to.
    \param[IN] buf_size     The size of the buffer.
    \param[IN] type         Feature-Type of the data bytes.
    \param[IN] offset       The reading offset of the Feature-Type data bytes.
    \param[OUT] status      The result of copying the data bytes to the buffer.
*/
static void gaiaCorePlugin_CopyUserFeature(uint8 *buf, const uint16 buf_size, gaia_user_feature_type_t type, const uint16 offset, gaia_get_user_feature_reading_status_t *status);

/*! \brief Sets the user-defined feature data to the response PDUs for the
           'Get User Feature' and 'Get User Feature Next' commands.
*/
static void gaiaCorePlugin_SendUserFeatureResponse(GAIA_TRANSPORT *t, bool next_cmd, uint8 type, uint16 offset);

static bool gaiaCorePlugin_GetSupportedFeaturesPayload(uint16 payload_length, uint8 *payload);

/*! \brief Function that sends all available notifications
*/
static void gaiaCorePlugin_SendAllNotifications(GAIA_TRANSPORT *t);

/*! \brief Function that sends all available notifications
*/
static void gaiaCorePlugin_SendChargerStatusNotification(GAIA_TRANSPORT *t, bool plugged);

/*! \brief Gaia core plugin function to be registered as an observer of charger messages

    \param Task     Task passed to the registration function

    \param id       Messages id sent over from the charger

    \param message  Message
*/
static void gaiaCorePlugin_ChargerTask(Task task, MessageId message_id, Message message);

/*! \brief Populates and sends thepacket for get supported features (next)

    \param transport    Transport type

    \param pdu_id       PDU ID of the command
*/
static void gaiaCorePlugin_PopulateSupportedFeaturesPacketAndSend(GAIA_TRANSPORT *transport, uint8 pdu_id);

/*! \brief Calculates the number of features that are reported to the mobile app based on the payload size

    \param Task     Payload length

    \return         Number of features
*/
static uint8 gaiaCorePlugin_NumberOfFeaturesReported(uint8 payload_length);

static bool charger_client_is_registered = FALSE;

static TaskData gaia_core_plugin_task;

static bool current_charger_plugged_in_state = FALSE;

static uint8 num_of_remaining_features;

/*! \brief Pointer to the head of the linked list that the Application may
           register its feature list data that can be read by the mobile app
           with 'Get User Feature' and 'Get User Feature Next' commands.
*/
const gaia_user_defined_feature_data_t *gaia_user_feature_linked_list = NULL;

void GaiaCorePlugin_Init(void)
{
    static const gaia_framework_plugin_functions_t functions =
    {
        .command_handler = gaiaCorePlugin_MainHandler,
        .send_all_notifications = gaiaCorePlugin_SendAllNotifications,
        .transport_connect = NULL,
        .transport_disconnect = NULL,
    };

    DEBUG_LOG("GaiaCorePlugin_Init");

    GaiaFramework_RegisterFeature(GAIA_CORE_FEATURE_ID, GAIA_CORE_PLUGIN_VERSION, &functions);

    /* Register the core gaia plugin as an observer for charger messages */
    gaia_core_plugin_task.handler = gaiaCorePlugin_ChargerTask;
    charger_client_is_registered = Charger_ClientRegister((Task)&gaia_core_plugin_task);
}

void GaiaCorePlugin_RegisterGetUserFeatureData(const gaia_user_defined_feature_data_t *data_ptr)
{
    gaia_user_feature_linked_list = data_ptr;
}

static gaia_framework_command_status_t gaiaCorePlugin_MainHandler(GAIA_TRANSPORT *t, uint8 pdu_id, uint16 payload_length, const uint8 *payload)
{
    DEBUG_LOG("gaiaCorePlugin_MainHandler, transport %p, pdu_id %u", t, pdu_id);

    switch (pdu_id)
    {
        case get_api_version:
            gaiaCorePlugin_GetApiVersion(t);
            break;

        case get_supported_features:
            gaiaCorePlugin_GetSupportedFeatures(t);
            break;

        case get_supported_features_next:
            gaiaCorePlugin_GetSupportedFeaturesNext(t);
            break;

        case get_serial_number:
            gaiaCorePlugin_GetSerialNumber(t);
            break;

        case get_variant:
            gaiaCorePlugin_GetVariant(t);
            break;

        case get_application_version:
            gaiaCorePlugin_GetApplicationVersion(t);
            break;

        case device_reset:
            gaiaCorePlugin_DeviceReset(t);
            break;

        case register_notification:
            gaiaCorePlugin_RegisterNotification(t, payload_length, payload);
            break;

        case unregister_notification:
            gaiaCorePlugin_UnregisterNotification(t, payload_length, payload);
            break;

        case data_transfer_setup:
            gaiaCorePlugin_DataTransferSetup(t, payload_length, payload);
            break;

        case data_transfer_get:
            gaiaCorePlugin_DataTransferGet(t, payload_length, payload);
            break;

        case data_transfer_set:
            gaiaCorePlugin_DataTransferSet(t, payload_length, payload);
            break;

        case get_transport_info:
            gaiaCorePlugin_GetTransportInfo(t, payload_length, payload);
            break;

        case set_transport_parameter:
            gaiaCorePlugin_SetTransportParameter(t, payload_length, payload);
            break;

        case get_user_feature:
            gaiaCorePlugin_GetUserFeature(t, payload_length, payload);
            break;

        case get_user_feature_next:
            gaiaCorePlugin_GetUserFeatureNext(t, payload_length, payload);
            break;

        case get_device_bluetooth_address:
            gaiaCorePlugin_GetDeviceBluetoothAddress(t, payload_length, payload);
            break;
        
        default:
            DEBUG_LOG("gaiaCorePlugin_MainHandler, unhandled call for %u", pdu_id);
            return command_not_handled;
    }

    return command_handled;
}

static void gaiaCorePlugin_GetApiVersion(GAIA_TRANSPORT *t)
{
    static const uint8 value[2] = { GAIA_V3_VERSION_MAJOR, GAIA_V3_VERSION_MINOR };
    DEBUG_LOG_INFO("gaiaCorePlugin_GetApiVersion");

    GaiaFramework_SendResponse(t, GAIA_CORE_FEATURE_ID, get_api_version, sizeof(value), value);
}

static void gaiaCorePlugin_GetSupportedFeatures(GAIA_TRANSPORT *t)
{
    num_of_remaining_features = GaiaFrameworkFeature_GetNumberOfRegisteredFeatures();

    DEBUG_LOG("gaiaCorePlugin_GetSupportedFeatures");

    gaiaCorePlugin_PopulateSupportedFeaturesPacketAndSend(t, get_supported_features);
}


static void gaiaCorePlugin_GetSupportedFeaturesNext(GAIA_TRANSPORT *t)
{
    DEBUG_LOG("gaiaCorePlugin_GetSupportedFeaturesNext");

    gaiaCorePlugin_PopulateSupportedFeaturesPacketAndSend(t, get_supported_features_next);
}

static void gaiaCorePlugin_GetSerialNumber(GAIA_TRANSPORT *t)
{
    const char * response_payload = DeviceInfo_GetSerialNumber();
    uint8 response_payload_length = strlen(response_payload);

    DEBUG_LOG("gaiaCorePlugin_GetSerialNumber");

    GaiaFramework_SendResponse(t, GAIA_CORE_FEATURE_ID, get_serial_number, response_payload_length, (uint8 *)response_payload);
}

static void gaiaCorePlugin_GetVariant(GAIA_TRANSPORT *t)
{
    const char * response_payload = DeviceInfo_GetName();
    uint8 response_payload_length = strlen(response_payload);

    DEBUG_LOG("gaiaCorePlugin_GetVariant");

    GaiaFramework_SendResponse(t, GAIA_CORE_FEATURE_ID, get_variant, response_payload_length, (uint8 *)response_payload);
}

static void gaiaCorePlugin_GetApplicationVersion(GAIA_TRANSPORT *t)
{
    const char * response_payload = DeviceInfo_GetFirmwareVersion();
    uint8 response_payload_length = strlen(response_payload);

    DEBUG_LOG("gaiaCorePlugin_GetApplicationVersion, %s", response_payload);

    GaiaFramework_SendResponse(t, GAIA_CORE_FEATURE_ID, get_application_version, response_payload_length, (uint8 *)response_payload);
}

static void gaiaCorePlugin_DeviceReset(GAIA_TRANSPORT *t)
{
    DEBUG_LOG("gaiaCorePlugin_DeviceReset");

    GaiaFramework_SendResponse(t, GAIA_CORE_FEATURE_ID, device_reset, 0, NULL);

    SystemReboot_Reboot();
}

static void gaiaCorePlugin_RegisterNotification(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    bool error = TRUE;
    if (payload_length > 0)
    {
        const gaia_features_t feature = payload[0];
        DEBUG_LOG_INFO("gaiaCorePlugin_RegisterNotification, feature_id %u", feature);
        if (GaiaFrameworkFeature_RegisterForNotifications(t, feature))
        {
            GaiaFramework_SendResponse(t, GAIA_CORE_FEATURE_ID, register_notification, 0, NULL);
            GaiaFrameworkFeature_SendAllNotifications(t, feature);
            error = FALSE;
        }
        else
            DEBUG_LOG_ERROR("gaiaCorePlugin_RegisterNotification, failed to register feature_id %u", payload[0]);

    }
    else
        DEBUG_LOG_ERROR("gaiaCorePlugin_RegisterNotification, no feature in packet");

    if (error)
        GaiaFramework_SendError(t, GAIA_CORE_FEATURE_ID, register_notification, 0);
}

static void gaiaCorePlugin_UnregisterNotification(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    bool error = TRUE;
    if (payload_length > 0)
    {
        const gaia_features_t feature = payload[0];
        DEBUG_LOG_INFO("gaiaCorePlugin_UnregisterNotification, feature_id %u", feature);
        if (GaiaFrameworkFeature_UnregisterForNotifications(t, feature))
        {
            GaiaFramework_SendResponse(t, GAIA_CORE_FEATURE_ID, unregister_notification, 0, NULL);
            error = FALSE;
        }
        else
            DEBUG_LOG_ERROR("gaiaCorePlugin_UnregisterNotification, failed to unregister feature_id %u", payload[0]);

    }
    else
        DEBUG_LOG_ERROR("gaiaCorePlugin_UnregisterNotification, no feature in packet");

    if (error)
        GaiaFramework_SendError(t, GAIA_CORE_FEATURE_ID, unregister_notification, 0);
}

static void gaiaCorePlugin_GetTransportInfo(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    bool error = TRUE;
    if (payload_length > 0)
    {
        uint32 value;
        const gaia_transport_info_key_t key = payload[0];
        if (Gaia_TransportGetInfo(t, key, &value))
        {
            uint8 response[1 + sizeof(value)];
            response[0] = key;
            ByteUtilsSet4Bytes(response, 1, value);
            GaiaFramework_SendResponse(t, GAIA_CORE_FEATURE_ID, get_transport_info, sizeof(response), response);
            DEBUG_LOG_INFO("gaiaCorePlugin_GetTransportInfo, key %u, value %u", key, value);
            error = FALSE;
        }
        else
            DEBUG_LOG_ERROR("gaiaCorePlugin_GetTransportInfo, key %u not accepted", key);

    }
    else
        DEBUG_LOG_ERROR("gaiaCorePlugin_GetTransportInfo, no key in packet");

    if (error)
        GaiaFramework_SendError(t, GAIA_CORE_FEATURE_ID, get_transport_info, invalid_parameter);
}

static void gaiaCorePlugin_SetTransportParameter(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    bool error = TRUE;
    if (payload_length >= 5)
    {
        const gaia_transport_info_key_t key = payload[0];
        uint32 value = ByteUtilsGet4BytesFromStream(payload + 1) ;
        DEBUG_LOG_INFO("gaiaCorePlugin_SetTransportInfo, key %u, requested value %u", key, value);
        if (Gaia_TransportSetParameter(t, key, &value))
        {
            uint8 response[1 + sizeof(value)];
            response[0] = key;
            ByteUtilsSet4Bytes(response, 1, value);
            GaiaFramework_SendResponse(t, GAIA_CORE_FEATURE_ID, set_transport_parameter, sizeof(response), response);
            DEBUG_LOG_INFO("gaiaCorePlugin_SetTransportInfo, actual value %u", value);
            error = FALSE;
        }
    }
    else
        DEBUG_LOG_ERROR("gaiaCorePlugin_SetTransportInfo, no key and/or value in packet");

    if (error)
        GaiaFramework_SendError(t, GAIA_CORE_FEATURE_ID, set_transport_parameter, invalid_parameter);
}

static void gaiaCorePlugin_DataTransferSetup(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    DEBUG_LOG("gaiaCorePlugin_DataTransferSetup");
    if (payload_length == GAIA_DATA_TRANSFER_SETUP_CMD_PAYLOAD_SIZE)
    {
        GaiaFramework_DataTransferSetup(t, payload_length, payload);
    }
    else
    {
        DEBUG_LOG("gaiaCorePlugin_DataTransferSetup, Invalid payload length: %d", payload_length);
        GaiaFramework_SendError(t, GAIA_CORE_FEATURE_ID, data_transfer_setup, GAIA_STATUS_INVALID_PARAMETER);
    }
}

static void gaiaCorePlugin_DataTransferGet(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    DEBUG_LOG("gaiaCorePlugin_DataTransferGet");
    if (payload_length == GAIA_DATA_TRANSFER_GET_CMD_PAYLOAD_SIZE)
    {
        GaiaFramework_DataTransferGet(t, payload_length, payload);
    }
    else
    {
        DEBUG_LOG("gaiaCorePlugin_DataTransferGet, Invalid payload length: %d", payload_length);
        GaiaFramework_SendError(t, GAIA_CORE_FEATURE_ID, data_transfer_get, GAIA_STATUS_INVALID_PARAMETER);
    }
}

static void gaiaCorePlugin_DataTransferSet(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    DEBUG_LOG("gaiaCorePlugin_DataTransferSet");
    if (GAIA_DATA_TRANSFER_SET_CMD_HEADER_SIZE < payload_length)
    {
        GaiaFramework_DataTransferSet(t, payload_length, payload);
    }
    else
    {
        DEBUG_LOG("gaiaCorePlugin_DataTransferSet, Invalid payload length: %d", payload_length);
        GaiaFramework_SendError(t, GAIA_CORE_FEATURE_ID, data_transfer_set, GAIA_STATUS_INVALID_PARAMETER);
    }
}

static uint16 gaiaCorePlugin_GetFormattedDataBytesFromStringList(uint8 *buf, const uint16 buf_size, gaia_user_defined_feature_data_t *list, const uint16 offset, uint16 *buf_used)
{
/*
    User-defined Feature List Overall Format:
            0        1        2        3        4        5        6        7      ...        L    (Byte)
        +--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+
        |Type(*1)|     Size (*2)   |  Data (e.g. Application Feature List)         ...            |
        +--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+
        |<-  Feature-Type Header ->|<---    Feature Data (e.g. Application Feature List)      --->|

        (*1) Feature-Type: 0x01 = 'Application Feature List'
        (*2) Size (16bits): The size of the 'Data', which is equal to (L - 2).

    Feature Data (Application Feature List) Format:
            0        1        2        3        ...      N       N + 1    N + 2    N + 3    N + 4    ...        M    (Byte)
        +--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+
        |Idx0(*1)|Siz0(*2)|     Text data0      ...           |  Idx1  |  Size1 |     Text data1     ...            |
        +--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+
        (*1) Index: Just an ascending number starting from zero.
        (*2) Size (8bits): The size of the text data, which is equal to (N - 1).

*/
    uint16 app_list_data_size;      /* This holds the total data size including the 'Feature-Type' and 'Size' octets. */
    uint16 this_record_start_pos;
    uint16 remaining_buf_size;
    uint8 str_size;
    uint8 index;
    uint8 buf_item_index = 0;
    uint16 bytestream_pos = offset;
    uint16 buf_pos = 0;

    PanicFalse(0 < buf_size);

    /* If the requested data bytes include (a part of) the Feature-Type header,
     * put aside the space for the header, which will be set later as the total
     * length of the Feature Data is not known yet.*/
    if (bytestream_pos < 3)
    {
        buf_pos = 3 - bytestream_pos;   /* Put aside the space for the Feature-Type header. */
        bytestream_pos = 3;             /* Set this to the start of the Feature Data. */
    }

    app_list_data_size = sizeof(uint8) + sizeof(app_list_data_size);
    DEBUG_LOG_DEBUG("gaiaCorePlugin_GetFormattedDataBytesFromStringList, HDR app_list_data_size: %u", app_list_data_size);

    /* Scan the Application Feature List to generate the data bytes of the Feature Data.
     * The list is just an array of NULL-terminated strings. The data bytes is an byte
     * array that the index and the size (8bits) header is added to each string.
     * (Note that NULL-terminator is not copied to the data bytes.) */
    for (index = 0; index < list->num_of_strings; index++)
    {
        if (NULL == list->string_list[index])
            continue;
        buf_item_index += 1;

        this_record_start_pos = app_list_data_size;

        /* A record consists of 'Index', 'Size' and a text string.
         * For example:
         *  0x05, 0x0B, 0x41, 0x64, 0x70, 0x74, 0x69, 0x76, 0x65, 0x2D, 0x41, 0x4E, 0x43
         *  Index,Size, |<--------------------- "Adaptive-ANC" ------------------------>|
         */
        str_size = (uint8) strlen(list->string_list[index]);
        app_list_data_size += sizeof(uint8) + sizeof(uint8) + str_size;     /* Add the size of this record: Index + Size + Text-Length. */

        DEBUG_LOG_DEBUG("gaiaCorePlugin_GetFormattedDataBytesFromStringList, [%u] app_list_data_size: %u = %d + %d + %d", index, app_list_data_size, (uint8)sizeof(uint8), (uint8)sizeof(uint8), str_size);

        if (app_list_data_size <= bytestream_pos)
            continue;   /* Just scan until the point where the requested data bytes start. */

        /* Make sure not to overrun the buffer. */
        if (buf_size <= buf_pos)
            continue;   /* Note that the iteration needs to carry on to calculate the total size. */

        remaining_buf_size = buf_size - buf_pos;
        {
            uint8 offset_in_record = bytestream_pos - this_record_start_pos;

            switch (offset_in_record)
            {
            case 0:
                buf[buf_pos++] = buf_item_index - 1;
                bytestream_pos++;
                remaining_buf_size--;
                if (remaining_buf_size == 0)
                    break;
                /* Fall through */
            case 1:
                buf[buf_pos++] = str_size;
                bytestream_pos++;
                remaining_buf_size--;
                if (remaining_buf_size == 0)
                    break;
                /* Fall through */
            default:
                {
                    uint8 str_offset = (offset_in_record < 2) ? 0 : (offset_in_record - 2);
                    uint8 copy_size = str_size - str_offset;

                    copy_size = (remaining_buf_size < copy_size) ? remaining_buf_size : copy_size;
                    memcpy(&buf[buf_pos], list->string_list[index] + str_offset, copy_size);
                    buf_pos += copy_size;
                    bytestream_pos += copy_size;
                }
            }
        }
    }

    if (0 == buf_item_index)   /* No valid data at all? */
    {
        DEBUG_LOG_INFO("gaiaCorePlugin_GetFormattedDataBytesFromStringList, No valid data in the list!");
        *buf_used = 0;
        return 0;   /* No more data */
    }

    if (offset < 3)
    {
        /* The Size does not include the Feature-Type and Size header octets. */
        uint16 size = app_list_data_size - sizeof(uint8) - sizeof(uint16);

        DEBUG_LOG_DEBUG("gaiaCorePlugin_GetFormattedDataBytesFromStringList, Feature-Type: %02X, Type-Size: %04X (%u)", (uint8) list->type, size, size);
        switch (offset)
        {
        case 0:
            buf[0] = (uint8) list->type;
            if (1 < buf_size)
                buf[1] = (uint8) (size >> 8);
            if (2 < buf_size)
                buf[2] = (uint8) (size & 0xFF);
            break;
        case 1:
            buf[0] = (uint8) (size >> 8);
            if (1 < buf_size)
                buf[1] = (uint8) (size & 0xFF);
            break;
        case 2:
            buf[0] = (uint8) (size & 0xFF);
            break;
        }
        if (buf_size <= 3)
        {
            buf_pos = buf_size;
            bytestream_pos = buf_size;
        }
    }

    *buf_used = buf_pos;
    if (bytestream_pos < app_list_data_size)
        return bytestream_pos;
    else
        return 0;   /* No more data. */
}

/*! \brief Copies the user-defined feature list data (starting from the 'offset')
           to the buffer.

    \param[IN] buf          Pointer to the buffer where the data is copied to.
    \param[IN] buf_size     The size of the buffer.
    \param[IN] type         Feature-Type of the data bytes.
    \param[IN] offset       The reading offset of the Feature-Type data bytes.
    \param[OUT] status      The result of copying the data bytes to the buffer.
*/
static void gaiaCorePlugin_CopyUserFeature(uint8 *buf, const uint16 buf_size, gaia_user_feature_type_t type, const uint16 offset, gaia_get_user_feature_reading_status_t *status)
{
    const gaia_user_defined_feature_data_t *list_ptr = gaia_user_feature_linked_list;
    gaia_user_defined_feature_data_t app_feature_list;
    uint16 remaining_buf_size = buf_size;
    uint16 offset_in_the_list = offset;
    uint16 next_offset = 0;
    uint16 buf_ptr = 0;
    uint16 buf_used = 0;

    DEBUG_LOG_DEBUG("gaiaCorePlugin_CopyUserFeature: (type:%02X, offset:%04X, buf_size:%u)", (uint8) type, offset, buf_size);

    if (type != gaia_user_feature_type_start_from_zero)
    {
        /* Move forward the linked list pointer to the specified Feature-Type. */
        while (list_ptr)
        {
            if (list_ptr->type == type)
                break;
            list_ptr = list_ptr->next;
        }
    }
    app_feature_list.type = type;

    status->more_data = FALSE;
    while (list_ptr)
    {
        app_feature_list.type           = list_ptr->type;
        app_feature_list.string_list    = list_ptr->string_list;
        app_feature_list.num_of_strings = list_ptr->num_of_strings;

        if (0 < remaining_buf_size)
        {
            next_offset = gaiaCorePlugin_GetFormattedDataBytesFromStringList(&buf[buf_ptr], remaining_buf_size, &app_feature_list, offset_in_the_list, &buf_used);

            DEBUG_LOG_DEBUG("gaiaCorePlugin_CopyUserFeature, buf_used:%u, next_offset:%u", buf_used, next_offset);
            buf_ptr += buf_used;
            PanicFalse(buf_used <= remaining_buf_size);     /* Buffer overrun check. */
            remaining_buf_size -= buf_used;
            if (next_offset != 0)   /* There are some remaining part of the list that does not fit into the buffer? */
            {
                status->more_data = TRUE;
                offset_in_the_list = next_offset;
                break;              /* The buffer is full. */
            }
            /* There are some space in the buffer yet. Try to fill it with another list data if any. */
            offset_in_the_list = 0;
        }
        else
        {
            status->more_data = TRUE;
            break;              /* The buffer is full. */
        }
        list_ptr = list_ptr->next;
    }

    status->feature_type = app_feature_list.type;
    status->next_offset  = offset_in_the_list;
    status->buf_used     = buf_ptr;
    DEBUG_LOG_DEBUG("gaiaCorePlugin_CopyUserFeature, MoreData:%u, Type:%02X, next_offset:%04X\n", status->more_data, (uint8) status->feature_type, status->next_offset);
}

static void gaiaCorePlugin_SendUserFeatureResponse(GAIA_TRANSPORT *t, bool next_cmd, uint8 type, uint16 offset)
{
    const uint8 flag_more_data = 0x01;
    uint8 pdu_id = (next_cmd)? get_user_feature_next : get_user_feature;
    uint16 payload_size = GaiaFramework_GetPacketSpace(t);
    uint32 value;

    PanicFalse(Gaia_TransportGetInfo(t, GAIA_TRANSPORT_PAYLOAD_SIZE, &value));

    {
        uint8 *response = GaiaFramework_CreatePacket(t, GAIA_CORE_FEATURE_ID, pdu_id, payload_size);
        PanicNull(response);
        gaia_get_user_feature_reading_status_t reading_status =
        {
            .feature_type   = (gaia_user_feature_type_t) type,
            .next_offset    = 0,
        };

        DEBUG_LOG_DEBUG("gaiaCorePlugin_SendUserFeatureResponse, (type:%02X, offset:%04X)", (uint8)type, offset);
        if (gaia_user_feature_linked_list)
        {
            gaiaCorePlugin_CopyUserFeature(&response[4], payload_size - 4, (gaia_user_feature_type_t) type, offset, &reading_status);
            PanicFalse(reading_status.buf_used <= (payload_size - 4));
            DEBUG_LOG_DEBUG("gaiaCorePlugin_SendUserFeatureResponse, MoreData:%u, Type:%02X, NextOffset:%04X, BufUsed:%u",
                            reading_status.more_data, (uint8)reading_status.feature_type, reading_status.next_offset, reading_status.buf_used);
        }
        else
            DEBUG_LOG_INFO("gaiaCorePlugin_SendUserFeatureResponse, No User Feature Lists data are set!");


        if (reading_status.more_data)
        {
            response[0] = flag_more_data;                               /* Bitflag field (LSB: 'MoreData' bit) */
            response[1] = reading_status.feature_type;                  /* Reading Status[0] */
            response[2] = (uint8)(reading_status.next_offset >> 8);     /* Reading Status[1] */
            response[3] = (uint8)(reading_status.next_offset & 0xFF);   /* Reading Status[2] */
        }
        else
        {
            response[0] = 0x00;                                         /* Bitflag field (LSB: 'MoreData' bit) */
            response[1] = 0x00;                                         /* Reading Status[0] */
            response[2] = 0x00;                                         /* Reading Status[1] */
            response[3] = 0x00;                                         /* Reading Status[2] */
        }

        GaiaFramework_FlushPacket(t, (4 + reading_status.buf_used), response);
        DEBUG_LOG_INFO("gaiaCorePlugin_SendUserFeatureResponse, rsp[0-3] %02X %02X %02X %02X, Size:%u",
                       response[0], response[1], response[2], response[3], (4 + reading_status.buf_used));
    }
}

static void gaiaCorePlugin_GetUserFeature(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    UNUSED(payload);

    if (payload_length == 0)
    {
        DEBUG_LOG_INFO("gaiaCorePlugin_GetUserFeature");
        gaiaCorePlugin_SendUserFeatureResponse(t, FALSE, 0, 0);
    }
    else
    {
        DEBUG_LOG_ERROR("gaiaCorePlugin_GetUserFeature, %d bytes payload but this cmd has no parameters!", payload_length);
        GaiaFramework_SendError(t, GAIA_CORE_FEATURE_ID, get_user_feature, invalid_parameter);
    }
}

static void gaiaCorePlugin_GetUserFeatureNext(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    if (payload_length == 3)
    {
        uint8 usr_def_feature_type = payload[0];
        uint16 offset = ((uint16)payload[1] << 8) | payload[2];

        DEBUG_LOG_INFO("gaiaCorePlugin_GetUserFeatureNext, (%02X %02X %02X)", payload[0], payload[1], payload[2]);
        gaiaCorePlugin_SendUserFeatureResponse(t, TRUE, usr_def_feature_type, offset);
    }
    else
    {
        DEBUG_LOG_ERROR("gaiaCorePlugin_GetUserFeatureNext, Invalid command parameter length: %u", payload_length);
        GaiaFramework_SendError(t, GAIA_CORE_FEATURE_ID, get_user_feature_next, invalid_parameter);
    }
}

static void gaiaCorePlugin_GetDeviceBluetoothAddress(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    UNUSED(payload);
    
    if (payload_length == 0)
    {
        bdaddr bd_addr = {0};

        if (appDeviceGetMyBdAddr(&bd_addr))
        {
            uint8 response[SIZE_DEVICE_BD_ADDR];
            
            response[0] = bd_addr.nap >> 8;
            response[1] = bd_addr.nap;
            response[2] = bd_addr.uap;
            response[3] = bd_addr.lap >> 16;
            response[4] = bd_addr.lap >> 8;
            response[5] = bd_addr.lap;
            
            DEBUG_LOG_INFO("gaiaCorePlugin_GetDeviceBluetoothAddress: %02X:%02X:%02X:%02X:%02X:%02X",
                response[0], response[1], response[2], response[3], response[4], response[5]);
                
            GaiaFramework_SendResponse(t, GAIA_CORE_FEATURE_ID, get_device_bluetooth_address, SIZE_DEVICE_BD_ADDR, response);
        }
        else
        {
            DEBUG_LOG_ERROR("gaiaCorePlugin_GetDeviceBluetoothAddress: not available");
            GaiaFramework_SendError(t, GAIA_CORE_FEATURE_ID, get_device_bluetooth_address, failed_insufficient_resources);
        }
    }
    else
    {
        DEBUG_LOG_ERROR("gaiaCorePlugin_GetDeviceBluetoothAddress: payload_length %u, expected 0", payload_length);
        GaiaFramework_SendError(t, GAIA_CORE_FEATURE_ID, get_device_bluetooth_address, invalid_parameter);
    }
}

static void gaiaCorePlugin_SendAllNotifications(GAIA_TRANSPORT *t)
{
    DEBUG_LOG("gaiaCorePlugin_SendAllNotifications");

    if (charger_client_is_registered)
    {
        current_charger_plugged_in_state = Charger_IsConnected();
        gaiaCorePlugin_SendChargerStatusNotification(t, current_charger_plugged_in_state);
    }
}

static void gaiaCorePlugin_SendChargerStatusNotification(GAIA_TRANSPORT *t, bool plugged)
{
    UNUSED(t);
    uint8 payload = (uint8)plugged;

    DEBUG_LOG("gaiaCorePlugin_SendChargerStatusNotification");
    GaiaFramework_SendNotification(GAIA_CORE_FEATURE_ID, charger_status_notification, sizeof(payload), &payload);
}

static void gaiaCorePlugin_ChargerTask(Task task, MessageId message_id, Message message)
{
    UNUSED(task);
    UNUSED(message);

    bool charger_plugged = current_charger_plugged_in_state;

    switch(message_id)
    {
        case CHARGER_MESSAGE_DETACHED:
            charger_plugged = FALSE;
            break;

        case CHARGER_MESSAGE_ATTACHED:
        case CHARGER_MESSAGE_COMPLETED:
        case CHARGER_MESSAGE_CHARGING_OK:
        case CHARGER_MESSAGE_CHARGING_LOW:
            charger_plugged = TRUE;
            break;

        default:
            DEBUG_LOG("gaiaCorePlugin_ChargerTask, Unknown charger message");
            break;
    }

    if (charger_plugged != current_charger_plugged_in_state)
    {
        current_charger_plugged_in_state = charger_plugged;
        gaiaCorePlugin_SendChargerStatusNotification(0, current_charger_plugged_in_state);
    }
}

static uint8 gaiaCorePlugin_NumberOfFeaturesReported(uint8 payload_length)
{
    /* Payload length of the packet minus the one byte for the more to come indication */
    return ((payload_length - MORE_TO_COME_PAYLOAD_LENGTH) / NUM_OF_BYTES_PER_FEATURE);
}

static bool gaiaCorePlugin_GetSupportedFeaturesPayload(uint16 payload_length, uint8 *payload)
{
    uint16 payload_index;
    feature_list_handle_t *handle = NULL;
    bool status = TRUE;

    DEBUG_LOG_INFO("gaiaCorePlugin_GetSupportedFeaturesPayload");

    for (payload_index = 0; payload_index < payload_length; payload_index+=2 )
    {
        handle = GaiaFrameworkFeature_GetNextHandle(handle);
        if (handle)
        {
            status = GaiaFrameworkFeature_GetFeatureIdAndVersion(handle, &payload[payload_index], &payload[payload_index + 1]);
        }
        else
        {
            DEBUG_LOG_ERROR("gaiaCorePlugin_GetSupportedFeaturesPayload, FAILED");
            status = FALSE;
        }

        if (!status)
        {
            break;
        }
    }

    return status;
}

static void gaiaCorePlugin_PopulateSupportedFeaturesPacketAndSend(GAIA_TRANSPORT *transport, uint8 pdu_id)
{
    uint16 number_of_required_bytes = (num_of_remaining_features * NUM_OF_BYTES_PER_FEATURE) + 1;
    uint8 more_to_come = 0x01;
    uint8 *response_payload = NULL;
    uint16 response_payload_length = GaiaFramework_GetPacketSpace(transport);
    uint32 value;

    PanicFalse(Gaia_TransportGetInfo(transport, GAIA_TRANSPORT_PAYLOAD_SIZE, &value));
    response_payload_length = MIN(response_payload_length, ((uint16)value));

    if (number_of_required_bytes < response_payload_length)
    {
        response_payload_length = number_of_required_bytes;
        more_to_come = 0x00;
    }

    if (response_payload_length > 0)
    {
        response_payload = GaiaFramework_CreatePacket(transport, GAIA_CORE_FEATURE_ID, pdu_id, response_payload_length);
        PanicNull(response_payload);
        response_payload[0] = more_to_come;

        if (gaiaCorePlugin_GetSupportedFeaturesPayload((response_payload_length - 1), &response_payload[1]))
        {
            DEBUG_LOG("gaiaCorePlugin_PopulateSupportedFeaturesPacketAndSend, SUCCESS");
            GaiaFramework_FlushPacket(transport, response_payload_length, response_payload);
            num_of_remaining_features -= gaiaCorePlugin_NumberOfFeaturesReported(response_payload_length);
        }
        else
        {
            DEBUG_LOG("gaiaCorePlugin_PopulateSupportedFeaturesPacketAndSend, FAILED");
            GaiaFramework_SendError(transport, GAIA_CORE_FEATURE_ID, pdu_id, 0);
        }
    }
    else
    {
        DEBUG_LOG("gaiaCorePlugin_PopulateSupportedFeaturesPacketAndSend, nothing left to send");
        GaiaFramework_SendResponse(transport, GAIA_CORE_FEATURE_ID, pdu_id, response_payload_length, NULL);
        num_of_remaining_features = 0;
    }
}

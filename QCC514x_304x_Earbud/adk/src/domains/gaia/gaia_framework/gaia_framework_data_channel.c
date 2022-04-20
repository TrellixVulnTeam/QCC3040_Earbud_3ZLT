/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Functions for setting up and shutting down Gaia data transfer channel.
*/

#include "gaia_core_plugin.h"
#include "gaia_framework_data_channel.h"

#include <panic.h>
#include <stdlib.h>

/* Enable debug log outputs with per-module debug log levels.
 * The log output level for this module can be changed with the PyDbg command:
 *      >>> apps1.log_level("gaia_fw_data_ch", 3)
 * Where the second parameter value means:
 *      0:ERROR, 1:WARN, 2:NORMAL(= INFO), 3:VERBOSE(= DEBUG), 4:V_VERBOSE, 5:V_V_VERBOSE
 * See 'logging.h' and PyDbg 'log_level()' command descriptions for details.
 */
#define DEBUG_LOG_MODULE_NAME gaia_fw_data_ch
#include <logging.h>
DEBUG_LOG_DEFINE_LEVEL_VAR

#ifdef ENABLE_GAIA_FRAMEWORK_DATA_CH_PANIC
#define GAIA_FRAMEWORK_DATA_CH_PANIC() Panic()
#else
#define GAIA_FRAMEWORK_DATA_CH_PANIC()
#endif


/*! \brief Types of transport
*/
typedef enum
{
    /*! No data channel is set up yet. */
    data_ch_status_none = 0,
    /*! Allocation of data channel has been requested. */
    data_ch_status_allocating,
    /*! A data chanel is listening for incoming connect_req. */
    data_ch_status_listening,
    /*! The SDP record of the data channel is visible to the peer. */
    data_ch_status_listening_and_visible,
    /*! The data channel is established but the SDP record is not withdrawn yet. */
    data_ch_status_connected,
    /*! The data channel is established and the SDP record is being withdrawn. */
    data_ch_status_connected_removing_sdp,
    /*! Ready for data transfer. */
    data_ch_status_data_transfer_ready,
    /*! Deallocation of data channel has been requested. */
    data_ch_status_deallocating,

    /*! Total number of the data channel states. */
    number_of_data_ch_state,
} data_transfer_channel_states_t;


/*! \brief Session instance that binds a Session ID to a Gaia Feature.
*/
typedef struct __session_instance_t
{
    /*! A unique 16-bit ID that distinguishes each data transfer session. */
    gaia_data_transfer_session_id_t session_id;
    /*! Feature ID, which tells a Gaia Feature being bound with this session. */
    uint8                           feature_id;

    /*! Pointer to the table of the data channel handler functions. */
    gaia_framework_data_channel_functions_t *functions;

    /*! Transport type specified by Gaia 'Data Transfer Setup' command. */
    core_plugin_transport_type_t    transport_type;

    GAIA_TRANSPORT *                transport;

    /*! Transform of a stream if in use, otherwise this is NULL. */
    Transform                       data_channel_transform;

    struct __session_instance_t *next;
} session_instance_t;

/*! \brief Session queue for serialising requests and their responsees.
*/
typedef struct __session_queue_t
{
    /*! A unique 16-bit ID that distinguishes each data transfer session. */
    gaia_data_transfer_session_id_t session_id;

    /*! Pointer to the next element. */
    struct __session_queue_t    *next;
} session_queue_t;


/*! \brief Get the Gaia Framework status code from a data transfer status.

    \param status_code  Status code of a data transfer session.

    \return             Gaia Framework status code.
*/
static uint8 gaiaFrameworkDataChannel_GetGaiaStatusFromDataTransferStatus(data_transfer_status_code_t status_code);


/*! \brief Find the session instance for the Gaia Feature (with a Session ID as the key).

    \param feature_id   Feautue ID that represents a Gaia Feature, which a data
                        transfer session is bound to.

    \return Returns the the pointer to the session instance if exists, otherwise NULL.
*/
static session_instance_t* gaiaFrameworkDataChannel_FindSessionInstance(gaia_data_transfer_session_id_t session_id);


/*! \brief Return the first Session Instance of the linked list.
*/
static session_instance_t* gaiaFrameworkDataChannel_GetFirstSessionInstance(void);


/*! \brief Add a session instance to the list.

    \param session_id       A 16-bit ID that represents a data transfer session.

    \param feature_id       Feautue ID that represents a Gaia Feature, which will
                            be bound with the data transfer session.

    \param functions        Pointer to the table of data channel handler functions.

    \return Returns TRUE on success, otherwise FALSE.
*/
static bool gaiaFrameworkDataChannel_AddSessionInstance(GAIA_TRANSPORT *t, gaia_data_transfer_session_id_t session_id, uint8 feature_id, const gaia_framework_data_channel_functions_t *functions);


/*! \brief Delete the specified session instance from the linked list.

    \param session_id   A 16-bit ID that represents a data transfer session.

    \return Returns TRUE on success, otherwise FALSE.
*/
static bool gaiaFrameworkDataChannel_DeleteSessionInstance(gaia_data_transfer_session_id_t session_id);


/*! \brief Send 'Data Transfer Setup' Response to the mobile app.

    \param session_id   A 16-bit ID that represents a data transfer session.

    This function sends a Response to the 'Data Transfer Setup' command to the
    mobile app.
*/
static void gaiaFrameworkDataChannel_SendDataTransferSetupResponse(gaia_data_transfer_session_id_t session_id);



/*! \brief The root of the Session Instance linked list. */
static session_instance_t *session_instance_linked_list = NULL;



static uint8 gaiaFrameworkDataChannel_GetGaiaStatusFromDataTransferStatus(data_transfer_status_code_t status_code)
{
    uint8 gaia_fw_status = GAIA_STATUS_INCORRECT_STATE;

    switch (status_code)
    {
        case data_transfer_status_success:
            gaia_fw_status = GAIA_STATUS_SUCCESS;
            break;
        case data_transfer_no_more_data:
            gaia_fw_status = GAIA_STATUS_SUCCESS;   /* NB: 'No more data' is a normal case. */
            break;
        case data_transfer_invalid_parameter:
            gaia_fw_status = GAIA_STATUS_INVALID_PARAMETER;
            break;
        case data_transfer_status_insufficient_resource:
            gaia_fw_status = GAIA_STATUS_INSUFFICIENT_RESOURCES;
            break;

        case data_transfer_status_failure_with_unspecified_reason:
            /* Fall through */
        case data_transfer_status_failed_to_get_data_info:
            /* Fall through */
        case data_transfer_status_invalid_sink:
            /* Fall through */
        case data_transfer_status_invalid_source:
            /* Fall through */
        case data_transfer_status_invalid_stream:
            gaia_fw_status = GAIA_STATUS_INCORRECT_STATE;
            break;

        default:
            gaia_fw_status = GAIA_STATUS_INCORRECT_STATE;
            DEBUG_LOG_ERROR("GaiaFW DataTransfer: PANIC Unknown DataTransfer Status code: %d", status_code);
            GAIA_FRAMEWORK_DATA_CH_PANIC();
            break;
    }

    return gaia_fw_status;
}


static session_instance_t* gaiaFrameworkDataChannel_FindSessionInstance(gaia_data_transfer_session_id_t session_id)
{
    session_instance_t *instance = session_instance_linked_list;

    while (instance != NULL)
    {
        if (instance->session_id == session_id)
        {
            return instance;
        }
        instance = instance->next;
    }
    return NULL;
}


static session_instance_t* gaiaFrameworkDataChannel_GetFirstSessionInstance(void)
{
    return session_instance_linked_list;
}


static bool gaiaFrameworkDataChannel_AddSessionInstance(GAIA_TRANSPORT *t, gaia_data_transfer_session_id_t session_id, uint8 feature_id, const gaia_framework_data_channel_functions_t *functions)
{
    session_instance_t *instance = session_instance_linked_list;

    /* Look for the end of the linked list. */
    while (instance != NULL)
    {
        if (instance->next == NULL)
        {
            break;
        }
        instance = instance->next;
    }

    {
        session_instance_t *sr;

        sr = (session_instance_t*) PanicUnlessMalloc(sizeof(session_instance_t));
        sr->session_id = session_id;
        sr->feature_id = feature_id;
        sr->functions = (gaia_framework_data_channel_functions_t*) functions;
        sr->transport = t;
        sr->next = NULL;

        if (session_instance_linked_list == NULL)
        {
            session_instance_linked_list = sr;
        }
        else
        {
            instance->next = sr;
        }
    }
    return TRUE;
}


static bool gaiaFrameworkDataChannel_DeleteSessionInstance(gaia_data_transfer_session_id_t session_id)
{
    session_instance_t *instance = session_instance_linked_list;
    session_instance_t *prev = NULL;

    while (instance != NULL)
    {
        if (instance->session_id == session_id)
        {
            if (prev == NULL)
            {
                session_instance_linked_list = NULL;
            }
            else
            {
                prev->next = instance->next;
            }
            free(instance);
            return TRUE;
        }
        prev = instance;
        instance = instance->next;
    }
    return FALSE;
}


static void gaiaFrameworkDataChannel_SendDataTransferSetupResponse(gaia_data_transfer_session_id_t session_id)
{
    session_instance_t  *instance;
    instance = gaiaFrameworkDataChannel_FindSessionInstance(session_id);
    if (instance != NULL)
    {
        uint8 rsp_payload[2];

        rsp_payload[0] = (uint8) ((session_id & 0xFF00) >> 8);
        rsp_payload[1] = (uint8)  (session_id & 0x00FF);

        GaiaFramework_SendResponse(instance->transport, GAIA_CORE_FEATURE_ID, data_transfer_setup, sizeof(rsp_payload), rsp_payload);
    }
    else
    {
        DEBUG_LOG_ERROR("GaiaFW DataTransfer: Data Transfer Setup Rsp, SessionID: 0x%04X has no instance!", session_id);
        GAIA_FRAMEWORK_DATA_CH_PANIC();
    }

    DEBUG_LOG_DEBUG("GaiaFW DataTransfer: Data Transfer Setup Rsp, SessionID: 0x%04X", session_id);
}


static gaia_framework_data_channel_functions_t* gaiaFramework_GetDataTransferFunctionTable(gaia_data_transfer_session_id_t session_id)
{
    session_instance_t *instance = session_instance_linked_list;

    while (instance != NULL)
    {
        if (instance->session_id == session_id)
        { 
            return instance->functions;
        }
        instance = instance->next;
    }
    return NULL;
}


gaia_data_transfer_session_id_t GaiaFramework_CreateDataTransferSession(GAIA_TRANSPORT *t, uint8 feature_id, const gaia_framework_data_channel_functions_t *functions)
{
    static gaia_data_transfer_session_id_t session_id_counter = 0x0000;

    /* Create a new Session ID */
    session_id_counter = (session_id_counter + 1) & 0xFFFF;
    if (session_id_counter == INVALID_DATA_TRANSFER_SESSION_ID)
    {
        session_id_counter += 1;
    }

    /* Create a new Session Instance */
    gaiaFrameworkDataChannel_AddSessionInstance(t, session_id_counter, feature_id, functions);

    return session_id_counter;
}


void GaiaFramework_DeleteDataTransferSession(gaia_data_transfer_session_id_t session_id)
{
    bool result;

    result = gaiaFrameworkDataChannel_DeleteSessionInstance(session_id);
    if (result != TRUE)
    {
        DEBUG_LOG_ERROR("GaiaFW DataTransfer: DeleteSession failed to remove Session ID:0x%04X", session_id);
        GAIA_FRAMEWORK_DATA_CH_PANIC();
    }
}


bool GaiaFramework_DataTransferSetup(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    UNUSED(payload_length);
    bool result = FALSE;
    core_plugin_transport_type_t transport_type = payload[2];
    gaia_data_transfer_session_id_t session_id = (uint16)payload[0] << 8 | payload[1];
    session_instance_t *instance = NULL;

    DEBUG_LOG_DEBUG("GaiaFramework_DataTransferSetup");

    /* Check if the Session ID is valid (= registered) or not. */
    instance = gaiaFrameworkDataChannel_FindSessionInstance(session_id);
    if (instance == NULL)
    {
        GaiaFramework_SendError(t, GAIA_CORE_FEATURE_ID, data_transfer_setup, GAIA_STATUS_INVALID_PARAMETER);
    }
    else
    {
        instance->transport_type = transport_type;

        switch (transport_type)
        {
            case transport_gaia_command_response:
                /* No need to open a new link.
                * As this transport uses existing Gaia link for data transfer with
                * the payloads of 'Data Transfer Get/Set' Commands. */
                gaiaFrameworkDataChannel_SendDataTransferSetupResponse(session_id);
                result = TRUE;
                break;

            default:
                DEBUG_LOG_WARN("GaiaFramework_DataTransferSetup: Invalid transport type:%d", transport_type);
                GaiaFramework_SendError(t, GAIA_CORE_FEATURE_ID, data_transfer_setup, GAIA_STATUS_INVALID_PARAMETER);
                break;
        }
    }

    return result;
}


bool GaiaFramework_DataTransferGet(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    gaia_data_transfer_session_id_t session_id;
    session_instance_t *instance = NULL;

    DEBUG_LOG_DEBUG("GaiaFramework_DataTransferGet");
    if (payload_length < 2)
    {
        DEBUG_LOG_WARN("GaiaFramework_DataTransferGet, WARNING: Invalid param length %u", payload_length);
        return FALSE;
    }

    /* Check if the Session ID is valid (= registered) or not. */
    session_id = (uint16)payload[0] << 8 | payload[1];
    instance = gaiaFrameworkDataChannel_FindSessionInstance(session_id);
    if (instance == NULL)
    {
        GaiaFramework_SendError(t, GAIA_CORE_FEATURE_ID, data_transfer_get, GAIA_STATUS_INVALID_PARAMETER);
        return FALSE;
    }
    else
    {
        uint32 start_offset   = (uint32)payload[2] << 24 | (uint32)payload[3] << 16 | (uint32)payload[4] << 8 | (uint32)payload[5];
        uint32 requested_size = (uint32)payload[6] << 24 | (uint32)payload[7] << 16 | (uint32)payload[8] << 8 | (uint32)payload[9];
        uint8 *tx_buf;
        uint32 value;
        uint16 payload_size = GaiaFramework_GetPacketSpace(t);

        PanicFalse(Gaia_TransportGetInfo(t, GAIA_TRANSPORT_PAYLOAD_SIZE, &value));
        if ((2 + requested_size) < value)
            payload_size = MIN(payload_size, (2 + requested_size));
        else
            payload_size = MIN(payload_size, ((uint16)value));

        tx_buf = GaiaFramework_CreatePacket(t, GAIA_CORE_FEATURE_ID, data_transfer_get, payload_size);
        PanicNull(tx_buf);

        tx_buf[0] = (uint8) ((session_id & 0xFF00) >> 8);
        tx_buf[1] = (uint8)  (session_id & 0x00FF);
        {
            uint16 size_used;
            data_transfer_status_code_t status;
            gaia_framework_data_channel_functions_t *functions = gaiaFramework_GetDataTransferFunctionTable(session_id);
            PanicNull(functions);

            /* Call the registered 'get_transfer_data' function */
            status = functions->get_transfer_data(start_offset, requested_size, (payload_size - 2), &tx_buf[2], &size_used);

            if (status == data_transfer_status_success)
                GaiaFramework_FlushPacket(t, (2 + size_used), tx_buf);
            else
            {
                uint8 gaia_status = gaiaFrameworkDataChannel_GetGaiaStatusFromDataTransferStatus(status);
                GaiaFramework_SendError(t, GAIA_CORE_FEATURE_ID, data_transfer_get, gaia_status);
                return FALSE;
            }
        }
    }
    return TRUE;
}


bool GaiaFramework_DataTransferSet(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    gaia_data_transfer_session_id_t session_id = (uint16)payload[0] << 8 | payload[1];
    session_instance_t *instance = NULL;

    DEBUG_LOG_DEBUG("GaiaFramework_DataTransferSet");

    /* Check if the Session ID is valid (= registered) or not. */
    instance = gaiaFrameworkDataChannel_FindSessionInstance(session_id);
    if (instance == NULL || payload_length <= 6)
    {
        GaiaFramework_SendError(t, GAIA_CORE_FEATURE_ID, data_transfer_set, GAIA_STATUS_INVALID_PARAMETER);
    }
    else
    {
        uint32 start_offset = (uint32)payload[2] << 24 | (uint32)payload[3] << 16 | (uint32)payload[4] << 8 | (uint32)payload[5];
        data_transfer_status_code_t status;
        gaia_framework_data_channel_functions_t *functions = gaiaFramework_GetDataTransferFunctionTable(session_id);
        PanicNull(functions);

        /* Call the registered 'set_transfer_data' function */
        status = functions->set_transfer_data(start_offset, (payload_length - 6), &payload[6]);

        if (status == data_transfer_status_success)
        {
            uint8 rsp_payload[2];

            rsp_payload[0] = (uint8) ((session_id & 0xFF00) >> 8);
            rsp_payload[1] = (uint8)  (session_id & 0x00FF);
            GaiaFramework_SendResponse(t, GAIA_CORE_FEATURE_ID, data_transfer_set, sizeof(rsp_payload), rsp_payload);
        }
        else
        {
            uint8 gaia_status = gaiaFrameworkDataChannel_GetGaiaStatusFromDataTransferStatus(status);
            GaiaFramework_SendError(t, GAIA_CORE_FEATURE_ID, data_transfer_set, gaia_status);
            return FALSE;
        }
    }
    return TRUE;
}


void GaiaFramework_ShutDownDataChannels(void)
{
    session_instance_t *instance;
    bool result;
    
    while (TRUE)
    {
        instance = gaiaFrameworkDataChannel_GetFirstSessionInstance();
        if (instance == NULL)
        {
            break;
        }

        result = gaiaFrameworkDataChannel_DeleteSessionInstance(instance->session_id);
        if (result == FALSE)
        {
            DEBUG_LOG_ERROR("GaiaFW DataTransfer: PANIC Broken instance linked list: SessionID: 0x%04X", instance->session_id);
            GAIA_FRAMEWORK_DATA_CH_PANIC();
        }
    }
}

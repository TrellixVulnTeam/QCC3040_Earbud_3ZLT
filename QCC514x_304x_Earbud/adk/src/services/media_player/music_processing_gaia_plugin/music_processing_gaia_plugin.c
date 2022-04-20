/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Music processing gaia plugin component
*/

#if defined(INCLUDE_GAIA) && defined(INCLUDE_MUSIC_PROCESSING)

#include <panic.h>
#include <stdlib.h>
#include <byte_utils.h>
#include <kymera.h>

#include "music_processing_gaia_plugin.h"
#include "music_processing.h"

/* This value is the total bytes of the types of the data required per band of EQ */
#define TOTAL_BYTES_OF_EQ_INFO_PER_BAND 7
#define MusicProcessing_NumberOfBands(START_BAND, END_BAND) ((END_BAND - START_BAND) + 1)


static gaia_framework_command_status_t musicProcessingGaiaPlugin_MainHandler(GAIA_TRANSPORT *t, uint8 pdu_id, uint16 payload_length, const uint8 *payload);

static void musicProcessingGaiaPlugin_GetEqState(GAIA_TRANSPORT *t);

static void musicProcessingGaiaPlugin_GetAvailableEqPresets(GAIA_TRANSPORT *t);

static void musicProcessingGaiaPlugin_GetEqSet(GAIA_TRANSPORT *t);

static void musicProcessingGaiaPlugin_SetEqSet(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload);

static void musicProcessingGaiaPlugin_GetUserSetNumberOfBands(GAIA_TRANSPORT *t);

static void musicProcessingGaiaPlugin_GetUserEqSetConfiguration(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload);

static void musicProcessingGaiaPlugin_SetUserEqSetConfiguration(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload);

static void musicProcessingGaiaPlugin_HandleKymeraMessage(Task task, MessageId id, Message message);

static Task musicProcessingGaiaPlugin_KymeraStateTask(void);

static void musicProcessingGaiaPlugin_SendAllNotifications(GAIA_TRANSPORT *t);

static void musicProcessingGaiaPlugin_EqStateChange(void);

static void musicProcessingGaiaPlugin_EqSetChange(void);

inline static void musicProcessingGaiaPlugin_UserEqBandChange(uint8 payload_length, uint8 * payload);

inline static void musicProcessingGaiaPlugin_PopulateEqBandInformation(uint8 start_band, uint8 end_band, uint8 * response_payload);

inline static void musicProcessingGaiaPlugin_LoadEqParametersToPayload(uint8 band, uint8 * response_payload);

inline static void musicProcessingGaiaPlugin_ConvertEqGains(uint8 number_of_bands, const uint8 * payload, int16 * gain);

/*! \todo This is a temporary function to ensure the data passed by the application match what we expect at the app */
static void musicProcessingGaiaPlugin_PrintData(uint16 payload_length, const uint8 *payload);

static uint8 eq_state_active;

static uint8 active_preset = 0;

static const TaskData kymera_task = {musicProcessingGaiaPlugin_HandleKymeraMessage};


bool MusicProcessingGaiaPlugin_Init(Task init_task)
{
    UNUSED(init_task);

    static const gaia_framework_plugin_functions_t functions =
    {
        .command_handler = musicProcessingGaiaPlugin_MainHandler,
        .send_all_notifications = musicProcessingGaiaPlugin_SendAllNotifications,
        .transport_connect = NULL,
        .transport_disconnect = NULL,
    };

    DEBUG_LOG_VERBOSE("MusicProcessingGaiaPlugin_Init");

    eq_state_active = MusicProcessing_IsEqActive();

    Kymera_RegisterNotificationListener(musicProcessingGaiaPlugin_KymeraStateTask());

    GaiaFramework_RegisterFeature(GAIA_MUSIC_PROCESSING_FEATURE_ID, MUSIC_PROCESSING_GAIA_PLUGIN_VERSION, &functions);

    return TRUE;
}

void MusicProcessingGaiaPlugin_EqActiveChanged(uint8 eq_active)
{
    DEBUG_LOG_VERBOSE("MusicProcessingGaiaPlugin_EqActiveChanged");

    eq_state_active = eq_active;

    musicProcessingGaiaPlugin_EqStateChange();
}

void MusicProcessingGaiaPlugin_PresetChanged(uint8 preset_id)
{
    DEBUG_LOG_VERBOSE("MusicProcessingGaiaPlugin_EqActiveChanged, new preset ID is: %d", preset_id);

    active_preset = preset_id;

    musicProcessingGaiaPlugin_EqSetChange();
}

void MusicProcessingGaiaPlugin_BandGainsChanged(uint8 num_bands, uint8 start_band)
{
    DEBUG_LOG_VERBOSE("MusicProcessingGaiaPlugin_BandGainsChanged");

    uint8 band_id;
    uint8 * payload = PanicUnlessMalloc(num_bands + 1);

    payload[0] = num_bands;

    for (band_id = 0; band_id < num_bands; band_id++)
    {
        payload[1+band_id] = start_band + band_id;
    }

    musicProcessingGaiaPlugin_UserEqBandChange((num_bands + 1), payload);

    free(payload);
}

/*! \brief Function pointer definition for the command handler

    \param transport    Transport type

    \param pdu_id       PDU specific ID for the message

    \param length       Length of the payload

    \param payload      Payload data

    \return Gaia framework command status code
*/
static gaia_framework_command_status_t musicProcessingGaiaPlugin_MainHandler(GAIA_TRANSPORT *t, uint8 pdu_id, uint16 payload_length, const uint8 *payload)
{
    DEBUG_LOG_VERBOSE("musicProcessingGaiaPlugin_MainHandler, transport %p, pdu_id %u", t, pdu_id);

    switch (pdu_id)
    {
        case get_eq_state:
            musicProcessingGaiaPlugin_GetEqState(t);
            break;

        case get_available_eq_presets:
            musicProcessingGaiaPlugin_GetAvailableEqPresets(t);
            break;

        case get_eq_set:
            musicProcessingGaiaPlugin_GetEqSet(t);
            break;

        case set_eq_set:
            musicProcessingGaiaPlugin_SetEqSet(t, payload_length, payload);
            break;

        case get_user_set_number_of_bands:
            musicProcessingGaiaPlugin_GetUserSetNumberOfBands(t);
            break;

        case get_user_eq_set_configuration:
            musicProcessingGaiaPlugin_GetUserEqSetConfiguration(t, payload_length, payload);
            break;

        case set_user_eq_set_configuration:
            musicProcessingGaiaPlugin_SetUserEqSetConfiguration(t, payload_length, payload);
            break;

        default:
            DEBUG_LOG_VERBOSE("musicProcessingGaiaPlugin_MainHandler, unhandled call for %u", pdu_id);
            return command_not_handled;
    }

    musicProcessingGaiaPlugin_PrintData(payload_length, payload);

    return command_handled;
}

static void musicProcessingGaiaPlugin_PrintData(uint16 payload_length, const uint8 *payload)
{
    DEBUG_LOG_VERBOSE("musicProcessingGaiaPlugin_PrintData, payload length is %d", payload_length);

    if (payload_length > 0)
    {
        uint8 i;
        for (i = 0; i < payload_length; i++)
        {
             DEBUG_LOG_VERBOSE("musicProcessingGaiaPlugin_PrintData payload[%d] = %d", i, payload[i]);
        }
    }
}

/*! \brief Command to decide whether the user can interact with the User EQ settings (predefined or user set)

    \param transport    Transport type
*/
static void musicProcessingGaiaPlugin_GetEqState(GAIA_TRANSPORT *t)
{
    uint8 eq_state = ((MusicProcessing_IsEqActive()) ? (1) : (0));

    DEBUG_LOG_VERBOSE("musicProcessingGaiaPlugin_GetEqState");

    GaiaFramework_SendResponse(t, GAIA_MUSIC_PROCESSING_FEATURE_ID, get_eq_state, sizeof(eq_state), &eq_state);
}

/*! \brief Command to find out the IDs of the supported presets. Each preset is identified by a number which you’ll convert to a string and present to the user

    \param transport    Transport type
*/
static void musicProcessingGaiaPlugin_GetAvailableEqPresets(GAIA_TRANSPORT *t)
{
    uint8 *eq_preset_information;
    uint8 num_of_banks = Kymera_GetNumberOfEqBanks();

    DEBUG_LOG_VERBOSE("musicProcessingGaiaPlugin_GetAvailableEqPresets");

    eq_preset_information = PanicUnlessMalloc(num_of_banks+1);

    eq_preset_information[0] = num_of_banks;
    eq_preset_information[1] = 0;

    if(num_of_banks > 2)
    {
        Kymera_PopulatePresets(&eq_preset_information[2]);
    }

    eq_preset_information[num_of_banks] = EQ_BANK_USER;

    DEBUG_LOG_DATA_VERBOSE(eq_preset_information, num_of_banks+1);

    GaiaFramework_SendResponse(t, GAIA_MUSIC_PROCESSING_FEATURE_ID, get_available_eq_presets, num_of_banks+1, eq_preset_information);

    free(eq_preset_information);
}

/*! \brief Command to find out what the currently selected preset (or User or off) is

    \param transport    Transport type
*/
static void musicProcessingGaiaPlugin_GetEqSet(GAIA_TRANSPORT *t)
{
    uint8 active_eq_type = MusicProcessing_GetActiveEqType();

    DEBUG_LOG_VERBOSE("musicProcessingGaiaPlugin_GetEqSet %d", active_eq_type);

    GaiaFramework_SendResponse(t, GAIA_MUSIC_PROCESSING_FEATURE_ID, get_eq_set, sizeof(active_eq_type), &active_eq_type);
}

/*! \brief Command to set the new preset value or user set or Off

    \param transport    Transport type
*/
static void musicProcessingGaiaPlugin_SetEqSet(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    bool error = TRUE;

    DEBUG_LOG_VERBOSE("musicProcessingGaiaPlugin_SetEqSet");

    if (payload_length > 0)
    {
        if (MusicProcessing_SetPreset(payload[0]))
        {
            GaiaFramework_SendResponse(t, GAIA_MUSIC_PROCESSING_FEATURE_ID, set_eq_set, 1, &payload[0]);
            error = FALSE;
        }
        else
        {
            DEBUG_LOG_ERROR("musicProcessingGaiaPlugin_SetEqSet, invalid preset ID %d", payload[0]);
        }
    }
    else
    {
        DEBUG_LOG_ERROR("musicProcessingGaiaPlugin_SetEqSet, no payload");
    }

    if (error)
    {
        GaiaFramework_SendError(t, GAIA_MUSIC_PROCESSING_FEATURE_ID, set_eq_set, invalid_parameter);
    }
}

/*! \brief Command to find out how many frequency bands the User set supports

    \param transport    Transport type
*/
static void musicProcessingGaiaPlugin_GetUserSetNumberOfBands(GAIA_TRANSPORT *t)
{
    uint8 number_of_active_bands = MusicProcessing_GetNumberOfActiveBands();

    DEBUG_LOG_VERBOSE("musicProcessingGaiaPlugin_GetUserSetNumberOfBands %d", number_of_active_bands);

    GaiaFramework_SendResponse(t, GAIA_MUSIC_PROCESSING_FEATURE_ID, get_user_set_number_of_bands, sizeof(number_of_active_bands), &number_of_active_bands);
}

/*! \brief Command to find out how many frequency bands the User set supports and the current gain value of each band

    \param transport    Transport type
*/
static void musicProcessingGaiaPlugin_GetUserEqSetConfiguration(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    bool error = TRUE;

    DEBUG_LOG_VERBOSE("musicProcessingGaiaPlugin_GetUserEqSetConfiguration");

    if (payload_length >= 2)
    {
        uint8 start_band = payload[0];
        uint8 end_band = payload[1];
        uint8 num_of_bands = MusicProcessing_GetNumberOfActiveBands();

        if ((start_band <= num_of_bands) && (end_band <= num_of_bands))
        {
            uint8 number_of_band_config_requested = MusicProcessing_NumberOfBands(start_band, end_band);

            DEBUG_LOG_VERBOSE("musicProcessingGaiaPlugin_GetUserEqSetConfiguration start %d end %d num %d num requested %d",
                    start_band, end_band, num_of_bands, number_of_band_config_requested);

            if (number_of_band_config_requested <= num_of_bands)
            {
                uint8 response_length = (number_of_band_config_requested * TOTAL_BYTES_OF_EQ_INFO_PER_BAND) + 2;
                uint8 *response_payload = PanicUnlessMalloc(response_length);

                response_payload[0] = start_band;
                response_payload[1] = end_band;

                musicProcessingGaiaPlugin_PopulateEqBandInformation(start_band, end_band, &response_payload[2]);

                DEBUG_LOG_VERBOSE("response_length %d", response_length);
                DEBUG_LOG_DATA_VERBOSE(response_payload, response_length);

                GaiaFramework_SendResponse(t, GAIA_MUSIC_PROCESSING_FEATURE_ID, get_user_eq_set_configuration, response_length, response_payload);
                error = FALSE;
                free(response_payload);
            }
            else
            {
                DEBUG_LOG_ERROR("musicProcessingGaiaPlugin_GetUserEqSetConfiguration, invalid number of band configurations requested");
            }
        }
        else
        {
            DEBUG_LOG_ERROR("musicProcessingGaiaPlugin_SetUserEqSetConfiguration, invalid start or end band");
        }
    }
    else
    {
        DEBUG_LOG_ERROR("musicProcessingGaiaPlugin_GetUserEqSetConfiguration, no payload");
    }

    if (error)
    {
        GaiaFramework_SendError(t, GAIA_MUSIC_PROCESSING_FEATURE_ID, get_user_eq_set_configuration, invalid_parameter);
    }
}

/*! \brief Command to set the gains of a specific set of bands

    \param transport    Transport type
*/
static void musicProcessingGaiaPlugin_SetUserEqSetConfiguration(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    bool error = TRUE;

    DEBUG_LOG_VERBOSE("musicProcessingGaiaPlugin_SetUserEqSetConfiguration");

    if (payload_length >= 2)
    {
        uint8 start_band = payload[0];
        uint8 end_band = payload[1];

        uint8 num_of_bands = MusicProcessing_GetNumberOfActiveBands();

        if ((start_band <= num_of_bands) && (end_band <= num_of_bands))
        {
            uint8 number_of_bands_to_be_modified = MusicProcessing_NumberOfBands(start_band, end_band);

            DEBUG_LOG_VERBOSE("musicProcessingGaiaPlugin_SetUserEqSetConfiguration start band %d end band %d num of bands %d",
                    start_band, end_band, number_of_bands_to_be_modified);

            if ((number_of_bands_to_be_modified <= num_of_bands) &&
                (payload_length == ((number_of_bands_to_be_modified * sizeof(int16) + 2))))
            {
                int16 *gains = PanicUnlessMalloc(sizeof(int16) * number_of_bands_to_be_modified);

                musicProcessingGaiaPlugin_ConvertEqGains(number_of_bands_to_be_modified, &payload[2], gains);

                if (MusicProcessing_SetUserEqBands(start_band, end_band, gains))
                {
                    GaiaFramework_SendResponse(t, GAIA_MUSIC_PROCESSING_FEATURE_ID, set_user_eq_set_configuration, 0, NULL);
                    error = FALSE;
                }

                free(gains);
            }
            else
            {
                DEBUG_LOG_ERROR("musicProcessingGaiaPlugin_SetUserEqSetConfiguration, invalid number of bands requested to be modified or payload length");
            }
        }
        else
        {
            DEBUG_LOG_ERROR("musicProcessingGaiaPlugin_SetUserEqSetConfiguration, invalid start or end band");
        }
    }
    else
    {
        DEBUG_LOG_ERROR("musicProcessingGaiaPlugin_SetUserEqSetConfiguration, no payload");
    }

    if (error)
    {
        GaiaFramework_SendError(t, GAIA_MUSIC_PROCESSING_FEATURE_ID, set_user_eq_set_configuration, invalid_parameter);
    }
}

/*! \brief Function that handles mkymera messages

    \param task     Kymera task

    \param id       Message ID

    \param message  Message
*/
static void musicProcessingGaiaPlugin_HandleKymeraMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);

    DEBUG_LOG_VERBOSE("musicProcessingGaiaPlugin_HandleKymeraMessage");

    switch(id)
    {
        case KYMERA_NOTIFICATION_EQ_AVAILABLE:
            MusicProcessingGaiaPlugin_EqActiveChanged(1);
            break;

        case KYMERA_NOTIFICATION_EQ_UNAVAILABLE:
            MusicProcessingGaiaPlugin_EqActiveChanged(0);
            break;

        case KYMERA_NOTIFCATION_USER_EQ_BANDS_UPDATED:
            {
                KYMERA_NOTIFCATION_USER_EQ_BANDS_UPDATED_T *msg = (KYMERA_NOTIFCATION_USER_EQ_BANDS_UPDATED_T *)message;
                DEBUG_LOG_VERBOSE("musicProcessingGaiaPlugin_HandleKymeraMessage USER_EQ_BANDS_UPDATED, send gaia notification");
                MusicProcessingGaiaPlugin_BandGainsChanged((msg->end_band - msg->start_band + 1), msg->start_band);
            }
            break;

        default:
            break;
    };
}

/*! \brief Function that provides the mkymera message handler

    \return task    Kymera Message Handler
*/
static Task musicProcessingGaiaPlugin_KymeraStateTask(void)
{
  return (Task)&kymera_task;
}

/*! \brief Function that sends all available notifications

    \param transport    Transport type
*/
static void musicProcessingGaiaPlugin_SendAllNotifications(GAIA_TRANSPORT *t)
{
    UNUSED(t);

    DEBUG_LOG_VERBOSE("musicProcessingGaiaPlugin_SendAllNotifications");

    musicProcessingGaiaPlugin_EqStateChange();
    musicProcessingGaiaPlugin_EqSetChange();
}

/*! \brief Gaia Client will be told if the User EQ is not present

    \param transport    Transport type
*/
static void musicProcessingGaiaPlugin_EqStateChange(void)
{
    DEBUG_LOG_VERBOSE("musicProcessingGaiaPlugin_EqStateChange");

    GaiaFramework_SendNotification(GAIA_MUSIC_PROCESSING_FEATURE_ID, eq_state_change, 1, &eq_state_active);
}

/*! \brief Gaia Client will be told if the User EQ set (preset, User set or Off) changes

    \param transport    Transport type
*/
static void musicProcessingGaiaPlugin_EqSetChange(void)
{
    DEBUG_LOG_VERBOSE("musicProcessingGaiaPlugin_EqSetChange");

    GaiaFramework_SendNotification(GAIA_MUSIC_PROCESSING_FEATURE_ID, eq_set_change, 1, &active_preset);
}

/*! \brief Gaia Client will be told if there are User EQ band changes

    \param transport    Transport type
*/
inline static void musicProcessingGaiaPlugin_UserEqBandChange(uint8 payload_length, uint8 * payload)
{
    DEBUG_LOG_VERBOSE("musicProcessingGaiaPlugin_UserEqBandChange");

    GaiaFramework_SendNotification(GAIA_MUSIC_PROCESSING_FEATURE_ID, user_eq_band_change, payload_length, payload);
}

/*! \brief Itterates through the requested EQ bands and loads the data to the response payload

    \param start_band           Starting EQ band

    \param end_band             End EQ band

    \param response_payload     Response payload
*/
inline static void musicProcessingGaiaPlugin_PopulateEqBandInformation(uint8 start_band, uint8 end_band, uint8 * response_payload)
{
    uint8 current_band;
    uint8 data_pointer = 0;

    DEBUG_LOG_VERBOSE("musicProcessingGaiaPlugin_PopulateEqBandInformation start_band %d, end_band %d", start_band, end_band);

    for (current_band = start_band; current_band <= end_band; current_band++)
    {
        musicProcessingGaiaPlugin_LoadEqParametersToPayload(current_band, &response_payload[data_pointer]);
        data_pointer += TOTAL_BYTES_OF_EQ_INFO_PER_BAND;
    }
}

/*! \brief Converts and loads the values of the EQ information to the payload

    \param band     EQ band

    \param response_payload  Payload
*/
inline static void musicProcessingGaiaPlugin_LoadEqParametersToPayload(uint8 band, uint8 * response_payload)
{
    kymera_eq_paramter_set_t param_set;

    uint8 filter_type;
    uint16 gain;
    uint8 data_pointer = 0;

    DEBUG_LOG_VERBOSE("musicProcessingGaiaPlugin_LoadEqParametersToPayload for band: %d", band);

    Kymera_GetEqBandInformation(band, &param_set);
    filter_type = param_set.filter_type;
    gain = (uint16)param_set.gain;

    DEBUG_LOG_VERBOSE("musicProcessingGaiaPlugin_LoadEqParametersToPayload freq %d, q %d, type %d, gain %d",
            param_set.cut_off_freq, param_set.q, filter_type, gain);

    data_pointer += ByteUtilsMemCpyUnpackString(&response_payload[data_pointer], &param_set.cut_off_freq, sizeof(uint16));
    data_pointer += ByteUtilsMemCpyUnpackString(&response_payload[data_pointer], &param_set.q, sizeof(uint16));
    data_pointer += ByteUtilsMemCpyFromStream(&response_payload[data_pointer], &filter_type, sizeof(uint8));
    data_pointer += ByteUtilsMemCpyUnpackString(&response_payload[data_pointer], &gain, sizeof(uint16));
}

/*! \brief Converts the payloads to EQ band gains

    \param number_of_bands  Number EQ band

    \param payload          Payload

    \param gain            EQ gain
*/
inline static void musicProcessingGaiaPlugin_ConvertEqGains(uint8 number_of_bands, const uint8 * payload, int16 * gain)
{
    uint8 current_band;
    uint8 data_pointer = 0;

    DEBUG_LOG_VERBOSE("musicProcessingGaiaPlugin_ConvertEqGains");

    for (current_band = 0; current_band < number_of_bands; current_band++)
    {
        gain[current_band] = (int16)ByteUtilsGet2BytesFromStream(&payload[data_pointer]);
        data_pointer += sizeof(int16);
    }
}

#endif /* defined(INCLUDE_GAIA) && defined(INCLUDE_MUSIC_PROCESSING) */

/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       anc_session_data.c
\brief      Handles and persists session data corresponding to ANC domain.
*/


#ifdef ENABLE_ANC
#define DEBUG_LOG_MODULE_NAME anc_session_data
#include <logging.h>
DEBUG_LOG_DEFINE_LEVEL_VAR

#include "anc_session_data.h"
#include "anc_state_manager_private.h"
#include "kymera.h"

#include <panic.h>
#include <stdlib.h>
#include <ps_key_map.h>
#include <ps.h>

#define anc_session_data_size_words       (sizeof(anc_session_data_t))/(sizeof(uint16))

/**************** Utility function declarations for ANC session data ********************************/
static void ancSessionData_GetDefaults(anc_session_data_t* session_data);
static void ancSessionData_ReadFromPS(anc_session_data_t* session_data);
static bool ancSessionData_WriteToPS(anc_session_data_t* session_data);
static bool ancSessionData_isSameAsStorage(anc_session_data_t* session_data);


/**************** Utility function definitions for ANC session data ********************************/
static void ancSessionData_GetDefaults(anc_session_data_t* session_data)
{
    DEBUG_LOG("ancSessionData_GetDefaults\n");

    session_data->toggle_configurations.anc_toggle_way_config[0] = ancConfigToggleWay1();
    session_data->toggle_configurations.anc_toggle_way_config[1] = ancConfigToggleWay2();
    session_data->toggle_configurations.anc_toggle_way_config[2] = ancConfigToggleWay3();

    session_data->standalone_config.anc_config = ancConfigStandalone();
    session_data->standalone_config.is_same_as_current = (ancConfigStandalone() == anc_toggle_config_is_same_as_current);

    session_data->playback_config.anc_config = ancConfigPlayback();
    session_data->playback_config.is_same_as_current = (ancConfigPlayback() == anc_toggle_config_is_same_as_current);

    session_data->sco_config.anc_config = ancConfigVoiceCall();
    session_data->sco_config.is_same_as_current = (ancConfigVoiceCall() == anc_toggle_config_is_same_as_current);

    session_data->va_config.anc_config = ancConfigVoiceAssistant();
    session_data->va_config.is_same_as_current = (ancConfigVoiceAssistant() == anc_toggle_config_is_same_as_current);
}

static void ancSessionData_ReadFromPS(anc_session_data_t* session_data)
{
    DEBUG_LOG("ancSessionData_ReadFromPS reading ANC session data from PS\n");
    PsRetrieve(PS_KEY_ANC_SESSION_DATA, session_data, anc_session_data_size_words);
}

static bool ancSessionData_WriteToPS(anc_session_data_t* session_data)
{
    uint16 written_words;
    bool write_status;

    written_words = PsStore(PS_KEY_ANC_SESSION_DATA, session_data, anc_session_data_size_words);

    if (written_words != anc_session_data_size_words)
    {
        write_status = FALSE;
        DEBUG_LOG_WARN("ancSessionData_WriteToPS Unable to save session data. %d words written",
                       written_words);
    }
    else
    {
        write_status = TRUE;
        DEBUG_LOG_INFO("ancSessionData_WriteToPS Saved session data, %d words written",
                       written_words);
    }

    return write_status;

}

static bool ancSessionData_isSameAsStorage(anc_session_data_t* session_data)
{
    uint16 current_size_words;
    uint16 *anc_session_ps_data;
    bool storage_status = FALSE;

    current_size_words = PsRetrieve(PS_KEY_ANC_SESSION_DATA, NULL, 0);

    if(current_size_words != 0)
    {
        anc_session_ps_data = PanicUnlessMalloc(sizeof(anc_session_data_t));

        PsRetrieve(PS_KEY_ANC_SESSION_DATA, anc_session_ps_data, anc_session_data_size_words);
        storage_status = (memcmp(anc_session_ps_data, session_data, sizeof(anc_session_data_t)) == 0);

        free(anc_session_ps_data);
    }

    return storage_status;
}


/**************** Interface function definitions for ANC session data ********************************/
void AncSessionData_GetSessionData(anc_session_data_t* session_data)
{
    uint16 current_size_words;

    current_size_words = PsRetrieve(PS_KEY_ANC_SESSION_DATA, NULL, 0);

    (current_size_words != 0) ? ancSessionData_ReadFromPS(session_data) :
                                    ancSessionData_GetDefaults(session_data);
}

bool AncSessionData_SetSessionData(anc_session_data_t* session_data)
{
    bool status = TRUE;

    if(!ancSessionData_isSameAsStorage(session_data))
    {
        status = ancSessionData_WriteToPS(session_data);
    }

    return status;
}

#endif /* ENABLE_ANC */

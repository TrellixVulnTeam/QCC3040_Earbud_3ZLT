/*!
\copyright  Copyright (c) 2019-2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    device_test_service
\brief      Implementation of the auth commands for device test service
*/
/*! \addtogroup device_test_service
@{
*/

#include "device_test_service.h"
#include "device_test_service_auth.h"
#include "device_test_parse.h"

#include <anc.h>
#include <anc_state_manager.h>
#include <logging.h>

#include <stdio.h>

#ifdef ENABLE_ANC

/*! \name Range of ANC mode value supported in the AT command.

    These are mapped onto the equivalent anc_mode_x enumerated
    type.
 */
/*! @{ */
#define MIN_AT_ANC_MODE     1
#define MAX_AT_ANC_MODE     10
/* @} */

/*! \name Mapping of gain_path parameters in the AT commands to
    defines used by the ANC library
 */
/*! @{ */
#define FFA_PATH    1   /*!< Feed Forward path A */
#define FFB_PATH    2   /*!< Feed Forward path B */
#define FB_PATH     3    /*!< Feed Back path */
/*! @} */

/*! Response message sent to the read fine gain command */
#define ANC_READ_FINE_GAIN_RESPONSE "+ANCREADFINEGAIN:"
/*! Maximum length of the response message, allowing for an 8
    bit value (max '255' - 3 digits) */
#define FULL_FINE_GAIN_RESPONSE_LEN (sizeof(ANC_READ_FINE_GAIN_RESPONSE) + 3)

/*! Response message sent to the read coarse gain command */
#define ANC_READ_COARSE_GAIN_RESPONSE "+ANCREADCOARSEGAIN:"
/*! Maximum length of the response message*/
#define FULL_COARSE_GAIN_RESPONSE_LEN (sizeof(ANC_READ_COARSE_GAIN_RESPONSE) + 6)

/*! Response message sent to the read fine gain command for both ANC instances*/
/* 3 digits for instance0 gain, 3 digits for instance1 gain. Plus 1 to accomodate the space in sprintf between two instance gains*/
#define ANC_READ_FINE_GAIN_DUAL_RESPONSE  "+ANCREADFINEGAINDUAL:"
#define FULL_FINE_GAIN_DUAL_RESPONSE_LEN (sizeof(ANC_READ_FINE_GAIN_DUAL_RESPONSE) + 7)


/*! Map the ANC mode supplied in the AT command to a mode understood by libraries 

    \param requested_mode   The mode value from the AT command
    \param[out] anc_mode    Pointer to location to take the library mode value

    \return TRUE if the value passed was legal, and value could be mapped. 
            FALSE otherwise
*/
static bool deviceTestService_AncMapMode(uint16 requested_mode, anc_mode_t* anc_mode)
{
    if (MIN_AT_ANC_MODE <= requested_mode && requested_mode <= MAX_AT_ANC_MODE)
    {
        *anc_mode = (anc_mode_t)(requested_mode - MIN_AT_ANC_MODE + anc_mode_1);
        return TRUE;
    }
    DEBUG_LOG_WARN("deviceTestService_AncMapMode Attempt to map a bad ANC mode:%d",requested_mode);
    return FALSE;
}

/*! Map the gain path supplied in the AT command to a mode understood by libraries 

    \param requested_gain_path   The gain_path value from the AT command
    \param[out] gain_path        Pointer to location to take the library gain_path value

    \return TRUE if the value passed was legal, and value could be mapped. 
            FALSE otherwise
*/
static bool deviceTestService_AncMapGainPath(uint16 requested_gain_path, 
                                           audio_anc_path_id *gain_path)
{
    switch (requested_gain_path)
    {
        case FFA_PATH:
            *gain_path = AUDIO_ANC_PATH_ID_FFA;
            break;

        case FFB_PATH:
            *gain_path = AUDIO_ANC_PATH_ID_FFB;
            break;

        case FB_PATH:
            *gain_path = AUDIO_ANC_PATH_ID_FB;
            break;

        default:
            DEBUG_LOG_WARN("deviceTestService_AncMapGainPath. Attempt to map a bad gain path:%d", requested_gain_path);
            return FALSE;
    }

    return TRUE;
}


static bool deviceTestService_AncCommandsAllowed(void)
{
    return DeviceTestService_CommandsAllowed() && AncStateManager_IsSupported();
}


/*! Handle the AT+ANCENABLE command

    Handle the enable, making sure to send either an "OK" or "ERROR" response.

    \note ANC Enable needs the audio sub system to be started, which can
            cause a delay before the OK response is sent.

    \param task Identifier for the task that issued the command. This should be
                passed when responding to the command.
    \param[in] ancEnable Structure supplied by the AT command parser, providing
                    the parameters passed in the AT command (the requested mode to
                    enable)
*/
void DeviceTestServiceCommand_HandleAncEnable(Task task, 
                const struct DeviceTestServiceCommand_HandleAncEnable *ancEnable)
{
    anc_mode_t anc_mode;
    uint8 gain;

    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleAncEnable. mode:%d", ancEnable->mode);

    if (   !deviceTestService_AncCommandsAllowed()
        || !deviceTestService_AncMapMode(ancEnable->mode, &anc_mode))
    {
        DeviceTestService_CommandResponseError(task);
        return;
    }

    if (!AncStateManager_IsEnabled())
    {
        AncStateManager_Enable();
    }

    AncSetMode(anc_mode);

    /* The return value from AncSetMode does not indicate whether the mode
       set was successful, so attempt a read - which will return ERROR 
       if the mode was invalid mode */
    DeviceTestService_CommandResponseOkOrError(task, 
                            AncReadFineGain(AUDIO_ANC_PATH_ID_FFA, &gain));
}


/*! Handle the AT+ANCDISABLE command

    Handle the disable, making sure to send either an "OK" or "ERROR" response.

    \param task Identifier for the task that issued the command. This should be
                passed when responding to the command.
*/
void DeviceTestServiceCommand_HandleAncDisable(Task task)
{
    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleAncDisable");
    
    if (!deviceTestService_AncCommandsAllowed())
    {
        DeviceTestService_CommandResponseError(task);
        return;
    }

    if (AncStateManager_IsEnabled())
    {
        AncStateManager_Disable();
    }

    DeviceTestService_CommandResponseOk(task);
}


/*! Handle the AT+ANCSETFINEGAIN command

    Handle the command, making sure to send either an "OK" or "ERROR" response.

    Parameters are checked, any mistakes causing an error response.

    \param task Identifier for the task that issued the command. This should be
                passed when responding to the command.
    \param[in] ancSetFineGain Structure supplied by the AT command parser, providing
                              the parameters passed in the AT command (the requested 
                              mode, gain path and gain value)
*/
void DeviceTestServiceCommand_HandleAncSetFineGain(Task task, 
                    const struct DeviceTestServiceCommand_HandleAncSetFineGain *ancSetFineGain)
{
    anc_mode_t anc_mode;
    audio_anc_path_id gain_path;
    bool status = FALSE;

    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleAncSetFindGain  mode:%u Path:%u Gain:%u ",
                     ancSetFineGain->mode,
                     ancSetFineGain->gainpath, ancSetFineGain->gainvalue);

    if (   !deviceTestService_AncCommandsAllowed()
        || !AncStateManager_IsEnabled()
        || !deviceTestService_AncMapMode(ancSetFineGain->mode, &anc_mode)
        || !deviceTestService_AncMapGainPath(ancSetFineGain->gainpath, &gain_path)
        || ancSetFineGain->gainvalue > 0xFF)
    {
        DeviceTestService_CommandResponseError(task);
        return;
    }

    /*Set gain for currently set mode in DSP*/
    switch(ancSetFineGain->gainpath)
    {
        case FFA_PATH:
            status = AncConfigureFFAPathGain(AUDIO_ANC_INSTANCE_0,
                                             (uint8)ancSetFineGain->gainvalue);
            
#ifdef ENABLE_ENHANCED_ANC
            status = AncConfigureFFAPathGain(AUDIO_ANC_INSTANCE_1,
                                             (uint8)ancSetFineGain->gainvalue);
#endif
        break;

        case FFB_PATH:
            status = AncConfigureFFBPathGain(AUDIO_ANC_INSTANCE_0,
                                             (uint8)ancSetFineGain->gainvalue);

            
#ifdef ENABLE_ENHANCED_ANC
            status = AncConfigureFFBPathGain(AUDIO_ANC_INSTANCE_1,
                                             (uint8)ancSetFineGain->gainvalue);
#endif
            break;

        case FB_PATH:
            status = AncConfigureFBPathGain(AUDIO_ANC_INSTANCE_0,
                                            (uint8)ancSetFineGain->gainvalue);
            
#ifdef ENABLE_ENHANCED_ANC
            status = AncConfigureFBPathGain(AUDIO_ANC_INSTANCE_1,
                                            (uint8)ancSetFineGain->gainvalue);
#endif
            break;

        default:
            /* Panic here as the mapping should have ensured a valid gain_path
               A Panic would indicate a new gain_path should be supported but 
               has not been implemented here */
            Panic();
            break;
    }

    DeviceTestService_CommandResponseOkOrError(task, status);
}


/*! Handle the AT+ANCREADFINEGAIN command

    Handle the command, making sure to send either an "OK" or "ERROR" response.

    Parameters are checked, any mistakes causing an error response.

    \param task Identifier for the task that issued the command. This should be
                passed when responding to the command.
    \param[in] ancReadFineGain Structure supplied by the AT command parser, providing
                               the parameters passed in the AT command (the requested 
                               mode and gain path)
*/
void DeviceTestServiceCommand_HandleAncReadFineGain(Task task, 
                const struct DeviceTestServiceCommand_HandleAncReadFineGain *ancReadFineGain)
{
    anc_mode_t anc_mode;
    audio_anc_path_id gain_path;
    uint8 gain=0;
    char response[FULL_FINE_GAIN_RESPONSE_LEN];

    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleAncReadFineGain mode:%u Path:%u ",
                     ancReadFineGain->mode, ancReadFineGain->gainpath);

    if (   !deviceTestService_AncCommandsAllowed()
        || !AncStateManager_IsEnabled()
        || !deviceTestService_AncMapMode(ancReadFineGain->mode, &anc_mode)
        || !deviceTestService_AncMapGainPath(ancReadFineGain->gainpath, &gain_path))
    {
        DeviceTestService_CommandResponseError(task);
        return;
    }

    /*This ANC interface makes sure to return the gain which golden config plus any delta from earlier calibration*/
    /*Both instances have same values configured*/
    /*Make sure to reset ANC or do a mode change and come back to required mode to read the updated gain values after a Write gain is called*/
    AncReadFineGainFromInstance(AUDIO_ANC_INSTANCE_0, gain_path, &gain);

    /* We don't have a handy function for itoa and it is not commonly
       needed. sprintf is available, and is an embedded one so relatively
       efficient */
    sprintf(response, ANC_READ_FINE_GAIN_RESPONSE "%d", gain);

    DeviceTestService_CommandResponse(task, response, FULL_FINE_GAIN_RESPONSE_LEN);
    DeviceTestService_CommandResponseOk(task);
}

/*! Handle the AT+ANCREADCOARSEGAIN command

    Handle the command, making sure to send either an "OK" or "ERROR" response.

    Parameters are checked, any mistakes causing an error response.

    \param task Identifier for the task that issued the command. This should be
                passed when responding to the command.
    \param[in] ancReadCoarseGain Structure supplied by the AT command parser, providing
                               the parameters passed in the AT command (the requested 
                               mode and gain path)
*/
void DeviceTestServiceCommand_HandleAncReadCoarseGain(Task task, 
                const struct DeviceTestServiceCommand_HandleAncReadCoarseGain *ancReadCoarseGain)
{
    anc_mode_t anc_mode;
    audio_anc_path_id gain_path;
    uint16 gain=0;
    char response[FULL_COARSE_GAIN_RESPONSE_LEN];

    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleAncReadCoarseGain mode:%u Path:%u ",
                     ancReadCoarseGain->mode, ancReadCoarseGain->gainpath);

    if (   !deviceTestService_AncCommandsAllowed()
         || !AncStateManager_IsEnabled()
         || !deviceTestService_AncMapMode(ancReadCoarseGain->mode, &anc_mode)
         || !deviceTestService_AncMapGainPath(ancReadCoarseGain->gainpath, &gain_path))
     {
         DeviceTestService_CommandResponseError(task);
         return;
     }
    
     /*This ANC interface makes sure to return the coarse gain for instance 0*/
     /*Both instances have same values configured*/
     /*Make sure to reset ANC or do a mode change and come back to required mode to read the updated gain values after a Write gain is called*/
     AncReadCoarseGainFromInstance(AUDIO_ANC_INSTANCE_0, gain_path, &gain);

    /* We don't have a handy function for itoa and it is not commonly
       needed. sprintf is available, and is an embedded one so relatively
       efficient */
    sprintf(response, ANC_READ_COARSE_GAIN_RESPONSE "%d", gain);

    DeviceTestService_CommandResponse(task, response, FULL_COARSE_GAIN_RESPONSE_LEN);
    DeviceTestService_CommandResponseOk(task);
}


/*! Handle the AT+ANCWRITEFINEGAIN command

    Handle the command, making sure to send either an "OK" or "ERROR" response.

    Parameters are checked, any mistakes causing an error response.

    \param task Identifier for the task that issued the command. This should be
                passed when responding to the command.
    \param[in] ancWriteFineGain Structure supplied by the AT command parser, providing
                                the parameters passed in the AT command (the requested 
                                mode, gain path and gain value)
*/
void DeviceTestServiceCommand_HandleAncWriteFineGain(Task task, 
                const struct DeviceTestServiceCommand_HandleAncWriteFineGain *ancWriteFineGain)
{
    anc_mode_t anc_mode;
    audio_anc_path_id gain_path;

    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleAncWriteFindGain  mode:%u Path:%u Gain:%u ",
                     ancWriteFineGain->mode,
                     ancWriteFineGain->gainpath, ancWriteFineGain->gainvalue);

    if (   !deviceTestService_AncCommandsAllowed()
        || !AncStateManager_IsEnabled()
        || !deviceTestService_AncMapMode(ancWriteFineGain->mode, &anc_mode)
        || !deviceTestService_AncMapGainPath(ancWriteFineGain->gainpath, &gain_path)
        || ancWriteFineGain->gainvalue > 0xFF
        || !AncWriteFineGain(gain_path, ancWriteFineGain->gainvalue ))
        /*This ANC interface makes sure to write the Delta gain (difference between golden config gain and the configured gain in write)*/
    {
        DeviceTestService_CommandResponseError(task);
        return;
    }

    DeviceTestService_CommandResponseOk(task);
}


/*! Handle the AT+ANCSETFINEGAINDUAL command

    Handle the command, making sure to send either an "OK" or "ERROR" response.

    Parameters are checked, any mistakes causing an error response.

    \param task Identifier for the task that issued the command. This should be
                passed when responding to the command.
    \param[in] ancSetFineGain Structure supplied by the AT command parser, providing
                              the parameters passed in the AT command (the requested 
                              mode, gain path and gain value for instance 0 and 1)
*/
void DeviceTestServiceCommand_HandleAncSetFineGainDual(Task task, 
                    const struct DeviceTestServiceCommand_HandleAncSetFineGainDual *ancSetFineGainDual)
{
    anc_mode_t anc_mode;
    audio_anc_path_id gain_path;
    bool status = FALSE;

    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleAncSetFindGain  mode:%u Path:%u Instance0Gain:%u Instance1Gain:%u ",
                     ancSetFineGainDual->mode,
                     ancSetFineGainDual->gainpath, ancSetFineGainDual->instance0gain, ancSetFineGainDual->instance1gain);

    if (   !deviceTestService_AncCommandsAllowed()
        || !AncStateManager_IsEnabled()
        || !deviceTestService_AncMapMode(ancSetFineGainDual->mode, &anc_mode)
        || !deviceTestService_AncMapGainPath(ancSetFineGainDual->gainpath, &gain_path)
        || (ancSetFineGainDual->instance0gain > 0xFF)
        || ancSetFineGainDual->instance1gain > 0xFF)
    {
        DeviceTestService_CommandResponseError(task);
        return;
    }

    /*Set gain for currently set mode in DSP*/
    switch(ancSetFineGainDual->gainpath)
    {
        case FFA_PATH:
            status = AncConfigureParallelFilterFFAPathGain((uint8)ancSetFineGainDual->instance0gain, (uint8)ancSetFineGainDual->instance1gain);
            break;

        case FFB_PATH:            
            status = AncConfigureParallelFilterFFBPathGain((uint8)ancSetFineGainDual->instance0gain, (uint8)ancSetFineGainDual->instance1gain);
            break;

        case FB_PATH:            
            status = AncConfigureParallelFilterFBPathGain((uint8)ancSetFineGainDual->instance0gain, (uint8)ancSetFineGainDual->instance1gain);
            break;

        default:
            /* Panic here as the mapping should have ensured a valid gain_path
               A Panic would indicate a new gain_path should be supported but 
               has not been implemented here */
            Panic();
            break;
    }

    DeviceTestService_CommandResponseOkOrError(task, status);
}


/*! Handle the AT+ANCREADFINEGAINDUAL command

    Handle the command for both instances of ANC, making sure to send either an "OK" or "ERROR" response.

    Parameters are checked, any mistakes causing an error response.

    \param task Identifier for the task that issued the command. This should be
                passed when responding to the command.
    \param[in] ancReadFineGain Structure supplied by the AT command parser, providing
                               the parameters passed in the AT command (the requested 
                               mode and gain path)
*/
void DeviceTestServiceCommand_HandleAncReadFineGainDual(Task task, 
                const struct DeviceTestServiceCommand_HandleAncReadFineGainDual *ancReadFineGainDual)
{
    anc_mode_t anc_mode;
    audio_anc_path_id gain_path;
    uint8 instance0_gain=0;
    uint8 instance1_gain=0;
    char response[FULL_FINE_GAIN_DUAL_RESPONSE_LEN];

    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleAncReadFineGainDual mode:%u Path:%u ",
                     ancReadFineGainDual->mode, ancReadFineGainDual->gainpath);

    if (   !deviceTestService_AncCommandsAllowed()
        || !AncStateManager_IsEnabled()
        || !deviceTestService_AncMapMode(ancReadFineGainDual->mode, &anc_mode)
        || !deviceTestService_AncMapGainPath(ancReadFineGainDual->gainpath, &gain_path)
        || !AncReadFineGainParallelFilter(gain_path, &instance0_gain, &instance1_gain))
        /*Reads gain from ANC Audio PS key*/
    {
        DeviceTestService_CommandResponseError(task);
        return;
    }

    /* We don't have a handy function for itoa and it is not commonly
       needed. sprintf is available, and is an embedded one so relatively
       efficient */
    sprintf(response, ANC_READ_FINE_GAIN_DUAL_RESPONSE "%d %d", instance0_gain, instance1_gain);

    DeviceTestService_CommandResponse(task, response, FULL_FINE_GAIN_DUAL_RESPONSE_LEN);
    DeviceTestService_CommandResponseOk(task);
}


/*! Handle the AT+ANCWRITEFINEGAINDUAL command

    Handle the command, making sure to send either an "OK" or "ERROR" response.

    Parameters are checked, any mistakes causing an error response.

    \param task Identifier for the task that issued the command. This should be
                passed when responding to the command.
    \param[in] ancWriteFineGain Structure supplied by the AT command parser, providing
                                the parameters passed in the AT command (the requested 
                                mode, gain path and gain value for instance 0 and 1)
*/
void DeviceTestServiceCommand_HandleAncWriteFineGainDual(Task task, 
                const struct DeviceTestServiceCommand_HandleAncWriteFineGainDual *ancWriteFineGainDual)
{
    anc_mode_t anc_mode;
    audio_anc_path_id gain_path;

    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleAncWriteFindGainDual  mode:%u Path:%u Instance0Gain:%u Instance1Gain:%u ",
                     ancWriteFineGainDual->mode,
                     ancWriteFineGainDual->gainpath, ancWriteFineGainDual->instance0gain, ancWriteFineGainDual->instance1gain);

    if (   !deviceTestService_AncCommandsAllowed()
        || !AncStateManager_IsEnabled()
        || !deviceTestService_AncMapMode(ancWriteFineGainDual->mode, &anc_mode)
        || !deviceTestService_AncMapGainPath(ancWriteFineGainDual->gainpath, &gain_path)
        || ancWriteFineGainDual->instance0gain > 0xFF
        || ancWriteFineGainDual->instance1gain > 0xFF
        || !AncWriteFineGainParallelFilter(gain_path, ancWriteFineGainDual->instance0gain, ancWriteFineGainDual->instance1gain))
        /*Updates gain to ANC Audio PS key*/
    {
        DeviceTestService_CommandResponseError(task);
        return;
    }

    DeviceTestService_CommandResponseOk(task);
}


#else /* ENABLE_ANC */

/* Stub command included if ANC is disabled.*/
void DeviceTestServiceCommand_HandleAncDisable(Task task)
{
    UNUSED(task);
    DeviceTestService_CommandResponseError(task);
}

/* Stub command included if ANC is disabled.*/
void DeviceTestServiceCommand_HandleAncEnable(Task task, 
                const struct DeviceTestServiceCommand_HandleAncEnable *ancEnable)
{
    UNUSED(ancEnable);

    DeviceTestServiceCommand_HandleAncDisable(task);
}

/* Stub command included if ANC is disabled.*/
void DeviceTestServiceCommand_HandleAncSetFineGain(Task task, 
                    const struct DeviceTestServiceCommand_HandleAncSetFineGain *ancSetFineGain)
{
    UNUSED(ancSetFineGain);

    DeviceTestServiceCommand_HandleAncDisable(task);
}

/* Stub command included if ANC is disabled.*/
void DeviceTestServiceCommand_HandleAncReadFineGain(Task task, 
                const struct DeviceTestServiceCommand_HandleAncReadFineGain *ancReadFineGain)
{
    UNUSED(ancReadFineGain);

    DeviceTestServiceCommand_HandleAncDisable(task);
}


/* Stub command included if ANC is disabled.*/
void DeviceTestServiceCommand_HandleAncWriteFineGain(Task task, 
                const struct DeviceTestServiceCommand_HandleAncWriteFineGain *ancWriteFineGain)
{
    UNUSED(ancWriteFineGain);

    DeviceTestServiceCommand_HandleAncDisable(task);
}


/* Stub command included if ANC is disabled.*/
void DeviceTestServiceCommand_HandleAncSetFineGainDual(Task task, 
                    const struct DeviceTestServiceCommand_HandleAncSetFineGainDual *ancSetFineGainDual)
{
    UNUSED(ancSetFineGainDual);

    DeviceTestServiceCommand_HandleAncDisable(task);
}

/* Stub command included if ANC is disabled.*/
void DeviceTestServiceCommand_HandleAncReadFineGainDual(Task task, 
                const struct DeviceTestServiceCommand_HandleAncReadFineGainDual *ancReadFineGainDual)
{
    UNUSED(ancReadFineGainDual);

    DeviceTestServiceCommand_HandleAncDisable(task);
}


/* Stub command included if ANC is disabled.*/
void DeviceTestServiceCommand_HandleAncWriteFineGainDual(Task task, 
                const struct DeviceTestServiceCommand_HandleAncWriteFineGainDual *ancWriteFineGainDual)
{
    UNUSED(ancWriteFineGainDual);

    DeviceTestServiceCommand_HandleAncDisable(task);
}

/* Stub command included if ANC is disabled.*/
void DeviceTestServiceCommand_HandleAncReadCoarseGain(Task task, 
                const struct DeviceTestServiceCommand_HandleAncReadCoarseGain *ancReadCoarseGain)
{
    UNUSED(ancReadCoarseGain);

    DeviceTestServiceCommand_HandleAncDisable(task);
}


#endif /* !ENABLE_ANC */
/*! @} End of group documentation */

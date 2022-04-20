/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    device_test_service
\brief      Implementation of the audio tone commands of the device test service
*/
/*! \addtogroup device_test_service
@{
*/

#include "device_test_service.h"
#include "device_test_service_auth.h"
#include "device_test_service_audio.h"
#include "device_test_parse.h"

#include <kymera.h>

#include <stdio.h>
#include <logging.h>

#define DTS_TONE_MAX_TONE 119

typedef enum
{
    DEVICE_TEST_SERVICE_TONES_MSG_TIMEOUT = INTERNAL_MESSAGE_BASE,
    DEVICE_TEST_SERVICE_TONES_MSG_CANCEL,
    DEVICE_TEST_SERVICE_TONES_MSG_TONE_DONE, 

    /*! This must be the final message */
    DEVICE_TEST_SERVICE_TONES_MESSAGE_END
} deviceTestService_ToneInternalMessageId;
ASSERT_INTERNAL_MESSAGES_NOT_OVERFLOWED(DEVICE_TEST_SERVICE_TONES_MESSAGE_END)

LOGGING_PRESERVE_MESSAGE_TYPE(deviceTestService_ToneInternalMessageId)

static void deviceTestService_ContinualToneTaskHandler(Task task, MessageId id, Message message);
static TaskData deviceTestService_ContinualToneTask = {deviceTestService_ContinualToneTaskHandler};

static uint16 deviceTestService_ctt_tone_active = 0;

/* Note that the definitions for ringtones come from app/ringtone/ringtone_if.h */
/*! Tone sequence used for fixed tone sequence
 */
static const ringtone_note deviceTestService_testtone[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(B6,   SEMIQUAVER),
    RINGTONE_NOTE(G6,   SEMIQUAVER),
    RINGTONE_NOTE(D7,   SEMIQUAVER),
    RINGTONE_NOTE(REST, SEMIQUAVER),
    RINGTONE_NOTE(B6,   SEMIQUAVER),
    RINGTONE_NOTE(G6,   SEMIQUAVER),
    RINGTONE_NOTE(D7,   SEMIQUAVER),
    RINGTONE_END
};

/*! Structure used for playing a variable length tone. Pre-populated
    with a fixed note for convenience.

    Calculating the tempo and type of note is complex. Rather than 
    attempting accuracy use 3 different calcuations for ranges.
    Longer notes will rely on being cancelled rather than the note
    duration being set.

    Note that the note TEMPO has a range, documented as below
    4096, but also needs to be above ~28/29.

    The longest tone as defined here runs for 72 seconds and is used 
    for the longer delays.

    The shortest durations use a demi-semi-quaver (1/32 of a note).
 */
static ringtone_note deviceTestService_fixedtone[] =
{
    RINGTONE_DECAY(0),      /* No decay so constant sine wave */
    RINGTONE_TIMBRE(sine),
    RINGTONE_TEMPO(30),    /* This is number of crotchets a minute */
    RINGTONE_NOTE(C5, SEMIBREVE),
    RINGTONE_NOTE_TIE(C5, SEMIBREVE),
    RINGTONE_NOTE_TIE(C5, SEMIBREVE),
    RINGTONE_NOTE_TIE(C5, SEMIBREVE),
    RINGTONE_NOTE_TIE(C5, SEMIBREVE),
    RINGTONE_NOTE_TIE(C5, SEMIBREVE),
    RINGTONE_NOTE_TIE(C5, SEMIBREVE),
    RINGTONE_NOTE_TIE(C5, SEMIBREVE),
    RINGTONE_NOTE_TIE(C5, SEMIBREVE),
    RINGTONE_END
};

#define DTS_FIXED_TONE_TIE_NOTE_OFFSET  4

/*! Helper macros for updating deviceTestService_fixedtone

    Offsets are hard coded as not used outside of these macros
 */
/*! @{ */
#define DTS_FIXED_TONE_SET_TEMPO(_tempo)        \
                    deviceTestService_fixedtone[2] = (ringtone_note)RINGTONE_TEMPO(_tempo)
#define DTS_FIXED_TONE_SET_PITCH(_pitch)        \
                    deviceTestService_fixedtone[3] = (ringtone_note)((_pitch) << RINGTONE_SEQ_NOTE_PITCH_POS);
#define DTS_FIXED_TONE_SET_NOTE_LENGTH(_note)   \
                    deviceTestService_fixedtone[3] = \
                            (ringtone_note) (   ((unsigned)deviceTestService_fixedtone[3]) \
                                                  | (_note) \
                                                  | RINGTONE_SEQ_NOTE)
#define DTS_FIXED_TONE_SET_TIE_NOTE_AT_OFFSET(_n)     \
                    deviceTestService_fixedtone[_n] = \
                            (ringtone_note) (   ((unsigned)deviceTestService_fixedtone[3]) \
                                              | RINGTONE_SEQ_NOTE_TIE_MASK)
#define DTS_FIXED_TONE_SET_SHORT_END()          \
                    deviceTestService_fixedtone[4] = RINGTONE_END
/*! @} */


static void deviceTestService_ConfigureToneWithNote(uint16 note)
{
    DTS_FIXED_TONE_SET_PITCH(note);
    /* Inject a RINGTONE_END instead of the first TIED note */
    DTS_FIXED_TONE_SET_SHORT_END();
}


static void deviceTestService_ConfigureShortTone(uint16 duration_ms)
{
    uint16 tempo = (60000 / 8) / duration_ms;

    /* Set the tempo based on using a note of 1/32 note, 1/8 crotchet */
    DTS_FIXED_TONE_SET_TEMPO(tempo);
    DTS_FIXED_TONE_SET_NOTE_LENGTH(RINGTONE_NOTE_DEMISEMIQUAVER);
}


static void deviceTestService_ConfigureMediumTone(uint16 duration_ms)
{
    uint16 tempo = 60000 / duration_ms;

    /* Set the tempo based on using a crotchet */
    DTS_FIXED_TONE_SET_TEMPO(tempo);
    DTS_FIXED_TONE_SET_NOTE_LENGTH(RINGTONE_NOTE_CROTCHET);
}

/*! Play a very long continual tone 

    This plays a single tone of 72 seconds (if not stopped) 
 */
static void deviceTestService_ConfigureLongTone(void)
{
    unsigned i;

    DTS_FIXED_TONE_SET_TEMPO(30);
    DTS_FIXED_TONE_SET_NOTE_LENGTH(RINGTONE_NOTE_SEMIBREVE);

    for (i=DTS_FIXED_TONE_TIE_NOTE_OFFSET;
         i < ARRAY_DIM(deviceTestService_fixedtone) - 1;
         i++)
    {
        DTS_FIXED_TONE_SET_TIE_NOTE_AT_OFFSET(i);
    }

}


/*! Command handler for AT + AUDIOPLAYTESTTONE

    The function decides if the command is allowed, and if so plays a
    pre-programmed tone sequence assuming that the audio subsystem
    is/can be in the correct state.

    Otherwise errors are reported

    \param[in] task The task to be used in command responses
 */
void DeviceTestServiceCommand_HandleAudioPlayTestTone(Task task)
{
    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleAudioPlayTestTone");

    if (!DeviceTestService_CommandsAllowed())
    {
        DeviceTestService_CommandResponseError(task);
        return;
    }

    if (!Kymera_IsIdle())
    {
        DeviceTestService_CommandResponseError(task);
        return;
    }

    appKymeraTonePlay(deviceTestService_testtone, 0, TRUE, NULL, 0);

    DeviceTestService_CommandResponseOk(task);
}


static void deviceTestService_PlayContinualTone(uint16 note, uint16 duration_ms)
{
    DEBUG_LOG_FN_ENTRY("deviceTestService_PlayContinualTone");

    MessageCancelAll(&deviceTestService_ContinualToneTask, 
                     DEVICE_TEST_SERVICE_TONES_MSG_TONE_DONE);
    deviceTestService_ctt_tone_active = 1;

    deviceTestService_ConfigureToneWithNote(note);

    /* There is no direct API to play a tone of indeterminate length,
       Configuration of Tempo and note size is also a complex calculation
       over the range of 65 seconds.

        To work around the limitations, configure the shorter tones to an
        accurate length, while for the longer tones configure a long note 
        and rely on timeout to cancel the tone */
    if (duration_ms <= 150)
    {
        deviceTestService_ConfigureShortTone(duration_ms);
    }
    else if (duration_ms <= 2000)
    {
        deviceTestService_ConfigureMediumTone(duration_ms);
    }
    else
    {
        deviceTestService_ConfigureLongTone();

        /* A long tone is not accurate, so use a timeout to cancel */
        MessageSendLater(&deviceTestService_ContinualToneTask, 
                         DEVICE_TEST_SERVICE_TONES_MSG_TIMEOUT, NULL, 
                         duration_ms);
    }

    MessageSendConditionally(&deviceTestService_ContinualToneTask, 
                             DEVICE_TEST_SERVICE_TONES_MSG_TONE_DONE, NULL, 
                             &deviceTestService_ctt_tone_active);

    appKymeraTonePlay(deviceTestService_fixedtone, 0, TRUE, 
                      &deviceTestService_ctt_tone_active, 1);
}


static void deviceTestService_ContinualToneTaskHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);

    switch (id)
    {
        case DEVICE_TEST_SERVICE_TONES_MSG_CANCEL:
        case DEVICE_TEST_SERVICE_TONES_MSG_TIMEOUT:
            MessageCancelAll(&deviceTestService_ContinualToneTask,
                             DEVICE_TEST_SERVICE_TONES_MSG_TONE_DONE);
            appKymeraTonePromptCancel();
            break;

        case DEVICE_TEST_SERVICE_TONES_MSG_TONE_DONE:
            /* Tone has completed. Nothing else to do. */
            MessageCancelAll(&deviceTestService_ContinualToneTask,
                             DEVICE_TEST_SERVICE_TONES_MSG_TIMEOUT);
            break;
    }
}


/*! Command handler for AT + AUDIOPLAYTONE = %d:tone , %d:speakerSelection , %d:durationMs

    The function decides if the command is allowed, and if so starts playing
    the requested tone, assuming that the audio subsystem is/can be in the correct 
    state.

    Otherwise errors are reported

    \note If loopback is not running, the response will still be OK.

    \param[in] task The task to be used in command responses
    \param[in] tone_params Parameters passed to the command
 */
void DeviceTestServiceCommand_HandleAudioPlayTone(Task task,
                        const struct DeviceTestServiceCommand_HandleAudioPlayTone *tone_params)
{
    appKymeraHardwareOutput output;

    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleAudioPlayTone. Tone:%d, speaker:0x%x, duration:%d",
                            tone_params->tone, tone_params->speakerSelection, tone_params->durationMs);

    if (!DeviceTestService_CommandsAllowed())
    {
        DeviceTestService_CommandResponseError(task);
        return;
    }

    /* Illegal tone value are disallowed if this is not a cancel (duration 0) */
    if (tone_params->durationMs && tone_params->tone > DTS_TONE_MAX_TONE)
    {
        DeviceTestService_CommandResponseError(task);
        return;
    }

    if (!DeviceTestService_SpeakerExists(tone_params->speakerSelection, &output))
    {
        DeviceTestService_CommandResponseError(task);
        return;
    }

    if (tone_params->durationMs)
    {
        deviceTestService_PlayContinualTone(tone_params->tone, tone_params->durationMs);
    }
    else
    {
        MessageSend(&deviceTestService_ContinualToneTask, DEVICE_TEST_SERVICE_TONES_MSG_CANCEL, NULL);
    }

    DeviceTestService_CommandResponseOk(task);
}

/*! @} End of group documentation */


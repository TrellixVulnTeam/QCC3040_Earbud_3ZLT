/*****************************************************************************

Copyright (c) 2018 Qualcomm Technologies International, Ltd.

FILE NAME
    ama_private.h

DESCRIPTION
    Private AMA data and functions shared throughout the library.

*********************************************************************************/
#ifndef _AMA_PRIVATE_H_
#define _AMA_PRIVATE_H_

#include "ama_protocol.h"

#define NUMBER_OF_SUPPORTED_TRANSPORTS (3)

#define AMA_FEATURE_AUXILIARY_CONNECTED                   0x100
#define AMA_FEATURE_BLUETOOTH_A2DP_ENABLED                0x130
#define AMA_FEATURE_BLUETOOTH_HFP_ENABLED                 0x131
#define AMA_FEATURE_BLUETOOTH_A2DP_CONNECTED              0x132
#define AMA_FEATURE_BLUETOOTH_HFP_CONNECTED               0x133
#define AMA_FEATURE_BLUETOOTH_CLASSIC_DISCOVERABLE        0x134

#define AMA_FEATURE_DEVICE_CALIBRATION_REQUIRED           0x200
#define AMA_FEATURE_DEVICE_THEME                          0x201
#define AMA_FEATURE_DEVICE_DND_ENABLED                    0x202
#define AMA_FEATURE_DEVICE_CELLULAR_CONNECTIVITY_STATUS   0x203


#define AMA_FEATURE_MESSAGE_NOTIFICATION                  0x300
#define AMA_FEATURE_CALL_NOTIFICATION                     0x301
#define AMA_FEATURE_REMOTE_NOTIFICATION                   0x302

#define AMA_FEATURE_EQUALIZER_BASS                        0x450
#define AMA_FEATURE_EQUALIZER_MID                         0x451
#define AMA_FEATURE_EQUALIZER_TREBLE                      0x452

#define AMA_FEATURE_INVALID                               0xFFFF

#define AMA_VERSION_EXCHANGE_SIZE 20

/* Bit masks for the RESPONSE__PAYLOAD_DEVICE_FEATURES features field */
#define AMA_DEVICE_FEATURE_BATTERY_LEVEL                (1 << 6)
#define AMA_DEVICE_FEATURE_ANC                          (1 << 7)
#define AMA_DEVICE_FEATURE_PASSTHROUGH                  (1 << 8)
#define AMA_DEVICE_FEATURE_WAKE_WORD                    (1 << 9)
#define AMA_DEVICE_FEATURE_PRIVACY_MODE                 (1 << 10)
#define AMA_DEVICE_FEATURE_EQUALIZER                    (1 << 11)

/* AMA TO-DO cg11 the following copies stuff defined in constants-ph.h.h */
typedef enum {
    ama_error_code_success = 0,
    ama_error_code_unknown = 1,
    ama_error_code_internal = 2,
    ama_error_code_unsupported = 3,
    ama_error_code_user_cancelled = 4,
    ama_error_code_not_found = 5,
    ama_error_code_invalid = 6,
    ama_error_code_busy = 7
} ama_error_code_t;

typedef enum {
    AMA_STATE_VALUE_NOT_SET = 0,
    AMA_STATE_VALUE_BOOLEAN = 2,
    AMA_STATE_VALUE_INTEGER = 3
} ama_state_value_case_t;

typedef enum _AMA_MEDIA_CONTROL {
    AMA_MEDIA_CONTROL_PLAY ,
    AMA_MEDIA_CONTROL_PAUSE,
    AMA_MEDIA_CONTROL_NEXT,
    AMA_MEDIA_CONTROLPREVIOUS
}AMA_MEDIA_CONTROL;

/* macro for allocating message structures */
#define MAKE_AMA_MESSAGE(TYPE) MESSAGE_MAKE(message,TYPE##_T);
#define MAKE_AMA_MESSAGE_WITH_LEN(TYPE, LEN) TYPE##_T *message = (TYPE##_T *) PanicUnlessMalloc(sizeof(TYPE##_T) + sizeof(uint8)*LEN);

/* Return the task associated with Voice Assistant */
#define Ama_GetTask() ((Task)&ama_task)

void Ama_Disconnect(void);

void AmaProtocol_SendAppMsg(ama_message_type_t id, void* data);

void Ama_SendKeepAlive(void);

void AmaProtocol_MediaControl(AMA_MEDIA_CONTROL control);


/***************************************************************************
DESCRIPTION
    Returns the ama device configuration information.

PARAMS
    None

RETURNS
    pointer to the ama device configuration.
*/
ama_device_config_t * AmaProtocol_GetDeviceConfiguration(void);

uint8 AmaProtocol_GetNumTransportSupported(void);

bdaddr* AmaProtocol_GetLocalAddress(void);

void AmaNotifyAppMsg_StateMsg(ama_speech_state_t state);

void AmaNotifyAppMsg_StopSpeechMsg(void);

void AmaNotifyAppMsg_ProvideSpeechMsg(uint32 dailog_id);

void AmaNotifyAppMsg_SynchronizeSettingMsg(void);

void AmaNotifyAppMsg_ControlPktMsg(uint8* pkt, uint16 pkt_size);

void AmaNotifyAppMsg_TransportSwitch(ama_transport_t transport);

void AmaNotifyAppMsg_OverrideAssistant(void);

void AmaParse_ResetState(void);

bool AmaParse_ParseData(const uint8* data, uint16 size);

uint16 AmaParse_PrepareVersionPacket(uint8* packet, uint8 major, uint8 minor);

uint16 AmaParse_PrepareVoiceData(uint8 *packet, uint16 length);

uint16 AmaParse_PrepareControlData(uint8* packet, uint16 length);

/***************************************************************************
DESCRIPTION
    Sends the Provide Speech response to phone

PARAMS
    accept - TRUE is accepted else FALSE
    dailog_id - Dailog id for the response

RETURNS
    None
*/
void AmaSendCommand_ProvideSpeechRsp(bool accept, uint32 dailog_id);

#endif /* _AMA_PRIVATE_H_ */


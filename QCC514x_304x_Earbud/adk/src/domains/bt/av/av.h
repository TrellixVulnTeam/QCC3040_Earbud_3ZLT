/*!
\copyright  Copyright (c) 2008 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file AV State Machines (A2DP & AVRCP)

@startuml

    [*] -down-> NULL : appAvInit()
    NULL : Initialising AV application module
    NULL -down-> INITIALISING_A2DP : A2dpInit()

    INITIALISING_A2DP : Initialising A2DP profile library
    INITIALISING_A2DP -down-> INITIALISING_AVRCP : A2DP_INIT_CFM/AvrcpInit()

    INITIALISING_AVRCP : Initialising AVRCP profile library
    INITIALISING_AVRCP -down-> IDLE : AVRCP_INIT_CFM

    IDLE : Initialised and ready for connections

@enduml
*/

#ifndef AV_H_
#define AV_H_

#include <a2dp.h>
#include <avrcp.h>
#include <task_list.h>
#include <source.h>

#include "av_seids.h"
#include "av_context_provider_if.h"
#include "av_callback_interface.h"
#include "bt_device.h"
#include "a2dp_profile.h"
#include "avrcp_profile.h"
#include "av_typedef.h"
#include "audio_sources_list.h"
#include "audio_sources.h"

#include <marshal.h>

#ifdef INCLUDE_AV

extern const av_callback_interface_t av_plugin_interface;

/*! Special value to indicate a volume has not been set. */
#define VOLUME_UNSET 0xff

/*! Maximum allowed volume setting. */
#define VOLUME_MAX (127)

/*! Delay in milliseconds before pausing an a2dp source that
    doesn't get routed */
#define DELAY_BEFORE_PAUSING_UNROUTED_SOURCE (1500)

/*! \brief Codec types used for instance identification */
typedef enum
{
    AV_CODEC_ANY,
    AV_CODEC_TWS,
    AV_CODEC_NON_TWS
} avCodecType;

/*! \brief AV task state machine states */
typedef enum
{
    AV_STATE_NULL,                  /*!< Startup state */
    AV_STATE_INITIALISING_A2DP,     /*!< Initialising A2DP profile library */
    AV_STATE_INITIALISING_AVRCP,    /*!< Initialising AVRCP profile library */
    AV_STATE_IDLE                   /*!< Initialised and ready for connections */
} avState;

/*! When responding to an incoming AVRCP connection, these are the
    allowed responses */
typedef enum
{
    AV_AVRCP_REJECT,
    AV_AVRCP_ACCEPT,
    AV_AVRCP_ACCEPT_PASSIVE     /*!< The passive means accept connection, but make no attempt to maintain it */
} avAvrcpAccept;

/*! \brief Internal message IDs */
enum av_avrcp_internal_messages
{
    AV_INTERNAL_REMOTE_IND,
    AV_INTERNAL_REMOTE_REPEAT,
};

/*! \brief Message IDs from AV to registered AVRCP clients */
enum av_avrcp_messages
{
    AV_AVRCP_CONNECT_IND=AV_AVRCP_MESSAGE_BASE, /*!< This message must be responded to. With a reject if
                                                     not concerned with the link */
    AV_AVRCP_CONNECT_CFM,
    AV_AVRCP_DISCONNECT_IND,            /*!< Indication sent when a link starts disconnecting */

    AV_AVRCP_SET_VOLUME_IND,
    AV_AVRCP_VOLUME_CHANGED_IND,
    AV_AVRCP_PLAY_STATUS_CHANGED_IND,

    AV_AVRCP_VENDOR_PASSTHROUGH_IND,
    AV_AVRCP_VENDOR_PASSTHROUGH_CFM,

    /*! This must be the final message */
    AV_AVRCP_MESSAGE_END
};

/*! \brief Message IDs from AV to registered status clients  */
enum av_status_messages
{
    AV_INIT_CFM=AV_MESSAGE_BASE,
    AV_CREATE_IND,
    AV_DESTROY_IND,

    AV_A2DP_CONNECTED_IND,
    AV_A2DP_DISCONNECTED_IND,
    AV_A2DP_MEDIA_CONNECTED,
    AV_A2DP_AUDIO_CONNECTING,
    AV_A2DP_AUDIO_CONNECTED,
    AV_A2DP_AUDIO_DISCONNECTED,

    /*! Message indicating that the media channel is available for use */

    AV_A2DP_CONNECT_CFM,
    AV_A2DP_DISCONNECT_CFM,

    AV_AVRCP_CONNECTED_IND,
    AV_AVRCP_DISCONNECTED_IND,          /*!< Indication sent when a link completed disconnection */

    AV_AVRCP_CONNECT_CFM_PROFILE_MANAGER,
    AV_AVRCP_DISCONNECT_CFM,
    AV_AVRCP_PLAY_STATUS_PLAYING_IND,
    AV_AVRCP_PLAY_STATUS_NOT_PLAYING_IND,

    AV_STREAMING_ACTIVE_IND,
    AV_STREAMING_INACTIVE_IND,

    /*! This must be the final message */
    AV_MESSAGE_END
};

/*! \brief Message IDs for AV messages to registered UI clients */
enum av_ui_messages
{
    AV_REMOTE_CONTROL = AV_UI_MESSAGE_BASE,
    AV_ERROR,
    AV_CONNECTED,
    AV_CONNECTED_PEER,
    AV_STREAMING_ACTIVE,
    AV_STREAMING_ACTIVE_APTX,
    AV_STREAMING_INACTIVE,
    AV_DISCONNECTED,
    AV_LINK_LOSS,
    AV_A2DP_NOT_ROUTED,

    /*! This must be the final message */
    AV_UI_MESSAGE_END
};

typedef struct
{
    audio_source_t audio_source;
} AV_A2DP_AUDIO_CONNECT_MESSAGE_T;


typedef struct
{
    audio_source_t audio_source;
} AV_A2DP_AUDIO_DISCONNECT_MESSAGE_T;


/*! Message sent when the media channel is connected and available for use (whatever that means) */


/*! \brief Message sent to indicate that an A2DP link has disconnected.
    This is sent to all tasks registered for messages */
typedef struct
{
    avInstanceTaskData *av_instance;    /*!< The AV instance this applies to */
    bdaddr bd_addr;                     /*!< Bluetooth address of connected device */
    bool local_initiated;               /*!< Whether the connection was requested on this device */
} AV_A2DP_CONNECTED_IND_T;
extern const marshal_type_descriptor_t marshal_type_descriptor_AV_A2DP_CONNECTED_IND_T;


/*! \brief Reasons for an A2DP link disconnection.
    This is a reduced list compared to hci disconnect codes available */
typedef enum
{
    AV_A2DP_INVALID_REASON,
    AV_A2DP_CONNECT_FAILED,         /*!< A connection attempt failed */
    AV_A2DP_DISCONNECT_LINKLOSS,    /*!< Link dropped */
    AV_A2DP_DISCONNECT_NORMAL,      /*!< A requested disconnect completed */
    AV_A2DP_DISCONNECT_LINK_TRANSFERRED,  /*!< Link transferred to peer device */
    AV_A2DP_DISCONNECT_ERROR,       /*!< Disconnect due to some error */
} avA2dpDisconnectReason;

/*! \brief Message sent to indicate that an A2DP link has disconnected.
    This is sent to all tasks registered for messages */
typedef struct
{
    avInstanceTaskData *av_instance;    /*!< The AV instance this applies to */
    bdaddr bd_addr;                     /*!< Bluetooth address of disconnected device */
    avA2dpDisconnectReason reason;      /*!< Reason for disconnection */
} AV_A2DP_DISCONNECTED_IND_T;
extern const marshal_type_descriptor_t marshal_type_descriptor_AV_A2DP_DISCONNECTED_IND_T;

typedef struct
{
    device_t device;
    bool successful;
} AV_A2DP_CONNECT_CFM_T;

typedef struct
{
    device_t device;
    bool successful;
} AV_A2DP_DISCONNECT_CFM_T;

/*! \brief Message sent to indicate avrcp playback status is playing */
typedef struct
{
    avInstanceTaskData *av_instance;    /*!< The AV instance this applies to */
} AV_AVRCP_PLAY_STATUS_PLAYING_IND_T;

/*! \brief Message sent to indicate avrcp playback status is not playing */
typedef struct
{
    avInstanceTaskData *av_instance;    /*!< The AV instance this applies to */
} AV_AVRCP_PLAY_STATUS_NOT_PLAYING_IND_T;


/*! \brief Message sent to indicate that an AVRCP link has connected.
    This is sent to all tasks registered for avrcp messages */
typedef struct
{
    avInstanceTaskData *av_instance;    /*!< The AV instance this applies to */
    bdaddr bd_addr;                     /*!< Bluetooth address of connected device */
    uint16 connection_id;               /*!< Connection ID */
    uint16 signal_id;                   /*!< Signalling identifier  */
} AV_AVRCP_CONNECT_IND_T;

/*! Message sent to indicate result of requested AVRCP link connection */
typedef struct
{
    avInstanceTaskData *av_instance;    /*!< The AV instance this applies to */
    avrcp_status_code status;           /*!< Status of the connection request */
} AV_AVRCP_CONNECT_CFM_T;

/*! Message sent to indicate that an AVRCP link connection has disconnected */
typedef struct
{
    avInstanceTaskData *av_instance;    /*!< The AV instance this applies to */
    avrcp_status_code status;           /*!< Reason for the disconnection */
} AV_AVRCP_DISCONNECT_IND_T;

typedef struct
{
    device_t device;
    bool successful;
} AV_AVRCP_CONNECT_CFM_PROFILE_MANAGER_T;

typedef struct
{
    device_t device;
    bool successful;
} AV_AVRCP_DISCONNECT_CFM_T;

/*! Message sent to indicate an AVRCP link has connected */
typedef struct
{
    avInstanceTaskData *av_instance;    /*!< The AV instance this applies to */
    bdaddr bd_addr;                     /*!< Bluetooth address of the disconnected device */
    Sink sink;                          /*!< The Sink for the link */
} AV_AVRCP_CONNECTED_IND_T;
extern const marshal_type_descriptor_t marshal_type_descriptor_AV_AVRCP_CONNECTED_IND_T;

/*! \brief Message sent to indicate an AVRCP link has completed disconnection process
    Normally expect a \ref AV_AVRCP_DISCONNECT_IND, followed by \ref AV_AVRCP_DISCONNECTED_IND.
 */
typedef struct
{
    avInstanceTaskData *av_instance;    /*!< The AV instance this applies to */
    bdaddr bd_addr;                     /*!< Bluetooth address of the disconnected device */
} AV_AVRCP_DISCONNECTED_IND_T;
extern const marshal_type_descriptor_t marshal_type_descriptor_AV_AVRCP_DISCONNECTED_IND_T;

/*! Message sent to report an incoming passthrough message */
typedef struct
{
    avInstanceTaskData *av_instance;    /*!< The AV instance this applies to */
    uint16 opid;                        /*!< The passthrough operation ID */
    uint16 size_payload;                /*!< Size of the variable length payload (in octets) */
    uint8 payload[1];                   /*!< Actual payload. The message is variable length. */
} AV_AVRCP_VENDOR_PASSTHROUGH_IND_T;

/*! Message sent to confirm an outgoing passthrough has been processed */
typedef struct
{
    avInstanceTaskData *av_instance;    /*!< The AV instance this applies to */
    avrcp_status_code status;           /*!< The status of the request. Can be failure. */
    uint16 opid;                        /*!< The passthrough operation ID */
} AV_AVRCP_VENDOR_PASSTHROUGH_CFM_T;

/*! Internal message for a remote control message */
typedef struct
{
    avc_operation_id op_id; /*!< Command to repeat */
    uint8 state;            /*!< State relevant to the command. Often just 0 or 1. */
    bool beep;              /*!< Whether to issue a UI indication when this command is applied */
} AV_INTERNAL_REMOTE_IND_T;

typedef struct
{
    avInstanceTaskData *av_instance;    /*!< The AV instance this applies to */
    bdaddr bd_addr;
    uint8 volume;
} AV_AVRCP_SET_VOLUME_IND_T;

typedef struct
{
    avInstanceTaskData *av_instance;    /*!< The AV instance this applies to */
    bdaddr bd_addr;
    uint8 volume;
} AV_AVRCP_VOLUME_CHANGED_IND_T;

typedef struct
{
    avInstanceTaskData *av_instance;    /*!< The AV instance this applies to */
    bdaddr bd_addr;
    avrcp_response_type play_status;
} AV_AVRCP_PLAY_STATUS_CHANGED_IND_T;

/* Dummy message to permit marshal type definition. */
typedef struct
{
    uint32 reserved;
} AV_STREAMING_ACTIVE_IND_T;
extern const marshal_type_descriptor_t marshal_type_descriptor_AV_STREAMING_ACTIVE_IND_T;

/* Dummy message to permit marshal type definition. */
typedef struct
{
    uint32 reserved;
} AV_STREAMING_INACTIVE_IND_T;
extern const marshal_type_descriptor_t marshal_type_descriptor_AV_STREAMING_INACTIVE_IND_T;

/*! Internal message for a repeated remote control message */
typedef struct
{
    avc_operation_id op_id; /*!< Command to repeat */
    uint8 state;            /*!< State relevant to the command. Often just 0 or 1. */
    bool beep;              /*!< Whether to issue a UI indication when this command is applied */
} AV_INTERNAL_REMOTE_REPEAT_T;

/*! \brief Internal A2DP & AVRCP message IDs */
enum av_internal_messages
{
    AV_INTERNAL_A2DP_BASE,
    AV_INTERNAL_A2DP_SIGNALLING_CONNECT_IND = AV_INTERNAL_A2DP_BASE,
    AV_INTERNAL_A2DP_CONNECT_REQ,
    AV_INTERNAL_A2DP_DISCONNECT_REQ,
    AV_INTERNAL_A2DP_SUSPEND_MEDIA_REQ,
    AV_INTERNAL_A2DP_RESUME_MEDIA_REQ,
    AV_INTERNAL_A2DP_CONNECT_MEDIA_REQ,
    AV_INTERNAL_A2DP_DISCONNECT_MEDIA_REQ,
    AV_INTERNAL_AVRCP_UNLOCK_IND,
    AV_INTERNAL_A2DP_DESTROY_REQ,
    AV_INTERNAL_A2DP_UNROUTED_PAUSE_REQUEST,
    AV_INTERNAL_A2DP_UNROUTED_PREVENT_REPEAT,
    AV_INTERNAL_A2DP_TOP,

    AV_INTERNAL_AVRCP_BASE,
    AV_INTERNAL_AVRCP_CONNECT_IND = AV_INTERNAL_AVRCP_BASE, /*!< Internal indication of signalling channel connection */
    /*TODO: Can we simply not ignore the messages*/
    AV_INTERNAL_AVRCP_CONNECT_RES,
    AV_INTERNAL_AVRCP_CONNECT_REQ,            /*!< Internal request to connect AVRCP */
    AV_INTERNAL_AVRCP_CONNECT_LATER_REQ,      /*!< Internal request to connect AVRCP later */
    AV_INTERNAL_AVRCP_DISCONNECT_REQ,         /*!< Internal request to disconnect AVRCP */
    AV_INTERNAL_AVRCP_DISCONNECT_LATER_REQ,   /*!< Internal request to disconnect AVRCP later */
    AV_INTERNAL_AVRCP_DESTROY_REQ,            /*!< Internal request to clean up */
    AV_INTERNAL_AVRCP_REMOTE_REQ,             /*!< Internal request to send AVRCP passthrough command */
    AV_INTERNAL_AVRCP_REMOTE_REPEAT_REQ,      /*!< Internal request to send repeat AVRCP passthrough command */
    AV_INTERNAL_AVRCP_VENDOR_PASSTHROUGH_RES,
    AV_INTERNAL_AVRCP_VENDOR_PASSTHROUGH_REQ, /*!< Internal request to send a vendor passthrough command */
    AV_INTERNAL_AVRCP_NOTIFICATION_REGISTER_REQ,
    AV_INTERNAL_AVRCP_PLAY_REQ,               /*!< Internal request to send AVRCP play */
    AV_INTERNAL_AVRCP_PAUSE_REQ,              /*!< Internal request to send AVRCP pause */
    AV_INTERNAL_AVRCP_PLAY_TOGGLE_REQ,        /*!< Internal request to send AVRCP play or pause depending on current playback status */
    AV_INTERNAL_AVRCP_CLEAR_PLAYBACK_LOCK_IND, /*!< Timeout waiting for playback status notifiation, clear lock */
    AV_INTERNAL_SET_ABSOLUTE_VOLUME_IND,      /*!< Internal message to apply volume */
    AV_INTERNAL_ALLOW_ABSOLUTE_VOLUME,        /*!< Internal message to stop absolute volume suppression */
    AV_INTERNAL_AVRCP_TOP,

    AV_INTERNAL_VOLUME_STORE_REQ,
};

/*! Internal indication of signalling channel connection */
typedef struct
{
    uint16      device_id;              /*!< A2DP Device Identifier */
    unsigned    flags:6;                /*!< Connect flags */
} AV_INTERNAL_A2DP_SIGNALLING_CONNECT_IND_T;

/*! Internal request to connect signalling channel */
typedef struct
{
    unsigned    flags:6;                /*!< Connect flags */
    unsigned    num_retries:3;          /*!< Number of connect retries */
} AV_INTERNAL_A2DP_CONNECT_REQ_T;

/*! Internal request to disconnect */
typedef struct
{
    unsigned    flags:6;                /*!< Disconnect flags */
} AV_INTERNAL_A2DP_DISCONNECT_REQ_T;

/*! Internal request to connect media channel */
typedef struct
{
    uint8  seid;                        /*!< Required SEID, or 0 if no preference */
    uint16 delay_ms;                    /*!< Delay in milliseconds to connect media channel */
} AV_INTERNAL_A2DP_CONNECT_MEDIA_REQ_T;


/*! Internal request to suspend streaming */
typedef struct
{
    avSuspendReason reason;             /*!< Suspend reason */
} AV_INTERNAL_A2DP_SUSPEND_MEDIA_REQ_T;

/*! Internal request to resume streaming */
typedef struct
{
    avSuspendReason reason;             /*!< Start reason */
} AV_INTERNAL_A2DP_RESUME_MEDIA_REQ_T;

/*! Internal request to send SEP capabilities to remote device */
typedef struct
{
    /*! A2dp library transaction id that should be passed back to library when calling A2dpGetCapsResponse() */
    uint8  id;
    /*! The seid being queried */
    const sep_config_type *sep_config;
} AV_INTERNAL_A2DP_GET_CAPS_IND_T;

/*! Internal indication of channel connection */
typedef struct
{
    uint16  connection_id;  /*!< The ID for this connection.  Must be returned as part of the AvrcpConnectResponse API. */
    uint16  signal_id;      /*!< Signalling identifier */
} AV_INTERNAL_AVRCP_CONNECT_IND_T;

/*! Internal routing of a response to a connection request, indicating acceptance */
typedef struct
{
    Task            ind_task;       /*!< Task that received indication */
    Task            client_task;    /*!< Task responding */
    uint16          connection_id;  /*!< Connection ID */
    uint16          signal_id;      /*!< Signalling identifier  */
    avAvrcpAccept   accept;         /*!< How the connection is accepted/rejected */
} AV_INTERNAL_AVRCP_CONNECT_RES_T;

/*! Internal request to connect AVRCP channel */
typedef struct
{
    /*! Task requesting the connection. */
    Task    client_task;
} AV_INTERNAL_AVRCP_CONNECT_REQ_T;

/*! Internal request message to disconnect AVRCP channel */
typedef struct
{
    Task    client_task;    /*!< Task requesting the disconnect.  */
} AV_INTERNAL_AVRCP_DISCONNECT_REQ_T;

/*! Internal message returning the response to a
    \ref AV_INTERNAL_AVRCP_VENDOR_PASSTHROUGH_REQ */
typedef struct
{
    avrcp_response_type response;       /*!< Response code */
} AV_INTERNAL_AVRCP_VENDOR_PASSTHROUGH_RES_T;

/*! Internal message to pass a vendor command over AVRCP.
    Used for TWS signalling to both peer and handset */
typedef struct
{
    Task client_task;           /*!< Task to receive response messages */
    avc_operation_id op_id;     /*!< ID to be sent */
    uint16 size_payload;        /*!< Number of octets in the payload, 0 allowed */
    uint8 payload[1];           /*!< Start of command payload. Message is variable length */
} AV_INTERNAL_AVRCP_VENDOR_PASSTHROUGH_REQ_T;

/*! Internal message to initiate a remote control request, possibly repeating. */
typedef struct
{
    avc_operation_id op_id; /*!< Operation ID */
    uint8 state;            /*!< Button press or release */
    unsigned ui:1;          /*!< Flag when set indicates tone should be played */
    uint16 repeat_ms;       /*!< Period between repeats (0 for none) */
} AV_INTERNAL_AVRCP_REMOTE_REQ_T;

/*! Internal message to trigger a remote control request repeatedly.
    Same structure as AV_INTERNAL_AVRCP_REMOTE_REQ_T, which is for the first
    request. */
typedef AV_INTERNAL_AVRCP_REMOTE_REQ_T AV_INTERNAL_AVRCP_REMOTE_REPEAT_REQ_T;

/*! Internal message to initiate registering notifications. */
typedef struct
{
    avrcp_supported_events event_id;
} AV_INTERNAL_AVRCP_NOTIFICATION_REGISTER_REQ_T;

/*! \brief Get Sink for AV instance */
#define appAvGetSink(theInst) \
    (appA2dpIsConnected(theInst) ? A2dpSignallingGetSink((theInst)->a2dp.device_id) : \
                                   appAvrcpIsConnected(theInst) ? AvrcpGetSink((theInst)->avrcp.avrcp) : 0)

/*! \brief Check if a AV instance has same Bluetooth Address */
#define appAvIsBdAddr(bd_addr) \
    (appAvInstanceFindFromBdAddr((bd_addr)) != NULL)


/*! A2DP connect/disconnect flags */
typedef enum
{
    A2DP_CONNECT_NOFLAGS      = 0,         /*!< Empty flags set */
    A2DP_CONNECT_MEDIA        = (1 << 0),  /*!< Connect media channel automatically */
    A2DP_START_MEDIA_PLAYBACK = (1 << 1),  /*!< Start media playback on connect */
} appAvA2dpConnectFlags;


/*!< AV data structure */
extern avTaskData  app_av;

/*! Get pointer to AV data structure */
#define AvGetTaskData()  (&app_av)

#define AvGetTask() &(app_av.task)

/*! \brief Initialise AV task

    Called at start up to initialise the main AV task and initialises the state-machine.
*/
bool appAvInit(Task init_task);

/*! \brief returns av task pointer to requesting component

    \return av task pointer.
*/
Task appGetAvPlayerTask(void);

/*! \brief Register a task to receive avrcp messages from the av module

    \note This function can be called multiple times for the same task. It
      will only appear once on a list.

    \param  client_task Task to be added to the list of registered clients
    \param  interests   Not used at present
 */
void appAvAvrcpClientRegister(Task client_task, uint8 interests);

/*! \brief Register a task to receive AV status messages

    \note This function can be called multiple times for the same task. It
      will only appear once on a list.

    \param  client_task Task to be added to the list of registered clients
 */
void appAvStatusClientRegister(Task client_task);

/*! \brief Register a task to receive AV UI messages

    \note This function can be called multiple times for the same task. It
      will only appear once on a list.

    \param  client_task Task to be added to the list of registered clients
 */
void appAvUiClientRegister(Task client_task);

/*! \brief Unregister a task to receive AV status messages

    \note This function can be called multiple times for the same task. It
      will only appear once on a list.

    \param  client_task Task to be removed from the list of registered clients
 */
void appAvStatusClientUnregister(Task client_task);

/*! \brief Connect A2DP to a specific Bluetooth device

    This function requests an A2DP connection. If there is no AV entry
    for the device, it will be created. If the AV already exists any
    pending link destructions for AVRCP and A2DP will be cancelled.

    If there is an existing A2DP connection, then the function will
    return FALSE.

    \param  bd_addr     Bluetooth address of device to connect to
    \param  a2dp_flags  Flags to apply to connection. Can be combined as a bitmask.

    \return TRUE if the connection has been requested. FALSE otherwise, including
        when a connection already exists.
*/
bool appAvA2dpConnectRequest(const bdaddr *bd_addr, appAvA2dpConnectFlags a2dp_flags);

/*! \brief Request disconnection of A2DP from specified AV

    \param[in] av_inst  Instance to disconnect A2DPfrom

    \return TRUE if disconnect has been requested
 */
bool appAvA2dpDisconnectRequest(avInstanceTaskData *av_inst);

/*! \brief Connect AVRCP to a specific Bluetooth device

    This function requests an AVRCP connection.
    If there is no AV entry for the device, it will be created. No check is
    made for an existing AVRCP connection, but if the AV already exists any
    pending link destructions for AVRCP and A2DP will be cancelled.

    If the function returns TRUE, then the client_task should receive an
    AV_AVRCP_CONNECT_CFM message whether the connection succeeds or not. See note.

    \note If there was no existing AV entry for the device, and hence no ACL,
    then the AV_AVRCP_CONNECT_CFM message will not be sent if the ACL could not
    be created.

    \param  client_task Task to receive response messages
    \param  bd_addr     Bluetooth address of device to connect to

    \return TRUE if the connection has been requested, FALSE otherwise
*/
bool appAvAvrcpConnectRequest(Task client_task, const bdaddr *bd_addr);

/*! \brief Application response to a connection request

    After a connection request has been received and processed by the
    application, this function should be called to accept or reject the
    request.

    \param[in] ind-task       Task that received ind, that will no longer receive subsequent messages
    \param[in] client_task    Task responding, that will receive subsequent messages
    \param[in] bd_addr        Bluetooth address of connected device
    \param     connection_id  Connection ID
    \param     signal_id      Signal ID
    \param     accept         Whether to accept or reject the connection
 */
void appAvAvrcpConnectResponse(Task ind_task, Task client_task, const bdaddr *bd_addr, uint16 connection_id, uint16 signal_id, avAvrcpAccept accept);

/*! \brief Request disconnection of AVRCP notifying the specified client.

    \param[in] client_task  Task to receive disconnect confirmation
    \param[in] av_inst      Instance to disconnect AVRCP from

    \return TRUE if disconnect has been requested
 */
bool appAvAvrcpDisconnectRequest(Task client_task, avInstanceTaskData* av_inst);

/*! \brief If asked to connect to a handset, set the play flag so that media
    starts on connection

    \param  play    If TRUE, set the play flag
 */
void appAvPlayOnHandsetConnection(bool play);

/*! \brief Function to send a status message to AV's status clients.
    \param id The message ID to send.
    \param msg The message content.
    \param size The size of the message.
*/
void appAvSendStatusMessage(MessageId id, void *msg, size_t size);

/*! \brief Function to send a status message to AV's status clients.
    \param av_instance The instance for which audio was connected/disconnected
    \param connected TRUE to send AV_A2DP_AUDIO_CONNECTED, FALSE for AV_A2DP_AUDIO_DISCONNECTED
*/
void AvSendAudioConnectedStatusMessage(avInstanceTaskData* av_instance, MessageId id);

/*! \brief Function to send a UI message to AV's UI clients.
    \param id The message ID to send.
    \param msg The message content.
    \param size The size of the message.
*/
void appAvSendUiMessage(MessageId id, void *msg, size_t size);

/*! \brief Function to send a UI message without message content to AV's UI clients.
    \param id The message ID to send.
*/
void appAvSendUiMessageId(MessageId id);

/*! \brief Suspend AV link

    This function is called whenever a module in the headset has a reason to
    suspend AV streaming.  An internal message is sent to every AV
    instance, if the instance is currently streaming it will attempt to
    suspend.

    \note There may be multple reasons that streaming is suspended at any time.

    \param  reason  Why streaming should be suspended. What activity has started.
*/
void appAvStreamingSuspend(avSuspendReason reason);

/*! \brief Resume AV link

    This function is called whenever a module in the headset has cleared it's
    reason to suspend AV streaming.  An internal message is sent to every AV
    instance.

    \note There may be multple reasons why streaming is currently suspended.
      All suspend reasons have to be cleared before the AV instance will
      attempt to resume streaming.

    \param  reason  Why streaming can now be resumed. What activity has completed.
*/
void appAvStreamingResume(avSuspendReason reason);

/*! \brief Create AV instance for A2DP sink or source

    Creates an AV instance entry for the bluetooth address supplied (bd_addr).

    \note No check is made for there already being an instance
    for this address.

    \param bd_addr Address the created instance will represent

    \return Pointer to the created instance, or NULL if it was not
        possible to create

*/
avInstanceTaskData *appAvInstanceCreate(const bdaddr *bd_addr, const av_callback_interface_t *plugin_interface);

/*! \brief Destroy AV instance for A2DP sink or source

    This function should only be called if the instance no longer has
    either a connected A2DP or a connected AVRCP.  If either is still
    connected, the function will silently fail.

    The function will panic if theInst is not valid, for instance
    if already destroyed.

    \param  theInst The instance to destroy

*/
void appAvInstanceDestroy(avInstanceTaskData *theInst);

/*! \brief Return AV instance for A2DP sink

    This function walks through the AV instance table looking for the
    first instance which is a connected sink that can use the
    specified codec.

    \param codec_type   Codec to look for

    \return Pointer to AV information for a connected source,NULL if none
        was found
*/
avInstanceTaskData *appAvGetA2dpSink(avCodecType codec_type);

/*! \brief Return AV instance for A2DP source

    This function walks through the AV instance table looking for the
    first instance which is a connected source.

    \return Pointer to AV information for a connected source,NULL if none
            was found
*/
avInstanceTaskData *appAvGetA2dpSource(void);

/*! \brief Find AV instance with Bluetooth Address

    \note This function returns the AV. It does not check for an
            active connection, or whether A2DP/AVRCP exists.

    \param  bd_addr Bluetooth address to search our AV connections for

    \return Pointer to AV data that matches the bd_addr requested. NULL if
            none was found.
*/
avInstanceTaskData *appAvInstanceFindFromBdAddr(const bdaddr *bd_addr);

/*! \brief Find AV instance with A2DP state

    This function attempts to find the other AV instance with a matching A2DP state.
    The state is selected using a mask and matching state.

    \param  theInst     AV instance that we want to find a match to
    \param  mask        Mask value to be applied to the a2dp state of a connection
    \param  expected    State expected after applying the mask

    \return Pointer to the AV that matches, NULL if no matching AV was found
*/
avInstanceTaskData *appAvInstanceFindA2dpState(const avInstanceTaskData *theInst, uint8 mask, uint8 expected);

/*! \brief Find AV instance for AVRCP passthrough

    This function finds the AV instance to send a AVRCP passthrough command.
    If an AV instance has a Sink SEID as it's last used SEID, then the
    passthrough command should be sent using that instance, otherwise use
    an AV instance with no last used SEID as this will be for an AV source that
    has just paired but hasn't yet connected the A2DP media channel.

    \return Pointer to the AV to use, NULL if no appropriate AV found
*/
avInstanceTaskData *appAvInstanceFindAvrcpForPassthrough(audio_source_t source);

/*! \brief Find AV instance with device

    \note This function returns the AV. It does not check for an
            active connection, or whether A2DP/AVRCP exists.

    \param  device Device to search our AV connections for

    \return Pointer to AV data that matches the device requested. NULL if
            none was found.
*/
avInstanceTaskData *Av_InstanceFindFromDevice(device_t device);

/*! \brief Find device with AV instance

    \note This function returns the device. It does not check for an
            active connection, or whether A2DP/AVRCP exists.

    \param  instance The instance to search for

    \return Device associated with the AV instance or NULL if none was found.
*/
device_t Av_FindDeviceFromInstance(avInstanceTaskData* av_instance);

void appAvInstanceA2dpConnected(avInstanceTaskData *theInst);
void appAvInstanceA2dpDisconnected(avInstanceTaskData *theInst);
void appAvInstanceAvrcpConnected(avInstanceTaskData *theInst);
void appAvInstanceAvrcpDisconnected(avInstanceTaskData *theInst, bool is_disconnect_request);

void appAvInstanceHandleMessage(Task task, MessageId id, Message message);

/*! \brief Check if at least one A2DP or AVRCP link is connected

    \return TRUE if either an A2DP or an AVRCP link is connected. FALSE otherwise.
*/
bool appAvHasAConnection(void);

/*! \brief Check if A2DP is connected

    \return TRUE if there is an AV instance that is connected in A2DP sink role
*/
bool Av_IsA2dpSinkConnected(void);

/*! \brief Check if A2DP Source is connected

    \return TRUE if there is an AV instance that is connected in A2DP Source role
*/
bool Av_IsA2dpSourceConnected(void);

/*! \brief Check if A2DP is streaming

    \return TRUE if there is an AV that is streaming
*/
bool Av_IsA2dpSinkStreaming(void);

/*! \brief Has sink device associated with av_instance started streaming

    \param  av_instance The instance to query

    \return TRUE if the device has started streaming in A2DP sink role
*/
bool Av_InstanceIsA2dpSinkStarted(avInstanceTaskData* av_instance);

/*! \brief Is the device associated with av_instance disconnected

    \param  av_instance The instance to query

    \return TRUE if the device is disconnected
*/
bool Av_InstanceIsDisconnected(avInstanceTaskData* theInst);

/*! \brief Is the device associated with av_instance connected as a2dp sink

    \param  av_instance The instance to query

    \return TRUE if the device is connected in a2dp sink role
*/
bool Av_InstanceIsA2dpSinkConnected(avInstanceTaskData* theInst);

/*! \brief Check if AV play status

    \return TRUE if there is an AV that is active
*/
bool appAvIsPlayStatusActive(void);

/*! \brief Check if an instance is valid

    Checks if the instance passed is still a valid AV. This allows
    you to check whether theInst is still valid.

    \param  theInst Instance to check

    \returns TRUE if the instance is valid, FALSE otherwise
 */
bool appAvIsValidInst(avInstanceTaskData* theInst);

/*! \brief provides AV(media player) current context to ui module

    \param[in]  void

    \return     current context of av module.
*/
audio_source_provider_context_t AvGetCurrentContext(audio_source_t source);

/*! \brief Check if AVRCP is connected for AV usage

    \param[in]  theInst The AV Instance to be checked for an AVRCP connection.

    \return     TRUE if the AV task of this instance is registered for AVRCP messages
*/
bool appAvIsAvrcpConnected(avInstanceTaskData* theInst);


/*! \brief Schedules media playback if in correct AV state and flag is set.
    \param  theInst The AV instance.
    \return TRUE if media play is scheduled, otherwise FALSE.
 */
bool appAvInstanceStartMediaPlayback(avInstanceTaskData *theInst);

void appAvConfigStore(void);

/*! \brief Set the play status if the real status is not known

    The AV should know whether we are playing audio, based on AVRCP
    status messages. This information can be missing, in which case
    this function allows you to set a status. It won't override
    a known status.

    \param theInst  the AV instance which should take the hint
    \param status   The status hint to be used
 */
void appAvHintPlayStatus(avInstanceTaskData *theInst, avrcp_play_status status);

/*! \brief Query if one AV link is playing.
    \return TRUE if playing, forward or reverse seeking.
*/
bool Av_IsInstancePlaying(avInstanceTaskData* theInst);

/*! \brief Query if any AV links are playing.
    \return TRUE if any link is playing, forward or reverse seeking.
*/
bool Av_IsPlaying(void);

/*! \brief Query if the AV link is paused or stopped.
    \return TRUE if the instance is paused or stopped.
*/
bool Av_IsInstancePaused(avInstanceTaskData* theInst);

/*! \brief Query if all AV links are paused or stopped.
    \return TRUE if all instances are paused or stopped.
*/
bool Av_IsPaused(void);

/*! \brief Performs setup required when this device becomes the primary.

*/
void Av_SetupForPrimaryRole(void);

/*! \brief Performs setup required when this device becomes the secondary.

*/
void Av_SetupForSecondaryRole(void);

/*! \brief Inform AV that the audio latency has changed.
           AV will determine the new latency and request A2DP profile send a
           delay report to connected source devices.
*/
void Av_ReportChangedLatency(void);

/*! \brief Obtain the source associated with av_instance

    \param  av_instance The instance to query

    \return the source for this instance. audio_source_none if none associated. 
*/
audio_source_t Av_GetSourceForInstance(avInstanceTaskData* instance);

/*! \brief Obtain the handset av_instance associated with source

    \param  source The source to find the instance for.

    \return pointer to the handset instance associated with source. NULL if none
*/
avInstanceTaskData* Av_GetInstanceForHandsetSource(audio_source_t source);

/*! \brief Get the device associated with an AV instance

    \param  av_instance The avInstanceTaskData*
    
    \return The device associated with the AV instance or NULL if there
    is no device associated with the AV instance
*/
device_t Av_GetDeviceForInstance(avInstanceTaskData* av_instance);

/*! \brief Update the volume for the currently focussed handset
*/
void Av_UpdateStoredVolumeForFocussedHandset(void);

/*! \brief Provides callbacks to components that may have context information on an A2DP stream
    \return TRUE on success, FALSE otherwise
*/
bool Av_RegisterContextProvider(const av_context_provider_if_t *provider_if);

/*! \brief Reset play status and hint for an AV instance
*/
void Av_ResetPlayStatus(avInstanceTaskData* av_instance);

#else
#define Av_IsA2dpSinkStreaming() (FALSE)
#endif /* INCLUDE_AV */

#endif /* AV_H_ */

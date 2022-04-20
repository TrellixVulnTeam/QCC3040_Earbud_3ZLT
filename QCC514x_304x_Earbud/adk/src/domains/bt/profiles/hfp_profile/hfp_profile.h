/*!
\copyright  Copyright (c) 2008 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   hfp_profile HFP Profile
\ingroup    profiles
\brief      Interface to application domain HFP component.

@startuml

    [*] --> NULL
    NULL --> HFP_STATE_INITIALISING_HFP

    HFP_STATE_INITIALISING_HFP : Initialising HFP instance
    HFP_STATE_INITIALISING_HFP --> HFP_STATE_DISCONNECTED : HFP_INIT_CFM

    HFP_STATE_DISCONNECTED : No HFP connection
    state HFP_STATE_CONNECTING {
        HFP_STATE_CONNECTING_LOCAL : Locally initiated connection in progress
        HFP_STATE_CONNECTING_REMOTE : Remotely initiated connection is progress
        HFP_STATE_DISCONNECTED -down-> HFP_STATE_CONNECTING_LOCAL
        HFP_STATE_DISCONNECTED -down-> HFP_STATE_CONNECTING_REMOTE
    }

    state HFP_STATE_CONNECTED {

        HFP_STATE_CONNECTING --> HFP_STATE_CONNECTED_IDLE : no_call_setup/no_call
        HFP_STATE_CONNECTING --> HFP_STATE_CONNECTED_INCOMING : incoming_call_setup/no_call
        HFP_STATE_CONNECTING --> HFP_STATE_CONNECTED_OUTGOING : outgoing_call_setup/no_call
        HFP_STATE_CONNECTING --> HFP_STATE_CONNECTED_OUTGOING : outgoing_call_alerting_setup/no_call

        HFP_STATE_CONNECTING --> HFP_STATE_CONNECTED_ACTIVE : no_call_setup/call
        HFP_STATE_CONNECTING --> HFP_STATE_CONNECTED_ACTIVE : incoming_call_setup/call
        HFP_STATE_CONNECTING --> HFP_STATE_CONNECTED_ACTIVE : outgoing_call_setup/call
        HFP_STATE_CONNECTING --> HFP_STATE_CONNECTED_ACTIVE : outgoing_call_alerting_setup/call

        HFP_STATE_CONNECTED_IDLE : HFP connected, no call in progress
        HFP_STATE_CONNECTED_IDLE -down-> HFP_STATE_CONNECTED_ACTIVE
        HFP_STATE_CONNECTED_OUTGOING : HFP connected, outgoing call in progress
        HFP_STATE_CONNECTED_INCOMING -right-> HFP_STATE_CONNECTED_ACTIVE
        HFP_STATE_CONNECTED_INCOMING : HFP connected, incoming call in progress
        HFP_STATE_CONNECTED_OUTGOING -left-> HFP_STATE_CONNECTED_ACTIVE
        HFP_STATE_CONNECTED_ACTIVE : HFP connected, active call in progress
        HFP_STATE_CONNECTED_ACTIVE -down-> HFP_STATE_DISCONNECTING
        HFP_STATE_DISCONNECTING :

        HFP_STATE_DISCONNECTING -up-> HFP_STATE_DISCONNECTED
    }

@enduml

*/

#ifndef HFP_PROFILE_H_
#define HFP_PROFILE_H_

#include <device.h>
#include <hfp.h>
#include <marshal.h>
#include <task_list.h>
#include "hfp_abstraction.h"
#include "hfp_profile_typedef.h"
#include "volume_types.h"


#ifdef INCLUDE_HFP

/*! Expose access to the app HFP taskdata, so the accessor macros work. */
extern hfpInstanceTaskData the_hfp_instance;

/*! \brief HFP settings structure

    This structure defines the HFP settings that are stored
    in persistent store.
*/
typedef struct
{
    unsigned int volume:4;          /*!< Speaker volume */
    unsigned int mic_volume:4;      /*!< Microphone volume */
} hfpPsConfigData;

/*! \brief Message IDs from HFP to main application task */
enum hfp_profile_messages
{
    APP_HFP_INIT_CFM = APP_HFP_MESSAGE_BASE,    /*!< Indicate HFP has been initialised */
    APP_HFP_CONNECTED_IND,                      /*!< SLC connected */
    APP_HFP_DISCONNECTED_IND,                   /*!< SLC disconnected */
    APP_HFP_SCO_CONNECTED_IND,                  /*!< Active SCO connected*/
    APP_HFP_SCO_DISCONNECTED_IND,               /*!< SCO channel disconnect */
    APP_HFP_SLC_STATUS_IND,                     /*!< SLC status updated */
    APP_HFP_AT_CMD_CFM,                         /*!< Result of an send AT command request */
    APP_HFP_AT_CMD_IND,                         /*!< AT command received not handled within HFP profile  */
    APP_HFP_SCO_INCOMING_RING_IND,              /*!< There is an incoming call (not connected) */
    APP_HFP_SCO_INCOMING_ENDED_IND,             /*!< Incoming call has gone away (unanswered) */
    APP_HFP_VOLUME_IND,                         /*!< New HFP volume level */
    APP_HFP_CONNECT_CFM,
    APP_HFP_DISCONNECT_CFM,
    APP_HFP_SCO_CONNECTING_SYNC_IND,

    APP_HFP_MESSAGE_END,                        /*! This must be the final message */
};

typedef struct
{
    device_t device;
    bool successful;
} APP_HFP_CONNECT_CFM_T;

typedef struct
{
    device_t device;
    bool successful;
} APP_HFP_DISCONNECT_CFM_T;

/* Message content for APP_HFP_SCO_CONNECTING_SYNC_IND */
typedef struct
{
    device_t device;
} APP_HFP_SCO_CONNECTING_SYNC_IND_T;

/*! Message sent to status_notify_list clients indicating HFP profile has connected. */
typedef struct
{
    bdaddr bd_addr;                 /*!< Address of AG */
} APP_HFP_CONNECTED_IND_T;
extern const marshal_type_descriptor_t marshal_type_descriptor_APP_HFP_CONNECTED_IND_T;

/*! \brief HFP disconnect reasons */
typedef enum
{
    APP_HFP_CONNECT_FAILED,             /*!< Connect attempt failed */
    APP_HFP_DISCONNECT_LINKLOSS,        /*!< Disconnect due to link loss following supervision timeout */
    APP_HFP_DISCONNECT_NORMAL,          /*!< Disconnect initiated by local or remote device */
    APP_HFP_DISCONNECT_ERROR            /*!< Disconnect due to unknown reason */
} appHfpDisconnectReason;

/*! Message sent to status_notify_list clients indicating HFP profile has disconnected. */
typedef struct
{
    bdaddr bd_addr;                 /*!< Address of AG */
    appHfpDisconnectReason reason;  /*!< Disconnection reason */
} APP_HFP_DISCONNECTED_IND_T;
extern const marshal_type_descriptor_t marshal_type_descriptor_APP_HFP_DISCONNECTED_IND_T;

/*! Dummy message to permit marhsalling type definition. */
typedef struct
{
    uint32 reserved;
} APP_HFP_SCO_CONNECTED_IND_T;
extern const marshal_type_descriptor_t marshal_type_descriptor_APP_HFP_SCO_CONNECTED_IND_T;

/*! Dummy message to permit marhsalling type definition. */
typedef struct
{
    uint32 reserved;
} APP_HFP_SCO_DISCONNECTED_IND_T;
extern const marshal_type_descriptor_t marshal_type_descriptor_APP_HFP_SCO_DISCONNECTED_IND_T;

/*! Message sent to status_notify_list clients indicating SLC state. */
typedef struct
{
    bool slc_connected;                 /*!< SLC Connected True/False */
    bdaddr bd_addr;                     /*!< Address of AG */
} APP_HFP_SLC_STATUS_IND_T;

/*! Message sent to at_cmd_task with result of AT command transmission. */
typedef struct
{
    bool status;                        /*!< Status indicating if AT command was sent successfully */
} APP_HFP_AT_CMD_CFM_T;

/*! Message sent to at_cmd_task indicating new incoming TWS AT command. */
typedef struct
{
    bdaddr addr;                        /*!< Bluetooth address of Handset */
    uint16 size_data;                   /*!< Size of the AT command */
    uint8 data[1];                      /*!< AT command */
} APP_HFP_AT_CMD_IND_T;

/*! Definition of #APP_HFP_VOLUME_IND message sent to registered
    clients. */
typedef struct
{
    voice_source_t source;  /*!< The source with a new volume */
    uint8 volume;           /*!< Latest HFP volume level. */
} APP_HFP_VOLUME_IND_T;

/*! \brief Initialise the HFP module. */
bool HfpProfile_Init(Task init_task);

/*! \brief Initiate HFP connection to device

    Attempt to connect to the specified HFP AG.

    \param  bd_addr Bluetooth address of the HFP AG to connect to
    \param  profile The version of hfp profile to use when connecting

    \return TRUE if a connection was requested. FALSE is returned
        if there is already an HFP connection. The error will apply
        even if the existing HFP connection is to the requested handset.

*/
bool HfpProfile_ConnectWithBdAddr(const bdaddr *bd_addr, hfp_profile profile);

/*! \brief Initiate HFP profile connection with device

    Attempt to connect to the specified HFP AG.

    \param  bd_addr Bluetooth address of the HFP AG to connect to
*/
void hfpProfile_Connect(bdaddr *bd_addr);

/*! \brief Disconnect HFP profile from a device

    Attempt to disconnect to the specified HFP AG.

    \param  bd_addr Bluetooth address of the HFP AG to disconnect
*/
void hfpProfile_Disconnect(bdaddr *bd_addr);

/*! \brief Register with HFP to receive notifications of SLC connect/disconnect.

    \param  task    The task being registered to receive notifications.
 */
void appHfpClientRegister(Task task);

/*! \brief Register with HFP to receive notifications of state changes.

    \param  task    The task being registered to receive notifications.
 */
void HfpProfile_RegisterStatusClient(Task task);

/*! \brief Register a task to receive HFP message group messages.

    \param task The task that will receive the messages.

    \param group Must be APP_HFP_MESSAGE_GROUP.
*/
void hfpProfile_RegisterHfpMessageGroup(Task task, message_group_t group);

/*! \brief Register a task to receive SYSTEM message group messages. 

    \param task The task that will receive the messages.

    \param group Must be SYSTEM_MESSAGE_GROUP.
*/
void hfpProfile_RegisterSystemMessageGroup(Task task, message_group_t group);

/*! \brief Get the current HFP volume.

    \note Currently uses the voice_source_1 instance.
*/
uint8 appHfpGetVolume(hfpInstanceTaskData * instance);

/*! \brief Inform hfp profile of current device Primary/Secondary role.

    \param primary TRUE to set Primary role, FALSE to set Secondary role.
*/
void HfpProfile_SetRole(bool primary);

/*! \brief Get HFP sink */
Sink HfpProfile_GetSink(hfpInstanceTaskData * instance);

/*! \brief Get current AG address */
bdaddr * HfpProfile_GetHandsetBdAddr(hfpInstanceTaskData * instance);

/*! \brief Get application HFP instance task.

    \note currently this returns the voice_source_1 instance task.
*/
Task HfpProfile_GetInstanceTask(hfpInstanceTaskData * instance);

/*! \brief Is HFP SCO active for a particular HFP instance

    \param instance the HFP instance to query

    \return True if SCO is active for the specified HFP instance
*/
bool HfpProfile_IsScoActiveForInstance(hfpInstanceTaskData * instance);

/*! \brief Is HFP SCO connecting for a particular HFP instance

    \param instance the HFP instance to query

    \return True if SCO is connecting for the specified HFP instance
*/
bool HfpProfile_IsScoConnectingForInstance(hfpInstanceTaskData * instance);

/*! \brief Is HFP SCO disconnecting for a particular HFP instance

    \param instance the HFP instance to query

    \return True if SCO is disconnecting for the specified HFP instance
*/
bool HfpProfile_IsScoDisconnectingForInstance(hfpInstanceTaskData * instance);

/*! \brief Is HFP SCO active for any HFP instance

    \return True if SCO is active for any HFP instance
*/
bool HfpProfile_IsScoActive(void);

/*! \brief Is microphone muted for a particular HFP instance

    \param instance the HFP instance to query

    \return True if microphone is muted for the specified HFP instance
*/
bool HfpProfile_IsMicrophoneMuted(hfpInstanceTaskData * instance);

/*! \brief Is HFP connected for a particular HFP instance

    \param instance the HFP instance to query

    \return True if HFP is connected for the specified HFP instance
*/
bool appHfpIsConnectedForInstance(hfpInstanceTaskData * instance);

/*! \brief Is HFP connected for any HFP instance

    \return True if HFP is connected for any HFP instance
 */
bool appHfpIsConnected(void);

/*! \brief Is HFP disconnected */
bool HfpProfile_IsDisconnected(hfpInstanceTaskData * instance);

/*! \brief Is HFP in a call for a particular HFP instance

    \param instance the HFP instance to query

    \return True if HFP is in a call on the specified HFP instance
*/
bool appHfpIsCallForInstance(hfpInstanceTaskData * instance);

/*! \brief Is HFP in a call for any HFP instance

    \return True if HFP is in a call for any HFP instance
 */
bool appHfpIsCall(void);

/*! \brief Is HFP in an active call for a particular HFP instance

    \param instance the HFP instance to query

    \return True if HFP is in a call for the specified HFP instance
 */
bool appHfpIsCallActiveForInstance(hfpInstanceTaskData * instance);

/*! \brief Is HFP in an active call for any HFP instance

    \return True if HFP is call active for any HFP instance
 */
bool appHfpIsCallActive(void);

/*! \brief Is HFP in an incoming call for a particular HFP instance

    \param instance the HFP instance to query

    \return True if HFP has an incoming call on the specified HFP instance
 */
bool appHfpIsCallIncomingForInstance(hfpInstanceTaskData * instance);

/*! \brief Is HFP in an incoming call for any HFP instance

    \return True if HFP is call incoming for any HFP instance
 */
bool appHfpIsCallIncoming(void);

/*! \brief Is HFP in an outgoing call for a particular HFP instance

    \param instance the HFP instance to query

    \return True if HFP has an outgoing call on the specified HFP instance
 */
bool appHfpIsCallOutgoingForInstance(hfpInstanceTaskData * instance);

/*! \brief Is HFP in an outgoing call for any HFP instance

    \return True if HFP is call outgoing for any HFP instance
 */
bool appHfpIsCallOutgoing(void);

/*! \brief Get the HFP instance for the Focused voice source, as far as UI interactions are concerned. */
hfpInstanceTaskData * HfpProfile_GetInstanceForVoiceSourceWithUiFocus(void);

/*! \brief Get the HFP instance for the routed voice source, if it is an HFP source. */
hfpInstanceTaskData * HfpProfile_GetInstanceForVoiceSourceWithAudioFocus(void);

/*! \brief Get the default volume for an HFP instance

    \return a uint8 with the default volume
 */
volume_t HfpProfile_GetDefaultVolume(void);

/*! \brief Get the default microphone gain for an HFP instance

    \return a uint8 with the default microphone gain
 */
uint8 HfpProfile_GetDefaultMicGain(void);

/*! \brief Set a task which the HFP profile will use to synchronise eSCO start.
    \param task The task.
    \note When HFP_AUDIO_CONNECT_IND_T is received by the profile, the handler
    by default immediately accepts the eSCO connection. If a task is registered
    using this function, the handler sends a APP_HFP_SCO_CONNECTING_SYNC_IND
    message to the registered task instead. When the task receives this message
    it shall take action to prepare for the eSCO connection and then call
    HfpProfile_ScoConnectingSyncResponse to inform hfp profile how to respond.
    \note The APP_HFP_SCO_CONNECTING_SYNC_IND will be sent for all HFP instances.
    \note A NULL task un-sets the sync task.
*/
void HfpProfile_SetScoConnectingSyncTask(Task task);

/*! \brief Respond to the APP_HFP_SCO_CONNECTING_SYNC_IND indication.
    \param device The HFP device being synced.
    \param task The task accepting or rejecting the SCO connection.
    \param accept Set to TRUE to accept the connection.
*/
void HfpProfile_ScoConnectingSyncResponse(device_t device, Task task, bool accept);

/*! \brief Blocks the handset for super wide band(swb)calls.
    \param bd_addr Bluetooth address of the handset to be blocked.
    \return TRUE if handset blocked or FALSE if handset not blocked.
*/
bool HfpProfile_IsHandsetBlockedForSwb(const bdaddr *bd_addr);

/*! \brief Checks if the handset supports super wide band(swb)calls.
    \param qce_codec_mode_id Qualcomm codec extension (QCE) mode ID.
    \return TRUE if handset supports swb otherwise FALSE if handset
            does not support swb.
*/
bool HfpProfile_HandsetSupportsSuperWideband(uint16 qce_codec_mode_id);

#else

#define HfpProfile_IsScoActive() (FALSE)
#define HfpProfile_SetRole(primary) UNUSED(primary)

#endif
#endif /* HFP_PROFILE_H_ */

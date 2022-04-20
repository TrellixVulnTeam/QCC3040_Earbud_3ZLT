/*!
\copyright  Copyright (c) 2008 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   hfp_profile HFP Profile
\ingroup    profiles
\brief      HFP Profile private types.
*/

#ifndef HFP_PROFILE_PRIVATE_H_
#define HFP_PROFILE_PRIVATE_H_

#include <battery_monitor.h>
#include <device.h>
#include <hfp.h>
#include <panic.h>
#include <task_list.h>
#include <bandwidth_manager.h>
#include <voice_sources.h>

#include "hfp_profile_typedef.h"
#include "hfp_abstraction.h"

#define PSKEY_LOCAL_SUPPORTED_FEATURES (0x00EF)
#define PSKEY_LOCAL_SUPPORTED_FEATURES_SIZE (4)
#define PSKEY_LOCAL_SUPPORTED_FEATURES_DEFAULTS { 0xFEEF, 0xFE8F, 0xFFDB, 0x875B }

/*! \brief Get SLC status notify list */
#define appHfpGetSlcStatusNotifyList() (task_list_flexible_t *)(&(hfp_profile_task_data.slc_status_notify_list))

/*! \brief Get status notify list */
#define appHfpGetStatusNotifyList() (task_list_flexible_t *)(&(hfp_profile_task_data.status_notify_list))

/*! Macro for creating a message based on the message name */
#define MAKE_HFP_MESSAGE(TYPE) \
    TYPE##_T *message = PanicUnlessNew(TYPE##_T);
/*! Macro for creating a variable length message based on the message name */
#define MAKE_HFP_MESSAGE_WITH_LEN(TYPE, LEN) \
    TYPE##_T *message = (TYPE##_T *)PanicUnlessMalloc(sizeof(TYPE##_T) + (LEN-1));

#define hfpInstanceFromTask(task) ((hfpInstanceTaskData*)task)

/* Interval in ms to check aptX voice packet status */
#define HFP_CHECK_APTX_VOICE_PACKETS_INTERVAL_MS   (240)

/* Delay in ms to check aptX voice packet status first time.
   This is greater then regular interval (HFP_CHECK_APTX_VOICE_PACKETS_INTERVAL_MS)
   because SWBS decoder may start before the handset SWBS encoder has sent any
   SCO frames.So until the first actual encoded audio frame arrives, decoder logs
   it as a frame_error (since there is no data to decode).So we should
   start reading first frame after longer delay which will ensure that we have
   good sco frames if there is audio and will avoid false trigger of no swb audio. */

#define HFP_CHECK_APTX_VOICE_PACKETS_FIRST_TIME_DELAY_MS   (1500)

typedef struct
{
    TaskData task;
    /*! List of tasks to notify of SLC connection status. */
    TASK_LIST_WITH_INITIAL_CAPACITY(HFP_SLC_STATUS_NOTIFY_LIST_INIT_CAPACITY) slc_status_notify_list;
    /*! List of tasks to notify of general HFP status changes */
    TASK_LIST_WITH_INITIAL_CAPACITY(HFP_STATUS_NOTIFY_LIST_INIT_CAPACITY) status_notify_list;
    /*! List of tasks requiring confirmation of HFP connect requests */
    task_list_with_data_t connect_request_clients;
    /*! List of tasks requiring confirmation of HFP disconnect requests */
    task_list_with_data_t disconnect_request_clients;

    /*! Task to handle TWS+ AT commands. */
    Task at_cmd_task;

    /*! The task to send SCO sync messages */
    Task sco_sync_task;
} hfpTaskData;

extern hfpTaskData hfp_profile_task_data;

/*! \brief Internal message IDs */
enum hfp_profile_internal_messages
{
                                                /*!  Internal message to store the HFP device config */
    HFP_INTERNAL_CONFIG_WRITE_REQ = INTERNAL_MESSAGE_BASE,
    HFP_INTERNAL_HSP_INCOMING_TIMEOUT,          /*!< Internal message to indicate timeout from incoming call */
    HFP_INTERNAL_HFP_CONNECT_REQ,               /*!< Internal message to connect to HFP */
    HFP_INTERNAL_HFP_DISCONNECT_REQ,            /*!< Internal message to disconnect HFP */
    HFP_INTERNAL_HFP_LAST_NUMBER_REDIAL_REQ,    /*!< Internal message to request last number redial */
    HFP_INTERNAL_HFP_VOICE_DIAL_REQ,            /*!< Internal message to request voice dial */
    HFP_INTERNAL_HFP_VOICE_DIAL_DISABLE_REQ,    /*!< Internal message to disable voice dial */
    HFP_INTERNAL_HFP_CALL_ACCEPT_REQ,           /*!< Internal message to accept an incoming call */
    HFP_INTERNAL_HFP_CALL_REJECT_REQ,           /*!< Internal message to reject an incoming call */
    HFP_INTERNAL_HFP_CALL_HANGUP_REQ,           /*!< Internal message to hang up an active call */
    HFP_INTERNAL_HFP_MUTE_REQ,                  /*!< Internal message to mute an active call */
    HFP_INTERNAL_HFP_TRANSFER_REQ,              /*!< Internal message to transfer active call between AG and device */
    HFP_INTERNAL_NUMBER_DIAL_REQ,
    HFP_INTERNAL_OUT_OF_BAND_RINGTONE_REQ,      /*!< Internal message to request out of band ringtone indication */
    HFP_INTERNAL_HFP_RELEASE_WAITING_REJECT_INCOMING_REQ,
    HFP_INTERNAL_HFP_ACCEPT_WAITING_RELEASE_ACTIVE_REQ,
    HFP_INTERNAL_HFP_ACCEPT_WAITING_HOLD_ACTIVE_REQ,
    HFP_INTERNAL_HFP_ADD_HELD_TO_MULTIPARTY_REQ,
    HFP_INTERNAL_HFP_JOIN_CALLS_AND_HANG_UP,
    HFP_INTERNAL_CHECK_APTX_VOICE_PACKETS_COUNTER_REQ,


    /*! This must be the final message */
    HFP_INTERNAL_MESSAGE_END
};

typedef struct
{
    hfpInstanceTaskData * instance;            /*!< Hfp Instance */
}HFP_INTERNAL_OUT_OF_BAND_RINGTONE_REQ_T;

typedef struct
{
    device_t device;        /*!< Device to serialise */
} HFP_INTERNAL_CONFIG_WRITE_REQ_T;

/*! Internal message to indicate timeout from incoming call */
typedef struct
{
    hfpInstanceTaskData * instance;            /*!< Hfp Instance */
} HFP_INTERNAL_HSP_INCOMING_TIMEOUT_T;

/*! Internal connect request message */
typedef struct
{
    bdaddr addr;            /*!< Address of AG */
    hfp_connection_type_t profile;  /*!< Profile to use */
    uint16 flags;           /*!< Connection flags */
} HFP_INTERNAL_HFP_CONNECT_REQ_T;

/*! Internal disconnect request message */
typedef struct
{
    hfpInstanceTaskData * instance;
    bool silent;            /*!< Disconnect silent flag */
} HFP_INTERNAL_HFP_DISCONNECT_REQ_T;

/*!< Internal message to request last number redial */
typedef struct
{
    hfpInstanceTaskData * instance;
} HFP_INTERNAL_HFP_LAST_NUMBER_REDIAL_REQ_T;

/*!< Internal message to request voice dial */
typedef struct
{
    hfpInstanceTaskData * instance;
} HFP_INTERNAL_HFP_VOICE_DIAL_REQ_T;

/*!< Internal message to disable voice dial */
typedef struct
{
    hfpInstanceTaskData * instance;
} HFP_INTERNAL_HFP_VOICE_DIAL_DISABLE_REQ_T;

/*!< Internal message to accept an incoming call */
typedef struct
{
    hfpInstanceTaskData * instance;
} HFP_INTERNAL_HFP_CALL_ACCEPT_REQ_T;

/*!< Internal message to reject an incoming call */
typedef struct
{
    hfpInstanceTaskData * instance;
} HFP_INTERNAL_HFP_CALL_REJECT_REQ_T;

/*!< Internal message to hang up an active call */
typedef struct
{
    hfpInstanceTaskData * instance;
} HFP_INTERNAL_HFP_CALL_HANGUP_REQ_T;

/*! Internal mute request message */
typedef struct
{
    hfpInstanceTaskData * instance;
    bool mute;              /*!< Mute enable/disable */
} HFP_INTERNAL_HFP_MUTE_REQ_T;

/*! Internal audio transfer request message */
typedef struct
{
    voice_source_t source;
    voice_source_audio_transfer_direction_t direction;    /*!< Transfer to/from AG from/to Headset */
} HFP_INTERNAL_HFP_TRANSFER_REQ_T;

typedef struct
{
    hfpInstanceTaskData * instance;
    unsigned length;
    uint8  number[1];
} HFP_INTERNAL_NUMBER_DIAL_REQ_T;

/*! Internal aptx voice packets count message */
typedef struct
{
    hfpInstanceTaskData * instance;
} HFP_INTERNAL_CHECK_APTX_VOICE_PACKETS_COUNTER_REQ_T;

/*! \brief Send a HFP connect confirmation to the specified task.

    This function also removes the task from the pending connection requests task list.

    \param task - the task that shall be notified of the connect confirm.
    \param data - the task_list data, specifying the device address that was connected.
    \param arg - the data regarding the connection, i.e. success bool and bd_addr.
*/
bool HfpProfile_FindClientSendConnectCfm(Task task, task_list_data_t *data, void *arg);

/*! \brief Send a HFP disconnect confirmation to the specified task.

    This function also removes the task from the pending disconnect requests clients task list.

    \param task - the task that shall be notified of the disconnect confirm.
    \param data - the task_list data, specifying the device address that was disconnected.
    \param arg - the data regarding the disconnection, i.e. success bool and bd_addr.
*/
bool HfpProfile_FindClientSendDisconnectCfm(Task task, task_list_data_t *data, void *arg);


/*! \brief Initiate HFP connection to default

    Attempt to connect to the previously connected HFP AG.

    \return TRUE if a connection was requested. FALSE is returned
        in the case of an error such as HFP not being supported by
        the handset or there already being an HFP connection. The
        error will apply even if the existing HFP connection is
        to the requested handset.
*/
bool HfpProfile_ConnectHandset(void);

/*! \brief Store HFP configuration

    \param device - the device in the database to serialise

    This function is called to store the current HFP configuration.

    The configuration isn't stored immediately, instead a timer is started, any
    currently running timer is cancel.  On timer expiration the configuration
    is written to Persistent Store, (see \ref HfpProfile_HandleConfigWriteRequest).
    This is to avoid multiple writes when the user adjusts the playback volume.
*/
void HfpProfile_StoreConfig(device_t device);

/*! \brief Handle config write request

    \param device - the device in the database to serialise

    This function is called to write the current HFP configuration
    stored in the Device Database.
*/
void HfpProfile_HandleConfigWriteRequest(device_t device);

/*! \brief Handle HFP error

    Some error occurred in the HFP state machine, to avoid the state machine
    getting stuck, drop connection and move to 'disconnected' state.
*/
void HfpProfile_HandleError(hfpInstanceTaskData * instance, MessageId id, Message message);

/*! \brief Check SCO encryption

    This functions is called to check if SCO is encrypted or not.  If there is
    a SCO link active, a call is in progress and the link becomes unencrypted,
    send a Telephony message that could be used to provide an indication tone
    to the user, depenedent on UI configuration.
*/
void HfpProfile_CheckEncryptedSco(hfpInstanceTaskData * instance);

#endif /* HFP_PROFILE_PRIVATE_H_ */

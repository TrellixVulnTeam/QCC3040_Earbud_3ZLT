/*!
\copyright  Copyright (c) 2017 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       dfu.h
\brief      Header file for the device firmware upgrade management.
*/

#ifndef DFU_DOMAIN_H_
#define DFU_DOMAIN_H_

#ifdef INCLUDE_DFU

#include <upgrade.h>
#include <task_list.h>


#include <domain_message.h>


#ifdef INCLUDE_DFU_PEER
#include "dfu_peer_sig_typedef.h"
#include "dfu_peer_sig_marshal_typedef.h"
#endif

/*! Defines the upgrade client task list initial capacity */
#define THE_DFU_CLIENT_LIST_INIT_CAPACITY 1

#define ZERO_DURATION 0

/*! Messages that are sent by the dfu module */
typedef enum {
    /*! Message sent after the device has restarted. This indicates that an
        upgrade has nearly completed and upgrade mode is needed to allow the
        upgrade to be confirmed */
    DFU_REQUESTED_TO_CONFIRM = DFU_MESSAGE_BASE,
    DFU_REQUESTED_IN_PROGRESS,      /*!< Message sent after the device has restarted. This
                                             indicates that an upgrade is in progress
                                             and has been interrupted */
    DFU_ACTIVITY,                   /*!< The DFU module has seen some DFU activity */
    DFU_STARTED,                    /*!< An DFU is now in progress. Started or continued. */
    DFU_COMPLETED,                  /*!< An DFU has been completed */
    DFU_CLEANUP_ON_ABORT,           /*!<An DFU is aborted, clean up DFU specific entities */
    DFU_ABORTED,                    /*!< An upgrade has been aborted.
                                             either owing to device
                                             initiated error OR device
                                             initiated error in Handover
                                             scenario OR Host inititated
                                             abort */
    DFU_PRE_START,                  /*!< A DFU is about to start, in the pre
                                             start phase, the alternate DFU bank
                                             may be erased. So early notify the
                                             app to cancel DFU timers to avoid
                                             false DFU timeouts. */
    DFU_READY_FOR_SILENT_COMMIT,    /*!< The DFU is ready for the silent commit. */
    DFU_READY_TO_REBOOT,            /*!< DFU file transfer complete, ready to
                                             reboot into new image. App should
                                             perform any required shutdown
                                             actions, then call Dfu_RebootConfirm().
                                             This message is only sent if
                                             Dfu_RequireRebootPermission(TRUE)
                                             has been called. */

    DFU_MESSAGE_END                 /*! This must be the final message */
} dfu_messages_t;




typedef enum
{
    REBOOT_REASON_NONE,
    REBOOT_REASON_DFU_RESET,
    REBOOT_REASON_ABRUPT_RESET,
    REBOOT_REASON_REVERT_RESET
} dfu_reboot_reason_t;

/* Enum to determine whether to start hash checking on primary device based on
 * peer data transfer completion
 */
typedef enum
{
    DFU_PEER_DATA_TRANSFER_COMPLETED,
    DFU_PEER_DATA_TRANSFER_STARTED,
    DFU_PEER_DATA_TRANSFER_NOT_STARTED
}peer_data_transfer_status;

/*Structure holding upgrade version and config version info w.r.t. different apps
like earbud, headset etc.*/
typedef struct
{
    upgrade_version upgrade_ver;
    uint16 config_ver;
} dfu_VersionInfo;

/*! Structure holding data for the DFU module */
typedef struct
{
        /*! Task for handling messaging from upgrade library */
    TaskData        dfu_task;
#ifdef INCLUDE_DFU_PEER
    bool            is_dfu_aborted_on_handover;
#endif
    /*! Flag to allow a specific DFU mode, entered when entering the case.
        This flag is set when using the UI to request DFU. The device will
        need to be placed into the case (attached to a charger) before in-case
        DFU will be allowed */
    bool enter_dfu_mode:1;

    /*! Flag to indicate the application has requested notification of reboots,
        and they should not go ahead until the application confirms. */
    bool reboot_permission_required:1;

    /*!< Set to REBOOT_REASON_DFU_RESET for reboot phase of 
         upgrade i.e. when upgrade library sends APP_UPGRADE_REQUESTED_TO_CONFIRM
         and sets to REBOOT_REASON_ABRUPT_RESET for abrupt reset 
         i.e. when upgrade library sends APP_UPGRADE_REQUESTED_IN_PROGRESS. */
    dfu_reboot_reason_t dfu_reboot_reason;

#ifdef INCLUDE_DFU_PEER
    /*
     * Since this variable control conditional message triggers, reverse value
     * is used.
     * i.e. 1: Erase not done and 
     *      0: Erase done
     */
    uint16          peerEraseDone;

    uint32          peerProfilesToConnect;

    peer_data_transfer_status          peerDataTransferStatus;
#endif

        /*! List of tasks to notify of UPGRADE activity. */
    TASK_LIST_WITH_INITIAL_CAPACITY(THE_DFU_CLIENT_LIST_INIT_CAPACITY) client_list;

    dfu_VersionInfo verInfo;

    /* Variable to store the qos setting operation info */
    bool is_qos_release_needed_post_dfu;
} dfu_task_data_t;

/*!< Task information for DFU support */
extern dfu_task_data_t app_dfu;

/*! Get the info for the applications dfu support */
#define Dfu_GetTaskData()     (&app_dfu)

/*! Get the Task info for the applications dfu task */
#define Dfu_GetTask()         (&app_dfu.dfu_task)

/*! Get the client list for the applications DFU task */
#define Dfu_GetClientList()         (task_list_flexible_t *)(&app_dfu.client_list)

/*
 * ToDo: These DFU states such as "DFU aborted on Handover"
 *       can be better managed as an enum rather than as exclusive boolean types
 */
#define Dfu_SetDfuAbortOnHandoverState(x) (app_dfu.is_dfu_aborted_on_handover = (x))
#define Dfu_IsDfuAbortOnHandoverDone()    (app_dfu.is_dfu_aborted_on_handover)

bool Dfu_EarlyInit(Task init_task);

bool Dfu_Init(Task init_task);


/*! \brief Allow upgrades to be started

    The library used for firmware upgrades will always allow connections.
    However, it is possible to stop upgrades from beginning or completing.

    \param allow    allow or disallow upgrades

    \return TRUE if the request has taken effect. This setting can not be
        changed if an upgrade is in progress in which case the
        function will return FALSE.
 */
extern bool Dfu_AllowUpgrades(bool allow);

/*! \brief Turn on or off permission requests for DFU reboots

    By default the DFU domain will reboot the device automatically as required
    during the upgrade process. This behaviour can be overridden if the
    application has actions it needs to perform before a device shutdown.

    If permission_required is set to TRUE, then a DFU_READY_TO_REBOOT message
    will be sent to all registered clients instead of rebooting into a new image
    directly. Otherwise, the message is not sent.

    On recipt of DFU_READY_TO_REBOOT, the application must respond by calling
    Dfu_RebootConfirm() once it is ready for the reboot to go ahead, otherwise
    the upgrade will not complete successfully. This is not required if
    permission_required is set to FALSE (the default).

    \note Changes to reboot permissions will not take effect until the next time
          Dfu_AllowUpgrades() is called.

    \param permission_required  Whether the application should be consulted
                                before rebooting into a new image.
    \return None
 */
void Dfu_RequireRebootPermission(bool permission_required);

/*! \brief Notify the DFU domain that a reboot can go ahead now

    This function must be called on reciept of a DFU_READY_TO_REBOOT message,
    and will cause the Upgrade library to reboot into a new image.

    \param None
    \return None
*/
void Dfu_RebootConfirm(void);


/*! \brief Handler for system messages. All of which are sent to the application.

    This function is called to handle any system messages that this module
    is interested in. If a message is processed then the function returns TRUE.

    \param  id              Identifier of the system message
    \param  message         The message content (if any)
    \param  already_handled Indication whether this message has been processed by
                            another module. The handler may choose to ignore certain
                            messages if they have already been handled.

    \returns TRUE if the message has been processed, otherwise returns the
         value in already_handled
 */
bool Dfu_HandleSystemMessages(MessageId id, Message message, bool already_handled);


/*! \brief Add a client to the UPGRADE module

    Messages from #av_headet_upgrade_messages will be sent to any task
    registered through this API

    \param task Task to register as a client
 */
void Dfu_ClientRegister(Task task);

/*! \brief Set the context of the UPGRADE module

    The value is stored in the UPGRADE PsKey and hence is non-volatile

    \param dfu_context_t Upgrade context to set
 */
void Dfu_SetContext(upgrade_context_t context);

/*! \brief Get the context of the UPGRADE module

    The value is stored in the UPGRADE PsKey and hence is non-volatile

    \returns The non-volatile context of the UPGRADE module from the UPGRADE PsKey

 */
upgrade_context_t Dfu_GetContext(void);

/*! \brief Gets the reboot reason

    \param None

    \returns REBOOT_REASON_DFU_RESET for defined reboot phase of upgrade
            else REBOOT_REASON_ABRUPT_RESET for abrupt reset.

 */
dfu_reboot_reason_t Dfu_GetRebootReason(void);

/*! \brief Sets the reboot reason

    \param dfu_reboot_reason_t Reboot reason

 */
void Dfu_SetRebootReason(dfu_reboot_reason_t val);

/*! \brief Clears upgrade related PSKeys

    \param None

 \return TRUE if upgrade PSKEYs are cleared, FALSE otherwise */
bool Dfu_ClearPsStore(void);

/*!
    Get the handset profile mask required for DFU.

    Returns the handset profile mask required for DFU operation.

    \param None
    \return Handset profile mask required for DFU operation
*/
uint32 Dfu_GetDFUHandsetProfiles(void);

/*!
    \brief Device is not in use currently so, proceed with the pending silent commit if any.

    \param None
    \return None
*/
void Dfu_HandleDeviceNotInUse(void);

/*!
    \brief Find out if silent commit is enabled.

    \return TRUE if silent commit is set
*/
bool Dfu_IsSilentCommitEnabled(void);

/*!
    Get the progress of upgrade.
	
    Returns TRUE if upgrade is in progress else FALSE.
*/
bool Dfu_IsUpgradeInProgress(void);

void Dfu_SetVersionInfo(uint16 uv_major, uint16 uv_minor, uint16 cfg_ver);

/*!
    \brief Set if silent commit is supported

    \param is_silent_commit_supported Used to assign required value (0 or 1)
    \return None
*/
void Dfu_SetSilentCommitSupported(uint8 is_silent_commit_supported);

/*
    \brief Request for Qos value during DFU over BLE connection.
           Set the QoS as low latency for better
           DFU performance over LE Transport.
           This will come at the cost of high power consumption.

    \param None

    \return void
*/
void Dfu_RequestQOS(void);

/*
    \brief Release Qos value after DFU is over BLE connection.
           Set the QoS as low power after DFU transfer
           is over during LE Transport.

    \param None

    \return void
*/
void Dfu_ReleaseQOS(void);


#ifdef INCLUDE_DFU_PEER
/*!
    \brief Set the peerDataTransferStatus as per the peer upgrade status

    \param peer_data_transfer_status Used to assign required enum values
    \return None
*/
void Dfu_SetPeerDataTransferStatus(peer_data_transfer_status status);

/*
    \brief The default behaviour of Upgrade libarary is to send Host command
           response using UpgradeHostIFData*** APIs. But when device is
           operating as secondry device i.e. connection is over L2CAp for dfu
           download then upgrade response is sent via UpgradePeerIfData** APIs.

    \param is_primary To get the device role information

    \return void
*/
void Dfu_UpgradeHostRspSwap(bool is_primary);
#endif /* INCLUDE_DFU_PEER */

#else
#define Dfu_RequireRebootPermission(perm) ((void)0)
#define Dfu_RebootConfirm() ((void)0)
#define Dfu_EnteredDfuMode() ((void)(0))
#define Dfu_HandleSystemMessages(_id, _msg, _handled) (_handled)
#define Dfu_ClientRegister(tsk) ((void)0)
#define Dfu_SetContext(ctx) ((void)0)
#define Dfu_GetContext() (UPGRADE_CONTEXT_UNUSED)
#define Dfu_GetRebootReason() (REBOOT_REASON_NONE)
#define Dfu_SetRebootReason(val) ((void)0)
#define Dfu_ClearPsStore() (FALSE)
#define Dfu_GetDFUHandsetProfiles() (0)
#define Dfu_HandleDeviceNotInUse() ((void)0)
#define Dfu_IsSilentCommitEnabled() (FALSE)
#define Dfu_IsUpgradeInProgress() (FALSE)
#define Dfu_SetVersionInfo(_uv_major, _uv_minor, _cfg_ver) ((void)0)
#define Dfu_SetSilentCommitSupported(_is_silent_commit_supported) ((void)0)
#define Dfu_RequestQOS(void) ((void)0)
#define Dfu_ReleaseQOS(void) ((void)0)
#define Dfu_SetPeerDataTransferStatus(status) ((void)0)
#define Dfu_UpgradeHostRspSwap(_is_primary) ((void)0)
#endif /* INCLUDE_DFU */

#endif /* DFU_DOMAIN_H_ */

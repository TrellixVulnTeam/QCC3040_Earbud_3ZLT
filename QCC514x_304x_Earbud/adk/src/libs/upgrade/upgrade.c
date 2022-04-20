/****************************************************************************
Copyright (c) 2014 - 2015, 2020 Qualcomm Technologies International, Ltd.


FILE NAME
    upgrade.c

DESCRIPTION
    Upgrade library API implementation.
*/

#define DEBUG_LOG_MODULE_NAME upgrade
#include <logging.h>
DEBUG_LOG_DEFINE_LEVEL_VAR

#include <string.h>
#include <stdlib.h>
#include <boot.h>
#include <message.h>
#include <byte_utils.h>
#include <panic.h>

#include "upgrade_ctx.h"
#include "upgrade_private.h"
#include "upgrade_sm.h"
#include "upgrade_host_if.h"
#include "upgrade_psstore.h"
#include "upgrade_partitions.h"
#include "upgrade_msg_vm.h"
#include "upgrade_msg_internal.h"
#include "upgrade_host_if_data.h"

#include "upgrade_partition_data.h"

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_TYPE(upgrade_application_message)
LOGGING_PRESERVE_MESSAGE_TYPE(upgrade_transport_message)
LOGGING_PRESERVE_MESSAGE_TYPE(UpgradeMsgHost)
LOGGING_PRESERVE_MESSAGE_TYPE(UpgradeMsgInternal)


static void SMHandler(Task task, MessageId id, Message message);
static void SendUpgradeInitCfm(Task task, upgrade_status_t status);
static void RequestApplicationReconnectIfNeeded(void);
static bool isPsKeyStartValid(uint16 dataPskeyStart);
static void UpgradeHandleCommitRevert(uint16 dataPskey, uint16 dataPskeyStart);

const upgrade_response_functions_t Upgrade_fptr = {
    .SendSyncCfm = UpgradeHostIFDataSendSyncCfm,
    .SendShortMsg = UpgradeHostIFDataSendShortMsg,
    .SendStartCfm = UpgradeHostIFDataSendStartCfm,
    .SendBytesReq = UpgradeHostIFDataSendBytesReq,
    .SendErrorInd = UpgradeHostIFDataSendErrorInd,
    .SendIsCsrValidDoneCfm = UpgradeHostIFDataSendIsCsrValidDoneCfm,
    .SendVersionCfm = UpgradeHostIFDataSendVersionCfm,
    .SendVariantCfm = UpgradeHostIFDataSendVariantCfm,
    .SendSilentCommitSupportedCfm = UpgradeHostIFDataSendSilentCommitSupportedCfm
};

/****************************************************************************
NAME
    UpgradeGetFPtr

DESCRIPTION
    To get the Upgrade librabry fptr to be set in UpgradeCtxGet()->funcs.
*/
const upgrade_response_functions_t *UpgradeGetFPtr(void)
{
    return &Upgrade_fptr;
}

/****************************************************************************
NAME
    UpgradeInit

DESCRIPTION
    Perform initialisation for the upgrade library. This consists of fixed
    initialisation as well as taking account of the information provided
    by the application.
*/

void UpgradeInit(Task appTask,uint16 dataPskey,uint16 dataPskeyStart,
    const UPGRADE_UPGRADABLE_PARTITION_T * logicalPartitions,
    uint16 numPartitions,
    upgrade_power_management_t power_mode,
    const char * dev_variant,
    upgrade_permission_t init_perm,
    const upgrade_version *init_version,
    uint16 init_config_version)
{
    UpgradeCtx *upgradeCtx;

    DEBUG_LOG("UpgradeInit");

    upgradeCtx = PanicUnlessMalloc(sizeof(*upgradeCtx));
    memset(upgradeCtx, 0, sizeof(*upgradeCtx));
    upgradeCtx->mainTask = appTask;
    upgradeCtx->smTaskData.handler = SMHandler;

    UpgradeCtxSet(upgradeCtx);

    /* handle permission initialisation, must be an "enabled" state */
    if (   (init_perm != upgrade_perm_assume_yes)
        && (init_perm != upgrade_perm_always_ask))
    {
        Panic();
    }
    UpgradeCtxGet()->perms = init_perm;

    /* Set functions for Upgrade */
    UpgradeCtxGet()->funcs = &Upgrade_fptr;

    /* set the initial power management mode */
    UpgradeCtxGet()->power_mode = power_mode;

    /* set the initial state to battery ok, expecting sink powermanagement to soon update the state */
    UpgradeCtxGet()->power_state = upgrade_battery_ok;

    UpgradeCtxGet()->waitForPeerAbort = FALSE;

    /* Image upgarde copy gets done after the data transfer and validation. */
    UpgradeCtxSetImageCopyStatus(IMAGE_UPGRADE_COPY_NOT_STARTED);
    upgradeCtx->imageUpgradeHashStatus = IMAGE_UPGRADE_HASH_NOT_STARTED;

    /* store the device variant */
    if(dev_variant != NULL)
    {
        strncpy(UpgradeCtxGet()->dev_variant, dev_variant, UPGRADE_HOST_VARIANT_CFM_BYTE_SIZE );
    }

    if (!isPsKeyStartValid(dataPskeyStart)
        || !UpgradePartitionsSetMappingTable(logicalPartitions,numPartitions))
    {
        SendUpgradeInitCfm(appTask, upgrade_status_unexpected_error);
        free(upgradeCtx);
        UpgradeCtxSet(NULL);
        return;
    }

    /* By default, we do not need to reboot on abort because we are running from boot bank only.
       In case if we abort at commit time(after warm reboot), we need to reboot and revert back
       to boot bank. at that time, this flag will be set to TRUE */
    UpgradeCtxGet()->isImageRevertNeededOnAbort = FALSE;

    UpgradeHandleCommitRevert(dataPskey, dataPskeyStart);
    UpgradeLoadPSStore(dataPskey,dataPskeyStart);

    /* @todo Need to deal with two things here
     * Being called when the PSKEY has already been set-up
     * being called for the first time. should we/can we verify partition
     * mapping
     */
    DEBUG_LOG_VERBOSE("UpgradeInit : upgrade_version major = %d, upgrade_version minor = %d and init config version = %d", init_version->major, init_version->minor, init_config_version);
    /* Initial version setting */
    if (UpgradeCtxGetPSKeys()->version.major == 0
        && UpgradeCtxGetPSKeys()->version.minor == 0)
    {
        UpgradeCtxGetPSKeys()->version = *init_version;
    }

    if (UpgradeCtxGetPSKeys()->config_version == 0)
    {
        UpgradeCtxGetPSKeys()->config_version = init_config_version;
    }

    /* Make this call before initialising the state machine so that the
       SM cannot cause the initial state to change */
    RequestApplicationReconnectIfNeeded();

    /* initialise the state machine and pass in the event that enables upgrades
     * @todo this UPGRADE_VM_PERMIT_UPGRADE event can be removed if we're always
     * starting in an upgrade enabled state, just need to initialise the state
     * machine in the correct state. */
    UpgradeSMInit();
    UpgradeSMHandleMsg(UPGRADE_VM_PERMIT_UPGRADE, 0);
    UpgradeHostIFClientConnect(&upgradeCtx->smTaskData);

    SendUpgradeInitCfm(appTask, upgrade_status_success);
}

void UpgradeSetPartitionDataBlockSize(uint32 size)
{
    UpgradeCtxGet()->partitionDataBlockSize = size;
}

/****************************************************************************
NAME
    UpgradePowerManagementSetState

DESCRIPTION
    Receives the current state of the power management from the Sink App

RETURNS

*/
upgrade_status_t UpgradePowerManagementSetState(upgrade_power_state_t state)
{
    DEBUG_LOG("UpgradePowerManagementSetState, state %u", state);

    /* if initially the power management was set to disabled, don't accept any change */
    /* we need to make sure this is called AFTER UpgradeInit is called */
    if(UpgradeCtxGet()->power_mode == upgrade_power_management_disabled)
    {
        return upgrade_status_invalid_power_state;
    }

    UpgradeCtxGet()->power_state = state;

    if(UpgradeCtxGet()->power_state == upgrade_battery_low)
    {
        MessageSend(UpgradeGetUpgradeTask(), UPGRADE_INTERNAL_BATTERY_LOW, NULL);
    }

    return upgrade_status_success;
}

/****************************************************************************
NAME
    UpgradeGetPartitionInUse

DESCRIPTION
    Find out current physical partition for a logical partition.

RETURNS
    uint16 representing the partition that is active.
    UPGRADE_PARTITION_NONE_MAPPED is returned for an invalid partition.
*/
uint16 UpgradeGetPartitionInUse(uint16 logicalPartition)
{
    return (uint16)UpgradePartitionsPhysicalPartition(logicalPartition,UpgradePartitionActive);
}

/****************************************************************************
NAME
    UpgradeGetAppTask

DESCRIPTION
    Returns the VM application task registered with the library at
    initialisation in #UpgradeInit

RETURNS
    Task VM application task
*/
Task UpgradeGetAppTask(void)
{
    return UpgradeCtxGet()->mainTask;
}

/****************************************************************************
NAME
    UpgradeGetUpgradeTask

DESCRIPTION
    Returns the upgrade library main task.

RETURNS
    Task Upgrade library task.
*/
Task UpgradeGetUpgradeTask(void)
{
    return &UpgradeCtxGet()->smTaskData;
}

/****************************************************************************
NAME
    UpgradeHandleMsg

DESCRIPTION
    Main message handler for messages to the upgrade library from VM
    applications.
*/
void UpgradeHandleMsg(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UpgradeSMHandleMsg(id, message);
}

/****************************************************************************
NAME
    UpgradePermit

DESCRIPTION
    Control the permission the upgrade has for upgrade operations.

RETURNS
    upgrade_status_t Success or failure of requested permission type.
*/
upgrade_status_t UpgradePermit(upgrade_permission_t perm)
{
    DEBUG_LOG("UpgradePermit, perm %u", perm);
    switch (perm)
    {
        case upgrade_perm_no:
            /* if we already have an upgrade in progress, return an
             * error and do not modify our permissions */
            if (UpgradeSMUpgradeInProgress())
            {
                return upgrade_status_in_progress;
            }
            break;

        case upgrade_perm_assume_yes:
            /* fall-thru - both cases are permitting an upgrade */
        case upgrade_perm_always_ask:
            UpgradeSMHandleMsg(UPGRADE_VM_PERMIT_UPGRADE, 0);
            break;

        default:
            return upgrade_status_unexpected_error;
    }

    /* remember the permission setting */
    UpgradeCtxGet()->perms = perm;

    return upgrade_status_success;
}

/****************************************************************************
NAME
    UpgradeTransportConnectRequest

DESCRIPTION
    When a client wants to initiate an upgrade, the transport
    must first connect to the upgrade library so that it knows
    which Task to use to send messages to a client.

    The Upgrade library will respond by sending
    UPGRADE_TRANSPORT_CONNECT_CFM to transportTask.

*/
void UpgradeTransportConnectRequest(Task transportTask, upgrade_data_cfm_type_t cfm_type, uint32 max_request_size)
{
    DEBUG_LOG("UpgradeTransportConnectRequest, transportTask 0x%p, cfm_type %u, max_request_size %lu",
        (void *)transportTask, cfm_type, max_request_size);
    UpgradeHostIFTransportConnect(transportTask, cfm_type, max_request_size);
}

/****************************************************************************
NAME
    UpgradeProcessDataRequest

DESCRIPTION
    All data packets from a client should be sent to the Upgrade library
    via this function. Data packets must be in order but do not need
    to contain a whole upgrade message.

    The Upgrade library will respond by sending
    UPGRADE_TRANSPORT_DATA_CFM to the Task set in
    UpgradeTransportConnectRequest().

*/
void UpgradeProcessDataRequest(uint16 size_data, uint8 *data)
{
    DEBUG_LOG("UpgradeProcessDataRequest, size_data %u", size_data);
    (void)UpgradeHostIFProcessDataRequest(data, size_data);
}

/****************************************************************************
NAME
    UpgradeFlowControlProcessDataRequest

DESCRIPTION
    Similar to UpgradeProcessDataRequest but an appropriate wrapper to be used
    as public API.
*/

bool UpgradeFlowControlProcessDataRequest(uint8 *data, uint16 size_data)
{
    DEBUG_LOG("UpgradeFlowControlProcessDataRequest, size_data %u", size_data);
    return UpgradeHostIFProcessDataRequest(data, size_data);
}

/****************************************************************************
NAME
    UpgradeTransportDisconnectRequest

DESCRIPTION
    When a transport no longer needs to use the Upgrade
    library it must disconnect.

    The Upgrade library will respond by sending
    UPGRADE_TRANSPORT_DISCONNECT_CFM to the Task set in
    UpgradeTransportConnectRequest().

*/
void UpgradeTransportDisconnectRequest(void)
{
    DEBUG_LOG("UpgradeTransportDisconnectRequest");
    UpgradeHostIFTransportDisconnect();
}

/****************************************************************************
NAME
    UpgradeTransportInUse

DESCRIPTION
    Indicates whether the upgrade library currently has a transport connected.

*/
bool UpgradeTransportInUse(void)
{
    bool inUse = UpgradeHostIFTransportInUse();
    DEBUG_LOG("UpgradeTransportInUse, in_use %u", inUse);
    return inUse;
}

void SMHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UpgradeSMHandleMsg(id, message);
}

/****************************************************************************
NAME
    UpgradeDfuStatus

DESCRIPTION
    Inform the Upgrade library of the result of an attempt to upgrade
    internal flash using DFU file from a serial flash partition.

RETURNS
    n/a
*/
void UpgradeDfuStatus(MessageDFUFromSQifStatus *message)
{
    DEBUG_LOG("UpgradeDfuStatus, status %u", message->status);

    /* TODO: Make sure that this structure already exists */
    switch(message->status)
    {
    case DFU_SQIF_STATUS_SUCCESS:
        {
            UpgradeCtxGetPSKeys()->loader_msg = UPGRADE_LOADER_MSG_SUCCESS;

            /* If there are one or more data partitions to update,
               we need to (re)calculate new FSTAB and warm reboot.
               (before reconnecting with the host.) */
            if (UpgradeSetToTryUpgrades())
            {
                UpgradeSavePSKeys();
                BootSetMode(BootGetMode());
            }
        }
        break;

    case DFU_SQIF_STATUS_ERROR:
        UpgradeCtxGetPSKeys()->loader_msg = UPGRADE_LOADER_MSG_ERROR;
        break;
    }

    UpgradeSavePSKeys();
}

/****************************************************************************
NAME
    UpgradeEraseStatus

DESCRIPTION
    Inform the Upgrade library of the result of an attempt to erase SQIF.

RETURNS
    n/a
*/
void UpgradeEraseStatus(Message message)
{
    UpgradeSMEraseStatus(message);
}

/****************************************************************************
NAME
    UpgradeCopyStatus

DESCRIPTION
    Inform the Upgrade library of the result of an attempt to copy SQIF.

RETURNS
    n/a
*/
void UpgradeCopyStatus(Message message)
{
    UpgradeSMCopyStatus(message);
}

/****************************************************************************
NAME
    UpgradeCopyAudioStatus

DESCRIPTION
    Inform the Upgrade library of the result of an attempt to copy the Audio SQIF.

RETURNS
    n/a
*/
void UpgradeCopyAudioStatus(Message message)
{
    DEBUG_LOG_DEBUG("UpgradeCopyAudioStatus(%p)", message);
#ifdef MESSAGE_IMAGE_UPGRADE_AUDIO_STATUS
    UpgradeSMCopyAudioStatus(message);
#endif
}

/****************************************************************************
NAME
    UpgradeHashAllSectionsUpdateStatus

DESCRIPTION
    Inform the Upgrade library of the result an attempt to calculate teh hash over all sections.

RETURNS
    n/a
*/
void UpgradeHashAllSectionsUpdateStatus(Message message)
{
    DEBUG_LOG_DEBUG("UpgradeHashAllSectionsUpdateStatus(%p)\n", message);
#ifdef MESSAGE_IMAGE_UPGRADE_HASH_ALL_SECTIONS_UPDATE_STATUS
    UpgradeSMHashAllSectionsUpdateStatus(message);
#endif
}

/****************************************************************************
NAME
    UpgradeApplyResponse

DESCRIPTION
    Handle application decision on applying (reboot) an upgrade.

    If the application wishes to postpone the reboot, resend the message to
    the application after the requested delay. Otherwise, push a reboot
    event into the state machine.

    @todo do we want protection against these being called by a bad application
     at the wrong time? The state machine *should* cover this.

RETURNS
    n/a
*/
void UpgradeApplyResponse(uint32 postpone)
{
    if (!postpone)
    {
        MessageSend(UpgradeGetUpgradeTask(), UPGRADE_INTERNAL_REBOOT, NULL);
    }
    else
    {
        MessageSendLater(UpgradeCtxGet()->mainTask, UPGRADE_APPLY_IND, 0, postpone);
    }
}

/****************************************************************************
NAME
    UpgradeApplyAudioShutDown

DESCRIPTION
    Sends a message to sink upgrade's main handler in order to call into sink
    audio and shut down any voice or audio streams

RETURNS
    n/a
*/
void UpgradeApplyAudioShutDown(void)
{
    MessageSend(UpgradeCtxGet()->mainTask, UPGRADE_SHUT_AUDIO, NULL);
}

/****************************************************************************
NAME
    UpgradeApplyAudioCopyFailed

DESCRIPTION
    Sends a message to sink upgrade's main handler in order to clear the audio
    busy flag should the copy of the audio image fail

RETURNS
    n/a
*/
void UpgradeApplyAudioCopyFailed(void)
{
    MessageSend(UpgradeCtxGet()->mainTask, UPGRADE_AUDIO_COPY_FAILURE, NULL);
}

/****************************************************************************
NAME
    UpgradeCopyAudioImage

DESCRIPTION
    Calls into the main state machine to invoke the trap call for the audio
    image copy

RETURNS
    n/a
*/
void UpgradeCopyAudioImage(void)
{
    UpgradeSMHandleAudioDFU();
}

/****************************************************************************
NAME
    UpgradeBlockingResponse

DESCRIPTION
    Handle application decision on blocking the system (erase).

    If the application wishes to postpone the blocking erase, resend the
    message to the application after the requested delay. Otherwise, push an
    erase event into the state machine.

    @todo do we want protection against these being called by a bad application
     at the wrong time? The state machine *should* cover this.

RETURNS
    n/a
*/
void UpgradeBlockingResponse(uint32 postpone)
{
    if (!postpone)
    {
        MessageSend(UpgradeGetUpgradeTask(), UPGRADE_INTERNAL_ERASE, NULL);
    }
    else
    {
        MessageSendLater(UpgradeCtxGet()->mainTask, UPGRADE_BLOCKING_IND, 0, postpone);
    }
}

/****************************************************************************
NAME
    UpgradeRunningNewApplication

DESCRIPTION
    Query the upgrade library to see if we are part way through an upgrade.

    This is used by the application during early boot to check if the
    running application is the upgrade one but it hasn't been committed yet.

    Note: This should only to be called during the early init phase, before
          UpgradeInit has been called.

RETURNS
    TRUE if the upgraded application is running but hasn't been
    committed yet. FALSE otherwise, or in the case of an error.
*/
bool UpgradeRunningNewApplication(uint16 dataPskey, uint16 dataPskeyStart)
{
    if (UpgradeIsInitialised() || !isPsKeyStartValid(dataPskeyStart))
        return FALSE;

    if (UpgradePsRunningNewApplication(dataPskey, dataPskeyStart))
    {
        return TRUE;
    }

    return FALSE;
}

/************************************************************************************
NAME
    UpgradeSendStartUpgradeDataInd

DESCRIPTION
    To inform vm app that downloading of upgrade data from host app has begun.

RETURNS
    None
*/

void UpgradeSendStartUpgradeDataInd(void)
{
    DEBUG_LOG_DEBUG("UpgradeSendStartUpgradeDataInd");
#ifndef HOSTED_TEST_ENVIRONMENT
    MessageSendConditionally(UpgradeCtxGet()->mainTask,
                            UPGRADE_START_DATA_IND, NULL,
                            (uint16 *)&UpgradeCtxGet()->isImgUpgradeEraseDone);
#else
    MessageSend(UpgradeCtxGet()->mainTask, UPGRADE_START_DATA_IND, NULL);
#endif
}

/************************************************************************************
NAME
    UpgradeSendEndUpgradeDataInd

DESCRIPTION
    To inform vm app that downloading of upgrade data from host app has ended.

RETURNS
    None
*/

void UpgradeSendEndUpgradeDataInd(upgrade_end_state_t state, uint32 message_delay)
{
    UPGRADE_END_DATA_IND_T *upgradeEndDataInd = (UPGRADE_END_DATA_IND_T *)PanicUnlessMalloc(sizeof(UPGRADE_END_DATA_IND_T));

    upgradeEndDataInd->state = state;

    DEBUG_LOG_DEBUG("UpgradeSendEndUpgradeDataInd: state enum:upgrade_end_state_t:%d, message_delay:%d", state, message_delay);

    if(message_delay)
        MessageSendLater(UpgradeCtxGet()->mainTask, UPGRADE_END_DATA_IND, upgradeEndDataInd, message_delay);
    else
        MessageSend(UpgradeCtxGet()->mainTask, UPGRADE_END_DATA_IND, upgradeEndDataInd);
}

/************************************************************************************
NAME
    UpgradeSendReadyForSilentCommitInd

DESCRIPTION
    To inform vm app that silent commit command has been received from the host.

RETURNS
    None
*/

void UpgradeSendReadyForSilentCommitInd(void)
{
    DEBUG_LOG_DEBUG("UpgradeSendReadyForSilentCommitInd");
    MessageSend(UpgradeCtxGet()->mainTask, UPGRADE_READY_FOR_SILENT_COMMIT, NULL);
}

/****************************************************************************
NAME
    SendUpgradeInitCfm

DESCRIPTION
    Build and send an UPGRADE_INIT_CFM message and send to the specified task.

RETURNS
    n/a
*/
static void SendUpgradeInitCfm(Task task, upgrade_status_t status)
{
    MESSAGE_MAKE(upgradeInitCfm, UPGRADE_INIT_CFM_T);
    upgradeInitCfm->status = status;
    MessageSend(task, UPGRADE_INIT_CFM, upgradeInitCfm);
}

/****************************************************************************
NAME
    RequestApplicationReconnectIfNeeded

DESCRIPTION
    Check the upgrade status and decide if the application needs to consider
    restarting communication / UI so that it can connect to a host.

    If needed, builds and send an UPGRADE_RESTARTED_IND_T message and sends to
    the application task.

NOTE
    Considered implementing this as part of UpgradeSMInit() which also looks at
    the resume point information, but it is not really related to the SM.
*/
static void RequestApplicationReconnectIfNeeded(void)
{
    upgrade_reconnect_recommendation_t reconnect = upgrade_reconnect_not_required;

    DEBUG_LOG_INFO("RequestApplicationReconnectIfNeeded(): upgrade_in_progress_key %d, dfu_partition_num %d",
                   UpgradeCtxGetPSKeys()->upgrade_in_progress_key,
                   UpgradeCtxGetPSKeys()->dfu_partition_num);

    switch(UpgradeCtxGetPSKeys()->upgrade_in_progress_key)
    {
        /* Resume from the beginning, includes download phase. */
    case UPGRADE_RESUME_POINT_START:
    case UPGRADE_RESUME_POINT_ERROR:
        {
            if (UpgradeCtxGetPSKeys()->id_in_progress)
            {
                /* Not in a critical operation, but there is an upgrade in progress,
                   either on this device or on peer device (primary device)
                   (in the case of _ERROR) presumably was an upgrade in progress.
                   So the application may want to restart operations to allow it to
                   resume */
                reconnect = upgrade_reconnect_recommended_in_progress;
            }
        }
        break;

    case UPGRADE_RESUME_POINT_ERASE:
            /* Not in a critical operation, but there is an upgrade in progress.
               Separated from the two cases above as there is a lesser argument
               for the reconnect - so may change in future. */
        reconnect = upgrade_reconnect_recommended_in_progress;
        break;

    case UPGRADE_RESUME_POINT_PRE_VALIDATE:
    case UPGRADE_RESUME_POINT_PRE_REBOOT:
    {
        /* There is an upgrade in progress so, the application should
         * restart operations to allow it to resume.
         */
        reconnect = upgrade_reconnect_recommended_in_progress;
        break;
     }

    case UPGRADE_RESUME_POINT_POST_REBOOT:
    case UPGRADE_RESUME_POINT_COMMIT:
        if (UpgradeCtxGetPSKeys()->dfu_partition_num == 0)
        {
            /* We are in the middle of an upgrade that requires the host/app to
               confirm its success. */
            reconnect = upgrade_reconnect_required_for_confirm;
        }
        else
        {
            /* There is a DFU to be finished off. No host interaction is
               needed but won't hurt. */
            reconnect = upgrade_reconnect_recommended_as_completed;
        }
        break;
    }

    if (reconnect != upgrade_reconnect_not_required)
    {
        UPGRADE_RESTARTED_IND_T *restarted = (UPGRADE_RESTARTED_IND_T*)
                                                PanicUnlessMalloc(sizeof(*restarted));
        restarted->reason = reconnect;
        UpgradeCtxGet()->reconnect_reason = reconnect;
        MessageSend(UpgradeCtxGet()->mainTask, UPGRADE_RESTARTED_IND, restarted);
    }

}

/****************************************************************************
NAME
    isPsKeyStartValid

DESCRIPTION
    Verify that the upgrade PS key start offset is within valid limits.

RETURNS
    TRUE if offset is ok, FALSE otherwise.
*/
static bool isPsKeyStartValid(uint16 dataPskeyStart)
{
    uint16 available_space = PSKEY_MAX_STORAGE_LENGTH - dataPskeyStart;

    if ((dataPskeyStart >= PSKEY_MAX_STORAGE_LENGTH)
        || (available_space < UPGRADE_PRIVATE_PSKEY_USAGE_LENGTH_WORDS))
        return FALSE;
    else
        return TRUE;
}

/****************************************************************************
NAME
    UpgradeHandleCommitRevert

DESCRIPTION
    Check to detect reverted commit or unexpected reset of device during post reboot 
    phase and clear the pskeys if detected.
*/
static void UpgradeHandleCommitRevert(uint16 dataPskey, uint16 dataPskeyStart)
{
    /* ImageUpgradeSwapTryStatus will return false if we are running from 
     * the boot bank and true if we are running from the alternate bank */
    bool result = ImageUpgradeSwapTryStatus();
    uint16 resumePoint = UpgradePsGetResumePoint(dataPskey, dataPskeyStart);
    DEBUG_LOG_INFO("UpgradeHandleCommitRevert ImageUpgradeSwapTryStatus() returns %d and resume point is %d",result, resumePoint);

    /* If user resets the device in the post reboot phase or aborts at the commit 
     * screen then, device will reboot from the boot bank but still, the resume point
     * will be post reboot in the PSStore. In this case we need to abort the DFU. */
    if(!result && resumePoint == UPGRADE_RESUME_POINT_POST_REBOOT)
    {
        /* Clear the PsKeys */
        PsStore(dataPskey, 0, 0);
        UpgradeClearHeaderPSKeys();
        /* Inform DFU domain about reverting the upgrade for required actions*/
        MessageSend(UpgradeGetAppTask(), UPGRADE_REVERT_RESET, NULL);
    }
}

void UpgradeApplicationValidationStatus(bool pass)
{
    MESSAGE_MAKE(msg, UPGRADE_VM_EXE_FS_VALIDATION_STATUS_T);
    msg->result = pass;
    MessageSend(UpgradeGetUpgradeTask(), UPGRADE_VM_EXE_FS_VALIDATION_STATUS, msg);
}

bool UpgradeIsDataTransferMode(void)
{
    if(UpgradeSMGetState() == UPGRADE_STATE_DATA_TRANSFER)
        return TRUE;
    else
        return FALSE;
}


/****************************************************************************
NAME
    UpgradeImageSwap

DESCRIPTION
     This function will eventually call the ImageUpgradeSwapTry() trap to initiate a full chip reset,
      load and run images from the other image bank.

RETURNS
    None
*/
void UpgradeImageSwap(void)
{
    DEBUG_LOG("UpgradeImageSwap");
    UpgradeSMHandleValidated(UPGRADE_VM_DFU_COPY_VALIDATION_SUCCESS, NULL);
}

void UpgradeHandleAbortDuringUpgrade(void)
{
    if (UpgradeIsInProgress() && UpgradeIsAborting())
    {
        DEBUG_LOG("UpgradeHandleAbortDuringUpgrade: already aborting.");
    }
    else
    {
        if (UpgradeIsInProgress())
        {
            DEBUG_LOG("UpgradeHandleAbortDuringUpgrade: app not ready");
            UpgradeFatalError(UPGRADE_HOST_ERROR_APP_NOT_READY);
        }
        else
        {
            DEBUG_LOG("UpgradeHandleAbortDuringUpgrade: nothing to abort.");
        }
    }
}

/*!
    @brief Flow off or on processing of received upgrade data packets residing
           in Source Buffer.

    @note Scheme especially required for DFU over LE but currently commonly
          applied to DFU over LE or BR/EDR and when upgrade data is relayed from
          Primary to Secondary too.

    Returns None
*/
void UpgradeFlowOffProcessDataRequest(bool enable)
{
    /*
     * TODO: Can be skipped for DFU over BR/EDR transport and also when
     *       upgrade data is relayed from Primary to Secondary over BR/EDR.
     */
    UpgradeCtxGet()->dfu_rx_flow_off = enable;
}

/*!
    @brief Check if processing of received upgrade data packets residing
           in Source Buffer is flowed off or on.

    @note Scheme especially required for DFU over LE but currently commonly
          applied to DFU over LE or BR/EDR and when upgrade data is relayed from
          Primary to Secondary too.

    Returns TRUE when Source Buffer draining is flowed off in order to limit
            queued messages within acceptable limits to prevent pmalloc pools
            exhaustion, else FALSE.
*/
bool UpgradeIsProcessDataRequestFlowedOff(void)
{
    /*
     * TODO: Can be skipped for DFU over BR/EDR transport and also when
     *       upgrade data is relayed from Primary to Secondary over BR/EDR.
     */
    return UpgradeCtxGet()->dfu_rx_flow_off;
}

/***************************************************************************
NAME
    UpgradeIsInProgress

DESCRIPTION
    Return boolean indicating if an upgrade is currently in progress.
*/
bool UpgradeIsInProgress(void)
{
    return UpgradeSMUpgradeInProgress();
}

/***************************************************************************
NAME
    UpgradeIsAborting

DESCRIPTION
    Return boolean indicating if an upgrade is currently aborting.
*/
bool UpgradeIsAborting(void)
{
    return UpgradeSMGetState() == UPGRADE_STATE_ABORTING;
}

/***************************************************************************
NAME
    UpgradeIsScoActive

DESCRIPTION
    Finds whether indicating if SCO is active or not by accessing the upgrade context.
RETURN
    Returns pointer to uint16.
*/
uint16 *UpgradeIsScoActive(void)
{
    return &(UpgradeCtxGet()->isScoActive);
}

/****************************************************************************
NAME
    UpgradeSetScoActive

DESCRIPTION
    Used to assign required value (0 or 1) to SCO flag in upgrade context depending on active ongoing call.
*/
void UpgradeSetScoActive(bool scoState)
{
    UpgradeCtxGet()->isScoActive = (uint16)scoState;
    DEBUG_LOG("UpgradeSetScoActive state : %u", UpgradeCtxGet()->isScoActive);
}

bool UpgradePartitionDataInitWrapper(bool *WaitForEraseComplete)
{
    bool ret = TRUE;
    /* If the upgrade state machine is already in data transfer or greater state, it means the partition initialization
     * has already happend. We can get here even in data transfer state if PEB had GAIA linkloss and it reconnected.
     * in this case we should not re-initialize. */
    if(UpgradeSMGetState() < UPGRADE_STATE_DATA_TRANSFER)
    {
        ret = UpgradePartitionDataInit(WaitForEraseComplete);
    }
    else
    {
        *WaitForEraseComplete=FALSE;
    }
    DEBUG_LOG("UpgradePartitionDataInitWrapper ret:%d WaitForEraseComplete:%d", ret, *WaitForEraseComplete);
    return ret;
}

void UpgradeRestartReconnectionTimer(void)
{
    /*
     * In the post reboot DFU commit phase, now main role (Primary/Secondary)
     * are no longer fixed rather dynamically selected by Topology using role
     * selection. This process may take time so its recommendable to reset this
     * reconnection timer in linkloss scenarios (if any) in the post reboot
     * DFU commit phase.
     */
    if (MessageCancelAll(UpgradeGetUpgradeTask(),
                            UPGRADE_INTERNAL_RECONNECTION_TIMEOUT))
    {
        DEBUG_LOG("UpgradeRestartReconnectionTimer UPGRADE_INTERNAL_RECONNECTION_TIMEOUT");
        MessageSendLater(UpgradeGetUpgradeTask(),
                            UPGRADE_INTERNAL_RECONNECTION_TIMEOUT, NULL,
                            D_SEC(UPGRADE_WAIT_FOR_RECONNECTION_TIME_SEC));
    }
    if (MessageCancelAll(UpgradeGetUpgradeTask(),
                            UPGRADE_INTERNAL_SILENT_COMMIT_RECONNECTION_TIMEOUT))
    {
        DEBUG_LOG("UpgradeRestartReconnectionTimer UPGRADE_INTERNAL_SILENT_COMMIT_RECONNECTION_TIMEOUT");
        MessageSendLater(UpgradeGetUpgradeTask(),
                    UPGRADE_INTERNAL_SILENT_COMMIT_RECONNECTION_TIMEOUT, NULL,
                    D_SEC(UPGRADE_WAIT_FOR_RECONNECTION_TIME_SEC));
    }
}

uint32 UpgradeInProgressId(void)
{
    return (UpgradeCtxGetPSKeys()->id_in_progress);
}

void UpgradeSetInProgressId(uint32 id_in_progress)
{
    UpgradeCtxGetPSKeys()->id_in_progress = id_in_progress;
    UpgradeSavePSKeys();
}

bool UpgradeIsSilentCommitEnabled(void)
{
    return (UpgradeCtxGetPSKeys()->is_silent_commit != 0);
}

/****************************************************************************
NAME
    UpgradeRebootForSilentCommit

DESCRIPTION
     Initiate DFU reboot for silent commit. This function will eventually call
     the ImageUpgradeSwapTry() trap to initiate a full chip reset, load and run
     images from the other image bank.

RETURNS
    None
*/
void UpgradeRebootForSilentCommit(void)
{
    uint16 in_progress_key = UpgradeCtxGetPSKeys()->upgrade_in_progress_key;
    if(in_progress_key == UPGRADE_RESUME_POINT_PRE_REBOOT)
    {
        DEBUG_LOG("UpgradeRebootForSilentCommit: Send message to reboot");
        UpgradeSMHandleValidated(UPGRADE_INTERNAL_SILENT_COMMIT_REBOOT, NULL);
    }
    else
    {
        DEBUG_LOG("UpgradeRebootForSilentCommit: Ignored since resume point is %d",
                   in_progress_key);
    }
}

/****************************************************************************
NAME
    UpgradeSetSilentCommitSupported

DESCRIPTION
    Used to assign required value (0 or 1) to isSilentCommitSupported flag in
    upgrade context by application.
*/
void UpgradeSetSilentCommitSupported(uint8 is_silent_commit_supported)
{
    UpgradeCtxGet()->isSilentCommitSupported = is_silent_commit_supported;
    DEBUG_LOG("UpgradeSetSilentCommitSupported: %u", UpgradeCtxGet()->isSilentCommitSupported);
}

/****************************************************************************
NAME
    UpgradeSetPeerDfuSupport

DESCRIPTION
    Used to assign TRUE to is_peer_dfu_supported flag in
    upgrade context by dfu peer domain.
*/
void UpgradeSetPeerDfuSupport(bool is_peer_dfu_supported)
{
    UpgradeCtxGet()->isUpgradePeerDfuSupported = is_peer_dfu_supported;
}

/****************************************************************************
NAME
    UpgradeGetPeerDfuSupport

DESCRIPTION
    Get the value stored in isUpgradePeerDfuSupported
*/
bool UpgradeGetPeerDfuSupport(void)
{
    return UpgradeCtxGet()->isUpgradePeerDfuSupported;
}

/****************************************************************************
NAME
    UpgradeSetFPtr

DESCRIPTION
     To set the appropriate fptr in UpgradeCtxGet()->funcs.

RETURNS
    None
*/
void UpgradeSetFPtr(const upgrade_response_functions_t *fptr)
{
    UpgradeCtxGet()->funcs = fptr;
}

/****************************************************************************
NAME
    UpgradeClientSendData

DESCRIPTION
    Wrapper function which invokes UpgradeHostIFClientSendData()
*/
void UpgradeClientSendData(uint8 *data, uint16 dataSize)
{
    UpgradeHostIFClientSendData(data, dataSize);
}

/****************************************************************************
NAME
    UpgradeGetPartitionFirstWord

DESCRIPTION
    A function to find the first word of partition data from the DFU Header PS Keys
*/
static uint32 UpgradeGetPartitionFirstWord(uint8 *data)
{
    uint32 firstWord;
    firstWord  = (uint32)data[3] << 24;
    firstWord |= (uint32)data[2] << 16;
    firstWord |= (uint32)data[1] << 8;
    firstWord |= (uint32)data[0];

    return firstWord;
}

/****************************************************************************
NAME
    UpgradePartitionOffsetHelper

DESCRIPTION
    Helper function for UpgradeGetDfuFileOffset, to add the offset
    (full partition length or last written position) of a partiton to the total DFU file offset
*/
static void UpgradePartitionOffsetHelper(uint32 *part_data_len, uint16 *part_num,
            uint32 *firstWord, uint16 last_closed_part, uint32 *total_dfu_file_offset,
            uint16 *hdr_idx, bool *part_hdr, UpgradePartitionDataCtx *ctx, bool *early_return)
{
    DEBUG_LOG_VERBOSE("UpgradePartitionOffsetHelper : Part_num = %d, last_closed_partn = %d, Part_data len = %d,  firstWord = 0x%08lx, Total offset so far = %d, hdr_idx = %d",
                      *part_num, last_closed_part, *part_data_len, *firstWord, *total_dfu_file_offset, *hdr_idx);

    /*Variables w.r.t latest active partition*/
    uint32 part_offset;
    uint32 sink_first_word = *firstWord;
    uint16 sink_part_num = *part_num;
    ctx->state = UPGRADE_PARTITION_DATA_STATE_DATA;
    ctx->partitionLength = *part_data_len;
    if(*part_num < last_closed_part)
    {
        /*subtracting firstWord size here since it is already counted along with partition header contribution*/
        *total_dfu_file_offset += (*part_data_len-PARTITION_FIRST_WORD_SIZE);
        /*Reset remaining variables*/
        *part_data_len = 0;
        *hdr_idx = 0;
        *firstWord = 0;
        *part_hdr = FALSE;
        ctx->state = UPGRADE_PARTITION_DATA_STATE_GENERIC_1ST_PART;
        ctx->newReqSize = HEADER_FIRST_PART_SIZE;
        ctx->openNextPartition = TRUE;
    }
    else if(*part_num >= last_closed_part)
    {
        DEBUG_LOG_INFO("Partiton on which DFU was interrupted = %d and first word = 0x%08lx",*part_num, *firstWord);

        /* Partition could have been already open if we are resuming (without rebooting the device) 
           the upgrade so, check and use the partitionHdl if its non-zero. */
        if(!ctx->partitionHdl)
        {
            DEBUG_LOG_INFO("UpgradePartitionOffsetHelper, open partion %d to write", sink_part_num);
            ctx->partitionHdl = UpgradePartitionDataPartitionOpen(sink_part_num, sink_first_word);
            if (!ctx->partitionHdl)
            {
                DEBUG_LOG_ERROR("UpgradePartitionOffsetHelper, failed to open partition %u", sink_part_num);
                Panic();
            }
        }

        if(ImageUpgradeSinkGetPosition((Sink) ctx->partitionHdl, &part_offset)) /*Trap call to get offset from sink*/
        {
            DEBUG_LOG_INFO("UpgradePartitionOffsetHelper : Sink offset of interrupted partiton : %ld", part_offset);
        }
        else
        {
            DEBUG_LOG_ERROR("PANIC, UpgradePartitionOffsetHelper : Could not retrieve partition offset");
            Panic();
        }
        *total_dfu_file_offset += (part_offset); 
        *part_hdr = FALSE;
        *firstWord = 0;
        ctx->newReqSize = (*part_data_len - part_offset  - PARTITION_FIRST_WORD_SIZE);

        DEBUG_LOG_INFO("Total DFU file offset = %ld", *total_dfu_file_offset);
        /*Since we are here this indicates the last interrupted partition therefore
        we can return the total DFU file offset early rather than going through all DFU Header PS Key*/
        *early_return = TRUE;
    }
}

/****************************************************************************
NAME
    UpgradeGetDfuFileOffset

DESCRIPTION
    Calculates and returns an offset from start of DFU File as requested by GSound library.
    Uses DFU Header PS Key information.
*/
uint32 UpgradeGetDfuFileOffset(void)
{
    uint8 *key_cache; /*To store contents of Header PS Key*/
    uint16 header_pskey, part_num=0;

    /*hdr_idx : Keeps track of how much of upgrade header / partition header is currently parsed (in bytes)*/
    uint16 hdr_idx = 0;
    /*header_pskey_offset : Keeps a track of how much of PS Key is currently parsed (in bytes)*/
    uint16 header_pskey_offset = 0;
    uint16 header_pskey_len; /*initially in words*/
    uint32 upg_hdr_len=0;
    uint32 part_data_len=0;
    uint32 firstWord = 0;
    uint32 total_dfu_file_offset = 0;

    next_partition_header_field_t nextPartitionField = part_hdr_id;
    bool upg_hdr = FALSE;
    bool part_hdr = FALSE;
    /*A flag to indicate if the last interrupted partition's contribution is done to total offset hence return early*/
    bool early_return = FALSE;
    UpgradePartitionDataCtx *ctx = UpgradeCtxGetPartitionData();
    if(!ctx)
    {
        DEBUG_LOG_INFO("UpgradeGetDfuFileOffset : No UpgradePartitionDataCtx found !");
        UpgradePartitionDataInitHelper(FALSE);
        ctx = UpgradeCtxGetPartitionData();
    }

    uint16 last_closed_part = UpgradeCtxGetPSKeys()->last_closed_partition;
    DEBUG_LOG_INFO("UpgradeGetDfuFileOffset : Last Closed Partition = %d\n", last_closed_part);

    for(header_pskey = DFU_HEADER_PSKEY_START; header_pskey <= DFU_HEADER_PSKEY_END; header_pskey++)
    {
        /* Find out how many words are written into PSKEY and read contents into local key cache */
        header_pskey_len = PsRetrieve(header_pskey, NULL, 0);

        if(header_pskey_len == 0)
        {
            DEBUG_LOG_INFO("UpgradeGetDfuFileOffset : EMPTY header PS Key = %d", header_pskey);
            break;
        }
        ctx->dfuHeaderPskey = header_pskey;
        ctx->dfuHeaderPskeyOffset = 0;
        
        /*Converting PSKey len in bytes since all the calculations are being done in bytes.*/
        header_pskey_len *= (sizeof(uint16));
        /* Allocate temporary buffer to store dfu header PSKEY data */
        key_cache = (uint8 *)PanicUnlessMalloc(header_pskey_len);
        /*Read the contents of dfu header pskey into the key cache*/
        PsRetrieve(header_pskey, key_cache, header_pskey_len);
#if 0
        for(uint8 i=0; i<header_pskey_len; ++i)
        {
            DEBUG_LOG_INFO("UpgradeGetDfuFileOffset : key_cache[%d] : %d\n",i, key_cache[i]);
        }
#endif
        header_pskey_offset = 0;
        /* This condition at the start of a ps key takes care of the case where
         * upgrade header spans across more than one ps key. In that case we'd like
         * to update the header_pskey_offset accordingly until we are done with upgrade header.
         */
        if(upg_hdr)
            header_pskey_offset += hdr_idx;

        if(header_pskey_offset < header_pskey_len )
        {
            while(header_pskey_offset <  header_pskey_len)
            {
                DEBUG_LOG_INFO("UpgradeGetDfuFileOffset : Initial hdr_idx = %d and Initial header_pskey_offset = %d ", hdr_idx, header_pskey_offset);
    /*==========================================================================*/
                /*If it is the Upgrade header : APPUHDR5*/
                if(hdr_idx == 0 && 0 == strncmp((char *)&key_cache[header_pskey_offset], UpgradeFWIFGetHeaderID(), ID_FIELD_SIZE))
                {
                    DEBUG_LOG_INFO("UpgradeGetDfuFileOffset : Upgrade Header");
                    ctx->state = UPGRADE_PARTITION_DATA_STATE_HEADER;
                    upg_hdr = TRUE;
                    hdr_idx = ID_FIELD_SIZE;
                    upg_hdr_len = ByteUtilsGet4BytesFromStream(&key_cache[hdr_idx]);
                    /*increment hdr_idx by size of length field of the upgrade header and then by the value of length field. i.e (8+4+length of upgrade header)*/
                    hdr_idx += ((HEADER_FIRST_PART_SIZE-ID_FIELD_SIZE)+upg_hdr_len);
                    DEBUG_LOG_INFO("UpgradeGetDfuFileOffset : hdr_idx in APPUHDR5 = %d , upg_hdr_len APPUHDR5= %ld ", hdr_idx, upg_hdr_len);
                    DEBUG_LOG_INFO("UpgradeGetDfuFileOffset : Header PS Key len : %d", header_pskey_len);
                    if(hdr_idx <= header_pskey_len && header_pskey_len > HEADER_FIRST_PART_SIZE)
                    {
                        DEBUG_LOG_INFO("UpgradeGetDfuFileOffset : Upgrade Header covered in single DFU header PS Keys");
                        total_dfu_file_offset += hdr_idx;
                        header_pskey_offset += hdr_idx;
                        upg_hdr = FALSE;
                        hdr_idx = 0; /*Reset this indicating next header will be that of a partition*/
                        ctx->state = UPGRADE_PARTITION_DATA_STATE_GENERIC_1ST_PART;
                        ctx->newReqSize = HEADER_FIRST_PART_SIZE;
                        ctx->dfuHeaderPskeyOffset = header_pskey_offset;
                        DEBUG_LOG_INFO("UpgradeGetDfuFileOffset : DONE WITH UPGRADE HEADER.");
                    }
                    else
                    {
                        DEBUG_LOG_INFO("UpgradeGetDfuFileOffset : Upgrade Header spans across multiple PS Keys or only header first part of Upgrade header is stored in PS key");
                        header_pskey_offset += header_pskey_len;
                        total_dfu_file_offset += header_pskey_len;
                        hdr_idx -= header_pskey_len;
                        if(header_pskey_len == HEADER_FIRST_PART_SIZE)
                        {
                            DEBUG_LOG_INFO("Only header first part of Upgrade header is stored in PS key");
                            ctx->newReqSize = upg_hdr_len-(header_pskey_len);
                            ctx->dfuHeaderPskeyOffset = header_pskey_len; /*which should be 12 as of now*/
                            if(key_cache)
                                free(key_cache);
                            return total_dfu_file_offset;
                        }
                    }
                    ctx->isUpgradeHdrAvailable = TRUE;
                    DEBUG_LOG_VERBOSE("UpgradeGetDfuFileOffset : After 1st iteration on APPUHDR5, header_pskey_offset = %d, hdr_idx = %d, total_dfu_file_offset so far = %d\n", header_pskey_offset, hdr_idx, total_dfu_file_offset);
                }
    /*==========================================================================*/
                /*If it is the partition header : PARTDATA*/
                else if (hdr_idx == 0 && 0 == strncmp((char *)&key_cache[header_pskey_offset], UpgradeFWIFGetPartitionID(), ID_FIELD_SIZE) && !part_hdr)
                {
                    DEBUG_LOG_INFO("UpgradeGetDfuFileOffset : Partition Header");
                    part_hdr = TRUE;
                    part_data_len = 0;
                    part_num = 0;
                    ctx->state = UPGRADE_PARTITION_DATA_STATE_DATA_HEADER;
                    
                    /*if pskey offset + 20 (i.e size of entire partition header) <= ps key length*/
                    if(header_pskey_offset + (HEADER_FIRST_PART_SIZE+PARTITION_TYPE_SIZE+PARTITION_NUM_SIZE+PARTITION_FIRST_WORD_SIZE) <= header_pskey_len)
                    {
                        DEBUG_LOG_INFO("UpgradeGetDfuFileOffset : Partition Header is fully contained inside current Header PS Key");
                        header_pskey_offset += ID_FIELD_SIZE;
                        part_data_len = ByteUtilsGet4BytesFromStream(&key_cache[header_pskey_offset])-(PARTITION_TYPE_SIZE+PARTITION_NUM_SIZE);
                        /*increment header pskey offset by size of len field (since already parsed) and size of type field (since we are not intereseted in getting its value)*/
                        header_pskey_offset += (PARTITION_LEN_SIZE + PARTITION_TYPE_SIZE);
                        part_num = ByteUtilsGet2BytesFromStream(&key_cache[header_pskey_offset]);
                        header_pskey_offset += PARTITION_NUM_SIZE;
                        firstWord = UpgradeGetPartitionFirstWord(&key_cache[header_pskey_offset]);
                        header_pskey_offset += PARTITION_FIRST_WORD_SIZE;
                        total_dfu_file_offset += (HEADER_FIRST_PART_SIZE+PARTITION_TYPE_SIZE+PARTITION_NUM_SIZE+PARTITION_FIRST_WORD_SIZE);

                        ctx->dfuHeaderPskeyOffset = header_pskey_offset;

                        UpgradePartitionOffsetHelper(&part_data_len, &part_num, &firstWord, last_closed_part, &total_dfu_file_offset, &hdr_idx, &part_hdr, ctx, &early_return);
                        /*return if last interrupted partition already made its contribution*/
                        if(early_return)
                        {
                            DEBUG_LOG_INFO("UpgradeGetDfuFileOffset : Return total dfu file offset = %ld", total_dfu_file_offset);
                            if(key_cache)
                                free(key_cache);
                            return total_dfu_file_offset;
                        }
                    }
                    else if(header_pskey_offset + (HEADER_FIRST_PART_SIZE+PARTITION_TYPE_SIZE+PARTITION_NUM_SIZE) <= header_pskey_len)
                    {
                        DEBUG_LOG_INFO("UpgradeGetDfuFileOffset : Partition Header is contained till part_num inside current Header PSKey");
                        hdr_idx += (HEADER_FIRST_PART_SIZE+PARTITION_TYPE_SIZE+PARTITION_NUM_SIZE);
                        header_pskey_offset += ID_FIELD_SIZE;
                        part_data_len = ByteUtilsGet4BytesFromStream(&key_cache[header_pskey_offset])-(PARTITION_TYPE_SIZE+PARTITION_NUM_SIZE);
                        /*increment header pskey offset by size of len field (since already parsed) and size of type field (since we are not intereseted in getting its value)*/
                        header_pskey_offset += (PARTITION_LEN_SIZE + PARTITION_TYPE_SIZE);
                        part_num = ByteUtilsGet2BytesFromStream(&key_cache[header_pskey_offset]);
                        header_pskey_offset += PARTITION_NUM_SIZE;
                        nextPartitionField = part_hdr_first_word;
                    }
                    else if(header_pskey_offset + (HEADER_FIRST_PART_SIZE+PARTITION_TYPE_SIZE) <= header_pskey_len)
                    {
                        DEBUG_LOG_INFO("UpgradeGetDfuFileOffset : Partition Header is contained till part_type inside current Header PSKey");
                        hdr_idx += (HEADER_FIRST_PART_SIZE+PARTITION_TYPE_SIZE);
                        header_pskey_offset += ID_FIELD_SIZE;
                        part_data_len = ByteUtilsGet4BytesFromStream(&key_cache[header_pskey_offset])-(PARTITION_TYPE_SIZE+PARTITION_NUM_SIZE);
                        /*increment header pskey offset by size of len field (since already parsed) and size of type field (since we are not intereseted in getting its value)*/
                        header_pskey_offset += (PARTITION_LEN_SIZE + PARTITION_TYPE_SIZE);
                        nextPartitionField = part_hdr_num;
                    }
                    else if(header_pskey_offset + HEADER_FIRST_PART_SIZE <= header_pskey_len)
                    {
                        DEBUG_LOG_INFO("UpgradeGetDfuFileOffset : Partition Header is contained till part_len inside current Header PSKey");
                        hdr_idx += HEADER_FIRST_PART_SIZE;
                        header_pskey_offset += ID_FIELD_SIZE;
                        part_data_len = ByteUtilsGet4BytesFromStream(&key_cache[header_pskey_offset])-(PARTITION_TYPE_SIZE+PARTITION_NUM_SIZE);
                        header_pskey_offset += PARTITION_LEN_SIZE;
                        nextPartitionField = part_hdr_type;
                    }
                    else if(header_pskey_offset + ID_FIELD_SIZE <= header_pskey_len)
                    {
                        DEBUG_LOG_INFO("UpgradeGetDfuFileOffset : Partition Header is contained till part_ID inside current Header PSKey");
                        hdr_idx += ID_FIELD_SIZE;
                        header_pskey_offset += ID_FIELD_SIZE;
                        nextPartitionField = part_hdr_len;
                    }
                    
                    total_dfu_file_offset += hdr_idx;
                    /*If part header is interrupted mid way next req size has to be of 8 bytes since header first part will be there in PS Key*/
                    ctx->newReqSize = (ID_FIELD_SIZE); /*Usually at this point has to be 8 bytes
                    , since we are in partition header hence header first part HAS to be present in PS Key*/
                    ctx->dfuHeaderPskeyOffset = header_pskey_offset;

                }
    /*==========================================================================*/
                /*If it is footer : APPUPFTR, simply returning the calculated total file offset so far*/
                else if (hdr_idx == 0 && 0 == strncmp((char *)&key_cache[header_pskey_offset], UpgradeFWIFGetFooterID(), ID_FIELD_SIZE))
                {
                    DEBUG_LOG_INFO("UpgradeGetDfuFileOffset : Footer, Returning total dfu file offset = %ld", total_dfu_file_offset);
                    ctx->state = UPGRADE_PARTITION_DATA_STATE_FOOTER;
                    if(key_cache)
                        free(key_cache);
                    return total_dfu_file_offset;
                }
    /*==========================================================================*/
                /*Evaluate remainder of DFU upgrade header (will be done in new PS KEY) since hdr_idx != 0 and we are not yet finished parsing the upgrade header*/
                else if(upg_hdr && hdr_idx != 0 && hdr_idx < header_pskey_len)
                {
                    DEBUG_LOG_VERBOSE("UpgradeGetDfuFileOffset : Adding remaining APPUHDR5 hdr_idx to total_offset");
                    total_dfu_file_offset += hdr_idx;
                    upg_hdr = FALSE;
                    ctx->state = UPGRADE_PARTITION_DATA_STATE_GENERIC_1ST_PART;
                    ctx->newReqSize = HEADER_FIRST_PART_SIZE;
                    ctx->dfuHeaderPskeyOffset = hdr_idx;
                    hdr_idx = 0;
                    DEBUG_LOG_VERBOSE("UpgradeGetDfuFileOffset : Done with Upgrade Header completely.");
                    DEBUG_LOG_VERBOSE("UpgradeGetDfuFileOffset : Current header_pskey_offset = %d and hdr_idx = %d", header_pskey_offset, hdr_idx);
                }
    /*==========================================================================*/
                /*Evaluate remainder of partition header (will be done in next PS KEY) since hdr_idx != 0 and we are not finished parsing the partition header*/
                else if(part_hdr && hdr_idx != 0 && hdr_idx < header_pskey_len)
                {
                    DEBUG_LOG_INFO("UpgradeGetDfuFileOffset : Partition Header continued across Header PS Key so looking for next partition field enum:next_partition_header_field_t:%d", nextPartitionField);
                    if(nextPartitionField == part_hdr_len)
                    {
                        DEBUG_LOG_INFO("UpgradeGetDfuFileOffset : Inside Continued Partition Header and current Partition field = part_hdr_len");
                        part_data_len = ByteUtilsGet4BytesFromStream(&key_cache[header_pskey_offset])-(PARTITION_TYPE_SIZE+PARTITION_NUM_SIZE);
                        /*increment header pskey offset by size of len field (since already parsed) and size of type field (since we are not intereseted in getting its value)*/
                        header_pskey_offset += (PARTITION_LEN_SIZE); 
                        /*case where in continued partition header{i.e Partition header split across 2 ps keys} only First part of partiton header was saved in PS key*/
                        if(header_pskey_offset == header_pskey_len)
                        {
                            DEBUG_LOG_INFO("UpgradeGetDfuFileOffset : We are in next PS Key and only header first part for partition header was saved in PS Key");
                            total_dfu_file_offset += header_pskey_offset;
                            /*Now the new request size would be = entire partition header size - header first part size i.e (20-12)*/
                            ctx->newReqSize = ((HEADER_FIRST_PART_SIZE+PARTITION_TYPE_SIZE+PARTITION_NUM_SIZE+PARTITION_FIRST_WORD_SIZE)-HEADER_FIRST_PART_SIZE);
                            ctx->dfuHeaderPskeyOffset = header_pskey_offset;
                            /*ctx->state will be partition header only as of now since we did not change it*/
                            if(key_cache)
                                free(key_cache);
                            return total_dfu_file_offset;
                        }
                        header_pskey_offset+= (PARTITION_TYPE_SIZE);
                        part_num = ByteUtilsGet2BytesFromStream(&key_cache[header_pskey_offset]);
                        header_pskey_offset += PARTITION_NUM_SIZE;
                        firstWord = UpgradeGetPartitionFirstWord(&key_cache[header_pskey_offset]);
                        header_pskey_offset += PARTITION_FIRST_WORD_SIZE;
                    }
                    else if(nextPartitionField == part_hdr_type)
                    {
                        DEBUG_LOG_INFO("UpgradeGetDfuFileOffset : Inside Continued Partition Header and current Partition field = part_hdr_type");
                        header_pskey_offset += PARTITION_TYPE_SIZE; /*increment by size of Partition Type field in header*/
                        part_num = ByteUtilsGet2BytesFromStream(&key_cache[header_pskey_offset]);
                        header_pskey_offset += PARTITION_NUM_SIZE;
                        firstWord = UpgradeGetPartitionFirstWord(&key_cache[header_pskey_offset]);
                        header_pskey_offset += PARTITION_FIRST_WORD_SIZE;
                    }
                    else if(nextPartitionField == part_hdr_num)
                    {
                        DEBUG_LOG_INFO("UpgradeGetDfuFileOffset : Inside Continued Partition Header and current Partition field = part_hdr_num");
                        part_num = ByteUtilsGet2BytesFromStream(&key_cache[header_pskey_offset]);
                        header_pskey_offset += PARTITION_NUM_SIZE;
                        firstWord = UpgradeGetPartitionFirstWord(&key_cache[header_pskey_offset]);
                        header_pskey_offset += PARTITION_FIRST_WORD_SIZE;
                    }
                    else if(nextPartitionField == part_hdr_first_word)
                    {
                        DEBUG_LOG_INFO("UpgradeGetDfuFileOffset : Inside Continued Partition Header and current Partition field = part_hdr_first_word");
                        firstWord = UpgradeGetPartitionFirstWord(&key_cache[header_pskey_offset]);
                        header_pskey_offset += PARTITION_FIRST_WORD_SIZE;
                    }

                    total_dfu_file_offset += header_pskey_offset;
                    /* By now we have the required partition num, len of that partition and the first word of the partition
                    * data for a particular partition header in ps key now check if the entire partition len
                    * needs to be added to total offset or if this was the partition where upgrade was interrupted*/

                    DEBUG_LOG_INFO("UpgradeGetDfuFileOffset :Continued in partition header, header_pskey_offset = %d, part_data_len in PARTDATA = %d, part_num in PARTDATA = %d, firstWord in PARTDATA = 0x%08lx", header_pskey_offset, part_data_len, part_num, firstWord);
                    DEBUG_LOG_INFO("UpgradeGetDfuFileOffset : Partition num %d : and last closed partition (Upgrade Lib PSKey) : %d", part_num, last_closed_part);

                    ctx->dfuHeaderPskeyOffset = header_pskey_offset;
                    
                    UpgradePartitionOffsetHelper(&part_data_len, &part_num, &firstWord, last_closed_part, &total_dfu_file_offset, &hdr_idx, &part_hdr, ctx, &early_return);
                    if(early_return)
                    {
                        DEBUG_LOG_INFO("UpgradeGetDfuFileOffset : Return total dfu file offset = %ld", total_dfu_file_offset);
                        if(key_cache)
                            free(key_cache);
                        return total_dfu_file_offset;
                    }
                }
    /*==========================================================================*/
                /*Wrong header encountered ?*/
                else if(hdr_idx == 0 && 0 != strncmp((char *)&key_cache[header_pskey_offset], UpgradeFWIFGetHeaderID(), ID_FIELD_SIZE)
                        && 0 != strncmp((char *)&key_cache[header_pskey_offset], UpgradeFWIFGetPartitionID(), ID_FIELD_SIZE)
                        && 0 != strncmp((char *)&key_cache[header_pskey_offset], UpgradeFWIFGetFooterID(), ID_FIELD_SIZE))
                {
                    DEBUG_LOG_ERROR("PANIC UpgradeGetDfuFileOffset : Unknown Header should not have been here!");
                    Panic();
                }
            }
        }
        /* To handle a case where dfu upgrade header spans across the entire
         * dfu key (when more than 2) say pskey 1,2,3,..,n then this handles a
         * case of pskey 2 to n-1 being skipped entirely (and its len being added to total offset)
         */
        else if(upg_hdr && hdr_idx >= header_pskey_len)
        {
            DEBUG_LOG_VERBOSE("UpgradeGetDfuFileOffset : Upgrade Header spans across more than 2 Keys");
            total_dfu_file_offset += header_pskey_len;
            hdr_idx -= header_pskey_len;
            ctx->newReqSize -= (header_pskey_len);
            ctx->dfuHeaderPskeyOffset = header_pskey_len;
            if(hdr_idx == 0)
            {
                upg_hdr = FALSE;
                ctx->state = UPGRADE_PARTITION_DATA_STATE_GENERIC_1ST_PART;
                ctx->newReqSize = HEADER_FIRST_PART_SIZE;
                DEBUG_LOG_VERBOSE("UpgradeGetDfuFileOffset : Done with Upgrade Header");
            }
        }

        if(key_cache)
            free(key_cache);
    }
    DEBUG_LOG_INFO("UpgradeGetDfuFileOffset : DFU file offset post iterating through all non-empty keys = %ld", total_dfu_file_offset);
    return total_dfu_file_offset;
}

/***********************************************************************
NAME
    Upgrade_SetContext

DESCRIPTION
    Sets the context of the UPGRADE module
    The value is stored in the UPGRADE PsKey and hence is non-volatile
*/
void Upgrade_SetContext(upgrade_context_t context)
{
    UpgradeCtxGetPSKeys()->upgrade_context = context;
    UpgradeSavePSKeys();
}

/***********************************************************************
NAME
    Upgrade_GetContext

DESCRIPTION
    Gets the context of the UPGRADE module
    The value is retreived from the non-volatile UPGRADE PsKey.
*/
upgrade_context_t Upgrade_GetContext(void)
{
    if(!UpgradeCtxGetPSKeys())
    {
        DEBUG_LOG_ERROR("Upgrade_GetContext : Upgrade PSKey not found !");
        Panic();
    }
    return UpgradeCtxGetPSKeys()->upgrade_context;
}

/***********************************************************************
NAME
    UpgradePartitionDataInitHelper

DESCRIPTION
    Helper to UpgradePartitionDataInit. This method initializes the partition data ctx
    accordingly depending on host type and is called from DFU domain.
*/
void UpgradePartitionDataInitHelper(bool dfu_file_offset_required)
{
    DEBUG_LOG("UpgradePartitionDataInitHelper : dfu_file_offset_required = %d", dfu_file_offset_required);
    UpgradePartitionDataCtx *ctx;

    ctx = UpgradeCtxGetPartitionData();
    if (!ctx)
    {
        ctx = (UpgradePartitionDataCtx *)PanicUnlessMalloc(sizeof(*ctx));
        memset(ctx, 0, sizeof(*ctx));
        UpgradeCtxSetPartitionData(ctx);
    }

    if(dfu_file_offset_required)
    {
        /*ctx->state, newReqSize, dfuHeaderPskey, dfuHeaderPskeyOffset for UpgradePartitionDataCtx will be set inside this API*/
        UpgradeCtxGet()->dfu_file_offset = UpgradeGetDfuFileOffset();
        DEBUG_LOG_INFO("UpgradePartitionDataInitHelper: new reqSize = %d and offset from start of dfu file= %d", ctx->newReqSize, UpgradeCtxGet()->dfu_file_offset);
    }
    else
    {
        ctx->newReqSize = HEADER_FIRST_PART_SIZE;
        ctx->offset = 0;
        ctx->state = UPGRADE_PARTITION_DATA_STATE_GENERIC_1ST_PART;
    }
    DEBUG_LOG("UpgradePartitionDataInitHelper : UpgradePartitionDataCtx->partitionLength = %d, newReqSize = %d, totalReqSize = %d, totalReceivedSize = %d, offset = %d, state enum:UpgradePartitionDataState:%d, dfuHeaderPskey = %d, dfuHeaderPskeyOffset = %d",
                   ctx->partitionLength, ctx->newReqSize, ctx->totalReqSize, ctx->totalReceivedSize, ctx->offset,ctx->state, ctx->dfuHeaderPskey, ctx->dfuHeaderPskeyOffset);
}

/***************************************************************************
NAME
    Upgrade_SetHostType

DESCRIPTION
    Set upgrade host type which is to be used internally by upgrade lib to differentiate
    between the resume flow for GAIA and GAA_OTA
*/
void Upgrade_SetHostType(upgrade_context_t host_type)
{
    DEBUG_LOG_VERBOSE("Upgrade_SetHostType to enum:upgrade_context_t:%d", host_type);
    UpgradeCtxGet()->upg_host_type = host_type;
}

/***************************************************************************
NAME
    Upgrade_GetHostType

DESCRIPTION
    Get upgrade host type from UpgradeCtx
*/
upgrade_context_t Upgrade_GetHostType(void)
{
    DEBUG_LOG_VERBOSE("UpgradeGetHostType enum:upgrade_context_t:%d", UpgradeCtxGet()->upg_host_type);
    return UpgradeCtxGet()->upg_host_type;
}

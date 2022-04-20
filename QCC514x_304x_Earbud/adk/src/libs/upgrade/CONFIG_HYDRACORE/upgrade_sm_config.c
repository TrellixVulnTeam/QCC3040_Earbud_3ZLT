/****************************************************************************
Copyright (c) 2014 - 2020 Qualcomm Technologies International, Ltd.


FILE NAME
    upgrade_sm.c

DESCRIPTION

NOTES

*/

#define DEBUG_LOG_MODULE_NAME upgrade
#include <logging.h>

#include <stdlib.h>
#include <boot.h>
#include <loader.h>
#include <ps.h>
#include <panic.h>
#include <upgrade.h>
#include <imageupgrade.h>

#include "upgrade_ctx.h"
#include "upgrade_host_if_data.h"
#include "upgrade_partition_data.h"
#include "upgrade_psstore.h"
#include "upgrade_partitions.h"
#include "upgrade_partition_validation.h"
#include "upgrade_partitions.h"
#include "upgrade_sm.h"
#include "upgrade_msg_vm.h"
#include <upgrade_protocol.h>
#include "upgrade_msg_fw.h"
#include "upgrade_msg_internal.h"
#include "upgrade_fw_if.h"

static uint8 is_validated = 0;

/*
NAME
    UpgradeSMConfigSetValidation - Set the static variable is_validated
*/
void UpgradeSMConfigSetValidation(uint8 val)
{
    DEBUG_LOG_DEBUG("UpgradeSMConfigSetValidation: val=%u", val);
    is_validated = val;
}

/*
NAME
    IsValidatedToTrySwap - Ensures all data are validated before trying to swap image.

DESCRIPTION
    Uses an incremental flag to ensure that all parts of a DFU image have been
    copied and validated before trying to call the ImageUpgradeSwapTry() trap.
*/
static void IsValidatedToTrySwap(bool reset)
{
    if(reset)
    {
        UpgradeSMConfigSetValidation(0);
        return;
    }
	
    DEBUG_LOG_INFO("IsValidatedToTrySwap, is_validated %d", is_validated);

    switch(is_validated)
    {
    /* Last part of the DFU image has been copied and validated */
    case 0:
        DEBUG_LOG_INFO("IsValidatedToTrySwap, all DFU images have been validated");
        is_validated++;
        break;

    /* All part have been copied and validated */
    case 1:
        {
            DEBUG_LOG_INFO("IsValidatedToTrySwap(): Shutdown audio before calling ImageUpgradeSwapTry()");

            /*
             * The audio needs to be shut down before calling the ImageUpgradeSwapTry trap.
             * This is applicable to audio SQIF or ROM, to avoid deadlocks in Apps P0, causing P0 to not stream audio data or process image swap request.
             */
            UpgradeApplyAudioShutDown();
        }
        break;

    default:
        return;
    }
}

/*
NAME
    FileTransferCompleted - Operation post file transfer completion

DESCRIPTION
    If it is a silent commit, only store silent commit flag in PSKEY and inform
    peer (if applicable).
    If it is either interactive commit, or a go ahead for reboot post silent
    commit, store resume point in PSKEY and initiate DFU reboot.
*/
static void FileTransferCompleted(bool is_silent_commit, bool inform_peer)
{
    uint8 action = (uint8)is_silent_commit;
    UpgradeSendUpgradeOpsStatus(UpgradeGetAppTask(), upgrade_ops_init_peer_context, NO_ACTION);
    if(inform_peer)
    {
        UpgradeSendUpgradeOpsStatus(UpgradeGetAppTask(), upgrade_ops_trnsfr_complt_res_send_to_peer,
                                    action);
    }

    if(is_silent_commit)
    {
        UpgradeCtxGetPSKeys()->is_silent_commit = UPGRADE_COMMIT_SILENT;
        UpgradeSavePSKeys();
        DEBUG_LOG_DEBUG("FileTransferCompleted: is_silent_commit saved");
        /* tell host application we're complete */
        UpgradeCtxGet()->funcs->SendShortMsg(UPGRADE_HOST_SILENT_COMMIT_CFM);
        /* If the current device is not a primary device then, inform the application 
         * that silent commit command has been received from the host.
         * primary device should wait for secondary to process the command first. */

        UpgradeSendUpgradeOpsStatus(UpgradeGetAppTask(), upgrade_ops_send_silent_commit_ind_to_host, NO_ACTION);
    }
    else
    {
        UpgradeCtxGetPSKeys()->upgrade_in_progress_key = 
                                            UPGRADE_RESUME_POINT_POST_REBOOT;
        UpgradeSavePSKeys();
        DEBUG_LOG_DEBUG("FileTransferCompleted: UPGRADE_RESUME_POINT_POST_REBOOT saved");

#ifndef HOSTED_TEST_ENVIRONMENT
        if(UPGRADE_IS_PEER_SUPPORTED)
        {
            /* After the UPGRADE_PEER_TRANSFER_COMPLETE_RES msg is sent to the
             *  peer device, the primary device wait for 1 sec before
             * reboot. The handling of device reboot will be done in
             * UPGRADE_INTERNAL_TRIGGER_REBOOT case.
             */
            DEBUG_LOG_DEBUG("FileTransferCompleted: Reboot after 1sec");
            MessageSendLater(UpgradeGetUpgradeTask(), UPGRADE_INTERNAL_TRIGGER_REBOOT,
                             NULL, UPGRADE_WAIT_FOR_REBOOT);
        }
        else
        {
            /* For a standalone DFU, immediately go ahead and reboot. The handling
             * of device reboot will be done in UPGRADE_INTERNAL_TRIGGER_REBOOT case.
             */
            DEBUG_LOG_INFO("FileTransferCompleted: Reboot now");
            MessageSend(UpgradeGetUpgradeTask(), UPGRADE_INTERNAL_TRIGGER_REBOOT, NULL);
        }
#endif
    }
}

/* This is the last state before reboot */
bool UpgradeSMHandleValidated(MessageId id, Message message)
{
    UpgradeCtx *ctx = UpgradeCtxGet();

    DEBUG_LOG_INFO("UpgradeSMHandleValidated, MESSAGE:0x%x, message %p", id, message);

    switch(id)
    {
        
    case UPGRADE_INTERNAL_CONTINUE:

         /* Check if UPGRADE_HOST_IS_CSR_VALID_DONE_REQ message is received */
        if(ctx->isCsrValidDoneReqReceived)
        {
            UpgradeSendUpgradeOpsStatus(UpgradeGetAppTask(), upgrade_ops_internal_handle_post_vldtn_msg_rcvd, NO_ACTION);
        }
        else
        {
            /* We come to this scenario when primary device is reset during primary
             * to secondary file transfer and role switch happens. The new primary
             * will connect to new secondary after the dfu file is transferred to 
             * new primary from Host. After the connection, the new primary does not
             * know whether the new secondary got all the dfu data or not. So, the
             * new secondary on reaching to UpgradeSMHandleValidated through resume
             * point on boot-up will wait for the image copy to get completed first,
             * as this get initiated in HandleValidating(), and then send the
             * UPGRADE_HOST_TRANSFER_COMPLETE_IND to new primary device, so that the
             * new primary knows that file transfer is completed in new secondary,
             * and proceed ahead with reboot and commit phase of the DFU.
             */
            DEBUG_LOG("UpgradeSMHandleValidated: send UPGRADE_HOST_TRANSFER_COMPLETE_IND");
            UpgradeSendUpgradeOpsStatus(UpgradeGetAppTask(), upgrade_ops_handle_csr_valid_done_req_not_received, NO_ACTION);
        }
        break;

    case UPGRADE_HOST_TRANSFER_COMPLETE_RES:
        {
            UPGRADE_HOST_TRANSFER_COMPLETE_RES_T *msg = (UPGRADE_HOST_TRANSFER_COMPLETE_RES_T *)message;

            /* Interactive Commit */
            if(msg !=NULL && msg->action == UPGRADE_COMMIT_INTERACTIVE)
            {
                DEBUG_LOG("UpgradeSMHandleValidated: Interactive Commit");
                /* Host could have reconnected and selected interactive commit while a 
                 * silent commit was pending so, reset silent commit flag if its set. */
                if(UpgradeCtxGetPSKeys()->is_silent_commit)
                {
                    UpgradeCtxGetPSKeys()->is_silent_commit = FALSE;
                    UpgradeSavePSKeys();
                }
                /* Initiate DFU reboot */
                FileTransferCompleted(FALSE, TRUE);
            }

            /* Silent Commit */
            else if(msg !=NULL && msg->action == UPGRADE_COMMIT_SILENT)
            {
                DEBUG_LOG("UpgradeSMHandleValidated: Silent Commit");

                /* If the host sends silent commit request and device does not support it,
                 * send error message to the host. */
                if(!ctx->isSilentCommitSupported)
                {
                    UpgradeFatalError(UPGRADE_HOST_ERROR_SILENT_COMMIT_NOT_SUPPORTED);
                }
                else
                {
                    /* For silent commit, update PSKEY but do not initiate DFU reboot */
                    FileTransferCompleted(TRUE, TRUE);
                }
            }

            /* Abort */
            else
            {
                UpgradeSendUpgradeOpsStatus(UpgradeGetAppTask(),
                upgrade_ops_abort_post_transfer_complete, NO_ACTION);
            }
        }
        break;

        case UPGRADE_INTERNAL_SILENT_COMMIT_REBOOT:
            {
                /* Restore the SM state and is_validation flag (which might have
                 * got reset if device was reset post silent commit command) to
                 * make sure that DFU reboot gets triggered.
                 */
                UpgradeSMSetState(UPGRADE_STATE_VALIDATED);
                UpgradeSMConfigSetValidation(1);

                /* Initiate DFU reboot since we got go ahead to reboot for silent commit case. 
                 * There is no need to inform peer about this since, DFU domain will handle it. */
                FileTransferCompleted(FALSE, FALSE);
            }
            break;
    
    case UPGRADE_INTERNAL_TRIGGER_REBOOT:
        {
            UpgradeSendEndUpgradeDataInd(upgrade_end_state_complete,
                                         UPGRADE_SEND_END_DATA_IND_WITHOUT_DELAY);

            UpgradeSendUpgradeOpsStatus(UpgradeGetAppTask(), upgrade_ops_permit_reboot_on_condition, NO_ACTION);

            /*Can consider disconnecting streams here*/

            /* if we have permission, go ahead and call loader/reboot */
            if (UpgradeSMHavePermissionToProceed(UPGRADE_APPLY_IND))
            {
                DEBUG_LOG_DEBUG("UpgradeSMHandleValidated: IsValidatedToTrySwap(FALSE) in UPGRADE_HOST_TRANSFER_COMPLETE_RES");
                IsValidatedToTrySwap(FALSE);
            }
        }
        break;
    case UPGRADE_HOST_IS_CSR_VALID_DONE_REQ:
        {
            UpgradeSendUpgradeOpsStatus(UpgradeGetAppTask(), upgrade_ops_handle_post_vldtn_msg_rcvd, NO_ACTION);
        }
        break;

    case UPGRADE_HOST_TRANSFER_COMPLETE_IND:
        /* Receive the UPGRADE_HOST_TRANSFER_COMPLETE_IND message from Peer
         * device once the DFU data is successfully transferred and validated
         * in Peer device. Then, send UPGRADE_HOST_TRANSFER_COMPLETE_IND
         * to Host now.
         */
        {
            /* Check if the Image copy is completed or not in Primary device. 
             * If not, then wait for the completion, else send the UPGRADE_HOST_TRANSFER_COMPLETE_IND
             * to Host on Image copy completion
             */
            if(UpgradeCtxGet()->isImgUpgradeCopyDone)
            {
                /* During Peer DFU process, set the resume point as 
                 * UPGRADE_RESUME_POINT_PRE_REBOOT here. Refer to HandleValidating()
                 * routine for more details.
                 */
                if (UpgradeCtxGetPSKeys()->upgrade_in_progress_key != UPGRADE_RESUME_POINT_POST_REBOOT)
                {
                    UpgradeCtxGetPSKeys()->upgrade_in_progress_key = UPGRADE_RESUME_POINT_PRE_REBOOT;
                    UpgradeSavePSKeys();
                    DEBUG_LOG("P&R: UPGRADE_RESUME_POINT_PRE_REBOOT saved @ %d", __LINE__);
                }
                else
                {
                    DEBUG_LOG("Not changing from UPGRADE_RESUME_POINT_POST_REBOOT @ %d", __LINE__);
                }

                /* Validation completed, and now waiting for UPGRADE_TRANSFER_COMPLETE_RES
                 * protocol message. Update resume point and ensure we remember it.
                 */
                DEBUG_LOG_DEBUG("UpgradeSMHandleValidated: UPGRADE_HOST_TRANSFER_COMPLETE_IND sent to Host");
                UpgradeCtxGet()->funcs->SendShortMsg(UPGRADE_HOST_TRANSFER_COMPLETE_IND);
            }
#ifndef HOSTED_TEST_ENVIRONMENT
            else
            {
                DEBUG_LOG("UpgradeSMHandleValidated: Copy not completed in primary, wait");
                MessageSendConditionally(UpgradeGetUpgradeTask(), UPGRADE_HOST_TRANSFER_COMPLETE_IND,
                                         NULL, UpgradeCtxGetImageCopyStatus());
            }
#endif
            break;
        }

    /* application finally gave permission, warm reboot */
    case UPGRADE_INTERNAL_REBOOT:
        {
            DEBUG_LOG_DEBUG("UpgradeSMHandleValidated: IsValidatedToTrySwap(FALSE) in UPGRADE_INTERNAL_REBOOT");
            IsValidatedToTrySwap(FALSE);
        }
        break;

    case UPGRADE_VM_IMAGE_UPGRADE_COPY_SUCCESSFUL:
        DEBUG_LOG_INFO("UpgradeSMHandleValidated, UPGRADE_VM_IMAGE_UPGRADE_COPY_SUCCESSFUL");
        /*
         * Try the images from the "other image bank" in all QSPI devices.
         * The apps p0 will initiate a warm reset.
         */
        DEBUG_LOG_DEBUG("UpgradeSMHandleValidated: IsValidatedToTrySwap(FALSE) in UPGRADE_VM_IMAGE_UPGRADE_COPY_SUCCESSFUL");

        IsValidatedToTrySwap(FALSE);
        break;

    case UPGRADE_VM_DFU_COPY_VALIDATION_SUCCESS:
        {
            DEBUG_LOG_DEBUG("UpgradeSMHandleValidated: ImageUpgradeSwapTry() in UPGRADE_VM_DFU_COPY_VALIDATION_SUCCESS");
            ImageUpgradeSwapTry();
        }
        break;

    case UPGRADE_VM_AUDIO_DFU_FAILURE:
        UpgradeApplyAudioCopyFailed();
    case UPGRADE_VM_IMAGE_UPGRADE_COPY_FAILED:
        UpgradeSMMoveToState(UPGRADE_STATE_SYNC);
        break;

    default:
        return FALSE;
    }

    return TRUE;
}

/*
NAME
    UpgradeSMAbort - Clean everything and go to the sync state.

DESCRIPTION
    Common handler for clearing up an upgrade after an abort
    and going back to a state that is ready for a new upgrade.
*/
bool UpgradeSMAbort(void)
{
    UpgradeSMConfigSetValidation(0);

    /*If we received an abort request before starting the DFU*/
    if(UpgradeInProgressId() == 0)
    {
        DEBUG_LOG("UpgradeSMAbort return false to inform synchronous abort, Upgrade not yet started");
        return FALSE;
    }

    UpgradeSendUpgradeOpsStatus(UpgradeGetAppTask(), upgrade_ops_handle_abort, NO_ACTION);

    /* if we are going to reboot to revert commit, then we are alredy running from alternate bank
     * so, we shouldn't erase. return False to inform synchronous abort*/
    if(UpgradeCtxGet()->isImageRevertNeededOnAbort)
    {
        DEBUG_LOG("UpgradeSMAbort return false to inform synchronous abort and without erase");
        return FALSE;
    }

    /* if we have permission erase immediately and return to the SYNC state
     * to start again if required */
    if (UpgradeSMHavePermissionToProceed(UPGRADE_BLOCKING_IND))
    {
#ifdef MESSAGE_IMAGE_UPGRADE_COPY_STATUS
        /*
         * There may be non-blocking traps such as ImageUpgradeCopy in progress.
         * Call the ImageUpgradeAbortCommand() trap to abort any of those. It
         * will do no harm if there are no non-blocking traps in progress.
         */
        DEBUG_LOG_DEBUG("UpgradeSMAbort: ImageUpgradeAbortCommand()");
        ImageUpgradeAbortCommand();
#endif  /* MESSAGE_IMAGE_UPGRADE_COPY_STATUS */
        UpgradeSMErase();
        UpgradeSMSetState(UPGRADE_STATE_SYNC);
    }

    return TRUE;
}

uint16 UpgradeSMNewImageStatus(void)
{
    uint16 err = 0;
    bool result = ImageUpgradeSwapTryStatus();
    DEBUG_LOG_DEBUG("UpgradeSMNewImageStatus: ImageUpgradeSwapTryStatus() = %u", result);
    if (!result)
    {
        err = UPGRADE_HOST_ERROR_LOADER_ERROR;
    }
    return err;
}

/*
NAME
    UpgradeSMCheckEraseComplete

DESCRIPTION
    Indicate whether the erase has completed.
    Returns FALSE for CONFIG_HYDRACORE as UpgradePartitionsEraseAllManaged
    is non-blocking and completion is indicated by the
    MESSAGE_IMAGE_UPGRADE_ERASE_STATUS message.
*/
bool UpgradeSMCheckEraseComplete(void)
{
    if (UPGRADE_RESUME_POINT_ERASE != UpgradeCtxGetPSKeys()->upgrade_in_progress_key)
    {
        if(UpgradeCtxGet()->smState == UPGRADE_STATE_COMMIT)
        {
            return TRUE;
        }
        else
        {
            return FALSE;
        }
    }
    else
    {
        return TRUE;
    }
}

/*
NAME
    UpgradeSMActionOnValidated

DESCRIPTION
    Calls ImageUpgradeCopy().
*/
void UpgradeSMActionOnValidated(void)
{
#ifdef MESSAGE_IMAGE_UPGRADE_COPY_STATUS
    DEBUG_LOG_DEBUG("UpgradeSMActionOnValidated: ImageUpgradeCopy()");
    ImageUpgradeCopy();
#endif  /* MESSAGE_IMAGE_UPGRADE_COPY_STATUS */
}

/*
NAME
    UpgradeSMHandleAudioDFU

DESCRIPTION
    Calls UpgradeSMHandleAudioImageCopy().
*/
void UpgradeSMHandleAudioDFU(void)
{
#ifdef MESSAGE_IMAGE_UPGRADE_AUDIO_STATUS
    DEBUG_LOG_DEBUG("UpgradeSMHandleAudioDFU: ImageUpgradeAudio()");
    ImageUpgradeAudio();
#endif
}

/*
NAME
    UpgradeSMHandleAbort

DESCRIPTION
    Perform Upgrade Abort related activities
*/
void UpgradeSMHandleAbort(void)
{
    UpgradeSendEndUpgradeDataInd(upgrade_end_state_abort,
                                 UPGRADE_SEND_END_DATA_IND_WITHOUT_DELAY);
    IsValidatedToTrySwap(TRUE);
    UpgradeSMMoveToState(UPGRADE_STATE_SYNC);
}

/*
NAME
    UpgradeSMSetPermission
DESCRIPTION
    Set the Upgrade Ctx perms
*/
void UpgradeSMSetPermission(upgrade_permission_t perm)
{
    UpgradeCtxGet()->perms = perm;
}

/*
NAME
    UpgradeSMHandleImageCopyStatusForPrim

DESCRIPTION
    For primary device, check for the upgrade image copy done and status and
    accordingly return the value. Return TRUE if the image copy is done and
    image copy status is received or image copy is in progress. Otherwise return FALSE.
*/
bool UpgradeSMHandleImageCopyStatusForPrim(void)
{
    if(UpgradeCtxGet()->isImgUpgradeCopyDone)
    {
        if(UpgradeCtxGet()->ImgUpgradeCopyStatus)
        {
            UpgradeCtxSetImageCopyStatus(IMAGE_UPGRADE_COPY_COMPLETED);
            return TRUE;
        }
        else
            return FALSE; /* No need to setup peer connection */
    }
    else
    {
        UpgradeCtxSetImageCopyStatus(IMAGE_UPGRADE_COPY_IN_PROGRESS);
        return TRUE;
    }
}

/*
NAME
    UpgradeSMHandleImageCopyStatus

DESCRIPTION
    Check if the upgrade image copy is successful. If yes, set the 
    upgrade_in_progress_key to UPGRADE_RESUME_POINT_PRE_REBOOT and send 
    UPGRADE_HOST_TRANSFER_COMPLETE_IND. Else, wait for the upgrade image copy
    to get over.
*/
void UpgradeSMHandleImageCopyStatus(bool is_internal_state_handling)
{
#ifndef HOSTED_TEST_ENVIRONMENT
    /* Check for image upgrade copy status. If the status is in
     * progress,then send conditional message to send
     * UPGRADE_HOST_TRANSFER_COMPLETE_IND to Host once the image
     * upgrade copy is completed.
     */
    if(UpgradeCtxGet()->isImgUpgradeCopyDone)
    {
        /* Validation completed, and now waiting for UPGRADE_TRANSFER_COMPLETE_RES
         * protocol message. Update resume point and ensure we remember it. */
        if (UpgradeCtxGetPSKeys()->upgrade_in_progress_key != UPGRADE_RESUME_POINT_POST_REBOOT)
        {
            UpgradeCtxGetPSKeys()->upgrade_in_progress_key = UPGRADE_RESUME_POINT_PRE_REBOOT;
            UpgradeSavePSKeys();
            DEBUG_LOG("P&R: UPGRADE_RESUME_POINT_PRE_REBOOT saved @ %d", __LINE__);
        }
        else
        {
            DEBUG_LOG("Not changing from UPGRADE_RESUME_POINT_POST_REBOOT @ %d", __LINE__);
        }
        UpgradeCtxGet()->funcs->SendShortMsg(UPGRADE_HOST_TRANSFER_COMPLETE_IND);
    }
    else
    {
        if(is_internal_state_handling)
        {
            MessageSendConditionally(UpgradeGetUpgradeTask(), UPGRADE_INTERNAL_CONTINUE, NULL,
                                          UpgradeCtxGetImageCopyStatus());
        }
        else
        {
            MessageSendConditionally(UpgradeGetUpgradeTask(), UPGRADE_HOST_IS_CSR_VALID_DONE_REQ, NULL,
                                          UpgradeCtxGetImageCopyStatus());
        }
    }
#endif
}

/*
NAME
    UpgradeSMSetImageCopyStatusToComplete

DESCRIPTION
    Set Upgrade image copy status to IMAGE_UPGRADE_COPY_COMPLETED.
*/
void UpgradeSMSetImageCopyStatusToComplete(void)
{
    UpgradeCtxSetImageCopyStatus(IMAGE_UPGRADE_COPY_COMPLETED);
}

/*
NAME
    UpgradeSMWaitForPeerConnection

DESCRIPTION
    Send message to loop internally until peer connection is established.
*/
void UpgradeSMWaitForPeerConnection(uint16 * peer_connection_status)
{
#ifndef HOSTED_TEST_ENVIRONMENT
    MessageSendConditionally(UpgradeGetUpgradeTask(), UPGRADE_INTERNAL_CONTINUE, NULL,
                                                      peer_connection_status);
#endif
}

/*
NAME
    UpgradeSMHandleValidDoneReqNotReceived

DESCRIPTION
    When primary device is reset during primary to secondary file transfer and
    role switch happens. Then, post new primary device data transfer completion,
    handle the image copy process of new secondary device, and eventually send
    UPGRADE_HOST_TRANSFER_COMPLETE_IND to new primary device.
    Refer to UpgradeSMHandleValidated() to understand more.
*/
void UpgradeSMHandleValidDoneReqNotReceived(void)
{
#ifndef HOSTED_TEST_ENVIRONMENT
    if(UpgradeCtxGet()->isImgUpgradeCopyDone)
    {
        UpgradeSendUpgradeOpsStatus(UpgradeGetAppTask(), upgrade_ops_reset_peer_current_state, NO_ACTION);

        /* Validation completed, and now waiting for UPGRADE_TRANSFER_COMPLETE_RES
         * protocol message. Update resume point and ensure we remember it. */
        if (UpgradeCtxGetPSKeys()->upgrade_in_progress_key != UPGRADE_RESUME_POINT_POST_REBOOT)
        {
            UpgradeCtxGetPSKeys()->upgrade_in_progress_key = UPGRADE_RESUME_POINT_PRE_REBOOT;
            UpgradeSavePSKeys();
            DEBUG_LOG("P&R: UPGRADE_RESUME_POINT_PRE_REBOOT saved @ %d", __LINE__);
        }
        else
        {
            DEBUG_LOG("Not changing from UPGRADE_RESUME_POINT_POST_REBOOT @ %d", __LINE__);
        }
        /* Send the UPGRADE_HOST_TRANSFER_COMPLETE_IND to complete the
         * data transfer process of DFU.
         */
        UpgradeCtxGet()->funcs->SendShortMsg(UPGRADE_HOST_TRANSFER_COMPLETE_IND);
    }
    else
        MessageSendConditionally(UpgradeGetUpgradeTask(), UPGRADE_INTERNAL_CONTINUE, NULL,
                                             UpgradeCtxGetImageCopyStatus());
#endif
}
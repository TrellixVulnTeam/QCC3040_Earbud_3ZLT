/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
*/

#ifdef INCLUDE_QCOM_CON_MANAGER

#include <message.h>
#include <task_list.h>
#include <panic.h>
#include <logging.h>
#include <app/bluestack/vsdm_prim.h>
#include "connection_manager.h"
#include "system_state.h"

#include "qualcomm_connection_manager.h"
#include "qualcomm_connection_manager_private.h"


#ifndef HOSTED_TEST_ENVIRONMENT

/*! There is checking that the messages assigned by this module do
not overrun into the next module's message ID allocation */
ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(QCOM_CON_MANAGER, QCOM_CON_MANAGER_MESSAGE_END)

#endif

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_TYPE(qcm_msgs_t)
LOGGING_PRESERVE_MESSAGE_TYPE(qualcomm_connection_manager_internal_message_t)

static bool qcomConManagerIsQhsSupported(const uint8 *features)
{
    return ((features[QHS_MODE_2345_OCTET] & QHS_MODE_2345_MASK) ||
            (features[QHS_MODE_6_OCTET]    & QHS_MODE_6_MASK));
}

static bool qcomConManagerIsFastExitSubrateSupported(const uint8 *features)
{
    return ((features[FAST_EXIT_SNIFF_SUBRATE_OCTET] & FAST_EXIT_SNIFF_SUBRATE_MASK) != 0);
}

static void qcomConManagerSendQhsConnectedIndicationToClients(const bdaddr *bd_addr)
{
    qcomConManagerTaskData *sp = QcomConManagerGet();
    MESSAGE_MAKE(qhs_connected_ind, QCOM_CON_MANAGER_QHS_CONNECTED_T);
    qhs_connected_ind->bd_addr = *bd_addr;
    TaskList_MessageSend(&sp->client_tasks, QCOM_CON_MANAGER_QHS_CONNECTED, qhs_connected_ind);
}

static void qcomConManagerSendReadRemoteQlmSuppFeaturesReq(const BD_ADDR_T *bd_addr)
{
    MAKE_VSDM_PRIM_T(VSDM_READ_REMOTE_QLM_SUPP_FEATURES_REQ);
    prim->phandle = 0;
    prim->bd_addr = *bd_addr;
    VmSendVsdmPrim(prim);
}

static void qcomConManagerTriggerRemoteFeaturesRead(const BD_ADDR_T *bd_addr, uint32 delay_ms)
{
    MESSAGE_MAKE(msg, QCOM_CON_MANAGER_INTERNAL_READ_QLM_REMOTE_FEATURES_T);
    msg->bd_addr = *bd_addr;
    MessageCancelFirst(QcomConManagerGetTask(), QCOM_CON_MANAGER_INTERNAL_READ_QLM_REMOTE_FEATURES);
    MessageSendLater(QcomConManagerGetTask(), QCOM_CON_MANAGER_INTERNAL_READ_QLM_REMOTE_FEATURES, msg, delay_ms);
}

static void qcomConManagerHandleVsdmQlmPhyChangeInd(const VSDM_QCM_PHY_CHANGE_IND_T * ind)
{
    DEBUG_LOG("qcomConManagerHandleVsdmQlmPhyChangeInd  qhs status=%d bdaddr lap=%06lX",
              ind->status,ind->bd_addr.lap);

    if(ind->status == HCI_SUCCESS)
    {
        bdaddr bd_addr;
        BdaddrConvertBluestackToVm(&bd_addr, &ind->bd_addr);
        qcomConManagerTriggerRemoteFeaturesRead(&ind->bd_addr, D_IMMEDIATE);
        ConManagerSetQhsConnectStatus(&bd_addr,TRUE);
        appDeviceSetQhsConnected(&bd_addr, TRUE);
        qcomConManagerSendQhsConnectedIndicationToClients(&bd_addr);
    }
}

static void qcomConManagerHandleVsdmRemoteQlmSuppFeaturesCfm(const VSDM_READ_REMOTE_QLM_SUPP_FEATURES_CFM_T *cfm)
{
    bool qhs_supported = FALSE;
    bool fast_exit_subrate_supported = FALSE;
    bdaddr bd_addr;
	
    BdaddrConvertBluestackToVm(&bd_addr, &cfm->bd_addr);

    /* Check if qhs is supported or not */
    if (cfm->status == HCI_SUCCESS)
    {
        qhs_supported = qcomConManagerIsQhsSupported(cfm->qlmp_supp_features);
        fast_exit_subrate_supported = qcomConManagerIsFastExitSubrateSupported(cfm->qlmp_supp_features);

        if (!qhs_supported)
        {
            /* Device doesn't support QHS, so clear the flag */
            appDeviceSetQhsConnected(&bd_addr, FALSE);
        }

    }

    DEBUG_LOG("qcomConManagerHandleVsdmRemoteQlmSuppFeaturesCfm status=%d lap:%x qhs:%d fes:%d",
               cfm->status, bd_addr.lap, qhs_supported, fast_exit_subrate_supported);

    ConManagerSetQhsSupportStatus(&bd_addr, qhs_supported);
    ConManagerSetFastExitSniffSubrateSupportStatus(&bd_addr, fast_exit_subrate_supported);
}

static void qcomConManagerHandleVsdmQlmConnectionCompleteInd(const VSDM_QLM_CONNECTION_COMPLETE_IND_T *ind)
{
    bool qlmp_connected = FALSE;
    bdaddr bd_addr;

    DEBUG_LOG("qcomConManagerHandleVsdmQlmConnectionCompleteInd status=0x%x lap=%06lX",
               ind->status,ind->bd_addr.lap);

    if(ind->status == HCI_SUCCESS)
    {
        qcomConManagerTriggerRemoteFeaturesRead(&ind->bd_addr, REQUEST_REMOVE_FEATURES_DELAY_MS);
        qlmp_connected = TRUE;
    }

    BdaddrConvertBluestackToVm(&bd_addr, &ind->bd_addr);
    ConManagerSetQlmpConnectStatus(&bd_addr, qlmp_connected);
}

static void qcomConManagerHandleVsdmScHostSuppOverrideCfm(const VSDM_WRITE_SC_HOST_SUPP_OVERRIDE_CFM_T *cfm)
{
    PanicFalse(cfm->status == HCI_SUCCESS);
}

static void qcomConManagerSendScHostSuppOverrideReq(void)
{
    uint8 count =0;

    MAKE_VSDM_PRIM_T(VSDM_WRITE_SC_HOST_SUPP_OVERRIDE_REQ);
    prim->phandle = 0;
    prim->num_compIDs = count;

    while(vendor_info[count].comp_id != 0)
    {
        prim->compID[count] = vendor_info[count].comp_id;
        prim->min_lmpVersion[count] = vendor_info[count].min_lmp_version;
        prim->min_lmpSubVersion[count] = vendor_info[count].min_lmp_sub_version;
        prim->num_compIDs = ++count;

        /* Check if input number of comp ids are not exceeding
           maximum number of comp ids supported by vs prims */
        if(prim->num_compIDs == VSDM_MAX_NO_OF_COMPIDS)
        {
            Panic();
        }
    }

    if(prim->num_compIDs != 0)
    {
        VmSendVsdmPrim(prim);
    }

    /* Send init confirmation */
    MessageSend(SystemState_GetTransitionTask(), QCOM_CON_MANAGER_INIT_CFM, NULL);
}

static void qcomConManagerSetLocalSupportedFeaturesFlags(uint8 *features)
{
    /* Check if qhs is supported or not */
    if (qcomConManagerIsQhsSupported(features))
    {
        QcomConManagerGet()->qhs = TRUE;
    }
    if (qcomConManagerIsFastExitSubrateSupported(features))
    {
        QcomConManagerGet()->fast_exit_sniff_subrate = TRUE;
    }
}

static void qcomConManagerHandleVsdmLocalQlmSuppFeaturesCfm(const VSDM_READ_LOCAL_QLM_SUPP_FEATURES_CFM_T *cfm)
{
    PanicFalse(cfm->status == HCI_SUCCESS);

    qcomConManagerSetLocalSupportedFeaturesFlags(cfm->qlmp_supp_features);

    if (QcomConManagerGet()->qhs)
    {
        /* Send SC host override req as local device supports qhs*/
        qcomConManagerSendScHostSuppOverrideReq();
    }
    else
    {
        DEBUG_LOG("qcomConManagerHandleVsdmLocalQlmSuppFeaturesCfm: QHS not supported");
        /* Inform application that qcom con manager init completed */
        MessageSend(SystemState_GetTransitionTask(), QCOM_CON_MANAGER_INIT_CFM, NULL);
    }
}


static void qcomConManagerSendReadLocalQlmSuppFeaturesReq(void)
{
    /*send VSDM_READ_LOCAL_QLM_SUPP_FEATURES_REQ  to find if qhs is supported or not*/
    MAKE_VSDM_PRIM_T(VSDM_READ_LOCAL_QLM_SUPP_FEATURES_REQ);
    prim->phandle = 0;
    VmSendVsdmPrim(prim);
}

static void qcomConManagerHandleVsdmRegisterCfm(const VSDM_REGISTER_CFM_T *cfm)
{
    PanicFalse(cfm->result == VSDM_RESULT_SUCCESS);
    /*Check if local device supports qhs or not and proceed to create qhs with remote
      handset if local supports qhs */
    qcomConManagerSendReadLocalQlmSuppFeaturesReq();
}

static void qcomConManagerRegisterReq(void)
{
    MAKE_VSDM_PRIM_T(VSDM_REGISTER_REQ);
    prim->phandle = 0;
    VmSendVsdmPrim(prim);
}

static void qcomConManagerHandleBluestackVsdmPrim(const VSDM_UPRIM_T *vsprim)
{
    switch (vsprim->type)
    {
    case VSDM_REGISTER_CFM:
        qcomConManagerHandleVsdmRegisterCfm((const VSDM_REGISTER_CFM_T *)vsprim);
        break;
    case VSDM_READ_LOCAL_QLM_SUPP_FEATURES_CFM:
        qcomConManagerHandleVsdmLocalQlmSuppFeaturesCfm((VSDM_READ_LOCAL_QLM_SUPP_FEATURES_CFM_T *)vsprim);
        break;
    case VSDM_WRITE_SC_HOST_SUPP_OVERRIDE_CFM:
        qcomConManagerHandleVsdmScHostSuppOverrideCfm((const VSDM_WRITE_SC_HOST_SUPP_OVERRIDE_CFM_T *)vsprim);
        break;
     case VSDM_QLM_CONNECTION_COMPLETE_IND:
        qcomConManagerHandleVsdmQlmConnectionCompleteInd((const VSDM_QLM_CONNECTION_COMPLETE_IND_T *)vsprim);
        break;
    case VSDM_READ_REMOTE_QLM_SUPP_FEATURES_CFM:
       qcomConManagerHandleVsdmRemoteQlmSuppFeaturesCfm((const VSDM_READ_REMOTE_QLM_SUPP_FEATURES_CFM_T *)vsprim);
       break;
    case VSDM_QCM_PHY_CHANGE_IND:
        qcomConManagerHandleVsdmQlmPhyChangeInd((const VSDM_QCM_PHY_CHANGE_IND_T *)vsprim);
        break;
    default:
        DEBUG_LOG("qcomConManagerHandleBluestackVsdmPrim : unhandled vsprim type %x", vsprim->type);
        break;
    }
}

static void qcomConManagerHandleMessage(Task task, MessageId id, Message message)
{
   UNUSED(task);

   switch (id)
   {
     /* VSDM prims from firmware */
    case MESSAGE_BLUESTACK_VSDM_PRIM:
        qcomConManagerHandleBluestackVsdmPrim((const VSDM_UPRIM_T *)message);
        break;

    case QCOM_CON_MANAGER_INTERNAL_READ_QLM_REMOTE_FEATURES:
    {
        const QCOM_CON_MANAGER_INTERNAL_READ_QLM_REMOTE_FEATURES_T *msg = message;
        qcomConManagerSendReadRemoteQlmSuppFeaturesReq(&msg->bd_addr);
        break;
    }

    default:
        break;
    }
}
/* Global functions */

bool QcomConManagerInit(Task task)
{
    UNUSED(task);

    DEBUG_LOG("QcomConManagerInit");
    memset(&qcom_con_manager, 0, sizeof(qcom_con_manager));
    qcom_con_manager.task.handler = qcomConManagerHandleMessage;
    TaskList_Initialise(&qcom_con_manager.client_tasks);

    /* Register with the firmware to receive MESSAGE_BLUESTACK_VSDM_PRIM messages */
    MessageVsdmTask(QcomConManagerGetTask());

    /* Register with the VSDM service */
    qcomConManagerRegisterReq();

    return TRUE;
}

void QcomConManagerRegisterClient(Task client_task)
{
    DEBUG_LOG("QcomConManagerRegisterClient");
    TaskList_AddTask(&qcom_con_manager.client_tasks, client_task);
}

void QcomConManagerUnRegisterClient(Task client_task)
{
    TaskList_RemoveTask(&qcom_con_manager.client_tasks, client_task);
}

bool QcomConManagerIsFastExitSniffSubrateSupported(const bdaddr *addr)
{
    return QcomConManagerGet()->fast_exit_sniff_subrate &&
           ConManagerGetFastExitSniffSubrateSupportStatus(addr);
}

#endif /* INCLUDE_QCOM_CON_MANAGER */

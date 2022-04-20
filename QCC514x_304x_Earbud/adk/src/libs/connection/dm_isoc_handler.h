/****************************************************************************
Copyright (c) 2019 - 2020 Qualcomm Technologies International, Ltd.


FILE NAME
    dm_isoc_handler.h

DESCRIPTION

*/

#ifndef    CONNECTION_DM_ISOC_HANDLER_H_
#define    CONNECTION_DM_ISOC_HANDLER_H_

#include <app/bluestack/dm_prim.h>
#include "connection_private.h"

#ifndef CL_EXCLUDE_ISOC

/* If we don't get a DM_ISOC_REGISTER_CFM by the time this expires, assume it failed */
#define ISOC_REGISTER_TIMEOUT    (1000)

/* If we don't get a DM_ISOC_UNREGISTER_CFM by the time this expires, assume it failed */
#define ISOC_UNREGISTER_TIMEOUT    (1000)

/****************************************************************************
NAME
    connectionHandleIsocRegisterReq

DESCRIPTION
    Register the task as utilising Isochronous connections. This registers it with
    BlueStack so on an incoming Isochronous connection the task will be asked whether
    its willing to accept it.

RETURNS
    void
*/
void connectionHandleIsocRegisterReq(connectionDmIsocState *state, const CL_INTERNAL_ISOC_REGISTER_REQ_T *req);


/****************************************************************************
NAME
    connectionHandleIsocRegisterCfm

DESCRIPTION
    Task has been sucessfully registered for receiving Isochronous connection
    notifications - inform the client.

RETURNS
    void
*/
void connectionHandleIsocRegisterCfm(connectionDmIsocState *state, const DM_ISOC_REGISTER_CFM_T *cfm);


/****************************************************************************
NAME
    connectionHandleIsocRegisterTimeoutInd

DESCRIPTION
    Task has not been registered for receiving Isochronous connection
    notifications - inform the client.

RETURNS
    void
*/
/*void connectionHandleIsocRegisterTimeoutInd(const CL_INTERNAL_ISOC_REGISTER_TIMEOUT_IND_T *ind);*/


/****************************************************************************
NAME
    connectionHandleIsocUnregisterReq

DESCRIPTION
    Unregister task with BlueStack indicating it is no longer interested in
    being notified about incoming Isochronous connections.

RETURNS
    void
*/
/*void connectionHandleIsocUnregisterReq(const CL_INTERNAL_ISOC_UNREGISTER_REQ_T *req);*/


/****************************************************************************
NAME
    connectionHandleIsocUnregisterCfm

DESCRIPTION
    Task has been sucessfully unregistered from receiving Isochronous connection
    notifications - inform the client.

RETURNS
    void
*/
/*void connectionHandleIsocUnregisterCfm(const DM_ISOC_UNREGISTER_CFM_T *cfm);*/


/****************************************************************************
NAME
    connectionHandleIsocUnregisterTimeoutInd

DESCRIPTION
    Task has not been unregistered from receiving Isochronous connection
    notifications - inform the client.

RETURNS
    void
*/
/*void connectionHandleIsocUnregisterTimeoutInd(const CL_INTERNAL_ISOC_UNREGISTER_TIMEOUT_IND_T *ind);*/


/****************************************************************************
NAME
    connectionHandleIsocConnectReq

DESCRIPTION
    This function will initiate an Isochronous Connection request.

RETURNS
    void
*/
void connectionHandleIsocConnectReq(connectionDmIsocState *state, const CL_INTERNAL_ISOC_CIS_CONNECT_REQ_T *req);


/****************************************************************************
NAME
    connectionHandleIsocConnectCfm

DESCRIPTION
    Response to the Isochronous connection request indicating either that a Isochronous
    connection has been opened or that the attempt has failed.

RETURNS
    void
*/
void connectionHandleIsocConnectCfm(connectionDmIsocState *state, const DM_ISOC_CIS_CONNECT_CFM_T *cfm);


/****************************************************************************
NAME
    connectionHandleIsocConnectInd

DESCRIPTION
    Indication that the remote device wishes to open a Isochronous connection.

RETURNS
    void
*/
void connectionHandleIsocConnectInd(connectionDmIsocState *state, const DM_ISOC_CIS_CONNECT_IND_T *ind);


/****************************************************************************
NAME
    connectionHandleIsocConnectRes

DESCRIPTION
    Response accepting (or not) an incoming Isochronous connection.

RETURNS
    void
*/
void connectionHandleIsocConnectRes(connectionDmIsocState *state, const CL_INTERNAL_ISOC_CIS_CONNECT_RES_T *res);


/****************************************************************************
NAME
    connectionHandleIsocDisconnectReq

DESCRIPTION
    Request to disconnect an existing Isochronous connection.

RETURNS
    void
*/
void connectionHandleIsocDisconnectReq(connectionDmIsocState *state, const CL_INTERNAL_ISOC_CIS_DISCONNECT_REQ_T *req);


/****************************************************************************
NAME
    connectionHandleIsocDisconnectInd

DESCRIPTION
    Indication that the Isochronous connection has been disconnected. This is
    received by both sides, regardless of which one initiated the disconnect.

RETURNS
    void
*/
void connectionHandleIsocDisconnectInd(connectionDmIsocState *state, const DM_ISOC_CIS_DISCONNECT_IND_T *ind);


/****************************************************************************
NAME
    connectionHandleIsocDisconnectCfm

DESCRIPTION
    Confirmation that the Isochronous connection has been disconnected. This is
    received by both sides, regardless of which one initiated the disconnect <--- is this correct?

RETURNS
    void
*/
void connectionHandleIsocDisconnectCfm(connectionDmIsocState *state, const DM_ISOC_CIS_DISCONNECT_CFM_T *cfm);


/****************************************************************************
NAME
    connectionHandleIsocRenegotiateReq

DESCRIPTION
    Request to change the connection parameters of an existing Isochronous connection.

RETURNS
    void
*/
/*void connectionHandleIsocRenegotiateReq(const CL_INTERNAL_ISOC_RENEGOTIATE_REQ_T *req);*/


/****************************************************************************
NAME
    connectionHandleIsocRenegotiateInd

DESCRIPTION
    Indication that remote device has changed the connection parameters of an existing
    Isochronous connection.

RETURNS
    void
*/
/*void connectionHandleIsocRenegotiateInd(const DM_ISOC_RENEGOTIATE_IND_T *ind);*/


/****************************************************************************
NAME
    connectionHandleIsocRenegotiateCfm

DESCRIPTION
    Confirmation of local device's attempt to change the connection parameters of an existing
    Isochronous connection.


RETURNS
    void
*/
/*void connectionHandleIsocRenegotiateCfm(const DM_ISOC_RENEGOTIATE_CFM_T *cfm);*/

/****************************************************************************
NAME
    connectionHandleIsocConfigureCigReq

DESCRIPTION
    Request to configure a CIG.


RETURNS
    void
*/
void connectionHandleIsocConfigureCigReq(connectionDmIsocState *state, const CL_INTERNAL_ISOC_CONFIGURE_CIG_REQ_T *req);

/****************************************************************************
NAME
    connectionHandleIsocConfigureCigCfm

DESCRIPTION
    Confirmation of a local device's attempt to configure a CIG.


RETURNS
    void
*/
void connectionHandleIsocConfigureCigCfm(connectionDmIsocState *state, const DM_ISOC_CONFIGURE_CIG_CFM_T *cfm);

/****************************************************************************
NAME
    connectionHandleIsocRemoveCigReq

DESCRIPTION
    Request to remove a CIG.


RETURNS
    void
*/
void connectionHandleIsocRemoveCigReq(connectionDmIsocState *state, const CL_INTERNAL_ISOC_REMOVE_CIG_REQ_T *req);

/****************************************************************************
NAME
    connectionHandleIsocRemoveCigCfm

DESCRIPTION
    Confirmation of a local device's attempt to remove a CIG.


RETURNS
    void
*/
void connectionHandleIsocRemoveCigCfm(connectionDmIsocState *state, const DM_ISOC_REMOVE_CIG_CFM_T *cfm);

/****************************************************************************
NAME
    connectionHandleIsocSetupDataPathReq

DESCRIPTION
    Request to set an Isochronous connection's data path.


RETURNS
    void
*/
void connectionHandleIsocSetupDataPathReq(connectionDmIsocState *state, const CL_INTERNAL_ISOC_SETUP_ISOCHRONOUS_DATA_PATH_REQ_T *req);

/****************************************************************************
NAME
    connectionHandleIsocSetupDataPathCfm

DESCRIPTION
    Confirmation of a local device's attempt to set an Isochronous connection's data path.


RETURNS
    void
*/
void connectionHandleIsocSetupDataPathCfm(connectionDmIsocState *state, const DM_ISOC_SETUP_ISO_DATA_PATH_CFM_T *cfm);

/****************************************************************************
NAME
    connectionHandleIsocRemoveDataPathReq

DESCRIPTION
    Request to remove an Isochronous connection's data path.


RETURNS
    void
*/
void connectionHandleIsocRemoveDataPathReq(connectionDmIsocState *state, const CL_INTERNAL_ISOC_REMOVE_ISO_DATA_PATH_REQ_T * req);

/****************************************************************************
NAME
    connectionHandleIsocRemoveDataPathCfm

DESCRIPTION
    Confirmation of a local device's attempt to remove an Isochronous connection's data path.


RETURNS
    void
*/
void connectionHandleIsocRemoveDataPathCfm(connectionDmIsocState *state, const DM_ISOC_REMOVE_ISO_DATA_PATH_CFM_T *cfm);

/****************************************************************************
NAME
    connectionHandleIsocCreateBigReq

DESCRIPTION
    Request to create a BIG


RETURNS
    void
*/
void connectionHandleIsocCreateBigReq(connectionDmIsocState *state, const CL_INTERNAL_ISOC_CREATE_BIG_REQ_T *req);

/****************************************************************************
NAME
    connectionHandleIsocCreateBigCfm

DESCRIPTION
    Confirmation of a local device's attempt to create a BIG


RETURNS
    void
*/
void connectionHandleIsocCreateBigCfm(connectionDmIsocState *state, const DM_ISOC_CREATE_BIG_CFM_T *cfm);

/****************************************************************************
NAME
    connectionHandleIsocTerminateBigReq

DESCRIPTION
    Request to terminate a BIG


RETURNS
    void
*/
void connectionHandleIsocTerminateBigReq(connectionDmIsocState *state, const CL_INTERNAL_ISOC_TERMINATE_BIG_REQ_T *req);

/****************************************************************************
NAME
    connectionHandleIsocTerminateBigCfm

DESCRIPTION
    Confirmation of a local device's attempt to terminate a BIG


RETURNS
    void
*/
void connectionHandleIsocTerminateBigCfm(connectionDmIsocState *state, const DM_ISOC_TERMINATE_BIG_CFM_T *cfm);

/****************************************************************************
NAME
    connectionHandleIsocBigCreateSyncReq

DESCRIPTION
    Request to synchronize to a BIG


RETURNS
    void
*/
void connectionHandleIsocBigCreateSyncReq(connectionDmIsocState *state, const CL_INTERNAL_ISOC_BIG_CREATE_SYNC_REQ_T *req);

/****************************************************************************
NAME
    connectionHandleIsocBigCreateSyncCfm

DESCRIPTION
    Confirmation of a local device's attempt to to synchronize to a BIG


RETURNS
    void
*/
void connectionHandleIsocBigCreateSyncCfm(connectionDmIsocState *state, const DM_ISOC_BIG_CREATE_SYNC_CFM_T *cfm);

/****************************************************************************
NAME
    connectionHandleIsocBigTerminateSyncInd

DESCRIPTION
    Indication of either status of DM_ISOC_BIG_TERMINATE_SYNC_REQ or that the
    BIG has been terminated by the remote device or sync lost with remote device.


RETURNS
    void
*/
void connectionHandleIsocBigTerminateSyncInd(const DM_ISOC_BIG_TERMINATE_SYNC_IND_T *ind);

/****************************************************************************
NAME
    connectionHandleIsocBigInfoAdvReportInd

DESCRIPTION
    Indication that a BIG Advertising report has been received.


RETURNS
    void
*/
void connectionHandleIsocBigInfoAdvReportInd(connectionDmIsocState *state, const DM_HCI_ULP_BIGINFO_ADV_REPORT_IND_T *ind);

#endif
#endif    /* CONNECTION_DM_ISOC_HANDLER_H_ */

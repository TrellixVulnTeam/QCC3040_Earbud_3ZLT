/****************************************************************************
Copyright (c) 2004 - 2020 Qualcomm Technologies International, Ltd.


FILE NAME
    a2dp_profile_handler.c

DESCRIPTION
    File containing the profile handler function for the a2dp library.

NOTES

*/


/****************************************************************************
    Header files
*/
#include "a2dp_debug.h"

#include "a2dp.h"
#include "a2dp_codec_handler.h"
#include "a2dp_init.h"
#include "a2dp_l2cap_handler.h"
#include "a2dp_private.h"
#include "a2dp_profile_handler.h"
#include "a2dp_packet_handler.h"
#include "a2dp_sdp.h"
#include "a2dp_command_handler.h"

#ifndef BUILD_FOR_23F
#include <sink.h>
#include <stream.h>
#endif

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_TYPE(A2DP_INTERNAL_T)

/*****************************************************************************/
static void handleUnexpected(uint16 type)
{
    A2DP_FATAL_IN_DEBUG("A2DP handleUnexpected - MsgId 0x%x", type);

    UNUSED(type); /* only used in debug */
}


/****************************************************************************/
static void sendEncryptionChangeInd(const CL_SM_ENCRYPTION_CHANGE_IND_T *ind)
{
    MAKE_A2DP_MESSAGE(A2DP_ENCRYPTION_CHANGE_IND);
    message->encrypted = ind->encrypted;
    message->a2dp = a2dp;
    MessageSend(a2dp->clientTask, A2DP_ENCRYPTION_CHANGE_IND, message);
}


/*****************************************************************************/
void a2dpProfileHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch (id)
    {
    case A2DP_INTERNAL_L2CAP_CONNECT_REQ + 0:
    case A2DP_INTERNAL_L2CAP_CONNECT_REQ + 1:
    case A2DP_INTERNAL_L2CAP_CONNECT_REQ + 2:
    case A2DP_INTERNAL_L2CAP_CONNECT_REQ + 3:
    case A2DP_INTERNAL_L2CAP_CONNECT_REQ + 4:
    case A2DP_INTERNAL_L2CAP_CONNECT_REQ + 5:
    case A2DP_INTERNAL_L2CAP_CONNECT_REQ + 6:
        DEBUG_LOG("a2dpProfileHandler A2DP_INTERNAL_L2CAP_CONNECT_REQ");
        a2dpHandleL2capConnectReq((const A2DP_INTERNAL_L2CAP_CONNECT_REQ_T *) message);
        break;
    
    case A2DP_INTERNAL_SIGNALLING_CONNECT_REQ:
        DEBUG_LOG("a2dpProfileHandler A2DP_INTERNAL_SIGNALLING_CONNECT_REQ");
        a2dpHandleSignallingConnectReq((const A2DP_INTERNAL_SIGNALLING_CONNECT_REQ_T *) message);
        break;

    case A2DP_INTERNAL_SIGNALLING_CONNECT_RES:
        DEBUG_LOG("a2dpProfileHandler A2DP_INTERNAL_SIGNALLING_CONNECT_RES");
        a2dpHandleSignallingConnectRes((const A2DP_INTERNAL_SIGNALLING_CONNECT_RES_T *) message);
        break;

    case A2DP_INTERNAL_SIGNALLING_DISCONNECT_REQ:
        DEBUG_LOG("a2dpProfileHandler A2DP_INTERNAL_SIGNALLING_DISCONNECT_REQ");
        a2dpHandleSignallingDisconnectReq((const A2DP_INTERNAL_SIGNALLING_DISCONNECT_REQ_T *) message);
        break;

    case A2DP_INTERNAL_SIGNALLING_MTU_REQ:
        DEBUG_LOG("a2dpProfileHandler A2DP_INTERNAL_SIGNALLING_MTU_REQ");
        a2dpHandleSignallingUseLargeMTU((const A2DP_INTERNAL_SIGNALLING_MTU_REQ_T *) message);
        break;

    case A2DP_INTERNAL_CODEC_CONFIGURE_RSP:
        DEBUG_LOG("a2dpProfileHandler A2DP_INTERNAL_CODEC_CONFIGURE_RSP");
        a2dpHandleCodecConfigureResponse((const A2DP_INTERNAL_CODEC_CONFIGURE_RSP_T *) message);
        break;

    case A2DP_INTERNAL_MEDIA_OPEN_REQ:
        DEBUG_LOG("a2dpProfileHandler A2DP_INTERNAL_OPEN_REQ");
        a2dpStreamEstablish((const A2DP_INTERNAL_MEDIA_OPEN_REQ_T *) message);
        break;

    case A2DP_INTERNAL_MEDIA_OPEN_RES:
        DEBUG_LOG("a2dpProfileHandler A2DP_INTERNAL_OPEN_RES");
        a2dpStreamOpenResponse((const A2DP_INTERNAL_MEDIA_OPEN_RES_T *) message);
        break;

    case A2DP_INTERNAL_MEDIA_START_REQ:
        DEBUG_LOG("a2dpProfileHandler A2DP_INTERNAL_START_REQ");
        a2dpStreamStart((const A2DP_INTERNAL_MEDIA_START_REQ_T *) message);
        break;
        
    case A2DP_INTERNAL_MEDIA_START_RES:
        DEBUG_LOG("a2dpProfileHandler A2DP_INTERNAL_START_RES");
        a2dpStreamStartResponse((const A2DP_INTERNAL_MEDIA_START_RES_T *) message);
        break;
        
    case A2DP_INTERNAL_MEDIA_SUSPEND_REQ:
        DEBUG_LOG("a2dpProfileHandler A2DP_INTERNAL_MEDIA_SUSPEND_REQ");
        a2dpStreamSuspend((const A2DP_INTERNAL_MEDIA_SUSPEND_REQ_T *) message);
        break;

    case A2DP_INTERNAL_MEDIA_CLOSE_REQ:
        DEBUG_LOG("a2dpProfileHandler A2DP_INTERNAL_MEDIA_CLOSE_REQ");
        a2dpStreamRelease((const A2DP_INTERNAL_MEDIA_CLOSE_REQ_T *) message);
        break;
        
    case A2DP_INTERNAL_MEDIA_RECONFIGURE_REQ:
        DEBUG_LOG("a2dpProfileHandler A2DP_INTERNAL_MEDIA_RECONFIGURE_REQ");
        a2dpStreamReconfigure((const A2DP_INTERNAL_MEDIA_RECONFIGURE_REQ_T *) message);
        break;
        
    case A2DP_INTERNAL_MEDIA_AV_SYNC_DELAY_REQ:
        DEBUG_LOG("a2dpProfileHandler A2DP_INTERNAL_MEDIA_AV_SYNC_DELAY_REQ");
        a2dpStreamDelayReport(((const A2DP_INTERNAL_MEDIA_AV_SYNC_DELAY_REQ_T *)message)->device, 
                              ((const A2DP_INTERNAL_MEDIA_AV_SYNC_DELAY_REQ_T *)message)->delay);
        break;
        
    case A2DP_INTERNAL_MEDIA_AV_SYNC_DELAY_RES:
        DEBUG_LOG("a2dpProfileHandler A2DP_INTERNAL_MEDIA_AV_SYNC_DELAY_RES");
        a2dpStreamDelayReport(((const A2DP_INTERNAL_MEDIA_AV_SYNC_DELAY_RES_T *)message)->device, 
                              ((const A2DP_INTERNAL_MEDIA_AV_SYNC_DELAY_RES_T *)message)->delay);
        break;
        
    case A2DP_INTERNAL_LINKLOSS_TIMEOUT_BASE + 0:
    case A2DP_INTERNAL_LINKLOSS_TIMEOUT_BASE + 1:
    case A2DP_INTERNAL_LINKLOSS_TIMEOUT_BASE + 2:
    case A2DP_INTERNAL_LINKLOSS_TIMEOUT_BASE + 3:
    case A2DP_INTERNAL_LINKLOSS_TIMEOUT_BASE + 4:
    case A2DP_INTERNAL_LINKLOSS_TIMEOUT_BASE + 5:
    case A2DP_INTERNAL_LINKLOSS_TIMEOUT_BASE + 6:
        DEBUG_LOG_INFO("a2dpProfileHandler A2DP_INTERNAL_LINKLOSS_TIMEOUT");
        a2dpHandleL2capLinklossTimeout(id);
        break;
    
    case A2DP_INTERNAL_CLIENT_RSP_TIMEOUT_BASE + 0:
    case A2DP_INTERNAL_CLIENT_RSP_TIMEOUT_BASE + 1:
    case A2DP_INTERNAL_CLIENT_RSP_TIMEOUT_BASE + 2:
    case A2DP_INTERNAL_CLIENT_RSP_TIMEOUT_BASE + 3:
    case A2DP_INTERNAL_CLIENT_RSP_TIMEOUT_BASE + 4:
    case A2DP_INTERNAL_CLIENT_RSP_TIMEOUT_BASE + 5:
    case A2DP_INTERNAL_CLIENT_RSP_TIMEOUT_BASE + 6:
        DEBUG_LOG_INFO("a2dpProfileHandler A2DP_INTERNAL_CLIENT_RSP_TIMEOUT");
        a2dpHandleInternalClientRspTimeout(id);
        break;
    
    case A2DP_INTERNAL_REMOTE_CMD_TIMEOUT_BASE + 0:
    case A2DP_INTERNAL_REMOTE_CMD_TIMEOUT_BASE + 1:
    case A2DP_INTERNAL_REMOTE_CMD_TIMEOUT_BASE + 2:
    case A2DP_INTERNAL_REMOTE_CMD_TIMEOUT_BASE + 3:
    case A2DP_INTERNAL_REMOTE_CMD_TIMEOUT_BASE + 4:
    case A2DP_INTERNAL_REMOTE_CMD_TIMEOUT_BASE + 5:
    case A2DP_INTERNAL_REMOTE_CMD_TIMEOUT_BASE + 6:
        DEBUG_LOG_INFO("a2dpProfileHandler A2DP_INTERNAL_REMOTE_CMD_TIMEOUT");
        a2dpHandleInternalRemoteCmdTimeout(id);
        break;
    
    case A2DP_INTERNAL_WATCHDOG_BASE + 0:
    case A2DP_INTERNAL_WATCHDOG_BASE + 1:
    case A2DP_INTERNAL_WATCHDOG_BASE + 2:
    case A2DP_INTERNAL_WATCHDOG_BASE + 3:
    case A2DP_INTERNAL_WATCHDOG_BASE + 4:
    case A2DP_INTERNAL_WATCHDOG_BASE + 5:
    case A2DP_INTERNAL_WATCHDOG_BASE + 6:
        DEBUG_LOG_INFO("a2dpProfileHandler A2DP_INTERNAL_WATCHDOG_IND");
        a2dpHandleInternalWatchdogTimeout(id);
        break;

#if 0
    case A2DP_INTERNAL_RECONFIGURE_REQ:
        DEBUG_LOG("a2dpProfileHandler A2DP_INTERNAL_RECONFIGURE_REQ");
        a2dpHandleReconfigureReq((A2DP_INTERNAL_RECONFIGURE_REQ_T *) message);
        break;
#endif

#if 0
    case A2DP_INTERNAL_SEND_CODEC_PARAMS_REQ:
        DEBUG_LOG("a2dpProfileHandler A2DP_INTERNAL_SEND_CODEC_PARAMS_REQ");
        a2dpSendCodecAudioParams();
        if (((A2DP_INTERNAL_SEND_CODEC_PARAMS_REQ_T *) message)->send_reconfigure_message)
            a2dpSendReconfigureCfm(a2dp_success);
        break;
#endif

#if 0
    case A2DP_INTERNAL_GET_CAPS_TIMEOUT_IND:
        DEBUG_LOG("a2dpProfileHandler A2DP_INTERNAL_GET_CAPS_TIMEOUT_IND");
        a2dpGetCapsTimeout();
        break;
#endif

    default:
        switch(id)
        {
        case CL_SDP_REGISTER_CFM:
            DEBUG_LOG("a2dpProfileHandler CL_SDP_REGISTER_CFM");
            a2dpHandleSdpRegisterCfm((const CL_SDP_REGISTER_CFM_T *) message);
            break;

        case CL_L2CAP_REGISTER_CFM:
            DEBUG_LOG("a2dpProfileHandler CL_L2CAP_REGISTER_CFM");
            a2dpHandleL2capRegisterCfm((const CL_L2CAP_REGISTER_CFM_T *) message);
            break;

        case CL_L2CAP_CONNECT_IND:
            DEBUG_LOG("a2dpProfileHandler CL_L2CAP_CONNECT_IND");
            a2dpHandleL2capConnectInd((const CL_L2CAP_CONNECT_IND_T *) message);
            break;

        case CL_L2CAP_CONNECT_CFM:
            DEBUG_LOG("a2dpProfileHandler CL_L2CAP_CONNECT_CFM");
            a2dpHandleL2capConnectCfm((const CL_L2CAP_CONNECT_CFM_T *) message);
            break;

        case CL_L2CAP_DISCONNECT_IND:
            DEBUG_LOG("a2dpProfileHandler CL_L2CAP_DISCONNECT_IND");
            a2dpHandleL2capDisconnect(((const CL_L2CAP_DISCONNECT_IND_T *)message)->sink, ((const CL_L2CAP_DISCONNECT_IND_T *)message)->status);
            ConnectionL2capDisconnectResponse(((const CL_L2CAP_DISCONNECT_IND_T *)message)->identifier, ((const CL_L2CAP_DISCONNECT_IND_T *)message)->sink);
            break;

        case CL_L2CAP_DISCONNECT_CFM:
            DEBUG_LOG("a2dpProfileHandler CL_L2CAP_DISCONNECT_CFM");
            a2dpHandleL2capDisconnect(((const CL_L2CAP_DISCONNECT_CFM_T *)message)->sink, ((const CL_L2CAP_DISCONNECT_CFM_T *)message)->status);
            break;

        case CL_SM_ENCRYPTION_CHANGE_IND:
            DEBUG_LOG("a2dpProfileHandler CL_SM_ENCRYPTION_CHANGE_IND");
            /* We have received an indication that the encryption status of the sink has changed */
            sendEncryptionChangeInd((const CL_SM_ENCRYPTION_CHANGE_IND_T *) message);
            break;

        case CL_DM_ROLE_CFM:
        case CL_SM_ENCRYPTION_KEY_REFRESH_IND:
        case CL_L2CAP_TIMEOUT_IND:
            break;

        case CL_SDP_SERVICE_SEARCH_ATTRIBUTE_CFM:
            a2dpHandleSdpServiceSearchAttributeCfm ((const CL_SDP_SERVICE_SEARCH_ATTRIBUTE_CFM_T *)message);
            break;

        default:
            switch(id)
            {
            case MESSAGE_MORE_DATA:
                DEBUG_LOG_VERBOSE("a2dpProfileHandler MESSAGE_MORE_DATA");
                /* Data has arrived on the signalling channel */
                a2dpHandleSignalPacket( a2dpFindDeviceFromSink( StreamSinkFromSource(((const MessageMoreData *)message)->source) ));
                break;

            case MESSAGE_MORE_SPACE:
            case MESSAGE_STREAM_DISCONNECT:
            case MESSAGE_SOURCE_EMPTY:
                break;

            default:
                handleUnexpected(id);
                break;
            }
        }
    }
}

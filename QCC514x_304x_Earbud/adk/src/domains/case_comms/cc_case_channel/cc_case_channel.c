/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    case_comms
\brief      Case channel handling.
*/
/*! \addtogroup case_comms
@{
*/

#include "cc_case_channel.h"
#include "cc_protocol.h"
#include "cc_with_case.h"
#include "cc_with_earbuds.h"

#include <state_of_charge.h>
#include <phy_state.h>
#include <multidevice.h>
#include <bt_device.h>
#include <system_reboot.h>
#include <power_manager.h>
#include <local_addr.h>
#include <ui.h>

#include <logging.h>
#include <panic.h>

#ifdef INCLUDE_CASE_COMMS

#pragma unitcodesection KEEP_PM

/*! Definition of the fields in the #CASE_CHANNEL_MID_CASE_STATUS message. */
/*! @{ */
#define CASE_STATUS_MIN_SIZE                (1)
#define CASE_STATUS_SIZE_INC_BATTERY        (4)
#define CASE_STATUS_CASE_INFO_OFFSET        (0)
#define CASE_STATUS_CASE_BATT_OFFSET        (1)
#define CASE_STATUS_LEFT_BATT_OFFSET        (2)
#define CASE_STATUS_RIGHT_BATT_OFFSET       (3)
#define CASE_STATUS_CASE_INFO_LID_MASK      (0x1 << 0)
#define CASE_STATUS_CASE_INFO_CC_MASK       (0x1 << 1)
/*! @} */

/*! Two types of status request can be sent to an Earbud
        - Simple request for Earbud status
        - Info request for status of a specific piece of information

    Both requests are made using the CASE_CHANNEL_MID_EARBUD_STATUS_REQ message.
    
    A simple request specifies no payload to the request and is identified by
    the MID type only.
    
    An info request supplies an additional single payload byte with the info type
    for which status is requested.
 */
/*! Definition of the fields in the #CASE_CHANNEL_MID_EARBUD_STATUS_REQ message which
    specifies an info type for which status is requested. */
/*! @{ */
#define EARBUD_INFO_REQ_SIZE                (1)
#define EARBUD_INFO_REQ_TYPE_OFFSET         (0)
/*! @} */

/*! Definition of the fields in the simple #CASE_CHANNEL_MID_EARBUD_STATUS message. */
/*! @{ */
#define EARBUD_STATUS_SIZE                  (2)
#define EARBUD_STATUS_FLAGS_OFFSET          (0)
#define EARBUD_STATUS_FLAGS_PP_MASK         (0x1 << 0)
#define EARBUD_STATUS_FLAGS_INFO_MASK       (0x1 << 7)
#define EARBUD_STATUS_BATT_OFFSET           (1)
#define EARBUD_STATUS_INFO_VALUE_MASK       (0x7f)
/*! @} */

/*! Definition of the fields in the #CASE_CHANNEL_MID_EARBUD_STATUS with info message. */
/*! @{ */
#define EARBUD_INFO_HEADER_OFFSET           (0)
#define EARBUD_INFO_PAYLOAD_OFFSET          (1)
/*! @} */

/*! Size of Earbud info status messages. */
/*! @{ */
#define EARBUD_INFO_ADDRESS_SIZE            (7)
/*! @} */

/*! Definition of the fields in the #CASE_CHANNEL_MID_RESET message. */
/*! @{ */
#define EARBUD_RESET_SIZE                   (1)
#define EARBUD_RESET_PAYLOAD_OFFSET         (0)
#define EARBUD_RESET_TYPE_REBOOT            (0x0)
#define EARBUD_RESET_TYPE_FACTORY           (0x1)
/*! @} */

/*! Maximum size payload of case comms message. */
#define LOOPBACK_BUFFER_SIZE    376

/*! Definition of the common fields in the #CASE_CHANNEL_MID_COMMAND message. */
/*! @{ */
#define EARBUD_CMD_TYPE_SIZE                (1)
#define EARBUD_CMD_TYPE_OFFSET              (0)
/*! @} */

/*! Definition of the common fields in the #CASE_CHANNEL_MID_COMMAND_RESPONSE message. */
/*! @{ */
#define EARBUD_CMD_RESP_TYPE_SIZE           (1)
#define EARBUD_CMD_RESP_TYPE_OFFSET         (0)
/*! @} */

/*! Definition of the fields in the EB_CMD_PEER_PAIR type CASE_CHANNEL_MID_COMMAND message. */
/*! @{ */
#define EARBUD_CMD_TYPE_PEER_PAIR           (0)
#define EARBUD_CMD_PEER_PAIR_PAYLOAD_OFFSET (1)
#define EARBUD_CMD_PEER_PAIR_PAYLOAD_SIZE   (6)
#define EARBUD_CMD_PEER_PAIR_TOTAL_SIZE     (EARBUD_CMD_TYPE_SIZE + EARBUD_CMD_PEER_PAIR_PAYLOAD_SIZE)
/*! @} */

/*! Definition of the fields in the EB_CMD_PEER_PAIR response CASE_CHANNEL_MID_COMMAND_RESPONSE message. */
/*! @{ */
#define EARBUD_CMD_RESP_PEER_PAIR_PAYLOAD_SIZE      (1)
#define EARBUD_CMD_RESP_PEER_PAIR_PAYLOAD_OFFSET    (1)
#define EARBUD_CMD_RESP_PEER_PAIR_ACCEPTED          (0x1)
#define EARBUD_CMD_RESP_PEER_PAIR_REJECTED          (0x0)
#define EARBUD_CMD_RESP_PEER_PAIR_TOTAL_SIZE        (EARBUD_CMD_RESP_TYPE_SIZE + EARBUD_CMD_RESP_PEER_PAIR_PAYLOAD_SIZE)
/*! @} */

/*! Definition of the fields in the EB_CMD_SHIPPING_MODE type CASE_CHANNEL_MID_COMMAND message. */
/*! @{ */
#define EARBUD_CMD_SHIPPING_MODE_TOTAL_SIZE     (EARBUD_CMD_TYPE_SIZE)
/*! @} */

/*! Definition of the fields in the EB_CMD_SHIPPING_MODE response CASE_CHANNEL_MID_COMMAND_RESPONSE message. */
/*! @{ */
#define EARBUD_CMD_RESP_SHIPPING_MODE_PAYLOAD_SIZE      (1)
#define EARBUD_CMD_RESP_SHIPPING_MODE_PAYLOAD_OFFSET    (1)
#define EARBUD_CMD_RESP_SHIPPING_MODE_ACCEPTED          (0x1)
#define EARBUD_CMD_RESP_SHIPPING_MODE_REJECTED          (0x0)
#define EARBUD_CMD_RESP_SHIPPING_MODE_TOTAL_SIZE        (EARBUD_CMD_RESP_TYPE_SIZE + EARBUD_CMD_RESP_SHIPPING_MODE_PAYLOAD_SIZE)
/*! @} */

/*! \brief Types of case channel messages.
    \note These values are used in the protocol with the case
          and must remain in sync with case software.
*/
typedef enum
{
    /*! Case status message, including lid open/closed and battery levels */
    CASE_CHANNEL_MID_CASE_STATUS        = 0,

    /*! Earbud status message, can be either simple format or info format,
        depending on the type of CASE_CHANNEL_MID_EARBUD_STATUS_REQ received. */
    CASE_CHANNEL_MID_EARBUD_STATUS      = 1,

    /*! Reserved for future use. */
    CASE_CHANNEL_MID_RESET              = 2,

    /*! Request for Earbud to send a #CASE_CHANNEL_MID_EARBUD_STATUS.
        When no payload is supplied this message request a simple format
        CASE_CHANNEL_MID_EARBUD_STATUS response.
        If a payload is supplied specifying an info type (earbud_info_t)
        this message requests a CASE_CHANNEL_MID_EARBUD_STATUS response
        with info format providing the status of the requested info type. */
    CASE_CHANNEL_MID_EARBUD_STATUS_REQ  = 3,

    /*! Loopback.
        Test message, if case sends the message to Earbuds they respond with
        a loopback message containing the same contents as received. */
    CASE_CHANNEL_MID_LOOPBACK           = 4,

    /*! Command from Earbud to Case to perform requested operation. */
    CASE_CHANNEL_MID_COMMAND            = 5,

    /*! Response to command by Earbud to Case, for requested operation.
        Not all commands require a response. */
    CASE_CHANNEL_MID_COMMAND_RESPONSE   = 6,
} case_channel_mid_t;

/*! \brief Types of Earbud Info which may be requested. */
typedef enum
{
    /*! The programmed BT address of the device. */
    EB_INFO_BT_ADDRESS  = 0
} earbud_info_t;

/*! \brief Types of Earbud Command. */
typedef enum
{
    /*! Command the Earbud to Peer Pair with Address. */
    EB_CMD_PEER_PAIR        = 0,

    /*! Command the Earbud to enter shipping mode. */
    EB_CMD_SHIPPING_MODE    = 2,
} earbud_cmd_t;

/* Extract BT address from Casecomms messages into bdaddr. */
static void ccCaseChannel_GetBdAddrFromMsg(bdaddr* addr, const uint8* msg)
{
    addr->lap = (msg[0] | (msg[1] << 8) | (msg[2] << 16));
    addr->uap =  msg[3];
    addr->nap = (msg[4] | (msg[5] << 8));
}

/* Write BT address from bdaddr into casecomms message. */
static void ccCaseChannel_SetBdAddrInMsg(bdaddr* addr, uint8* msg)
{
    msg[0] =  addr->lap & 0xff;
    msg[1] = (addr->lap >> 8) & 0xff;
    msg[2] = (addr->lap << 16) & 0xff;
    msg[3] =  addr->uap & 0xff;
    msg[4] =  addr->nap & 0xff;
    msg[5] = (addr->nap >> 8) & 0xff;
}

/*! \brief Utility function to get local battery state in format expected.
    \return uint8 Local device battery and charging state in combined format.
    \note See description in #Case_GetCaseBatteryState() for format details.
*/
static uint8 ccCaseChannel_GetLocalBatteryState(void)
{
    uint8 battery_state = Soc_GetBatterySoc();
    phyState phy_state = appPhyStateGetState();

    /* if device is in the case it is assumed to be charging */
    if (phy_state == PHY_STATE_IN_CASE)
    {
        BATTERY_STATE_SET_CHARGING(battery_state);
    }

    return battery_state;
}

/*! \brief Build the flags field of the #CASE_CHANNEL_MID_EARBUD_STATUS message.
    \return uint8 Byte containing data for #EARBUD_STATUS_FLAGS_OFFSET field.
*/
static uint8 ccCaseChannel_EarbudStatusFlags(void)
{
    uint8 info = 0;

    /* only a single entry at the moment, indicating if the earbud
     * is paired with a peer. */
    if (BtDevice_IsPairedWithPeer())
    {
        info |= EARBUD_STATUS_FLAGS_PP_MASK;
    }

    return info;
}

/*! \brief Determine current lid state from case info.
    \param msg Case info byte of the case status message.
    \return case_lid_state_t Open or closed state of the case lid.
*/
static case_lid_state_t ccCaseChannel_LidState(uint8 msg)
{
    if (msg & CASE_STATUS_CASE_INFO_LID_MASK)
    {
        return CASE_LID_STATE_OPEN;
    }
    else
    {
        return CASE_LID_STATE_CLOSED;
    }
}

/*! \brief Handler for #CASE_CHANNEL_MID_CASE_STATUS message.
    \param msg Pointer to incoming message.
    \param length Size of the message in bytes.
    \note Parse case info message and generate events for case state change.
*/
static void ccCaseChannel_HandleCaseStatus(const uint8* msg, unsigned length)
{
    case_lid_state_t lid_state = CASE_LID_STATE_UNKNOWN;
    bool case_charger_connected = FALSE;
    uint8 peer_batt_state = 0;
    uint8 local_batt_state = 0;

    if (length >= CASE_STATUS_MIN_SIZE)
    {
        /* case info always present */
        lid_state = ccCaseChannel_LidState(msg[CASE_STATUS_CASE_INFO_OFFSET]);
        case_charger_connected = msg[CASE_STATUS_CASE_INFO_OFFSET] & CASE_STATUS_CASE_INFO_CC_MASK ? TRUE:FALSE;
        CcWithCase_LidEvent(lid_state);

        /* battery status info *may* be present */
        if (length >= CASE_STATUS_SIZE_INC_BATTERY)
        {
            peer_batt_state = Multidevice_IsLeft() ? msg[CASE_STATUS_RIGHT_BATT_OFFSET] :
                                                     msg[CASE_STATUS_LEFT_BATT_OFFSET];
            local_batt_state = ccCaseChannel_GetLocalBatteryState();

            CcWithCase_PowerEvent(msg[CASE_STATUS_CASE_BATT_OFFSET],
                            peer_batt_state, local_batt_state, 
                            case_charger_connected);
        }
    }
    else
    {
        DEBUG_LOG_WARN("ccCaseChannel_HandleCaseStatus invalid length %d", length);
    }
}

/* Get device programmed BT address and return to case. */
static void ccCaseChannel_HandleAddressInfoReq(void)
{
    uint8 status_msg[EARBUD_INFO_ADDRESS_SIZE];
    bdaddr addr;
    
    if (LocalAddr_GetProgrammedBtAddress(&addr))
    {
        /* set top bit to indicate info response and set info type */
        status_msg[EARBUD_INFO_HEADER_OFFSET] = EARBUD_STATUS_FLAGS_INFO_MASK | (EB_INFO_BT_ADDRESS & EARBUD_STATUS_INFO_VALUE_MASK);
        ccCaseChannel_SetBdAddrInMsg(&addr, &status_msg[EARBUD_INFO_PAYLOAD_OFFSET]);

        if (!CcProtocol_Transmit(CASECOMMS_DEVICE_CASE,
                    CASECOMMS_CID_CASE, CASE_CHANNEL_MID_EARBUD_STATUS,
                    status_msg, EARBUD_INFO_ADDRESS_SIZE))
        {
            DEBUG_LOG_WARN("ccCaseChannel_HandleAddressInfoReq TX rejected");
        }
    }
    else
    {
        DEBUG_LOG_ERROR("ccCaseChannel_HandleAddressInfoReq programmed BT address not available");
    }
}

static void ccCaseChannel_SendSimpleEarbudStatus(void)
{
    uint8 status_msg[EARBUD_STATUS_SIZE] = {0,0};

    status_msg[EARBUD_STATUS_FLAGS_OFFSET] = ccCaseChannel_EarbudStatusFlags();
    status_msg[EARBUD_STATUS_BATT_OFFSET] = ccCaseChannel_GetLocalBatteryState();

    if (!CcProtocol_Transmit(CASECOMMS_DEVICE_CASE,
                             CASECOMMS_CID_CASE, CASE_CHANNEL_MID_EARBUD_STATUS,
                             status_msg, EARBUD_STATUS_SIZE))
    {
        DEBUG_LOG_WARN("ccCaseChannel_SendSimpleEarbudStatus TX rejected");
    }
}

/*! Handler for #CASE_CHANNEL_MID_EARBUD_STATUS_REQ message. */
static void ccCaseChannel_HandleEarbudStatusReq(const uint8* msg, unsigned length)
{
    if (length < EARBUD_INFO_REQ_SIZE)
    {
        /* no payload with info type, so return simple status */
        ccCaseChannel_SendSimpleEarbudStatus();
    }
    else
    {
        /* process request for some earbud info */
        earbud_info_t info = msg[EARBUD_INFO_REQ_TYPE_OFFSET];
        switch (info)
        {
            case EB_INFO_BT_ADDRESS:
                ccCaseChannel_HandleAddressInfoReq();
                break;
            default:
                DEBUG_LOG_WARN("ccCaseChannel_HandleAddressInfoReq unsupported info type %d", info);
                break;
        }
    }
}

/* Handle incomnng info status with earbud BT address, extract address and pass to cc_with_earbuds. */
static void ccCaseChannel_HandleEBAddress(const uint8* msg, unsigned length, cc_dev_t source)
{
    bdaddr addr;

    UNUSED(length);

    ccCaseChannel_GetBdAddrFromMsg(&addr, &msg[EARBUD_INFO_PAYLOAD_OFFSET]);
    CcWithEarbuds_EarbudBtAddressRx(&addr, source);
}

/* Demux earbud info status types to handlers. */
static void ccCaseChannel_HandleEarbudInfoStatus(const uint8* msg, unsigned length, cc_dev_t source)
{
    earbud_info_t info_type = msg[EARBUD_INFO_HEADER_OFFSET] & EARBUD_STATUS_INFO_VALUE_MASK;

    switch (info_type)
    {
        case EB_INFO_BT_ADDRESS:
            ccCaseChannel_HandleEBAddress(msg, length, source); 
            break;
        default:
            DEBUG_LOG_ERROR("ccCaseChannel_HandleEarbudInfoStatus unsupported info type %d", info_type);
            break;
    }
}

/* Determine status type, simple or info. */
static void ccCaseChannel_HandleEarbudStatus(const uint8* msg, unsigned length, cc_dev_t source)
{
    uint8 earbud_battery_state = BATTERY_STATUS_UNKNOWN;
    bool peer_paired = FALSE;

    if (length >= EARBUD_STATUS_SIZE)
    {
        if ((msg[EARBUD_STATUS_FLAGS_OFFSET] & EARBUD_STATUS_FLAGS_INFO_MASK) != EARBUD_STATUS_FLAGS_INFO_MASK)
        { 
            /* info bit not set, treat as simple status message */
            peer_paired = (msg[EARBUD_STATUS_FLAGS_OFFSET] & EARBUD_STATUS_FLAGS_PP_MASK) ? TRUE : FALSE;
            earbud_battery_state = msg[EARBUD_STATUS_BATT_OFFSET];

            CcWithEarbuds_EarbudStatusRx(source, earbud_battery_state, peer_paired);
        }
        else
        {
            /* handle as status containing info */
            ccCaseChannel_HandleEarbudInfoStatus(msg, length, source);
        }
    }
    else
    {
        DEBUG_LOG_WARN("ccCaseChannel_HandleEarbudStatus invalid length %d", length);
    }
}
            
static void ccCaseChannel_HandleReset(const uint8* msg, unsigned length, cc_dev_t source_dev)
{
    if (length >= EARBUD_RESET_SIZE)
    {
        if (msg[EARBUD_RESET_PAYLOAD_OFFSET] == EARBUD_RESET_TYPE_FACTORY)
        {
            DEBUG_LOG_ALWAYS("ccCaseChannel_HandleReset factory reset requested by enum:cc_dev_t:%d", source_dev);
            Ui_InjectUiInput(ui_input_factory_reset_request);
        }
        else
        {
            DEBUG_LOG_ALWAYS("ccCaseChannel_HandleReset reset requested by enum:cc_dev_t:%d", source_dev);
            SystemReboot_Reboot();
        }
    }
    else
    {
        DEBUG_LOG_WARN("ccCaseChannel_HandleReset invalid length %d", length);
    }
}

static void ccCaseChannel_HandleLoopback(const uint8* msg, unsigned length, cc_dev_t source_dev)
{
    uint8 content[LOOPBACK_BUFFER_SIZE];
    uint16 length_ceil = length <= LOOPBACK_BUFFER_SIZE ? length : LOOPBACK_BUFFER_SIZE;

    if (source_dev == CASECOMMS_DEVICE_CASE)
    {
        memcpy(content, msg, length_ceil);

        DEBUG_LOG_VERBOSE("ccCaseChannel_HandleLoopback earbud");
        if (!CcProtocol_Transmit(CASECOMMS_DEVICE_CASE,
                                 CASECOMMS_CID_CASE, CASE_CHANNEL_MID_LOOPBACK,
                                 content, length_ceil))
        {
            DEBUG_LOG_WARN("ccCaseChannel_HandleLoopback TX rejected");
        }
    }
    else
    {
        DEBUG_LOG_VERBOSE("ccCaseChannel_HandleLoopback case");
        CcWithEarbuds_LoopbackRx(source_dev, content, length_ceil); 
    }
}

static void ccCaseChannel_HandlePeerPairCmd(const uint8* msg, unsigned length, cc_dev_t source_dev)
{
    bdaddr peer_addr;

    UNUSED(source_dev);

    DEBUG_LOG_VERBOSE("ccCaseChannel_HandlePeerPairCmd");
    
    if (length >= EARBUD_CMD_PEER_PAIR_TOTAL_SIZE)
    {
        ccCaseChannel_GetBdAddrFromMsg(&peer_addr, msg+EARBUD_CMD_PEER_PAIR_PAYLOAD_OFFSET);
        CcWithCase_PeerPairCmdRx(&peer_addr);
    }
    else
    {
        DEBUG_LOG_WARN("ccCaseChannel_HandlePeerPairCmd bad cmd length %d", length);
        /* Indicate to case that peer pairing will not be starting */ 
        CcCaseChannel_PeerPairCmdRespTx(FALSE);
    }
}

static void ccCaseChannel_HandleShippingModeCmd(const uint8* msg, unsigned length, cc_dev_t source_dev)
{
    UNUSED(msg);
    UNUSED(source_dev);

    DEBUG_LOG_VERBOSE("ccCaseChannel_HandleShippingModeCmd");

    if (length >= EARBUD_CMD_SHIPPING_MODE_TOTAL_SIZE)
    {
        CcWithCase_ShippingModeCmdRx();
    }
    else
    {
        DEBUG_LOG_VERBOSE("ccCaseChannel_HandleShippingModeCmd bad cmd length %d", length);
        /* indicate to case that shipping mode will not be entered */
        CcCaseChannel_ShippingModeCmdRespTx(FALSE);
    }
}

static void ccCaseChannel_HandleCommand(const uint8* msg, unsigned length, cc_dev_t source_dev)
{
    if (length >= EARBUD_CMD_TYPE_SIZE)
    {
        earbud_cmd_t cmd = msg[EARBUD_CMD_TYPE_OFFSET];
        switch (cmd)
        {
            case EB_CMD_PEER_PAIR:
                ccCaseChannel_HandlePeerPairCmd(msg, length, source_dev);
                break;

            case EB_CMD_SHIPPING_MODE:
                ccCaseChannel_HandleShippingModeCmd(msg, length, source_dev);
                break;

            default:
                DEBUG_LOG_WARN("ccCaseChannel_HandleCommand unsupported cmd type %d", cmd);
                break;
        }
    }
}

static void ccCaseChannel_HandlePeerPairCmdResp(const uint8* msg, unsigned length, cc_dev_t source_dev)
{
    if (length >= EARBUD_CMD_RESP_PEER_PAIR_TOTAL_SIZE)
    {
        bool resp = msg[EARBUD_CMD_RESP_PEER_PAIR_PAYLOAD_OFFSET] == EARBUD_CMD_RESP_PEER_PAIR_ACCEPTED ? TRUE : FALSE;
        CcWithEarbuds_PeerPairResponseRx(source_dev, resp);
    }
}

static void ccCaseChannel_HandleShippingModeCmdResp(const uint8* msg, unsigned length, cc_dev_t source_dev)
{
    if (length >= EARBUD_CMD_RESP_SHIPPING_MODE_TOTAL_SIZE)
    {
        bool resp = msg[EARBUD_CMD_RESP_SHIPPING_MODE_PAYLOAD_OFFSET] == EARBUD_CMD_RESP_SHIPPING_MODE_ACCEPTED ? TRUE : FALSE;
        CcWithEarbuds_ShippingModeResponseRx(source_dev, resp);
    }
}

static void ccCaseChannel_HandleCommandResponse(const uint8* msg, unsigned length, cc_dev_t source_dev)
{
    if (length >= EARBUD_CMD_RESP_TYPE_SIZE)
    {
        earbud_cmd_t cmd = msg[EARBUD_CMD_RESP_TYPE_OFFSET];
        switch (cmd)
        {
            case EB_CMD_PEER_PAIR:
                ccCaseChannel_HandlePeerPairCmdResp(msg, length, source_dev);
                break;

            case EB_CMD_SHIPPING_MODE:
                ccCaseChannel_HandleShippingModeCmdResp(msg, length, source_dev);
                break;

            default:
                DEBUG_LOG_WARN("ccCaseChannel_HandleCommandResponse unsupported cmd response type %d", cmd);
                break;
        }
    }
}

static void CcCaseChannel_HandleTxStatus(cc_tx_status_t status, unsigned mid)
{
    DEBUG_LOG_V_VERBOSE("CcCaseChannel_HandleTxStatus sts enum:cc_tx_status_t:%d mid:%d", status, mid);

    switch (mid)
    {
        /* messages transmitted by the case, send status to cc_with_earbuds */
        case CASE_CHANNEL_MID_CASE_STATUS:
        case CASE_CHANNEL_MID_RESET:
        case CASE_CHANNEL_MID_EARBUD_STATUS_REQ:
        case CASE_CHANNEL_MID_LOOPBACK:
        case CASE_CHANNEL_MID_COMMAND:
            CcWithEarbuds_TransmitStatusRx(status, mid);
            break;

        /* messages transmitted by Earbuds, no current need for Earbuds (CcWithCase) to
           receive status, all transmits are handled by cc_case_channel in
           response to messages initiated by the case. */
        case CASE_CHANNEL_MID_EARBUD_STATUS:
        case CASE_CHANNEL_MID_COMMAND_RESPONSE:
            break;

        default:
            DEBUG_LOG_WARN("CcCaseChannel_HandleTxStatus unsupported MID:%d", mid);
            break;
    }
}

static void CcCaseChannel_HandleRxInd(unsigned mid, const uint8* msg, unsigned length, cc_dev_t source_dev)
{
    switch (mid)
    {
        case CASE_CHANNEL_MID_CASE_STATUS:
            ccCaseChannel_HandleCaseStatus(msg, length);
            break;
        case CASE_CHANNEL_MID_EARBUD_STATUS_REQ:
            ccCaseChannel_HandleEarbudStatusReq(msg, length);
            break;
        case CASE_CHANNEL_MID_EARBUD_STATUS:
            ccCaseChannel_HandleEarbudStatus(msg, length, source_dev);
            break;
        case CASE_CHANNEL_MID_RESET:
            ccCaseChannel_HandleReset(msg, length, source_dev);
            break;
        case CASE_CHANNEL_MID_LOOPBACK:
            ccCaseChannel_HandleLoopback(msg, length, source_dev);
            break;
        case CASE_CHANNEL_MID_COMMAND:
            ccCaseChannel_HandleCommand(msg, length, source_dev);
            break;
        case CASE_CHANNEL_MID_COMMAND_RESPONSE:
            ccCaseChannel_HandleCommandResponse(msg, length, source_dev);
            break;
        default:
            DEBUG_LOG_WARN("CcCaseChannel_HandleRxInd unsupported mid %d", mid);
            break;
    }
}

void CcCaseChannel_Init(void)
{
    cc_chan_config_t cfg;

    cfg.cid = CASECOMMS_CID_CASE;
    cfg.tx_sts = CcCaseChannel_HandleTxStatus;
    cfg.rx_ind = CcCaseChannel_HandleRxInd;

    CcProtocol_RegisterChannel(&cfg);
}

bool CcCaseChannel_EarbudStatusReqTx(cc_dev_t dest)
{
    if (   (dest != CASECOMMS_DEVICE_LEFT_EB)
        && (dest != CASECOMMS_DEVICE_RIGHT_EB))
    {
        DEBUG_LOG_ERROR("CcCaseChannel_EarbudStatusReqTx bad dest enum:cc_dev_t:%d", dest);
        return FALSE;
    }

    /* response is expected, so use standard transmit */
    return CcProtocol_Transmit(dest, CASECOMMS_CID_CASE,
                               CASE_CHANNEL_MID_EARBUD_STATUS_REQ,
                               NULL, 0);
}

bool CcCaseChannel_CaseStatusTx(cc_dev_t dest, case_status_config_t* config)
{
    uint8 msg[CASE_STATUS_SIZE_INC_BATTERY];
    unsigned len = config->short_form ? CASE_STATUS_MIN_SIZE : CASE_STATUS_SIZE_INC_BATTERY;
    
    /* Valid message for either single earbud or broadcast to both earbuds */
    if (dest == CASECOMMS_DEVICE_CASE)
    {
        DEBUG_LOG_ERROR("CcCaseChannel_CaseStatusTx bad dest enum:cc_dev_t:%d", dest);
        return FALSE;
    }

    msg[CASE_STATUS_CASE_INFO_OFFSET] = (config->lid_open ? CASE_STATUS_CASE_INFO_LID_MASK : 0) |
                                        (config->charger_connected ? CASE_STATUS_CASE_INFO_CC_MASK : 0);

    if (!config->short_form)
    {
        msg[CASE_STATUS_CASE_BATT_OFFSET] = config->case_battery_state;
        msg[CASE_STATUS_LEFT_BATT_OFFSET] = config->left_earbud_battery_state;
        msg[CASE_STATUS_RIGHT_BATT_OFFSET] = config->right_earbud_battery_state;
    }
    
    /* no response expected, so use notification type transmit */
    return CcProtocol_TransmitNotification(dest, CASECOMMS_CID_CASE, CASE_CHANNEL_MID_CASE_STATUS,
                                           msg, len);
}

bool CcCaseChannel_EarbudResetTx(cc_dev_t dest, bool factory_reset)
{
    /* default reset type to be reboot */
    uint8 msg[EARBUD_RESET_SIZE] = {EARBUD_RESET_TYPE_REBOOT};

    /* Valid message for either single earbud or broadcast to both earbuds */
    if (dest == CASECOMMS_DEVICE_CASE)
    {
        DEBUG_LOG_ERROR("CcCaseChannel_EarbudResetTx bad dest enum:cc_dev_t:%d", dest);
        return FALSE;
    }

    /* override reboot type to factory if required */
    if (factory_reset)
    {
        msg[EARBUD_RESET_PAYLOAD_OFFSET] = EARBUD_RESET_TYPE_FACTORY;
    }
    
    /* no response expected, so use notification type transmit */
    return CcProtocol_TransmitNotification(dest, CASECOMMS_CID_CASE, CASE_CHANNEL_MID_RESET,
                                           msg, EARBUD_RESET_SIZE);
}

bool CcCaseChannel_EarbudBtAddressInfoReqTx(cc_dev_t dest)
{
    uint8 msg[EARBUD_INFO_REQ_SIZE];

    msg[EARBUD_INFO_REQ_TYPE_OFFSET] = EB_INFO_BT_ADDRESS;

    if (   (dest != CASECOMMS_DEVICE_LEFT_EB)
        && (dest != CASECOMMS_DEVICE_RIGHT_EB))
    {
        DEBUG_LOG_ERROR("CcCaseChannel_EarbudBtAddressInfoReqTx bad dest enum:cc_dev_t:%d", dest);
        return FALSE;
    }

    /* response is expected, so use standard transmit */
    return CcProtocol_Transmit(dest, CASECOMMS_CID_CASE,
                               CASE_CHANNEL_MID_EARBUD_STATUS_REQ,
                               msg, EARBUD_INFO_REQ_SIZE);
}

bool CcCaseChannel_LoopbackTx(cc_dev_t dest, uint8* data, unsigned len)
{
    if (   (dest != CASECOMMS_DEVICE_LEFT_EB)
        && (dest != CASECOMMS_DEVICE_RIGHT_EB))
    {
        DEBUG_LOG_ERROR("CcCaseChannel_EarbudBtAddressInfoReqTx bad dest enum:cc_dev_t:%d", dest);
        return FALSE;
    }

    /* response is expected, so use standard transmit */
    return CcProtocol_Transmit(dest, CASECOMMS_CID_CASE,
                               CASE_CHANNEL_MID_LOOPBACK,
                               data, len);
}

bool CcCaseChannel_PeerPairCmdTx(cc_dev_t dest, bdaddr* addr)
{
    uint8 msg[EARBUD_CMD_PEER_PAIR_TOTAL_SIZE];

    if (   (dest != CASECOMMS_DEVICE_LEFT_EB)
        && (dest != CASECOMMS_DEVICE_RIGHT_EB))
    {
        DEBUG_LOG_ERROR("CcCaseChannel_EarbudBtAddressInfoReqTx bad dest enum:cc_dev_t:%d", dest);
        return FALSE;
    }

    msg[EARBUD_CMD_TYPE_OFFSET] = EARBUD_CMD_TYPE_PEER_PAIR;
    ccCaseChannel_SetBdAddrInMsg(addr, &msg[EARBUD_CMD_PEER_PAIR_PAYLOAD_OFFSET]);

    /* response is expected, so use standard transmit */
    return CcProtocol_Transmit(dest, CASECOMMS_CID_CASE,
                               CASE_CHANNEL_MID_COMMAND,
                               msg, EARBUD_CMD_PEER_PAIR_TOTAL_SIZE);
}

bool CcCaseChannel_PeerPairCmdRespTx(bool peer_pair_started)
{
    uint8 msg[EARBUD_CMD_RESP_PEER_PAIR_TOTAL_SIZE];

    msg[EARBUD_CMD_RESP_TYPE_OFFSET] = EB_CMD_PEER_PAIR;
    msg[EARBUD_CMD_RESP_PEER_PAIR_PAYLOAD_OFFSET] = peer_pair_started ? EARBUD_CMD_RESP_PEER_PAIR_ACCEPTED
                                                                      : EARBUD_CMD_RESP_PEER_PAIR_REJECTED;

    return CcProtocol_TransmitNotification(CASECOMMS_DEVICE_CASE, CASECOMMS_CID_CASE,
                                           CASE_CHANNEL_MID_COMMAND_RESPONSE,
                                           msg, EARBUD_CMD_RESP_PEER_PAIR_TOTAL_SIZE);
}

bool CcCaseChannel_ShippingModeCmdTx(cc_dev_t dest)
{
    uint8 msg[EARBUD_CMD_SHIPPING_MODE_TOTAL_SIZE];

    if (   (dest != CASECOMMS_DEVICE_LEFT_EB)
        && (dest != CASECOMMS_DEVICE_RIGHT_EB))
    {
        DEBUG_LOG_ERROR("CcCaseChannel_ShippingModeCmdTx bad dest enum:cc_dev_t:%d", dest);
        return FALSE;
    }

    msg[EARBUD_CMD_TYPE_OFFSET] = EB_CMD_SHIPPING_MODE;

    /* response is expected, so use standard transmit */
    return CcProtocol_Transmit(dest, CASECOMMS_CID_CASE,
                               CASE_CHANNEL_MID_COMMAND,
                               msg, EARBUD_CMD_SHIPPING_MODE_TOTAL_SIZE);
}

bool CcCaseChannel_ShippingModeCmdRespTx(bool cmd_accepted)
{
    uint8 msg[EARBUD_CMD_RESP_SHIPPING_MODE_TOTAL_SIZE];

    msg[EARBUD_CMD_RESP_TYPE_OFFSET] = EB_CMD_SHIPPING_MODE;
    msg[EARBUD_CMD_RESP_SHIPPING_MODE_PAYLOAD_OFFSET] = cmd_accepted ? EARBUD_CMD_RESP_SHIPPING_MODE_ACCEPTED
                                                                     : EARBUD_CMD_RESP_SHIPPING_MODE_REJECTED;

    return CcProtocol_TransmitNotification(CASECOMMS_DEVICE_CASE, CASECOMMS_CID_CASE,
                                           CASE_CHANNEL_MID_COMMAND_RESPONSE,
                                           msg, EARBUD_CMD_RESP_SHIPPING_MODE_TOTAL_SIZE);
}

#endif /* INCLUDE_CASE_COMMS */
/*! @} End of group documentation */

/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    case_domain
\brief      Case channel handling.
*/
/*! \addtogroup case_domain
@{
*/

#include "case.h"
#include "case_private.h"
#include "case_comms.h"
#include "case_comms_case_channel.h"

#include <battery_monitor.h>
#include <phy_state.h>
#include <multidevice.h>
#include <bt_device.h>

#include <logging.h>

#ifdef INCLUDE_CASE

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

/*! Definition of the fields in the #CASE_CHANNEL_MID_EARBUD_STATUS message. */
/*! @{ */
#define EARBUD_STATUS_SIZE                  (2)
#define EARBUD_STATUS_INFO_OFFSET           (0)
#define EARBUD_STATUS_BATT_OFFSET           (1)
#define EARBUD_STATUS_INFO_PP_MASK          (0x1 << 0)
/*! @} */

/*! \brief Types of case channel messages.
    \note These values are used in the protocol with the case
          and must remain in sync with case software.
*/
typedef enum
{
    /*! Case status message, including lid open/closed and battery levels */
    CASE_CHANNEL_MID_CASE_STATUS        = 0,

    /*! Earbud status message. */
    CASE_CHANNEL_MID_EARBUD_STATUS      = 1,

    /*! Reserved for future use. */
    CASE_CHANNEL_MID_RESERVED           = 2,

    /*! Request for Earbud to send a #CASE_CHANNEL_MID_EARBUD_STATUS */
    CASE_CHANNEL_MID_EARBUD_STATUS_REQ  = 3,
} case_channel_mid_t;

/*! \brief Utility function to get local battery state in format expected.
    \return uint8 Local device battery and charging state in combined format.
    \note See description in #Case_GetCaseBatteryState() for format details.
*/
static uint8 case_GetLocalBatteryState(void)
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

/*! \brief Build the info field of the #CASE_CHANNEL_MID_EARBUD_STATUS message.
    \return uint8 Byte containing data for #EARBUD_STATUS_INFO_OFFSET field.
*/
static uint8 case_EarbudStatusInfo(void)
{
    uint8 info = 0;

    /* only a single entry at the moment, indicating if the earbud
     * is paired with a peer. */
    if (BtDevice_IsPairedWithPeer())
    {
        info |= EARBUD_STATUS_INFO_PP_MASK;
    }

    return info;
}

/*! \brief Determine current lid state from case info.
    \param msg Case info byte of the case status message.
    \return case_lid_state_t Open or closed state of the case lid.
*/
static case_lid_state_t case_CaseChannelGetLidState(uint8 msg)
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
static void case_CaseChannelCaseStatus(uint8* msg, unsigned length)
{
    case_lid_state_t lid_state = CASE_LID_STATE_UNKNOWN;
    bool case_charger_connected = FALSE;
    uint8 peer_batt_state = 0;
    uint8 local_batt_state = 0;

    if (length >= CASE_STATUS_MIN_SIZE)
    {
        /* case info always present */
        lid_state = case_CaseChannelGetLidState(msg[CASE_STATUS_CASE_INFO_OFFSET]);
        case_charger_connected = msg[CASE_STATUS_CASE_INFO_OFFSET] & CASE_STATUS_CASE_INFO_CC_MASK ? TRUE:FALSE;
        Case_LidEvent(lid_state);

        /* battery status info *may* be present */
        if (length >= CASE_STATUS_SIZE_INC_BATTERY)
        {
            peer_batt_state = Multidevice_IsLeft() ? msg[CASE_STATUS_RIGHT_BATT_OFFSET] :
                                                     msg[CASE_STATUS_LEFT_BATT_OFFSET];
            local_batt_state = case_GetLocalBatteryState();

            Case_PowerEvent(msg[CASE_STATUS_CASE_BATT_OFFSET],
                            peer_batt_state, local_batt_state, 
                            case_charger_connected);
        }
    }
    else
    {
        DEBUG_LOG_WARN("case_CaseChannelCaseStatus invalid length %d", length);
    }
}

/*! \brief Handler for #CASE_CHANNEL_MID_EARBUD_STATUS_REQ message.
    \param msg Pointer to incoming message.
    \param length Size of the message in bytes.
    \note Send a #CASE_CHANNEL_MID_EARBUD_STATUS message to the case.
*/
static void case_CaseChannelEarbudStatusReq(void)
{
    uint8 status_msg[EARBUD_STATUS_SIZE] = {0,0};

    status_msg[EARBUD_STATUS_INFO_OFFSET] = case_EarbudStatusInfo();
    status_msg[EARBUD_STATUS_BATT_OFFSET] = case_GetLocalBatteryState();

    if (Case_CommsTransmit(CASECOMMS_DEVICE_CASE,
                CASECOMMS_CID_CASE, CASE_CHANNEL_MID_EARBUD_STATUS,
                status_msg, EARBUD_STATUS_SIZE))
    {
        DEBUG_LOG_VERBOSE("case_CaseChannelEarbudStatusReq TX accepted");
    }
    else
    {
        DEBUG_LOG_WARN("case_CaseChannelEarbudStatusReq TX rejected");
    }
}

void Case_CommsCaseChannelHandleTransmitStatus(case_comms_tx_status_t status)
{
    DEBUG_LOG_VERBOSE("Case_CommsCaseChannelHandleTransmitStatus sts enum:case_comms_tx_status_t:%d", status);
}

void Case_CommsCaseChannelHandleMessage(unsigned mid, uint8* msg, unsigned length)
{
    switch (mid)
    {
        case CASE_CHANNEL_MID_CASE_STATUS:
            case_CaseChannelCaseStatus(msg, length);
            break;
        case CASE_CHANNEL_MID_EARBUD_STATUS_REQ:
            /* message has no payload */
            case_CaseChannelEarbudStatusReq();
            break;
        default:
            DEBUG_LOG_WARN("case_CommsCaseChannelHandleMessage unsupported mid %d", mid);
            break;
    }
}

#endif /* INCLUDE_CASE */
/*! @} End of group documentation */

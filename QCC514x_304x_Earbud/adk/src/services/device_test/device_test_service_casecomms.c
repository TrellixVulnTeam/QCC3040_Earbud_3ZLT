/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    device_test_service
\brief      Device test service interface to case communications, to send and receive messages
            over the DTS case comms channel.

            DTS has 2 sub-channels which are multiplexed over the DTS case comms channel.
            The sub-channels are 'management' and 'tunnel' and are identified by the case
            comms message ID (MID) used in DTS case comms channel messages.

            The management sub-channel is used exclusively by the case to communicate
            with DTS on the Earbuds.

            The tunnel sub-channel is used by external hosts, to tunnel standard DTS
            messaging through the case to the Earbuds. This is analogous to standard
            DTS messaging direct to the Earbuds over other supported DTS transports
            such as SPP.
*/

#include "device_test_service_casecomms.h"
#include "device_test_service.h"
#include "device_test_service_data.h"

#include <device_test_service_common.h>

#include <logging.h>
#include <panic.h>

#if defined(INCLUDE_DEVICE_TEST_SERVICE) && defined(INCLUDE_CASE_COMMS)

#include <cc_protocol.h>

/* Send current mode back to the case. */
static void deviceTestServiceCasecomms_HandleGetMode(void)
{
    uint8 current_mode[DTS_CC_MAN_MSG_MODE_SIZE];
    current_mode[DTS_CC_MAN_MSG_TYPE_OFFSET] = DTS_CC_MAN_MSG_MODE;
    current_mode[DTS_CC_MAN_MSG_MODE_OFFSET] = DeviceTestService_TestModeType();
    CcProtocol_Transmit(CASECOMMS_DEVICE_CASE, CASECOMMS_CID_DTS, DTS_CC_MID_MANAGEMENT,
                        current_mode, DTS_CC_MAN_MSG_MODE_SIZE);
}

/* Save the requested mode and set the preserve flag. */
static void deviceTestServiceCasecomms_HandlePreserveMode(const uint8* msg, unsigned length)
{
    if (length >= DTS_CC_MAN_MSG_PRESERVE_MODE_SIZE)
    {
        DeviceTestService_SaveTestMode(msg[DTS_CC_MAN_MSG_PRESERVE_MODE_OFFSET]);
        DeviceTestService_PreserveMode(TRUE);
    }
    else
    {
        DEBUG_LOG_WARN("deviceTestServiceCasecomms_HandlePreserveMode bad length %d", length);
    }
}

/* Multiplex management sub-channel message type to handlers. */
static void deviceTestServiceCasecomms_HandleManChan(const uint8* msg, unsigned length, cc_dev_t source_dev)
{
    UNUSED(source_dev);

    if (length >= DTS_CC_MAN_MSG_MIN_SIZE)
    {
        switch (msg[DTS_CC_MAN_MSG_TYPE_OFFSET])
        {
            case DTS_CC_MAN_MSG_GET_MODE:
                deviceTestServiceCasecomms_HandleGetMode();
                break;
            case DTS_CC_MAN_MSG_PRESERVE_MODE:
                deviceTestServiceCasecomms_HandlePreserveMode(msg, length);
                break;
            default:
                DEBUG_LOG_WARN("deviceTestServiceCasecomms_HandlePreserveMode unsupported msg type %d", msg[DTS_CC_MAN_MSG_TYPE_OFFSET]);
                break;
        }
    }
    else
    {
        DEBUG_LOG_WARN("deviceTestServiceCasecomms_HandleManChan bad length %d", length);
    }
}

static void deviceTestServiceCasecomms_HandleRxInd(unsigned mid, const uint8* msg, unsigned length, cc_dev_t source_dev)
{
    if (mid == DTS_CC_MID_MANAGEMENT)
    {
        deviceTestServiceCasecomms_HandleManChan(msg, length, source_dev);
    }
    else
    {
        /* Pass tunnelled DTS command from host to input handling */
        DEBUG_LOG_WARN("deviceTestServiceCasecomms_HandleRxInd tunnel sub-channel not supported");
        Panic();
    }
}

static void deviceTestServiceCasecomms_HandleTxStatus(cc_tx_status_t status, unsigned mid)
{
    UNUSED(status);
    UNUSED(mid);
}

void DeviceTestServiceCasecomms_Init(void)
{
    cc_chan_config_t cfg;

    cfg.cid = CASECOMMS_CID_DTS;
    cfg.tx_sts = deviceTestServiceCasecomms_HandleTxStatus;
    cfg.rx_ind = deviceTestServiceCasecomms_HandleRxInd;

    CcProtocol_RegisterChannel(&cfg);
}

#endif /* INCLUDE_DEVICE_TEST_SERVICE && INCLUDE_CASE_COMMS */

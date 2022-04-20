/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    device_test_service
\brief      Common device test service(DTS) definitions for use in service and domain components.

            When DTS is used over case comms, both the DTS component at the service layer
            and the Case Comms component(s) in the domain layer require access to common types
            and definitions.
*/

#ifndef DEVICE_TEST_SERVICE_COMMON_H
#define DEVICE_TEST_SERVICE_COMMON_H

/*! Definition of common DTS management sub-channel fields. */
/*! @{ */
#define DTS_CC_MAN_MSG_MIN_SIZE             (1)
#define DTS_CC_MAN_MSG_TYPE_OFFSET          (0)
/*! @} */

/*! Definition of DTS_CC_MAN_MSG_MODE message fields. */
/*! @{ */
#define DTS_CC_MAN_MSG_MODE_SIZE            (2)
#define DTS_CC_MAN_MSG_MODE_OFFSET          (1)
/*! @} */

/*! Definition of DTS_CC_MAN_MSG_PRESERVE_MODE message fields. */
/*! @{ */
#define DTS_CC_MAN_MSG_PRESERVE_MODE_SIZE   (2)
#define DTS_CC_MAN_MSG_PRESERVE_MODE_OFFSET (1)
/*! @} */

/*! Types of mode in which DTS may be configured. */
typedef enum
{
    /*! DTS is not enabled. */
    DTS_MODE_DISABLED       = 0x0000,

    /*! DTS is enabled and will try to connect on startup. 
        This mode is expected to be used for production testing. */
    DTS_MODE_ENABLED        = 0x0001,

    /*! Device testing is enabled, but not the device test service.
        LE connctions will be enabled.

        This mode is expected to be used during production to halt
        application startup for other setup activities, but not
        for performing production testing. */
    DTS_MODE_ENABLED_IDLE   = 0x0002,

    /*! Device testing is enabled, but not the device test service.

      This mode is expected to be used during production to put
      the device into RF DUT mode only. */
    DTS_MODE_ENABLED_DUT    = 0x0003,
} device_test_service_mode_t;

/*! \brief Type of DTS message ID used over case comms.

    The DTS case comms channel has two sub channels, the channel is identified by
    the case comms message ID (MID) in the case comms header:
*/
typedef enum
{
    /*! DTS Management sub-channel, used by case firmware to communicate with DTS over case comms. */
    DTS_CC_MID_MANAGEMENT   = 0,
     
    /*! DTS Tunnel sub-channel, used by external host tunneling DTS to Earbuds through the case. This is
        equivalent to DTS over SPP direct to the Earbuds. */
    DTS_CC_MID_TUNNEL       = 1,
} dts_casecomms_mid_t;

/*! \brief Type of DTS Management sub-channel message.
*/
typedef enum
{
    /*! Get current DTS mode. */
    DTS_CC_MAN_MSG_GET_MODE = 0,
    
    /*! Currnet mode, sent in response to DTS_CC_MAN_MSG_GET_MODE. */
    DTS_CC_MAN_MSG_MODE = 1,
    
    /*! Command to set the DTS mode and preserve it over factory reset. */
    DTS_CC_MAN_MSG_PRESERVE_MODE = 2,
} dts_casecomms_man_t;

#endif /* DEVICE_TEST_SERVICE_COMMON_H */

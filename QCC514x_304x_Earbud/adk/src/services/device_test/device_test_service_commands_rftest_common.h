/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    device_test_service
\brief      Common functions for implementation of radio test commands
            in the device test service.
*/
/*! \addtogroup device_test_service
@{
*/

#include "device_test_service_rftest.h"

#ifdef INCLUDE_DEVICE_TEST_SERVICE_RADIOTEST_V2

typedef enum
{
    RFTEST_INTERNAL_CARRIER_WAVE = INTERNAL_MESSAGE_BASE,
    RFTEST_INTERNAL_TXSTART,
    RFTEST_INTERNAL_DUTMODE,
    RFTEST_INTERNAL_LETXSTART,
    RFTEST_INTERNAL_LERXSTART,

    RFTEST_INTERNAL_TEST_TIMEOUT,

    /*! This must be the final message */
    RFTEST_INTERNAL_MESSAGE_END
} rftest_internal_message_t;

ASSERT_INTERNAL_MESSAGES_NOT_OVERFLOWED(RFTEST_INTERNAL_MESSAGE_END)

/*! State of the RFTest portion of device test service */
struct deviceTestServiceRfTestState
{
    bool test_running;
    bool le_test_running;
    Task le_response_pending;
    /*! Anonymous structure used to collate whether settings have been configured */
    struct
    {
        bool channel:1;
        bool packet:1;
        bool address:1;
        bool power:1;
        bool stop_pio:1;
	bool stop_time:1;
	bool stop_touch:1;
    } configured;
    /*! Anonymous structure used to collate settings  */
    struct
    {
        uint8 channel;
        uint8 cw_channel;
        uint16 packet_type;
        uint16 packet_payload;
        uint16 packet_length;
        uint16 test_timeout;
        uint8  lt_addr;
        uint8 power;
        uint8 test_reboot;
        uint8 test_stop_pio;
        uint8 le_channel;
        uint8 le_length;
        uint8 le_pattern;
        bdaddr address;
    } configuration;
};

/*! State of the RFTest portion of device test service */
extern struct deviceTestServiceRfTestState deviceTestService_rf_test_state;

/*! Task information for the device test service */
extern TaskData device_test_service_rftest_task;

#define RFTEST_TASK() (&device_test_service_rftest_task)


/*! Timeout in milli-seconds to delay a command after sending an OK 
    response. This time should allow for an OK response to be sent
    before performing any other activity */
#define deviceTestService_CommandDelayMs()  100


/*! \brief Helper macro to see if any RF test is running */
#define RUNNING() (BREDR_RUNNING() || LE_RUNNING())

/*! \brief Helper macro to see if any RF test is running */
#define LE_RESPONSE_TASK() deviceTestService_rf_test_state.le_response_pending

/*! \brief Helper macro to see if a BREDR RF test is running */
#define BREDR_RUNNING() deviceTestService_rf_test_state.test_running
/*! \brief Helper macro to see if an LE RF test is running */
#define LE_RUNNING() deviceTestService_rf_test_state.le_test_running

/*! \brief Helper macro to access the configuration status of a named setting */
#define CONFIGURED(_field) deviceTestService_rf_test_state.configured._field
/*! \brief Helper macro to access storage for a named setting */
#define SETTING(_field) deviceTestService_rf_test_state.configuration._field


/*! \brief Helper function to initiate test timeout or enable 
        completion on PIO change */
void deviceTestService_RfTest_SetupForTestCompletion(void);

/*! \brief Helper function to make sure status is cleaned when
        a test completes */
void deviceTestService_RfTest_TearDownOnTestCompletion(void);

/*! \brief Place the device into DUT mode and setup for test completion */
void deviceTestService_RfTestBredr_DutMode(void);

/*! \brief Start a BREDR carrier wave test and setup for test completion */
void deviceTestService_RfTestBredr_CarrierTest(void);

/*! \brief Start a BREDR TX test and setup for test completion */
void deviceTestService_RfTestBredr_TxStart(void);

/*! \brief Start an LE transmit test and setup for test completion */
void deviceTestService_RfTest_LeTxStart(void);

/*! \brief Start an LE receive test and setup for test completion */
void deviceTestService_RfTest_LeRxStart(void);

#endif /* INCLUDE_DEVICE_TEST_SERVICE_RADIOTEST_V2 */

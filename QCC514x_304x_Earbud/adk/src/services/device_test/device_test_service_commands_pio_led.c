/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    device_test_service
\brief      Implementation of device test service commands for testing LEDs
            and PIOs.
*/
/*! \addtogroup device_test_service
@{
*/

#include "device_test_service.h"
#include "device_test_service_auth.h"
#include "device_test_parse.h"

#include <led_manager_protected.h>
#include <pio_common.h>
#include <pio_monitor.h>    /* Included to use an independent define for the 
                               number of PIOs */
#ifndef PIOS_PER_BANK
#include <hydra_dev.h>
#endif

#include <logging.h>
#include <stdio.h>


#define VALID_PIO(_pio)     ((_pio) < PIOM_NUM_PIOS)
#define VALID_ONOFF(_onoff) ((_onoff) <= 1)

#define VALID_LED_VALUE(_value) ((_value) <= 255)

COMPILE_TIME_ASSERT(PIOM_NUM_PIOS<=96,Need_update_to_support_more_PIOS);

/*! Response message sent when reading a single PIO */
#define TEST_PIO_INPUT_PIO_RESPONSE_BASE "+TESTPIOINPUTPIO:"

/*! Maximum length of the response message, allowing for up to 999 PIOS
    and the bit value :999,1 
    The NULL character at end of string comes from the base string */
#define FULL_INPUT_PIO_RESPONSE_LEN (sizeof(TEST_PIO_INPUT_PIO_RESPONSE_BASE) + 5)

/*! Base of response message sent when reading all PIOs */
#define TEST_ALL_PIO_RESPONSE_BASE "+TESTPIOINPUTS:"

/*! Length of all PIO response, based on the number of bits being reported */
#define FULL_ALL_PIO_RESPONSE_LEN(_bits) (sizeof(TEST_ALL_PIO_RESPONSE_BASE) + ((_bits) + 3) / 4)


/*! Command handler for AT + TESTPIOOUTPUT

    The function decides if the command is allowed and parameters are
    valid.  The requested PIO is then updated.

    Otherwise errors are reported

    \note The PIO is not configured by this command. If the PIO is not
    one used by the application normally, the actual output may not change.

    \param[in] task The task to be used in command responses
 */
void DeviceTestServiceCommand_HandlePioOutput(Task task,
                        const struct DeviceTestServiceCommand_HandlePioOutput *params)
{
    unsigned pio = params->pio;
    unsigned on = params->onOff;

    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandlePioOutput Pio:%d On:%d",
                     pio, on);

    if (   !DeviceTestService_CommandsAllowed()
        || !VALID_PIO(pio)
        || !VALID_ONOFF(on))
    {
        DeviceTestService_CommandResponseError(task);
        return;
    }

    unsigned bank = PioCommonPioBank(pio);
    unsigned mask = PioCommonPioMask(pio);

    PioSet32Bank(bank, mask, on ? mask : 0);

    DeviceTestService_CommandResponseOk(task);
}


/*! Command handler for AT + TESTPIOINPUT

    The function decides if the command is allowed and parameters are
    valid.  If so the PIO requested is read and a command response sent
    followed by OK.

    Otherwise errors are reported

    \note The PIO is not configured by this command. If the PIO is not
    one used by the application normally, then it may not be possible
    to read the input correctly.

    \param[in] task The task to be used in command responses
 */
void DeviceTestServiceCommand_HandlePioInput(Task task,
                        const struct DeviceTestServiceCommand_HandlePioInput *params)
{
    char response[FULL_INPUT_PIO_RESPONSE_LEN];
    unsigned pio = params->pio;

    if (   !DeviceTestService_CommandsAllowed()
        || !VALID_PIO(pio))
    {
        DeviceTestService_CommandResponseError(task);

        DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandlePioInput Pio:%d. Error",
                         pio);
        return;
    }

    unsigned bank = PioCommonPioBank(pio);
    unsigned mask = PioCommonPioMask(pio);

    bool set = !!(PioGet32Bank(bank) & mask);

    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandlePioInput Pio:%d. Read:%d",
                     pio, set);

    sprintf(response, TEST_PIO_INPUT_PIO_RESPONSE_BASE "%d,%d", pio, set);
    DeviceTestService_CommandResponse(task, response, FULL_INPUT_PIO_RESPONSE_LEN);
    DeviceTestService_CommandResponseOk(task);
}

COMPILE_TIME_ASSERT(PIOS_PER_BANK == 32, unsupported_bank_size);

static void deviceTestService_GenerateAllPioResponse(unsigned bits, char *base)
{
    pio_common_allbits  all;
    unsigned bits_in_last_bank = bits % PIOS_PER_BANK;
    unsigned banks_left = (bits + PIOS_PER_BANK - 1) / PIOS_PER_BANK;

    PioCommonBitsRead(&all);

    /* If, in future, the number of bits supported is not an exact number
       of banks... this code deals with the first bank. And was tested. */
    if (bits_in_last_bank)
    {
        unsigned nibbles = (bits + 3) / 4;
        unsigned mask = (1 << bits_in_last_bank) - 1;

        base += sprintf(base, "%0*X", nibbles, all.mask[--banks_left] & mask);
        bits -= bits_in_last_bank;
    }
    while (banks_left--)
    {
        base += sprintf(base, "%08X", all.mask[banks_left]);
    }
}


/*! Command handler for AT + TESTPIOINPUTS

    The function decides if the command is allowed.  If so all PIOs are read 
    and command responses sent followed by OK.

    Otherwise errors are reported

    \note The PIOs are not configured by this command. PIOs that are not
    used by the application normally may not be configures as inputs , then it may not be possible
    to read the input correctly.

    \param[in] task The task to be used in command responses
 */
void DeviceTestServiceCommand_HandlePioAllInputs(Task task)
{
    char response[FULL_ALL_PIO_RESPONSE_LEN(PIOM_NUM_PIOS)] = TEST_ALL_PIO_RESPONSE_BASE;
    unsigned hex_offset=sizeof(TEST_ALL_PIO_RESPONSE_BASE) - 1;

    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandlePioInputs. %d pios", PIOM_NUM_PIOS);

    if (!DeviceTestService_CommandsAllowed())
    {
        DeviceTestService_CommandResponseError(task);

        return;
    }

    deviceTestService_GenerateAllPioResponse(PIOM_NUM_PIOS, &response[hex_offset]);
    DeviceTestService_CommandResponse(task, response, FULL_ALL_PIO_RESPONSE_LEN(PIOM_NUM_PIOS));
    DeviceTestService_CommandResponseOk(task);
}


/*! Command handler for AT + TESTLED

    The function decides if the command is allowed.  If so the LEDs requested are 
    set and a command response (OK) sent.

    Otherwise errors are reported

    \note The LED parameters are not checked. This avoids the need to update
          the test command implementation if additional LEDs are supported
          in the future.

    \param[in] task The task to be used in command responses
 */
void DeviceTestServiceCommand_HandleLed(Task task, 
                                        const struct DeviceTestServiceCommand_HandleLed *params)
{
    unsigned value = params->ledValue;

    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleLed value:0x%x", value);

    if (   !DeviceTestService_CommandsAllowed()
        || !VALID_LED_VALUE(value))
    {
        DeviceTestService_CommandResponseError(task);

        return;
    }

    if (value)
    {
        LedManager_ForceLeds(value);
    }
    else
    {
        LedManager_ForceLedsStop();
    }
    DeviceTestService_CommandResponseOk(task);
}


/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    device_test_service
\brief      Implementation of common helper functions for AT command implementation
*/
/*! \addtogroup device_test_service
@{
*/

#include "device_test_service_commands_helper.h"

uint8 deviceTestService_HexToNumber(char hex_char)
{
    if ('A' <= hex_char && hex_char <= 'F')
    {
        return (hex_char - 'A' + 10);
    }
    if ('0' <= hex_char && hex_char <= '9')
    {
        return (hex_char - '0');
    }
    if ('a' <= hex_char && hex_char <= 'f')
    {
        return (hex_char - 'a' + 10);
    }

    /* Choose to return a value rather than Panic.
       Not exactly an attack vector, but better */
    return 0;
}


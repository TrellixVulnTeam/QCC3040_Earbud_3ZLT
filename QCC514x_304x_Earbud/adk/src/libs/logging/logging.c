/*!
\copyright  Copyright (c) 2008 - 2018 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\brief      Implementation for VM adaptation of logging to pydbg
*/

#include <logging.h>

#ifndef DISABLE_LOG
/*! Sequence variable used in debug output so that missing log items can be detected. 

    This sequence can be removed by defining LOGGING_EXCLUDE_SEQUENCE before logging.h
    is included. This has the effect of allowing more debug (as the sequence is itself
    a logging parameter), but loses the ability to detect missing logging.

    Note that the sequence can be excluded on a per file basis.
 */
uint16 globalDebugLineCount = 0;

#ifndef DISABLE_DEBUG_LOG_LEVELS
debug_log_level_t debug_log_level__global = DEFAULT_LOG_LEVEL;
#endif

void debugLogData(const uint8 *data, uint16 data_size)
{
#if !defined(DESKTOP_BUILD) && defined(INSTALL_HYDRA_LOG)
    while (data_size >= 8)
    {
        _DEBUG_LOG_UNCONDITIONAL("%02x %02x %02x %02x %02x %02x %02x %02x",
                             data[0], data[1], data[2], data[3],
                             data[4], data[5], data[6], data[7]);
        data += 8;
        data_size -= 8;
    }

    switch (data_size)
    {
    case 7:
        _DEBUG_LOG_UNCONDITIONAL("%02x %02x %02x %02x %02x %02x %02x",
                             data[0], data[1], data[2], data[3],
                             data[4], data[5], data[6]);
        break;
    case 6:
        _DEBUG_LOG_UNCONDITIONAL("%02x %02x %02x %02x %02x %02x",
                             data[0], data[1], data[2], data[3],
                             data[4], data[5]);
        break;
    case 5:
        _DEBUG_LOG_UNCONDITIONAL("%02x %02x %02x %02x %02x",
                             data[0], data[1], data[2], data[3],
                             data[4]);
        break;
    case 4:
        _DEBUG_LOG_UNCONDITIONAL("%02x %02x %02x %02x",
                             data[0], data[1], data[2], data[3]);
        break;
    case 3:
        _DEBUG_LOG_UNCONDITIONAL("%02x %02x %02x",
                             data[0], data[1], data[2]);
        break;
    case 2:
        _DEBUG_LOG_UNCONDITIONAL("%02x %02x",
                             data[0], data[1]);
        break;
    case 1:
        _DEBUG_LOG_UNCONDITIONAL("%02x",
                             data[0]);
        break;
    default:
        break;
    }
#else
    UNUSED(data);
    UNUSED(data_size);
#endif
}


#endif /* #ifndef DISABLE_LOG */

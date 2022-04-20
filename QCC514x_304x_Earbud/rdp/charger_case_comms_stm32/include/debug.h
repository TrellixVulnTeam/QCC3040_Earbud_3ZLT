/**
 * @file    debug.h
 * @brief   Interface for external debugging of the charger case application.
 */

#ifndef __DEBUG_H
#define __DEBUG_H

#include <stdint.h>
#include <stdbool.h>
#include "cli_parse.h"

CLI_RESULT ats_test(uint8_t cmd_source);
void debug_enable_test_mode(bool enable, uint8_t cmd_source);

#endif /* __DEBUG_H */

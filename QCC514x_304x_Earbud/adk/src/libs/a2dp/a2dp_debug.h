#ifndef A2DP_DEBUG_H
#define A2DP_DEBUG_H

#define DEBUG_LOG_MODULE_NAME a2dp_lib
#include <logging.h>

#ifdef A2DP_DEBUG_LIB
#include <panic.h>
#include <stdio.h>
#include <logging.h>
#define A2DP_FATAL_IN_DEBUG(...)   {DEBUG_LOG_ERROR(__VA_ARGS__); Panic();}
#else
#define A2DP_FATAL_IN_DEBUG(...)
#endif

#endif // A2DP_DEBUG_H

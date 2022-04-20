#ifndef ECHO_DEBUG_H

#define ECHO_DEBUG_H

#ifdef ECHO_DEBUG_PRINT_ENABLED

#include "logging.h"

#define ECHO_DEBUG_PRINT(...)				DEBUG_LOG_INFO(__VA_ARGS__)

#define ECHO_DEBUG_BD_ADDR_PRINT(addr)		AUL_DEBUG_PRINT("BD_ADDR: %04x:%02x:%06x", addr.nap, addr.uap, addr.lap);

#else

#define ECHO_DEBUG_PRINT(...)

#define ECHO_DEBUG_BD_ADDR_PRINT(addr)

#endif

#endif	/* ECHO_DEBUG_H */


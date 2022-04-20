#ifndef AUL_DEBUG_H

#define AUL_DEBUG_H

#ifdef AUL_DEBUG_PRINT_ENABLED

#include "logging.h"

#define AUL_DEBUG_PRINT(...)				DEBUG_LOG_INFO(__VA_ARGS__)

#define AUL_DEBUG_BD_ADDR_PRINT(addr)		AUL_DEBUG_PRINT("BD_ADDR: %04x:%02x:%06x", addr.nap, addr.uap, addr.lap);

#else

#define AUL_DEBUG_PRINT(...)

#define AUL_DEBUG_BD_ADDR_PRINT(addr)

#endif

#endif	/* AUL_DEBUG_H */


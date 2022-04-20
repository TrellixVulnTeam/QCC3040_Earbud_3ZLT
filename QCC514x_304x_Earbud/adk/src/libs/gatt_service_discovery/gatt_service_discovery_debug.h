/* Copyright (c) 2020 Qualcomm Technologies International, Ltd. */
/*  */


#ifndef GATT_SERVICE_DISCOVERY_DEBUG_H_
#define GATT_SERVICE_DISCOVERY_DEBUG_H_

#include <stdio.h>
/* Macro used to generate debug version of this library */
#ifdef GATT_SERVICE_DISCOVERY_DEBUG_LIB


#ifndef DEBUG_PRINT_ENABLED
#define DEBUG_PRINT_ENABLED
#endif

#include <panic.h>
#include <print.h>


#define GATT_SD_DEBUG_INFO(x) {PRINT(("%s:%d - ", __FILE__, __LINE__)); PRINT(x);}
#define GATT_SD_DEBUG_PANIC(x) {GATT_SD_DEBUG_INFO(x); Panic();}
#define GATT_SD_PANIC(x) {GATT_SD_DEBUG_INFO(x); Panic();}


#else /* GATT_SERVICE_DISCOVERY_DEBUG_LIB */


#define GATT_SD_DEBUG_INFO(x)
#define GATT_SD_DEBUG_PANIC(x)
#define GATT_SD_PANIC(x) {printf(x);}

#endif /* GATT_SERVICE_DISCOVERY_DEBUG_LIB */


#endif /* GATT_SERVICE_DISCOVERY_DEBUG_H_ */

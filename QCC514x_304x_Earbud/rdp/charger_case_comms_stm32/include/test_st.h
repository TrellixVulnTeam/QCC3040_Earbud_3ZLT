/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Redefinitions of STM peripherals for use in unit tests.
*/

#ifndef TEST_ST_H_
#define TEST_ST_H_

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

#ifdef AHBPERIPH_BASE
#undef AHBPERIPH_BASE
#define AHBPERIPH_BASE ((uint32_t)test_AHBPERIPH)
#endif

#ifdef AHB2PERIPH_BASE
#undef AHB2PERIPH_BASE
#define AHB2PERIPH_BASE ((uint32_t)test_AHB2PERIPH)
#endif

#ifdef APBPERIPH_BASE
#undef APBPERIPH_BASE
#define APBPERIPH_BASE ((uint32_t)test_APBPERIPH)
#endif

#ifdef SCS_BASE
#undef SCS_BASE
#define SCS_BASE ((uint32_t)test_SCS)
#endif

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

uint8_t test_AHBPERIPH[0x4400];
uint8_t test_AHB2PERIPH[0x1800];
uint8_t test_APBPERIPH[0x20000];
uint8_t test_SCS[0x2000];

#endif /* TEST_ST_H_ */

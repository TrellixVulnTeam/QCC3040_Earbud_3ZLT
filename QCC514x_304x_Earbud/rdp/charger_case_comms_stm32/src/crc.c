/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief     CRC functions.
           * CRC-8-WCDMA encoder and verifier. The input is fed in reverse order (MSB first).
           * CRC-16-CITT-FALSE encoder and verifier
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include "crc.h"

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

/* The polynomial for 8-bits CRC is  X^8 + x^7 + x^4 + x^3 + x+ 1
 * The reversed representation is used */
#define GNPOLY   0xd9

/*
* The CRC table size is based on how many bits at a time we are going
* to process through the table.  Given that we are processing the data
* 8 bits at a time, this gives us 2^8 (256) entries.
*/
#define CRC_TAB_SIZE 256

/*
* Mask for a CRC-16 polynomial: x^16 + x^12 + x^5 + 1
* This is more commonly called CCITT-16.
*/
#define CRC_16_POLYNOMIAL 0x1021

/*
* Seed value for the CRC register. The all ones seed is part of CCITT-16,
* and it allows for detection of an entire data stream of zeroes.
*/
#define CRC_16_SEED 0xFFFF

/*
* Residual CRC value to compare against a return value of crc_calculate_crc16().
* Use crc_calculate_crc16() to calculate a 16-bit CRC and append it to the buffer.
* When crc_calculate_crc16() is applied to the unchanged result, it returns
* CRC_16_OK.
*/
#define CRC_16_OK 0xE2F0

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

/*
* CRC table for 16 bit CRC, with generator polynomial 0x1021,
* calculated 8 bits at a time, MSB first.
*/
static const uint16_t crc16_table[ CRC_TAB_SIZE ] =
{
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
    0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
    0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
    0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
    0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
    0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
    0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
    0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
    0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
    0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
    0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
    0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
    0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
    0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
    0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
    0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
    0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
    0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
    0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
    0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
    0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
    0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
    0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
};

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

static uint16_t update_crc8_loop(uint16_t crc, uint8_t loop_no, uint8_t gnp)
{
    uint16_t cnt;

    for(cnt = 0; cnt < loop_no; cnt++)
    {
        crc = (crc & 0x1)? ((crc >> 1) ^ gnp):(crc >> 1);
    }

    return crc;
}

/**
 * @brief This function performs CRC encoding or validation depending on \encode.
 * If \encode is True, it generating a CRC.
 * If \encode is False, it is validating a CRC.
 *
 * @param *data  Pointer to the data which should be encoded or validated.
 *               If encode is True, This is the data to encode a CRC for.
 *               If encode is False, The data decode and 8 bit CR
 * @param data_len The lengh of data in octets. It does not include the 8 bit CRC byte when verifying
 * @param encode If True, generate a CRC. If false, validate the data with the CRC as the final octet.
 * @param *parity Pointer to the encoded/decoded CRC value.
 */
static void crc_encoder(uint8_t *data, uint8_t data_len, bool encode, uint8_t *parity)
{
    /*
    * The polynomial for CRC is  X^8 + x^7 + x^4 + x^3 + x+ 1
    */
    uint16_t gnp = GNPOLY;
    uint16_t crc;
    uint16_t cnt;

    /* Initial value is all ones to ensure CRC fails with data full of zero's. */
    crc = 0xff;

    /* first encoded/decoded data in U8 word count */
    for (cnt = 0; cnt < data_len; cnt++)
    {
        crc = crc ^ (data[cnt] & 0xff);
        crc = update_crc8_loop(crc, 8, gnp);
    }

    /* If decoding, read 8-bit parity which is the next word of the input data */
    if(!encode)
    {
        /* It is decoding, then process the 8-bit parity*/
        crc = crc ^ (data[data_len] & 0xff);
        crc = update_crc8_loop(crc, 8, gnp);
    }
    *parity  = crc & 0xff;
}

uint8_t crc_calculate_crc8(uint8_t *data, uint8_t data_len)
{
    uint8_t crc_result;
    crc_encoder(data, data_len, true, &crc_result);
    return crc_result;
}

bool crc_verify_crc8(uint8_t *data, uint8_t data_len)
{
    uint8_t crc_parity;
    crc_encoder(data, data_len, false, &crc_parity);
    return crc_parity == 0;
}

/*
* This function calculates a 16-bit CRC over a specified number of data
* bytes. It can be used to produce a CRC and to check a CRC.
*
* Returns a word holding 16 bits which are the contents of the CRC
* register as calculated over the specified data bytes.  If this
* function is being used to check a CRC, then the return value will be
* equal to CRC_16_OK if the CRC checks correctly.
*
* Based on the earbud function crc_16_calc().
*/
uint16_t crc_calculate_crc16(uint8_t *data, uint8_t data_len)
{
    uint16_t crc_16;

    /*
    * Generate a CRC-16 by looking up the transformation in a table and
    * XOR-ing it into the CRC, one byte at a time.
    */
    for (crc_16 = CRC_16_SEED ; data_len; data_len--, data++)
    {
        crc_16 = (uint16_t)(crc16_table[ (crc_16 >> (16 - 8)) ^ *data ] ^ (crc_16 << 8));
    }

    /* Return the 1's complement of the CRC */
    return (uint16_t)( ~crc_16 );
}

bool crc_verify_crc16(uint8_t *data, uint8_t data_len)
{
    return (crc_calculate_crc16(data, data_len) == CRC_16_OK) ? true:false;
}

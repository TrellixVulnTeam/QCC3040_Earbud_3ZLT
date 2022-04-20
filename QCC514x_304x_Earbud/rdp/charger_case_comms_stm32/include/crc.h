/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      CRC functions.
            * CRC-8-WCDMA encoder and verifier
            * CRC-16-CITT-FALSE encoder and verifier
*/

#ifndef CRC_H_
#define CRC_H_

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include <stdint.h>
#include <stdbool.h>

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

/**
 * @brief Generate a CRC8 based on an array of octets.
 *
 * @param *data The data to perform the CRC8 over.
 * @param data_len The length of data in octets
 * @return The CRC8 value calculated.
 */
uint8_t crc_calculate_crc8(uint8_t *data, uint8_t data_len);

/**
 * @brief Verify an array of octets against a CRC8 value.
 *        It is assumed that the last octet is the CRC8 checksum for the
 *        preceeding bytes.
 *
 * @param *data The data to perform the CRC8 over.
 * @param data_len The length of data in octets to verify.
 * @return true if the CRC8 parity succeeds, false if the verification fails.
 */
bool crc_verify_crc8(uint8_t *data, uint8_t data_len);

/**
 * @brief Generate a CRC16 based on an array of octets.
 *
 * @param *data The data to perform the CRC16 over.
 * @param data_len The length of data in octets
 * @return The CRC16 value calculated.
 */
uint16_t crc_calculate_crc16(uint8_t *data, uint8_t data_len);

/**
 * @brief Verify an array of octets against a CRC16 value.
 *        It is assumed that the last two octets are the CRC16 checksum for the
 *        preceeding bytes.
 *
 * @param *data The data to perform the CRC16 over.
 * @param data_len The length of data in octets to verify.
 * @return true if the CRC16 parity succeeds, false if the verification fails.
 */
bool crc_verify_crc16(uint8_t *data, uint8_t data_len);

#endif /* CRC_H_ */

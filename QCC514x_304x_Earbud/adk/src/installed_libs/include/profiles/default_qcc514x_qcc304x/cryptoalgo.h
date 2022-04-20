/****************************************************************************
Copyright (c) 2018 Qualcomm Technologies International, Ltd.


FILE NAME
    cryptoalgo.h

DESCRIPTION
    A cryptographic API library on P1 for applications.Note: This is to be changed later for P0 support.

*/

#ifndef _CRYPTOALGO_H__
#define _CRYPTOALGO_H__

#define AES_BLOCK_SIZE 16
#define SHA256_DIGEST_SIZE 32
#define AES_CTR_NONCE_SIZE 8

#define SECP256R1_PUBLIC_KEY_SIZE       64
#define SECP256R1_PRIVATE_KEY_SIZE      32
#define SECP256R1_SHARED_SECRET_SIZE    32

/*Function to do AES128 encrypt*/
extern void aes128_encrypt(uint8 in[AES_BLOCK_SIZE], uint8 out[AES_BLOCK_SIZE], uint8 key[AES_BLOCK_SIZE]);

/*Function to do AES128 decrypt*/
extern void aes128_decrypt(uint8 in[AES_BLOCK_SIZE], uint8 out[AES_BLOCK_SIZE], uint8 key[AES_BLOCK_SIZE]);

/*Function to do sha256 hash*/
extern void sha256(const uint8 *data, uint16 len, uint8 digest[SHA256_DIGEST_SIZE]);

/*Function to do generate a shared secret key with ECDH*/
extern bool secp256r1_shared_secret(const uint8 public_key[SECP256R1_PUBLIC_KEY_SIZE],
                             const uint8 private_key[SECP256R1_PRIVATE_KEY_SIZE],
                             uint8 shared_secret[SECP256R1_SHARED_SECRET_SIZE]);

/**
  * Encrypt the data using AES-CTR.
  * Please note that no memory is allocated. All the arrays should be pre-allocated by the caller.
  * Please note that the encrypted output array(encr_out) has the same size as the input array(data)
  * and should be pre-allocated by the caller.
  *
  * Parameters:
  * [in] data      Input data array
  * [in] data_sz   data size in bytes
  * [out] encr_out  Encrypted data. The data has the same size as input data (data_sz).
  * [in] key   Key array
  * [in] nonce nonce array
  *
  * Return: TRUE if the function is succesful
  *         FALSE if the function fails due to wrong input
  */
bool aesCtr_encrypt(uint8 *data, uint16 data_sz, uint8 *encr_out, uint8 key[AES_BLOCK_SIZE], uint8 nonce[AES_CTR_NONCE_SIZE]);

/**
  * Decrypt the encrypted Data using AES-CTR.
  *
  * Please note that no memory is allocated. All the arrays should be pre-allocated by the caller.
  * Please note that the decrypted output array(decr_out) has the same size as the input array(encr_in)
  * and should be pre-allocated by the caller.
  *
  * Parameters:
  * [in] encr_in      Input encryped data array
  * [in] encr_in_sz   data size in bytes
  * [out] decr_out    Decrypted data. The data has the same size as input data (encr_in_sz).
  * [in] key   Key array
  * [in] nonce nonce array.
  *
  * Return: TRUE if the function is succesful
  *         FALSE if the function fails due to wrong input
  */
bool aesCtr_decrypt(uint8 *encr_in, uint16 encr_in_sz, uint8 *decr_out, uint8 key[AES_BLOCK_SIZE], uint8 nonce[AES_CTR_NONCE_SIZE]);

/** Perform HAMC-SHA256.
  * Please note that the caller should allocate memory for the arrays used.
  *
  * Parameters:
  * [in] data      Input data array
  * [in] data_sz   data size in bytes
  * [out] hamc_sha256_out  HAMC-SHA256 output array
  * [in] key   Key array
  * [in] nonce nonce array
  *
  * Return: TRUE if the function is succesful
  *         FALSE if the function fails. Wrong input or unavailability of memory are some of the reasons
  */
bool hmac_sha256(uint8 *data, uint16 data_sz, uint8 hmac_sha256_out[SHA256_DIGEST_SIZE], uint8 key[AES_BLOCK_SIZE], uint8 nonce[AES_CTR_NONCE_SIZE]);


#endif

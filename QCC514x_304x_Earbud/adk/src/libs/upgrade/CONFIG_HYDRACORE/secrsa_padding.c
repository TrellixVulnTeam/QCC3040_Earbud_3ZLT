/****************************************************************************
Copyright (c) 2016 Qualcomm Technologies International, Ltd.


FILE NAME
    secrsa_padding.c

DESCRIPTION
    This file provides a PKCS #1 PSS verify (decode) implementation.

NOTES

*/

#include "imageupgrade.h"
#include "secrsa_padding.h"
#include <string.h>
#include <stdlib.h>
#include "logging.h"

#define PMALLOC_POOL_BLOCK_SIZE     220

#define MAX_DER_HEADER_LEN   32

static int ce_hash(const uint8 *msg, uint32 msg_len, uint8 *digest)
{
    int ret = CE_SUCCESS;
    hash_context_t ctx = NULL;

    do
    {
        if ((ctx = ImageUpgradeHashInitialise(SHA256_ALGORITHM))
            == NULL)
        {
            ret = CE_ERROR_FAILURE;
            break;
        }

        /* The memory for msg was malloc'ed so should be aligned for uint16* OK. */
        if ( !ImageUpgradeHashMsgUpdate(ctx, msg, msg_len))
        {
            ret = CE_ERROR_FAILURE;
            break;
        }

        /* The memory for digest was malloc'ed so should be aligned for uint16* OK. */
        if ( !ImageUpgradeHashFinalise(ctx, digest, HASH_ALGORITHM_LEN_SHA256))
        {
            ret = CE_ERROR_FAILURE;
            break;
        }

    } while (0);

    return ret;
}

static void i2osp(uint8 *out, uint8 *counter, uint8 len)
{
    uint8    i, j;

    for( i = 0, j = len - 1; i < len; i++, j-- )
    {
        out[i] = counter[j];
    }
}


/**
  PKCS #1 MGF1 (Mask Generation Function).
   
  @param[in]  seed             The data to use as seed 
  @param[in]  seedlen          The length of the seed
  @param[out] mask             The pointer hold the generated mask
  @param[in]  masklen          The lenght of requested mask 
  
  @return 
   CE_SUCCESS               - Function executes successfully. \n
   CE_ERROR_INVALID_ARG     - one or more invalid parameter values. \n
   CE_ERROR_NOT_SUPPORTED   - the feature is not supported. \n
   CE_ERROR_INVALID_PACKET  - invalid packet. \n
   CE_ERROR_BUFFER_OVERFLOW - not enough space for output. \n
   CE_ERROR_FAILURE         - one of the Hash functions failed. \n
   CE_ERROR_NO_MEMORY       - out of memory. \n

  @dependencies
  None. 
 */
static int ce_pkcs1_mgf1
(
    unsigned char   *seed,
    unsigned long   seedlen,
    unsigned char   *mask,
    unsigned long   masklen
)
{
    int             ce_status = CE_SUCCESS;
    unsigned long   counter = 0;
    uint8           temp_counter[4] = {0};
    int             i = 0;
    int             hashlen = 0;
    hash_context_t  hash_ctx = NULL;
    uint8           *temp_buf = NULL;

    /* i/p param check */
    if (NULL == seed
        ||
        0 == seedlen 
        ||
        NULL == mask 
        ||
        0 == masklen )
    {
        return CE_ERROR_INVALID_ARG;
    }

    /* get the hash length */
    hashlen = MAX_DER_HEADER_LEN;

    do
    {
        for ( ;i < masklen; counter++ )
        {
            hash_ctx = ImageUpgradeHashInitialise(SHA256_ALGORITHM);
            if ( NULL == hash_ctx )
            {
                ce_status = CE_ERROR_NOT_SUPPORTED;
                break;
            }

            if ( !ImageUpgradeHashMsgUpdate(hash_ctx, (uint8*)seed, seedlen) )
            {
                ce_status = CE_ERROR_FAILURE;
                break;
            }

            i2osp(temp_counter, (uint8*)&counter, 4);

            if ( !ImageUpgradeHashMsgUpdate(hash_ctx, temp_counter, 4) )
            {
                ce_status = CE_ERROR_FAILURE;
                break;
            }

            if ( masklen >= i + hashlen )
            {
                if ( !ImageUpgradeHashFinalise(hash_ctx, (mask+i), 
                    HASH_ALGORITHM_LEN_SHA256) )
                {
                    ce_status = CE_ERROR_FAILURE;
                    break;
                }
            }
            else
            {
                if ( NULL == temp_buf )
                {
                    DEBUG_LOG("ce_pkcs1_mgf1: hashlen:%ld", hashlen);

                    /* temp_buf has not yet been allocated, so allocate it.
                     * Otherwise re-use the buffer already allocated or there
                     * will be a memory leek within the for loop.
                     */
                    temp_buf = (uint8 *)malloc(hashlen);
                }

                if ( NULL == temp_buf )
                {
                    ce_status = CE_ERROR_NO_MEMORY;
                    break;
                }

                if ( !ImageUpgradeHashFinalise(hash_ctx, temp_buf, 
                    HASH_ALGORITHM_LEN_SHA256) )
                {
                    ce_status = CE_ERROR_FAILURE;
                    break;
                }
                memcpy((mask+i), temp_buf, (hashlen - ((i+hashlen) - masklen)));
            }
            i += hashlen;
        }
    }while( 0 );

    if( temp_buf )
    {
        memset(temp_buf, 0, hashlen);
        free(temp_buf);
    }

    return ce_status;
}/* ce_pkcs1_mgf1 */

/**
  PKCS #1 PSS verify.

  @param[in] msghash          The encoded data to decode
  @param[in] msghashlen       The length of the encoded data (octets)
  @param[in] sig              The signature
  @param[in] siglen           The signature length
  @param[in] saltlen          The length of salt
  @param[in] modulus_bitlen   The bit length of the RSA modulus

  @return 
   CE_SUCCESS                   - Function executes successfully. \n
   CE_ERROR_INVALID_ARG         - a paremeter is invalid. \n
   CE_ERROR_INVALID_SIGNATURE   - the signature is invalid. \n
   CE_ERROR_NO_MEMORY           - out of memory. \n
   CE_ERROR_NOT_SUPPORTED       - the feature is not supported. \n
   CE_ERROR_INVALID_PACKET      - invalid packet. \n
   CE_ERROR_BUFFER_OVERFLOW     - not enough space for output. \n
   CE_ERROR_FAILURE             - ce_pkcs1_mgf1 or ce_hash failed. \n

 */
int ce_pkcs1_pss_padding_verify
(
    const unsigned char *msghash,
    unsigned long       msghashlen,
    const unsigned char *sig,
    unsigned long       siglen,
    unsigned long       saltlen,
    unsigned long       modulus_bitlen
)
{
    int                   ce_status = CE_SUCCESS;
    int                   modulus_len = 0;
    unsigned char         *m_ = NULL;
    unsigned long         m_len = 0;
    unsigned char         *hash = NULL;
    unsigned long         hashlen = 0;
    unsigned char         *hash_ = NULL;
    unsigned char         *salt = NULL;
    unsigned char         *DB = NULL;
    unsigned long         DBlen = 0;
    unsigned char         *dbMask = NULL;
    unsigned char         *maskedDB = NULL;
    unsigned long         maskedDBlen = 0;
    unsigned long         PSlen = 0;
    unsigned long         i = 0;

    DEBUG_LOG("ce_pkcs1_pss_padding_verify: msghashlen:%ld", msghashlen);
    DEBUG_LOG("ce_pkcs1_pss_padding_verify: siglen:%ld", siglen);
    DEBUG_LOG("ce_pkcs1_pss_padding_verify: saltlen:%ld", saltlen);
    DEBUG_LOG("ce_pkcs1_pss_padding_verify: modulus_bitlen:%ld", modulus_bitlen);


    /* i/p param check */
    if (NULL == msghash
        ||
        0 == msghashlen
        ||
        NULL == sig
        ||
        0 == siglen )
    {
        return CE_ERROR_INVALID_ARG;
    }

    do
    {
        modulus_len = (modulus_bitlen >> 3) + (modulus_bitlen & 7 ? 1 : 0);

        hashlen = MAX_DER_HEADER_LEN;

        /* check size limitations */
        if (siglen < modulus_len
            ||
            siglen < hashlen + saltlen + 2
            ||
            modulus_len < hashlen + saltlen + 2 )
        {
            ce_status = CE_ERROR_INVALID_SIGNATURE;
            break;
        }

        /*
         * sig = maskedDB || hash || 0xbc
         * Here || means octet string concatination.
         */
        if ( sig[siglen-1] != 0xBC )
        {
             ce_status = CE_ERROR_INVALID_SIGNATURE;
             break;
        }

        maskedDB = (unsigned char *)sig;
        maskedDBlen = modulus_len - hashlen - 1;

        hash_ = (unsigned char *)&sig[maskedDBlen];

        if ( (sig[0] & ~(0xFF >> ((modulus_len<<3) - (modulus_bitlen-1)))) != 0 )
        {
            ce_status = CE_ERROR_INVALID_SIGNATURE;
            break;
        }

        if ( saltlen < PMALLOC_POOL_BLOCK_SIZE )
        {
            /* dbMask = MGF (H, DBlen) */
            DBlen = modulus_len - hashlen - 1;
            DEBUG_LOG("ce_pkcs1_pss_padding_verify: DBlen:%ld", DBlen);
            /*
             * Exclusive pmallocs for M' and DB, so dbMask ONLY corresponds to
             * octet string comprising of (PS || 0x1 || salt).
             */
            dbMask = (unsigned char*)malloc(DBlen);
        }
        else
        {
            unsigned char         dbMaskOffset = 0;

            DEBUG_LOG("ce_pkcs1_pss_padding_verify: saltlen >= 220");

            /* dbMask = MGF (H, DBlen) */
            DBlen = modulus_len - hashlen - 1;
            DEBUG_LOG("ce_pkcs1_pss_padding_verify: DBlen:%ld", DBlen);

            /*
             * When saltlen > PMALLOC_POOL_BLOCK_SIZE this can result into
             * requesting 2 large sized pmalloc pool in quick succession within
             * this function.
             *
             * As the larger sized pmalloc pool are less in number, its more
             * likely the pmalloc request can fail. In order to avoid such
             * failures, wherever possible try to optimally request for large
             * size pmalloc pool.
             *
             * Here its feasible to club the 2 large size pmalloc pool request
             * into one as described below:
             *
             * Output of the following expressions,
             *      dbMask = MGF (H, DBlen)
             *      DB = maskedDB ^ dbMask
             * is an octet string composed as follows:
             *      Padding (PS) || 0x1 || salt
             * Here || means string concatination.
             *
             * Whereas m_ is later composed as follows:
             *      M' = (0x)00 00 00 00 00 00 00 00 || mHash || salt
             * Here M' is referred as m_ and || means octet string concatination
             * Using this M', Hash(M') (referred as hash) is computed and
             * compared with H'(refered as hash_)
             *
             * From the above 2 octet strings for DB and M', clearly there is a
             * common part in these octet strings (i.e. salt octet string).
             *
             * So combine allocate memory as big as to be able to fit DB
             * as well as M' octet strings at different instances of time.
             *
             * i.e. Combined M' = (0x)00 00 00 00 00 00 00 00 || msghash ||
             *                      (PS || 0x1 || salt)
             *      where len of octet string comprising of (PS || 0x1 || salt)
             *      is equal to DBlen and corresponds to DB (or dbMask)
             *      and len of octet string comprising of
             *      (0x)00 00 00 00 00 00 00 00 || msghash || salt is equal to
             *      actual m_len and corresponds to actual M'.
             */
            m_len = 8 + msghashlen + DBlen;
            DEBUG_LOG("ce_pkcs1_pss_padding_verify: m_len:%ld", m_len);
            m_ = (unsigned char*)malloc(m_len);

            /*
             * Combined pmalloc for M' and DB, so appropriately setup start of
             * dbMask.
             */
            dbMaskOffset = m_len - DBlen;

            dbMask = (m_ != NULL) ? &m_[dbMaskOffset] : NULL;
        }
        if ( NULL == dbMask )
        {
            ce_status = CE_ERROR_NO_MEMORY;
            break;
        }

        DB = dbMask;
        ce_status = ce_pkcs1_mgf1(hash_, hashlen, dbMask, DBlen);
        if ( CE_SUCCESS != ce_status )
        {
            ce_status = CE_ERROR_FAILURE;
            break;
        }

        /* DB = maskedDB ^ dbMask */
        for ( i = 0; i < DBlen; i++ )
        {
            DB[i] = maskedDB[i] ^ dbMask[i];
        }

        DB[0] &= 0xFF >> ((modulus_len<<3) - (modulus_bitlen-1));

        /* PS = (0x00)1 (0x00)2 (0x00)3 ...
            (0x00)(modulus_len - hashlen - saltlen - 2) */
        PSlen = modulus_len - hashlen - saltlen - 2;
        DEBUG_LOG("ce_pkcs1_pss_padding_verify: PSlen:%ld", PSlen);

        /*
         * DB = PS || 0x01 || salt
         * Here || means octet string concatination.
         */
        for ( i = 0; i < PSlen; i++ )
        {
            if ( DB[i] != 0 )
            {
                ce_status = CE_ERROR_INVALID_SIGNATURE;
                break;
            }
        }

        if ( CE_SUCCESS != ce_status )
        {
            break;
        }
        if ( DB[PSlen] != 0x01 )
        {
            ce_status = CE_ERROR_INVALID_SIGNATURE;
            break;
        }

        salt = &DB[PSlen+1];

        if ( saltlen < PMALLOC_POOL_BLOCK_SIZE )
        {
            /* M' = (0x)00 00 00 00 00 00 00 00 || msghash || salt */
            m_len = 8 + msghashlen + saltlen;
            DEBUG_LOG("ce_pkcs1_pss_padding_verify: m_len:%ld", m_len);
            /*
             * Exclusive pmallocs for M' and DB, so m_ ONLY corresponds to
             * octet string comprising of
             * (0x)00 00 00 00 00 00 00 00 || msghash || salt.
             */
             m_ = (unsigned char*)malloc(m_len);
        }
        else
        {
            /*
             * Combined pmalloc for M' and DB, so appropriately setup start of
             * m_ and its respective m_len.
             * Earlier m_ was allocated as m_len = 8 + msghashlen + DBlen
             * where DBlen = PSlen + 1 + saltlen because the output of the
             * following expressions,
             *      dbMask = MGF (H, DBlen)
             *      DB = maskedDB ^ dbMask
             * is an octet string composed as follows:
             *      Padding (PS) || 0x1 || salt
             * Here || means string concatination.
             *
             * So exclude PSlen + 1 from the actual m_len.
             */
            m_len = 8 + msghashlen + saltlen;

            /*
             * As M' and DB are combinedly malloc'd, reset DB (i.e dbMask)
             * as we are done processing of DB and to avoid double freeing.
             */
            dbMask = NULL;

            /* m_ is appropriately set (above) at allocation */
        }
        if ( NULL == m_ )
        {
            ce_status = CE_ERROR_NO_MEMORY;
            break;
        }

        if ( saltlen < PMALLOC_POOL_BLOCK_SIZE )
        {
            memset(m_, 0, m_len);
            memcpy((m_ + 8), msghash, msghashlen);
            memcpy((m_ + 8 + msghashlen), salt, saltlen);
        }
        else
        {
            /* Retain the derived salt and reset the rest */
            memset(m_, 0, 8 + msghashlen);
            /* Copy the encoded data to decode */
            memcpy((m_+ 8), msghash, msghashlen);
            /* Strip (PS || 0x1), move the derived salt just after encoded data */
            memmove((m_ + 8 + msghashlen), salt, saltlen);
        }

        /* hash M' */
        DEBUG_LOG("ce_pkcs1_pss_padding_verify: hashlen:%ld", hashlen);
        hash = (unsigned char*)malloc(hashlen);
        if ( NULL == hash )
        {
            ce_status = CE_ERROR_NO_MEMORY;
            break;
        }

        /* hash = hash(M') */
        ce_status = ce_hash(m_, m_len, hash);
        if ( CE_SUCCESS != ce_status )
        {
            ce_status = CE_ERROR_FAILURE;
            break;
        }

        if ( 0 != memcmp(hash, hash_, hashlen) )
        {
            ce_status = CE_ERROR_INVALID_SIGNATURE;
        }
    }while( 0 );

    /* clean up */
    if ( dbMask )
        free(dbMask);

    if ( m_ )
        free(m_);

    if ( hash )
        free(hash);

    return ce_status;
}/* ce_pkcs1_pss_padding_verify */


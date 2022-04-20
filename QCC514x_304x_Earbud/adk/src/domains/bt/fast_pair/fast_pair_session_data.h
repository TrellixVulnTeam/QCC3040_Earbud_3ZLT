/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       fast_pair_session_data.h
\brief      Fast pairing module device database access.
*/

#ifndef FAST_PAIR_SESSION_DATA_H_
#define FAST_PAIR_SESSION_DATA_H_
#include "fast_pair_account_key_sync.h"

/*! \brief Get the Fast Pair anti spoofing private key

    This interface can be used to get Google provided ASPK
    
    \param uint8* aspk allocated buffer to copy anti-spoofing private key into.

    \return None
 */
void fastPair_GetAntiSpoofingPrivateKey(uint8* aspk);

/*! \brief Get the Fast Pair account keys

    This interface can be used to get fast pair account keys
    It's callers responsibility to free the account_keys after intended use.
    \param uint8** address of a pointer which will point to account keys buffer 

    \return uint16 Number of account keys in the buffer
 */
uint16 fastPair_GetAccountKeys(uint8** account_keys);

/*! \brief Get the number of Fast Pair account keys

    This interface can be used to get number of fast pair account keys

    \param None 

    \return uint16 Number of account keys
 */
uint16 fastPair_GetNumAccountKeys(void);

/*! \brief Store the Fast Pair account key

    This interface can be used to store a new fast pair account key

    \param uint8* Reference to account key buffer 

    \return bool TRUE if account key is stored else FALSE
 */
bool fastPair_StoreAccountKey(const uint8* account_key);

/*! \brief Store the Fast Pair account keys with the index values
 *
    This inteface can be used to store the complete fast pair account key info

    \param fast_pair_account_key_sync_req_t* reference to the fast pair account key info

    \return bool TRUE if account keys are stored else FALSE
 */
bool fastPair_StoreAllAccountKeys(fast_pair_account_key_sync_req_t* account_key_info);

/*! \brief Delete the Fast Pair account keys

    This interface can be used to delete all fast pair account keys in case of Factory Reset

    \param None 

    \return bool TRUE if all account keys are deleted else FALSE
 */
bool fastPair_DeleteAllAccountKeys(void);


/*! \brief Store the Fast Pair Personalized Name in persistent storage.
           Maximum allowed length is FAST_PAIR_PNAME_DATA_LEN.

    \param pname      Personalized Name buffer.
    \param pname_len  Length of Personalized Name buffer. maximum allowed value is FAST_PAIR_PNAME_DATA_LEN

    \return bool TRUE if Personalized Name is stored else FALSE
 */
bool fastPair_StorePName(const uint8 *pname, uint8 pname_len);

/*! \brief Read the Fast Pair Personalized Name from persistent storage.

    \param pname      Personalized Name buffer of size FAST_PAIR_PNAME_DATA_LEN to be read to.
    \param pname_len  Valid length of Personalized Name read.

    \return bool TRUE if Personalized Name is read else FALSE
 */
bool fastPair_GetPNameFromStore(uint8 pname[FAST_PAIR_PNAME_DATA_LEN], uint8 *pname_len);


/*! \brief Store the Fast Pair Personalized Name in persistent storage.
           The total size stored is FAST_PAIR_PNAME_STORAGE_LEN.
           The first byte holds the valid part of personalized name.
           Bytes from index 1 holds the personalized name in utf8 format.
           NOTE: This API should be used for PName Sync (Marshall) purposes only.

    \param pname Personalized Name buffer of size FAST_PAIR_PNAME_STORAGE_LEN

    \return bool TRUE if Personalized Name is stored else FALSE
 */
bool fastPair_StorePNameInPSStore(const uint8 pname[FAST_PAIR_PNAME_STORAGE_LEN]);

/*! \brief Set the Fast Pair scrambled ASPK fetched from PS.

    \param key scrambled aspk fetched from PS
    \param size_of_key scrambled aspk size

    \return None
 */
void fastPair_SetPrivateKey(const uint16 *key, unsigned size_of_key);


#endif /* FAST_PAIR_SESSION_DATA_H_ */

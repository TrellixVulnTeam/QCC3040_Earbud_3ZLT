/*!
\copyright  Copyright (c) 2018-20 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       fast_pair_session_data.c
\brief      Fast pairing module device database access
*/

#include "fast_pair_session_data.h"

#include <rsa_pss_constants.h>
#include <device.h>
#include <device_list.h>
#include <stdlib.h>
#include <byte_utils.h>
#include <panic.h>
#include <bt_device.h>
#include <logging.h>

#include <device_db_serialiser.h>
#include <device_properties.h>
#include <pddu_map.h>
#include "fast_pair_account_key_sync.h"
#include "fast_pair_config.h"

/* TODO below macros to be placed in common header file for FP */
#define MAX_FAST_PAIR_ACCOUNT_KEYS              (5)
#define FAST_PAIR_ACCOUNT_KEY_LEN               (16)
#define FAST_PAIR_PRIVATE_KEY_LEN               (32)

typedef struct fastpair_account_key_info
{
    uint16 account_key_index[MAX_FAST_PAIR_ACCOUNT_KEYS];
    uint8 account_keys[FAST_PAIR_ACCOUNT_KEY_LEN * MAX_FAST_PAIR_ACCOUNT_KEYS];
} fastpair_account_key_info_t;


/* Fast Pair seed */
const uint16 seed[FAST_PAIR_PRIVATE_KEY_LEN/2] = {0x11ac, 0x5a6e, 0x0e49, 0x5aa3, 0xe3e0, 0xbb20, 0xac0e, 0xf136, 0x5dfb, 0x5282, 0x002b, 0x37f2, 0x28f1,
0xd18c, 0xa613, 0x8de2};
/* Fast Pair scrambled anti spoofing private key */
const uint16 *private_key;

static void fastpair_serialise_persistent_device_data(device_t device, void *buf, uint8 offset)
{
    void *account_key_index_value = NULL;
    void *account_keys_value = NULL;
    size_t account_key_index_size = 0;
    size_t account_keys_size = 0;
    void *pname_value = NULL;
    size_t pname_size = 0;
    uint8 buf_accKey_offset=0;
    UNUSED(offset);

     /* store account key data to PS store*/
    if(Device_GetProperty(device, device_property_fast_pair_account_key_index, &account_key_index_value, &account_key_index_size) &&
        Device_GetProperty(device, device_property_fast_pair_account_keys, &account_keys_value, &account_keys_size))
    {
        fastpair_account_key_info_t *buffer = PanicUnlessMalloc(sizeof(fastpair_account_key_info_t));
        memcpy(buffer->account_key_index, account_key_index_value, sizeof(buffer->account_key_index));
        memcpy(buffer->account_keys, account_keys_value, sizeof(buffer->account_keys));
        memcpy(buf, buffer, sizeof(fastpair_account_key_info_t));
        free(buffer);

        buf_accKey_offset = sizeof(fastpair_account_key_info_t);
    }
    /* Store pname data to PS store */
    if(Device_GetProperty(device, device_property_fast_pair_personalized_name, &pname_value, &pname_size) )
    {
        uint8 *pname_buf = (uint8*)buf;
        pname_buf = pname_buf+buf_accKey_offset;
        memcpy(pname_buf, pname_value,sizeof(uint8)*FAST_PAIR_PNAME_STORAGE_LEN);
    }
    else
    {
        DEBUG_LOG("fastpair_serialise_persistent_device_data: Device_GetProperty(device_property_fast_pair_personalized_name) fails ");
    }
}

static void fastpair_deserialise_persistent_device_data(device_t device, void *buf, uint8 data_length, uint8 offset)
{
    UNUSED(offset);
    UNUSED(data_length);
    /* PS retrieve data to device database */
    fastpair_account_key_info_t *buffer = (fastpair_account_key_info_t *)buf;
    Device_SetProperty(device, device_property_fast_pair_account_key_index, &buffer->account_key_index, sizeof(buffer->account_key_index));
    Device_SetProperty(device, device_property_fast_pair_account_keys, &buffer->account_keys, sizeof(buffer->account_keys));

    uint8 fastpair_account_key_info_t_size = sizeof(fastpair_account_key_info_t);
    uint8 *pname_buf = (uint8 *)buf+fastpair_account_key_info_t_size;
    Device_SetProperty(device, device_property_fast_pair_personalized_name, pname_buf, sizeof(uint8)*FAST_PAIR_PNAME_STORAGE_LEN);
 }

static uint8 fastpair_get_device_data_len(device_t device)
{
    void *account_key_index_value = NULL;
    void *account_keys_value = NULL;
    void *pname_value = NULL;
    size_t account_key_index_size = 0;
    size_t account_keys_size = 0;
    size_t pname_size = 0;
    uint8 fast_pair_device_data_len = 0;

    if(DEVICE_TYPE_SELF == BtDevice_GetDeviceType(device))
    {
        if(Device_GetProperty(device, device_property_fast_pair_account_key_index, &account_key_index_value, &account_key_index_size) &&
           Device_GetProperty(device, device_property_fast_pair_account_keys, &account_keys_value, &account_keys_size))
        {
           PanicFalse(sizeof(fastpair_account_key_info_t) == (account_keys_size + account_key_index_size));
           fast_pair_device_data_len += sizeof(fastpair_account_key_info_t);

           if (Device_GetProperty(device, device_property_fast_pair_personalized_name, &pname_value, &pname_size))
           {
               PanicFalse((sizeof(uint8) * FAST_PAIR_PNAME_STORAGE_LEN) == pname_size);
               fast_pair_device_data_len += sizeof(uint8) * FAST_PAIR_PNAME_STORAGE_LEN;
           }
           return fast_pair_device_data_len;
        }
    }
    return 0;
}

static bool fastpair_duplicate_index_found(uint16* buffer, uint16 buffer_size)
{
    for(uint16 i = 0; i < buffer_size; i++)
    {
        /* Index 0xFFFF represents invalid (unused) index so should not be
            considered for checking duplicate entries */
        if(buffer[i] != 0xFFFF)
        {
            for(uint16 j = i+1; j < buffer_size; j++)
            {
                if(buffer[i] == buffer[j])
                {
                    DEBUG_LOG("Fastpair session data : Duplicate account key index found");
                    return TRUE;
                }
            }
        }
    }
    return FALSE;
}

static void fastpair_print_account_keys(uint16 num_keys, uint8* account_keys)
{
    DEBUG_LOG("Fastpair session data : Number of account keys %d", num_keys);
    DEBUG_LOG("Fastpair session data : Account keys : ");
    for(uint16 i=0; i<num_keys; i++)
    {
        DEBUG_LOG("%d) : ",i+1);
        for(uint16 j=0; j<FAST_PAIR_ACCOUNT_KEY_LEN; j++)
        {
            DEBUG_LOG("%02x ", (account_keys)[j+(i*FAST_PAIR_ACCOUNT_KEY_LEN)]);
        }
    }
}

/*! \brief Register Fast Pair PDDU
 */
void FastPair_RegisterPersistentDeviceDataUser(void)
{
    DeviceDbSerialiser_RegisterPersistentDeviceDataUser(
        PDDU_ID_FAST_PAIR,
        fastpair_get_device_data_len,
        fastpair_serialise_persistent_device_data,
        fastpair_deserialise_persistent_device_data);
}

void fastPair_SetPrivateKey(const uint16 *key, unsigned size_of_key)
{
    PanicNull((uint16 *)key);
    PanicFalse(size_of_key >= FAST_PAIR_PRIVATE_KEY_LEN/2);

    private_key = key;
}

/*! \brief Get the Fast Pair anti spoofing private key
 */
void fastPair_GetAntiSpoofingPrivateKey(uint8* aspk)
{
    uint16* unscrambled_aspk = PanicUnlessMalloc(FAST_PAIR_PRIVATE_KEY_LEN);

    for(uint16 i=0, j=FAST_PAIR_PRIVATE_KEY_LEN/2; i <(FAST_PAIR_PRIVATE_KEY_LEN/2) && j > 0; i++,j--)
    {
		/* Use last 16 words of M array in rsa_decrypt_constant_mod structure */
        unscrambled_aspk[i] = private_key[i] ^ rsa_decrypt_constant_mod.M[RSA_SIGNATURE_SIZE - j] ^ seed[i];
    }
    ByteUtilsMemCpyUnpackString(aspk, (const uint16 *)&unscrambled_aspk[0], FAST_PAIR_PRIVATE_KEY_LEN);

    DEBUG_LOG("Fastpair session data : Unscrambled ASPK : ");
    for(uint16 j=0; j<FAST_PAIR_PRIVATE_KEY_LEN; j++)
    {
        DEBUG_LOG("%02x ", aspk[j]);
    }
    free(unscrambled_aspk);
}

/*! \brief Get the Fast Pair account keys
 */
uint16 fastPair_GetAccountKeys(uint8** account_keys)
{
    uint16 num_keys = 0;
    
    deviceType type = DEVICE_TYPE_SELF;
    device_t my_device = DeviceList_GetFirstDeviceWithPropertyValue(device_property_type, &type, sizeof(deviceType));
    if(my_device)
    {
        void *account_key_index_value = NULL;
        void *account_keys_value = NULL;
        size_t account_key_index_size = 0;
        size_t account_keys_size = 0;
        if(Device_GetProperty(my_device, device_property_fast_pair_account_key_index, &account_key_index_value, &account_key_index_size) &&
            Device_GetProperty(my_device, device_property_fast_pair_account_keys, &account_keys_value, &account_keys_size))
        {
            fastpair_account_key_info_t *buffer = PanicUnlessMalloc(sizeof(fastpair_account_key_info_t));
            *account_keys = PanicUnlessMalloc(FAST_PAIR_ACCOUNT_KEY_LEN * MAX_FAST_PAIR_ACCOUNT_KEYS);
            memcpy(buffer->account_key_index, account_key_index_value, sizeof(buffer->account_key_index));
            memcpy(buffer->account_keys, account_keys_value, sizeof(buffer->account_keys));

            if(!fastpair_duplicate_index_found(buffer->account_key_index, MAX_FAST_PAIR_ACCOUNT_KEYS))
            {
                /* Validate account keys stored in Account key Index.
                   Check if account key Index values are less than  MAX_FAST_PAIR_ACCOUNT_KEYS
                   Account keys when read from PS store will be validated by checking first octet is 0x04
                 */
                for(uint16 count=0;count<MAX_FAST_PAIR_ACCOUNT_KEYS;count++)
                {
                    if((buffer->account_key_index[count] < MAX_FAST_PAIR_ACCOUNT_KEYS) &&
                        buffer->account_keys[buffer->account_key_index[count] * FAST_PAIR_ACCOUNT_KEY_LEN] == 0x04)
                    {
                        num_keys++;
                    }
                    else
                    {
                        /* No more valid account keys stored in PS */
                        break;
                    }
                }
                memcpy(*account_keys, buffer->account_keys, sizeof(buffer->account_keys));
            }
            fastpair_print_account_keys(num_keys, *account_keys);
            free(buffer);
        }
        else
        {
            /* No account keys were found return from here */
            DEBUG_LOG("Fastpair session data : Number of account keys %d", num_keys);
        }
    }
    else
    {
        DEBUG_LOG("Fastpair session data : Unexpected Error. Shouldn't have reached here");
    }
    return num_keys;
}

/*! \brief Get the Fast Pair account keys
 */
uint16 fastPair_GetNumAccountKeys(void)
{
    uint16 num_keys = 0;
    deviceType type = DEVICE_TYPE_SELF;

    device_t my_device = DeviceList_GetFirstDeviceWithPropertyValue(device_property_type, &type, sizeof(deviceType));
    if(my_device)
    {
        void *account_key_index_value = NULL;
        size_t account_key_index_size = 0;

        if(Device_GetProperty(my_device, device_property_fast_pair_account_key_index, &account_key_index_value, &account_key_index_size) &&
            account_key_index_size)
        {
            uint16 account_key_index[MAX_FAST_PAIR_ACCOUNT_KEYS];
            memset(account_key_index, 0xFF,sizeof(account_key_index));
            memcpy(account_key_index, account_key_index_value, sizeof(account_key_index));
            if(!fastpair_duplicate_index_found(account_key_index, MAX_FAST_PAIR_ACCOUNT_KEYS))
            {
                for(uint16 count=0;count<MAX_FAST_PAIR_ACCOUNT_KEYS;count++)
                {
                    if(account_key_index[count] < MAX_FAST_PAIR_ACCOUNT_KEYS)
                    {
                        num_keys++;
                    }
                    else
                    {
                        /* No more valid account keys stored in PS */
                        break;
                    }
                }
            }
        }
    }
    DEBUG_LOG("Fastpair session data : Number of account keys %d", num_keys);
    return num_keys;
}

/*! \brief Store the Fast Pair account key
    We can store up to 5 account keys i.e. 16*5 = 80 bytes of data. To maintain account key priorities & to handle duplicate account keys writes,
    an account key index is maintained. Account key index ranges from 0 to 4. Value at index 0 represents the most recent account key while value at index 4
    holds least used account key.Below illustration explains different scenarios :

    Account key index & account keys buffer will hold invalid value of 0XFF for unused slots.

    1. Assuming 5 account keys (A,B,C,D,E) are already written & a new account key (F) needs to be added, account key index & account key buffer will look as below :

        Account Key Index :  |4|3|2|1|0| --> Array of uint16 (Size : 5)
        Account Key Buffer : |A|B|C|D|E|--> Array of uint8 (Size : 80)

    ->New account Key F received :

        Account Key Index :  |0|4|3|2|1| --> Array of uint16 (Size : 5)
        Account Key Buffer : |F|B|C|D|E|--> Array of uint8 (Size : 80)

    2. Assuming 5 account keys (A,B,C,D,E) are already written & a duplicate account key (A) needs to be added, account key index & account key buffer will look as below :

        Account Key Index :  |4|3|2|1|0| --> Array of uint16 (Size : 5)
        Account Key Buffer : |A|B|C|D|E|--> Array of uint8 (Size : 80)

    ->Duplicate account Key A received :

        Account Key Index :  |0|4|3|2|1| --> Array of uint16 (Size : 5)
        Account Key Buffer : |A|B|C|D|E|--> Array of uint8 (Size : 80)
 */
bool fastPair_StoreAccountKey(const uint8* account_key)
{
    bool result = FALSE;

    /* Check if received account key is valid. First byte of account key must be 0x04 */
    if(account_key && account_key[0] == 0x04)
    {
        deviceType type = DEVICE_TYPE_SELF;
        /* First find the SELF deivce to add account keys to */
        device_t my_device = DeviceList_GetFirstDeviceWithPropertyValue(device_property_type, &type, sizeof(deviceType));

        DEBUG_LOG("Fastpair session data : Store account key :");
        for(uint16 i=0; i<FAST_PAIR_ACCOUNT_KEY_LEN; i++)
        {
            DEBUG_LOG("%02x ",account_key[i]);
        }

        if(my_device)
        {
            void *account_key_index_value = NULL;
            void *account_keys_value = NULL;
            size_t account_key_index_size = 0;
            size_t account_keys_size = 0;
            uint16 num_keys = 0;
            uint16 duplicate_account_key_index = 0xFF; /* Invalid Index as AKI varies from 0 to (MAX_FAST_PAIR_ACCOUNT_KEYS-1) */
            fastpair_account_key_info_t *buffer = PanicUnlessMalloc(sizeof(fastpair_account_key_info_t));

            memset(buffer, 0xFF, sizeof(fastpair_account_key_info_t));
            /* SELF device is found, check whether account key index & account keys properties exists on SELF device */
            if(Device_GetProperty(my_device, device_property_fast_pair_account_key_index, &account_key_index_value, &account_key_index_size) &&
                Device_GetProperty(my_device, device_property_fast_pair_account_keys, &account_keys_value, &account_keys_size))
            {
                memcpy(buffer->account_key_index, account_key_index_value, sizeof(buffer->account_key_index));
                memcpy(buffer->account_keys, account_keys_value, sizeof(buffer->account_keys));

                 /* Validate account keys stored in Account key Index. 
                   Check if account key Index values are less than  MAX_FAST_PAIR_ACCOUNT_KEYS
                   Account keys when read from PS store will be validated by checking first octet is 0x04 
                 */
                if(fastpair_duplicate_index_found(buffer->account_key_index, MAX_FAST_PAIR_ACCOUNT_KEYS))
                {
                    /* Duplicate account key index suggests data corruption, don't proceed */
                    free(buffer);
                    return result;
                }
                for(uint16 count=0;count<MAX_FAST_PAIR_ACCOUNT_KEYS;count++)
                {
                    if(buffer->account_key_index[count] < MAX_FAST_PAIR_ACCOUNT_KEYS)
                    {
                        num_keys++;
                    }
                    else
                    {
                        /* No more valid account keys stored in PS */ 
                        break;
                    }
                }
                /* Newly added account key will always have highest priority. 
                   The account key list will also point from highest to lowest priority. 
                 */
                for(uint16 count = 0; count < num_keys; count++)
                {
                    if(memcmp(account_key, &buffer->account_keys[buffer->account_key_index[count] * FAST_PAIR_ACCOUNT_KEY_LEN], FAST_PAIR_ACCOUNT_KEY_LEN) == 0)
                    {
                        /* Found duplicate account key. Remove duplicate key by removing earlier written value */
                        duplicate_account_key_index = count;
                        break;
                    }
                }
                if(duplicate_account_key_index == 0xFF)
                {
                    /* No duplicate account key found. Add to existing list.*/
                    DEBUG_LOG("Fastpair session data : No duplicate account key found. Add to existing list");
                    /* If list is not already full, add to the account key list */
                    if(num_keys < MAX_FAST_PAIR_ACCOUNT_KEYS)
                    {
                        /* If the account key list is not full then only account key positions from 0 to num_keys -1 
                           are utilized. The num_keys should be free to use. Store the new account key */
                        memcpy(&buffer->account_keys[num_keys * FAST_PAIR_ACCOUNT_KEY_LEN], account_key, FAST_PAIR_ACCOUNT_KEY_LEN);

                        /* Update account key Index */
                        for(uint16 count = num_keys; count > 0 ; count--)
                        {
                            buffer->account_key_index[count] = buffer->account_key_index[count-1];
                        }
                        buffer->account_key_index[0] = num_keys;

                        /* Update number of account keys */
                        num_keys++;
                    }
                    else if(num_keys == MAX_FAST_PAIR_ACCOUNT_KEYS)
                    {
                        /* Account key index will point to most recently used account key to least recently account key.
                           Account key Index 0 will always point to the most recently used account key and account key Index
                           and MAX_FAST_PAIR_ACCOUNT_KEYS-1 index will have least recently used location of account key
                         */
                        uint16 temp = buffer->account_key_index[MAX_FAST_PAIR_ACCOUNT_KEYS-1];

                        /* Copy the account key */
                        memcpy(&buffer->account_keys[temp * FAST_PAIR_ACCOUNT_KEY_LEN], account_key, FAST_PAIR_ACCOUNT_KEY_LEN);

                        /* Update account key Index */
                        for(uint16 count = num_keys; count > 1; count--)
                        {
                            buffer->account_key_index[count-1] = buffer->account_key_index[count-2];
                        }
                        buffer->account_key_index[0] = temp;

                    }
                    else
                    {
                        /* Should not reach here */
                        DEBUG_LOG("Fastpair session data : Account key number mismatch!");
                    }
                }
                else
                {
                    DEBUG_LOG("Fastpair session data : Duplicate account key is found");
                    /* Duplicate account key found. Remove that and update index 0 to duplicate key */
                    if(duplicate_account_key_index != 0)
                    {
                        /* Account key index will point to most recently used account key to least recently account key.
                           Account key Index 0 will always point to the most recently used account key and account key Index
                           and MAX_FAST_PAIR_ACCOUNT_KEYS-1 index will have least recently used location of account key
                         */
                        uint16 temp = buffer->account_key_index[duplicate_account_key_index];

                        /* Update account key Index */
                        for(uint16 count = duplicate_account_key_index; count > 0; count--)
                        {
                            buffer->account_key_index[count] = buffer->account_key_index[count-1];
                        }
                        buffer->account_key_index[0] = temp;
                    }
                }
                /* Store Account key Index and Account keys to PS Store */
                Device_SetProperty(my_device, device_property_fast_pair_account_key_index, &buffer->account_key_index, sizeof(buffer->account_key_index));
                Device_SetProperty(my_device, device_property_fast_pair_account_keys, &buffer->account_keys, sizeof(buffer->account_keys));
                free(buffer);
                DeviceDbSerialiser_Serialise();
                DEBUG_LOG("Fastpair session data : Number of account keys %d", num_keys);
            }
            else
            {
                /* This is the first account key getting written, add it to the first slot */
                buffer->account_key_index[0] = 0x0000;
                memcpy(&buffer->account_keys[0], account_key, FAST_PAIR_ACCOUNT_KEY_LEN);
                Device_SetProperty(my_device, device_property_fast_pair_account_key_index, &buffer->account_key_index, sizeof(buffer->account_key_index));
                Device_SetProperty(my_device, device_property_fast_pair_account_keys, &buffer->account_keys, sizeof(buffer->account_keys));
                free(buffer);
                DeviceDbSerialiser_Serialise();
                DEBUG_LOG("Fastpair session data : Number of account keys %d", num_keys);
            }
            result = TRUE;
        }
        else
        {
            DEBUG_LOG("Fastpair session data : Unexpected Error. Shouldn't have reached here");
        }
    }
    else
    {
        DEBUG_LOG("Fastpair session data : Invalid account key received");
    }
    return result;
}

/*! \brief Store the Fast Pair account keys with the index values
 */
bool fastPair_StoreAllAccountKeys(fast_pair_account_key_sync_req_t *account_key_info)
{
    bool result = FALSE;
    deviceType type = DEVICE_TYPE_SELF;
    /* Find the SELF deivce to add account keys to */
    device_t my_device = DeviceList_GetFirstDeviceWithPropertyValue(device_property_type, &type, sizeof(deviceType));
    if(my_device)
    {
        DEBUG_LOG("Fastpair session data : Storing the complete account key info.");
        Device_SetProperty(my_device, device_property_fast_pair_account_key_index, &account_key_info->account_key_index, sizeof(account_key_info->account_key_index));
        Device_SetProperty(my_device, device_property_fast_pair_account_keys, &account_key_info->account_keys, sizeof(account_key_info->account_keys));

        result = TRUE;
        DeviceDbSerialiser_Serialise();
    }
    return result;
}

/*! \brief Delete the Fast Pair account keys
 */
bool fastPair_DeleteAllAccountKeys(void)
{
    bool result = FALSE;
    deviceType type = DEVICE_TYPE_SELF;
    device_t my_device = DeviceList_GetFirstDeviceWithPropertyValue(device_property_type, &type, sizeof(deviceType));

    DEBUG_LOG("Fastpair session data : Delete all account keys");
    if(my_device)
    {
        void *account_key_index_value = NULL;
        void *account_keys_value = NULL;
        size_t account_key_index_size = 0;
        size_t account_keys_size = 0;
        if(Device_GetProperty(my_device, device_property_fast_pair_account_key_index, &account_key_index_value, &account_key_index_size) &&
            Device_GetProperty(my_device, device_property_fast_pair_account_keys, &account_keys_value, &account_keys_size))
        {
            fastpair_account_key_info_t *buffer = PanicUnlessMalloc(sizeof(fastpair_account_key_info_t));
            memset(buffer, 0xFF, sizeof(fastpair_account_key_info_t));

            Device_SetProperty(my_device, device_property_fast_pair_account_key_index, &buffer->account_key_index, sizeof(buffer->account_key_index));
            Device_SetProperty(my_device, device_property_fast_pair_account_keys, &buffer->account_keys, sizeof(buffer->account_keys));

            free(buffer);
            DeviceDbSerialiser_Serialise();
            result = TRUE;
        }
    }
    else
    {
        DEBUG_LOG("Fastpair session data : Unexpected Error. Shouldn't have reached here");
    }
    return result;
}

/*****      PERSONALIZED NAME      *****/

/*! \brief Store the personalized name in persistent storage.
 */
bool fastPair_StorePNameInPSStore(const uint8 pname[FAST_PAIR_PNAME_STORAGE_LEN])
{
    deviceType type = DEVICE_TYPE_SELF;
    /* First find the SELF deivce to add pname to */
    device_t my_device = DeviceList_GetFirstDeviceWithPropertyValue(device_property_type, &type, sizeof(deviceType));

    if(NULL == my_device)
    {
        DEBUG_LOG_ERROR("fastPair_StorePName : Unexpected Error. Shouldn't have reached here");
        return FALSE;
    }

    /* Store personalized name to PS Store */
    Device_SetProperty(my_device, device_property_fast_pair_personalized_name, pname, sizeof(uint8)*FAST_PAIR_PNAME_STORAGE_LEN);
    DeviceDbSerialiser_Serialise();

    return TRUE;
}

/*! \brief Store the personalized name of length pname_len in persistent storage.
 */
bool fastPair_StorePName(const uint8 *pname, uint8 pname_len)
{
    uint8 buffer[FAST_PAIR_PNAME_STORAGE_LEN];
    if((NULL == pname)||(0 == pname_len))
    {
        DEBUG_LOG_ERROR("fastPair_StorePName: pname is null or length of pname is zero.");
        return FALSE;
    }
    else if(pname_len > FAST_PAIR_PNAME_DATA_LEN)
    {
        buffer[0] = FAST_PAIR_PNAME_DATA_LEN;
        DEBUG_LOG("fastPair_StorePName: Pname length - %d which is more than 64 bytes, truncate it to 64 bytes.",pname_len);
        memcpy(buffer+(uint8)sizeof(uint8), pname, FAST_PAIR_PNAME_DATA_LEN);
    }
    else
    {
        buffer[0]=pname_len;
        memcpy(buffer+(uint8)sizeof(uint8),pname,pname_len*sizeof(uint8));
    }
    fastPair_StorePNameInPSStore(buffer);
    return TRUE;
}

/*! \brief Read the personalized name from persistent storage.
 */
bool fastPair_GetPNameFromStore(uint8 pname[FAST_PAIR_PNAME_DATA_LEN], uint8 *pname_len)
{
    deviceType type = DEVICE_TYPE_SELF;
    size_t pname_ps_size = 0;
    void *pname_ps_ptr=NULL;
    uint8 *pname_ptr=NULL;
    device_t my_device;

    if(NULL == pname_len)
    {
        DEBUG_LOG_ERROR("fastPair_GetPNameFromStore : pname_len input parameter is NULL");
        return FALSE;
    }

    my_device = DeviceList_GetFirstDeviceWithPropertyValue(device_property_type, &type, sizeof(deviceType));
    if(NULL == my_device)
    {
        DEBUG_LOG_ERROR("fastPair_GetPNameFromStore : Unexpected Error. Shouldn't have reached here");
        return FALSE;
    }

    if(Device_GetProperty(my_device, device_property_fast_pair_personalized_name, &pname_ps_ptr, &pname_ps_size) )
    {
        DEBUG_LOG("fastPair_GetPNameFromStore: Read personalized name of length %d from persistent store", pname_ps_size);
        if(FAST_PAIR_PNAME_STORAGE_LEN != pname_ps_size)
        {
            DEBUG_LOG_ERROR("fastPair_GetPNameFromStore: Unexpected Error. Read length %d is not equal to FAST_PAIR_PNAME_STORAGE_LEN",pname_ps_size);
            return FALSE;
        }

        /* Copy the length to pname_len and pname data to pname array */
        memcpy(pname_len,pname_ps_ptr,sizeof(uint8) );
        /* If the pname string length is 0 or more than FAST_PAIR_PNAME_DATA_LEN */
        if((0 == *pname_len)||(*pname_len > FAST_PAIR_PNAME_DATA_LEN))
        {
            DEBUG_LOG_ERROR("fastPair_GetPNameFromStore : Invalid pname length of %d. So, ignoring pname",*pname_len );
            return FALSE;
        }
        pname_ptr = (uint8 *)pname_ps_ptr + (uint8)(sizeof(uint8));
        memcpy(pname, pname_ptr, FAST_PAIR_PNAME_DATA_LEN*sizeof(uint8));
        return TRUE;
    }
    else
    {
        DEBUG_LOG_ERROR("FastPair_GetPName : Device_GetProperty API fails to read personalized name");
        return FALSE;
    }
}

/*! \brief Delete the personalized name
 */
void FastPair_DeletePName(void)
{
    uint8 pname[FAST_PAIR_PNAME_STORAGE_LEN];
    /* Set the personalized name to 0. */
    memset(pname, 0x00, FAST_PAIR_PNAME_STORAGE_LEN*sizeof(uint8));

    fastPair_StorePNameInPSStore(pname);
}


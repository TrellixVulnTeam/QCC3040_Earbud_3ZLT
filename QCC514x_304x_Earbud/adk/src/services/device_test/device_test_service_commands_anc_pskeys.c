/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    device_test_service
\brief      Implementation of device test service commands for handling ANC specific audio persistent
             storage keys.
*/
/*! \addtogroup device_test_service
@{
*/

#include "device_test_service.h"
#include "device_test_service_auth.h"
#include "device_test_service_commands_helper.h"
#include "device_test_parse.h"

#include <panic.h>

#include <ps.h>
#include <logging.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>


/*! WARNING: These are ANC specific defines. It is applicable to QCC_514x and QCC_515x devices.
      Not necessasirly applicable to QCC_516x series.
*/
#define ANC_CONFIG_HEADER_SIZE   (3)
#define ANC_CONFIG_INSTANCE0_OFFSET   (ANC_CONFIG_HEADER_SIZE)
#define ANC_CONFIG_KEY_LEN_PER_INSTANCE (184) /*In words. See anc_tuning_data.h */
#define ANC_CONFIG_INSTANCE1_OFFSET   (ANC_CONFIG_INSTANCE0_OFFSET + ANC_CONFIG_KEY_LEN_PER_INSTANCE)

#define ANC_CONFIG_INSTANCE_0 0
#define ANC_CONFIG_INSTANCE_1 1

#define ANC_CONFIG_TOTAL_SIZE ((ANC_CONFIG_KEY_LEN_PER_INSTANCE*2) + ANC_CONFIG_HEADER_SIZE)/*in words*/

/*! The maximum length of PSKEY that can be supported 

    This length will be honoured for reading PSKEYS. Pskeys of this length
    can be written if the command reaches here. The length may be restricted
    elsewhere in the system */
#define PSKEY_MAX_LENGTH_SUPPORTED  200

#define MAX_AUDIO_PS_KEY_BITS 32

/*! The base part of the response to ANCGETPSKEY */
#define ANCGETPSKEY_RESPONSE "+ANCGETPSKEY:"

/*! Worst case variable length portion for ANCGETPSKEY */
#define ANCGETPSKEY_RESPONSE_VARIABLE_EXAMPLE "519,65535,"

/*! Format string for the variable portion of ANCGETPSKEY_RESPONSE response */
#define ANCGETPSKEY_RESPONSE_FORMAT "0x%x, 0x%x, "

/*! The length of the full response, including the maximum length of 
    any variable portion. As we use sizeof() this will include the 
    terminating NULL character. 

    Common value used for PSKEYGET and PSKEYMIBGET. PSKEYGET has 
    longer response */
#define BASE_PSKEY_RESPONSE_LEN (sizeof(ANCGETPSKEY_RESPONSE) + sizeof(ANCGETPSKEY_RESPONSE_VARIABLE_EXAMPLE))

/*! Length of local buffer for response to a PSKEYGET */
#define PSKEY_BUFFER_BYTES 121

/*! Length of each word in the response string
    4 hex digits and a space */
#define PSKEY_LENGTH_OF_WORD_IN_RESPONSE 5

/*! Helper macro to find out how many words of a response can fit in a specified length 
    Allow for a NULL terminator added by use of sprintf */
#define PSKEY_NUM_WORDS_IN(_len) ((_len - 1) / PSKEY_LENGTH_OF_WORD_IN_RESPONSE)

/*! Number of words to fit in the first part of response */
#define PSKEY_NUM_WORDS_IN_FIRST_RESPONSE  PSKEY_NUM_WORDS_IN(PSKEY_BUFFER_BYTES - BASE_PSKEY_RESPONSE_LEN)

/*! Number of words in a continuation response */
#define PSKEY_NUM_WORDS_IN_RESPONSE PSKEY_NUM_WORDS_IN(PSKEY_BUFFER_BYTES)


/*! Macro to map between local and global PSKEY identifiers */
#define PSKEY_MAP(_START, _END, _INTERNAL) { (_START), (_END), (_INTERNAL), (_INTERNAL + ((_END)-(_START))) }

#define HEXDIGIT_IN_BITS          4
#define HexDigitsForNumberOfBits(_bits)   (((_bits) + 3) / HEXDIGIT_IN_BITS)


static bool deviceTestService_extractAudioKeyId(const struct sequence *incoming_string, unsigned max_length_bits, uint32 *result)
{
    unsigned max_length_hex_digits = HexDigitsForNumberOfBits(max_length_bits);
    unsigned length = incoming_string->length;
    uint32 value = 0;
    const uint8 *hex_digits = incoming_string->data;

    *result = (uint32)-1;   /* Prepopulate with an error value */

    if (length == 0)
    {
        return FALSE;
    }

    while (length && max_length_hex_digits)
    {
        value <<= HEXDIGIT_IN_BITS;
        value = value + deviceTestService_HexToNumber(*hex_digits);

        hex_digits++;
        length--;
        max_length_hex_digits--;
    }

    *result = value;
    return TRUE;
}


/*! Command handler for AT + ANCSETPSKEY = pskey, instance, value

    This function sets the specified key to the requested value.

    Errors are reported if the requested key is not supported, or if
    the value cannot be validated.
    
    ANC Instance 0 and 1 contains 184 words of data each.
    Two instances are sent seperately to accomodate the data in view of memory and transport constraints.

    \param[in] task The task to be used in command responses
    \param[in] set  The parameters from the received command
 */
void DeviceTestServiceCommand_HandleAncSetPsKey(Task task,
                            const struct DeviceTestServiceCommand_HandleAncSetPsKey *set)
{
    uint32 audio_key;
    bool key_ok = deviceTestService_extractAudioKeyId(&set->pskey, MAX_AUDIO_PS_KEY_BITS, &audio_key);
    
    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleAncSetPsKey audio key:0x%x", audio_key);
    
    /* Allocate an extra word to simplify length checking */
    uint16 key_to_store[PSKEY_MAX_LENGTH_SUPPORTED+1];

    if (!DeviceTestService_CommandsAllowed() || !key_ok)
    {
        DeviceTestService_CommandResponseError(task);
        return;
    }

    unsigned nibbles = 0;
    unsigned string_len = set->value.length;
    const uint8 *input = set->value.data;
    uint16 value = 0;
    uint16 *next_word = key_to_store;
    bool error = FALSE;
    unsigned key_len = 0;
    
    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleAncSetPsKey string_len:%d", string_len);

    while (string_len-- && (key_len <= PSKEY_MAX_LENGTH_SUPPORTED))
    {
        char ch = *input++;

        if (!isxdigit(ch))
        {
            if (ch == ' ')
            {
                if (nibbles == 0)
                {
                    continue;
                }
            }
            error = TRUE;
            break;
        }
        value = (value << 4) + deviceTestService_HexToNumber(ch);
        if (++nibbles == 4)
        {
            *next_word++ = value;
            value = 0;
            nibbles = 0;
            key_len++;
        }
    }

    DEBUG_LOG_VERBOSE("DeviceTestServiceCommand_HandleAncSetPsKey audio key:%d, length:%d ",
                         audio_key, key_len);

    if (!error && nibbles == 0 && key_len==ANC_CONFIG_KEY_LEN_PER_INSTANCE)
    {
        uint16 total_key_length = 0;
        uint16 full_ps_key[ANC_CONFIG_TOTAL_SIZE]={0};
        
        if  ((PsReadAudioKey(audio_key, full_ps_key, ANC_CONFIG_TOTAL_SIZE, 0, &total_key_length) == ANC_CONFIG_TOTAL_SIZE)
                && (total_key_length == ANC_CONFIG_TOTAL_SIZE))
        {        
            uint16 offset = (set->instance==ANC_CONFIG_INSTANCE_0)? (ANC_CONFIG_INSTANCE0_OFFSET):(ANC_CONFIG_INSTANCE1_OFFSET);
            
            DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleAncSetPsKey offset: %d", offset);
            
            memcpy(&full_ps_key[offset], key_to_store, ANC_CONFIG_KEY_LEN_PER_INSTANCE * sizeof(uint16));
            
            if (PsUpdateAudioKey(audio_key, full_ps_key, total_key_length, 0, total_key_length))
            {            
                DeviceTestService_CommandResponseOk(task);
                return;
            }
        }
    }

    DEBUG_LOG_DEBUG("DeviceTestServiceCommand_HandleAncSetPsKey. Failed. Key:%d Error:%d Attempted length:%d",
                    audio_key, error, key_len);
    
    DeviceTestService_CommandResponseError(task);
}

/*! \brief Helper function to complete a PSKEY read response

    Starts with a buffer pre-populated with the start of the response.
    The buffer is then completed with the remaining data from the key.
    If the buffer is filled, then it is sent as a response and the next
    section(s) of response loaded into the buffer.

    \param task             The task information for the received command
    \param buffer           The start of the buffer to be used for output
    \param next_char        The next character to use in buffer
    \param key              The PSKEY data to be included in the response
    \param key_length_words The length of the PSKEY data
 */
static void deviceTestService_CompleteAncPskeyResponse(Task task,
                                                    char *buffer, char *next_char,
                                                    const uint16 *key, uint16 key_length_words)
{
    unsigned index_into_key = 0;
    unsigned portion_end = PSKEY_NUM_WORDS_IN_FIRST_RESPONSE;

    /* Now populate each response, adjusting variables for the next response
       (if any is needed) */
    while (index_into_key < key_length_words)
    {
        while (index_into_key < portion_end && index_into_key < key_length_words)
        {
            next_char += sprintf(next_char,"%04X ",key[index_into_key++]);
        }
        DeviceTestService_CommandResponsePartial(task,
                                                 buffer, next_char - buffer,
                                                 index_into_key <= PSKEY_NUM_WORDS_IN_FIRST_RESPONSE,
                                                 index_into_key >= key_length_words);

        next_char = buffer;
        portion_end += PSKEY_NUM_WORDS_IN_RESPONSE;
    }
    DeviceTestService_CommandResponseOk(task);
}

/*! Command handler for AT + ANCGETPSKEY = pskey, instance

    This function read the specified audio key for specified Instance and sends its value as a response 
    followed by OK.

    An error is reported if the requested key is not supported.

    \param[in] task The task to be used in command responses
    \param[in] get  The parameters from the received command
 */
    void DeviceTestServiceCommand_HandleAncGetPsKey(Task task,
                                const struct DeviceTestServiceCommand_HandleAncGetPsKey *get)
{
    uint32 audio_key;
    char   response[PSKEY_BUFFER_BYTES];
    uint16 retrieved_key[ANC_CONFIG_KEY_LEN_PER_INSTANCE]={0};
    uint16 instance_data_len = ANC_CONFIG_KEY_LEN_PER_INSTANCE;
    uint16 total_key_length = 0;

    bool key_ok = deviceTestService_extractAudioKeyId(&get->pskey, MAX_AUDIO_PS_KEY_BITS, &audio_key);

    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleAncGetPsKey audio ps key: 0x%x", audio_key);

    if (!DeviceTestService_CommandsAllowed() || !key_ok)
    {
        DeviceTestService_CommandResponseError(task);
        return;
    }
       
    uint16 offset = (get->instance==ANC_CONFIG_INSTANCE_0) ? (ANC_CONFIG_INSTANCE0_OFFSET):(ANC_CONFIG_INSTANCE1_OFFSET);    

    if (PsReadAudioKey(audio_key, retrieved_key, instance_data_len,
            offset, &total_key_length) == instance_data_len)
    {
        char *next_char = response;
        
        DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleAncGetPsKey. Total Len:%d, offset: %d, Instance data len: %d", total_key_length, offset, instance_data_len);
        
        /* Start the response with the text for ANCGETPSKEY */
        next_char += sprintf(next_char, ANCGETPSKEY_RESPONSE ANCGETPSKEY_RESPONSE_FORMAT, audio_key, get->instance);

        deviceTestService_CompleteAncPskeyResponse(task, response, next_char, retrieved_key, instance_data_len);
        return;
    }

    DeviceTestService_CommandResponseError(task);
}



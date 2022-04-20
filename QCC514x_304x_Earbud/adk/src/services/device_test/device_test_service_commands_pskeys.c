/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    device_test_service
\brief      Implementation of device test service commands for handling persistent
            storage keys.
*/
/*! \addtogroup device_test_service
@{
*/

#include "device_test_service.h"
#include "device_test_service_auth.h"
#include "device_test_service_commands_helper.h"
#include "device_test_parse.h"

#include <ps.h>
#include <logging.h>
#include <stdio.h>
#include <ctype.h>

/*! The maximum length of PSKEY that can be supported 

    This length will be honoured for reading PSKEYS. Pskeys of this length
    can be written if the command reaches here. The length may be restricted
    elsewhere in the system */
#define PSKEY_MAX_LENGTH_SUPPORTED  160

/*! The base part of the response to PSKEYGET */
#define PSKEYGET_RESPONSE "+PSKEYGET:"
/*! The base part of the response to PSKEYMIBGET */
#define PSKEYMIBGET_RESPONSE "+PSKEYMIBGET:"

/*! Format string for the variable portion of PSKEYGET response */
#define PSKEYGET_RESPONSE_VARIABLE_FMT "%d,%d,"
/*! Format string for the variable portion of PSKEYMIBGET response */
#define PSKEYMIBGET_RESPONSE_VARIABLE_FMT "%d,"

/*! Worst case variable length portion for PSKEYGET */
#define PSKEYGET_RESPONSE_VARIABLE_EXAMPLE "519,65535,"
/*! Worst case variable length portion for PSKEYMIBGET */
#define PSKEYMIBGET_RESPONSE_VARIABLE_EXAMPLE "65535,"

/*! The length of the full response, including the maximum length of 
    any variable portion. As we use sizeof() this will include the 
    terminating NULL character. 

    Common value used for PSKEYGET and PSKEYMIBGET. PSKEYGET has 
    longer response */
#define BASE_PSKEY_RESPONSE_LEN (sizeof(PSKEYGET_RESPONSE) + sizeof(PSKEYGET_RESPONSE_VARIABLE_EXAMPLE))

COMPILE_TIME_ASSERT(   sizeof(PSKEYGET_RESPONSE) + sizeof(PSKEYGET_RESPONSE_VARIABLE_EXAMPLE)
                    >= sizeof(PSKEYMIBGET_RESPONSE) + sizeof(PSKEYMIBGET_RESPONSE_VARIABLE_EXAMPLE),
                    pskey_get_is_not_the_longest_response);

/*! Length of local buffer for response to a PSKEYGET */
#define PSKEY_BUFFER_BYTES 121

/*! Length of each word in the response string

    4 hex digits and a space */
#define PSKEY_LENGTH_OF_WORD_IN_RESPONSE 5

/* Should be able to fit at least one word in response buffer */
COMPILE_TIME_ASSERT(PSKEY_BUFFER_BYTES >= BASE_PSKEY_RESPONSE_LEN + PSKEY_LENGTH_OF_WORD_IN_RESPONSE,
                    no_space_for_any_words_in_pskey_response);

/*! Helper macro to find out how many words of a response can fit in a specified length 
    Allow for a NULL terminator added by use of sprintf */
#define PSKEY_NUM_WORDS_IN(_len) ((_len - 1) / PSKEY_LENGTH_OF_WORD_IN_RESPONSE)

/*! Number of words to fit in the first part of response */
#define PSKEY_NUM_WORDS_IN_FIRST_RESPONSE  PSKEY_NUM_WORDS_IN(PSKEY_BUFFER_BYTES - BASE_PSKEY_RESPONSE_LEN)

/*! Number of words in a continuation response */
#define PSKEY_NUM_WORDS_IN_RESPONSE PSKEY_NUM_WORDS_IN(PSKEY_BUFFER_BYTES)


/*! Macro to map between local and global PSKEY identifiers */
#define PSKEY_MAP(_START, _END, _INTERNAL) { (_START), (_END), (_INTERNAL), (_INTERNAL + ((_END)-(_START))) }

/*! Structure containing the complete mapping between PSKEY identifiers */
static const struct 
{
    uint16 global_key_id_start;
    uint16 global_key_id_end;
    uint16 local_key_id_start;
    uint16 local_key_id_end;
} pskey_range_map[] = {
        PSKEY_MAP(PSKEY_USR0,       PSKEY_USR49, 0),
        PSKEY_MAP(PSKEY_DSP0,       PSKEY_DSP49, 50),
        PSKEY_MAP(PSKEY_CONNLIB0,   PSKEY_CONNLIB49, 100),
        PSKEY_MAP(PSKEY_USR50,      PSKEY_USR99, 150),
        PSKEY_MAP(PSKEY_CUSTOMER0,  PSKEY_CUSTOMER89, 200),
        PSKEY_MAP(PSKEY_READONLY0,  PSKEY_READONLY9, 290),
        PSKEY_MAP(PSKEY_CUSTOMER90, PSKEY_CUSTOMER299, 300),
        PSKEY_MAP(PSKEY_UPGRADE0,   PSKEY_UPGRADE9, 510),
};

/*! \brief convert a supplied pskey ID to an internal number 

    \param id User supplied pskey ID. Could be internal, or PSKEY ID

    \return The local/internal key id used by PsRetrieve(), PsStore().
            If the ID is not mapped, then the original ID is returned.
*/
static uint16  deviceTestServiceCommand_GetInternalId(uint16 id)
{
    unsigned map_entry = ARRAY_DIM(pskey_range_map);

    if (id <= 519)
    {
        /* Already an internal ID */
        return id;
    }

    while (map_entry--)
    {
        if (   pskey_range_map[map_entry].global_key_id_start <= id
            && id <= pskey_range_map[map_entry].global_key_id_end)
        {
            return    pskey_range_map[map_entry].local_key_id_start 
                   + (id - pskey_range_map[map_entry].global_key_id_start);
        }
    }

    DEBUG_LOG_WARN("DeviceTestServiceCommand_GetInternalId accessing unmapped PSKEY %d (0x%x)", id, id);
    return id;
}

/*! \brief convert a supplied internal pskey ID to the external PSKEY ID

    \param local_id The local identifier

    \return The matching global PSKEY ID
*/
static uint16 deviceTestServiceCommand_GetGlobalId(uint16 local_id)
{
    unsigned map_entry = ARRAY_DIM(pskey_range_map);

    while (map_entry--)
    {
        if (   pskey_range_map[map_entry].local_key_id_start <= local_id 
            && local_id <= pskey_range_map[map_entry].local_key_id_end)
        {
            return    pskey_range_map[map_entry].global_key_id_start 
                   + (local_id - pskey_range_map[map_entry].local_key_id_start);
        }
    }

    /* Garbage in, garbage out. This should never happen, but Panic doesnt seem like a good idea. */
    return local_id;
}


/*! Command handler for AT + PSKEYSET = pskey, value

    This function sets the specified key to the requested value.

    Errors are reported if the requested key is not supported, or if
    the value cannot be validated.

    \param[in] task The task to be used in command responses
    \param[in] set  The parameters from the received command
 */
void DeviceTestServiceCommand_HandlePskeySet(Task task,
                            const struct DeviceTestServiceCommand_HandlePskeySet *set)
{
    unsigned key = set->pskey;
    uint16 local_key = deviceTestServiceCommand_GetInternalId(key);
    /* Allocate an extra word to simplify length checking */
    uint16 key_to_store[PSKEY_MAX_LENGTH_SUPPORTED+1];

    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandlePskeySet. pskey:%d (local:%d)", key, local_key);

    if (   !DeviceTestService_CommandsAllowed())
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

    if (!error && nibbles == 0 && key_len && key_len <= PSKEY_MAX_LENGTH_SUPPORTED)
    {
        DEBUG_LOG_VERBOSE("DeviceTestServiceCommand_HandlePskeySet. Storing key:%d length:%d",
                            local_key, key_len);

        if (PsStore(local_key, key_to_store, key_len))
        {
            DeviceTestService_CommandResponseOk(task);
            return;
        }
        else
        {
            DEBUG_LOG_WARN("DeviceTestServiceCommand_HandlePskeySet. Failed to write. Defragging.");

            PsDefragBlocking();
            if (PsStore(local_key, key_to_store, key_len))
            {
                DeviceTestService_CommandResponseOk(task);
                return;
            }
        }
    }

    DEBUG_LOG_DEBUG("DeviceTestServiceCommand_HandlePskeySet. Failed. Key:%d Error:%d Attempted length:%d",
                    local_key, error, key_len);
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
static void deviceTestService_CompletePskeyResponse(Task task, 
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


/*! Command handler for AT + PSKEYGET = pskey, value

    This function read the specified key and sends its value as a response 
    followed by OK.

    An error is reported if the requested key is not supported.

    \param[in] task The task to be used in command responses
    \param[in] get  The parameters from the received command
 */
void DeviceTestServiceCommand_HandlePskeyGet(Task task,
                            const struct DeviceTestServiceCommand_HandlePskeyGet *get)
{
    uint16 key = get->pskey;
    uint16 local_key = deviceTestServiceCommand_GetInternalId(key);
    uint16 key_length_words;
    uint16 retrieved_key[PSKEY_MAX_LENGTH_SUPPORTED];
    char   response[PSKEY_BUFFER_BYTES];

    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandlePskeyGet. pskey:%d (local:%d)", key, local_key);

    if (   !DeviceTestService_CommandsAllowed())
    {
        DeviceTestService_CommandResponseError(task);
        return;
    }

    key_length_words = PsRetrieve(local_key, NULL, 0);
    if (key_length_words && key_length_words <= PSKEY_MAX_LENGTH_SUPPORTED)
    {
        if (PsRetrieve(local_key, retrieved_key, key_length_words) == key_length_words)
        {
            uint16 global_key = deviceTestServiceCommand_GetGlobalId(local_key);
            char *next_char = response;

            /* Start the response with text for PSKEYGET */
            next_char += sprintf(next_char, PSKEYGET_RESPONSE PSKEYGET_RESPONSE_VARIABLE_FMT,
                                 global_key, local_key);

            deviceTestService_CompletePskeyResponse(task, response, next_char, 
                                                    retrieved_key, key_length_words);

            return;
        }
    }

    DeviceTestService_CommandResponseError(task);
}

void DeviceTestServiceCommand_HandleMibPskeyGet(Task task,
                            const struct DeviceTestServiceCommand_HandleMibPskeyGet *get)
{
    uint16 key = get->pskey;
    uint16 key_length_words;
    uint16 retrieved_key[PSKEY_MAX_LENGTH_SUPPORTED];
    char   response[PSKEY_BUFFER_BYTES];

    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleMibPskeyGet. key:%d", key);

    if (   !DeviceTestService_CommandsAllowed())
    {
        DeviceTestService_CommandResponseError(task);
        return;
    }

    key_length_words = PsFullRetrieve(key, NULL, 0);
    if (key_length_words && key_length_words <= PSKEY_MAX_LENGTH_SUPPORTED)
    {
        if (PsFullRetrieve(key, retrieved_key, key_length_words) == key_length_words)
        {
            char *next_char = response;

            /* Start the response with the text for PSKEYMIBGET */
            next_char += sprintf(next_char, PSKEYMIBGET_RESPONSE PSKEYMIBGET_RESPONSE_VARIABLE_FMT,
                                 key);

            deviceTestService_CompletePskeyResponse(task, response, next_char, 
                                                    retrieved_key, key_length_words);
            return;
        }
    }

    DeviceTestService_CommandResponseError(task);
}



/*! Command handler for AT + PSKEYCLEAR = pskey

    This function clears the specified key to the requested value.

    Errors are reported if the requested key is not supported.

    \param[in] task The task to be used in command responses
    \param[in] clear The parameters from the received command
 */
void DeviceTestServiceCommand_HandlePskeyClear(Task task,
                            const struct DeviceTestServiceCommand_HandlePskeyClear *clear)
{
    unsigned key = clear->pskey;
    uint16 local_key = deviceTestServiceCommand_GetInternalId(key);

    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandlePskeyClear. pskey:%d (local:%d)", key, local_key);

    if (!DeviceTestService_CommandsAllowed())
    {
        DeviceTestService_CommandResponseError(task);
        return;
    }

    /* No error to detect if clearing a key */
    PsStore(local_key, NULL, 0);
    DeviceTestService_CommandResponseOk(task);
}


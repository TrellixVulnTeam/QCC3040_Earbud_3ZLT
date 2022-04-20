/****************************************************************************
 * Copyright (c) 2018 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 * \file  direct_access_test.c
 * \ingroup  capabilities
 *
 *  Basic direct access test capability
 *
 */
/****************************************************************************
Include Files
*/
#include "direct_access_test_private.h"
#include "capabilities.h"

/****************************************************************************
Private Function Definitions
*/
/* Message handlers */
static bool direct_access_test_create(OPERATOR_DATA *op_data, void *message_data, unsigned *response_id, void **response_data);

/* Op msg handlers */
static bool direct_access_test_opmsg_file_open(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data);
static bool direct_access_test_opmsg_file_read(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data);
static bool direct_access_test_opmsg_file_close(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data);

/* Data processing function */
static void direct_access_test_process_data(OPERATOR_DATA *, TOUCHED_TERMINALS*);
/*****************************************************************************
Private Constant Declarations
*/
/** The download self-test capability function handler table */
const handler_lookup_struct direct_access_test_handler_table =
{
    direct_access_test_create,  /* OPCMD_CREATE */
    base_op_destroy,            /* OPCMD_DESTROY */
    base_op_start,              /* OPCMD_START */
    base_op_stop,               /* OPCMD_STOP */
    NULL,                       /* OPCMD_RESET */
    NULL,                       /* OPCMD_CONNECT */
    NULL,                       /* OPCMD_DISCONNECT */
    NULL,                       /* OPCMD_BUFFER_DETAILS */
    NULL,                       /* OPCMD_DATA_FORMAT */
    NULL                        /* OPCMD_GET_SCHED_INFO */
};

/* Null terminated operator message handler table */
const opmsg_handler_lookup_table_entry direct_access_test_opmsg_handler_table[] =
    {{OPMSG_DOWNLOAD_DIRECT_ACCESS_TEST_ID_FILE_OPEN,     direct_access_test_opmsg_file_open},
    {OPMSG_DOWNLOAD_DIRECT_ACCESS_TEST_ID_FILE_READ,     direct_access_test_opmsg_file_read},
    {OPMSG_DOWNLOAD_DIRECT_ACCESS_TEST_ID_FILE_CLOSE,     direct_access_test_opmsg_file_close},
    {0, NULL}};

const CAPABILITY_DATA direct_access_test_cap_data =
{
    CAP_ID_DOWNLOAD_DIRECT_ACCESS_TEST,    /* Capability ID */
    0, 1,                                  /* Version information - hi and lo parts */
    1, 1,                                  /* Max number of sinks/inputs and sources/outputs */
    &direct_access_test_handler_table,     /* Pointer to message handler function table */
    direct_access_test_opmsg_handler_table,/* Pointer to operator message handler function table */
    direct_access_test_process_data,       /* Pointer to data processing function */
    0,                                     /* Reserved */
    sizeof(DIRECT_ACCESS_TEST_OP_DATA)     /* Size of capability-specific per-instance data */
};
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_DIRECT_ACCESS_TEST, DIRECT_ACCESS_TEST_OP_DATA)


/************************ Private Function Definitions ********************************** */

static inline DIRECT_ACCESS_TEST_OP_DATA *get_instance_data(OPERATOR_DATA *op_data)
{
    return (DIRECT_ACCESS_TEST_OP_DATA *) base_op_get_instance_data(op_data);
}

/* ********************************** API functions ************************************* */

static bool direct_access_test_create(OPERATOR_DATA *op_data, void *message_data, unsigned *response_id, void **response_data)
{

    /* call base_op create, which also allocates and fills response message */
    if (!base_op_create(op_data, message_data, response_id, response_data))
    {
        return FALSE;
    }

    /* A few entries to debug logging*/
    L2_DBG_MSG ( "direct_access_test operator created");
    L4_DBG_MSG ( "Direct access is used for accessing files in the");
    L4_DBG_MSG ( "Apps Flash via the Transaction BUS in read-only mode.");
    L4_DBG_MSG ( "The capability has an empty process_data function ");
    L4_DBG_MSG ( "and the file actions are triggered by operator messages.");
    L4_DBG_MSG1( "The operator can have a maximum of %d files open at any time.\n", MAX_FILES);

    return TRUE;
}


/* *************** Data processing-related functions and wrappers *********************** */

static void direct_access_test_process_data(OPERATOR_DATA *op_data, TOUCHED_TERMINALS *touched)
{
    /* Does nothing, it's not expected to be called as we don't run as a normal operator */
    return;
}


/* **************************** Operator message handlers ******************************** */

static bool direct_access_test_opmsg_file_open(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    unsigned fn_length = OPMSG_FIELD_GET(message_data, OPMSG_MSG_DOWNLOAD_DIRECT_ACCESS_TEST_FILE_OPEN, FILENAME_LENGTH);
    const char *fn_ptr = (const char *)OPMSG_FIELD_POINTER_GET_FROM_OFFSET(message_data, OPMSG_MSG_DOWNLOAD_DIRECT_ACCESS_TEST_FILE_OPEN, FILENAME, 0);
    DIRECT_ACCESS_TEST_OP_DATA *op_extra_data = get_instance_data(op_data);
    const char *temp_source = fn_ptr;
    char *filename;
    char *temp_dest;
    unsigned to_copy;
    unsigned index = direct_access_test_find_handle(op_extra_data, NULL);

    if (index == INVALID_INDEX)
    {
        return FALSE;
    }

    filename = xpnewn(fn_length, char);
    if (filename == NULL)
    {
        return FALSE;
    }

    temp_dest = filename;
    op_extra_data->records[index].valid_handle = FALSE;

    /* The incoming message contains two octets per word at the
     * lower addresses; we use these to form a string - the filename.
     */
    while(fn_length > 0)
    {
        to_copy = 0;
        if (fn_length >= 2)
        {
            to_copy = 2;
        }
        else if(fn_length == 1)
        {
            to_copy = 1;
        }
        memcpy(temp_dest, temp_source, to_copy);
        temp_source += 2*to_copy;
        temp_dest += to_copy;
        fn_length -= to_copy;
    }

    /* It is important to provide a callback to know whether the file has opened
     * successfuly or not (which is indicated by the second argument passed into the callback).
     * In this example, the callback only sets a handle validity flag which will later
     * be used when we attempt to read data from the file.
     * If apps_file_open() fails, for more details, look in the debuglog
     * for faults and entries beginning with 'direct_access: apps_file_open()'.
     */
    apps_file_open(filename, &op_extra_data->records[index].handle, simple_cb, &op_extra_data->records[index]);

    /* filename is no longer needed */
    pfree(filename);

    /* allocate space for response - length = 2 words for the file handle plus the echoed msgID */
    *resp_length = OPMSG_RSP_PAYLOAD_SIZE_RAW_DATA(2);
    *resp_data = (OP_OPMSG_RSP_PAYLOAD *)xpnewn(*resp_length, unsigned);
    if (*resp_data == NULL)
    {
        apps_file_close(op_extra_data->records[index].handle);
        return FALSE;
    }

    /* echo the opmsgID - 3rd field in the message_data */
    (*resp_data)->msg_id = OPMGR_GET_OPCMD_MESSAGE_MSG_ID((OPMSG_HEADER*)message_data);

    /* return the file handle - unsigned split in low uint16 followed by high uint16 */
    (*resp_data)->u.raw_data[0] = ((unsigned)op_extra_data->records[index].handle) & 0xFFFF;
    (*resp_data)->u.raw_data[1] = ((unsigned)op_extra_data->records[index].handle) >> 16;

    return TRUE;
}

static bool direct_access_test_opmsg_file_read(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    FILE_ACCESS_RECORD *handle = (FILE_ACCESS_RECORD *)OPMSG_FIELD_GET32(message_data, OPMSG_MSG_DOWNLOAD_DIRECT_ACCESS_TEST_FILE_READ, FILE_HANDLE);
    DIRECT_ACCESS_TEST_OP_DATA *op_extra_data = get_instance_data(op_data);
    unsigned amount = OPMSG_FIELD_GET32(message_data, OPMSG_MSG_DOWNLOAD_DIRECT_ACCESS_TEST_FILE_READ, AMOUNT);
    unsigned offset = OPMSG_FIELD_GET32(message_data, OPMSG_MSG_DOWNLOAD_DIRECT_ACCESS_TEST_FILE_READ, OFFSET);
    unsigned index = direct_access_test_find_handle(op_extra_data, (unsigned *)handle);
    FILE_READ_RESULTS status;

    unsigned resp_payload_words = (amount + 1)/2;
    unsigned temp_index = 0;
    unsigned resp_data_index = 0;
    unsigned val_to_write;

    if (amount == 0 || index == INVALID_INDEX)
    {
        return FALSE;
    }

    char *temp_dest = xpnewn(amount, char);

    if(temp_dest != NULL)
    {
        status = apps_file_read(handle, offset, amount, temp_dest);

        if (status == FLASH_READ_SUCCESS)
        {

            /* The data read from the file has no significance for this
             * capability - we send it to the operator creator to do
             * anything they want with it: ignore or validate (as they
             * should have access to the file we are reading from the Apps).
             *
             * Allocate space for the response - the length is resp_payload_words
             * words plus 1 word for the echoed msgID.
             */
            *resp_length = OPMSG_RSP_PAYLOAD_SIZE_RAW_DATA(resp_payload_words);
            *resp_data = (OP_OPMSG_RSP_PAYLOAD *)xpnewn(*resp_length, unsigned);
            if (*resp_data == NULL)
            {
                pfree(temp_dest);
                return FALSE;
            }

            /* echo the opmsgID - 3rd field in the message_data */
            (*resp_data)->msg_id = OPMGR_GET_OPCMD_MESSAGE_MSG_ID((OPMSG_HEADER*)message_data);

            /* copy the even part of amount */
            while (amount >= 2)
            {
                val_to_write = (unsigned)temp_dest[temp_index] & 0xFF;
                val_to_write += ((unsigned)temp_dest[temp_index+1] & 0xFF) <<8;
                (*resp_data)->u.raw_data[resp_data_index] = val_to_write;
                resp_data_index++;
                temp_index += 2;
                amount -= 2;
            }
            /* if amount is odd - copy the last octet */
            if (amount > 0)
            {
                (*resp_data)->u.raw_data[resp_data_index] = ((unsigned)temp_dest[temp_index]) & 0xFF;
            }

            pfree(temp_dest);
            return TRUE;
        }
        pfree(temp_dest);
    }
    return FALSE;
}

static bool direct_access_test_opmsg_file_close(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    DIRECT_ACCESS_TEST_OP_DATA *op_extra_data = get_instance_data(op_data);
    FILE_ACCESS_RECORD *handle = (FILE_ACCESS_RECORD *)OPMSG_FIELD_GET32(message_data, OPMSG_MSG_DOWNLOAD_DIRECT_ACCESS_TEST_FILE_READ, FILE_HANDLE);
    unsigned index = direct_access_test_find_handle(op_extra_data, (unsigned *)handle);

    if (index != INVALID_INDEX)
    {
        if(apps_file_close(handle))
        {
            /* apps_file_close frees the memory pointed to by the handle,
             * we need to clear the local stale file records */
            op_extra_data->records[index].handle = NULL;
            op_extra_data->records[index].valid_handle = FALSE;
            return TRUE;
        }
    }
    return FALSE;
}

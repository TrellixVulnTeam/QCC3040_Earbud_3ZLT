/****************************************************************************
 * Copyright (c) 2018 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 * \defgroup direct_access_test
 * \file  direct_access_test_private.h
 * \ingroup capabilities
 *
 * Download direct access operator private header file. <br>
 *
 */

#ifndef _DOWNLOAD_DIRECT_ACCESS_TEST_PRIVATE_H_
#define _DOWNLOAD_DIRECT_ACCESS_TEST_PRIVATE_H_
/****************************************************************************
Private Constant Definitions
*/
#define MAX_FILES               20
#define INVALID_INDEX         0xFF

/*****************************************************************************
Include Files
*/
#include <string.h>
#include "capabilities.h"
#include "direct_access/direct_access.h"
#include "pmalloc/pl_malloc.h"

/****************************************************************************
Public Type Declarations
*/

typedef struct
{
    /** File access record */
    FILE_ACCESS_RECORD *handle;

    bool valid_handle;

} FILE_RECORD_WRAPPER;


/* capability-specific extra operator data */
typedef struct
{
    /** The buffer at the input terminal */
    tCbuffer *ip_buffer;

    /** The buffer at the output terminal */
    tCbuffer *op_buffer;

    /** The audio data format configurations of the input terminal */
    AUDIO_DATA_FORMAT ip_format;

    /** The audio data format configurations of the output terminal */
    AUDIO_DATA_FORMAT op_format;

    /** File access record array */
    FILE_RECORD_WRAPPER records[MAX_FILES];

} DIRECT_ACCESS_TEST_OP_DATA;

/*****************************************************************************
Private Function Definitions
*/
/**
 * \brief Find a file handle in the operator data file records
 *        (if the handle passed in is NULL, it returns the index
 *         of the first free record)
 *
 * \param op_extra_data operator extra data
 * \return record's index or INVALID_INDEX if not found
 */
unsigned direct_access_test_find_handle(DIRECT_ACCESS_TEST_OP_DATA *op_extra_data, unsigned *handle);

/*
 * \brief A simple example of user callback
 *
 * \param cb_private_data callback's private data
 * \param file_open_result the result of apps_file_open()
 *        TRUE - success, FALSE - failure
 */
void simple_cb(void * cb_private_data, bool file_open_result);

#endif /* _DOWNLOAD_DIRECT_ACCESS_TEST_PRIVATE_H_ */

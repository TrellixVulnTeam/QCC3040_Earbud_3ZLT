/****************************************************************************
 * Copyright (c) 2018 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 * \file  direct_access_test_util.c
 * \ingroup  capabilities
 *
 *  Direct access test capability (utilities)
 *
 */
/****************************************************************************
Include Files
*/
#include "direct_access_test_private.h"

/*****************************************************************************
Private Function Declarations
*/

/**
 * \brief Find a file handle in the operator data file records
 *        (if the handle passed in is NULL, it returns the index
 *         of the first free record)
 *
 * \param op_extra_data operator extra data
 * \param handle the file handle to find
 * \return record's index or INVALID_INDEX if not found
 */
unsigned direct_access_test_find_handle(DIRECT_ACCESS_TEST_OP_DATA *op_extra_data, unsigned *handle)
{
    FILE_RECORD_WRAPPER *record_array = (FILE_RECORD_WRAPPER *)op_extra_data->records;
    unsigned i = 0;

    while(i < MAX_FILES)
    {
        if ((unsigned *)record_array[i].handle == handle)
        {
            return i;
        }
        i++;
    }
    return INVALID_INDEX;
}

/*
 * \brief A simple example of user callback
 *
 * \param cb_private_data callback's private data
 * \param file_open_result the result of apps_file_open()
 *        TRUE - success, FALSE - failure
 */
void simple_cb(void * cb_private_data, bool file_open_result)
{
    FILE_RECORD_WRAPPER * record = (FILE_RECORD_WRAPPER *)cb_private_data;

    if (record!= NULL)
    {
        record->valid_handle = file_open_result;
        if (!file_open_result)
        {
			/* For an unsuccessful file_open(), the handle would have been
			 * freed up by the direct_access component - here we make sure
			 * it is also cleared from our records.
			 */
			record->handle = NULL;
		}
    }
}


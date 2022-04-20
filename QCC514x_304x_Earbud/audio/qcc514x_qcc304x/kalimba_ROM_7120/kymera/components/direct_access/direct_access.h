/****************************************************************************
 * Copyright (c) 2018 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 * \defgroup Direct Access
 * \ingroup direct_access
 *
 * Direct Access API for capabilities.
 *
 */
#ifndef DIRECT_FLASH_ACCESS_H
#define DIRECT_FLASH_ACCESS_H

#ifdef INSTALL_DIRECT_FLASH_ACCESS

#include "types.h"

typedef struct FILE_ACCESS_RECORD FILE_ACCESS_RECORD;

typedef enum
{
    FLASH_READ_SUCCESS = 0,
    FLASH_READ_INVALID_HANDLE,
    FLASH_READ_ACCESS_BEYOND_EOF,
    FLASH_READ_NULL_DESTINATION,
    FLASH_READ_INVALID_CORE

} FILE_READ_RESULTS;

/**  User Callback type
 * \param private_data User's private data for the callback
 * \param file_open_result Flag passed in by the caller indicating:
 *    - successful file_open() - TRUE
 *    - failed file_open() - FALSE
 */
typedef void (*FILE_OPEN_CALLBACK)(void * private_data, bool file_open_result);

/**
 * \brief Open a file in read-only mode.
 *    This initially returns an invalid file handle which will be updated when
 *    Audio SubSystem receives a response from the Application SubSystem.
 *
 *    The user:
 *    - should provide a location for the allocated file handle to be stored
 *    - can specify a callback function and private data to be
 *    passed into the callback but their presence is not mandatory.
 *
 * \param fn Pointer to the file name (must be a null-terminated string)
 * \param f_handle Pointer to a file handle (to be updated when Apps responds)
 * \param user_callback callback function to be supplied by the user
 * \param user_private_data private data for the user callback
 */
void apps_file_open(const char * fn, FILE_ACCESS_RECORD ** f_handle, FILE_OPEN_CALLBACK user_callback, void * user_private_data);

/**
 * \brief Close a file.
 *
 * \param f_handle File handle
 * \return TRUE, for success, FALSE otherwise
 */
bool apps_file_close(FILE_ACCESS_RECORD * f_handle);

/**
 * \brief Read data from a file located in the Apps Flash.
 *
 * \param f_handle File handle
 * \param f_offset Offset within the file [octets]
 * \param amount Amout of data to read [octets]
 * \param dest Destination buffer (to achieve a high read speed it is recommended to be aligned on a 4-octet boundary)
 * \return Status SUCCESS or failure codes
 */
FILE_READ_RESULTS apps_file_read(FILE_ACCESS_RECORD * f_handle, unsigned offset, unsigned amount, void *dest);

#endif /* INSTALL_DIRECT_FLASH_ACCESS */

#endif /* DIRECT_FLASH_ACCESS_H */

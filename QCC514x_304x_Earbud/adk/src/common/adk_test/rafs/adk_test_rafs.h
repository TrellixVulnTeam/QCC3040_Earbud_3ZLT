/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       adk_test_rafs.h
\ingroup    adk_test_common
\brief      Interface for specifc random access file system testing functions
*/

/*! @{ */
#ifndef ADK_TEST_RAFS_H
#define ADK_TEST_RAFS_H

/* RAFS TESTS */
/* ---------- */
/*!
 * \brief appTestRafsFormat
 * Format the RAFS
 */
void appTestRafsFormat(void);

/*!
 * \brief appTestRafsMount
 * Mount the RAFS
 */
void appTestRafsMount(void);

/*!
 * \brief appTestRafsUnmount
 * Unmount the RAFS
 */
void appTestRafsUnmount(void);

/*!
 * \brief appTestRafsCompact
 * Compact the RAFS
 */
void appTestRafsCompact(void);

/*!
 * \brief appTestRafsOpenRead
 * Generate a filename with the format "%c_file", and open it for reading.
 * \param id
 */
void appTestRafsOpenRead(char id);

/*!
 * \brief appTestRafsOpenWrite
 * Generate a filename with the format "%c_file", and open it for writing.
 * \param id
 */
void appTestRafsOpenWrite(char id);

/*!
 * \brief appTestRafsClose
 * Close a previously opened file.
 */
void appTestRafsClose(void);

/*!
 * \brief appTestRafsRead
 * Read len bytes of data from the file into buffer.
 * \param buffer
 * \param len
 * \return the number of bytes read.
 */
size_t appTestRafsRead(char *buffer, size_t len);

/*!
 * \brief appTestRafsWrite
 * Write len bytes of data from buffer to the file.
 * \param buffer
 * \param len
 * \return the number of bytes written.
 */
size_t appTestRafsWrite(const char *buffer, size_t len);

/*!
 * \brief appTestRafsRemove
 * Generate a filename with the format "%c_file", and delete the file.
 * \param id
 */
void appTestRafsRemove(char id);

/*!
 * \brief appTestRafsOpenDir
 * Open directory for reading.
 * \return the number of directory entries
 */
uint16 appTestRafsOpenDir(void);

/*!
 * \brief appTestRafsReadDir
 * Dump the directory listing to the live_log()
 */
void appTestRafsReadDir(void);

/*!
 * \brief appTestRafsCloseDir
 * Close the directory
 */
void appTestRafsCloseDir(void);

/*!
 * \brief appTestRafsStatFs
 * Dump the file system status to the live_log()
 * \param buffer    Optional pointer to a rafs_statfs_t structure
 */
void appTestRafsStatFs(void *buffer);

/*!
 * \brief appTestRafsMakeLargeFile
 * Create a large file
 * \param id        The id in %c_file
 * \param nsectors  Number of 4KB sectors to fill
 * \param nrows     Number of rows (16 bytes) to append for a partial sector
 */
void appTestRafsMakeLargeFile(char id, int nsectors, int nrows);

/*!
 * \brief appTestRafsCopyFile
 * Copy a file into another file
 * \param fromId    The id in %c_file to copy from
 * \param toId      The id in %c_file to create
 * \param buffsize  The temporary buffer for copying with
 */
void appTestRafsCopyFile(char fromId, char toId, uint32 buffsize);

/*!
 * \brief appTestRafsIoControl
 * Test the IO control interface, for hotword support.
 * \param id        The id in %c_file
 */
void appTestRafsIoControl(char id);

#endif /* ADK_TEST_RAFS_H */

/*! @} */


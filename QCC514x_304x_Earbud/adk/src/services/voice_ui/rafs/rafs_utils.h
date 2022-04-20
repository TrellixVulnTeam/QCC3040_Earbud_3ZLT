/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       rafs_utils.h
\brief      Various internal support functions for rafs

*/

#ifndef RAFS_UTILS_H
#define RAFS_UTILS_H

#include <ra_partition_api.h>

/*!
 * \brief A thin wrapper for RaPartitionWrite
 * \param handle The handle of the partition
 * \param offset Offset (absolute) within RA partition.
 * \param length Number of bytes to write onto RA partition.
 * \param buffer Caller provided buffer (i.e. array of bytes) to write onto RA partition.
 * \return Result of write operation on RA partition.
 */
raPartition_result Rafs_PartWrite(raPartition_handle *handle, uint32 offset, uint32 length, const void * buffer);

/*!
 * \brief A thin wrapper for RaPartitionRead
 * \param handle The handle of the partition
 * \param offset Offset (absolute) within RA partition.
 * \param length Number of bytes to read from RA partition.
 * \param buffer Caller provided buffer (i.e. array of bytes) to read from RA partition.
 * \return Result of read operation on RA partition.
 */
raPartition_result Rafs_PartRead(raPartition_handle *handle, uint32 offset, uint32 length, void * buffer);

/*!
 * \brief Copy a string in at most num characters
 * \param dest  The destination of the copy
 * \param src   The source of the copy
 * \param num   The max characters to copy, excluding the \0
 * \return TRUE if the string fits, FALSE if not (it was truncated)
 */
bool safe_strncpy(char *dest, const char *src, rafs_size_t num);

/*!
 * \brief Copy two strings in at most num characters
 * \param s1    The first string
 * \param s2    The second string
 * \param num   The max number of chars to compare
 * \return <0, ==0, > 0
 */
int safe_strncmp(const char *s1, const char *s2, rafs_size_t num);

/*!
 * \brief Get the length of the string in at most num characters
 * \param src   The string to examine
 * \param num   The max number of chars to examine
 * \return The length, if that length is <num, otherwise -1
 */
int safe_strnlen(const char *src, rafs_size_t num);

/*!
 * \brief This function checks whether a block of memory consists
 *        entirely of bytes indicating that flash memory is in an erased state.
 * \param ptr   Pointer to some data
 * \param len   The number of bytes to check
 * \return TRUE if all the bytes are 0xFF
 */
bool Rafs_IsErasedData(const void *ptr, rafs_size_t len);

/*!
 * \brief Validate individual filename characters.
 * \param ch        A character to test
 * \return TRUE if the character is allowed in filenames.
 */
bool Rafs_IsValidFilenameCharacter(char ch);

/*!
 * \brief Validate a filename
 * \param filename  The filename to validate
 * \return RAFS_OK or a filename related error code.
 */
rafs_errors_t Rafs_IsValidFilename(const char *filename);

/*!
 * \brief Rafs_GetPartitionName
 * Extract the partition name from a path.
 *
 * \param path  The absolute pathname to be examined.
 * \param buff  The buffer where to store the n'th component.
 * \param len   The length of that buffer.
 * \return RAFS_OK if a field was extracted OK, or an error indicating a problem with the path.
 */
rafs_errors_t Rafs_GetPartitionName(const char *path, char *buff, rafs_size_t len);

/*!
 * \brief Rafs_GetFilename
 * Extract the filename name from a path.
 *
 * \param path  The absolute pathname to be examined.
 * \param buff  The buffer where to store the n'th component.
 * \param len   The length of that buffer.
 * \return RAFS_OK if a field was extracted OK, or an error indicating a problem with the path.
 */
rafs_errors_t Rafs_GetFilename(const char *path, char *buff, rafs_size_t len);


/*!
 * \brief Is RAFS ready and able to process a request?
 * \return RAFS_FAULT if uninitialised, RAFS_BUSY if busy or RAFS_OK if OK.
 */
rafs_errors_t Rafs_IsReady(void);

/*!
 * \brief Open an RA partition.
 * \param partition_name    The name of the partition.
 * \return OK or an error
 */
rafs_errors_t Rafs_PartitionOpen(const char *partition_name);

/*!
 * \brief Close a previously opened partition.
 */
void Rafs_PartitionClose(void);

/*!
 * \brief Allocate the temporary working dirent used when scanning the FAT.
 */
void Rafs_AllocateScanDirent(void);

/*!
 * \brief Free the temporary working dirent used when scanning the FAT.
 */
void Rafs_FreeScanDirent(void);

#endif /* RAFS_UTILS_H */

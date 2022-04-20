/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       rafs_directory.h
\brief      Implement directory level accesses to rafs.

*/

#ifndef RAFS_DIRECTORY_H
#define RAFS_DIRECTORY_H

#include "rafs_dirent.h"

/*!
  * To support the error recovery, the first byte of the dirent has
  * some special significance.  In order to make writing a dirent
  * seem as atomic as possible, we write the bulk of the data first,
  * then write the first byte last.
  */
typedef enum {
    DIRENT_WRITE_TYPE_ALL,    /*!< The whole dirent */
    DIRENT_WRITE_TYPE_FIRST,  /*!< Just the first byte */
    DIRENT_WRITE_TYPE_DATA,   /*!< Everything except the first byte */
} dirent_write_type_t;

/*!
 * \brief Read one or more contiguous members of a dir_entry_t
 * \param id            Which FAT to read from
 * \param data          Where to store the read data
 * \param entryNumber   The entry in the FAT to read.
 * \param member_offset The member offset from the start of dir_entry_t
 * \param member_size   The size of the member
 * \return
 */
bool Rafs_DirEntryReadMember(block_id_t id, void *data, uint32 entryNumber,
                             uint32 member_offset, uint32 member_size);

/*!
 * \brief Write one or more contiguous members of a dir_entry_t
 * \param id            Which FAT to write to
 * \param data          The data to be written
 * \param entryNumber   The entry in the FAT to write.
 * \param member_offset The member offset from the start of dir_entry_t
 * \param member_size   The size of the member
 * \return
 */
bool Rafs_DirEntryWriteMember(block_id_t id, const void *data, uint32 entryNumber,
                              uint32 member_offset, uint32 member_size);

/*!
 * \brief Read a dir_entry from one of the FATs
 * \param id            Which FAT to read from
 * \param dir_entry     Where to store the read data
 * \param entryNumber   The entry in the FAT to read.
 * \return TRUE if all is well, FALSE if there was a problem
 */
bool Rafs_DirEntryRead(block_id_t id, dir_entry_t *dir_entry, uint32 entryNumber);

/*!
 * \brief Write a dir_entry to one of the FATs
 * \param id            Which FAT to write to
 * \param dir_entry     The dir_entry to be written
 * \param entryNumber   The entry in the FAT to write.
 * \return TRUE if all is well, FALSE if there was a problem
 */
bool Rafs_DirEntryWrite(block_id_t id, const dir_entry_t *dir_entry, uint32 entryNumber);

/*!
 * \brief Write part of a dirent to one of the FATs
 * \param id            Which FAT to write to
 * \param dir_entry     The dir_entry to be written
 * \param entryNumber   The entry in the FAT to write.
 * \param type          Which part of the dirent to write.
 * \return TRUE if all is well, FALSE if there was a problem
 */
bool Rafs_DirEntryPartWrite(block_id_t id, const dir_entry_t *dir_entry, uint32 entryNumber, dirent_write_type_t type);

/*!
 * \brief Get the opened file structure associated with an id
 * \param file_id       The id
 * \return A pointer, if file_id is an opened file, or NULL otherwise
 */
open_file_t *Rafs_FindOpenFileInstanceById(rafs_file_t file_id);

/*!
 * \brief Get the opened file structure associated with filename
 * \param filename      The name of the file
 * \return A pointer, if filename is an opened file, or NULL otherwise
 */
open_file_t *Rafs_FindOpenFileInstanceByName(const char *filename);

/*!
 * \brief Find the index of a free slot in the open file table
 * \return The index of a free slot, or RAFS_INVALID_FD if there is no more space to open a file.
 */
rafs_file_t Rafs_FindFreeFileSlot(void);

/*!
 * \brief Search for a directory entry, given a filename
 * \param filename      The file to search for
 * \param dir_slot      The index position of the found file
 * \return TRUE if the file is found (see *dir_slot), or FALSE if not found.
 */
bool Rafs_FindDirEntryByFilename(const char *filename, uint32 *dir_slot);

/*!
 * \brief Construct a new dir_entry for a new file
 * \param result        Where to write new directory entry details to
 * \param filename      The file name
 * \param first_inode   The initial inode to be written to
 */
void Rafs_MakeNewDirEntryForFilename(dir_entry_t *result, const char *filename, inode_t *first_inode);

#endif /* RAFS_DIRECTORY_H */

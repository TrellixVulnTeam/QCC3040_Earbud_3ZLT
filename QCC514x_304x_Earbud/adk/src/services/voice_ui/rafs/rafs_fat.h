/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       rafs_fat.h
\brief      Implement functions which operate on the FAT table of rafs.

A sector map of all the pages in the partition is maintained.  This is
basically a bitmap where a '1' indicates the page is free and a '0'
indicates the page is in use.

*/

#ifndef RAFS_FAT_H
#define RAFS_FAT_H

#include "rafs_dirent.h"

/*! \brief the FAT index entry always occupies the first slot in the FAT */
#define ROOT_DIRENT_NUMBER  0

/*!
 * \brief rafs_isValidInode
 * Determine if the current inode points to real data, or
 * is either unused or deleted.
 * \param inode
 * \return TRUE if neither unused or deleted
 */
bool rafs_isValidInode(const inode_t *inode);

/*!
 * \brief determine if this dir_entry is for a deleted file.
 * This is done by examining the first character of the filename.
 * \param dir_entry    The directory entry to test
 * \return TRUE if this is a deleted entry, FALSE otherwise.
 */
bool Rafs_IsDeletedEntry(const dir_entry_t *dir_entry);

/*!
 * \brief Mark a block as being free for use
 * \param block_number   The block
 */
void Rafs_SectorMapMarkFreeBlock(uint32 block_number);

/*!
 * \brief Mark a block as being in use
 * \param block_number   The block
 */
void Rafs_SectorMapMarkUsedBlock(uint32 block_number);

/*!
 * \brief Test whether a block is in use or not.
 * \param block_number   The block
 * \return TRUE if the block is in use, FALSE if it's free for use
 */
bool Rafs_SectorMapIsBlockInUse(uint32 block_number);

/*!
 * \brief Count the number of free sectors on the disk
 * \return the number of free sectors
 */
uint16 Rafs_CountFreeSectors(void);

/*!
 * \brief Initialise the sector map.
 */
void Rafs_InitialiseSectorMap(void);

/*!
 * \brief Updates the sector map will all the known files in the directory.
 */
void Rafs_ScanDirectory(void);

/*!
 * \brief Count the number of files stored in the directory.
 * \return the number of files stored in the directory
 */
uint16 Rafs_CountActiveFiles(void);

/*!
 * \brief Get a pointer to the initial root.
 * \param id        PRI_FAT or SEC_FAT
 * \return a pointer to the initialised FAT root element.
 */
const dir_entry_t *Rafs_GetInitialRoot(block_id_t id);

/*!
 * \brief Write out a root directory entry
 * \param id        PRI_FAT or SEC_FAT
 * \return TRUE if all is well, FALSE otherwise
 */
bool Rafs_WriteRootDirent(block_id_t id);


/*! \brief Open a directory to read information about each file.
    \param[in] path         the path to the directory (could be "")
    \param[out] dir_id      a pointer to where to store the opened directory id
    \param[out] num         a pointer to where to store the number of directory entries in the opened directory.
    \return RAFS_OK or an error code.
 */
rafs_errors_t Rafs_DoOpendir(const char *path, rafs_dir_t *dir_id, uint16 *num);

/*! \brief Read the next directory entry
    \param[in] dir_id       the directory id
    \param[out] stat        a pointer to a stat structure to receive information
    \return RAFS_OK or an error code.
 */
rafs_errors_t Rafs_DoReaddir(rafs_dir_t dir_id, rafs_stat_t *stat);

/*! \brief Close a directory
    \param[in] dir_id       the directory id to close
    \return RAFS_OK or an error code.
 */
rafs_errors_t Rafs_DoClosedir(rafs_dir_t dir_id);

/*!
 * \brief Remove a file from the file system.
 * \param filename  The filename to remove.
 * \return RAFS_OK on success, or an error code.
 */
rafs_errors_t Rafs_DoRemove(Task task, const char *filename);

#endif /* RAFS_FAT_H */

/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       rafs_extent.h
\brief      Implement functions which operate on extents in rafs.

An extent is any contiguous sequence of either in-use or free sectors.
A single extent can be addressed by a single inode.

*/

#ifndef RAFS_EXTENT_H
#define RAFS_EXTENT_H

/*!
 * \brief Mark all the blocks used by a file as in use
 * \param inodes    The inodes associated with the file.
 * \param num       The number of inodes
 * \return TRUE if all is well, FALSE if there was a problem
 */
bool Rafs_AddExtentsToSectorMap(const inode_t inodes[], uint32 num);

/*!
 * \brief Mark all the blocks previously used by a file as now free
 * \param inodes    The inodes associated with the file.
 * \param num       The number of inodes
 * \return TRUE if all is well, FALSE if there was a problem
 */
bool Rafs_RemoveExtentsFromSectorMap(const inode_t inodes[], uint32 num);

/*!
 * \brief Find the largest contiguous block of free blocks
 * \return An inode representing the start and length of a writable block.
 */
inode_t Rafs_FindLongestFreeExtent(void);

/*!
 * \brief Rafs_CountFreeExtents
 * This counts the number of free extents that currently exist.
 * \return the number of free extents
 */
uint16 Rafs_CountFreeExtents(void);

/*!
 * \brief Rafs_GetFreeExtents
 * This returns a list of free extents, sorted into decreasing size order.
 * \param inodes    A pointer to an array of inodes to be filled in.
 * \param num       The size of the above array.
 * \return the actual number of extents that were found.

 * \note the return result could be greater than num.  If this is the case,
 * then the returned list might not contain the largest actual free extent.
 * Use \ref Rafs_CountFreeExtents beforehand to prepare a suitable buffer.
 */
uint16 Rafs_GetFreeExtents(inode_t *inodes, uint16 num);

#endif /* RAFS_EXTENT_H */

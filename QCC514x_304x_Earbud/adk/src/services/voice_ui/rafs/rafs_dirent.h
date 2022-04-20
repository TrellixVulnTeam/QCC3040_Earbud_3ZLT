/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       rafs_dirent.h
\brief      Implement directory level accesses to rafs.

*/

#include "rafs.h"

#ifndef RAFS_DIRENT_H
#define RAFS_DIRENT_H

#define MAX_INODES          3       /*!< Number of extents */

#define INODE_UNUSED        ((uint16)(~0))  /*!< Stored in an unused inode */
#define INODE_DELETED       ((uint16)( 0))  /*!< Stored in a deleted inode */

#define INITIAL_ACCESS      ((uint16)(~0))  /*!< The initial value for file_age_t.accessed */

#define DIRECTORY_SIZE      4096    /*!< Space for directory, should be same as partition block size */

#define RAFS_SIZEOF(x)      ((uint32)(sizeof(x)))

#define MAX_DIR_ENTRIES     (DIRECTORY_SIZE/RAFS_SIZEOF(dir_entry_t))

/*!
 * \brief Record the position and length of a file extent in the file system
 *
 * A file consists of one or more extents
 *      16-bit extents good for 4K sectors up to max 256MB (inode size=4)
 *      8-bit extents good for 4K sectors up to max 1MB (inode size=2)
 * An inode = { 0xff,0xff } is unused (so far)
 *      An inode can have a length of 0xff, but it can't also start at 0xff.
 * An inode = { 0x00,0x00 } is to be ignored (it also means it's deleted)
 *      An inode can start at 0x00, but it can't also have a length of 0x00.
 * The minimum useful length is 0x01.
 */
typedef struct {
    uint16     offset;                  /*!< The sector number where this extent starts */
    uint16     length;                  /*!< The number of contiguous sectors in this extent */
    /* 4 bytes */
} inode_t;

/*!
 * \brief The representation of a directory entry as stored in persistent storage.
 * \note Be very careful about compiler packing, alignment and choice of types.
 */
typedef struct {
    char       filename[RAFS_MAX_FILELEN];  /*!< Always \0 terminated */
    uint32     file_size;               /*!< in bytes, up to 4GB */
    stat_counters_t stat_counters;      /*!< the relative ages of the file */
    inode_t    inodes[MAX_INODES];      /*!< The extents used to store the file content */
} dir_entry_t;

/* Static asserts for the size and offsets of information written to storage */
/* */
STATIC_ASSERT(sizeof(dir_entry_t)==36,dirent_t_should_be_36);
STATIC_ASSERT(offsetof(dir_entry_t,file_size)==16,bad_filesize_offset);
STATIC_ASSERT(offsetof(dir_entry_t,inodes)==24,bad_inodes_offset);

/*!
 * \brief Used to track the state of each opened file.
 */
typedef struct {
    dir_entry_t         file_dir_entry; /*!< The directory entry of the open file */
    Task                task;           /*!< The task to receive notifications associated with this open file */
    uint32              directory_index;/*!< The directory slot of the open file */
    uint32              file_position;  /*!< The next read/write position within the whole file */
    uint32              extent_position;/*!< The position within the current inode */
    uint16              current_inode;  /*!< The current inode being used */
    rafs_mode_t         flags;          /*!< The flags passed to the open() call */
} open_file_t;

#endif /* RAFS_DIRENT_H */

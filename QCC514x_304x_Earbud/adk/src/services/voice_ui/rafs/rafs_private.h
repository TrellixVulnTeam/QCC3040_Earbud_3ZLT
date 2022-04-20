/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       rafs_private.h
\brief      The globally accessible internals of rafs.

This file contains things which would be static, if everything were in a single
source file.

*/

#ifndef RAFS_PRIVATE_H
#define RAFS_PRIVATE_H

#include <csrtypes.h>

#include "rafs_dirent.h"
#include "rafs_power.h"

/*!
 * \brief Block Identification
 *
 * RAFS maintains two FATs to provide some minimal form of integrity
 * checking in the face of possible device failures.  The focus is
 * more on performance.
 */
typedef enum {
    PRI_FAT,            /*!< FAT 1 */
    SEC_FAT,            /*!< FAT 2 */
    FILE_DATA,          /*!< First block for file data */
} block_id_t;

/*!
  \brief This holds information about the mounted partition.
  */
typedef struct {
    raPartition_handle  part_handle;                /*!< The partition manager handle */
    raPartition_info    part_info;                  /*!< The partition information (sizes of) */
    char                partition_name[RAFS_MOUNTPOINT_LEN];    /*!< The name of the mounted partition */
} partinfo_t;

/*!
  \brief This holds information about opened files.
  */
typedef struct {
    open_file_t        *open_files[RAFS_MAX_OPEN_FILES];        /*!< Tracking the opened files */
    dir_entry_t        *scan_dir_entry;             /*!< Used to hold one dir_entry from the flash */
    uint32              free_dir_slot;              /*!< The first unused dir_entry slot */
    bool                file_is_open_for_writing;   /*!< Don't support more than one writer at once. */
    bool                compact_is_needed;          /*!< Whether compact will find anything useful to do. */
    uint16              num_open_files;             /*!< How many files are open at the moment */
    uint16              sequence_number;            /*!< The next value to use as the file's 'created' age */
} fileinfo_t;

/*!
  \brief This holds information about the opened directory.
  */
typedef struct {
    uint32              readdir_index;      /*!< Which directory index to read next */
    uint16              num_files_present;  /*!< Number of files present */
    uint16              num_files_read;     /*!< Number of files returned via ReadDirectory */
} dirinfo_t;

/*!
 * \brief The RAFS global instance data
 * This structure contains the global persistent data of the RAFS.
 */
typedef struct {
    TaskData            task;       /*!< Our internal task for processing background activities */
    bool                busy;       /*!< Currently processing a background task (init, format etc) */
    partinfo_t         *partition;  /*!< Data relating to the mounted partition */
    uint32             *sector_in_use_map;          /*!< Bitmap of used sectors */
    uint32              sector_in_use_map_length;   /*!< The number of data elements \see sector_in_use_map points to */
    fileinfo_t         *files;      /*!< Data relating to opened files */
    dirinfo_t          *directory;  /*!< Data for Rafs_ReadDirectory */
    rafs_power_t        power;      /*!< The charger and battery status */
    rafs_ioc_set_app_status_idle_cb_t
                        idler;      /*!< An application callback to determine if the system is in an idle state */
} rafs_instance_t;

/*!
 * \brief Get the task instance data for RAFS
 * \return The instance of RAFS.
 */
rafs_instance_t *Rafs_GetTaskData(void);

/*!
 * \brief Set the task instance data for RAFS
 * \param self  The new instance pointer.
 */
void Rafs_SetTaskData(rafs_instance_t *self);

#endif /* RAFS_PRIVATE_H */

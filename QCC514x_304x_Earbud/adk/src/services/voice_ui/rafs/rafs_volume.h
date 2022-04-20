/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       rafs_volume.h
\brief      Implement functions which operate on a partition volume.

*/

#ifndef RAFS_VOLUME_H
#define RAFS_VOLUME_H

/*! \brief Rafs_Mount is used to mount a file system for use
    \param[in] partition_name  the identifier for the partition
    \return RAFS_OK or an error code.
 */
rafs_errors_t Rafs_DoMount(const char *partition_name);

/*! \brief Rafs_Unmount is used to unmount a file system
    \param[in] partition_name  the identifier for the partition
    \return RAFS_OK or an error code.
 */
rafs_errors_t Rafs_DoUnmount(const char *partition_name);

/*! \brief  Obtain information about the file system.
        Total available space - what would be available immediately after a format.
        Total free space - all the blocks not presently in use by the FAT and files.
        Max file size - the largest file that could be presently created
    \param[in] partition_name  the identifier for the partition
    \param[out] buf         a pointer to a buffer where file system info will be written.
    \return RAFS_OK or an error code.
 */
rafs_errors_t Rafs_DoStatfs(const char *partition_name, rafs_statfs_t *buf);

/*!
 * \brief Rafs_GetMountStatus
 * Determine if there is a partition mounted, and if so, it has the given name.
 * \param partition_name
 * \return RAFS_OK if the given partition is currently mounted.
 */
rafs_errors_t Rafs_GetMountStatus(const char *partition_name);


#endif /* RAFS_VOLUME_H */

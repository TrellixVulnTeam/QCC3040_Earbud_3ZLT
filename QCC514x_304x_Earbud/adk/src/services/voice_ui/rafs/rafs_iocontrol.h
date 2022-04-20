/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       rafs_iocontrol.h
\brief      Implementation of IO control

*/

#ifndef RAFS_IOCONTROL_H
#define RAFS_IOCONTROL_H

#include <csrtypes.h>

#include <ra_partition_api.h>

#include "rafs.h"

/*!
 * \brief Rafs_DoIocGetPartitionHandle
 * This implements the IO control method to get the current partition manager handle.
 * \param partition_name    The name of the partition (as used to mount the partition)
 * \param handle            A pointer to where to store the handle
 * \param out_length        A pointer to where to store the handle size.
 * \return RAFS_OK or an error.
 */
rafs_errors_t Rafs_DoIocGetPartitionHandle(const char *partition_name, raPartition_handle *handle, rafs_size_t *out_length);

/*!
 * \brief Rafs_DoIocGetFileOffset
 * This implements to the IO control method to get the absolute offset of the start of
 * the file data within the partition.
 * \param pathname          The pathname to the file.
 * \param value             A pointer to a uint32 to contain the calculated offset.
 * \param out_length        A pointer to where to store sizeof(uint32)
 * \return RAFS_OK or an error.
 */
rafs_errors_t Rafs_DoIocGetFileOffset(const char *pathname, uint32 *value, rafs_size_t *out_length);

/*!
 * \brief Rafs_DoIocSetAppStatusIdleCb
 * This allows an application to specify a callback function that RAFS will call
 * periodically to determine if the application is in an idle state.
 * The idle state is used by RAFS to perform certain housekeeping activities.
 * \param data      A pointer to a \see rafs_ioc_set_app_status_idle_cb_t
 * \return RAFS_OK or an error.
 */
rafs_errors_t Rafs_DoIocSetAppStatusIdleCb(const rafs_ioc_set_app_status_idle_cb_t *data);

#endif /* RAFS_IOCONTROL_H */

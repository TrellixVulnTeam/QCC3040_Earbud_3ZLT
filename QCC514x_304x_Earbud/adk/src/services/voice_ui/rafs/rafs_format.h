/*!
\copyright  Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       rafs_format.h
\brief      Implement functions to format the RAFS file system.
*/

#ifndef RAFS_FORMAT_H
#define RAFS_FORMAT_H

/*!
 * \brief Format the RAFS by erasing all data and rewriting the two FAT indices.
 * \param partition_name    The name of the partition to be formatted.
 * \param type              The type of format to be performed.
 * \return RAFS_OK on success, RAFS_INVALIDFAT for trouble.
 */
rafs_errors_t Rafs_DoFormat(const char *partition_name, rafs_format_type_t type);

#endif /* RAFS_FORMAT_H */

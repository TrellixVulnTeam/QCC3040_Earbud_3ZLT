/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       rafs_integrity.h
\brief      Performs integrity checks on the filesystem.

*/

#ifndef RAFS_INTEGRITY_H
#define RAFS_INTEGRITY_H

/*!
 * \brief Rafs_CheckIntegrity
 * Performs a number of validation and repair steps on RAFS.
 * \return OK or an error - which should be regarded as fatal.
 */
rafs_errors_t Rafs_CheckIntegrity(void);

#endif /* RAFS_INTEGRITY_H */

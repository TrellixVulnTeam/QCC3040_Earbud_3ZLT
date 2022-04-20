/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       rafs_init.h
\brief      The initialisation and shutdown phases of rafs

*/

#ifndef RAFS_INIT_H
#define RAFS_INIT_H

/*! \brief  This performs the initialisation of the RAFS.
    \return RAFS_OK or an error code.
 */
rafs_errors_t Rafs_DoInit(void);

/*! \brief  This shuts down the RAFS
    \return RAFS_OK or an error code.
 */
rafs_errors_t Rafs_DoShutdown(void);

#endif /* RAFS_INIT_H */

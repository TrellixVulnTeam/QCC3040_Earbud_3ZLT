/****************************************************************************
 * Copyright (c) 2021 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 *
 * \file aanc_security_public.h
 * \ingroup lib_private\aanc
 *
 * Public AANC security library header file.
 *
 */
/***************************************************************************/

#ifndef _AANC_SECURITY_LIB_PUBLIC_H_
#define _AANC_SECURITY_LIB_PUBLIC_H_

#include "types.h"

/******************************************************************************
Public Function Definitions
*/

/**
* \brief Create and initialize the handle for AANC features
* \param  f_handle  Address of pointer to feature handle for AANC feature
* \return  boolean indicating success or failure
*/
bool load_aanc_handle(void** f_handle);

/**
* \brief Unload and remove the handle for AANC features
* \param  f_handle  Pointer to feature handle for AANC feature
*/
void unload_aanc_handle(void* f_handle);

#endif /* _AANC_SECURITY_LIB_PUBLIC_H_ */
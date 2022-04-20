/****************************************************************************
 * Copyright (c) 2020 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 * \defgroup svad
 * \ingroup lib_private
 * \file  svad_handle.h
 * \ingroup svad
 *
 * svad handle private header file. <br>
 *
 */
 
/******************** Private functions ***************************************/


#ifndef _SVAD_HANDLE_H_
#define _SVAD_HANDLE_H_

/**
@brief Initialize the feature handle for SVAD
*/
bool load_svad_handle(void** f_handle);

/**
@brief Remove and unload the feature handle for SVAD
*/
void unload_svad_handle(void* f_handle);

#endif /* _SVAD_HANDLE_H_ */


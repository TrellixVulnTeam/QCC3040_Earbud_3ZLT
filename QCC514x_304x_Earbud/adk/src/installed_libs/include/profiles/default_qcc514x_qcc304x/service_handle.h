/* Copyright (c) 2021 Qualcomm Technologies International, Ltd. */
/*  */

/*!
@file    service_handle.h
@brief   Header file for the Service Handle library.

        Service Handle is a support library that manages handles
        to Services instance data.
*/

#ifndef SERVICE_HANDLE_T_H_
#define SERVICE_HANDLE_T_H_

#include <csrtypes.h>
#include <message.h>

/*!
    \brief Unique handle reference for a Service instance.
*/
 
typedef uint16 ServiceHandle;

/*
    @brief Creates a service data instance.  Will allocate memory of the specified size
    and return a unique handle to this instance data.

    @param newInstance Pointer to the the newly created service data instance
    @param newSize     The size of the service data to be created

    @return ServiceHandle unique handle to the new service data instance.
    If the handle is zero this indicates an initialisation failure.

*/
ServiceHandle ServiceHandleNewInstance(void **newInstance, const uint16 newSize);

/*!
    @brief Finds the service instance data from the supplied handle

    @param handle to the required service data

    @return pointer to the service data associated with the handle, if NULL
    the handle is invalid.

*/
void* ServiceHandleGetInstanceData(const ServiceHandle service_handle);

/*!
    @brief Frees the service instance data memory associated with the supplied handle.
    Handle will be invalid after a call to this function.

    @param handle to the required service data

    @return bool TRUE if the memory associated with the handle successfully released

*/
bool ServiceHandleFreeInstanceData(const ServiceHandle service_handle);

#endif /* SERVICE_HANDLE_T_H_ */

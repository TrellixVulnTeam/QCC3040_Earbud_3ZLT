/* Copyright (c) 2021 Qualcomm Technologies International, Ltd. */
/*  */

/*!
@file    service_handle.c
@brief   Header file for the Service Handle library.

        Service Handle is a support library that manages handles
        to Services instance data.
*/

#include "service_handle.h"

#include <panic.h>
#include <stdlib.h>
#include <stdint.h>

/*!
    \brief struct representing an array of data pointers.
*/
typedef struct __service_handle_data
{
    uint16  size;
    void*   instances[1];
} service_handle_data_t;

/*!
    \brief index of service data pointers.
*/
static service_handle_data_t *service_handle_data = NULL;

/* The handle returned by this library is a combination of the index of the
 * data pointer in the service_handle_data array and part of the data
 * pointer itself.  As entries in the array index are reused this goes some
 * way to prevent a request using the previous use of the handle incorrectly
 * returning an invalid pointer.
 * The following macros are used to construct and deconstruct the handle */
#define MAX_SERVICE_HANDLES (0x0fff)
#define SERVICE_HANDLE_MASK MAX_SERVICE_HANDLES
#define CHECKSUM_MASK ((uint16)~SERVICE_HANDLE_MASK)
#define CALC_HANDLE_CHECKSUM(px) ((uint16)((uintptr_t)(*px) & CHECKSUM_MASK))
#define GET_HANDLE_CHECKSUM(h) (h & CHECKSUM_MASK)
#define GET_HANDLE_INDEX(h) (h & SERVICE_HANDLE_MASK)

ServiceHandle ServiceHandleNewInstance(void **newInstance, const uint16 newSize)
{
    uint16 listLen;
    uint16 newIndex = 0;
    ServiceHandle newHandle;

    /* Create the required data instance */
    *newInstance = malloc(newSize);
    if(*newInstance == NULL)
    {
        /* failed to allocate memory for this instance, return 0 error */
        return 0;
    }

    /* Check if the index is initalised */
    if(service_handle_data == NULL)
    {
        /* set the list length to 0, list will be allocated later */
        listLen = 0;
    }
    else
    {
        /* Get the list length */
        listLen = service_handle_data->size;

        /* Sanity check the number of handles */
        if (listLen>=MAX_SERVICE_HANDLES)
        {
            return 0;
        }

        /* Search for empty slots in the handles list and reuse if available */
        while(newIndex < listLen)
        {
            /* If this slot is NULL we can reuse this handle */
            if(service_handle_data->instances[newIndex] == NULL)
            {
                break;
            }
            newIndex++;
        }
    }

    /* Check if more memory required to store the list */
    if (newIndex >= listLen)
    {
        service_handle_data_t *new_service_handle_data = NULL;
        new_service_handle_data = realloc(service_handle_data,
                                          sizeof(service_handle_data_t) + (sizeof(void*) * listLen) );

        /* If realloc fails, the original index list is still valid
         * but no more can be added */
        if(new_service_handle_data == NULL)
        {
            free(*newInstance);
            *newInstance = NULL;
            return 0;
        }

        service_handle_data = new_service_handle_data;

        service_handle_data->size = listLen+1;
    }

    /* Add the service data handle to the list */
    service_handle_data->instances[newIndex] = *newInstance;

    /* Assemble handle to be returned */
    newHandle = CALC_HANDLE_CHECKSUM(newInstance) | (newIndex+1);

    return newHandle;
}

void* ServiceHandleGetInstanceData(const ServiceHandle service_handle)
{
    uint16 handleIndex;

    /* Check if the index is initalised */
    if(service_handle_data == NULL)
    {
        return NULL;
    }

    /* Calculate the index from the handle passed in */
    handleIndex = GET_HANDLE_INDEX(service_handle);

    /* Check if handle is in range or record at that location is NULL */
    if(handleIndex > service_handle_data->size ||
       service_handle_data->instances[handleIndex-1] == NULL)
    {
        return NULL;
    }

    /* check the checksum of the one in the index vs the one passed in */
    if(CALC_HANDLE_CHECKSUM(&(service_handle_data->instances[handleIndex-1]))
       != GET_HANDLE_CHECKSUM(service_handle))
    {
        return NULL;
    }

    return service_handle_data->instances[handleIndex-1];
}

bool ServiceHandleFreeInstanceData(const ServiceHandle service_handle)
{
    /* Find the data associated with the handle and check if it is valid */
    void *data = ServiceHandleGetInstanceData(service_handle);
    if (data == NULL)
    {
        return FALSE;
    }

    /* free instance data */
    free(data);
    data = NULL;
    service_handle_data->instances[GET_HANDLE_INDEX(service_handle)-1] = NULL;

    /* if this is the last handle we can realloc or free memory */
    if(GET_HANDLE_INDEX(service_handle) == service_handle_data->size)
    {
        service_handle_data->size--;
        /* check for any free slots below the current index */
        while (service_handle_data->instances[service_handle_data->size-1] == NULL)
        {
            service_handle_data->size--;
        }

        if(service_handle_data->size == 0)
        {
            free(service_handle_data);
            service_handle_data = NULL;
        }
        else
        {
            service_handle_data = PanicNull(realloc(service_handle_data,
                                                     sizeof(service_handle_data_t) + (sizeof(void*) *
                                                     service_handle_data->size -1)));
        }
    }

    return TRUE;
}


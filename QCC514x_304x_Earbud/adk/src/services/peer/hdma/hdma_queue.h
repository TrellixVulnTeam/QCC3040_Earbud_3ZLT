/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Interface for Queue utility.
*/

#ifdef INCLUDE_HDMA

#ifndef HDMA_QUEUE_H
#define HDMA_QUEUE_H
#ifdef DEBUG_HDMA_UT
#include <stdlib.h>
#include <stddef.h>
#include "types.h"
#endif
#include "hdma_utils.h"

#define INDEX_NOT_DEFINED 0xFF

/*! enum describing Queue Data */
typedef enum {
	/*! RSSI Queue type */
    HDMA_QUEUE_RSSI=0,
    /*! MIC Queue type */
    HDMA_QUEUE_MIC,
    /*! LINK Queue type */
    HDMA_QUEUE_LINK_QUALITY
}queue_type_t;

/*! Structure to hold MIC/RSSI quality event information */
typedef struct
{
	/*!<  Timestamp of the event */
    uint16 timestamp;
	/*!<  Quality data contained in the event */
    uint16 data;
}quality_data_t;

/*! Structure to hold information of MIC/RSSI quality queue */
typedef struct
{
	/*!<  Circular buffer of length BUFFER_LEN */
    quality_data_t quality[BUFFER_LEN];
	/*!<  base time calculated to save memory */
    uint32 base_time;
	/*!<  exit point of the queue */
    uint8 rear;
	/*!<  entry point of the queue */
    uint8 front;
	/*!<  Current size of the queue */
    uint8 size; 
	/*!<  Max_SIZE of the queue */
    uint8 capacity;
}queue_t;

/*! \brief Creates a new queue having #quality_data_t data with fixed capacity of #BUFFER_LEN

    \param[in] queue Pointer to queue.
*/
void Hdma_QueueCreate(queue_t *q);

/*! \brief Destroy queue

    \param[in] queue Pointer to queue.
*/
void Hdma_QueueDestroy(queue_t *q);

/*! \brief Checks whether queue is full

    \param[in] queue Pointer to queue.
    \param[out] True if full.
*/
uint8 Hdma_IsQueueFull(queue_t* queue) ;

/*! \brief Checks whether queue is empty

    \param[in] queue Pointer to queue.
    \param[out] True if empty.
*/
uint8 Hdma_IsQueueEmpty(queue_t *queue) ;

/*! \brief Inserts data and timestamp in queue at rear. Overwrites front value in case queue is full

    \param[in] queue Pointer to queue.
    \param[in] data Quality value
    \param[in] timestamp Timestamp value
*/
void Hdma_QueueInsert(queue_t* queue,uint16 data, uint32 timestamp);

/*! \brief Deletes data and timestamp in queue from front and returns value.

    \param[in] Pointer to queue.
    \param[in] timestamp Timestamp value
    \param[out] Returns quality value from front
*/
uint8 Hdma_QueueDelete(queue_t *queue, uint32 *timestamp);

/*! \brief Read data and timestamp in queue from front and returns value.

    \param[in] Pointer to queue.
    \param[in] timestamp Timestamp value
    \param[out] Returns quality value from front
*/
uint8 Hdma_GetQueueFront(queue_t* queue, uint32 *timestamp) ;

/*! \brief Read data and timestamp in queue from rear and returns value.

    \param[in] Pointer to queue.
    \param[in] timestamp Timestamp value
    \param[out] Returns quality value from rear
*/
uint8 Hdma_GetQueueRear(queue_t* queue, uint32 *timestamp) ;
#endif

#endif /* INCLUDE_HDMA */

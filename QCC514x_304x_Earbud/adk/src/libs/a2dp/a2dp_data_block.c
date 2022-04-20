/****************************************************************************
Copyright (c) 2004 - 2015 Qualcomm Technologies International, Ltd.


FILE NAME
    a2dp_data_block.c

NOTES

    This file merges different blocks of data for an a2dp link into one, to
    reduce the number of allocated malloc blocks, a restriction on all 
    supported chips due to the approach to memory fragmentation.

    Data alignment needs to be taken into account.
    * Some supported processors have constraints on memory alignment
    * Some of the data allocated is measured in bytes (potentially odd)

    Where the SIZE of an element is passed in, it is assumed the sizeof()
    operator will have been used, ensuring padding.

    Padding the total size of each block for alignment ensures that 
    the rest of the code can ignore this, and only allocation/de-allocation
    is affected.

*/



/****************************************************************************
    Header files
*/
#include "a2dp_debug.h"

#include "a2dp_data_block.h"
#include "a2dp_private.h"
#include <string.h>    /* for memset */
#include <stdlib.h>


/* Multiple memory slot operation.  Data blocks reside in their own memory slots */
#define DBLK_TYPE         data_block_header
#define DBLK_HDR_SIZE     sizeof(DBLK_TYPE)
#define DBLK_BASE(n)      a2dp->data_blocks[(n)]
#define DBLK(n)           a2dp->data_blocks[(n)]->block
#define DBLK_DATA_SIZE(n) a2dp->data_blocks[(n)]->size_blocks_padded

/* If a chip has alignment constraints, need to take these into account in the 
 * allocation sizes. Have a function to round up.
 */
#if defined(__KALIMBA__) || defined(HOSTED_TEST_ENVIRONMENT)
            /* For Kalimba and host, pad to uint32 */
# define DBLK_PAD(x)    ((((x) + sizeof(uint32) - 1)/sizeof(uint32))*sizeof(uint32))
#elif defined(__XAP__) 
            /* No padding for XAP */
# define DBLK_PAD(x)    (x)
#endif

bool blockInit (void)
{
    uint16 i;
    
    for (i=0; i<A2DP_MAX_REMOTE_DEVICES_DEFAULT; i++)
    {
        if (DBLK_BASE(i) != NULL)
        {
            return FALSE;
        }
    }
    
    for (i=0; i<A2DP_MAX_REMOTE_DEVICES_DEFAULT; i++)
    {
        DBLK_BASE(i) = (DBLK_TYPE *)PanicNull(malloc(DBLK_HDR_SIZE));
        memset((uint8 *)DBLK_BASE(i), 0, DBLK_HDR_SIZE);
    }
    
    return TRUE;
}

uint8 *blockAdd (uint8 device_id, data_block_id block_id, uint8 element_count, uint8 element_size)
{
    if ((device_id < A2DP_MAX_REMOTE_DEVICES_DEFAULT) && (block_id < max_data_blocks))
    {
        data_block_info *data_block = &DBLK(device_id)[block_id];        
        DEBUG_LOG("blockAdd(devId=%u, enum:data_block_id:%d, cnt=%u, sz=%u)",device_id, block_id, element_count, element_size);
        
        if (!data_block->offset && element_count && element_size)
        {
            unsigned block_size = element_size * element_count;
            unsigned padded_block_size = DBLK_PAD(block_size);
            unsigned offset = DBLK_HDR_SIZE + DBLK_DATA_SIZE(device_id);  /* New block added at end of any existing ones */
            DBLK_TYPE *new_data_pool = (DBLK_TYPE *)realloc(DBLK_BASE(device_id), offset + padded_block_size);
            if (new_data_pool)
            {
                DBLK_BASE(device_id) = new_data_pool;
                memset(((uint8 *)DBLK_BASE(device_id))+offset, 0, padded_block_size);  /* For debug purposes */

                data_block = &DBLK(device_id)[block_id];
                DBLK_DATA_SIZE(device_id) += (uint16)padded_block_size;
                data_block->offset = offset;
                data_block->block_size = block_size;
                data_block->element_size = element_size;
                data_block->current_element = 0;

                DEBUG_LOG("blockAdd [@%X] size_blocks(%u)=%u",(unsigned)DBLK_BASE(device_id)+offset, device_id, DBLK_DATA_SIZE(device_id));
                return ((uint8 *)DBLK_BASE(device_id))+offset;
            }
        }
    }

    DEBUG_LOG_WARN("blockAdd failed - block already exists or unable to allocate memory");
    return 0;
}


void blockRemove (uint8 device_id, data_block_id block_id)
{
    if ((device_id < A2DP_MAX_REMOTE_DEVICES_DEFAULT) && (block_id < max_data_blocks))
    {
        data_block_info *data_block = &DBLK(device_id)[block_id];

        unsigned offset = data_block->offset;
        
        DEBUG_LOG("blockRemove(devId=%u, enum:data_block_id:%d)",device_id, block_id);
        
        if ( offset )
        {
            DBLK_TYPE *new_data_pool;
            unsigned padded_block_size = DBLK_PAD(data_block->block_size);

            data_block_info *block = &DBLK(device_id)[0];
            
            /* Reduce offsets of all blocks positioned above the block being removed */        
            do
            {
                DEBUG_LOG("blockRemove [%p]",(void*)block);

                if ( offset < block->offset )
                {
                    block->offset -= padded_block_size;
                }
            }

            while (++block <= &DBLK(device_id)[max_data_blocks-1]);
            
            /* Zero info parameters of block being removed */
            memset( data_block, 0, sizeof( data_block_info ) );
            

            /* Reduce overall size of all blocks */
            DBLK_DATA_SIZE(device_id) -= (uint16)padded_block_size;
            DEBUG_LOG("blockRemove size_blocks(%u)=%u\n", device_id,DBLK_DATA_SIZE(device_id));
            /* Shift blocks above removed block down by the appropriate amount.  For debug purposes, fill the now unused area at top of memory area */
            memmove((uint8*)DBLK_BASE(device_id)+offset, (uint8*)DBLK_BASE(device_id)+offset+padded_block_size, DBLK_DATA_SIZE(device_id)+DBLK_HDR_SIZE-offset);
            memset((uint8*)DBLK_BASE(device_id)+DBLK_DATA_SIZE(device_id)+DBLK_HDR_SIZE, 0xFF, padded_block_size);  /* For debug purposes */
            
            if ( (new_data_pool = (DBLK_TYPE *)realloc(DBLK_BASE(device_id), DBLK_HDR_SIZE+DBLK_DATA_SIZE(device_id))) != NULL )
            {
                DBLK_BASE(device_id) = new_data_pool;
            }
            /* No need to worry about a failed realloc, old one will still exist and be valid */
        }
    }
}


uint8 *blockGetBase (uint8 device_id, data_block_id block_id)
{
    if ((device_id < A2DP_MAX_REMOTE_DEVICES_DEFAULT) && (block_id < max_data_blocks))
    {

        unsigned offset = DBLK(device_id)[block_id].offset;

        DEBUG_LOG("blockGetBase(devId=%u, enum:data_block_id:%d)", device_id, block_id);

        if (offset)
        {

            return ((uint8*)DBLK_BASE(device_id))+offset;
        }
    }
    
    return NULL;
}


uint8 *blockGetIndexed (uint8 device_id, data_block_id block_id, uint8 element)
{
    if ((device_id < A2DP_MAX_REMOTE_DEVICES_DEFAULT) && (block_id < max_data_blocks))
    {

        data_block_info *data_block = &DBLK(device_id)[block_id];
        unsigned offset = data_block->element_size * element;
        
        DEBUG_LOG("blockGetIndexed(devId=%u, enum:data_block_id:%d, ele=%u)", device_id, block_id, element);
        
        offset += data_block->offset;
        
        if (offset)
        {

            return ((uint8*)DBLK_BASE(device_id))+offset;
        }
    }
    
    return NULL;
}


uint8 *blockGetCurrent (uint8 device_id, data_block_id block_id)
{
    if ((device_id < A2DP_MAX_REMOTE_DEVICES_DEFAULT) && (block_id < max_data_blocks))
    {

        data_block_info *data_block = &DBLK(device_id)[block_id];
        unsigned offset = data_block->element_size * data_block->current_element;

        DEBUG_LOG("blockGetCurrent(devId=%u, enum:data_block_id:%d)=%u", device_id, block_id, data_block->current_element);

        offset += data_block->offset;

        if (offset)
        {

            return ((uint8*)DBLK_BASE(device_id))+offset;
        }
    }
    
    return NULL;
}


uint8 *blockSetCurrent (uint8 device_id, data_block_id block_id, uint8 element)
{
    if ((device_id < A2DP_MAX_REMOTE_DEVICES_DEFAULT) && (block_id < max_data_blocks))
    {
        data_block_info *data_block = &DBLK(device_id)[block_id];
        
        DEBUG_LOG("blockSetCurrent(devId=%u, enum:data_block_id:%d, ele=%u)", device_id, block_id, element);
        
        if ( element == DATA_BLOCK_INDEX_NEXT )
        {
            element = (uint8)(data_block->current_element + 1);
        }
        else if ( element == DATA_BLOCK_INDEX_PREVIOUS )
        {
            element = (uint8)(data_block->current_element - 1);
        }
        
        if ( (data_block->element_size * element) < data_block->block_size )
        {
            data_block->current_element = element;
            return blockGetCurrent(device_id, block_id);
        }
    }

    return NULL;
}


uint16 blockGetSize (uint8 device_id, data_block_id block_id)
{
    if ((device_id < A2DP_MAX_REMOTE_DEVICES_DEFAULT) && (block_id < max_data_blocks))
    {
        DEBUG_LOG("blockGetSize(devId=%u, enum:data_block_id:%d)=%u  size_blocks()=%u", device_id, block_id, DBLK(device_id)[block_id].block_size, DBLK_DATA_SIZE(device_id));
        return (uint16)DBLK(device_id)[block_id].block_size;
    }
    
    return 0;
}



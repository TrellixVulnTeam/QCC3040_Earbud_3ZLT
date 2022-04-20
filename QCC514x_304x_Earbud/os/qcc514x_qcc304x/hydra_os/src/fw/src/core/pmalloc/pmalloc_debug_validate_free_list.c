/* Copyright (c) 2016 Qualcomm Technologies International, Ltd. */
/*   %%version */
/**
 * \file
 * Check sanity of a pool's free list
 *
 */

#include "pmalloc/pmalloc_private.h"


/**
 * Check sanity of a pool's free list
 */
#if defined(PMALLOC_FREE_LIST_CHECK) || defined(PFREE_FREE_LIST_CHECK)
void pmalloc_debug_validate_free_list(const pmalloc_pool *pool,
                                      const void *ptr)
{
    const void *blocks_start;
    const void *blocks_end;
    const void *block;
    size_t n;

    /* Range of blocks in this pool */
    blocks_start = (pool == pmalloc_pools)
        ? pmalloc_blocks
        : pool[-1].pool_end;
    blocks_end = pool->pool_end;

    /* Expected size of free list */
#ifdef PMALLOC_CUMULATIVE_BLOCKS
    n = (pool[1] >= pools_end) ? pmalloc_total_blocks : pool[1]->cblocks;
    n = (n - pool->blocks) - pool->allocated;
#else
    n = pool->blocks - pool->allocated;
#endif

    /* Walk the free list, checking that all the blocks lie within the pool
       and that the new pointer is not included */
    block = PMALLOC_UNSCRAMBLE_ADDRESS(pool->free);
    while ((blocks_start <= block) && (block < blocks_end) && (block != ptr)
           && n)
    {
        block = PMALLOC_UNSCRAMBLE_ADDRESS(* (void * const *) block);
        n--;
    }

    /* Check that the whole free list was traversed (i.e. that no invalid
       pointers were encountered) and that the free list size agrees with the
       number of blocks that have been allocated */
    if (block || n)
        panic(PANIC_HYDRA_INVALID_PRIVATE_MEMORY_POINTER);
}
#endif

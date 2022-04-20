/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       rafs_extent.c
\brief      Implement functions which operate on extents in rafs.

*/
#include <logging.h>
#include <panic.h>
#include <csrtypes.h>
#include <vmtypes.h>
#include <limits.h>
#include <stdlib.h>

#include "rafs.h"
#include "rafs_private.h"
#include "rafs_fat.h"
#include "rafs_utils.h"
#include "rafs_extent.h"

/*
 * Traverse extents callback functions
 * -----------------------------------
 */
/*!
 * A function pointer, for functions that will receive a sector
 * id and a user-supplied context pointer.
 */
typedef void (*extentfn)(uint16, void*);

typedef struct {
    extentfn    firstFree;  /*! Called on first sector of a new free extent */
    extentfn    nextFree;   /*! Called for each consecutive sector in the same extent */
    extentfn    firstUse;   /*! Called on first sector of a new in-use extent */
    extentfn    nextUse;    /*! Called for each consecutive sector in the same extent */
} extentfns_t;

/* Rafs_FindLongestFreeExtent context and callbacks */
/* ------------------------------------------------ */
typedef struct {
    inode_t     current;
    inode_t     result;
} find_longest;

/* Rafs_CountFreeExtents context and callbacks */
/* ------------------------------------------- */
typedef struct {
    uint16      count;
} count_extents;

/* Rafs_GetFreeExtents context and callbacks */
/* ----------------------------------------- */
typedef struct {
    inode_t     *list;
    uint16      listlen;
    uint16      count;
    inode_t     current;
} get_extents;

static void Rafs_FindLongestFirstFree(uint16 sector, void *ctx);
static void Rafs_FindLongestNextFree(uint16 sector, void *ctx);
static void Rafs_FindLongestFirstUse(uint16 sector, void *ctx);

static void Rafs_CountExtentsFirstFree(uint16 sector, void *ctx);

static void Rafs_GetExtentsFirstFree(uint16 sector, void *ctx);
static void Rafs_GetExtentsNextFree(uint16 sector, void *ctx);
static void Rafs_GetExtentsFirstUse(uint16 sector, void *ctx);
/* Sort inodes into largest to smallest size order */
static int Rafs_SortInodeLengthDescending(const void *pa, const void *pb);

static const extentfns_t find_longest_fns = {
    Rafs_FindLongestFirstFree,
    Rafs_FindLongestNextFree,
    Rafs_FindLongestFirstUse,
    0
};

static const extentfns_t count_extents_fns = {
    Rafs_CountExtentsFirstFree,
    0,
    0,
    0
};

static const extentfns_t get_extents_fns = {
    Rafs_GetExtentsFirstFree,
    Rafs_GetExtentsNextFree,
    Rafs_GetExtentsFirstUse,
    0
};


bool Rafs_AddExtentsToSectorMap(const inode_t inodes[], uint32 num)
{
    bool ok = TRUE;
    for(uint32 i = 0 ; ok && i < num ; i++)
    {
        if( rafs_isValidInode(&inodes[i]) )
        {
            for(uint16 s = 0 ; ok && s < inodes[i].length ; s++)
            {
                uint32 page = (uint32)inodes[i].offset + s;
                ok = !Rafs_SectorMapIsBlockInUse(page);
                if( ok )
                {
                    Rafs_SectorMapMarkUsedBlock(page);
                }
                else
                {
                    DEBUG_LOG_ALWAYS("Rafs_AddExtentsToSectorMap %u %u %u", i, s, page);
                }
            }
        }
    }
    return ok;
}

bool Rafs_RemoveExtentsFromSectorMap(const inode_t inodes[], uint32 num)
{
    bool ok = TRUE;
    for(uint32 i = 0 ; ok && i < num ; i++)
    {
        if( rafs_isValidInode(&inodes[i]) )
        {
            for(uint16 s = 0 ; ok && s < inodes[i].length ; s++)
            {
                uint32 page = (uint32)inodes[i].offset + s;
                ok = Rafs_SectorMapIsBlockInUse(page);
                if( ok )
                {
                    Rafs_SectorMapMarkFreeBlock(page);
                }
                else
                {
                    DEBUG_LOG_ALWAYS("Rafs_RemoveExtentsFromSectorMap %u %u %u", i, s, page);
                }
            }
        }
    }
    return ok;
}

/*!
 * \brief Rafs_TraverseExtents
 * This scans the sector use map, looking for contiguous extents of free and in-use
 * sectors.  As new extents are discovered, one of the four supplied functions is
 * called.
 * \param fns   A pointer to the four functions to be called.
 * \param ctx   A context pointer to pass to the supplied functions.
 */
static void Rafs_TraverseExtents(const extentfns_t *fns, void *ctx)
{
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    const uint32 bits_per_word = sizeof(rafs_self->sector_in_use_map[0]) * CHAR_BIT;
    bool free_run = (rafs_self->sector_in_use_map[0] & 1) == 0;
    for(uint32 m = 0 ; m < rafs_self->sector_in_use_map_length ; m++ )
    {
        for(uint32 b = 0 ; b < bits_per_word ; b++ )
        {
            uint32 mask = 1u << b;
            uint16 sector = (uint16)(m * bits_per_word + b);
            if( (rafs_self->sector_in_use_map[m] & mask) == mask )
            {
                if( !free_run )
                {
                    if( fns->firstFree )
                        fns->firstFree(sector, ctx);
                    free_run = TRUE;
                }
                else
                {
                    if( fns->nextFree )
                        fns->nextFree(sector, ctx);
                }
            }
            else
            {
                if( free_run )
                {
                    if( fns->firstUse )
                        fns->firstUse(sector, ctx);
                    free_run = FALSE;
                }
                else
                {
                    if( fns->nextUse )
                        fns->nextUse(sector, ctx);
                }
            }
        }
    }
}


static void Rafs_FindLongestFirstFree(uint16 sector, void *ctx)
{
    find_longest *p = ctx;
    p->current.offset = sector;
    p->current.length = 1;
}
static void Rafs_FindLongestNextFree(uint16 sector, void *ctx)
{
    UNUSED(sector);
    find_longest *p = ctx;
    p->current.length++;
}
static void Rafs_FindLongestFirstUse(uint16 sector, void *ctx)
{
    UNUSED(sector);
    find_longest *p = ctx;
    if( p->current.length > p->result.length )
        p->result = p->current;
    p->current.offset = 0;
    p->current.length = 0;
}

inode_t Rafs_FindLongestFreeExtent(void)
{
    find_longest    data = { { 0, 0 }, { 0, 0 } };
    Rafs_TraverseExtents(&find_longest_fns, &data);

    inode_t    result;
    if( data.current.length > data.result.length )
        result = data.current;
    else
        result = data.result;
    return result;
}


static void Rafs_CountExtentsFirstFree(uint16 sector, void *ctx)
{
    UNUSED(sector);
    count_extents *p = ctx;
    p->count++;
}

uint16 Rafs_CountFreeExtents(void)
{
    count_extents   data = { 0 };
    Rafs_TraverseExtents(&count_extents_fns, &data);
    return data.count;
}


static void Rafs_GetExtentsFirstFree(uint16 sector, void *ctx)
{
    get_extents *p = ctx;
    p->current.offset = sector;
    p->current.length = 1;
}
static void Rafs_GetExtentsNextFree(uint16 sector, void *ctx)
{
    UNUSED(sector);
    get_extents *p = ctx;
    p->current.length++;
}
static void Rafs_GetExtentsFirstUse(uint16 sector, void *ctx)
{
    UNUSED(sector);
    get_extents *p = ctx;
    if( p->current.length > 0 )
    {
        if( p->count < p->listlen )
            p->list[p->count] = p->current;
        p->count++;
    }
    p->current.offset = 0;
    p->current.length = 0;
}
static int Rafs_SortInodeLengthDescending(const void *pa, const void *pb)
{
    const inode_t   *a = pa;
    const inode_t   *b = pb;
    if ( a->length > b->length )
    {
        return -1;
    }
    else if ( a->length < b->length )
    {
        return +1;
    }
    else
    {
        /* sort by increasing start offset for same sizes */
        if ( a->offset < b->offset ) return -1;
        if ( a->offset > b->offset ) return +1;
    }
    return 0;
}

uint16 Rafs_GetFreeExtents(inode_t *inodes, uint16 num)
{
    get_extents   data = { inodes, num, 0, { 0, 0 } };
    Rafs_TraverseExtents(&get_extents_fns, &data);
    if( data.current.length > 0 )
    {
        if( data.count < data.listlen )
            data.list[data.count] = data.current;
        data.count++;
    }

    uint16 n = MIN(data.count,data.listlen);
    qsort(data.list, n, sizeof(data.list[0]), Rafs_SortInodeLengthDescending);

    return data.count;
}

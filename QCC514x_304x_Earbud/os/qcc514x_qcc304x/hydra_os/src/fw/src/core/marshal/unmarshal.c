/* Copyright (c) 2018 Qualcomm Technologies International, Ltd. */
/*   %%version */
/**
 * \file
 */

#include "marshal_base.h"

/** Unmarshal states */
enum unmarshal_state
{
    /** Objects are created and their values are unmarshalled */
    UNMARSHAL_VALUES = 0,
    /** Object pointer indexes are unmarshalled */
    UNMARSHAL_POINTER_INDEXES
};

/** Unmarshaller context */
typedef struct unmarshal_context
{
    /** Base marshal object - containing type descriptors and object sets */
    struct marshal_base base;

    /** Address of the start of the buffer */
    const uint8 *buf;

    /** The size (amount of data) in the marshal buffer */
    size_t size;

    /** Current read outdex to the marshal buffer */
    size_t outdex;

    /** The current unmarshalling state */
    enum unmarshal_state state;

    /** The index of the root object being unmarshalled */
    mob_index_t root_index;

    /** Index of next object to have its pointers unmarshalled. */
    mob_index_t next_index;

} unmarshal_context_t;

unmarshal_context_t *unmarshal_init(const marshal_type_descriptor_t * const *type_desc_list,
                                    size_t type_desc_list_elements)
{
    struct unmarshal_context *u = zpmalloc(sizeof(*u));

    base_init(&u->base, type_desc_list, type_desc_list_elements);

    /* Don't unmarshal the pointer indexes for the NULL object at the start of
       the object set */
    u->next_index = 0;
    u->root_index = MOB_INDEX_NULL+1;

    return u;
}

void unmarshal_set_buffer(unmarshal_context_t *u, const void *buf, size_t data)
{
    assert(u);
    assert(buf);

    u->buf = buf;
    u->size = data;
    u->outdex = 0;
}

void unmarshal_destroy(unmarshal_context_t *u, bool free_all_objects)
{
    assert(u);
    base_uninit(&u->base, free_all_objects);
    pfree(u);
}

void unmarshal_clear_store(unmarshaller_t u)
{
    assert(u);
    base_clear_store(&u->base);
    base_reset_pointer_member_set(&u->base);
    u->next_index = 0;
    u->root_index = MOB_INDEX_NULL+1;
}

static bool read_uint8(marshal_base_t *base, uint8 *value)
{
    unmarshal_context_t *u = STRUCT_FROM_MEMBER(unmarshal_context_t, base, base);
    if (u->outdex < u->size)
    {
        *value = *(u->buf + u->outdex++);
        return TRUE;
    }
    return FALSE;
}

static bool read_value(marshal_base_t *base, void *child_address, const marshal_type_descriptor_t *type_desc)
{
    unmarshal_context_t *u = STRUCT_FROM_MEMBER(unmarshal_context_t, base, base);
    size_t next_outdex;

    next_outdex = u->outdex + type_desc->size;
    if (next_outdex <= u->size)
    {
        marshal_custom_copy_cb copy_cb = memcpy;
        const marshal_custom_copy_cbs *cbs = type_desc->u.custom_copy_cbs;

        if (cbs && cbs->unmarshal_copy)
        {
            copy_cb = cbs->unmarshal_copy;
        }

        (void)copy_cb(child_address, u->buf + u->outdex, type_desc->size);
        u->outdex = next_outdex;
        return TRUE;
    }
    return FALSE;
}

static bool unmarshal_pointer_indexes(mobs_t *set, mob_index_t index, mob_t *object)
{
    marshal_base_t *base = STRUCT_FROM_MEMBER(marshal_base_t, pointer_member_set, set);
    unmarshal_context_t *u = STRUCT_FROM_MEMBER(unmarshal_context_t, base, base);
    mob_index_t referred_index;

    if (read_uint8(base, &referred_index))
    {
        mob_t referred;

#ifdef INCLUDE_MARSHAL_SHARED_MEMBERS
        /* Either the base object set or the shared member set should contain an
           object corresponding to this pointer member. Objects from the shared
           member set are given indexes above the number of objects in the base
           object set */
        mob_index_t size = mobs_size(&base->object_set);
        if (referred_index >= size)
        {
            mob_index_t new_index = (mob_index_t)(referred_index - size);
            assert(mobs_get_object(&base->shared_member_set, new_index, &referred));
        }
        else
        {
            assert(mobs_get_object(&base->object_set, referred_index, &referred));
        }
#else
        /* P0 does not require support for shared members */
        assert(mobs_get_object(&base->object_set, referred_index, &referred));
#endif

        if (referred.address)
        {
            /* For non-null references, check the marshalled referred index type
               matches the expected type */
            assert(object->type == referred.type);
        }

        *(void**)(object->address) = referred.address;
        assert(index < MOBS_MAX_OBJECTS);
        u->next_index = (mob_index_t)(index + 1);
        return TRUE;
    }
    return FALSE;
}

static bool store_pointers(marshal_base_t *base, void *pointer_addr,
                           const marshal_member_descriptor_t *desc, mob_t *parent)
{
    mob_t object;
    object.address = pointer_addr;
    object.type = desc->type;
    object.disambiguator = 0;

    mobs_push_unique(&base->pointer_member_set, &object);

    UNUSED(parent);

    return TRUE;
}

/* Allocate an address for the typed object */
static bool unmarshal_alloc(unmarshal_context_t *u, mob_t *object)
{
    const marshal_type_descriptor_t *type_desc = u->base.type_desc_list[object->type];
    size_t size;

    if (type_desc->dynamic_length)
    {
        uint8 disambiguator;
        if (!read_uint8(&u->base, &disambiguator))
        {
            return FALSE;
        }
        object->disambiguator = disambiguator;
        size = obj_size(&u->base, object->type, disambiguator);
    }
    else
    {
        object->disambiguator = 0;
        size = type_desc->size;
    }
    object->address = zpmalloc(size);
    return TRUE;
}

static bool unmarshal_values(unmarshal_context_t *u)
{
    /* Unmarshal the values, and also store the addreses of any pointer
        members during the traverse to avoid a secondary traversal later. */
    const traverse_callbacks_t callbacks = {read_value, store_pointers, NULL};

    /* Unmarshal object values */
    mob_t object = MOB_ZERO();

    mobs_t *pointer_member_mobs = &u->base.pointer_member_set;

    /** Stores the read outdex before starting a read from the marshalling buffer.
     *  If the read fails (empty), the backup is used to restore the outdex to
     *  the value prior to the failed read.
     */
    size_t backup_outdex;

    /** Stores the number of objects in the pointer member set before starting
     *  a read from the marshal buffer. If the read fails (empty), the set will
     *  be restored to the original fix prior to the failed read.
     */
    mob_index_t backup_pointer_member_set_size;

    for(;;)
    {
        backup_pointer_member_set_size = mobs_size(pointer_member_mobs);
        backup_outdex = u->outdex;

        if (read_uint8(&u->base, &object.type))
        {
            /* Indicates end of marshalled object values */
            if (MARSHAL_TYPES_MAX == object.type)
            {
                return TRUE;
            }

            assert(object.type < u->base.type_desc_list_elements);

            if (!unmarshal_alloc(u, &object))
            {
                break;
            }

            if (object_tree_traverse(&u->base, &object, &callbacks))
            {
                mobs_push_unique(&u->base.object_set, &object);
            }
            else
            {
                pfree(object.address);
                break;
            }
        }
        else
        {
            break;
        }
    }

    while (mobs_size(pointer_member_mobs) > backup_pointer_member_set_size)
    {
        assert(mobs_pop(pointer_member_mobs, NULL));
    }
    u->outdex = backup_outdex;
    return FALSE;
}

bool unmarshal(unmarshal_context_t *u, void **addr, marshal_type_t *type)
{
    assert(u);
    assert(addr);
    assert(type);
    assert(u->buf);

    switch (u->state)
    {
        case UNMARSHAL_VALUES:
        {
            if (!unmarshal_values(u))
            {
                break;
            }

#ifdef INCLUDE_MARSHAL_SHARED_MEMBERS
            if (u->base.has_shared_objects)
            {
                /* Register shared members before unmarshalling object indexes */
                assert(mobs_iterate(&u->base.object_set, u->root_index, register_shared_members));
            }
#endif

            u->state = UNMARSHAL_POINTER_INDEXES;
        }
        /*lint -fallthrough */

        case UNMARSHAL_POINTER_INDEXES:
        {
            if (mobs_iterate(&u->base.pointer_member_set, u->next_index, unmarshal_pointer_indexes))
            {
                mob_t object;
                assert(mobs_get_object(&u->base.object_set, u->root_index, &object));
                *addr = object.address;
                *type = object.type;
                u->state = UNMARSHAL_VALUES;
                u->next_index = 0;
                u->root_index = mobs_size(&u->base.object_set);
                base_reset_pointer_member_set(&u->base);
                return TRUE;
            }
        }
        break;

        default:
            assert(FALSE);
        break;
    }
    return FALSE;
}

size_t unmarshal_consumed(unmarshal_context_t *u)
{
    assert(u);
    return u->outdex;
}
